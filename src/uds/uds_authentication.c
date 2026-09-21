// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/uds_authentication.h"

#include <string.h>

static LinkUdsResult auth_fail(
    uint8_t *buffer, size_t buffer_size, size_t *written,
    LinkUdsResult result)
{
    if (written != NULL) *written = 0U;
    if (buffer != NULL && buffer_size != 0U) buffer[0] = 0U;
    return result;
}

static bool auth_put(
    uint8_t *buffer, size_t capacity, size_t *offset,
    const uint8_t *data, size_t length)
{
    if (buffer == NULL || offset == NULL ||
        (length != 0U && data == NULL) ||
        *offset > capacity || length > capacity - *offset) {
        return false;
    }
    if (length != 0U) {
        memcpy(buffer + *offset, data, length);
        *offset += length;
    }
    return true;
}

static bool auth_put_u8(
    uint8_t *buffer, size_t capacity, size_t *offset, uint8_t value)
{
    return auth_put(buffer, capacity, offset, &value, 1U);
}

static bool auth_put_u16(
    uint8_t *buffer, size_t capacity, size_t *offset, uint16_t value)
{
    const uint8_t bytes[2U] = {
        (uint8_t)(value >> 8U), (uint8_t)value
    };
    return auth_put(buffer, capacity, offset, bytes, sizeof(bytes));
}

static bool auth_span_args_valid(
    const uint8_t *data, size_t length, bool nonempty)
{
    return length <= UINT16_MAX &&
           (!nonempty || length != 0U) &&
           (length == 0U || data != NULL);
}

static bool auth_put_span16(
    uint8_t *buffer, size_t capacity, size_t *offset,
    const uint8_t *data, size_t length)
{
    return auth_put_u16(buffer, capacity, offset, (uint16_t)length) &&
           auth_put(buffer, capacity, offset, data, length);
}

static LinkUdsResult auth_start(
    uint8_t task, bool suppress_positive_response,
    uint8_t *buffer, size_t capacity, size_t *offset)
{
    uint8_t encoded;
    if (task > LINK_UDS_AUTH_CONFIGURATION || buffer == NULL ||
        offset == NULL) {
        return LINK_UDS_RESULT_INVALID_ARGUMENT;
    }
    if (capacity < 2U) return LINK_UDS_RESULT_BUFFER_TOO_SMALL;
    encoded = task;
    if (suppress_positive_response) encoded |= UINT8_C(0x80);
    buffer[0] = LINK_UDS_SERVICE_AUTHENTICATION;
    buffer[1] = encoded;
    *offset = 2U;
    return LINK_UDS_RESULT_OK;
}

LinkUdsResult link_uds_build_authentication_deauthenticate_request(
    bool suppress_positive_response,
    uint8_t *buffer, size_t buffer_size, size_t *written)
{
    size_t offset = 0U;
    LinkUdsResult result;
    if (written == NULL) return LINK_UDS_RESULT_INVALID_ARGUMENT;
    result = auth_start(
        LINK_UDS_AUTH_DEAUTHENTICATE, suppress_positive_response,
        buffer, buffer_size, &offset);
    if (result != LINK_UDS_RESULT_OK)
        return auth_fail(buffer, buffer_size, written, result);
    *written = offset;
    return LINK_UDS_RESULT_OK;
}

LinkUdsResult link_uds_build_authentication_verify_certificate_request(
    uint8_t task, bool suppress_positive_response,
    uint8_t communication_configuration,
    const uint8_t *certificate_client, size_t certificate_client_length,
    const uint8_t *challenge_client, size_t challenge_client_length,
    uint8_t *buffer, size_t buffer_size, size_t *written)
{
    size_t offset = 0U;
    LinkUdsResult result;
    const bool bidirectional =
        task == LINK_UDS_AUTH_VERIFY_CERTIFICATE_BIDIRECTIONAL;

    if (written == NULL ||
        (task != LINK_UDS_AUTH_VERIFY_CERTIFICATE_UNIDIRECTIONAL &&
         !bidirectional) ||
        !auth_span_args_valid(
            certificate_client, certificate_client_length, true) ||
        !auth_span_args_valid(
            challenge_client, challenge_client_length, bidirectional)) {
        return auth_fail(
            buffer, buffer_size, written, LINK_UDS_RESULT_INVALID_ARGUMENT);
    }
    result = auth_start(
        task, suppress_positive_response, buffer, buffer_size, &offset);
    if (result != LINK_UDS_RESULT_OK ||
        !auth_put_u8(
            buffer, buffer_size, &offset, communication_configuration) ||
        !auth_put_span16(
            buffer, buffer_size, &offset,
            certificate_client, certificate_client_length) ||
        !auth_put_span16(
            buffer, buffer_size, &offset,
            challenge_client, challenge_client_length)) {
        return auth_fail(
            buffer, buffer_size, written,
            result == LINK_UDS_RESULT_OK
                ? LINK_UDS_RESULT_BUFFER_TOO_SMALL : result);
    }
    *written = offset;
    return LINK_UDS_RESULT_OK;
}

