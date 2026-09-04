// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file transport.c
 * @brief Transport validation plus deterministic in-process ELM327 simulator.
 */
#include "link/transport.h"
#include "link/mercedes_me_adapter.h"
#include "link/elm327_simulator.h"

#include <stdio.h>
#include <string.h>

static unsigned char link_ascii_lower(unsigned char value)
{
    if (value >= (unsigned char)'A' && value <= (unsigned char)'Z')
        return (unsigned char)(value - (unsigned char)'A' + (unsigned char)'a');
    return value;
}

static bool link_ascii_contains_nocase(const char *text, const char *needle)
{
    size_t text_length, needle_length, start, offset;
    if (text == NULL || needle == NULL || needle[0] == '\0') return false;
    text_length = strlen(text);
    needle_length = strlen(needle);
    if (needle_length > text_length) return false;
    for (start = 0U; start + needle_length <= text_length; ++start) {
        for (offset = 0U; offset < needle_length; ++offset) {
            if (link_ascii_lower((unsigned char)text[start + offset]) !=
                link_ascii_lower((unsigned char)needle[offset])) break;
        }
        if (offset == needle_length) return true;
    }
    return false;
}

LinkAdapterKind link_adapter_kind_from_bluetooth_name(const char *name)
{
    if (link_mercedes_me_adapter_family_from_name(name) !=
        LINK_MERCEDES_ME_ADAPTER_UNKNOWN)
        return LINK_ADAPTER_KIND_MERCEDES_ME_NATIVE;
    if (name == NULL || name[0] == '\0') return LINK_ADAPTER_KIND_UNKNOWN;
    if (link_ascii_contains_nocase(name, "vgate") ||
        link_ascii_contains_nocase(name, "v-link") ||
        link_ascii_contains_nocase(name, "vlink") ||
        link_ascii_contains_nocase(name, "icar") ||
        link_ascii_contains_nocase(name, "obd") ||
        link_ascii_contains_nocase(name, "elm") ||
        link_ascii_contains_nocase(name, "car pro"))
        return LINK_ADAPTER_KIND_ELM327;
    return LINK_ADAPTER_KIND_UNKNOWN;
}

const char *link_adapter_kind_name(LinkAdapterKind kind)
{
    switch (kind) {
    case LINK_ADAPTER_KIND_UNKNOWN: return "unknown";
    case LINK_ADAPTER_KIND_ELM327: return "elm327";
    case LINK_ADAPTER_KIND_TACTRIX_OPENPORT2: return "tactrix-openport2";
    case LINK_ADAPTER_KIND_MERCEDES_ME_NATIVE: return "mercedes-me-native";
    case LINK_ADAPTER_KIND_STM32_LINK: return "stm32-link";
    }
    return "unknown";
}

bool link_adapter_kind_requires_native_protocol(LinkAdapterKind kind)
{
    return kind == LINK_ADAPTER_KIND_MERCEDES_ME_NATIVE ||
           kind == LINK_ADAPTER_KIND_STM32_LINK;
}

