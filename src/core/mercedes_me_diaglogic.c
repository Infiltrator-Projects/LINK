// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/mercedes_me_diaglogic.h"

#include <limits.h>
#include <string.h>

typedef struct ProtoReader {
    const uint8_t *bytes;
    size_t size;
    size_t position;
} ProtoReader;

typedef struct DtcForwardContext {
    LinkMercedesMeDiaglogicDtcFn callback;
    void *context;
} DtcForwardContext;

static void clear_slice(LinkMercedesMeProtoSlice *slice)
{
    if (slice == NULL) return;
    slice->data = NULL;
    slice->size = 0U;
}

static bool read_varint(ProtoReader *reader, uint64_t *value)
{
    uint64_t result = 0U;
    unsigned int shift = 0U;
    unsigned int index;
    if (reader == NULL || value == NULL) return false;
    for (index = 0U; index < 10U; ++index) {
        uint8_t byte;
        if (reader->position >= reader->size) return false;
        byte = reader->bytes[reader->position++];
        if (index == 9U && (byte & UINT8_C(0xfe)) != 0U) return false;
        result |= ((uint64_t)(byte & UINT8_C(0x7f))) << shift;
        if ((byte & UINT8_C(0x80)) == 0U) {
            *value = result;
            return true;
        }
        shift += 7U;
    }
    return false;
}

static bool read_key(ProtoReader *reader,
                     uint32_t *field_number,
                     unsigned int *wire_type)
{
    uint64_t key;
    if (!read_varint(reader, &key) || key == 0U ||
        (key >> 3U) > UINT32_MAX) return false;
    *field_number = (uint32_t)(key >> 3U);
    *wire_type = (unsigned int)(key & UINT64_C(7));
    return *field_number != 0U;
}

static bool read_slice(ProtoReader *reader, LinkMercedesMeProtoSlice *slice)
{
    uint64_t length;
    if (reader == NULL || slice == NULL || !read_varint(reader, &length))
        return false;
    if (length > (uint64_t)(reader->size - reader->position)) {
        reader->position = reader->size;
        return false;
    }
    slice->data = reader->bytes + reader->position;
    slice->size = (size_t)length;
    reader->position += (size_t)length;
    return true;
}

static bool read_fixed64(ProtoReader *reader, uint64_t *value)
{
    uint64_t result = 0U;
    unsigned int index;
    if (reader == NULL || value == NULL) return false;
    if (reader->size - reader->position < 8U) {
        reader->position = reader->size;
        return false;
    }
    for (index = 0U; index < 8U; ++index)
        result |= ((uint64_t)reader->bytes[reader->position + index])
                  << (index * 8U);
    reader->position += 8U;
    *value = result;
    return true;
}

static bool skip_field(ProtoReader *reader, unsigned int wire_type)
{
    uint64_t ignored;
    LinkMercedesMeProtoSlice slice;
    if (reader == NULL) return false;
    switch (wire_type) {
    case 0U:
        return read_varint(reader, &ignored);
    case 1U:
        if (reader->size - reader->position < 8U) {
            reader->position = reader->size;
            return false;
        }
        reader->position += 8U;
        return true;
    case 2U:
        return read_slice(reader, &slice);
    case 5U:
        if (reader->size - reader->position < 4U) {
            reader->position = reader->size;
            return false;
        }
        reader->position += 4U;
        return true;
    default:
        return false;
    }
}

static int64_t signed_varint(uint64_t raw)
{
    if (raw <= (uint64_t)INT64_MAX) return (int64_t)raw;
    return -(int64_t)(UINT64_MAX - raw) - INT64_C(1);
}

static LinkMercedesMeDiaglogicResult malformed_or_truncated(
    const ProtoReader *reader)
{
    return reader != NULL && reader->position >= reader->size
        ? LINK_MERCEDES_ME_DIAGLOGIC_TRUNCATED
        : LINK_MERCEDES_ME_DIAGLOGIC_MALFORMED;
}

const char *link_mercedes_me_diaglogic_result_name(
    LinkMercedesMeDiaglogicResult result)
{
    switch (result) {
    case LINK_MERCEDES_ME_DIAGLOGIC_OK: return "ok";
    case LINK_MERCEDES_ME_DIAGLOGIC_INVALID_ARGUMENT:
        return "invalid-argument";
    case LINK_MERCEDES_ME_DIAGLOGIC_TRUNCATED: return "truncated";
    case LINK_MERCEDES_ME_DIAGLOGIC_MALFORMED: return "malformed";
    case LINK_MERCEDES_ME_DIAGLOGIC_REQUIRED_FIELD_MISSING:
        return "required-field-missing";
    }
    return "unknown";
}