LinkUdsResult link_uds_build_authentication_proof_of_ownership_request(
    bool suppress_positive_response,
    const uint8_t *proof_client, size_t proof_client_length,
    const uint8_t *ephemeral_public_key_client,
    size_t ephemeral_public_key_client_length,
    uint8_t *buffer, size_t buffer_size, size_t *written)
{
    size_t offset = 0U;
    LinkUdsResult result;
    if (written == NULL ||
        !auth_span_args_valid(proof_client, proof_client_length, true) ||
        !auth_span_args_valid(
            ephemeral_public_key_client,
            ephemeral_public_key_client_length, false)) {
        return auth_fail(
            buffer, buffer_size, written, LINK_UDS_RESULT_INVALID_ARGUMENT);
    }
    result = auth_start(
        LINK_UDS_AUTH_PROOF_OF_OWNERSHIP, suppress_positive_response,
        buffer, buffer_size, &offset);
    if (result != LINK_UDS_RESULT_OK ||
        !auth_put_span16(
            buffer, buffer_size, &offset, proof_client, proof_client_length) ||
        !auth_put_span16(
            buffer, buffer_size, &offset,
            ephemeral_public_key_client,
            ephemeral_public_key_client_length)) {
        return auth_fail(
            buffer, buffer_size, written,
            result == LINK_UDS_RESULT_OK
                ? LINK_UDS_RESULT_BUFFER_TOO_SMALL : result);
    }
    *written = offset;
    return LINK_UDS_RESULT_OK;
}

LinkUdsResult link_uds_build_authentication_transmit_certificate_request(
    bool suppress_positive_response, uint16_t certificate_evaluation_id,
    const uint8_t *certificate_data, size_t certificate_data_length,
    uint8_t *buffer, size_t buffer_size, size_t *written)
{
    size_t offset = 0U;
    LinkUdsResult result;
    if (written == NULL ||
        !auth_span_args_valid(certificate_data, certificate_data_length, true)) {
        return auth_fail(
            buffer, buffer_size, written, LINK_UDS_RESULT_INVALID_ARGUMENT);
    }
    result = auth_start(
        LINK_UDS_AUTH_TRANSMIT_CERTIFICATE, suppress_positive_response,
        buffer, buffer_size, &offset);
    if (result != LINK_UDS_RESULT_OK ||
        !auth_put_u16(
            buffer, buffer_size, &offset, certificate_evaluation_id) ||
        !auth_put_span16(
            buffer, buffer_size, &offset,
            certificate_data, certificate_data_length)) {
        return auth_fail(
            buffer, buffer_size, written,
            result == LINK_UDS_RESULT_OK
                ? LINK_UDS_RESULT_BUFFER_TOO_SMALL : result);
    }
    *written = offset;
    return LINK_UDS_RESULT_OK;
}

LinkUdsResult link_uds_build_authentication_request_challenge_request(
    bool suppress_positive_response, uint8_t communication_configuration,
    const uint8_t algorithm_indicator[LINK_UDS_AUTH_ALGORITHM_INDICATOR_BYTES],
    uint8_t *buffer, size_t buffer_size, size_t *written)
{
    size_t offset = 0U;
    LinkUdsResult result;
    if (written == NULL || algorithm_indicator == NULL)
        return auth_fail(
            buffer, buffer_size, written, LINK_UDS_RESULT_INVALID_ARGUMENT);
    result = auth_start(
        LINK_UDS_AUTH_REQUEST_CHALLENGE, suppress_positive_response,
        buffer, buffer_size, &offset);
    if (result != LINK_UDS_RESULT_OK ||
        !auth_put_u8(
            buffer, buffer_size, &offset, communication_configuration) ||
        !auth_put(
            buffer, buffer_size, &offset, algorithm_indicator,
            LINK_UDS_AUTH_ALGORITHM_INDICATOR_BYTES)) {
        return auth_fail(
            buffer, buffer_size, written,
            result == LINK_UDS_RESULT_OK
                ? LINK_UDS_RESULT_BUFFER_TOO_SMALL : result);
    }
    *written = offset;
    return LINK_UDS_RESULT_OK;
}

