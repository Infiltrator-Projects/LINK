// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file obd2.c
 * @brief Portable standard OBD-II request and decoding engine.
 */
#include "link/obd2.h"

#include "infiltratr/core.h"

#include <string.h>

#define OBD2_MAX_LINE_BYTES 256U
#define OBD2_MAX_MESSAGE_BYTES 512U

static int obd2_hex_value(char value)
{
    if (value >= '0' && value <= '9') return (int)(value - '0');
    if (value >= 'A' && value <= 'F') return (int)(value - 'A') + 10;
    if (value >= 'a' && value <= 'f') return (int)(value - 'a') + 10;
    return -1;
}

static bool obd2_space(char value)
{
    return value == ' ' || value == '\t' || value == '\r';
}

static char obd2_hex_digit(unsigned int value)
{
    static const char digits[] = "0123456789ABCDEF";
    return digits[value & 0x0fU];
}

static bool obd2_vin_character_valid(uint8_t value)
{
    if (value >= (uint8_t)'0' && value <= (uint8_t)'9') return true;
    return value >= (uint8_t)'A' && value <= (uint8_t)'Z' &&
           value != (uint8_t)'I' && value != (uint8_t)'O' &&
           value != (uint8_t)'Q';
}

static LinkObd2Result obd2_write_command(
    const uint8_t *bytes, size_t byte_count, char *buffer, size_t buffer_size)
{
    size_t index;
    size_t needed;

    if (bytes == NULL || byte_count == 0U || buffer == NULL ||
        byte_count > (SIZE_MAX - 1U) / 2U) {
        return LINK_OBD2_RESULT_INVALID_ARGUMENT;
    }
    needed = byte_count * 2U + 1U;
    if (buffer_size < needed) {
        if (buffer_size != 0U) buffer[0] = '\0';
        return LINK_OBD2_RESULT_BUFFER_TOO_SMALL;
    }
    for (index = 0U; index < byte_count; ++index) {
        buffer[index * 2U] = obd2_hex_digit((unsigned int)bytes[index] >> 4U);
        buffer[index * 2U + 1U] = obd2_hex_digit(bytes[index]);
    }
    buffer[byte_count * 2U] = '\0';
    return LINK_OBD2_RESULT_OK;
}

static LinkObd2Result obd2_response_ready(const LinkElm327Response *response)
{
    if (response == NULL) return LINK_OBD2_RESULT_INVALID_ARGUMENT;
    if (response->result != LINK_ELM327_RESULT_OK) return LINK_OBD2_RESULT_ELM_ERROR;
    if (response->text[0] == '\0') return LINK_OBD2_RESULT_MALFORMED_RESPONSE;
    return LINK_OBD2_RESULT_OK;
}

static LinkObd2Result obd2_parse_hex_line(
    const char *line, size_t line_length,
    uint8_t *bytes, size_t bytes_size, size_t *byte_count)
{
    size_t index;
    size_t count = 0U;
    int high = -1;

    if (line == NULL || bytes == NULL || byte_count == NULL) {
        return LINK_OBD2_RESULT_INVALID_ARGUMENT;
    }
    *byte_count = 0U;
    for (index = 0U; index < line_length; ++index) {
        int value;
        if (obd2_space(line[index])) continue;
        value = obd2_hex_value(line[index]);
        if (value < 0) return LINK_OBD2_RESULT_MALFORMED_RESPONSE;
        if (high < 0) {
            high = value;
            continue;
        }
        if (count >= bytes_size) return LINK_OBD2_RESULT_BUFFER_TOO_SMALL;
        bytes[count++] = (uint8_t)(((unsigned int)high << 4U) |
                                   (unsigned int)value);
        high = -1;
    }
    if (high >= 0 || count == 0U) return LINK_OBD2_RESULT_MALFORMED_RESPONSE;
    *byte_count = count;
    return LINK_OBD2_RESULT_OK;
}


/*
 * ELM327 ATH1 adds the CAN responder identifier to each line. With ATS0,
 * 11-bit identifiers may be glued directly to the DLC (for example
 * "7E804410C1AF8"); with spaces enabled the same response is commonly
 * "7E8 04 41 0C 1A F8". Strip only a positively identified 11/29-bit
 * header and optional DLC, leaving ordinary headerless OBD payloads untouched.
 */
