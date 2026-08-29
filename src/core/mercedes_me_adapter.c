// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/mercedes_me_adapter.h"

#include <stddef.h>

static unsigned char ascii_lower(unsigned char value)
{
    if (value >= (unsigned char)'A' && value <= (unsigned char)'Z')
        return (unsigned char)(value - (unsigned char)'A' + (unsigned char)'a');
    return value;
}

static bool prefix_nocase(const char *text, const char *prefix)
{
    size_t i = 0U;
    if (text == NULL || prefix == NULL) return false;
    while (prefix[i] != '\0') {
        if (text[i] == '\0' ||
            ascii_lower((unsigned char)text[i]) !=
            ascii_lower((unsigned char)prefix[i])) return false;
        ++i;
    }
    return true;
}

LinkMercedesMeAdapterFamily link_mercedes_me_adapter_family_from_name(
    const char *name)
{
    unsigned char selector;
    if (name == NULL) return LINK_MERCEDES_ME_ADAPTER_UNKNOWN;
    if (prefix_nocase(name, "VAN-")) return LINK_MERCEDES_ME_ADAPTER_OTHER_APPS;
    if (!prefix_nocase(name, "MB-") || name[3] == '\0')
        return LINK_MERCEDES_ME_ADAPTER_UNKNOWN;
    selector = (unsigned char)name[3];
    if (selector == '1' || selector == '8' || selector == '9')
        return LINK_MERCEDES_ME_ADAPTER_BLE;
    if (selector == '2' || selector == '3' ||
        selector == '4' || selector == '6')
        return LINK_MERCEDES_ME_ADAPTER_FIRST_GENERATION;
    if (selector == '5' || selector == '7')
        return LINK_MERCEDES_ME_ADAPTER_SECOND_GENERATION;
    return LINK_MERCEDES_ME_ADAPTER_UNKNOWN;
}

const char *link_mercedes_me_adapter_family_name(
    LinkMercedesMeAdapterFamily family)
{
    switch (family) {
    case LINK_MERCEDES_ME_ADAPTER_UNKNOWN: return "unknown";
    case LINK_MERCEDES_ME_ADAPTER_BLE: return "ble";
    case LINK_MERCEDES_ME_ADAPTER_FIRST_GENERATION: return "first-generation";
    case LINK_MERCEDES_ME_ADAPTER_SECOND_GENERATION: return "second-generation";
    case LINK_MERCEDES_ME_ADAPTER_OTHER_APPS: return "other-apps";
    }
    return "unknown";
}

bool link_mercedes_me_adapter_prefers_ble(LinkMercedesMeAdapterFamily family)
{
    return family == LINK_MERCEDES_ME_ADAPTER_BLE;
}

bool link_mercedes_me_adapter_prefers_classic_spp(
    LinkMercedesMeAdapterFamily family)
{
    return family == LINK_MERCEDES_ME_ADAPTER_FIRST_GENERATION ||
           family == LINK_MERCEDES_ME_ADAPTER_SECOND_GENERATION;
}

const char *link_mercedes_me_qos_state_name(int ordinal)
{
    switch (ordinal) {
    case 0: return "QOS_STOPPED";
    case 1: return "QOS_GOOD";
    case 2: return "QOS_WEAK";
    case 3: return "QOS_BAD";
    default: return "QOS_UNKNOWN";
    }
}

const char *link_mercedes_me_execution_state_name(int ordinal)
{
    switch (ordinal) {
    case 0: return "SUCCESS";
    case 1: return "STATE_ERROR";
    case 2: return "COMMUNICATION_ERROR";
    case 3: return "ADAPTER_ERROR";
    default: return "EXECUTION_UNKNOWN";
    }
}