LinkMercedesMeDiaglogicResult link_mercedes_me_diaglogic_decode_value(
    const uint8_t *bytes,
    size_t size,
    LinkMercedesMeDiaglogicValue *value)
{
    ProtoReader reader;
    bool has_type = false;
    if (value == NULL || (bytes == NULL && size != 0U))
        return LINK_MERCEDES_ME_DIAGLOGIC_INVALID_ARGUMENT;
    memset(value, 0, sizeof(*value));
    reader.bytes = bytes;
    reader.size = size;
    reader.position = 0U;
    while (reader.position < reader.size) {
        uint32_t field;
        unsigned int wire;
        uint64_t raw;
        if (!read_key(&reader, &field, &wire))
            return malformed_or_truncated(&reader);
        switch (field) {
        case 1U:
            if (wire != 0U || !read_varint(&reader, &raw))
                return malformed_or_truncated(&reader);
            value->value_type = raw <= INT_MAX ? (int)raw : INT_MAX;
            has_type = true;
            break;
        case 2U:
            if (wire != 0U || !read_varint(&reader, &raw))
                return malformed_or_truncated(&reader);
            value->has_boolean_value = true;
            value->boolean_value = raw != 0U;
            break;
        case 3U:
            if (wire != 1U || !read_fixed64(&reader, &raw))
                return malformed_or_truncated(&reader);
            if (sizeof(value->double_value) != sizeof(raw))
                return LINK_MERCEDES_ME_DIAGLOGIC_MALFORMED;
            memcpy(&value->double_value, &raw, sizeof(raw));
            value->has_double_value = true;
            break;
        case 4U:
            if (wire != 0U || !read_varint(&reader, &raw))
                return malformed_or_truncated(&reader);
            value->long_value = signed_varint(raw);
            value->has_long_value = true;
            break;
        case 5U:
            if (wire != 2U || !read_slice(&reader, &value->string_value))
                return malformed_or_truncated(&reader);
            break;
        case 6U:
            if (wire != 2U || !read_slice(&reader, &value->bytes_value))
                return malformed_or_truncated(&reader);
            break;
        default:
            if (!skip_field(&reader, wire))
                return malformed_or_truncated(&reader);
            break;
        }
    }
    return has_type
        ? LINK_MERCEDES_ME_DIAGLOGIC_OK
        : LINK_MERCEDES_ME_DIAGLOGIC_REQUIRED_FIELD_MISSING;
}

LinkMercedesMeDiaglogicResult link_mercedes_me_diaglogic_decode_measured_item(
    const uint8_t *bytes,
    size_t size,
    LinkMercedesMeDiaglogicMeasuredItem *item)
{
    ProtoReader reader;
    bool has_data_id = false;
    if (item == NULL || (bytes == NULL && size != 0U))
        return LINK_MERCEDES_ME_DIAGLOGIC_INVALID_ARGUMENT;
    memset(item, 0, sizeof(*item));
    reader.bytes = bytes;
    reader.size = size;
    reader.position = 0U;
    while (reader.position < reader.size) {
        uint32_t field;
        unsigned int wire;
        uint64_t raw;
        LinkMercedesMeProtoSlice nested;
        LinkMercedesMeDiaglogicResult result;
        if (!read_key(&reader, &field, &wire))
            return malformed_or_truncated(&reader);
        switch (field) {
        case 1U:
            if (wire != 2U || !read_slice(&reader, &item->data_id))
                return malformed_or_truncated(&reader);
            has_data_id = true;
            break;
        case 2U:
            if (wire != 2U || !read_slice(&reader, &item->unit))
                return malformed_or_truncated(&reader);
            break;
        case 3U:
            if (wire != 2U || !read_slice(&reader, &nested))
                return malformed_or_truncated(&reader);
            result = link_mercedes_me_diaglogic_decode_value(
                nested.data, nested.size, &item->value);
            if (result != LINK_MERCEDES_ME_DIAGLOGIC_OK) return result;
            item->has_value = true;
            break;
        case 4U:
            if (wire != 0U || !read_varint(&reader, &raw))
                return malformed_or_truncated(&reader);
            item->responded_device_address = signed_varint(raw);
            item->has_responded_device_address = true;
            break;
        case 5U:
            if (wire != 0U || !read_varint(&reader, &raw))
                return malformed_or_truncated(&reader);
            item->timestamp = signed_varint(raw);
            item->has_timestamp = true;
            break;
        default:
            if (!skip_field(&reader, wire))
                return malformed_or_truncated(&reader);
            break;
        }
    }
    return has_data_id
        ? LINK_MERCEDES_ME_DIAGLOGIC_OK
        : LINK_MERCEDES_ME_DIAGLOGIC_REQUIRED_FIELD_MISSING;
}