static LinkObd2Result obd2_parse_data_line(
    const char *line, size_t line_length,
    uint8_t *bytes, size_t bytes_size, size_t *byte_count)
{
    size_t first = 0U;
    size_t last = line_length;
    size_t token_end;
    size_t data_start;
    size_t header_digits = 0U;
    bool headered = false;
    LinkObd2Result result;

    if (line == NULL || bytes == NULL || byte_count == NULL) {
        return LINK_OBD2_RESULT_INVALID_ARGUMENT;
    }

    while (first < last && obd2_space(line[first])) first++;
    while (last > first && obd2_space(line[last - 1U])) last--;
    token_end = first;
    while (token_end < last && obd2_hex_value(line[token_end]) >= 0) {
        token_end++;
    }

    if (token_end < last && obd2_space(line[token_end])) {
        header_digits = token_end - first;
        headered = header_digits == 3U || header_digits == 8U;
        data_start = token_end;
        while (data_start < last && obd2_space(line[data_start])) data_start++;
    } else if (token_end == last &&
               ((last - first) & 1U) != 0U &&
               last - first >= 7U) {
        /* Headerless payloads are byte-aligned; odd hex count identifies
         * an unspaced 11-bit CAN header (3 nibbles) followed by bytes. */
        header_digits = 3U;
        headered = true;
        data_start = first + header_digits;
    } else {
        data_start = first;
    }

    if (!headered) {
        return obd2_parse_hex_line(
            line + first, last - first, bytes, bytes_size, byte_count);
    }
    if (data_start >= last) return LINK_OBD2_RESULT_MALFORMED_RESPONSE;

    result = obd2_parse_hex_line(
        line + data_start, last - data_start, bytes, bytes_size, byte_count);
    if (result != LINK_OBD2_RESULT_OK) return result;

    /*
     * CAN responses with headers normally include a DLC byte immediately
     * after the identifier. Remove it only when it is self-consistent.
     */
    if (*byte_count >= 2U && bytes[0] <= 8U &&
        (size_t)bytes[0] <= *byte_count - 1U) {
        const size_t declared = (size_t)bytes[0];
        memmove(bytes, bytes + 1U, declared);
        *byte_count = declared;
    }
    return LINK_OBD2_RESULT_OK;
}

static bool obd2_parse_indexed_length(
    const char *line, size_t line_length, size_t *length)
{
    size_t value = 0U;
    size_t digits = 0U;
    size_t index;

    if (line == NULL || length == NULL) return false;
    for (index = 0U; index < line_length; ++index) {
        int nibble;
        if (obd2_space(line[index])) continue;
        nibble = obd2_hex_value(line[index]);
        if (nibble < 0 || digits >= 3U) return false;
        value = (value << 4U) | (size_t)nibble;
        digits++;
    }
    if (digits != 3U || value == 0U) return false;
    *length = value;
    return true;
}

static LinkObd2Result obd2_collect_indexed_message(
    const LinkElm327Response *response,
    uint8_t *message, size_t message_size, size_t *message_length,
    bool *found)
{
    const char *cursor;
    unsigned int expected_index = 0U;
    size_t written = 0U;
    size_t declared_length = 0U;
    bool declared_length_seen = false;
    bool collecting = false;
    LinkObd2Result result;

    if (message == NULL || message_length == NULL || found == NULL) {
        return LINK_OBD2_RESULT_INVALID_ARGUMENT;
    }
    *message_length = 0U;
    *found = false;
    result = obd2_response_ready(response);
    if (result != LINK_OBD2_RESULT_OK) return result;

    cursor = response->text;
    while (*cursor != '\0') {
        const char *end = strchr(cursor, '\n');
        size_t line_length = end == NULL ? strlen(cursor) : (size_t)(end - cursor);
        size_t first = 0U;
        size_t last = line_length;
        size_t colon = SIZE_MAX;
        size_t index;
        int line_index = -1;

        while (first < last && obd2_space(cursor[first])) first++;
        while (last > first && obd2_space(cursor[last - 1U])) last--;
        for (index = first; index < last; ++index) {
            if (cursor[index] == ':') { colon = index; break; }
        }

        if (colon != SIZE_MAX) {
            size_t prefix = first;
            uint8_t bytes[OBD2_MAX_LINE_BYTES];
            size_t byte_count = 0U;

            while (prefix < colon && obd2_space(cursor[prefix])) prefix++;
            if (prefix + 1U != colon) return LINK_OBD2_RESULT_MALFORMED_RESPONSE;
            line_index = obd2_hex_value(cursor[prefix]);
            if (line_index < 0) return LINK_OBD2_RESULT_MALFORMED_RESPONSE;
            if (!collecting) {
                if (line_index != 0) return LINK_OBD2_RESULT_MALFORMED_RESPONSE;
                collecting = true;
                *found = true;
                expected_index = 0U;
            }
            if ((unsigned int)line_index != expected_index) {
                return LINK_OBD2_RESULT_MALFORMED_RESPONSE;
            }
            result = obd2_parse_hex_line(cursor + colon + 1U,
                                          last - colon - 1U,
                                          bytes, sizeof(bytes), &byte_count);
            if (result != LINK_OBD2_RESULT_OK) return result;
            if (written > message_size || byte_count > message_size - written) {
                return LINK_OBD2_RESULT_BUFFER_TOO_SMALL;
            }
            memcpy(message + written, bytes, byte_count);
            written += byte_count;
            expected_index = (expected_index + 1U) & 0x0fU;
        } else if (!collecting) {
            size_t parsed_length = 0U;
            if (obd2_parse_indexed_length(cursor + first, last - first,
                                          &parsed_length)) {
                if (declared_length_seen) return LINK_OBD2_RESULT_MALFORMED_RESPONSE;
                if (parsed_length > message_size) return LINK_OBD2_RESULT_BUFFER_TOO_SMALL;
                declared_length = parsed_length;
                declared_length_seen = true;
            }
        } else {
            break;
        }

        if (end == NULL) break;
        cursor = end + 1;
    }

    if (*found) {
        if (written == 0U || (declared_length_seen && written < declared_length)) {
            return LINK_OBD2_RESULT_MALFORMED_RESPONSE;
        }
        *message_length = declared_length_seen ? declared_length : written;
    }
    return LINK_OBD2_RESULT_OK;
}

