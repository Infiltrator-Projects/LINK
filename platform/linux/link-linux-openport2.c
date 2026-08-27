// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file link-linux-openport2.c
 * @brief Native Linux Tactrix OpenPort 2.0 USB provider for LINK.
 *
 * The USB/J2534 wire implementation is the BSD-3-Clause OpenPort backend
 * vendored under third_party/openport2-j2534.  This file is LINK-owned glue:
 * it discovers the hardware, exposes it through the normal Linux adapter
 * chooser and presents an ELM-compatible transaction surface to the existing
 * LINK diagnostic controller.  Product layers therefore remain completely
 * transport agnostic.
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "link-linux-openport2.h"

#if defined(__linux__)

#include <ctype.h>
#include <errno.h>
#include <libusb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define LINK_OP2_SELECTION "OP2:Tactrix OpenPort 2.0"
#define LINK_OP2_VENDOR_ID UINT16_C(0x0403)
#define LINK_OP2_PRODUCT_ID UINT16_C(0xcc4d)

#define LINK_OP2_J2534_OK 0
#define LINK_OP2_J2534_TIMEOUT 9
#define LINK_OP2_ISO15765 6UL
#define LINK_OP2_FLOW_CONTROL_FILTER 3UL
#define LINK_OP2_ISO15765_FRAME_PAD 0x00000040UL
#define LINK_OP2_CAN_29BIT_ID 0x00000100UL
#define LINK_OP2_MSG_CAPACITY 4128U
#define LINK_OP2_RESPONSE_CAPACITY 8192U
#define LINK_OP2_IMMEDIATE_CAPACITY 1024U
#define LINK_OP2_RESPONSE_PENDING_EXTENSION_MS UINT64_C(3000)

typedef struct LinkOpenPortPassThruMsg {
    unsigned long ProtocolID;
    unsigned long RxStatus;
    unsigned long TxFlags;
    unsigned long Timestamp;
    unsigned long DataSize;
    unsigned long ExtraDataIndex;
    unsigned char Data[LINK_OP2_MSG_CAPACITY];
} LinkOpenPortPassThruMsg;

/*
 * Private declarations for the vendored BSD backend.  They deliberately stay
 * out of LINK's public API: consumers select "OP2:Tactrix OpenPort 2.0" and
 * keep using LinkTransport rather than depending on J2534 ABI details.
 */
extern int32_t PassThruOpen(const void *name, unsigned long *device_id);
extern int32_t PassThruClose(unsigned long device_id);
extern int32_t PassThruConnect(unsigned long device_id,
                               unsigned long protocol_id,
                               unsigned long flags,
                               unsigned long baud,
                               unsigned long *channel_id);
extern int32_t PassThruDisconnect(unsigned long channel_id);
extern int32_t PassThruReadMsgs(unsigned long channel_id,
                                LinkOpenPortPassThruMsg *message,
                                unsigned long *count,
                                unsigned long timeout_ms);
extern int32_t PassThruWriteMsgs(unsigned long channel_id,
                                 const LinkOpenPortPassThruMsg *message,
                                 unsigned long *count,
                                 unsigned long timeout_ms);
extern int32_t PassThruStartMsgFilter(
    unsigned long channel_id,
    unsigned long filter_type,
    const LinkOpenPortPassThruMsg *mask,
    const LinkOpenPortPassThruMsg *pattern,
    const LinkOpenPortPassThruMsg *flow_control,
    unsigned long *filter_id);
extern int32_t PassThruStopMsgFilter(unsigned long channel_id,
                                     unsigned long filter_id);
extern int32_t PassThruReadVersion(unsigned long device_id,
                                   char *firmware,
                                   char *library_version,
                                   char *api_version);

