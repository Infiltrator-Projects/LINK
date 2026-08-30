// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/kwp2000.h"

#include <string.h>

static LinkKwp2000Result write_failure(
    uint8_t *buffer, size_t buffer_size, size_t *written,
    LinkKwp2000Result result)
{
    if (buffer != NULL && buffer_size != 0U) buffer[0] = 0U;
    if (written != NULL) *written = 0U;
    return result;
}

static int request_service_is_supported(uint8_t service)
{
    switch (service) {
    case LINK_KWP2000_SERVICE_READ_DTC_BY_STATUS:
    case LINK_KWP2000_SERVICE_READ_ECU_IDENTIFICATION:
    case LINK_KWP2000_SERVICE_READ_DATA_BY_LOCAL_IDENTIFIER:
    case LINK_KWP2000_SERVICE_READ_DATA_BY_COMMON_IDENTIFIER:
    case LINK_KWP2000_SERVICE_TESTER_PRESENT:
        return 1;
    default:
        return 0;
    }
}

const char *link_kwp2000_result_name(LinkKwp2000Result result)
{
    switch (result) {
    case LINK_KWP2000_RESULT_OK: return "ok";
    case LINK_KWP2000_RESULT_NEGATIVE_RESPONSE: return "negative-response";
    case LINK_KWP2000_RESULT_INVALID_ARGUMENT: return "invalid-argument";
    case LINK_KWP2000_RESULT_BUFFER_TOO_SMALL: return "buffer-too-small";
    case LINK_KWP2000_RESULT_MALFORMED_PDU: return "malformed-pdu";
    case LINK_KWP2000_RESULT_UNEXPECTED_RESPONSE: return "unexpected-response";
    case LINK_KWP2000_RESULT_TRUNCATED: return "truncated";
    }
    return "unknown";
}

LinkKwp2000Result link_kwp2000_decode_response(
    uint8_t request_service,
    const uint8_t *pdu,
    size_t pdu_length,
    LinkKwp2000Response *response)
{
    LinkKwp2000Response decoded;
    uint8_t positive_service;

    if (!request_service_is_supported(request_service) ||
        pdu == NULL || response == NULL) {
        return LINK_KWP2000_RESULT_INVALID_ARGUMENT;
    }
    if (pdu_length == 0U) return LINK_KWP2000_RESULT_MALFORMED_PDU;

    memset(&decoded, 0, sizeof(decoded));
    decoded.request_service = request_service;
    positive_service = (uint8_t)(request_service + UINT8_C(0x40));

    if (pdu[0] == LINK_KWP2000_SERVICE_NEGATIVE_RESPONSE) {
        if (pdu_length != 3U || pdu[1] != request_service || pdu[2] == 0U)
            return LINK_KWP2000_RESULT_MALFORMED_PDU;
        decoded.kind = LINK_KWP2000_RESPONSE_NEGATIVE;
        decoded.response_service = pdu[0];
        decoded.negative_response_code = pdu[2];
        *response = decoded;
        return LINK_KWP2000_RESULT_NEGATIVE_RESPONSE;
    }
    if (pdu[0] != positive_service)
        return LINK_KWP2000_RESULT_UNEXPECTED_RESPONSE;

    decoded.kind = LINK_KWP2000_RESPONSE_POSITIVE;
    decoded.response_service = pdu[0];
    decoded.data = pdu + 1U;
    decoded.data_length = pdu_length - 1U;
    *response = decoded;
    return LINK_KWP2000_RESULT_OK;
}

LinkKwp2000Result link_kwp2000_build_read_local_identifier_request(
    uint8_t identifier,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *written)
{
    if (buffer == NULL || written == NULL || identifier == 0U)
        return write_failure(buffer, buffer_size, written,
                             LINK_KWP2000_RESULT_INVALID_ARGUMENT);
    if (buffer_size < 2U)
        return write_failure(buffer, buffer_size, written,
                             LINK_KWP2000_RESULT_BUFFER_TOO_SMALL);
    buffer[0] = LINK_KWP2000_SERVICE_READ_DATA_BY_LOCAL_IDENTIFIER;
    buffer[1] = identifier;
    *written = 2U;
    return LINK_KWP2000_RESULT_OK;
}