bool link_adapter_capabilities(
    LinkAdapterKind kind,
    LinkAdapterCapabilities *capabilities)
{
    LinkAdapterCapabilities resolved;

    if (capabilities == NULL) return false;
    memset(&resolved, 0, sizeof(resolved));
    resolved.kind = kind;

    switch (kind) {
    case LINK_ADAPTER_KIND_ELM327:
        /*
         * This describes LINK's supported ELM command surface, not a promise
         * that every clone implements every command correctly. Runtime
         * protocol discovery remains authoritative for an individual device.
         */
        resolved.flags =
            LINK_ADAPTER_CAP_BYTE_STREAM |
            LINK_ADAPTER_CAP_ELM_COMMAND_SURFACE |
            LINK_ADAPTER_CAP_ISOTP |
            LINK_ADAPTER_CAP_CAN_11BIT |
            LINK_ADAPTER_CAP_CAN_29BIT |
            LINK_ADAPTER_CAP_CAN_FILTERS |
            LINK_ADAPTER_CAP_MULTI_RESPONSE |
            LINK_ADAPTER_CAP_RESPONSE_CAN_ID;
        break;

    case LINK_ADAPTER_KIND_TACTRIX_OPENPORT2:
        /*
         * The LINK OpenPort provider exposes the existing ELM-compatible
         * command surface while executing ISO15765 through native J2534.
         */
        resolved.flags =
            LINK_ADAPTER_CAP_BYTE_STREAM |
            LINK_ADAPTER_CAP_ELM_COMMAND_SURFACE |
            LINK_ADAPTER_CAP_ISOTP |
            LINK_ADAPTER_CAP_CAN_11BIT |
            LINK_ADAPTER_CAP_CAN_29BIT |
            LINK_ADAPTER_CAP_CAN_FILTERS |
            LINK_ADAPTER_CAP_MULTI_RESPONSE |
            LINK_ADAPTER_CAP_RESPONSE_CAN_ID;
        resolved.default_response_timeout_ms = UINT64_C(400);
        break;

    case LINK_ADAPTER_KIND_MERCEDES_ME_NATIVE:
        /*
         * Limits below are recovered from archived Mercedes me Adapter
         * 4.7.61 native GDK.  29-bit CAN is intentionally not advertised:
         * the recovered command validation currently proves standard 0x7ff
         * CAN filtering, not a complete extended-ID command encoding.
         */
        resolved.flags =
            LINK_ADAPTER_CAP_BYTE_STREAM |
            LINK_ADAPTER_CAP_RAW_CAN |
            LINK_ADAPTER_CAP_ISOTP |
            LINK_ADAPTER_CAP_CAN_11BIT |
            LINK_ADAPTER_CAP_CAN_FILTERS |
            LINK_ADAPTER_CAP_MULTI_RESPONSE |
            LINK_ADAPTER_CAP_RESPONSE_CAN_ID |
            LINK_ADAPTER_CAP_SECURE_SESSION |
            LINK_ADAPTER_CAP_NATIVE_DIAGNOSTIC;
        resolved.max_standard_can_id = UINT32_C(0x7ff);
        resolved.max_raw_can_payload = 8U;
        resolved.max_isotp_payload = 100U;
        resolved.max_filter_ids = 15U;
        resolved.default_response_timeout_ms = UINT64_C(400);
        break;

    case LINK_ADAPTER_KIND_STM32_LINK:
        /*
         * LINK's STM32 edge is a direct CAN/ISO-TP tester. The current
         * reference binding uses classic CAN payloads; ISO-TP length remains
         * governed by the portable engine rather than an invented MCU limit.
         */
        resolved.flags =
            LINK_ADAPTER_CAP_RAW_CAN |
            LINK_ADAPTER_CAP_ISOTP |
            LINK_ADAPTER_CAP_CAN_11BIT |
            LINK_ADAPTER_CAP_CAN_29BIT |
            LINK_ADAPTER_CAP_CAN_FILTERS |
            LINK_ADAPTER_CAP_MULTI_RESPONSE |
            LINK_ADAPTER_CAP_RESPONSE_CAN_ID |
            LINK_ADAPTER_CAP_NATIVE_DIAGNOSTIC;
        resolved.max_standard_can_id = UINT32_C(0x7ff);
        resolved.max_raw_can_payload = 8U;
        break;

    case LINK_ADAPTER_KIND_UNKNOWN:
        *capabilities = resolved;
        return false;
    }

    *capabilities = resolved;
    return true;
}

bool link_adapter_has_capability(LinkAdapterKind kind, uint32_t capability)
{
    LinkAdapterCapabilities capabilities;
    if (capability == 0U ||
        !link_adapter_capabilities(kind, &capabilities)) {
        return false;
    }
    return (capabilities.flags & capability) == capability;
}

bool link_transport_is_valid(const LinkTransport *transport)
{
    if (transport == NULL || transport->struct_size < sizeof(*transport) ||
        transport->abi_version != LINK_TRANSPORT_ABI) {
        return false;
    }

    return transport->connect != NULL &&
           transport->disconnect != NULL &&
           transport->is_connected != NULL &&
           transport->write != NULL &&
           transport->set_receiver != NULL;
}

static char simulator_hex_digit(unsigned int value)
{
    static const char digits[] = "0123456789ABCDEF";
    return digits[value & 0x0fU];
}

