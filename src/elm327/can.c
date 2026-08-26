// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file can.c
 * @brief ELM327-managed ISO 15765 CAN diagnostic channel implementation.
 */
#include "link/elm327_can.h"

#include <stdio.h>
#include <string.h>

#define ELM327_CAN_MAX_DECODED_PDU (LINK_ELM327_MAX_RESPONSE / 2U)

static bool elm327_can_id_valid(uint32_t can_id, bool extended)
{
    return extended ? can_id <= UINT32_C(0x1fffffff)
                    : can_id <= UINT32_C(0x7ff);
}

static int elm327_can_hex_value(char value)
{
    if (value >= '0' && value <= '9') {
        return (int)(value - '0');
    }
    if (value >= 'A' && value <= 'F') {
        return (int)(value - 'A') + 10;
    }
    if (value >= 'a' && value <= 'f') {
        return (int)(value - 'a') + 10;
    }
    return -1;
}

static char elm327_can_hex_digit(unsigned int value)
{
    static const char digits[] = "0123456789ABCDEF";
    return digits[value & 0x0fU];
}

static bool elm327_can_space(char value)
{
    return value == ' ' || value == '\t' || value == '\r';
}

static LinkElm327CanResult elm327_can_format_id_command(
    const char *prefix,
    uint32_t can_id,
    bool extended,
    char *buffer,
    size_t buffer_size)
{
    char temporary[32];
    int written;

    if (prefix == NULL || buffer == NULL || buffer_size == 0U ||
        !elm327_can_id_valid(can_id, extended)) {
        if (buffer != NULL && buffer_size != 0U) {
            buffer[0] = '\0';
        }
        return LINK_ELM327_CAN_RESULT_INVALID_ARGUMENT;
    }

    buffer[0] = '\0';
    written = extended
        ? snprintf(temporary, sizeof(temporary), "%s%08lX", prefix,
                   (unsigned long)can_id)
        : snprintf(temporary, sizeof(temporary), "%s%03lX", prefix,
                   (unsigned long)can_id);
    if (written < 0 || (size_t)written >= sizeof(temporary)) {
        return LINK_ELM327_CAN_RESULT_INVALID_ARGUMENT;
    }
    if ((size_t)written + 1U > buffer_size) {
        return LINK_ELM327_CAN_RESULT_BUFFER_TOO_SMALL;
    }

    memcpy(buffer, temporary, (size_t)written + 1U);
    return LINK_ELM327_CAN_RESULT_OK;
}

static LinkElm327CanResult elm327_can_parse_hex_line(
    const char *line,
    size_t line_length,
    uint8_t *bytes,
    size_t bytes_size,
    size_t *byte_count)
{
    size_t index;
    size_t count = 0U;
    int high = -1;

    if (line == NULL || bytes == NULL || byte_count == NULL) {
        return LINK_ELM327_CAN_RESULT_INVALID_ARGUMENT;
    }
    *byte_count = 0U;

    for (index = 0U; index < line_length; ++index) {
        int value;
        const char current = line[index];
        if (elm327_can_space(current)) {
            continue;
        }
        value = elm327_can_hex_value(current);
        if (value < 0) {
            return LINK_ELM327_CAN_RESULT_MALFORMED_RESPONSE;
        }
        if (high < 0) {
            high = value;
            continue;
        }
        if (count >= bytes_size) {
            return LINK_ELM327_CAN_RESULT_PDU_TOO_LARGE;
        }
        bytes[count++] = (uint8_t)(((unsigned int)high << 4U) |
                                   (unsigned int)value);
        high = -1;
    }

    if (high >= 0 || count == 0U) {
        return LINK_ELM327_CAN_RESULT_MALFORMED_RESPONSE;
    }
    *byte_count = count;
    return LINK_ELM327_CAN_RESULT_OK;
}