typedef struct LinkLinuxOpenPort2State {
    bool connected;
    unsigned long device_id;
    unsigned long channel_id;
    unsigned long filter_ids[8];
    size_t filter_count;

    unsigned int requested_protocol;
    unsigned int automatic_protocol;
    unsigned int automatic_candidate;
    unsigned long active_bitrate;
    bool active_extended;

    uint32_t tx_can_id;
    uint32_t rx_can_id;
    bool tx_can_id_set;
    bool rx_can_id_set;
    bool filter_dirty;

    unsigned int response_timeout_ms;
    bool request_pending;
    bool request_functional;
    bool request_saw_response;
    bool request_saw_pending;
    uint64_t request_deadline_ms;
    uint8_t request_payload[LINK_OP2_MSG_CAPACITY - 4U];
    size_t request_payload_length;

    char response_text[LINK_OP2_RESPONSE_CAPACITY];
    size_t response_length;
    char immediate_text[LINK_OP2_IMMEDIATE_CAPACITY];
    size_t immediate_length;

    char firmware[96];
} LinkLinuxOpenPort2State;

static LinkLinuxOpenPort2State *openport_state(
    const LinkLinuxSerialTransport *transport)
{
    return transport != NULL
        ? (LinkLinuxOpenPort2State *)transport->provider_context
        : NULL;
}

static uint64_t monotonic_ms(void)
{
    struct timespec value;
    if (clock_gettime(CLOCK_MONOTONIC, &value) != 0) return 0U;
    return (uint64_t)value.tv_sec * UINT64_C(1000) +
           (uint64_t)value.tv_nsec / UINT64_C(1000000);
}

static void make_can_id_message(LinkOpenPortPassThruMsg *message,
                                uint32_t can_id,
                                bool extended)
{
    if (message == NULL) return;
    memset(message, 0, sizeof(*message));
    message->ProtocolID = LINK_OP2_ISO15765;
    message->TxFlags = extended ? LINK_OP2_CAN_29BIT_ID : 0UL;
    message->DataSize = 4UL;
    message->Data[0] = (unsigned char)((can_id >> 24U) & UINT32_C(0xff));
    message->Data[1] = (unsigned char)((can_id >> 16U) & UINT32_C(0xff));
    message->Data[2] = (unsigned char)((can_id >> 8U) & UINT32_C(0xff));
    message->Data[3] = (unsigned char)(can_id & UINT32_C(0xff));
}

static uint32_t message_can_id(const LinkOpenPortPassThruMsg *message)
{
    if (message == NULL || message->DataSize < 4UL) return 0U;
    return ((uint32_t)message->Data[0] << 24U) |
           ((uint32_t)message->Data[1] << 16U) |
           ((uint32_t)message->Data[2] << 8U) |
           (uint32_t)message->Data[3];
}

static void stop_filters(LinkLinuxOpenPort2State *state)
{
    size_t index;
    if (state == NULL || state->channel_id == 0UL) {
        if (state != NULL) state->filter_count = 0U;
        return;
    }
    for (index = 0U; index < state->filter_count; ++index) {
        (void)PassThruStopMsgFilter(
            state->channel_id, state->filter_ids[index]);
    }
    state->filter_count = 0U;
}

static void close_channel(LinkLinuxOpenPort2State *state)
{
    if (state == NULL) return;
    stop_filters(state);
    if (state->channel_id != 0UL) {
        (void)PassThruDisconnect(state->channel_id);
        state->channel_id = 0UL;
    }
    state->active_bitrate = 0UL;
    state->active_extended = false;
    state->filter_dirty = true;
}

static void clear_transaction(LinkLinuxOpenPort2State *state)
{
    if (state == NULL) return;
    state->request_pending = false;
    state->request_functional = false;
    state->request_saw_response = false;
    state->request_saw_pending = false;
    state->request_deadline_ms = 0U;
    state->request_payload_length = 0U;
    state->response_length = 0U;
    state->response_text[0] = '\0';
}

static void reset_bridge(LinkLinuxOpenPort2State *state)
{
    if (state == NULL) return;
    close_channel(state);
    clear_transaction(state);
    state->requested_protocol = 0U;
    state->automatic_protocol = 0U;
    state->automatic_candidate = 0U;
    state->tx_can_id = 0U;
    state->rx_can_id = 0U;
    state->tx_can_id_set = false;
    state->rx_can_id_set = false;
    state->filter_dirty = true;
    state->response_timeout_ms = 400U;
}