static LinkObd2Result obd2_find_pid_payload(
    const LinkElm327Response *response,
    uint8_t response_service,
    uint8_t pid,
    bool has_frame_number,
    uint8_t frame_number,
    uint8_t *payload,
    size_t payload_size,
    size_t *payload_length)
{
    const char *cursor;
    LinkObd2Result result;

    if (payload == NULL || payload_length == NULL) return LINK_OBD2_RESULT_INVALID_ARGUMENT;
    *payload_length = 0U;
    result = obd2_response_ready(response);
    if (result != LINK_OBD2_RESULT_OK) return result;

    cursor = response->text;
    while (*cursor != '\0') {
        const char *end = strchr(cursor, '\n');
        size_t line_length = end == NULL ? strlen(cursor) : (size_t)(end - cursor);
        uint8_t bytes[OBD2_MAX_LINE_BYTES];
        size_t byte_count = 0U;
        size_t data_index;
        size_t data_length;

        if (memchr(cursor, ':', line_length) != NULL) goto next_line;
        result = obd2_parse_data_line(cursor, line_length, bytes,
                                      sizeof(bytes), &byte_count);
        if (result != LINK_OBD2_RESULT_OK) {
            size_t indexed_length = 0U;
            if (obd2_parse_indexed_length(cursor, line_length, &indexed_length)) {
                goto next_line;
            }
            return result;
        }
        if (byte_count >= 2U && bytes[0] == response_service && bytes[1] == pid) {
            data_index = 2U;
            if (has_frame_number) {
                if (byte_count < 3U || bytes[2] != frame_number) goto next_line;
                data_index = 3U;
            }
            data_length = byte_count - data_index;
            if (data_length > payload_size) return LINK_OBD2_RESULT_BUFFER_TOO_SMALL;
            if (data_length != 0U) memcpy(payload, bytes + data_index, data_length);
            *payload_length = data_length;
            return LINK_OBD2_RESULT_OK;
        }
next_line:
        if (end == NULL) break;
        cursor = end + 1;
    }
    return LINK_OBD2_RESULT_UNEXPECTED_RESPONSE;
}