LinkUdsResult link_uds_build_authentication_verify_proof_request(
    uint8_t task, bool suppress_positive_response,
    const uint8_t algorithm_indicator[LINK_UDS_AUTH_ALGORITHM_INDICATOR_BYTES],
    const uint8_t *proof_client, size_t proof_client_length,
    const uint8_t *challenge_client, size_t challenge_client_length,
    const uint8_t *additional_parameter, size_t additional_parameter_length,
    uint8_t *buffer, size_t buffer_size, size_t *written)
{
    size_t offset = 0U;
    LinkUdsResult result;
    const bool bidirectional =
        task == LINK_UDS_AUTH_VERIFY_PROOF_BIDIRECTIONAL;

    if (written == NULL || algorithm_indicator == NULL ||
        (task != LINK_UDS_AUTH_VERIFY_PROOF_UNIDIRECTIONAL &&
         !bidirectional) ||
        !auth_span_args_valid(proof_client, proof_client_length, true) ||
        !auth_span_args_valid(
            challenge_client, challenge_client_length, bidirectional) ||
        !auth_span_args_valid(
            additional_parameter, additional_parameter_length, false)) {
        return auth_fail(
            buffer, buffer_size, written, LINK_UDS_RESULT_INVALID_ARGUMENT);
    }
    result = auth_start(
        task, suppress_positive_response, buffer, buffer_size, &offset);
    if (result != LINK_UDS_RESULT_OK ||
        !auth_put(
            buffer, buffer_size, &offset, algorithm_indicator,
            LINK_UDS_AUTH_ALGORITHM_INDICATOR_BYTES) ||
        !auth_put_span16(
            buffer, buffer_size, &offset, proof_client, proof_client_length) ||
        !auth_put_span16(
            buffer, buffer_size, &offset,
            challenge_client, challenge_client_length) ||
        !auth_put_span16(
            buffer, buffer_size, &offset,
            additional_parameter, additional_parameter_length)) {
        return auth_fail(
            buffer, buffer_size, written,
            result == LINK_UDS_RESULT_OK
                ? LINK_UDS_RESULT_BUFFER_TOO_SMALL : result);
    }
    *written = offset;
    return LINK_UDS_RESULT_OK;
}

LinkUdsResult link_uds_build_authentication_configuration_request(
    bool suppress_positive_response,
    uint8_t *buffer, size_t buffer_size, size_t *written)
{
    size_t offset = 0U;
    LinkUdsResult result;
    if (written == NULL) return LINK_UDS_RESULT_INVALID_ARGUMENT;
    result = auth_start(
        LINK_UDS_AUTH_CONFIGURATION, suppress_positive_response,
        buffer, buffer_size, &offset);
    if (result != LINK_UDS_RESULT_OK)
        return auth_fail(buffer, buffer_size, written, result);
    *written = offset;
    return LINK_UDS_RESULT_OK;
}

static bool auth_take_fixed(
    const uint8_t *data, size_t length, size_t *offset,
    size_t field_length, LinkUdsAuthenticationSpan *span)
{
    if (data == NULL || offset == NULL || span == NULL ||
        *offset > length || field_length > length - *offset) {
        return false;
    }
    span->data = data + *offset;
    span->length = field_length;
    *offset += field_length;
    return true;
}

static bool auth_take_span16(
    const uint8_t *data, size_t length, size_t *offset,
    bool nonempty, LinkUdsAuthenticationSpan *span)
{
    uint16_t field_length;
    if (data == NULL || offset == NULL || span == NULL ||
        *offset > length || length - *offset < 2U) {
        return false;
    }
    field_length = (uint16_t)(
        ((uint16_t)data[*offset] << 8U) | data[*offset + 1U]);
    *offset += 2U;
    if ((nonempty && field_length == 0U) ||
        (size_t)field_length > length - *offset) {
        return false;
    }
    span->data = data + *offset;
    span->length = field_length;
    *offset += field_length;
    return true;
}