static bool queue_text(LinkLinuxOpenPort2State *state, const char *text)
{
    size_t length;
    if (state == NULL || text == NULL || state->immediate_length != 0U)
        return false;
    length = strlen(text);
    if (length >= sizeof(state->immediate_text)) return false;
    memcpy(state->immediate_text, text, length + 1U);
    state->immediate_length = length;
    return true;
}

static bool queue_ok(LinkLinuxOpenPort2State *state)
{
    return queue_text(state, "OK\r>");
}

static void normalize_command(const uint8_t *bytes,
                              size_t size,
                              char *command,
                              size_t capacity)
{
    size_t index;
    size_t used = 0U;
    if (command == NULL || capacity == 0U) return;
    command[0] = '\0';
    if (bytes == NULL) return;
    for (index = 0U; index < size && used + 1U < capacity; ++index) {
        unsigned char value = bytes[index];
        if (isspace(value)) continue;
        command[used++] = (char)toupper(value);
    }
    command[used] = '\0';
}

static bool parse_hex_u32(const char *text, uint32_t *value)
{
    char *end = NULL;
    unsigned long parsed;
    if (text == NULL || value == NULL || text[0] == '\0') return false;
    errno = 0;
    parsed = strtoul(text, &end, 16);
    if (errno != 0 || end == text || *end != '\0' ||
        parsed > UINT32_MAX) {
        return false;
    }
    *value = (uint32_t)parsed;
    return true;
}