static int simulator_hex_value(char value)
{
    if (value >= '0' && value <= '9') return (int)(value - '0');
    if (value >= 'A' && value <= 'F') return (int)(value - 'A') + 10;
    if (value >= 'a' && value <= 'f') return (int)(value - 'a') + 10;
    return -1;
}

static bool simulator_space(unsigned char value)
{
    return value == ' ' || value == '\t' || value == '\r' ||
           value == '\n' || value == '\v' || value == '\f';
}

static bool simulator_canonicalise(
    const char *source, char *destination, size_t destination_size)
{
    size_t written = 0U;

    if (source == NULL || destination == NULL || destination_size == 0U) {
        return false;
    }
    while (*source != '\0') {
        unsigned char value = (unsigned char)*source++;
        if (simulator_space(value)) continue;
        if (written + 1U >= destination_size) {
            destination[0] = '\0';
            return false;
        }
        if (value >= (unsigned char)'a' && value <= (unsigned char)'z') {
            value = (unsigned char)(value - (unsigned char)'a' + (unsigned char)'A');
        }
        destination[written++] = (char)value;
    }
    destination[written] = '\0';
    return written != 0U;
}

static bool simulator_append_byte(
    char *buffer, size_t buffer_size, size_t *position, uint8_t value)
{
    if (buffer == NULL || position == NULL || *position + 2U >= buffer_size) {
        return false;
    }
    buffer[(*position)++] = simulator_hex_digit((unsigned int)value >> 4U);
    buffer[(*position)++] = simulator_hex_digit(value);
    buffer[*position] = '\0';
    return true;
}

static bool simulator_pid_supported(uint8_t pid)
{
    switch (pid) {
    case 0x01U: case 0x04U: case 0x05U: case 0x0bU: case 0x0cU: case 0x0dU:
    case 0x0fU: case 0x10U: case 0x11U: case 0x23U: case 0x2cU:
    case 0x2dU: case 0x33U: case 0x3cU: case 0x42U: case 0x46U:
    case 0x5cU: case 0x5eU: case 0x78U: case 0x7aU: case 0x7cU:
        return true;
    default:
        return false;
    }
}

static bool simulator_has_pid_after(uint8_t pid)
{
    unsigned int value;
    for (value = (unsigned int)pid + 1U; value <= 0xffU; ++value) {
        if (simulator_pid_supported((uint8_t)value)) return true;
    }
    return false;
}

static bool simulator_supported_pid_response(
    LinkElm327Simulator *simulator,
    uint8_t base, char *response, size_t response_size)
{
    uint32_t mask = 0U;
    size_t position = 0U;
    unsigned int offset;

    if (base != 0x00U && base != 0x20U && base != 0x40U && base != 0x60U) {
        return false;
    }
    for (offset = 1U; offset <= 32U; ++offset) {
        const uint8_t pid = (uint8_t)((unsigned int)base + offset);
        const bool supported = offset == 32U
            ? simulator_has_pid_after(pid)
            : simulator_pid_supported(pid);
        if (supported) mask |= UINT32_C(1) << (32U - offset);
    }

    response[0] = '\0';
    if (simulator != NULL && simulator->response_headers) {
        const int prefix = snprintf(response, response_size, "7E8");
        if (prefix != 3) return false;
        position = 3U;
        if (!simulator_append_byte(response, response_size, &position, 6U))
            return false;
    }
    if (!simulator_append_byte(response, response_size, &position, 0x41U) ||
        !simulator_append_byte(response, response_size, &position, base) ||
        !simulator_append_byte(response, response_size, &position,
                               (uint8_t)(mask >> 24U)) ||
        !simulator_append_byte(response, response_size, &position,
                               (uint8_t)(mask >> 16U)) ||
        !simulator_append_byte(response, response_size, &position,
                               (uint8_t)(mask >> 8U)) ||
        !simulator_append_byte(response, response_size, &position,
                               (uint8_t)mask)) {
        return false;
    }
    return true;
}

