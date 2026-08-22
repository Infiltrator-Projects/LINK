// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file uds.c
 * @brief Portable ISO 14229 UDS request/response and client-state engine.
 */
#include "link/uds.h"

#include "infiltratr/core.h"

#include <string.h>

static bool uds_request_service_valid(uint8_t service)
{
    return service != 0U && service != LINK_UDS_SERVICE_NEGATIVE_RESPONSE &&
           service <= 0xbfU;
}

static bool uds_session_type_valid(uint8_t session_type)
{
    return session_type >= 0x01U && session_type <= 0x7fU;
}

static void uds_clear_request(LinkUdsClient *client)
{
    client->deadline_us = 0U;
    client->request_service = 0U;
    client->request_subfunction = 0U;
    client->request_did = 0U;
    client->request_has_subfunction = false;
    client->request_has_did = false;
}

static LinkUdsResult uds_client_fail(LinkUdsClient *client, LinkUdsResult failure)
{
    if (client != NULL) {
        client->state = LINK_UDS_CLIENT_FAILED;
        client->failure = failure;
        client->deadline_us = 0U;
    }
    return failure;
}

static LinkUdsResult uds_write_failure(
    uint8_t *buffer, size_t buffer_size, size_t *written, LinkUdsResult result)
{
    if (written != NULL) *written = 0U;
    if (buffer != NULL && buffer_size != 0U) buffer[0] = 0U;
    return result;
}

const char *link_uds_result_name(LinkUdsResult result)
{
    switch (result) {
    case LINK_UDS_RESULT_OK: return "ok";
    case LINK_UDS_RESULT_COMPLETE: return "complete";
    case LINK_UDS_RESULT_WAITING: return "waiting";
    case LINK_UDS_RESULT_RESPONSE_PENDING: return "response-pending";
    case LINK_UDS_RESULT_NEGATIVE_RESPONSE: return "negative-response";
    case LINK_UDS_RESULT_INVALID_ARGUMENT: return "invalid-argument";
    case LINK_UDS_RESULT_BUFFER_TOO_SMALL: return "buffer-too-small";
    case LINK_UDS_RESULT_MALFORMED_PDU: return "malformed-pdu";
    case LINK_UDS_RESULT_UNEXPECTED_RESPONSE: return "unexpected-response";
    case LINK_UDS_RESULT_BUSY: return "busy";
    case LINK_UDS_RESULT_TIMEOUT: return "timeout";
    case LINK_UDS_RESULT_FAILED_STATE: return "failed-state";
    case LINK_UDS_RESULT_UNSUPPORTED: return "unsupported";
    }
    return "unknown";
}

const char *link_uds_client_state_name(LinkUdsClientState state)
{
    switch (state) {
    case LINK_UDS_CLIENT_IDLE: return "idle";
    case LINK_UDS_CLIENT_WAITING_RESPONSE: return "waiting-response";
    case LINK_UDS_CLIENT_RESPONSE_PENDING: return "response-pending";
    case LINK_UDS_CLIENT_COMPLETE: return "complete";
    case LINK_UDS_CLIENT_FAILED: return "failed";
    }
    return "unknown";
}

const char *link_uds_negative_response_code_name(uint8_t code)
{
    switch (code) {
    case 0x10U: return "general-reject";
    case 0x11U: return "service-not-supported";
    case 0x12U: return "subfunction-not-supported";
    case 0x13U: return "incorrect-message-length-or-invalid-format";
    case 0x22U: return "conditions-not-correct";
    case 0x24U: return "request-sequence-error";
    case 0x31U: return "request-out-of-range";
    case 0x33U: return "security-access-denied";
    case 0x35U: return "invalid-key";
    case 0x36U: return "exceed-number-of-attempts";
    case 0x37U: return "required-time-delay-not-expired";
    case 0x70U: return "upload-download-not-accepted";
    case 0x71U: return "transfer-data-suspended";
    case 0x72U: return "general-programming-failure";
    case 0x73U: return "wrong-block-sequence-counter";
    case LINK_UDS_NRC_RESPONSE_PENDING: return "response-pending";
    case 0x7eU: return "subfunction-not-supported-in-active-session";
    case 0x7fU: return "service-not-supported-in-active-session";
    default: return "unknown-nrc";
    }
}