static LinkMercedesMeDiaglogicResult find_requested_device_id(
    const uint8_t *bytes,
    size_t size,
    LinkMercedesMeProtoSlice *requested_device_id)
{
    ProtoReader reader;
    bool found = false;
    clear_slice(requested_device_id);
    reader.bytes = bytes;
    reader.size = size;
    reader.position = 0U;
    while (reader.position < reader.size) {
        uint32_t field;
        unsigned int wire;
        if (!read_key(&reader, &field, &wire))
            return malformed_or_truncated(&reader);
        if (field == 1U) {
            if (wire != 2U || !read_slice(&reader, requested_device_id))
                return malformed_or_truncated(&reader);
            found = true;
        } else if (!skip_field(&reader, wire)) {
            return malformed_or_truncated(&reader);
        }
    }
    return found
        ? LINK_MERCEDES_ME_DIAGLOGIC_OK
        : LINK_MERCEDES_ME_DIAGLOGIC_REQUIRED_FIELD_MISSING;
}

LinkMercedesMeDiaglogicResult
link_mercedes_me_diaglogic_decode_measured_item_collection(
    const uint8_t *bytes,
    size_t size,
    LinkMercedesMeDiaglogicMeasuredItemCollectionFn item_fn,
    void *context,
    size_t *item_count)
{
    ProtoReader reader;
    LinkMercedesMeProtoSlice requested;
    LinkMercedesMeDiaglogicResult result;
    size_t count = 0U;
    if ((bytes == NULL && size != 0U))
        return LINK_MERCEDES_ME_DIAGLOGIC_INVALID_ARGUMENT;
    if (item_count != NULL) *item_count = 0U;
    result = find_requested_device_id(bytes, size, &requested);
    if (result != LINK_MERCEDES_ME_DIAGLOGIC_OK) return result;
    reader.bytes = bytes;
    reader.size = size;
    reader.position = 0U;
    while (reader.position < reader.size) {
        uint32_t field;
        unsigned int wire;
        LinkMercedesMeProtoSlice nested;
        LinkMercedesMeDiaglogicMeasuredItem item;
        if (!read_key(&reader, &field, &wire))
            return malformed_or_truncated(&reader);
        if (field == 2U) {
            if (wire != 2U || !read_slice(&reader, &nested))
                return malformed_or_truncated(&reader);
            result = link_mercedes_me_diaglogic_decode_measured_item(
                nested.data, nested.size, &item);
            if (result != LINK_MERCEDES_ME_DIAGLOGIC_OK) return result;
            if (item_fn != NULL) item_fn(context, requested, &item);
            ++count;
        } else if (!skip_field(&reader, wire)) {
            return malformed_or_truncated(&reader);
        }
    }
    if (item_count != NULL) *item_count = count;
    return LINK_MERCEDES_ME_DIAGLOGIC_OK;
}

LinkMercedesMeDiaglogicResult link_mercedes_me_diaglogic_decode_dtc(
    const uint8_t *bytes,
    size_t size,
    LinkMercedesMeDiaglogicDtc *dtc)
{
    ProtoReader reader;
    bool has_code = false;
    bool has_flag = false;
    if (dtc == NULL || (bytes == NULL && size != 0U))
        return LINK_MERCEDES_ME_DIAGLOGIC_INVALID_ARGUMENT;
    memset(dtc, 0, sizeof(*dtc));
    reader.bytes = bytes;
    reader.size = size;
    reader.position = 0U;
    while (reader.position < reader.size) {
        uint32_t field;
        unsigned int wire;
        uint64_t raw;
        if (!read_key(&reader, &field, &wire))
            return malformed_or_truncated(&reader);
        switch (field) {
        case 1U:
            if (wire != 2U || !read_slice(&reader, &dtc->trouble_code))
                return malformed_or_truncated(&reader);
            has_code = true;
            break;
        case 2U:
            if (wire != 0U || !read_varint(&reader, &raw))
                return malformed_or_truncated(&reader);
            dtc->sporadic_flag = raw <= INT_MAX ? (int)raw : INT_MAX;
            has_flag = true;
            break;
        case 3U:
            if (wire != 2U || !read_slice(&reader, &dtc->display_text))
                return malformed_or_truncated(&reader);
            break;
        case 4U:
            if (wire != 2U || !read_slice(&reader, &dtc->responded_device_id))
                return malformed_or_truncated(&reader);
            break;
        default:
            if (!skip_field(&reader, wire))
                return malformed_or_truncated(&reader);
            break;
        }
    }
    return has_code && has_flag
        ? LINK_MERCEDES_ME_DIAGLOGIC_OK
        : LINK_MERCEDES_ME_DIAGLOGIC_REQUIRED_FIELD_MISSING;
}