LinkKwp2000Result link_kwp2000_decode_read_local_identifier_response(
    const uint8_t *pdu,
    size_t pdu_length,
    uint8_t expected_identifier,
    LinkKwp2000LocalIdentifierRecord *record)
{
    LinkKwp2000Response response;
    LinkKwp2000LocalIdentifierRecord decoded;
    LinkKwp2000Result result;

    if (record == NULL || expected_identifier == 0U)
        return LINK_KWP2000_RESULT_INVALID_ARGUMENT;
    result = link_kwp2000_decode_response(
        LINK_KWP2000_SERVICE_READ_DATA_BY_LOCAL_IDENTIFIER,
        pdu, pdu_length, &response);
    if (result != LINK_KWP2000_RESULT_OK) return result;
    if (response.data_length < 1U) return LINK_KWP2000_RESULT_MALFORMED_PDU;
    if (response.data[0] != expected_identifier)
        return LINK_KWP2000_RESULT_UNEXPECTED_RESPONSE;

    decoded.identifier = response.data[0];
    decoded.data = response.data + 1U;
    decoded.data_length = response.data_length - 1U;
    *record = decoded;
    return LINK_KWP2000_RESULT_OK;
}

LinkKwp2000Result link_kwp2000_build_tester_present_request(
    int response_required,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *written)
{
    if (buffer == NULL || written == NULL)
        return write_failure(buffer, buffer_size, written,
                             LINK_KWP2000_RESULT_INVALID_ARGUMENT);
    if (buffer_size < 2U)
        return write_failure(buffer, buffer_size, written,
                             LINK_KWP2000_RESULT_BUFFER_TOO_SMALL);
    buffer[0] = LINK_KWP2000_SERVICE_TESTER_PRESENT;
    buffer[1] = response_required
        ? LINK_KWP2000_TESTER_PRESENT_RESPONSE_REQUIRED
        : LINK_KWP2000_TESTER_PRESENT_NO_RESPONSE;
    *written = 2U;
    return LINK_KWP2000_RESULT_OK;
}

LinkKwp2000Result link_kwp2000_decode_tester_present_response(
    const uint8_t *pdu,
    size_t pdu_length,
    int response_required)
{
    LinkKwp2000Response response;
    LinkKwp2000Result result = link_kwp2000_decode_response(
        LINK_KWP2000_SERVICE_TESTER_PRESENT, pdu, pdu_length, &response);
    if (result != LINK_KWP2000_RESULT_OK) return result;

    /*
     * ISO 14230 implementations commonly return only 0x7E. Some stacks echo
     * the response-type byte. Accept either shape but reject unrelated data.
     */
    if (response.data_length == 0U) return LINK_KWP2000_RESULT_OK;
    if (response.data_length == 1U &&
        response.data[0] ==
            (response_required
                ? LINK_KWP2000_TESTER_PRESENT_RESPONSE_REQUIRED
                : LINK_KWP2000_TESTER_PRESENT_NO_RESPONSE)) {
        return LINK_KWP2000_RESULT_OK;
    }
    return LINK_KWP2000_RESULT_UNEXPECTED_RESPONSE;
}

LinkKwp2000Result link_kwp2000_build_read_common_identifier_request(
    uint16_t identifier,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *written)
{
    if (buffer == NULL || written == NULL)
        return write_failure(buffer, buffer_size, written,
                             LINK_KWP2000_RESULT_INVALID_ARGUMENT);
    if (buffer_size < 3U)
        return write_failure(buffer, buffer_size, written,
                             LINK_KWP2000_RESULT_BUFFER_TOO_SMALL);
    buffer[0] = LINK_KWP2000_SERVICE_READ_DATA_BY_COMMON_IDENTIFIER;
    buffer[1] = (uint8_t)(identifier >> 8U);
    buffer[2] = (uint8_t)identifier;
    *written = 3U;
    return LINK_KWP2000_RESULT_OK;
}

LinkKwp2000Result link_kwp2000_decode_read_common_identifier_response(
    const uint8_t *pdu,
    size_t pdu_length,
    uint16_t expected_identifier,
    LinkKwp2000CommonIdentifierRecord *record)
{
    LinkKwp2000Response response;
    LinkKwp2000CommonIdentifierRecord decoded;
    LinkKwp2000Result result;
    uint16_t identifier;

    if (record == NULL) return LINK_KWP2000_RESULT_INVALID_ARGUMENT;
    result = link_kwp2000_decode_response(
        LINK_KWP2000_SERVICE_READ_DATA_BY_COMMON_IDENTIFIER,
        pdu, pdu_length, &response);
    if (result != LINK_KWP2000_RESULT_OK) return result;
    if (response.data_length < 2U) return LINK_KWP2000_RESULT_MALFORMED_PDU;

    identifier = (uint16_t)(((uint16_t)response.data[0] << 8U) |
                            response.data[1]);
    if (identifier != expected_identifier)
        return LINK_KWP2000_RESULT_UNEXPECTED_RESPONSE;

    decoded.identifier = identifier;
    decoded.data = response.data + 2U;
    decoded.data_length = response.data_length - 2U;
    *record = decoded;
    return LINK_KWP2000_RESULT_OK;
}