static LinkObd2Result obd2_decode_sample_data(
    uint8_t pid, const uint8_t *data, size_t length, LinkObd2Sample *sample)
{
    LinkObd2Sample decoded = { .pid = pid, .value = 0.0, .unit = LINK_OBD2_UNIT_NONE };

    if (data == NULL || sample == NULL) return LINK_OBD2_RESULT_INVALID_ARGUMENT;
    switch (pid) {
    case 0x04U:
        if (length < 1U) return LINK_OBD2_RESULT_MALFORMED_RESPONSE;
        decoded.value = (double)data[0] * 100.0 / 255.0; decoded.unit = LINK_OBD2_UNIT_PERCENT; break;
    case 0x05U:
        if (length < 1U) return LINK_OBD2_RESULT_MALFORMED_RESPONSE;
        decoded.value = (double)data[0] - 40.0; decoded.unit = LINK_OBD2_UNIT_CELSIUS; break;
    case 0x0bU:
        if (length < 1U) return LINK_OBD2_RESULT_MALFORMED_RESPONSE;
        decoded.value = (double)data[0]; decoded.unit = LINK_OBD2_UNIT_KPA; break;
    case 0x0cU:
        if (length < 2U) return LINK_OBD2_RESULT_MALFORMED_RESPONSE;
        decoded.value = (double)(((unsigned int)data[0] << 8U) | data[1]) / 4.0;
        decoded.unit = LINK_OBD2_UNIT_RPM; break;
    case 0x0dU:
        if (length < 1U) return LINK_OBD2_RESULT_MALFORMED_RESPONSE;
        decoded.value = (double)data[0]; decoded.unit = LINK_OBD2_UNIT_KMH; break;
    case 0x0fU:
        if (length < 1U) return LINK_OBD2_RESULT_MALFORMED_RESPONSE;
        decoded.value = (double)data[0] - 40.0; decoded.unit = LINK_OBD2_UNIT_CELSIUS; break;
    case 0x10U:
        if (length < 2U) return LINK_OBD2_RESULT_MALFORMED_RESPONSE;
        decoded.value = (double)(((unsigned int)data[0] << 8U) | data[1]) / 100.0;
        decoded.unit = LINK_OBD2_UNIT_GRAMS_PER_SECOND; break;
    case 0x11U:
    case 0x45U:
    case 0x47U:
    case 0x48U:
    case 0x49U:
    case 0x4aU:
    case 0x4bU:
    case 0x4cU:
        if (length < 1U) return LINK_OBD2_RESULT_MALFORMED_RESPONSE;
        decoded.value = (double)data[0] * 100.0 / 255.0;
        decoded.unit = LINK_OBD2_UNIT_PERCENT; break;
    case 0x23U:
        if (length < 2U) return LINK_OBD2_RESULT_MALFORMED_RESPONSE;
        decoded.value = (double)(((unsigned int)data[0] << 8U) | data[1]) * 10.0;
        decoded.unit = LINK_OBD2_UNIT_KPA; break;
    case 0x2cU:
        if (length < 1U) return LINK_OBD2_RESULT_MALFORMED_RESPONSE;
        decoded.value = (double)data[0] * 100.0 / 255.0; decoded.unit = LINK_OBD2_UNIT_PERCENT; break;
    case 0x2fU:
        if (length < 1U) return LINK_OBD2_RESULT_MALFORMED_RESPONSE;
        decoded.value = (double)data[0] * 100.0 / 255.0; decoded.unit = LINK_OBD2_UNIT_PERCENT; break;
    case 0x2dU:
        if (length < 1U) return LINK_OBD2_RESULT_MALFORMED_RESPONSE;
        decoded.value = ((double)(int)data[0] - 128.0) * 100.0 / 128.0;
        decoded.unit = LINK_OBD2_UNIT_PERCENT; break;
    case 0x33U:
        if (length < 1U) return LINK_OBD2_RESULT_MALFORMED_RESPONSE;
        decoded.value = (double)data[0]; decoded.unit = LINK_OBD2_UNIT_KPA; break;
    case 0x3cU:
        if (length < 2U) return LINK_OBD2_RESULT_MALFORMED_RESPONSE;
        decoded.value = (double)(((unsigned int)data[0] << 8U) | data[1]) / 10.0 - 40.0;
        decoded.unit = LINK_OBD2_UNIT_CELSIUS; break;
    case 0x42U:
        if (length < 2U) return LINK_OBD2_RESULT_MALFORMED_RESPONSE;
        decoded.value = (double)(((unsigned int)data[0] << 8U) | data[1]) / 1000.0;
        decoded.unit = LINK_OBD2_UNIT_VOLTS; break;
    case 0x46U:
    case 0x5cU:
        if (length < 1U) return LINK_OBD2_RESULT_MALFORMED_RESPONSE;
        decoded.value = (double)data[0] - 40.0; decoded.unit = LINK_OBD2_UNIT_CELSIUS; break;
    case 0x5eU:
        if (length < 2U) return LINK_OBD2_RESULT_MALFORMED_RESPONSE;
        decoded.value = (double)(((unsigned int)data[0] << 8U) | data[1]) / 20.0;
        decoded.unit = LINK_OBD2_UNIT_LITRES_PER_HOUR; break;
    case 0x78U:
        if (length < 3U) return LINK_OBD2_RESULT_MALFORMED_RESPONSE;
        if ((data[0] & 0x01U) == 0U) return LINK_OBD2_RESULT_UNSUPPORTED_PID;
        decoded.value = (double)(((unsigned int)data[1] << 8U) | data[2]) / 10.0 - 40.0;
        decoded.unit = LINK_OBD2_UNIT_CELSIUS; break;
    case 0x7aU:
        if (length < 3U) return LINK_OBD2_RESULT_MALFORMED_RESPONSE;
        if ((data[0] & 0x01U) == 0U) return LINK_OBD2_RESULT_UNSUPPORTED_PID;
        {
            uint16_t raw = (uint16_t)(((uint16_t)data[1] << 8U) | data[2]);
            int32_t signed_raw = raw >= 0x8000U ? (int32_t)raw - 0x10000 : (int32_t)raw;
            decoded.value = (double)signed_raw / 100.0;
        }
        decoded.unit = LINK_OBD2_UNIT_KPA; break;
    case 0x7cU:
        if (length < 3U) return LINK_OBD2_RESULT_MALFORMED_RESPONSE;
        if ((data[0] & 0x01U) == 0U) return LINK_OBD2_RESULT_UNSUPPORTED_PID;
        decoded.value = (double)(((unsigned int)data[1] << 8U) | data[2]) / 10.0 - 40.0;
        decoded.unit = LINK_OBD2_UNIT_CELSIUS; break;
    default:
        return LINK_OBD2_RESULT_UNSUPPORTED_PID;
    }
    *sample = decoded;
    return LINK_OBD2_RESULT_OK;
}

static uint8_t obd2_dtc_response_service(LinkObd2DtcKind kind)
{
    switch (kind) {
    case LINK_OBD2_DTC_STORED: return 0x43U;
    case LINK_OBD2_DTC_PENDING: return 0x47U;
    case LINK_OBD2_DTC_PERMANENT: return 0x4aU;
    }
    return 0U;
}

static uint8_t obd2_dtc_request_service(LinkObd2DtcKind kind)
{
    switch (kind) {
    case LINK_OBD2_DTC_STORED: return 0x03U;
    case LINK_OBD2_DTC_PENDING: return 0x07U;
    case LINK_OBD2_DTC_PERMANENT: return 0x0aU;
    }
    return 0U;
}

static bool obd2_dtc_exists(const LinkObd2DtcList *list, const char *code)
{
    size_t index;
    if (list == NULL || code == NULL) return false;
    for (index = 0U; index < list->count; ++index) {
        if (infiltratr_string_equal(list->entries[index].code, code)) return true;
    }
    return false;
}