LinkUdsResult link_uds_decode_authentication_response(
    uint8_t expected_task, const uint8_t *pdu, size_t pdu_length,
    LinkUdsAuthenticationResponse *response)
{
    LinkUdsResponse generic;
    LinkUdsAuthenticationResponse decoded;
    LinkUdsResult result;
    const uint8_t *data;
    size_t length;
    size_t offset = 2U;

    if (response == NULL || expected_task > LINK_UDS_AUTH_CONFIGURATION)
        return LINK_UDS_RESULT_INVALID_ARGUMENT;
    result = link_uds_decode_response(
        LINK_UDS_SERVICE_AUTHENTICATION, pdu, pdu_length, &generic);
    if (result != LINK_UDS_RESULT_OK) return result;
    if (generic.data_length < 2U ||
        (generic.data[0] & UINT8_C(0x7f)) != expected_task) {
        return LINK_UDS_RESULT_UNEXPECTED_RESPONSE;
    }

    memset(&decoded, 0, sizeof(decoded));
    decoded.task = expected_task;
    decoded.return_parameter = generic.data[1];
    data = generic.data;
    length = generic.data_length;

    switch (expected_task) {
    case LINK_UDS_AUTH_DEAUTHENTICATE:
    case LINK_UDS_AUTH_TRANSMIT_CERTIFICATE:
    case LINK_UDS_AUTH_CONFIGURATION:
        break;
    case LINK_UDS_AUTH_VERIFY_CERTIFICATE_UNIDIRECTIONAL:
        if (!auth_take_span16(
                data, length, &offset, true, &decoded.challenge_server) ||
            !auth_take_span16(
                data, length, &offset, false,
                &decoded.ephemeral_public_key_server))
            return LINK_UDS_RESULT_MALFORMED_PDU;
        break;
    case LINK_UDS_AUTH_VERIFY_CERTIFICATE_BIDIRECTIONAL:
        if (!auth_take_span16(
                data, length, &offset, true, &decoded.challenge_server) ||
            !auth_take_span16(
                data, length, &offset, true, &decoded.certificate_server) ||
            !auth_take_span16(
                data, length, &offset, true,
                &decoded.proof_of_ownership_server) ||
            !auth_take_span16(
                data, length, &offset, false,
                &decoded.ephemeral_public_key_server))
            return LINK_UDS_RESULT_MALFORMED_PDU;
        break;
    case LINK_UDS_AUTH_PROOF_OF_OWNERSHIP:
        if (!auth_take_span16(
                data, length, &offset, false, &decoded.session_key_info))
            return LINK_UDS_RESULT_MALFORMED_PDU;
        break;
    case LINK_UDS_AUTH_REQUEST_CHALLENGE:
        if (!auth_take_fixed(
                data, length, &offset,
                LINK_UDS_AUTH_ALGORITHM_INDICATOR_BYTES,
                &decoded.algorithm_indicator) ||
            !auth_take_span16(
                data, length, &offset, true, &decoded.challenge_server) ||
            !auth_take_span16(
                data, length, &offset, false,
                &decoded.needed_additional_parameter))
            return LINK_UDS_RESULT_MALFORMED_PDU;
        break;
    case LINK_UDS_AUTH_VERIFY_PROOF_UNIDIRECTIONAL:
        if (!auth_take_fixed(
                data, length, &offset,
                LINK_UDS_AUTH_ALGORITHM_INDICATOR_BYTES,
                &decoded.algorithm_indicator) ||
            !auth_take_span16(
                data, length, &offset, false, &decoded.session_key_info))
            return LINK_UDS_RESULT_MALFORMED_PDU;
        break;
    case LINK_UDS_AUTH_VERIFY_PROOF_BIDIRECTIONAL:
        if (!auth_take_fixed(
                data, length, &offset,
                LINK_UDS_AUTH_ALGORITHM_INDICATOR_BYTES,
                &decoded.algorithm_indicator) ||
            !auth_take_span16(
                data, length, &offset, true,
                &decoded.proof_of_ownership_server) ||
            !auth_take_span16(
                data, length, &offset, false, &decoded.session_key_info))
            return LINK_UDS_RESULT_MALFORMED_PDU;
        break;
    default:
        return LINK_UDS_RESULT_UNSUPPORTED;
    }

    if (offset != length) return LINK_UDS_RESULT_MALFORMED_PDU;
    *response = decoded;
    return LINK_UDS_RESULT_OK;
}