LinkKwp2000Result link_kwp2000_build_read_ecu_identification_request(
    uint8_t option,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *written)
{
    if (buffer == NULL || written == NULL || option == 0U)
        return write_failure(buffer, buffer_size, written,
                             LINK_KWP2000_RESULT_INVALID_ARGUMENT);
    if (buffer_size < 2U)
        return write_failure(buffer, buffer_size, written,
                             LINK_KWP2000_RESULT_BUFFER_TOO_SMALL);
    buffer[0] = LINK_KWP2000_SERVICE_READ_ECU_IDENTIFICATION;
    buffer[1] = option;
    *written = 2U;
    return LINK_KWP2000_RESULT_OK;
}

LinkKwp2000Result link_kwp2000_decode_read_ecu_identification_response(
    const uint8_t *pdu,
    size_t pdu_length,
    uint8_t expected_option,
    LinkKwp2000EcuIdentificationRecord *record)
{
    LinkKwp2000Response response;
    LinkKwp2000EcuIdentificationRecord decoded;
    LinkKwp2000Result result;

    if (record == NULL || expected_option == 0U)
        return LINK_KWP2000_RESULT_INVALID_ARGUMENT;
    result = link_kwp2000_decode_response(
        LINK_KWP2000_SERVICE_READ_ECU_IDENTIFICATION,
        pdu, pdu_length, &response);
    if (result != LINK_KWP2000_RESULT_OK) return result;
    if (response.data_length < 1U) return LINK_KWP2000_RESULT_MALFORMED_PDU;
    if (response.data[0] != expected_option)
        return LINK_KWP2000_RESULT_UNEXPECTED_RESPONSE;

    decoded.option = response.data[0];
    decoded.data = response.data + 1U;
    decoded.data_length = response.data_length - 1U;
    *record = decoded;
    return LINK_KWP2000_RESULT_OK;
}

LinkKwp2000Result link_kwp2000_build_read_dtc_by_status_request(
    uint8_t request_type,
    uint16_t group_of_dtc,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *written)
{
    if (buffer == NULL || written == NULL || request_type == 0U)
        return write_failure(buffer, buffer_size, written,
                             LINK_KWP2000_RESULT_INVALID_ARGUMENT);
    if (buffer_size < 4U)
        return write_failure(buffer, buffer_size, written,
                             LINK_KWP2000_RESULT_BUFFER_TOO_SMALL);
    buffer[0] = LINK_KWP2000_SERVICE_READ_DTC_BY_STATUS;
    buffer[1] = request_type;
    buffer[2] = (uint8_t)(group_of_dtc >> 8U);
    buffer[3] = (uint8_t)group_of_dtc;
    *written = 4U;
    return LINK_KWP2000_RESULT_OK;
}

LinkKwp2000Result link_kwp2000_decode_read_dtc_by_status_response(
    const uint8_t *pdu,
    size_t pdu_length,
    LinkKwp2000DtcList *dtcs)
{
    LinkKwp2000Response response;
    LinkKwp2000Result result;
    size_t reported;
    size_t available;
    size_t index;

    if (dtcs == NULL) return LINK_KWP2000_RESULT_INVALID_ARGUMENT;
    memset(dtcs, 0, sizeof(*dtcs));

    result = link_kwp2000_decode_response(
        LINK_KWP2000_SERVICE_READ_DTC_BY_STATUS,
        pdu, pdu_length, &response);
    if (result != LINK_KWP2000_RESULT_OK) return result;
    if (response.data_length < 1U) return LINK_KWP2000_RESULT_MALFORMED_PDU;

    reported = response.data[0];
    if (response.data_length < 1U + reported * 3U)
        return LINK_KWP2000_RESULT_MALFORMED_PDU;

    dtcs->reported_count = reported;
    available = reported;
    if (available > LINK_KWP2000_MAX_DTCS) {
        available = LINK_KWP2000_MAX_DTCS;
        dtcs->truncated = 1;
    }
    for (index = 0U; index < available; ++index) {
        const size_t offset = 1U + index * 3U;
        dtcs->entries[index].code =
            (uint16_t)(((uint16_t)response.data[offset] << 8U) |
                       response.data[offset + 1U]);
        dtcs->entries[index].status = response.data[offset + 2U];
    }
    dtcs->count = available;
    return dtcs->truncated
        ? LINK_KWP2000_RESULT_TRUNCATED
        : LINK_KWP2000_RESULT_OK;
}