static int hex_value(char value)
{
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

static bool decode_hex_payload(const char *command,
                               uint8_t *payload,
                               size_t capacity,
                               size_t *length)
{
    size_t chars;
    size_t index;
    if (length != NULL) *length = 0U;
    if (command == NULL || payload == NULL || length == NULL)
        return false;
    chars = strlen(command);
    if (chars == 0U || (chars & 1U) != 0U || chars / 2U > capacity)
        return false;
    for (index = 0U; index < chars / 2U; ++index) {
        int high = hex_value(command[index * 2U]);
        int low = hex_value(command[index * 2U + 1U]);
        if (high < 0 || low < 0) return false;
        payload[index] = (uint8_t)((high << 4) | low);
    }
    *length = chars / 2U;
    return true;
}

static unsigned int current_protocol(const LinkLinuxOpenPort2State *state)
{
    if (state == NULL) return 0U;
    if (state->requested_protocol != 0U)
        return state->requested_protocol;
    if (state->automatic_protocol != 0U)
        return state->automatic_protocol;
    return state->automatic_candidate == 0U ? 6U : 8U;
}

static unsigned long protocol_bitrate(unsigned int protocol)
{
    return (protocol == 8U || protocol == 9U)
        ? 250000UL : 500000UL;
}

static bool protocol_extended(unsigned int protocol)
{
    return protocol == 7U || protocol == 9U;
}

static bool desired_extended(const LinkLinuxOpenPort2State *state)
{
    if (state == NULL) return false;
    if ((state->tx_can_id_set && state->tx_can_id > UINT32_C(0x7ff)) ||
        (state->rx_can_id_set && state->rx_can_id > UINT32_C(0x7ff))) {
        return true;
    }
    return protocol_extended(current_protocol(state));
}

static bool ensure_channel(LinkLinuxOpenPort2State *state)
{
    const unsigned int protocol = current_protocol(state);
    const unsigned long bitrate = protocol_bitrate(protocol);
    const bool extended = desired_extended(state);
    unsigned long channel_id = 0UL;
    int32_t result;

    if (state == NULL || !state->connected) return false;
    if (state->channel_id != 0UL &&
        state->active_bitrate == bitrate &&
        state->active_extended == extended) {
        return true;
    }

    close_channel(state);
    result = PassThruConnect(
        state->device_id,
        LINK_OP2_ISO15765,
        extended ? LINK_OP2_CAN_29BIT_ID : 0UL,
        bitrate,
        &channel_id);
    if (result != LINK_OP2_J2534_OK || channel_id == 0UL)
        return false;

    state->channel_id = channel_id;
    state->active_bitrate = bitrate;
    state->active_extended = extended;
    state->filter_dirty = true;
    return true;
}

static bool add_flow_filter(LinkLinuxOpenPort2State *state,
                            uint32_t mask_id,
                            uint32_t response_id,
                            uint32_t flow_id,
                            bool extended)
{
    LinkOpenPortPassThruMsg mask;
    LinkOpenPortPassThruMsg pattern;
    LinkOpenPortPassThruMsg flow;
    unsigned long filter_id = 0UL;
    int32_t result;

    if (state == NULL ||
        state->filter_count >=
            sizeof(state->filter_ids) / sizeof(state->filter_ids[0])) {
        return false;
    }

    make_can_id_message(&mask, mask_id, extended);
    make_can_id_message(&pattern, response_id, extended);
    make_can_id_message(&flow, flow_id, extended);
    result = PassThruStartMsgFilter(
        state->channel_id,
        LINK_OP2_FLOW_CONTROL_FILTER,
        &mask,
        &pattern,
        &flow,
        &filter_id);
    if (result != LINK_OP2_J2534_OK) return false;
    state->filter_ids[state->filter_count++] = filter_id;
    return true;
}

static bool install_filters(LinkLinuxOpenPort2State *state)
{
    bool custom;
    uint32_t tx_id;
    uint32_t rx_id;
    if (state == NULL || state->channel_id == 0UL) return false;
    if (!state->filter_dirty) return true;

    stop_filters(state);
    custom = state->tx_can_id_set || state->rx_can_id_set;

    if (custom) {
        tx_id = state->tx_can_id_set ? state->tx_can_id : UINT32_C(0x7df);
        if (state->rx_can_id_set) {
            rx_id = state->rx_can_id;
        } else if (!state->active_extended &&
                   tx_id <= UINT32_C(0x7f7)) {
            rx_id = tx_id + UINT32_C(8);
        } else {
            return false;
        }
        if (!add_flow_filter(
                state,
                state->active_extended
                    ? UINT32_C(0x1fffffff) : UINT32_C(0x7ff),
                rx_id,
                tx_id,
                state->active_extended)) {
            return false;
        }
    } else {
        uint32_t index;
        /*
         * LINK's automatic ELM-compatible acquisition deliberately starts with
         * 11-bit ISO15765.  Eight functional OBD responders are flow-controlled
         * exactly as the shared Windows J2534 Discover path does.
         */
        if (state->active_extended) return false;
        for (index = 0U; index < 8U; ++index) {
            if (!add_flow_filter(
                    state,
                    UINT32_C(0x7ff),
                    UINT32_C(0x7e8) + index,
                    UINT32_C(0x7e0) + index,
                    false)) {
                return false;
            }
        }
    }

    state->filter_dirty = false;
    return true;
}

static bool append_response_payload(LinkLinuxOpenPort2State *state,
                                    const uint8_t *payload,
                                    size_t payload_length)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t required;
    size_t index;
    if (state == NULL || payload == NULL || payload_length == 0U)
        return false;
    required = payload_length * 2U + 1U;
    if (state->response_length + required + 1U >
        sizeof(state->response_text)) {
        return false;
    }
    for (index = 0U; index < payload_length; ++index) {
        state->response_text[state->response_length++] =
            hex[payload[index] >> 4U];
        state->response_text[state->response_length++] =
            hex[payload[index] & UINT8_C(0x0f)];
    }
    state->response_text[state->response_length++] = '\r';
    state->response_text[state->response_length] = '\0';
    return true;
}

static bool payload_is_response_pending(const uint8_t *payload,
                                        size_t payload_length)
{
    return payload != NULL && payload_length >= 3U &&
           payload[0] == UINT8_C(0x7f) &&
           payload[2] == UINT8_C(0x78);
}

static void finish_request(LinkLinuxOpenPort2State *state)
{
    if (state == NULL || !state->request_pending) return;
    if (state->response_length == 0U) {
        (void)queue_text(state, "NO DATA\r>");
    } else if (state->response_length + 2U <=
               sizeof(state->immediate_text)) {
        memcpy(state->immediate_text,
               state->response_text,
               state->response_length);
        state->immediate_text[state->response_length] = '>';
        state->immediate_text[state->response_length + 1U] = '\0';
        state->immediate_length = state->response_length + 1U;
    } else {
        (void)queue_text(state, "BUFFER FULL\r>");
    }

    if (state->requested_protocol == 0U &&
        state->request_saw_response &&
        state->automatic_protocol == 0U) {
        state->automatic_protocol = current_protocol(state);
    }
    state->request_pending = false;
    state->request_functional = false;
    state->request_saw_pending = false;
}