static LinkObd2Result obd2_append_dtc_pairs(
    const uint8_t *bytes, size_t byte_count, size_t start,
    LinkObd2DtcKind kind, LinkObd2DtcList *list)
{
    size_t index;
    size_t end;
    if (bytes == NULL || list == NULL || start > byte_count) {
        return LINK_OBD2_RESULT_INVALID_ARGUMENT;
    }

    /*
     * Some vehicle ECUs report an empty Mode 03/07/0A list as a single
     * trailing 00 after the positive-response service byte (for example
     * "43 00"). That byte is padding/no-code evidence, not half of a DTC.
     * Accept only a lone trailing zero when the payload length is odd; any
     * non-zero orphan byte remains malformed.
     */
    end = byte_count;
    if (((end - start) & 1U) != 0U) {
        if (end == start || bytes[end - 1U] != 0U) {
            return LINK_OBD2_RESULT_MALFORMED_RESPONSE;
        }
        end--;
    }
    for (index = start; index + 1U < end; index += 2U) {
        char code[LINK_OBD2_DTC_TEXT_LENGTH];
        LinkObd2Result result;
        if (bytes[index] == 0U && bytes[index + 1U] == 0U) continue;
        result = link_obd2_decode_dtc_pair(bytes[index], bytes[index + 1U], code);
        if (result != LINK_OBD2_RESULT_OK) return result;
        if (obd2_dtc_exists(list, code)) continue;
        if (list->count >= LINK_OBD2_MAX_DTCS) return LINK_OBD2_RESULT_TOO_MANY_DTCS;
        list->entries[list->count].kind = kind;
        infiltratr_copy_string(list->entries[list->count].code,
                               sizeof(list->entries[list->count].code), code);
        list->count++;
    }
    return LINK_OBD2_RESULT_OK;
}

const char *link_obd2_result_name(LinkObd2Result result)
{
    switch (result) {
    case LINK_OBD2_RESULT_OK: return "ok";
    case LINK_OBD2_RESULT_INVALID_ARGUMENT: return "invalid-argument";
    case LINK_OBD2_RESULT_ELM_ERROR: return "elm-error";
    case LINK_OBD2_RESULT_MALFORMED_RESPONSE: return "malformed-response";
    case LINK_OBD2_RESULT_UNEXPECTED_RESPONSE: return "unexpected-response";
    case LINK_OBD2_RESULT_UNSUPPORTED_PID: return "unsupported-pid";
    case LINK_OBD2_RESULT_BUFFER_TOO_SMALL: return "buffer-too-small";
    case LINK_OBD2_RESULT_TOO_MANY_DTCS: return "too-many-dtcs";
    case LINK_OBD2_RESULT_NOT_AUTHORIZED: return "not-authorized";
    }
    return "unknown";
}

const char *link_obd2_unit_name(LinkObd2Unit unit)
{
    switch (unit) {
    case LINK_OBD2_UNIT_NONE: return "";
    case LINK_OBD2_UNIT_PERCENT: return "%";
    case LINK_OBD2_UNIT_CELSIUS: return "degC";
    case LINK_OBD2_UNIT_KPA: return "kPa";
    case LINK_OBD2_UNIT_RPM: return "rpm";
    case LINK_OBD2_UNIT_KMH: return "km/h";
    case LINK_OBD2_UNIT_GRAMS_PER_SECOND: return "g/s";
    case LINK_OBD2_UNIT_VOLTS: return "V";
    case LINK_OBD2_UNIT_LITRES_PER_HOUR: return "L/h";
    }
    return "";
}

const char *link_obd2_pid_name(uint8_t pid)
{
    switch (pid) {
    case 0x04U: return "Calculated engine load";
    case 0x05U: return "Engine coolant temperature";
    case 0x0bU: return "Intake manifold absolute pressure";
    case 0x0cU: return "Engine speed";
    case 0x0dU: return "Vehicle speed";
    case 0x0fU: return "Intake air temperature";
    case 0x10U: return "Mass air flow rate";
    case 0x11U: return "Absolute throttle valve position";
    case 0x23U: return "Fuel rail gauge pressure";
    case 0x2cU: return "Commanded EGR";
    case 0x2fU: return "Fuel tank level input";
    case 0x2dU: return "EGR error";
    case 0x33U: return "Barometric pressure";
    case 0x3cU: return "Catalyst temperature bank 1 sensor 1";
    case 0x42U: return "Control module voltage";
    case 0x45U: return "Relative throttle position";
    case 0x46U: return "Ambient air temperature";
    case 0x47U: return "Absolute throttle position B";
    case 0x48U: return "Absolute throttle position C";
    case 0x49U: return "Accelerator pedal position D";
    case 0x4aU: return "Accelerator pedal position E";
    case 0x4bU: return "Accelerator pedal position F";
    case 0x4cU: return "Commanded throttle actuator";
    case 0x5cU: return "Engine oil temperature";
    case 0x5eU: return "Engine fuel rate";
    case 0x78U: return "Exhaust gas temperature bank 1 sensor 1";
    case 0x7aU: return "DPF bank 1 differential pressure";
    case 0x7cU: return "DPF bank 1 inlet temperature";
    default: return "Unknown PID";
    }
}

LinkObd2Result link_obd2_build_live_pid_request(
    uint8_t pid, char *buffer, size_t buffer_size)
{
    const uint8_t bytes[] = { 0x01U, pid };
    return obd2_write_command(bytes, sizeof(bytes), buffer, buffer_size);
}

LinkObd2Result link_obd2_build_freeze_pid_request(
    uint8_t pid, uint8_t frame_number, char *buffer, size_t buffer_size)
{
    const uint8_t bytes[] = { 0x02U, pid, frame_number };
    return obd2_write_command(bytes, sizeof(bytes), buffer, buffer_size);
}