static bool simulator_live_payload(
    LinkElm327Simulator *simulator,
    uint8_t pid,
    uint8_t *data,
    size_t data_size,
    size_t *data_length)
{
    const uint64_t phase = simulator->sample_counter++ % UINT64_C(40);
    unsigned int raw;

    if (data == NULL || data_length == NULL || data_size < 3U) return false;
    *data_length = 0U;
    switch (pid) {
    case 0x01U:
        /* PID 01: MIL on, one confirmed DTC, compression-ignition monitors. */
        if (data_size < 4U) return false;
        data[0] = 0x81U;
        data[1] = 0x0fU;
        data[2] = 0x80U;
        data[3] = 0x00U;
        *data_length = 4U;
        return true;
    case 0x04U:
        data[0] = (uint8_t)(100U + (unsigned int)(phase % 40U)); *data_length = 1U; return true;
    case 0x05U:
        data[0] = (uint8_t)(126U + (unsigned int)(phase % 4U)); *data_length = 1U; return true;
    case 0x0bU:
        data[0] = (uint8_t)(96U + (unsigned int)(phase % 22U)); *data_length = 1U; return true;
    case 0x0cU:
        raw = (780U + (unsigned int)(phase * 61U)) * 4U;
        data[0] = (uint8_t)(raw >> 8U); data[1] = (uint8_t)raw; *data_length = 2U; return true;
    case 0x0dU:
        data[0] = (uint8_t)((phase * 7U) % 116U); *data_length = 1U; return true;
    case 0x0fU:
        data[0] = (uint8_t)(70U + (unsigned int)(phase % 5U)); *data_length = 1U; return true;
    case 0x10U:
        raw = 1400U + (unsigned int)(phase * 83U);
        data[0] = (uint8_t)(raw >> 8U); data[1] = (uint8_t)raw; *data_length = 2U; return true;
    case 0x11U:
        data[0] = (uint8_t)(42U + (unsigned int)(phase * 3U)); *data_length = 1U; return true;
    case 0x23U:
        raw = 3200U + (unsigned int)(phase * 75U);
        data[0] = (uint8_t)(raw >> 8U); data[1] = (uint8_t)raw; *data_length = 2U; return true;
    case 0x2cU:
        data[0] = (uint8_t)(55U + (unsigned int)(phase * 2U)); *data_length = 1U; return true;
    case 0x2dU:
        data[0] = (uint8_t)(124U + (unsigned int)(phase % 9U)); *data_length = 1U; return true;
    case 0x33U:
        data[0] = 100U; *data_length = 1U; return true;
    case 0x3cU:
        raw = (430U + (unsigned int)(phase * 4U) + 40U) * 10U;
        data[0] = (uint8_t)(raw >> 8U); data[1] = (uint8_t)raw; *data_length = 2U; return true;
    case 0x42U:
        raw = 13800U + (unsigned int)((phase % 6U) * 100U);
        data[0] = (uint8_t)(raw >> 8U); data[1] = (uint8_t)raw; *data_length = 2U; return true;
    case 0x46U:
        data[0] = 65U; *data_length = 1U; return true;
    case 0x5cU:
        data[0] = (uint8_t)(126U + (unsigned int)(phase % 5U)); *data_length = 1U; return true;
    case 0x5eU:
        raw = 120U + (unsigned int)(phase * 5U);
        data[0] = (uint8_t)(raw >> 8U); data[1] = (uint8_t)raw; *data_length = 2U; return true;
    case 0x78U:
        if (data_size < 9U) return false;
        raw = (360U + (unsigned int)(phase * 5U) + 40U) * 10U;
        data[0] = 0x0fU;
        data[1] = (uint8_t)(raw >> 8U); data[2] = (uint8_t)raw;
        raw += 80U;
        data[3] = (uint8_t)(raw >> 8U); data[4] = (uint8_t)raw;
        raw += 80U;
        data[5] = (uint8_t)(raw >> 8U); data[6] = (uint8_t)raw;
        raw += 80U;
        data[7] = (uint8_t)(raw >> 8U); data[8] = (uint8_t)raw;
        *data_length = 9U;
        return true;
    case 0x7aU:
        if (data_size < 7U) return false;
        raw = 80U + (unsigned int)(phase * 15U);
        data[0] = 0x07U;
        data[1] = (uint8_t)(raw >> 8U); data[2] = (uint8_t)raw;
        raw += 20U;
        data[3] = (uint8_t)(raw >> 8U); data[4] = (uint8_t)raw;
        raw += 20U;
        data[5] = (uint8_t)(raw >> 8U); data[6] = (uint8_t)raw;
        *data_length = 7U;
        return true;
    case 0x7cU:
        if (data_size < 9U) return false;
        raw = (330U + (unsigned int)(phase * 6U) + 40U) * 10U;
        data[0] = 0x0fU;
        data[1] = (uint8_t)(raw >> 8U); data[2] = (uint8_t)raw;
        raw += 100U;
        data[3] = (uint8_t)(raw >> 8U); data[4] = (uint8_t)raw;
        raw += 100U;
        data[5] = (uint8_t)(raw >> 8U); data[6] = (uint8_t)raw;
        raw += 100U;
        data[7] = (uint8_t)(raw >> 8U); data[8] = (uint8_t)raw;
        *data_length = 9U;
        return true;
    default:
        return false;
    }
}