static bool send_stored_request(LinkLinuxOpenPort2State *state)
{
    LinkOpenPortPassThruMsg message;
    uint32_t tx_id;
    unsigned long count = 1UL;
    int32_t result;

    if (state == NULL || state->request_payload_length == 0U)
        return false;
    if (!ensure_channel(state) || !install_filters(state))
        return false;

    if (state->tx_can_id_set) {
        tx_id = state->tx_can_id;
    } else {
        if (state->active_extended) return false;
        tx_id = UINT32_C(0x7df);
    }

    memset(&message, 0, sizeof(message));
    message.ProtocolID = LINK_OP2_ISO15765;
    message.TxFlags = LINK_OP2_ISO15765_FRAME_PAD |
        (state->active_extended ? LINK_OP2_CAN_29BIT_ID : 0UL);
    message.Data[0] =
        (unsigned char)((tx_id >> 24U) & UINT32_C(0xff));
    message.Data[1] =
        (unsigned char)((tx_id >> 16U) & UINT32_C(0xff));
    message.Data[2] =
        (unsigned char)((tx_id >> 8U) & UINT32_C(0xff));
    message.Data[3] =
        (unsigned char)(tx_id & UINT32_C(0xff));
    memcpy(
        message.Data + 4U,
        state->request_payload,
        state->request_payload_length);
    message.DataSize =
        (unsigned long)(state->request_payload_length + 4U);

    result = PassThruWriteMsgs(
        state->channel_id, &message, &count, 250UL);
    if (result != LINK_OP2_J2534_OK || count != 1UL)
        return false;

    state->request_pending = true;
    state->request_functional = !state->tx_can_id_set;
    state->request_saw_response = false;
    state->request_saw_pending = false;
    state->response_length = 0U;
    state->response_text[0] = '\0';
    state->request_deadline_ms =
        monotonic_ms() + (uint64_t)state->response_timeout_ms;
    return true;
}

static bool retry_automatic_protocol(LinkLinuxOpenPort2State *state)
{
    if (state == NULL ||
        state->requested_protocol != 0U ||
        state->automatic_protocol != 0U ||
        state->request_saw_response ||
        state->automatic_candidate != 0U) {
        return false;
    }

    /*
     * The native automatic path mirrors the two standard 11-bit CAN choices
     * that cover LINK's current Mercedes/Jaguar CAN diagnostic products:
     * ISO15765 11/500 first, then 11/250.  Explicit ATSP7/9 and extended ATSH
     * headers still provide 29-bit operation without guessing response routes.
     */
    state->automatic_candidate = 1U;
    close_channel(state);
    return send_stored_request(state);
}

static bool response_matches(const LinkLinuxOpenPort2State *state,
                             uint32_t can_id)
{
    if (state == NULL) return false;
    if (state->rx_can_id_set)
        return can_id == state->rx_can_id;
    if (state->tx_can_id_set && !state->active_extended &&
        state->tx_can_id <= UINT32_C(0x7f7)) {
        return can_id == state->tx_can_id + UINT32_C(8);
    }
    if (!state->tx_can_id_set && !state->active_extended)
        return can_id >= UINT32_C(0x7e8) &&
               can_id <= UINT32_C(0x7ef);
    return false;
}