LinkObd2Result link_obd2_build_supported_pid_request(
    uint8_t base_pid, char *buffer, size_t buffer_size)
{
    if ((base_pid & 0x1fU) != 0U || base_pid > 0xe0U) {
        return LINK_OBD2_RESULT_INVALID_ARGUMENT;
    }
    return link_obd2_build_live_pid_request(base_pid, buffer, buffer_size);
}

LinkObd2Result link_obd2_build_vin_request(char *buffer, size_t buffer_size)
{
    const uint8_t bytes[] = { 0x09U, 0x02U };
    return obd2_write_command(bytes, sizeof(bytes), buffer, buffer_size);
}

LinkObd2Result link_obd2_build_dtc_request(
    LinkObd2DtcKind kind, char *buffer, size_t buffer_size)
{
    uint8_t service = obd2_dtc_request_service(kind);
    if (service == 0U) return LINK_OBD2_RESULT_INVALID_ARGUMENT;
    return obd2_write_command(&service, 1U, buffer, buffer_size);
}

LinkObd2Result link_obd2_build_clear_dtc_request(
    const LinkObd2ClearAuthorization *authorization,
    char *buffer, size_t buffer_size)
{
    const uint8_t service = 0x04U;
    if (authorization == NULL || !authorization->confirmed ||
        !authorization->acknowledge_readiness_reset) {
        if (buffer != NULL && buffer_size != 0U) buffer[0] = '\0';
        return LINK_OBD2_RESULT_NOT_AUTHORIZED;
    }
    return obd2_write_command(&service, 1U, buffer, buffer_size);
}

void link_obd2_pid_set_clear(LinkObd2PidSet *set)
{
    if (set != NULL) memset(set, 0, sizeof(*set));
}

bool link_obd2_pid_set_contains(const LinkObd2PidSet *set, uint8_t pid)
{
    uint8_t mask;
    if (set == NULL) return false;
    mask = (uint8_t)(1U << (pid & 7U));
    return (set->bits[pid >> 3U] & mask) != 0U;
}

LinkObd2Result link_obd2_accept_supported_pids(
    const LinkElm327Response *response,
    uint8_t base_pid,
    LinkObd2PidSet *set,
    bool *has_more)
{
    const char *cursor;
    LinkObd2PidSet updated;
    bool matched = false;
    bool continuation = false;
    LinkObd2Result result;

    if (set == NULL || has_more == NULL || (base_pid & 0x1fU) != 0U ||
        base_pid > 0xe0U) return LINK_OBD2_RESULT_INVALID_ARGUMENT;
    *has_more = false;
    updated = *set;
    result = obd2_response_ready(response);
    if (result != LINK_OBD2_RESULT_OK) return result;

    cursor = response->text;
    while (*cursor != '\0') {
        const char *end = strchr(cursor, '\n');
        size_t line_length = end == NULL ? strlen(cursor) : (size_t)(end - cursor);
        uint8_t bytes[OBD2_MAX_LINE_BYTES];
        size_t byte_count = 0U;
        uint32_t mask;
        unsigned int bit;

        if (memchr(cursor, ':', line_length) != NULL) goto next_supported_line;
        result = obd2_parse_hex_line(cursor, line_length, bytes,
                                     sizeof(bytes), &byte_count);
        if (result != LINK_OBD2_RESULT_OK) return result;
        if (byte_count >= 6U && bytes[0] == 0x41U && bytes[1] == base_pid) {
            matched = true;
            mask = ((uint32_t)bytes[2] << 24U) |
                   ((uint32_t)bytes[3] << 16U) |
                   ((uint32_t)bytes[4] << 8U) |
                   (uint32_t)bytes[5];
            for (bit = 0U; bit < 32U; ++bit) {
                if ((mask & (UINT32_C(1) << (31U - bit))) != 0U) {
                    unsigned int supported_pid = (unsigned int)base_pid + bit + 1U;
                    if (supported_pid <= 0xffU) {
                        updated.bits[supported_pid >> 3U] |=
                            (uint8_t)(1U << (supported_pid & 7U));
                    }
                }
            }
        }
next_supported_line:
        if (end == NULL) break;
        cursor = end + 1;
    }
    if (!matched) return LINK_OBD2_RESULT_UNEXPECTED_RESPONSE;
    if (base_pid <= 0xc0U) {
        continuation = link_obd2_pid_set_contains(&updated,
                                                   (uint8_t)(base_pid + 0x20U));
    }
    *set = updated;
    *has_more = continuation;
    return LINK_OBD2_RESULT_OK;
}

LinkObd2Result link_obd2_decode_live_pid(
    const LinkElm327Response *response, uint8_t pid, LinkObd2Sample *sample)
{
    uint8_t data[16];
    size_t length = 0U;
    LinkObd2Result result;
    if (sample == NULL) return LINK_OBD2_RESULT_INVALID_ARGUMENT;
    result = obd2_find_pid_payload(response, 0x41U, pid, false, 0U,
                                   data, sizeof(data), &length);
    if (result != LINK_OBD2_RESULT_OK) return result;
    return obd2_decode_sample_data(pid, data, length, sample);
}