static bool elm327_can_parse_declared_length(
    const char *line,
    size_t line_length,
    size_t *length)
{
    size_t index;
    size_t value = 0U;
    size_t digits = 0U;

    if (line == NULL || length == NULL) {
        return false;
    }
    for (index = 0U; index < line_length; ++index) {
        int nibble;
        if (elm327_can_space(line[index])) {
            continue;
        }
        nibble = elm327_can_hex_value(line[index]);
        if (nibble < 0 || digits >= 3U) {
            return false;
        }
        value = (value << 4U) | (size_t)nibble;
        digits++;
    }
    if (digits != 3U || value == 0U) {
        return false;
    }
    *length = value;
    return true;
}

static LinkElm327CanResult elm327_can_decode_indexed(
    const LinkElm327Response *response,
    uint8_t *temporary,
    size_t temporary_size,
    size_t *decoded_length,
    bool *found)
{
    const char *cursor;
    unsigned int expected_index = 0U;
    size_t written = 0U;
    size_t declared_length = 0U;
    bool declared_length_seen = false;
    bool collecting = false;

    *decoded_length = 0U;
    *found = false;
    cursor = response->text;

    while (*cursor != '\0') {
        const char *end = strchr(cursor, '\n');
        const size_t line_length = end == NULL ? strlen(cursor)
                                                : (size_t)(end - cursor);
        size_t first = 0U;
        size_t last = line_length;
        size_t colon = SIZE_MAX;
        size_t index;

        while (first < last && elm327_can_space(cursor[first])) {
            first++;
        }
        while (last > first && elm327_can_space(cursor[last - 1U])) {
            last--;
        }
        for (index = first; index < last; ++index) {
            if (cursor[index] == ':') {
                colon = index;
                break;
            }
        }

        if (colon != SIZE_MAX) {
            uint8_t line_bytes[256];
            size_t line_byte_count = 0U;
            size_t prefix = first;
            int line_index;
            LinkElm327CanResult result;

            while (prefix < colon && elm327_can_space(cursor[prefix])) {
                prefix++;
            }
            if (prefix + 1U != colon) {
                return LINK_ELM327_CAN_RESULT_MALFORMED_RESPONSE;
            }
            line_index = elm327_can_hex_value(cursor[prefix]);
            if (line_index < 0) {
                return LINK_ELM327_CAN_RESULT_MALFORMED_RESPONSE;
            }
            if (!collecting) {
                if (line_index != 0) {
                    return LINK_ELM327_CAN_RESULT_MALFORMED_RESPONSE;
                }
                collecting = true;
                *found = true;
                expected_index = 0U;
            }
            if ((unsigned int)line_index != expected_index) {
                return LINK_ELM327_CAN_RESULT_MALFORMED_RESPONSE;
            }

            result = elm327_can_parse_hex_line(
                cursor + colon + 1U, last - colon - 1U,
                line_bytes, sizeof(line_bytes), &line_byte_count);
            if (result != LINK_ELM327_CAN_RESULT_OK) {
                return result;
            }
            if (line_byte_count > temporary_size - written) {
                return LINK_ELM327_CAN_RESULT_PDU_TOO_LARGE;
            }
            memcpy(temporary + written, line_bytes, line_byte_count);
            written += line_byte_count;
            expected_index = (expected_index + 1U) & 0x0fU;
        } else if (!collecting) {
            size_t parsed_length = 0U;
            if (elm327_can_parse_declared_length(
                    cursor + first, last - first, &parsed_length)) {
                if (declared_length_seen || parsed_length > temporary_size) {
                    return LINK_ELM327_CAN_RESULT_MALFORMED_RESPONSE;
                }
                declared_length = parsed_length;
                declared_length_seen = true;
            }
        } else if (first != last) {
            return LINK_ELM327_CAN_RESULT_UNEXPECTED_RESPONSE;
        }

        if (end == NULL) {
            break;
        }
        cursor = end + 1;
    }

    if (*found) {
        if (written == 0U ||
            (declared_length_seen && written != declared_length)) {
            return LINK_ELM327_CAN_RESULT_MALFORMED_RESPONSE;
        }
        *decoded_length = written;
    }
    return LINK_ELM327_CAN_RESULT_OK;
}