static void accept_pass_thru_message(LinkLinuxOpenPort2State *state,
                                     const LinkOpenPortPassThruMsg *message)
{
    const uint8_t *payload;
    size_t payload_length;
    uint32_t can_id;

    if (state == NULL || message == NULL ||
        message->DataSize <= 4UL ||
        message->DataSize > LINK_OP2_MSG_CAPACITY) {
        return;
    }

    /*
     * RxStatus 0x02 is an ISO-15765 start-of-message indication, not the
     * completed assembled response.  Treating it as complete can terminate a
     * directed multi-frame request before the final J2534 record arrives.
     */
    if (!link_linux_openport2_rx_status_is_complete_vehicle(
            message->RxStatus)) return;

    can_id = message_can_id(message);
    if (!response_matches(state, can_id)) return;

    payload = message->Data + 4U;
    payload_length = (size_t)message->DataSize - 4U;
    if (!append_response_payload(state, payload, payload_length)) {
        state->response_length = 0U;
        state->response_text[0] = '\0';
        (void)queue_text(state, "BUFFER FULL\r>");
        state->request_pending = false;
        return;
    }

    state->request_saw_response = true;
    if (payload_is_response_pending(payload, payload_length)) {
        state->request_saw_pending = true;
        state->request_deadline_ms =
            monotonic_ms() + LINK_OP2_RESPONSE_PENDING_EXTENSION_MS;
        return;
    }

    state->request_saw_pending = false;
    if (!state->request_functional)
        finish_request(state);
}

static bool handle_at_command(LinkLinuxOpenPort2State *state,
                              const char *command)
{
    uint32_t value;
    if (state == NULL || command == NULL) return false;

    if (strcmp(command, "ATZ") == 0 ||
        strcmp(command, "ATWS") == 0) {
        reset_bridge(state);
        return queue_text(state, "LINK OpenPort 2.0 native USB\r>");
    }
    if (strcmp(command, "ATI") == 0) {
        char identity[256];
        (void)snprintf(
            identity, sizeof(identity),
            "Tactrix OpenPort 2.0 / LINK native USB%s%s\r>",
            state->firmware[0] != '\0' ? " / FW " : "",
            state->firmware[0] != '\0' ? state->firmware : "");
        return queue_text(state, identity);
    }

    if (strcmp(command, "ATE0") == 0 ||
        strcmp(command, "ATL0") == 0 ||
        strcmp(command, "ATS0") == 0 ||
        strcmp(command, "ATH0") == 0 ||
        strcmp(command, "ATCAF1") == 0 ||
        strcmp(command, "ATCFC1") == 0) {
        return queue_ok(state);
    }

    if (strncmp(command, "ATSP", 4U) == 0 &&
        command[4] != '\0' && command[5] == '\0') {
        int protocol = hex_value(command[4]);
        if (protocol != 0 && protocol != 6 &&
            protocol != 7 && protocol != 8 && protocol != 9) {
            return queue_text(state, "?\r>");
        }
        close_channel(state);
        state->requested_protocol = (unsigned int)protocol;
        state->automatic_protocol = 0U;
        state->automatic_candidate = 0U;
        return queue_ok(state);
    }

    if (strcmp(command, "ATDPN") == 0) {
        char response[16];
        unsigned int protocol = current_protocol(state);
        (void)snprintf(
            response, sizeof(response),
            "%s%X\r>",
            state->requested_protocol == 0U ? "A" : "",
            protocol);
        return queue_text(state, response);
    }

    if (strcmp(command, "ATDP") == 0) {
        char response[96];
        unsigned int protocol = current_protocol(state);
        (void)snprintf(
            response, sizeof(response),
            "ISO 15765-4 CAN (%s, %lu kbaud)\r>",
            desired_extended(state) ? "29 bit ID" : "11 bit ID",
            protocol_bitrate(protocol) / 1000UL);
        return queue_text(state, response);
    }

    if (strncmp(command, "ATSH", 4U) == 0) {
        if (!parse_hex_u32(command + 4U, &value) ||
            value > UINT32_C(0x1fffffff)) {
            return queue_text(state, "?\r>");
        }
        state->tx_can_id = value;
        state->tx_can_id_set = true;
        state->filter_dirty = true;
        return queue_ok(state);
    }

    if (strncmp(command, "ATCRA", 5U) == 0) {
        if (!parse_hex_u32(command + 5U, &value) ||
            value > UINT32_C(0x1fffffff)) {
            return queue_text(state, "?\r>");
        }
        state->rx_can_id = value;
        state->rx_can_id_set = true;
        state->filter_dirty = true;
        return queue_ok(state);
    }

    if (strncmp(command, "ATST", 4U) == 0) {
        if (!parse_hex_u32(command + 4U, &value) || value > UINT32_C(0xff))
            return queue_text(state, "?\r>");
        state->response_timeout_ms = (unsigned int)value * 4U;
        if (state->response_timeout_ms < 40U)
            state->response_timeout_ms = 40U;
        return queue_ok(state);
    }

    if (strcmp(command, "ATPC") == 0) {
        close_channel(state);
        return queue_ok(state);
    }

    return queue_text(state, "?\r>");
}