LinkUdsResult link_uds_decode_response(
    uint8_t request_service, const uint8_t *pdu, size_t pdu_length,
    LinkUdsResponse *response)
{
    LinkUdsResponse decoded;
    uint8_t positive_service;

    if (!uds_request_service_valid(request_service) || pdu == NULL || response == NULL) {
        return LINK_UDS_RESULT_INVALID_ARGUMENT;
    }
    if (pdu_length == 0U) return LINK_UDS_RESULT_MALFORMED_PDU;

    memset(&decoded, 0, sizeof(decoded));
    decoded.request_service = request_service;
    positive_service = (uint8_t)(request_service + 0x40U);

    if (pdu[0] == LINK_UDS_SERVICE_NEGATIVE_RESPONSE) {
        if (pdu_length != 3U || pdu[2] == 0U) return LINK_UDS_RESULT_MALFORMED_PDU;
        if (pdu[1] != request_service) return LINK_UDS_RESULT_UNEXPECTED_RESPONSE;
        decoded.kind = LINK_UDS_RESPONSE_NEGATIVE;
        decoded.response_service = pdu[0];
        decoded.negative_response_code = pdu[2];
        *response = decoded;
        return LINK_UDS_RESULT_NEGATIVE_RESPONSE;
    }
    if (pdu[0] != positive_service) return LINK_UDS_RESULT_UNEXPECTED_RESPONSE;

    decoded.kind = LINK_UDS_RESPONSE_POSITIVE;
    decoded.response_service = pdu[0];
    decoded.data = pdu + 1U;
    decoded.data_length = pdu_length - 1U;
    *response = decoded;
    return LINK_UDS_RESULT_OK;
}

LinkUdsResult link_uds_build_session_control_request(
    uint8_t session_type, bool suppress_positive_response,
    uint8_t *buffer, size_t buffer_size, size_t *written)
{
    if (!uds_session_type_valid(session_type) || buffer == NULL || written == NULL) {
        return uds_write_failure(buffer, buffer_size, written,
                                 LINK_UDS_RESULT_INVALID_ARGUMENT);
    }
    if (buffer_size < 2U) {
        return uds_write_failure(buffer, buffer_size, written,
                                 LINK_UDS_RESULT_BUFFER_TOO_SMALL);
    }
    buffer[0] = LINK_UDS_SERVICE_DIAGNOSTIC_SESSION_CONTROL;
    buffer[1] = session_type;
    if (suppress_positive_response) buffer[1] |= 0x80U;
    *written = 2U;
    return LINK_UDS_RESULT_OK;
}

LinkUdsResult link_uds_decode_session_control_response(
    const uint8_t *pdu, size_t pdu_length, uint8_t expected_session_type,
    LinkUdsSessionResponse *response)
{
    LinkUdsResponse generic;
    LinkUdsSessionResponse decoded;
    LinkUdsResult result;

    if (!uds_session_type_valid(expected_session_type) || response == NULL) {
        return LINK_UDS_RESULT_INVALID_ARGUMENT;
    }
    result = link_uds_decode_response(LINK_UDS_SERVICE_DIAGNOSTIC_SESSION_CONTROL,
                                      pdu, pdu_length, &generic);
    if (result != LINK_UDS_RESULT_OK) return result;
    if (generic.data_length != 1U && generic.data_length != 5U) {
        return LINK_UDS_RESULT_MALFORMED_PDU;
    }
    if (generic.data[0] != expected_session_type) return LINK_UDS_RESULT_UNEXPECTED_RESPONSE;

    memset(&decoded, 0, sizeof(decoded));
    decoded.session_type = generic.data[0];
    if (generic.data_length == 5U) {
        decoded.timing_present = true;
        decoded.p2_server_max_ms =
            (uint16_t)(((uint16_t)generic.data[1] << 8U) | generic.data[2]);
        decoded.p2_star_server_max_10ms =
            (uint16_t)(((uint16_t)generic.data[3] << 8U) | generic.data[4]);
        if (decoded.p2_server_max_ms == 0U || decoded.p2_star_server_max_10ms == 0U) {
            return LINK_UDS_RESULT_MALFORMED_PDU;
        }
    }
    *response = decoded;
    return LINK_UDS_RESULT_OK;
}