static bool simulator_live_response(
    LinkElm327Simulator *simulator,
    uint8_t service,
    uint8_t pid,
    bool frame_number_present,
    uint8_t frame_number,
    char *response,
    size_t response_size)
{
    uint8_t data[16];
    size_t data_length = 0U;
    size_t position = 0U;
    size_t index;
    const size_t fixed_payload_length = 2U + (frame_number_present ? 1U : 0U);

    if (!simulator_live_payload(simulator, pid, data, sizeof(data), &data_length)) {
        return false;
    }
    response[0] = '\0';
    if (simulator->response_headers && fixed_payload_length + data_length <= 8U) {
        const int prefix = snprintf(response, response_size, "7E8");
        const size_t payload_length = fixed_payload_length + data_length;
        if (prefix != 3) return false;
        position = 3U;
        if (!simulator_append_byte(
                response, response_size, &position,
                (uint8_t)payload_length)) {
            return false;
        }
    }
    if (!simulator_append_byte(response, response_size, &position, service) ||
        !simulator_append_byte(response, response_size, &position, pid)) {
        return false;
    }
    if (frame_number_present &&
        !simulator_append_byte(response, response_size, &position, frame_number)) {
        return false;
    }
    for (index = 0U; index < data_length; ++index) {
        if (!simulator_append_byte(response, response_size, &position, data[index])) {
            return false;
        }
    }
    return true;
}

static bool simulator_vin_response(
    const LinkElm327Simulator *simulator,
    char *response,
    size_t response_size)
{
    static const char fallback_vin[] = "SMLATRTEST0000001";
    const char *vin = simulator->config.vin;
    size_t position = 0U;
    size_t index;

    if (vin == NULL || strlen(vin) != 17U) vin = fallback_vin;
    response[0] = '\0';
    if (!simulator_append_byte(response, response_size, &position, 0x49U) ||
        !simulator_append_byte(response, response_size, &position, 0x02U) ||
        !simulator_append_byte(response, response_size, &position, 0x01U)) {
        return false;
    }
    for (index = 0U; index < 17U; ++index) {
        if (!simulator_append_byte(response, response_size, &position,
                                   (uint8_t)vin[index])) {
            return false;
        }
    }
    return true;
}

static bool simulator_parse_byte(const char *text, uint8_t *value)
{
    int high, low;
    if (text == NULL || value == NULL || text[0] == '\0' || text[1] == '\0') {
        return false;
    }
    high = simulator_hex_value(text[0]);
    low = simulator_hex_value(text[1]);
    if (high < 0 || low < 0) return false;
    *value = (uint8_t)(((unsigned int)high << 4U) | (unsigned int)low);
    return true;
}