bool link_linux_openport2_is_selection(const char *device)
{
    return device != NULL &&
           strcmp(device, LINK_OP2_SELECTION) == 0;
}

size_t link_linux_openport2_discover(char paths[][256], size_t capacity)
{
    libusb_context *context = NULL;
    libusb_device **devices = NULL;
    ssize_t count;
    ssize_t index;
    size_t found = 0U;

    if (paths == NULL || capacity == 0U) return 0U;
    if (libusb_init(&context) != LIBUSB_SUCCESS) return 0U;
    count = libusb_get_device_list(context, &devices);
    if (count < 0) {
        libusb_exit(context);
        return 0U;
    }

    for (index = 0; index < count && found < capacity; ++index) {
        struct libusb_device_descriptor descriptor;
        if (libusb_get_device_descriptor(
                devices[index], &descriptor) != LIBUSB_SUCCESS) {
            continue;
        }
        if (descriptor.idVendor == LINK_OP2_VENDOR_ID &&
            descriptor.idProduct == LINK_OP2_PRODUCT_ID) {
            (void)snprintf(paths[found], 256U, "%s", LINK_OP2_SELECTION);
            ++found;
            break;
        }
    }

    libusb_free_device_list(devices, 1);
    libusb_exit(context);
    return found;
}

LinkTransportStatus link_linux_openport2_connect(
    LinkLinuxSerialTransport *transport)
{
    LinkLinuxOpenPort2State *state;
    char library_version[96] = {0};
    char api_version[96] = {0};
    int32_t result;

    if (transport == NULL || !transport->openport2)
        return LINK_TRANSPORT_INVALID_ARGUMENT;
    state = openport_state(transport);
    if (state != NULL && state->connected) return LINK_TRANSPORT_OK;
    if (state == NULL) {
        state = calloc(1U, sizeof(*state));
        if (state == NULL) return LINK_TRANSPORT_IO_ERROR;
        transport->provider_context = state;
    }

    result = PassThruOpen(NULL, &state->device_id);
    if (result != LINK_OP2_J2534_OK) {
        free(state);
        transport->provider_context = NULL;
        return LINK_TRANSPORT_IO_ERROR;
    }

    state->connected = true;
    state->filter_dirty = true;
    state->response_timeout_ms = 400U;
    (void)PassThruReadVersion(
        state->device_id,
        state->firmware,
        library_version,
        api_version);
    transport->connected = true;
    transport->fd = -1;
    return LINK_TRANSPORT_OK;
}

void link_linux_openport2_disconnect(LinkLinuxSerialTransport *transport)
{
    LinkLinuxOpenPort2State *state = openport_state(transport);
    if (state == NULL) {
        if (transport != NULL) transport->connected = false;
        return;
    }

    close_channel(state);
    if (state->connected) {
        (void)PassThruClose(state->device_id);
        state->connected = false;
    }
    clear_transaction(state);
    state->immediate_length = 0U;
    state->immediate_text[0] = '\0';
    if (transport != NULL) transport->connected = false;
}

bool link_linux_openport2_is_connected(
    const LinkLinuxSerialTransport *transport)
{
    const LinkLinuxOpenPort2State *state = openport_state(transport);
    return transport != NULL && transport->connected &&
           state != NULL && state->connected;
}