LinkUdsResult link_uds_build_tester_present_request(
    bool suppress_positive_response, uint8_t *buffer,
    size_t buffer_size, size_t *written)
{
    if (buffer == NULL || written == NULL) {
        return uds_write_failure(buffer, buffer_size, written,
                                 LINK_UDS_RESULT_INVALID_ARGUMENT);
    }
    if (buffer_size < 2U) {
        return uds_write_failure(buffer, buffer_size, written,
                                 LINK_UDS_RESULT_BUFFER_TOO_SMALL);
    }
    buffer[0] = LINK_UDS_SERVICE_TESTER_PRESENT;
    buffer[1] = suppress_positive_response ? 0x80U : 0x00U;
    *written = 2U;
    return LINK_UDS_RESULT_OK;
}

LinkUdsResult link_uds_decode_tester_present_response(
    const uint8_t *pdu, size_t pdu_length)
{
    LinkUdsResponse generic;
    LinkUdsResult result = link_uds_decode_response(
        LINK_UDS_SERVICE_TESTER_PRESENT, pdu, pdu_length, &generic);
    if (result != LINK_UDS_RESULT_OK) return result;
    if (generic.data_length != 1U) return LINK_UDS_RESULT_MALFORMED_PDU;
    if (generic.data[0] != 0x00U) return LINK_UDS_RESULT_UNEXPECTED_RESPONSE;
    return LINK_UDS_RESULT_OK;
}

LinkUdsResult link_uds_build_read_did_request(
    uint16_t identifier, uint8_t *buffer, size_t buffer_size, size_t *written)
{
    if (buffer == NULL || written == NULL) {
        return uds_write_failure(buffer, buffer_size, written,
                                 LINK_UDS_RESULT_INVALID_ARGUMENT);
    }
    if (buffer_size < 3U) {
        return uds_write_failure(buffer, buffer_size, written,
                                 LINK_UDS_RESULT_BUFFER_TOO_SMALL);
    }
    buffer[0] = LINK_UDS_SERVICE_READ_DATA_BY_IDENTIFIER;
    buffer[1] = (uint8_t)(identifier >> 8U);
    buffer[2] = (uint8_t)identifier;
    *written = 3U;
    return LINK_UDS_RESULT_OK;
}

LinkUdsResult link_uds_decode_read_did_response(
    const uint8_t *pdu, size_t pdu_length, uint16_t expected_identifier,
    LinkUdsDidRecord *record)
{
    LinkUdsResponse generic;
    LinkUdsDidRecord decoded;
    LinkUdsResult result;
    uint16_t identifier;

    if (record == NULL) return LINK_UDS_RESULT_INVALID_ARGUMENT;
    result = link_uds_decode_response(LINK_UDS_SERVICE_READ_DATA_BY_IDENTIFIER,
                                      pdu, pdu_length, &generic);
    if (result != LINK_UDS_RESULT_OK) return result;
    if (generic.data_length < 2U) return LINK_UDS_RESULT_MALFORMED_PDU;

    identifier = (uint16_t)(((uint16_t)generic.data[0] << 8U) | generic.data[1]);
    if (identifier != expected_identifier) return LINK_UDS_RESULT_UNEXPECTED_RESPONSE;
    decoded.identifier = identifier;
    decoded.data = generic.data + 2U;
    decoded.data_length = generic.data_length - 2U;
    *record = decoded;
    return LINK_UDS_RESULT_OK;
}

bool link_uds_did_definition_is_valid(const LinkUdsDidDefinition *definition)
{
    return definition != NULL && definition->key != NULL &&
           definition->key[0] != '\0' && definition->name != NULL &&
           definition->name[0] != '\0' &&
           definition->minimum_length <= definition->maximum_length;
}

LinkUdsResult link_uds_build_defined_did_request(
    const LinkUdsDidDefinition *definition,
    uint8_t *buffer, size_t buffer_size, size_t *written)
{
    if (!link_uds_did_definition_is_valid(definition)) {
        return uds_write_failure(buffer, buffer_size, written,
                                 LINK_UDS_RESULT_INVALID_ARGUMENT);
    }
    return link_uds_build_read_did_request(definition->identifier,
                                            buffer, buffer_size, written);
}

LinkUdsResult link_uds_decode_defined_did_response(
    const uint8_t *pdu, size_t pdu_length,
    const LinkUdsDidDefinition *definition, LinkUdsDidValue *value)
{
    LinkUdsDidRecord record;
    LinkUdsDidValue decoded;
    LinkUdsResult result;

    if (!link_uds_did_definition_is_valid(definition) || value == NULL) {
        return LINK_UDS_RESULT_INVALID_ARGUMENT;
    }
    result = link_uds_decode_read_did_response(pdu, pdu_length,
                                               definition->identifier, &record);
    if (result != LINK_UDS_RESULT_OK) return result;
    if (record.data_length < definition->minimum_length ||
        record.data_length > definition->maximum_length) {
        return LINK_UDS_RESULT_UNEXPECTED_RESPONSE;
    }
    decoded.definition = definition;
    decoded.data = record.data;
    decoded.data_length = record.data_length;
    *value = decoded;
    return LINK_UDS_RESULT_OK;
}