static LinkElm327CanResult elm327_can_decode_unindexed(
    const LinkElm327Response *response,
    uint8_t *temporary,
    size_t temporary_size,
    size_t *decoded_length)
{
    const char *cursor;
    bool pending_seen = false;
    bool final_seen = false;
    uint8_t pending_service = 0U;

    if (response == NULL || temporary == NULL || decoded_length == NULL) {
        return LINK_ELM327_CAN_RESULT_INVALID_ARGUMENT;
    }
    *decoded_length = 0U;
    cursor = response->text;
    while (*cursor != '\0') {
        const char *end = strchr(cursor, '\n');
        const size_t line_length = end == NULL ? strlen(cursor) : (size_t)(end - cursor);
        size_t first = 0U;
        size_t last = line_length;
        uint8_t line_bytes[ELM327_CAN_MAX_DECODED_PDU];
        size_t line_byte_count = 0U;
        LinkElm327CanResult result;

        while (first < last && elm327_can_space(cursor[first])) first++;
        while (last > first && elm327_can_space(cursor[last - 1U])) last--;
        if (first != last) {
            result = elm327_can_parse_hex_line(
                cursor + first, last - first,
                line_bytes, sizeof(line_bytes), &line_byte_count);
            if (result != LINK_ELM327_CAN_RESULT_OK) return result;

            if (line_byte_count == 3U && line_bytes[0] == 0x7fU &&
                line_bytes[2] == 0x78U && !final_seen) {
                if (pending_seen && pending_service != line_bytes[1])
                    return LINK_ELM327_CAN_RESULT_UNEXPECTED_RESPONSE;
                pending_seen = true;
                pending_service = line_bytes[1];
            } else {
                if (final_seen) return LINK_ELM327_CAN_RESULT_UNEXPECTED_RESPONSE;
                if (pending_seen) {
                    const bool matching_positive = line_bytes[0] == (uint8_t)(pending_service + 0x40U);
                    const bool matching_negative = line_byte_count >= 3U &&
                        line_bytes[0] == 0x7fU && line_bytes[1] == pending_service;
                    if (!matching_positive && !matching_negative)
                        return LINK_ELM327_CAN_RESULT_UNEXPECTED_RESPONSE;
                }
                if (line_byte_count > temporary_size)
                    return LINK_ELM327_CAN_RESULT_PDU_TOO_LARGE;
                memcpy(temporary, line_bytes, line_byte_count);
                *decoded_length = line_byte_count;
                final_seen = true;
            }
        }
        if (end == NULL) break;
        cursor = end + 1;
    }
    return final_seen ? LINK_ELM327_CAN_RESULT_OK
                      : LINK_ELM327_CAN_RESULT_UNEXPECTED_RESPONSE;
}

const char *link_elm327_can_result_name(LinkElm327CanResult result)
{
    switch (result) {
    case LINK_ELM327_CAN_RESULT_OK:
        return "ok";
    case LINK_ELM327_CAN_RESULT_INVALID_ARGUMENT:
        return "invalid-argument";
    case LINK_ELM327_CAN_RESULT_BUFFER_TOO_SMALL:
        return "buffer-too-small";
    case LINK_ELM327_CAN_RESULT_PDU_TOO_LARGE:
        return "pdu-too-large";
    case LINK_ELM327_CAN_RESULT_ELM_ERROR:
        return "elm-error";
    case LINK_ELM327_CAN_RESULT_MALFORMED_RESPONSE:
        return "malformed-response";
    case LINK_ELM327_CAN_RESULT_UNEXPECTED_RESPONSE:
        return "unexpected-response";
    case LINK_ELM327_CAN_RESULT_FAILED_STATE:
        return "failed-state";
    }
    return "unknown";
}