const char *link_mercedes_me_reason_name(int ordinal)
{
    static const char *const names[] = {
        "R_STOPPED",
        "R_STARTED",
        "R_NAME_NOT_FOUND",
        "R_NO_PING",
        "R_WRONG_PARTMU_VERSION",
        "R_BT_NOT_AKTIVE",
        "R_DEVICE_DISAPPEARED",
        "R_DEVICE_NO_SPP",
        "R_NOT_CONNECTABLE",
        "R_ILLEGAL_STATE_BT_DISCOVERY",
        "R_CONNECT_TIMEOUT",
        "R_SMK_NOT_AVAILABLE",
        "R_SMK_INVALID_PASSKEY",
        "R_SMK_BLOCK_TEMPORARY",
        "R_ADAPTER_ERROR_NO_AUTHENTICATION_01",
        "R_ADAPTER_ERROR_AUTHENTICATION_FAILED_02",
        "R_ADAPTER_ERROR_ADC_SELFTEST_ERROR_03",
        "R_ADAPTER_ERROR_MAC_ERROR_04",
        "R_ADAPTER_ERROR_SELFTEST_ERROR_05",
        "R_ADAPTER_ERROR_INVALID_MAC_MAPPING_11",
        "R_ADAPTER_ERROR_BT_RX_OVERFLOW_18",
        "R_ADAPTER_ERROR_CAN_OFF_21",
        "R_ADAPTER_ERROR_BT_TX_OVERFLOW_30",
        "R_ADAPTER_ERROR_CAN_RX_OVERFLOW_31",
        "R_ADAPTER_ERROR_CAN_TX_OVERFLOW_32",
        "R_BUS_ERROR_35",
        "R_GATT_FAILURE"
    };
    if (ordinal < 0 ||
        (size_t)ordinal >= sizeof(names) / sizeof(names[0]))
        return "R_UNKNOWN";
    return names[ordinal];
}

const char *link_mercedes_me_comm_state_name(int ordinal)
{
    switch (ordinal) {
    case 1: return "COMM_STATE_STARTED";
    case 2: return "COMM_STATE_NAME_NOT_FOUND";
    case 5: return "COMM_STATE_BT_NOT_AKTIVE";
    case 7: return "COMM_STATE_DEVICE_NO_SPP";
    case 8: return "COMM_STATE_NOT_CONNECTABLE";
    case 9: return "COMM_STATE_ILLEGAL_STATE_BT_DISCOVERY";
    case 10: return "COMM_STATE_CONNECT_TIMEOUT";
    case 11: return "COMM_STATE_GATT_FAILURE";
    default: return "COMM_STATE_UNKNOWN";
    }
}

bool link_mercedes_me_command_has_valid_terminator(
    const uint8_t *bytes,
    size_t size)
{
    return bytes != NULL && size != 0U &&
           bytes[size - 1U] == LINK_MERCEDES_ME_RECORD_TERMINATOR;
}

void link_mercedes_me_stream_parser_init(LinkMercedesMeStreamParser *parser)
{
    if (parser == NULL) return;
    parser->used = 0U;
    parser->overflow_count = 0U;
}

size_t link_mercedes_me_stream_parser_feed(
    LinkMercedesMeStreamParser *parser,
    const uint8_t *bytes,
    size_t size,
    LinkMercedesMeStreamEventFn event_fn,
    void *event_context)
{
    size_t index;
    size_t emitted = 0U;

    if (parser == NULL || (bytes == NULL && size != 0U)) return 0U;

    for (index = 0U; index < size; ++index) {
        const uint8_t value = bytes[index];
        const bool terminal =
            value == LINK_MERCEDES_ME_RECORD_TERMINATOR ||
            value == LINK_MERCEDES_ME_NACK_TERMINATOR;

        if (!terminal &&
            parser->used >= LINK_MERCEDES_ME_RX_CLEAR_THRESHOLD) {
            if (event_fn != NULL) {
                event_fn(event_context,
                         LINK_MERCEDES_ME_STREAM_OVERFLOW,
                         parser->bytes,
                         parser->used);
            }
            ++emitted;
            ++parser->overflow_count;
            parser->used = 0U;
            /*
             * The archived bridge clears the full accumulator at this point
             * and does not append the byte that triggered the overflow.
             */
            continue;
        }

        if (parser->used >= LINK_MERCEDES_ME_RX_BUFFER_CAPACITY) {
            ++parser->overflow_count;
            parser->used = 0U;
            continue;
        }

        parser->bytes[parser->used++] = value;
        if (terminal) {
            if (event_fn != NULL) {
                event_fn(event_context,
                         value == LINK_MERCEDES_ME_NACK_TERMINATOR
                             ? LINK_MERCEDES_ME_STREAM_NACK
                             : LINK_MERCEDES_ME_STREAM_RECORD,
                         parser->bytes,
                         parser->used);
            }
            ++emitted;
            parser->used = 0U;
        }
    }

    return emitted;
}