LinkUdsResult link_uds_client_init(
    LinkUdsClient *client, const LinkUdsClientConfig *config)
{
    if (client == NULL || config == NULL || config->p2_timeout_us == 0U ||
        config->p2_star_timeout_us == 0U) {
        return LINK_UDS_RESULT_INVALID_ARGUMENT;
    }
    memset(client, 0, sizeof(*client));
    client->initial_config = *config;
    client->p2_timeout_us = config->p2_timeout_us;
    client->p2_star_timeout_us = config->p2_star_timeout_us;
    client->active_session = LINK_UDS_SESSION_DEFAULT;
    client->state = LINK_UDS_CLIENT_IDLE;
    client->failure = LINK_UDS_RESULT_OK;
    return LINK_UDS_RESULT_OK;
}

void link_uds_client_reset(LinkUdsClient *client)
{
    if (client == NULL) return;
    client->p2_timeout_us = client->initial_config.p2_timeout_us;
    client->p2_star_timeout_us = client->initial_config.p2_star_timeout_us;
    client->active_session = LINK_UDS_SESSION_DEFAULT;
    client->state = LINK_UDS_CLIENT_IDLE;
    client->failure = LINK_UDS_RESULT_OK;
    uds_clear_request(client);
}

LinkUdsResult link_uds_client_begin(
    LinkUdsClient *client, const uint8_t *request_pdu,
    size_t request_length, uint64_t now_us)
{
    uint8_t service;

    if (client == NULL || request_pdu == NULL || request_length == 0U) {
        return LINK_UDS_RESULT_INVALID_ARGUMENT;
    }
    if (client->state == LINK_UDS_CLIENT_FAILED) return LINK_UDS_RESULT_FAILED_STATE;
    if (client->state == LINK_UDS_CLIENT_WAITING_RESPONSE ||
        client->state == LINK_UDS_CLIENT_RESPONSE_PENDING) return LINK_UDS_RESULT_BUSY;

    service = request_pdu[0];
    if (!uds_request_service_valid(service)) return LINK_UDS_RESULT_INVALID_ARGUMENT;
    uds_clear_request(client);
    client->request_service = service;

    if (service == LINK_UDS_SERVICE_DIAGNOSTIC_SESSION_CONTROL) {
        uint8_t session_type;
        if (request_length != 2U) {
            uds_clear_request(client);
            return LINK_UDS_RESULT_MALFORMED_PDU;
        }
        if ((request_pdu[1] & 0x80U) != 0U) {
            uds_clear_request(client);
            return LINK_UDS_RESULT_UNSUPPORTED;
        }
        session_type = request_pdu[1] & 0x7fU;
        if (!uds_session_type_valid(session_type)) {
            uds_clear_request(client);
            return LINK_UDS_RESULT_MALFORMED_PDU;
        }
        client->request_has_subfunction = true;
        client->request_subfunction = session_type;
    } else if (service == LINK_UDS_SERVICE_TESTER_PRESENT) {
        if (request_length != 2U) {
            uds_clear_request(client);
            return LINK_UDS_RESULT_MALFORMED_PDU;
        }
        if ((request_pdu[1] & 0x80U) != 0U) {
            uds_clear_request(client);
            return LINK_UDS_RESULT_UNSUPPORTED;
        }
        if (request_pdu[1] != 0x00U) {
            uds_clear_request(client);
            return LINK_UDS_RESULT_MALFORMED_PDU;
        }
        client->request_has_subfunction = true;
        client->request_subfunction = 0x00U;
    } else if (service == LINK_UDS_SERVICE_READ_DATA_BY_IDENTIFIER) {
        if (request_length != 3U) {
            uds_clear_request(client);
            return LINK_UDS_RESULT_UNSUPPORTED;
        }
        client->request_has_did = true;
        client->request_did =
            (uint16_t)(((uint16_t)request_pdu[1] << 8U) | request_pdu[2]);
    }

    client->deadline_us = infiltratr_u64_add_saturating(now_us, client->p2_timeout_us);
    client->state = LINK_UDS_CLIENT_WAITING_RESPONSE;
    client->failure = LINK_UDS_RESULT_OK;
    return LINK_UDS_RESULT_OK;
}

