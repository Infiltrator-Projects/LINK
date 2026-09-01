// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/doip.h"

#include <stdint.h>
#include <string.h>

static void doip_write_u16(uint8_t *out, uint16_t value)
{
    out[0] = (uint8_t)(value >> 8U);
    out[1] = (uint8_t)value;
}

static void doip_write_u32(uint8_t *out, uint32_t value)
{
    out[0] = (uint8_t)(value >> 24U);
    out[1] = (uint8_t)(value >> 16U);
    out[2] = (uint8_t)(value >> 8U);
    out[3] = (uint8_t)value;
}

static uint16_t doip_read_u16(const uint8_t *in)
{
    return (uint16_t)(((uint16_t)in[0] << 8U) | (uint16_t)in[1]);
}

static uint32_t doip_read_u32(const uint8_t *in)
{
    return ((uint32_t)in[0] << 24U) |
           ((uint32_t)in[1] << 16U) |
           ((uint32_t)in[2] << 8U) |
           (uint32_t)in[3];
}

const char *link_doip_result_name(LinkDoipResult result)
{
    switch (result) {
    case LINK_DOIP_RESULT_OK: return "ok";
    case LINK_DOIP_RESULT_INVALID_ARGUMENT: return "invalid-argument";
    case LINK_DOIP_RESULT_BUFFER_TOO_SMALL: return "buffer-too-small";
    case LINK_DOIP_RESULT_MALFORMED_FRAME: return "malformed-frame";
    case LINK_DOIP_RESULT_UNEXPECTED_PAYLOAD: return "unexpected-payload";
    }
    return "unknown";
}

LinkDoipResult link_doip_encode_frame(
    uint8_t protocol_version,
    uint16_t payload_type,
    const uint8_t *payload,
    size_t payload_length,
    uint8_t *frame,
    size_t frame_capacity,
    size_t *written)
{
    size_t total;
    if (written != NULL) *written = 0U;
    if (frame == NULL || written == NULL ||
        (payload == NULL && payload_length != 0U) ||
        payload_length > UINT32_MAX)
        return LINK_DOIP_RESULT_INVALID_ARGUMENT;
    if (payload_length > SIZE_MAX - LINK_DOIP_HEADER_SIZE)
        return LINK_DOIP_RESULT_INVALID_ARGUMENT;
    total = LINK_DOIP_HEADER_SIZE + payload_length;
    if (frame_capacity < total)
        return LINK_DOIP_RESULT_BUFFER_TOO_SMALL;

    frame[0] = protocol_version;
    frame[1] = (uint8_t)~protocol_version;
    doip_write_u16(frame + 2U, payload_type);
    doip_write_u32(frame + 4U, (uint32_t)payload_length);
    if (payload_length != 0U)
        memcpy(frame + LINK_DOIP_HEADER_SIZE, payload, payload_length);
    *written = total;
    return LINK_DOIP_RESULT_OK;
}

LinkDoipResult link_doip_decode_frame(
    const uint8_t *frame,
    size_t frame_length,
    LinkDoipHeader *header,
    const uint8_t **payload)
{
    uint32_t payload_length;
    size_t total;
    if (frame == NULL || header == NULL || payload == NULL)
        return LINK_DOIP_RESULT_INVALID_ARGUMENT;
    if (frame_length < LINK_DOIP_HEADER_SIZE)
        return LINK_DOIP_RESULT_MALFORMED_FRAME;
    if ((uint8_t)(frame[0] ^ frame[1]) != UINT8_C(0xFF))
        return LINK_DOIP_RESULT_MALFORMED_FRAME;

    payload_length = doip_read_u32(frame + 4U);
    if ((uint64_t)payload_length + LINK_DOIP_HEADER_SIZE > SIZE_MAX)
        return LINK_DOIP_RESULT_MALFORMED_FRAME;
    total = LINK_DOIP_HEADER_SIZE + (size_t)payload_length;
    if (frame_length != total)
        return LINK_DOIP_RESULT_MALFORMED_FRAME;

    header->protocol_version = frame[0];
    header->payload_type = doip_read_u16(frame + 2U);
    header->payload_length = payload_length;
    *payload = frame + LINK_DOIP_HEADER_SIZE;
    return LINK_DOIP_RESULT_OK;
}