static bool simulator_default_response(
    LinkElm327Simulator *simulator,
    const char *command,
    char *response,
    size_t response_size)
{
    uint8_t pid;

    if (strcmp(command, "ATZ") == 0 || strcmp(command, "ATWS") == 0) {
        const char *identifier = simulator->config.adapter_identifier;
        if (identifier == NULL || identifier[0] == '\0') identifier = "ELM327 v2.3 SIM";
        (void)snprintf(response, response_size, "%s", identifier);
        simulator->echo = true;
        simulator->response_headers = false;
        simulator->sample_counter = 0U;
        return true;
    }
    if (strcmp(command, "ATE0") == 0) {
        (void)snprintf(response, response_size, "OK"); simulator->echo = false; return true;
    }
    if (strcmp(command, "ATE1") == 0) {
        (void)snprintf(response, response_size, "OK"); simulator->echo = true; return true;
    }
    if (strcmp(command, "ATH0") == 0) {
        (void)snprintf(response, response_size, "OK");
        simulator->response_headers = false;
        return true;
    }
    if (strcmp(command, "ATH1") == 0) {
        (void)snprintf(response, response_size, "OK");
        simulator->response_headers = true;
        return true;
    }
    if (strcmp(command, "ATL0") == 0 || strcmp(command, "ATL1") == 0 ||
        strcmp(command, "ATS0") == 0 || strcmp(command, "ATS1") == 0 ||
        strcmp(command, "ATSP0") == 0 || strcmp(command, "ATSP6") == 0 ||
        strcmp(command, "ATCAF1") == 0 || strcmp(command, "ATCFC1") == 0 ||
        strncmp(command, "ATSH", 4U) == 0 || strncmp(command, "ATCRA", 5U) == 0) {
        (void)snprintf(response, response_size, "OK"); return true;
    }
    if (strcmp(command, "ATI") == 0) {
        const char *identifier = simulator->config.adapter_identifier;
        if (identifier == NULL || identifier[0] == '\0') identifier = "ELM327 v2.3 SIM";
        (void)snprintf(response, response_size, "%s", identifier); return true;
    }
    if (strcmp(command, "AT@1") == 0) {
        (void)snprintf(response, response_size, "LINK ELM327 SIMULATOR"); return true;
    }
    if (strcmp(command, "ATDP") == 0) {
        (void)snprintf(response, response_size, "AUTO, ISO 15765-4 (CAN 11/500)"); return true;
    }
    if (strcmp(command, "ATDPN") == 0) {
        (void)snprintf(response, response_size, "A6"); return true;
    }

    if (strlen(command) == 4U && command[0] == '0' && command[1] == '1' &&
        simulator_parse_byte(command + 2, &pid) &&
        (pid == 0x00U || pid == 0x20U || pid == 0x40U || pid == 0x60U)) {
        return simulator_supported_pid_response(
            simulator, pid, response, response_size);
    }
    if (strcmp(command, "03") == 0) {
        (void)snprintf(response, response_size, "4304010101"); return true;
    }
    if (strcmp(command, "07") == 0) {
        (void)snprintf(response, response_size, "470299"); return true;
    }
    if (strcmp(command, "0A") == 0) {
        (void)snprintf(response, response_size, "4A0401"); return true;
    }
    if (strcmp(command, "0902") == 0) {
        return simulator_vin_response(simulator, response, response_size);
    }
    if (strlen(command) == 4U && command[0] == '0' && command[1] == '1' &&
        simulator_parse_byte(command + 2, &pid)) {
        return simulator_live_response(simulator, 0x41U, pid, false, 0U,
                                       response, response_size);
    }
    if (strlen(command) == 6U && command[0] == '0' && command[1] == '2' &&
        simulator_parse_byte(command + 2, &pid)) {
        uint8_t frame = 0U;
        if (!simulator_parse_byte(command + 4, &frame)) return false;
        return simulator_live_response(simulator, 0x42U, pid, true, frame,
                                       response, response_size);
    }

    if (simulator->config.custom_responder != NULL &&
        simulator->config.custom_responder(
            simulator->config.custom_context, command,
            response, response_size)) {
        return true;
    }
    return false;
}