const char *link_elm327_can_stage_name(LinkElm327CanStage stage)
{
    switch (stage) {
    case LINK_ELM327_CAN_STAGE_SET_HEADER:
        return "set-header";
    case LINK_ELM327_CAN_STAGE_SET_RECEIVE_ADDRESS:
        return "set-receive-address";
    case LINK_ELM327_CAN_STAGE_ENABLE_AUTO_FORMATTING:
        return "enable-auto-formatting";
    case LINK_ELM327_CAN_STAGE_ENABLE_FLOW_CONTROL:
        return "enable-flow-control";
    case LINK_ELM327_CAN_STAGE_COMPLETE:
        return "complete";
    case LINK_ELM327_CAN_STAGE_FAILED:
        return "failed";
    }
    return "unknown";
}

bool link_elm327_can_channel_config_is_valid(
    const LinkElm327CanChannelConfig *config)
{
    return config != NULL &&
           elm327_can_id_valid(config->tx_can_id, config->extended_id) &&
           elm327_can_id_valid(config->rx_can_id, config->extended_id);
}

LinkElm327CanResult link_elm327_can_channel_begin(
    LinkElm327CanChannelState *state,
    const LinkElm327CanChannelConfig *config)
{
    if (state == NULL || !link_elm327_can_channel_config_is_valid(config)) {
        return LINK_ELM327_CAN_RESULT_INVALID_ARGUMENT;
    }
    memset(state, 0, sizeof(*state));
    state->config = *config;
    state->stage = LINK_ELM327_CAN_STAGE_SET_HEADER;
    state->failure = LINK_ELM327_CAN_RESULT_OK;
    state->elm_failure = LINK_ELM327_RESULT_OK;
    return LINK_ELM327_CAN_RESULT_OK;
}

LinkElm327CanResult link_elm327_can_channel_command(
    const LinkElm327CanChannelState *state,
    char *buffer,
    size_t buffer_size)
{
    const char *fixed = NULL;
    size_t length;

    if (buffer != NULL && buffer_size != 0U) {
        buffer[0] = '\0';
    }
    if (state == NULL || buffer == NULL || buffer_size == 0U ||
        !link_elm327_can_channel_config_is_valid(&state->config)) {
        return LINK_ELM327_CAN_RESULT_INVALID_ARGUMENT;
    }

    switch (state->stage) {
    case LINK_ELM327_CAN_STAGE_SET_HEADER:
        return elm327_can_format_id_command(
            "ATSH", state->config.tx_can_id, state->config.extended_id,
            buffer, buffer_size);
    case LINK_ELM327_CAN_STAGE_SET_RECEIVE_ADDRESS:
        return elm327_can_format_id_command(
            "ATCRA", state->config.rx_can_id, state->config.extended_id,
            buffer, buffer_size);
    case LINK_ELM327_CAN_STAGE_ENABLE_AUTO_FORMATTING:
        fixed = "ATCAF1";
        break;
    case LINK_ELM327_CAN_STAGE_ENABLE_FLOW_CONTROL:
        fixed = "ATCFC1";
        break;
    case LINK_ELM327_CAN_STAGE_COMPLETE:
    case LINK_ELM327_CAN_STAGE_FAILED:
        return LINK_ELM327_CAN_RESULT_FAILED_STATE;
    }

    length = strlen(fixed);
    if (length + 1U > buffer_size) {
        return LINK_ELM327_CAN_RESULT_BUFFER_TOO_SMALL;
    }
    memcpy(buffer, fixed, length + 1U);
    return LINK_ELM327_CAN_RESULT_OK;
}

