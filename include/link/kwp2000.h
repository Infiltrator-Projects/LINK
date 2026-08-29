/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef LINK_KWP2000_H
#define LINK_KWP2000_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ISO 14230-3 / KWP2000 application-layer helpers.
 *
 * This first LINK surface deliberately exposes only non-mutating diagnostic
 * reads plus the generic response decoder. It is suitable for KWP2000 carried
 * over ISO-TP/CAN as used by legacy and mixed-protocol vehicle networks.
 */
#define LINK_KWP2000_SERVICE_READ_DTC_BY_STATUS UINT8_C(0x18)
#define LINK_KWP2000_SERVICE_READ_ECU_IDENTIFICATION UINT8_C(0x1a)
#define LINK_KWP2000_SERVICE_READ_DATA_BY_COMMON_IDENTIFIER UINT8_C(0x22)
#define LINK_KWP2000_SERVICE_TESTER_PRESENT UINT8_C(0x3e)
#define LINK_KWP2000_SERVICE_NEGATIVE_RESPONSE UINT8_C(0x7f)

#define LINK_KWP2000_TESTER_PRESENT_RESPONSE_REQUIRED UINT8_C(0x01)
#define LINK_KWP2000_TESTER_PRESENT_NO_RESPONSE UINT8_C(0x02)

#define LINK_KWP2000_DTC_REQUEST_STORED_AND_STATUS UINT8_C(0x02)
#define LINK_KWP2000_DTC_GROUP_ALL UINT16_C(0xff00)
#define LINK_KWP2000_MAX_DTCS 64U

typedef enum LinkKwp2000Result {
    LINK_KWP2000_RESULT_OK = 0,
    LINK_KWP2000_RESULT_NEGATIVE_RESPONSE,
    LINK_KWP2000_RESULT_INVALID_ARGUMENT,
    LINK_KWP2000_RESULT_BUFFER_TOO_SMALL,
    LINK_KWP2000_RESULT_MALFORMED_PDU,
    LINK_KWP2000_RESULT_UNEXPECTED_RESPONSE,
    LINK_KWP2000_RESULT_TRUNCATED
} LinkKwp2000Result;

typedef enum LinkKwp2000ResponseKind {
    LINK_KWP2000_RESPONSE_POSITIVE = 0,
    LINK_KWP2000_RESPONSE_NEGATIVE
} LinkKwp2000ResponseKind;

typedef struct LinkKwp2000Response {
    LinkKwp2000ResponseKind kind;
    uint8_t request_service;
    uint8_t response_service;
    uint8_t negative_response_code;
    const uint8_t *data;
    size_t data_length;
} LinkKwp2000Response;

typedef struct LinkKwp2000CommonIdentifierRecord {
    uint16_t identifier;
    const uint8_t *data;
    size_t data_length;
} LinkKwp2000CommonIdentifierRecord;

typedef struct LinkKwp2000EcuIdentificationRecord {
    uint8_t option;
    const uint8_t *data;
    size_t data_length;
} LinkKwp2000EcuIdentificationRecord;

typedef struct LinkKwp2000Dtc {
    uint16_t code;
    uint8_t status;
} LinkKwp2000Dtc;

typedef struct LinkKwp2000DtcList {
    LinkKwp2000Dtc entries[LINK_KWP2000_MAX_DTCS];
    size_t count;
    size_t reported_count;
    int truncated;
} LinkKwp2000DtcList;

const char *link_kwp2000_result_name(LinkKwp2000Result result);

LinkKwp2000Result link_kwp2000_decode_response(
    uint8_t request_service,
    const uint8_t *pdu,
    size_t pdu_length,
    LinkKwp2000Response *response);

LinkKwp2000Result link_kwp2000_build_tester_present_request(
    int response_required,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *written);

LinkKwp2000Result link_kwp2000_decode_tester_present_response(
    const uint8_t *pdu,
    size_t pdu_length,
    int response_required);

LinkKwp2000Result link_kwp2000_build_read_common_identifier_request(
    uint16_t identifier,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *written);

LinkKwp2000Result link_kwp2000_decode_read_common_identifier_response(
    const uint8_t *pdu,
    size_t pdu_length,
    uint16_t expected_identifier,
    LinkKwp2000CommonIdentifierRecord *record);

LinkKwp2000Result link_kwp2000_build_read_ecu_identification_request(
    uint8_t option,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *written);

LinkKwp2000Result link_kwp2000_decode_read_ecu_identification_response(
    const uint8_t *pdu,
    size_t pdu_length,
    uint8_t expected_option,
    LinkKwp2000EcuIdentificationRecord *record);

LinkKwp2000Result link_kwp2000_build_read_dtc_by_status_request(
    uint8_t request_type,
    uint16_t group_of_dtc,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *written);

LinkKwp2000Result link_kwp2000_decode_read_dtc_by_status_response(
    const uint8_t *pdu,
    size_t pdu_length,
    LinkKwp2000DtcList *dtcs);

#ifdef __cplusplus
}
#endif

#endif