LinkMercedesMeDiaglogicResult
link_mercedes_me_diaglogic_decode_dtc_collection(
    const uint8_t *bytes,
    size_t size,
    LinkMercedesMeDiaglogicDtcFn dtc_fn,
    void *context,
    size_t *dtc_count)
{
    ProtoReader reader;
    LinkMercedesMeProtoSlice requested;
    LinkMercedesMeDiaglogicResult result;
    size_t count = 0U;
    if (bytes == NULL && size != 0U)
        return LINK_MERCEDES_ME_DIAGLOGIC_INVALID_ARGUMENT;
    if (dtc_count != NULL) *dtc_count = 0U;
    result = find_requested_device_id(bytes, size, &requested);
    if (result != LINK_MERCEDES_ME_DIAGLOGIC_OK) return result;
    reader.bytes = bytes;
    reader.size = size;
    reader.position = 0U;
    while (reader.position < reader.size) {
        uint32_t field;
        unsigned int wire;
        LinkMercedesMeProtoSlice nested;
        LinkMercedesMeDiaglogicDtc dtc;
        if (!read_key(&reader, &field, &wire))
            return malformed_or_truncated(&reader);
        if (field == 2U) {
            if (wire != 2U || !read_slice(&reader, &nested))
                return malformed_or_truncated(&reader);
            result = link_mercedes_me_diaglogic_decode_dtc(
                nested.data, nested.size, &dtc);
            if (result != LINK_MERCEDES_ME_DIAGLOGIC_OK) return result;
            if (dtc_fn != NULL) dtc_fn(context, requested, &dtc);
            ++count;
        } else if (!skip_field(&reader, wire)) {
            return malformed_or_truncated(&reader);
        }
    }
    if (dtc_count != NULL) *dtc_count = count;
    return LINK_MERCEDES_ME_DIAGLOGIC_OK;
}

LinkMercedesMeDiaglogicResult
link_mercedes_me_diaglogic_decode_vehicle_configuration(
    const uint8_t *bytes,
    size_t size,
    LinkMercedesMeDiaglogicVehicleConfiguration *configuration)
{
    ProtoReader reader;
    if (configuration == NULL || (bytes == NULL && size != 0U))
        return LINK_MERCEDES_ME_DIAGLOGIC_INVALID_ARGUMENT;
    memset(configuration, 0, sizeof(*configuration));
    reader.bytes = bytes;
    reader.size = size;
    reader.position = 0U;
    while (reader.position < reader.size) {
        uint32_t field;
        unsigned int wire;
        uint64_t raw;
        if (!read_key(&reader, &field, &wire))
            return malformed_or_truncated(&reader);
        switch (field) {
        case 1U:
            if (wire != 2U || !read_slice(&reader, &configuration->version))
                return malformed_or_truncated(&reader);
            break;
        case 2U:
            if (wire != 2U || !read_slice(&reader, &configuration->variant))
                return malformed_or_truncated(&reader);
            break;
        case 3U:
            if (wire != 0U || !read_varint(&reader, &raw))
                return malformed_or_truncated(&reader);
            configuration->timestamp = signed_varint(raw);
            configuration->has_timestamp = true;
            break;
        default:
            if (!skip_field(&reader, wire))
                return malformed_or_truncated(&reader);
            break;
        }
    }
    return LINK_MERCEDES_ME_DIAGLOGIC_OK;
}