LinkUdsResult link_uds_client_accept(
    LinkUdsClient *client, const uint8_t *response_pdu,
    size_t response_length, uint64_t now_us, LinkUdsResponse *response)
{
    LinkUdsResponse decoded;
    LinkUdsResult result;

    if (client == NULL || response_pdu == NULL || response == NULL) {
        return LINK_UDS_RESULT_INVALID_ARGUMENT;
    }
    if (client->state == LINK_UDS_CLIENT_FAILED) return LINK_UDS_RESULT_FAILED_STATE;
    if (client->state != LINK_UDS_CLIENT_WAITING_RESPONSE &&
        client->state != LINK_UDS_CLIENT_RESPONSE_PENDING) {
        return LINK_UDS_RESULT_UNEXPECTED_RESPONSE;
    }
    if (now_us >= client->deadline_us) return uds_client_fail(client, LINK_UDS_RESULT_TIMEOUT);

    result = link_uds_decode_response(client->request_service,
                                      response_pdu, response_length, &decoded);
    if (result == LINK_UDS_RESULT_NEGATIVE_RESPONSE) {
        *response = decoded;
        if (decoded.negative_response_code == LINK_UDS_NRC_RESPONSE_PENDING) {
            client->deadline_us = infiltratr_u64_add_saturating(
                now_us, client->p2_star_timeout_us);
            client->state = LINK_UDS_CLIENT_RESPONSE_PENDING;
            return LINK_UDS_RESULT_RESPONSE_PENDING;
        }
        client->state = LINK_UDS_CLIENT_COMPLETE;
        client->deadline_us = 0U;
        return LINK_UDS_RESULT_NEGATIVE_RESPONSE;
    }
    if (result != LINK_UDS_RESULT_OK) return uds_client_fail(client, result);

    if (client->request_has_subfunction &&
        client->request_service == LINK_UDS_SERVICE_DIAGNOSTIC_SESSION_CONTROL) {
        LinkUdsSessionResponse session;
        result = link_uds_decode_session_control_response(
            response_pdu, response_length, client->request_subfunction, &session);
        if (result != LINK_UDS_RESULT_OK) return uds_client_fail(client, result);
        client->active_session = session.session_type;
        if (session.timing_present) {
            client->p2_timeout_us = infiltratr_u64_multiply_saturating(
                (uint64_t)session.p2_server_max_ms, UINT64_C(1000));
            client->p2_star_timeout_us = infiltratr_u64_multiply_saturating(
                (uint64_t)session.p2_star_server_max_10ms, UINT64_C(10000));
        }
    } else if (client->request_has_subfunction &&
               client->request_service == LINK_UDS_SERVICE_TESTER_PRESENT) {
        result = link_uds_decode_tester_present_response(response_pdu, response_length);
        if (result != LINK_UDS_RESULT_OK) return uds_client_fail(client, result);
    } else if (client->request_has_did &&
               client->request_service == LINK_UDS_SERVICE_READ_DATA_BY_IDENTIFIER) {
        LinkUdsDidRecord record;
        result = link_uds_decode_read_did_response(response_pdu, response_length,
                                                   client->request_did, &record);
        if (result != LINK_UDS_RESULT_OK) return uds_client_fail(client, result);
    }

    *response = decoded;
    client->state = LINK_UDS_CLIENT_COMPLETE;
    client->deadline_us = 0U;
    return LINK_UDS_RESULT_COMPLETE;
}

LinkUdsResult link_uds_client_tick(LinkUdsClient *client, uint64_t now_us)
{
    if (client == NULL) return LINK_UDS_RESULT_INVALID_ARGUMENT;
    if (client->state == LINK_UDS_CLIENT_FAILED) return client->failure;
    if (client->state == LINK_UDS_CLIENT_IDLE) return LINK_UDS_RESULT_OK;
    if (client->state == LINK_UDS_CLIENT_COMPLETE) return LINK_UDS_RESULT_COMPLETE;
    if (now_us >= client->deadline_us) return uds_client_fail(client, LINK_UDS_RESULT_TIMEOUT);
    return LINK_UDS_RESULT_WAITING;
}
