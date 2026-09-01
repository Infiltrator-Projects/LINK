// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file doip.h
 * @brief Portable ISO 13400 DoIP framing for UDS/OBDonUDS.
 *
 * Socket discovery/connection belongs to platform providers. This module owns
 * the transport-neutral generic header and diagnostic-control payloads used to
 * carry UDS over DoIP.
 */
#ifndef LINK_DOIP_H
#define LINK_DOIP_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LINK_DOIP_HEADER_SIZE 8U

#define LINK_DOIP_PAYLOAD_GENERIC_NACK UINT16_C(0x0000)
#define LINK_DOIP_PAYLOAD_VEHICLE_IDENT_REQUEST UINT16_C(0x0001)
#define LINK_DOIP_PAYLOAD_VEHICLE_IDENT_REQUEST_EID UINT16_C(0x0002)
#define LINK_DOIP_PAYLOAD_VEHICLE_IDENT_REQUEST_VIN UINT16_C(0x0003)
#define LINK_DOIP_PAYLOAD_VEHICLE_ANNOUNCEMENT UINT16_C(0x0004)
#define LINK_DOIP_PAYLOAD_ROUTING_ACTIVATION_REQUEST UINT16_C(0x0005)
#define LINK_DOIP_PAYLOAD_ROUTING_ACTIVATION_RESPONSE UINT16_C(0x0006)
#define LINK_DOIP_PAYLOAD_ALIVE_CHECK_REQUEST UINT16_C(0x0007)
#define LINK_DOIP_PAYLOAD_ALIVE_CHECK_RESPONSE UINT16_C(0x0008)
#define LINK_DOIP_PAYLOAD_ENTITY_STATUS_REQUEST UINT16_C(0x4001)
#define LINK_DOIP_PAYLOAD_ENTITY_STATUS_RESPONSE UINT16_C(0x4002)
#define LINK_DOIP_PAYLOAD_POWER_MODE_REQUEST UINT16_C(0x4003)
#define LINK_DOIP_PAYLOAD_POWER_MODE_RESPONSE UINT16_C(0x4004)
#define LINK_DOIP_PAYLOAD_DIAGNOSTIC_MESSAGE UINT16_C(0x8001)
#define LINK_DOIP_PAYLOAD_DIAGNOSTIC_ACK UINT16_C(0x8002)
#define LINK_DOIP_PAYLOAD_DIAGNOSTIC_NACK UINT16_C(0x8003)

typedef enum LinkDoipResult {
    LINK_DOIP_RESULT_OK = 0,
    LINK_DOIP_RESULT_INVALID_ARGUMENT,
    LINK_DOIP_RESULT_BUFFER_TOO_SMALL,
    LINK_DOIP_RESULT_MALFORMED_FRAME,
    LINK_DOIP_RESULT_UNEXPECTED_PAYLOAD
} LinkDoipResult;

typedef struct LinkDoipHeader {
    uint8_t protocol_version;
    uint16_t payload_type;
    uint32_t payload_length;
} LinkDoipHeader;

const char *link_doip_result_name(LinkDoipResult result);

LinkDoipResult link_doip_encode_frame(
    uint8_t protocol_version,
    uint16_t payload_type,
    const uint8_t *payload,
    size_t payload_length,
    uint8_t *frame,
    size_t frame_capacity,
    size_t *written);

LinkDoipResult link_doip_decode_frame(
    const uint8_t *frame,
    size_t frame_length,
    LinkDoipHeader *header,
    const uint8_t **payload);

LinkDoipResult link_doip_build_routing_activation_request(
    uint8_t protocol_version,
    uint16_t source_address,
    uint8_t activation_type,
    uint8_t *frame,
    size_t frame_capacity,
    size_t *written);

LinkDoipResult link_doip_build_alive_check_response(
    uint8_t protocol_version,
    uint16_t source_address,
    uint8_t *frame,
    size_t frame_capacity,
    size_t *written);

LinkDoipResult link_doip_build_diagnostic_message(
    uint8_t protocol_version,
    uint16_t source_address,
    uint16_t target_address,
    const uint8_t *diagnostic_payload,
    size_t diagnostic_payload_length,
    uint8_t *frame,
    size_t frame_capacity,
    size_t *written);

LinkDoipResult link_doip_decode_diagnostic_message(
    const uint8_t *frame,
    size_t frame_length,
    uint16_t *source_address,
    uint16_t *target_address,
    const uint8_t **diagnostic_payload,
    size_t *diagnostic_payload_length);

#ifdef __cplusplus
}
#endif
#endif