LinkMercedesMeDiaglogicResult link_mercedes_me_diaglogic_decode_vehicle_status(
    const uint8_t *bytes,
    size_t size,
    LinkMercedesMeDiaglogicVehicleStatus *status,
    const LinkMercedesMeDiaglogicCallbacks *callbacks)
{
    ProtoReader reader;
    if (status == NULL || (bytes == NULL && size != 0U))
        return LINK_MERCEDES_ME_DIAGLOGIC_INVALID_ARGUMENT;
    memset(status, 0, sizeof(*status));
    reader.bytes = bytes;
    reader.size = size;
    reader.position = 0U;
    while (reader.position < reader.size) {
        uint32_t field;
        unsigned int wire;
        LinkMercedesMeProtoSlice nested;
        LinkMercedesMeDiaglogicResult result;
        if (!read_key(&reader, &field, &wire))
            return malformed_or_truncated(&reader);
        switch (field) {
        case 1U:
            if (wire != 2U || !read_slice(&reader, &status->assigned_vin))
                return malformed_or_truncated(&reader);
            break;
        case 2U:
            if (wire != 2U ||
                !read_slice(&reader, &status->error_code_as_string))
                return malformed_or_truncated(&reader);
            break;
        case 3U:
            if (wire != 2U || !read_slice(&reader, &status->error_message))
                return malformed_or_truncated(&reader);
            break;
        case 4U: {
            size_t count = 0U;
            if (wire != 2U || !read_slice(&reader, &nested))
                return malformed_or_truncated(&reader);
            result = link_mercedes_me_diaglogic_decode_dtc_collection(
                nested.data, nested.size,
                callbacks != NULL ? callbacks->dtc : NULL,
                callbacks != NULL ? callbacks->context : NULL,
                &count);
            if (result != LINK_MERCEDES_ME_DIAGLOGIC_OK) return result;
            ++status->dtc_collection_count;
            break;
        }
        case 5U: {
            LinkMercedesMeDiaglogicMeasuredItem item;
            if (wire != 2U || !read_slice(&reader, &nested))
                return malformed_or_truncated(&reader);
            result = link_mercedes_me_diaglogic_decode_measured_item(
                nested.data, nested.size, &item);
            if (result != LINK_MERCEDES_ME_DIAGLOGIC_OK) return result;
            if (callbacks != NULL && callbacks->measured_item != NULL)
                callbacks->measured_item(callbacks->context, &item);
            ++status->measured_item_count;
            break;
        }
        case 6U:
            if (wire != 2U || !read_slice(&reader, &nested))
                return malformed_or_truncated(&reader);
            result = link_mercedes_me_diaglogic_decode_vehicle_configuration(
                nested.data, nested.size, &status->vehicle_configuration);
            if (result != LINK_MERCEDES_ME_DIAGLOGIC_OK) return result;
            status->has_vehicle_configuration = true;
            break;
        case 7U:
            if (wire != 2U ||
                !read_slice(&reader, &status->obd_adapter_sw_version))
                return malformed_or_truncated(&reader);
            break;
        default:
            if (!skip_field(&reader, wire))
                return malformed_or_truncated(&reader);
            break;
        }
    }
    return LINK_MERCEDES_ME_DIAGLOGIC_OK;
}

LinkMercedesMeDiaglogicResult link_mercedes_me_diaglogic_decode_preview(
    const uint8_t *bytes,
    size_t size,
    LinkMercedesMeDiaglogicPreview *preview)
{
    ProtoReader reader;
    bool has_cycle_completed = false;
    if (preview == NULL || (bytes == NULL && size != 0U))
        return LINK_MERCEDES_ME_DIAGLOGIC_INVALID_ARGUMENT;
    memset(preview, 0, sizeof(*preview));
    reader.bytes = bytes;
    reader.size = size;
    reader.position = 0U;
    while (reader.position < reader.size) {
        uint32_t field;
        unsigned int wire;
        uint64_t raw;
        if (!read_key(&reader, &field, &wire))
            return malformed_or_truncated(&reader);
        switch (field) {
        case 1U:
            if (wire != 0U || !read_varint(&reader, &raw))
                return malformed_or_truncated(&reader);
            preview->cycle_completed = raw != 0U;
            has_cycle_completed = true;
            break;
        case 2U:
            if (wire != 2U ||
                !read_slice(&reader, &preview->pending_action_token))
                return malformed_or_truncated(&reader);
            break;
        case 3U:
            if (wire != 0U || !read_varint(&reader, &raw))
                return malformed_or_truncated(&reader);
            preview->repeatable = raw != 0U;
            preview->has_repeatable = true;
            break;
        default:
            if (!skip_field(&reader, wire))
                return malformed_or_truncated(&reader);
            break;
        }
    }
    return has_cycle_completed
        ? LINK_MERCEDES_ME_DIAGLOGIC_OK
        : LINK_MERCEDES_ME_DIAGLOGIC_REQUIRED_FIELD_MISSING;
}