LinkElm327CanResult link_elm327_can_channel_accept(
    LinkElm327CanChannelState *state,
    const LinkElm327Response *response)
{
    if (state == NULL || response == NULL ||
        state->stage == LINK_ELM327_CAN_STAGE_COMPLETE ||
        state->stage == LINK_ELM327_CAN_STAGE_FAILED) {
        return LINK_ELM327_CAN_RESULT_INVALID_ARGUMENT;
    }
    if (response->result != LINK_ELM327_RESULT_OK) {
        state->failure = LINK_ELM327_CAN_RESULT_ELM_ERROR;
        state->elm_failure = response->result;
        state->stage = LINK_ELM327_CAN_STAGE_FAILED;
        return state->failure;
    }
    if (!response->ok_seen) {
        state->failure = LINK_ELM327_CAN_RESULT_MALFORMED_RESPONSE;
        state->elm_failure = LINK_ELM327_RESULT_MALFORMED_RESPONSE;
        state->stage = LINK_ELM327_CAN_STAGE_FAILED;
        return state->failure;
    }

    state->stage = (LinkElm327CanStage)((unsigned int)state->stage + 1U);
    return LINK_ELM327_CAN_RESULT_OK;
}

LinkElm327CanResult link_elm327_can_build_pdu_command(
    const uint8_t *pdu,
    size_t pdu_length,
    char *buffer,
    size_t buffer_size,
    size_t *written)
{
    char temporary[LINK_ELM327_MAX_COMMAND];
    size_t index;
    size_t command_length;

    if (written != NULL) {
        *written = 0U;
    }
    if (pdu == NULL || pdu_length == 0U || buffer == NULL ||
        written == NULL || buffer_size == 0U) {
        if (buffer != NULL && buffer_size != 0U) {
            buffer[0] = '\0';
        }
        return LINK_ELM327_CAN_RESULT_INVALID_ARGUMENT;
    }
    buffer[0] = '\0';
    if (pdu_length > LINK_ELM327_CAN_MAX_REQUEST_PDU) {
        return LINK_ELM327_CAN_RESULT_PDU_TOO_LARGE;
    }

    command_length = pdu_length * 2U;
    for (index = 0U; index < pdu_length; ++index) {
        temporary[index * 2U] =
            elm327_can_hex_digit((unsigned int)pdu[index] >> 4U);
        temporary[index * 2U + 1U] = elm327_can_hex_digit(pdu[index]);
    }
    temporary[command_length] = '\0';

    if (command_length + 1U > buffer_size) {
        return LINK_ELM327_CAN_RESULT_BUFFER_TOO_SMALL;
    }
    memcpy(buffer, temporary, command_length + 1U);
    *written = command_length;
    return LINK_ELM327_CAN_RESULT_OK;
}

LinkElm327CanResult link_elm327_can_decode_pdu(
    const LinkElm327Response *response,
    uint8_t *pdu,
    size_t pdu_size,
    size_t *pdu_length)
{
    uint8_t temporary[ELM327_CAN_MAX_DECODED_PDU];
    size_t decoded_length = 0U;
    bool indexed = false;
    LinkElm327CanResult result;

    if (pdu_length != NULL) {
        *pdu_length = 0U;
    }
    if (response == NULL || pdu == NULL || pdu_size == 0U ||
        pdu_length == NULL) {
        return LINK_ELM327_CAN_RESULT_INVALID_ARGUMENT;
    }
    if (response->result != LINK_ELM327_RESULT_OK) {
        return LINK_ELM327_CAN_RESULT_ELM_ERROR;
    }
    if (response->text[0] == '\0') {
        return LINK_ELM327_CAN_RESULT_MALFORMED_RESPONSE;
    }

    result = elm327_can_decode_indexed(
        response, temporary, sizeof(temporary), &decoded_length, &indexed);
    if (result != LINK_ELM327_CAN_RESULT_OK) {
        return result;
    }

    if (!indexed) {
        result = elm327_can_decode_unindexed(
            response, temporary, sizeof(temporary), &decoded_length);
        if (result != LINK_ELM327_CAN_RESULT_OK) {
            return result;
        }
    }

    if (decoded_length > pdu_size) {
        return LINK_ELM327_CAN_RESULT_BUFFER_TOO_SMALL;
    }
    memcpy(pdu, temporary, decoded_length);
    *pdu_length = decoded_length;
    return LINK_ELM327_CAN_RESULT_OK;
}