LinkDoipResult link_doip_build_routing_activation_request(
    uint8_t protocol_version,
    uint16_t source_address,
    uint8_t activation_type,
    uint8_t *frame,
    size_t frame_capacity,
    size_t *written)
{
    uint8_t payload[7U] = {0};
    doip_write_u16(payload, source_address);
    payload[2] = activation_type;
    return link_doip_encode_frame(
        protocol_version, LINK_DOIP_PAYLOAD_ROUTING_ACTIVATION_REQUEST,
        payload, sizeof(payload), frame, frame_capacity, written);
}

LinkDoipResult link_doip_build_alive_check_response(
    uint8_t protocol_version,
    uint16_t source_address,
    uint8_t *frame,
    size_t frame_capacity,
    size_t *written)
{
    uint8_t payload[2U];
    doip_write_u16(payload, source_address);
    return link_doip_encode_frame(
        protocol_version, LINK_DOIP_PAYLOAD_ALIVE_CHECK_RESPONSE,
        payload, sizeof(payload), frame, frame_capacity, written);
}

LinkDoipResult link_doip_build_diagnostic_message(
    uint8_t protocol_version,
    uint16_t source_address,
    uint16_t target_address,
    const uint8_t *diagnostic_payload,
    size_t diagnostic_payload_length,
    uint8_t *frame,
    size_t frame_capacity,
    size_t *written)
{
    size_t payload_length;
    size_t total;
    if (written != NULL) *written = 0U;
    if (frame == NULL || written == NULL ||
        (diagnostic_payload == NULL && diagnostic_payload_length != 0U) ||
        diagnostic_payload_length > UINT32_MAX - 4U)
        return LINK_DOIP_RESULT_INVALID_ARGUMENT;
    payload_length = 4U + diagnostic_payload_length;
    if (payload_length > SIZE_MAX - LINK_DOIP_HEADER_SIZE)
        return LINK_DOIP_RESULT_INVALID_ARGUMENT;
    total = LINK_DOIP_HEADER_SIZE + payload_length;
    if (frame_capacity < total)
        return LINK_DOIP_RESULT_BUFFER_TOO_SMALL;

    frame[0] = protocol_version;
    frame[1] = (uint8_t)~protocol_version;
    doip_write_u16(frame + 2U, LINK_DOIP_PAYLOAD_DIAGNOSTIC_MESSAGE);
    doip_write_u32(frame + 4U, (uint32_t)payload_length);
    doip_write_u16(frame + 8U, source_address);
    doip_write_u16(frame + 10U, target_address);
    if (diagnostic_payload_length != 0U)
        memcpy(frame + 12U, diagnostic_payload, diagnostic_payload_length);
    *written = total;
    return LINK_DOIP_RESULT_OK;
}

LinkDoipResult link_doip_decode_diagnostic_message(
    const uint8_t *frame,
    size_t frame_length,
    uint16_t *source_address,
    uint16_t *target_address,
    const uint8_t **diagnostic_payload,
    size_t *diagnostic_payload_length)
{
    LinkDoipHeader header;
    const uint8_t *payload;
    LinkDoipResult result;
    if (source_address == NULL || target_address == NULL ||
        diagnostic_payload == NULL || diagnostic_payload_length == NULL)
        return LINK_DOIP_RESULT_INVALID_ARGUMENT;

    *diagnostic_payload = NULL;
    *diagnostic_payload_length = 0U;
    result = link_doip_decode_frame(frame, frame_length, &header, &payload);
    if (result != LINK_DOIP_RESULT_OK)
        return result;
    if (header.payload_type != LINK_DOIP_PAYLOAD_DIAGNOSTIC_MESSAGE ||
        header.payload_length < 4U)
        return LINK_DOIP_RESULT_UNEXPECTED_PAYLOAD;

    *source_address = doip_read_u16(payload);
    *target_address = doip_read_u16(payload + 2U);
    *diagnostic_payload = payload + 4U;
    *diagnostic_payload_length = (size_t)header.payload_length - 4U;
    return LINK_DOIP_RESULT_OK;
}