LinkTransportStatus link_linux_openport2_write(
    LinkLinuxSerialTransport *transport,
    const uint8_t *bytes,
    size_t size)
{
    LinkLinuxOpenPort2State *state = openport_state(transport);
    char command[256];

    if (transport == NULL || bytes == NULL || size == 0U)
        return LINK_TRANSPORT_INVALID_ARGUMENT;
    if (!link_linux_openport2_is_connected(transport) || state == NULL)
        return LINK_TRANSPORT_NOT_CONNECTED;
    if (state->request_pending || state->immediate_length != 0U)
        return LINK_TRANSPORT_BUSY;

    normalize_command(bytes, size, command, sizeof(command));
    if (command[0] == '\0') return LINK_TRANSPORT_INVALID_ARGUMENT;

    if (strncmp(command, "AT", 2U) == 0) {
        return handle_at_command(state, command)
            ? LINK_TRANSPORT_OK : LINK_TRANSPORT_IO_ERROR;
    }

    if (!decode_hex_payload(
            command,
            state->request_payload,
            sizeof(state->request_payload),
            &state->request_payload_length)) {
        (void)queue_text(state, "?\r>");
        return LINK_TRANSPORT_OK;
    }

    if (!send_stored_request(state)) {
        clear_transaction(state);
        (void)queue_text(state, "CAN ERROR\r>");
    }
    return LINK_TRANSPORT_OK;
}

static void emit_immediate(LinkLinuxSerialTransport *transport,
                           LinkLinuxOpenPort2State *state)
{
    size_t length;
    if (transport == NULL || state == NULL ||
        state->immediate_length == 0U ||
        transport->receiver == NULL) {
        return;
    }
    length = state->immediate_length;
    state->immediate_length = 0U;
    transport->receiver(
        transport->receiver_context,
        (const uint8_t *)state->immediate_text,
        length);
    state->immediate_text[0] = '\0';
}

void link_linux_openport2_pump(LinkLinuxSerialTransport *transport)
{
    LinkLinuxOpenPort2State *state = openport_state(transport);
    unsigned int iteration;

    if (!link_linux_openport2_is_connected(transport) || state == NULL)
        return;

    if (state->immediate_length != 0U) {
        emit_immediate(transport, state);
        return;
    }
    if (!state->request_pending) return;

    for (iteration = 0U; iteration < 4U && state->request_pending; ++iteration) {
        LinkOpenPortPassThruMsg messages[8];
        unsigned long count = 8UL;
        unsigned long index;
        int32_t result;

        memset(messages, 0, sizeof(messages));
        result = PassThruReadMsgs(
            state->channel_id, messages, &count, 1UL);
        if (result == LINK_OP2_J2534_TIMEOUT) break;
        if (result != LINK_OP2_J2534_OK) {
            state->request_pending = false;
            state->response_length = 0U;
            (void)queue_text(state, "CAN ERROR\r>");
            break;
        }
        for (index = 0UL; index < count && state->request_pending; ++index)
            accept_pass_thru_message(state, &messages[index]);
        if (count == 0UL) break;
    }

    if (state->request_pending &&
        monotonic_ms() >= state->request_deadline_ms) {
        if (state->request_saw_response) {
            finish_request(state);
        } else if (!retry_automatic_protocol(state)) {
            finish_request(state);
        }
    }

    if (state->immediate_length != 0U)
        emit_immediate(transport, state);
}

bool link_linux_openport2_probe(
    LinkLinuxSerialTransport *transport,
    char *identity,
    size_t identity_capacity)
{
    const LinkLinuxOpenPort2State *state = openport_state(transport);
    int written;
    if (identity != NULL && identity_capacity != 0U) identity[0] = '\0';
    if (!link_linux_openport2_is_connected(transport) ||
        state == NULL || identity == NULL || identity_capacity == 0U) {
        return false;
    }

    written = snprintf(
        identity,
        identity_capacity,
        "Tactrix OpenPort 2.0 · LINK native USB%s%s",
        state->firmware[0] != '\0' ? " · firmware " : "",
        state->firmware[0] != '\0' ? state->firmware : "");
    return written >= 0 && (size_t)written < identity_capacity;
}

void link_linux_openport2_destroy(LinkLinuxSerialTransport *transport)
{
    LinkLinuxOpenPort2State *state;
    if (transport == NULL) return;
    state = openport_state(transport);
    if (state == NULL) return;
    link_linux_openport2_disconnect(transport);
    free(state);
    transport->provider_context = NULL;
}

#endif