static void simulator_deliver(
    LinkElm327Simulator *simulator,
    const char *command,
    const char *body,
    bool echo_for_response)
{
    char wire[LINK_ELM327_MAX_COMMAND + LINK_ELM327_MAX_RESPONSE + 8U];
    size_t position = 0U;
    size_t length;

    if (simulator->receiver == NULL || body == NULL) return;
    if (echo_for_response) {
        length = strlen(command);
        if (length + 1U >= sizeof(wire)) return;
        memcpy(wire + position, command, length); position += length;
        wire[position++] = '\r';
    }
    length = strlen(body);
    if (position + length + 2U > sizeof(wire)) return;
    memcpy(wire + position, body, length); position += length;
    wire[position++] = '\r';
    wire[position++] = '>';
    simulator->receiver(simulator->receiver_context,
                        (const uint8_t *)wire, position);
}

static LinkTransportStatus simulator_connect(void *context)
{
    LinkElm327Simulator *simulator = context;
    if (simulator == NULL) return LINK_TRANSPORT_INVALID_ARGUMENT;
    simulator->connected = true;
    simulator->command_length = 0U;
    return LINK_TRANSPORT_OK;
}

static void simulator_disconnect(void *context)
{
    LinkElm327Simulator *simulator = context;
    if (simulator == NULL) return;
    simulator->connected = false;
    simulator->command_length = 0U;
}

static bool simulator_is_connected(void *context)
{
    const LinkElm327Simulator *simulator = context;
    return simulator != NULL && simulator->connected;
}

static LinkTransportStatus simulator_write(
    void *context, const uint8_t *data, size_t size)
{
    LinkElm327Simulator *simulator = context;
    size_t index;

    if (simulator == NULL || (data == NULL && size != 0U)) {
        return LINK_TRANSPORT_INVALID_ARGUMENT;
    }
    if (!simulator->connected) return LINK_TRANSPORT_NOT_CONNECTED;

    for (index = 0U; index < size; ++index) {
        const uint8_t value = data[index];
        if (value == (uint8_t)'\r' || value == (uint8_t)'\n') {
            char command[LINK_ELM327_MAX_COMMAND];
            char response[LINK_ELM327_MAX_RESPONSE];
            bool echo_for_response;

            if (simulator->command_length == 0U) continue;
            simulator->command[simulator->command_length] = '\0';
            if (!simulator_canonicalise(simulator->command,
                                        command, sizeof(command))) {
                simulator->command_length = 0U;
                return LINK_TRANSPORT_INVALID_ARGUMENT;
            }
            simulator->command_length = 0U;
            echo_for_response = simulator->echo;
            response[0] = '\0';
            if (!simulator_default_response(
                    simulator, command, response, sizeof(response))) {
                (void)snprintf(response, sizeof(response), "?");
            }
            simulator_deliver(simulator, command, response, echo_for_response);
            continue;
        }
        if (value < 0x20U || value > 0x7eU || value == (uint8_t)'>') {
            simulator->command_length = 0U;
            return LINK_TRANSPORT_INVALID_ARGUMENT;
        }
        if (simulator->command_length + 1U >= sizeof(simulator->command)) {
            simulator->command_length = 0U;
            return LINK_TRANSPORT_IO_ERROR;
        }
        simulator->command[simulator->command_length++] = (char)value;
    }
    return LINK_TRANSPORT_OK;
}

static void simulator_set_receiver(
    void *context, LinkTransportReceiveFn receiver, void *receiver_context)
{
    LinkElm327Simulator *simulator = context;
    if (simulator == NULL) return;
    simulator->receiver = receiver;
    simulator->receiver_context = receiver_context;
}

void link_elm327_simulator_init(
    LinkElm327Simulator *simulator,
    const LinkElm327SimulatorConfig *config)
{
    LinkElm327SimulatorConfig resolved = LINK_ELM327_SIMULATOR_CONFIG_INIT;
    if (simulator == NULL) return;
    if (config != NULL) resolved = *config;
    memset(simulator, 0, sizeof(*simulator));
    simulator->config = resolved;
    simulator->echo = true;
}

LinkTransport link_elm327_simulator_transport(LinkElm327Simulator *simulator)
{
    LinkTransport transport = LINK_TRANSPORT_INIT;
    transport.context = simulator;
    transport.connect = simulator_connect;
    transport.disconnect = simulator_disconnect;
    transport.is_connected = simulator_is_connected;
    transport.write = simulator_write;
    transport.set_receiver = simulator_set_receiver;
    return transport;
}