LinkObd2Result link_obd2_decode_freeze_pid(
    const LinkElm327Response *response, uint8_t pid, uint8_t frame_number,
    LinkObd2Sample *sample)
{
    uint8_t data[16];
    size_t length = 0U;
    LinkObd2Result result;
    if (sample == NULL) return LINK_OBD2_RESULT_INVALID_ARGUMENT;
    result = obd2_find_pid_payload(response, 0x42U, pid, true, frame_number,
                                   data, sizeof(data), &length);
    if (result != LINK_OBD2_RESULT_OK) return result;
    return obd2_decode_sample_data(pid, data, length, sample);
}

LinkObd2Result link_obd2_decode_readiness(
    const LinkElm327Response *response, LinkObd2Readiness *readiness)
{
    uint8_t data[8];
    size_t length = 0U;
    LinkObd2Result result;
    LinkObd2Readiness decoded;
    if (readiness == NULL) return LINK_OBD2_RESULT_INVALID_ARGUMENT;
    result = obd2_find_pid_payload(response, 0x41U, 0x01U, false, 0U,
                                   data, sizeof(data), &length);
    if (result != LINK_OBD2_RESULT_OK) return result;
    if (length < 4U) return LINK_OBD2_RESULT_MALFORMED_RESPONSE;
    memset(&decoded, 0, sizeof(decoded));
    memcpy(decoded.raw, data, sizeof(decoded.raw));
    decoded.mil_on = (data[0] & 0x80U) != 0U;
    decoded.confirmed_dtc_count = data[0] & 0x7fU;
    decoded.compression_ignition = (data[1] & 0x08U) != 0U;
    decoded.continuous_supported = data[1] & 0x07U;
    decoded.continuous_incomplete = (data[1] >> 4U) & 0x07U;
    decoded.noncontinuous_supported = data[2];
    decoded.noncontinuous_incomplete = data[3];
    *readiness = decoded;
    return LINK_OBD2_RESULT_OK;
}

LinkObd2Result link_obd2_decode_vin_pdu(
    const uint8_t *pdu,
    size_t pdu_length,
    char vin[LINK_OBD2_VIN_LENGTH + 1U])
{
    char decoded[LINK_OBD2_VIN_LENGTH + 1U];
    size_t index;

    if (pdu == NULL || vin == NULL) return LINK_OBD2_RESULT_INVALID_ARGUMENT;
    vin[0] = '\0';
    if (pdu_length < 3U ||
        pdu[0] != UINT8_C(0x49) ||
        pdu[1] != UINT8_C(0x02)) {
        return LINK_OBD2_RESULT_UNEXPECTED_RESPONSE;
    }
    if (pdu_length < 3U + LINK_OBD2_VIN_LENGTH) {
        return LINK_OBD2_RESULT_UNEXPECTED_RESPONSE;
    }
    for (index = 0U; index < LINK_OBD2_VIN_LENGTH; ++index) {
        const uint8_t value = pdu[index + 3U];
        if (!obd2_vin_character_valid(value))
            return LINK_OBD2_RESULT_MALFORMED_RESPONSE;
        decoded[index] = (char)value;
    }
    decoded[LINK_OBD2_VIN_LENGTH] = '\0';
    memcpy(vin, decoded, sizeof(decoded));
    return LINK_OBD2_RESULT_OK;
}

LinkObd2Result link_obd2_decode_vin(
    const LinkElm327Response *response, char vin[LINK_OBD2_VIN_LENGTH + 1U])
{
    uint8_t indexed[OBD2_MAX_MESSAGE_BYTES];
    char decoded[LINK_OBD2_VIN_LENGTH + 1U] = { 0 };
    size_t indexed_length = 0U;
    bool indexed_found = false;
    LinkObd2Result result;
    size_t written = 0U;

    if (vin == NULL) return LINK_OBD2_RESULT_INVALID_ARGUMENT;
    vin[0] = '\0';
    result = obd2_collect_indexed_message(response, indexed, sizeof(indexed),
                                           &indexed_length, &indexed_found);
    if (result != LINK_OBD2_RESULT_OK) return result;

    if (indexed_found) {
        return link_obd2_decode_vin_pdu(indexed, indexed_length, vin);
    } else {
        const char *cursor;
        result = obd2_response_ready(response);
        if (result != LINK_OBD2_RESULT_OK) return result;
        cursor = response->text;
        while (*cursor != '\0' && written < LINK_OBD2_VIN_LENGTH) {
            const char *end = strchr(cursor, '\n');
            size_t line_length = end == NULL ? strlen(cursor) : (size_t)(end - cursor);
            uint8_t bytes[OBD2_MAX_LINE_BYTES];
            size_t byte_count = 0U;
            size_t index;
            result = obd2_parse_hex_line(cursor, line_length, bytes,
                                         sizeof(bytes), &byte_count);
            if (result != LINK_OBD2_RESULT_OK) return result;
            if (byte_count >= 4U && bytes[0] == 0x49U && bytes[1] == 0x02U) {
                for (index = 3U; index < byte_count && written < LINK_OBD2_VIN_LENGTH;
                     ++index) {
                    if (!obd2_vin_character_valid(bytes[index])) {
                        return LINK_OBD2_RESULT_MALFORMED_RESPONSE;
                    }
                    decoded[written++] = (char)bytes[index];
                }
            }
            if (end == NULL) break;
            cursor = end + 1;
        }
    }
    if (written != LINK_OBD2_VIN_LENGTH) return LINK_OBD2_RESULT_UNEXPECTED_RESPONSE;
    decoded[written] = '\0';
    memcpy(vin, decoded, sizeof(decoded));
    return LINK_OBD2_RESULT_OK;
}

LinkObd2Result link_obd2_decode_dtc_pair(
    uint8_t high, uint8_t low, char code[LINK_OBD2_DTC_TEXT_LENGTH])
{
    static const char system_chars[] = { 'P', 'C', 'B', 'U' };
    if (code == NULL) return LINK_OBD2_RESULT_INVALID_ARGUMENT;
    code[0] = system_chars[(high >> 6U) & 0x03U];
    code[1] = obd2_hex_digit((high >> 4U) & 0x03U);
    code[2] = obd2_hex_digit(high);
    code[3] = obd2_hex_digit(low >> 4U);
    code[4] = obd2_hex_digit(low);
    code[5] = '\0';
    return LINK_OBD2_RESULT_OK;
}

LinkObd2Result link_obd2_decode_dtcs(
    const LinkElm327Response *response, LinkObd2DtcKind kind,
    LinkObd2DtcList *list)
{
    const char *cursor;
    uint8_t response_service = obd2_dtc_response_service(kind);
    uint8_t indexed[OBD2_MAX_MESSAGE_BYTES];
    LinkObd2DtcList decoded = { 0 };
    size_t indexed_length = 0U;
    bool indexed_found = false;
    bool matched = false;
    LinkObd2Result result;

    if (list == NULL || response_service == 0U) return LINK_OBD2_RESULT_INVALID_ARGUMENT;
    result = obd2_collect_indexed_message(response, indexed, sizeof(indexed),
                                           &indexed_length, &indexed_found);
    if (result != LINK_OBD2_RESULT_OK) return result;
    if (indexed_found) {
        if (indexed_length < 1U || indexed[0] != response_service) {
            return LINK_OBD2_RESULT_UNEXPECTED_RESPONSE;
        }
        result = obd2_append_dtc_pairs(indexed, indexed_length, 1U, kind, &decoded);
        if (result != LINK_OBD2_RESULT_OK) return result;
        *list = decoded;
        return LINK_OBD2_RESULT_OK;
    }

    result = obd2_response_ready(response);
    if (result != LINK_OBD2_RESULT_OK) return result;
    cursor = response->text;
    while (*cursor != '\0') {
        const char *end = strchr(cursor, '\n');
        size_t line_length = end == NULL ? strlen(cursor) : (size_t)(end - cursor);
        uint8_t bytes[OBD2_MAX_LINE_BYTES];
        size_t byte_count = 0U;
        result = obd2_parse_hex_line(cursor, line_length, bytes,
                                     sizeof(bytes), &byte_count);
        if (result != LINK_OBD2_RESULT_OK) return result;
        if (byte_count >= 1U && bytes[0] == response_service) {
            matched = true;
            result = obd2_append_dtc_pairs(bytes, byte_count, 1U, kind, &decoded);
            if (result != LINK_OBD2_RESULT_OK) return result;
        }
        if (end == NULL) break;
        cursor = end + 1;
    }
    if (!matched) return LINK_OBD2_RESULT_UNEXPECTED_RESPONSE;
    *list = decoded;
    return LINK_OBD2_RESULT_OK;
}

bool link_obd2_is_negative_response(
    const LinkElm327Response *response,
    uint8_t request_service,
    uint8_t *negative_response_code)
{
    const char *cursor;
    uint8_t indexed[OBD2_MAX_MESSAGE_BYTES];
    size_t indexed_length = 0U;
    bool indexed_found = false;
    bool matched = false;
    uint8_t captured_code = 0U;
    LinkObd2Result result;

    if (negative_response_code != NULL) *negative_response_code = 0U;
    if (response == NULL || request_service == 0U) return false;

    result = obd2_collect_indexed_message(
        response, indexed, sizeof(indexed), &indexed_length, &indexed_found);
    if (result != LINK_OBD2_RESULT_OK) return false;
    if (indexed_found) {
        if (indexed_length != 3U || indexed[0] != UINT8_C(0x7f) ||
            indexed[1] != request_service) {
            return false;
        }
        if (negative_response_code != NULL)
            *negative_response_code = indexed[2];
        return true;
    }

    if (obd2_response_ready(response) != LINK_OBD2_RESULT_OK) return false;
    cursor = response->text;
    while (*cursor != '\0') {
        const char *end = strchr(cursor, '\n');
        const size_t line_length = end == NULL
            ? strlen(cursor) : (size_t)(end - cursor);
        uint8_t bytes[OBD2_MAX_LINE_BYTES];
        size_t byte_count = 0U;

        result = obd2_parse_hex_line(
            cursor, line_length, bytes, sizeof(bytes), &byte_count);
        if (result != LINK_OBD2_RESULT_OK || byte_count != 3U ||
            bytes[0] != UINT8_C(0x7f) || bytes[1] != request_service) {
            return false;
        }
        if (!matched) captured_code = bytes[2];
        else if (bytes[2] != captured_code) return false;
        matched = true;
        if (end == NULL) break;
        cursor = end + 1;
    }
    if (matched && negative_response_code != NULL)
        *negative_response_code = captured_code;
    return matched;
}
