// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file ecu_probe.c
 * @brief Product-neutral read-only ECU/module identity probe.
 */
#include "link/ecu_probe.h"

#include "link/discover.h"
#include "link/uds.h"

#include <string.h>

#define LINK_ECU_PROBE_PDU_CAPACITY (LINK_ELM327_MAX_RESPONSE / 2U)

static LinkEcuProbeResult link_ecu_probe_fail(
    LinkEcuProbe *probe,
    LinkEcuProbeResult failure)
{
    if (probe != NULL) {
        probe->stage = LINK_ECU_PROBE_STAGE_FAILED;
        probe->failure = failure;
    }
    return failure;
}

static LinkEcuProbeResult link_ecu_probe_complete(LinkEcuProbe *probe)
{
    if (probe != NULL) {
        probe->stage = LINK_ECU_PROBE_STAGE_COMPLETE;
    }
    return LINK_ECU_PROBE_RESULT_COMPLETE;
}

static LinkEcuProbeResult link_ecu_probe_begin_reads(LinkEcuProbe *probe)
{
    if (probe == NULL) {
        return LINK_ECU_PROBE_RESULT_INVALID_ARGUMENT;
    }
    if (probe->profile.did_count != 0U) {
        probe->did_index = 0U;
        probe->stage = LINK_ECU_PROBE_STAGE_READ_DID;
        return LINK_ECU_PROBE_RESULT_OK;
    }
    if (probe->profile.read_dtcs) {
        probe->stage = LINK_ECU_PROBE_STAGE_READ_DTC_INFORMATION;
        return LINK_ECU_PROBE_RESULT_OK;
    }
    return link_ecu_probe_complete(probe);
}

static LinkEcuProbeResult link_ecu_probe_advance_did(LinkEcuProbe *probe)
{
    if (probe == NULL || probe->did_index >= probe->profile.did_count) {
        return LINK_ECU_PROBE_RESULT_FAILED_STATE;
    }
    probe->did_index++;
    if (probe->did_index < probe->profile.did_count) {
        return LINK_ECU_PROBE_RESULT_OK;
    }
    if (probe->profile.read_dtcs) {
        probe->stage = LINK_ECU_PROBE_STAGE_READ_DTC_INFORMATION;
        return LINK_ECU_PROBE_RESULT_OK;
    }
    return link_ecu_probe_complete(probe);
}

static LinkEcuProbeResult link_ecu_probe_build_checked_pdu(
    const uint8_t *pdu,
    size_t pdu_length,
    char *buffer,
    size_t buffer_size,
    size_t *written)
{
    link_safety_result safety;
    LinkElm327CanResult result;

    if (pdu == NULL || pdu_length == 0U || buffer == NULL ||
        buffer_size == 0U || written == NULL) {
        return LINK_ECU_PROBE_RESULT_INVALID_ARGUMENT;
    }

    safety = link_safety_classify(pdu, pdu_length);
    if (safety.decision != LINK_SAFETY_ALLOW_READ_ONLY) {
        return LINK_ECU_PROBE_RESULT_BLOCKED_BY_POLICY;
    }

    result = link_elm327_can_build_pdu_command(
        pdu, pdu_length, buffer, buffer_size, written);
    if (result == LINK_ELM327_CAN_RESULT_BUFFER_TOO_SMALL) {
        return LINK_ECU_PROBE_RESULT_BUFFER_TOO_SMALL;
    }
    if (result != LINK_ELM327_CAN_RESULT_OK) {
        return LINK_ECU_PROBE_RESULT_PDU_ERROR;
    }
    return LINK_ECU_PROBE_RESULT_OK;
}

static void link_ecu_probe_record_negative_response(
    uint8_t request_service,
    const uint8_t *pdu,
    size_t pdu_length,
    uint8_t *negative_response_code)
{
    LinkUdsResponse decoded;

    if (negative_response_code == NULL) {
        return;
    }
    *negative_response_code = 0U;
    if (link_uds_decode_response(
            request_service, pdu, pdu_length, &decoded) ==
        LINK_UDS_RESULT_NEGATIVE_RESPONSE) {
        *negative_response_code = decoded.negative_response_code;
    }
}

const char *link_ecu_probe_result_name(LinkEcuProbeResult result)
{
    switch (result) {
    case LINK_ECU_PROBE_RESULT_OK: return "ok";
    case LINK_ECU_PROBE_RESULT_COMPLETE: return "complete";
    case LINK_ECU_PROBE_RESULT_INVALID_ARGUMENT: return "invalid-argument";
    case LINK_ECU_PROBE_RESULT_BUFFER_TOO_SMALL: return "buffer-too-small";
    case LINK_ECU_PROBE_RESULT_CHANNEL_ERROR: return "channel-error";
    case LINK_ECU_PROBE_RESULT_PDU_ERROR: return "pdu-error";
    case LINK_ECU_PROBE_RESULT_UDS_ERROR: return "uds-error";
    case LINK_ECU_PROBE_RESULT_BLOCKED_BY_POLICY: return "blocked-by-policy";
    case LINK_ECU_PROBE_RESULT_FAILED_STATE: return "failed-state";
    }
    return "unknown";
}

const char *link_ecu_probe_stage_name(LinkEcuProbeStage stage)
{
    switch (stage) {
    case LINK_ECU_PROBE_STAGE_CONFIGURE_CHANNEL: return "configure-channel";
    case LINK_ECU_PROBE_STAGE_TESTER_PRESENT: return "tester-present";
    case LINK_ECU_PROBE_STAGE_READ_DID: return "read-did";
    case LINK_ECU_PROBE_STAGE_READ_DTC_INFORMATION: return "read-dtc-information";
    case LINK_ECU_PROBE_STAGE_COMPLETE: return "complete";
    case LINK_ECU_PROBE_STAGE_FAILED: return "failed";
    }
    return "unknown";
}

const char *link_ecu_probe_read_status_name(LinkEcuProbeReadStatus status)
{
    switch (status) {
    case LINK_ECU_PROBE_READ_NOT_ATTEMPTED: return "not-attempted";
    case LINK_ECU_PROBE_READ_AVAILABLE: return "available";
    case LINK_ECU_PROBE_READ_NO_RESPONSE: return "no-response";
    case LINK_ECU_PROBE_READ_NEGATIVE_RESPONSE: return "negative-response";
    case LINK_ECU_PROBE_READ_INVALID_RESPONSE: return "invalid-response";
    }
    return "unknown";
}

bool link_ecu_probe_profile_is_valid(const LinkEcuProbeProfile *profile)
{
    size_t index;

    if (profile == NULL ||
        !link_elm327_can_channel_config_is_valid(&profile->channel) ||
        profile->did_count > LINK_ECU_PROBE_MAX_DIDS ||
        (profile->did_count != 0U && profile->dids == NULL)) {
        return false;
    }
    for (index = 0U; index < profile->did_count; ++index) {
        if (profile->dids[index].key == NULL ||
            profile->dids[index].name == NULL) {
            return false;
        }
    }
    return profile->did_count != 0U || profile->read_dtcs ||
           profile->tester_present;
}

LinkEcuProbeResult link_ecu_probe_begin(
    LinkEcuProbe *probe,
    const LinkEcuProbeProfile *profile)
{
    LinkElm327CanResult result;
    size_t index;

    if (probe == NULL || !link_ecu_probe_profile_is_valid(profile)) {
        return LINK_ECU_PROBE_RESULT_INVALID_ARGUMENT;
    }

    memset(probe, 0, sizeof(*probe));
    probe->profile = *profile;
    probe->stage = LINK_ECU_PROBE_STAGE_CONFIGURE_CHANNEL;
    probe->failure = LINK_ECU_PROBE_RESULT_OK;
    probe->elm_can_failure = LINK_ELM327_CAN_RESULT_OK;
    probe->elm_failure = LINK_ELM327_RESULT_OK;
    probe->uds_failure = LINK_UDS_RESULT_OK;
    probe->dtc_status = LINK_ECU_PROBE_READ_NOT_ATTEMPTED;
    probe->dtc_elm_can_result = LINK_ELM327_CAN_RESULT_OK;
    probe->dtc_elm_result = LINK_ELM327_RESULT_OK;
    probe->dtc_uds_result = LINK_UDS_RESULT_OK;

    for (index = 0U; index < profile->did_count; ++index) {
        probe->did_results[index].request = &profile->dids[index];
        probe->did_results[index].status = LINK_ECU_PROBE_READ_NOT_ATTEMPTED;
        probe->did_results[index].elm_can_result = LINK_ELM327_CAN_RESULT_OK;
        probe->did_results[index].elm_result = LINK_ELM327_RESULT_OK;
        probe->did_results[index].uds_result = LINK_UDS_RESULT_OK;
    }

    result = link_elm327_can_channel_begin(&probe->channel, &profile->channel);
    if (result != LINK_ELM327_CAN_RESULT_OK) {
        probe->elm_can_failure = result;
        return link_ecu_probe_fail(probe, LINK_ECU_PROBE_RESULT_CHANNEL_ERROR);
    }
    return LINK_ECU_PROBE_RESULT_OK;
}

LinkEcuProbeResult link_ecu_probe_command(
    const LinkEcuProbe *probe,
    char *buffer,
    size_t buffer_size,
    size_t *written)
{
    uint8_t pdu[4];
    size_t pdu_length = 0U;
    LinkUdsResult uds_result;
    LinkElm327CanResult elm_result;

    if (written != NULL) {
        *written = 0U;
    }
    if (buffer != NULL && buffer_size != 0U) {
        buffer[0] = '\0';
    }
    if (probe == NULL || buffer == NULL || buffer_size == 0U ||
        written == NULL || !link_ecu_probe_profile_is_valid(&probe->profile)) {
        return LINK_ECU_PROBE_RESULT_INVALID_ARGUMENT;
    }

    switch (probe->stage) {
    case LINK_ECU_PROBE_STAGE_CONFIGURE_CHANNEL:
        elm_result = link_elm327_can_channel_command(
            &probe->channel, buffer, buffer_size);
        if (elm_result == LINK_ELM327_CAN_RESULT_BUFFER_TOO_SMALL) {
            return LINK_ECU_PROBE_RESULT_BUFFER_TOO_SMALL;
        }
        if (elm_result != LINK_ELM327_CAN_RESULT_OK) {
            return LINK_ECU_PROBE_RESULT_CHANNEL_ERROR;
        }
        *written = strlen(buffer);
        return LINK_ECU_PROBE_RESULT_OK;

    case LINK_ECU_PROBE_STAGE_TESTER_PRESENT:
        uds_result = link_uds_build_tester_present_request(
            false, pdu, sizeof(pdu), &pdu_length);
        if (uds_result != LINK_UDS_RESULT_OK) {
            return LINK_ECU_PROBE_RESULT_UDS_ERROR;
        }
        return link_ecu_probe_build_checked_pdu(
            pdu, pdu_length, buffer, buffer_size, written);

    case LINK_ECU_PROBE_STAGE_READ_DID:
        if (probe->did_index >= probe->profile.did_count) {
            return LINK_ECU_PROBE_RESULT_FAILED_STATE;
        }
        uds_result = link_uds_build_read_did_request(
            probe->profile.dids[probe->did_index].did,
            pdu, sizeof(pdu), &pdu_length);
        if (uds_result != LINK_UDS_RESULT_OK) {
            return LINK_ECU_PROBE_RESULT_UDS_ERROR;
        }
        return link_ecu_probe_build_checked_pdu(
            pdu, pdu_length, buffer, buffer_size, written);

    case LINK_ECU_PROBE_STAGE_READ_DTC_INFORMATION:
        uds_result = link_uds_build_report_dtcs_by_status_mask_request(
            LINK_UDS_DTC_STATUS_MASK_ALL,
            pdu, sizeof(pdu), &pdu_length);
        if (uds_result != LINK_UDS_RESULT_OK) {
            return LINK_ECU_PROBE_RESULT_UDS_ERROR;
        }
        return link_ecu_probe_build_checked_pdu(
            pdu, pdu_length, buffer, buffer_size, written);

    case LINK_ECU_PROBE_STAGE_COMPLETE:
    case LINK_ECU_PROBE_STAGE_FAILED:
        return LINK_ECU_PROBE_RESULT_FAILED_STATE;
    }

    return LINK_ECU_PROBE_RESULT_FAILED_STATE;
}

static LinkEcuProbeResult link_ecu_probe_accept_tester_present(
    LinkEcuProbe *probe,
    const LinkElm327Response *response)
{
    uint8_t pdu[LINK_ECU_PROBE_PDU_CAPACITY];
    size_t pdu_length = 0U;
    LinkElm327CanResult elm_result;
    LinkUdsResult uds_result;

    if (response->result != LINK_ELM327_RESULT_OK) {
        probe->elm_failure = response->result;
        return link_ecu_probe_fail(probe, LINK_ECU_PROBE_RESULT_UDS_ERROR);
    }
    elm_result = link_elm327_can_decode_pdu(
        response, pdu, sizeof(pdu), &pdu_length);
    if (elm_result != LINK_ELM327_CAN_RESULT_OK) {
        probe->elm_can_failure = elm_result;
        return link_ecu_probe_fail(probe, LINK_ECU_PROBE_RESULT_PDU_ERROR);
    }
    uds_result = link_uds_decode_tester_present_response(pdu, pdu_length);
    if (uds_result == LINK_UDS_RESULT_NEGATIVE_RESPONSE) {
        probe->uds_failure = uds_result;
        link_ecu_probe_record_negative_response(
            LINK_UDS_SERVICE_TESTER_PRESENT,
            pdu, pdu_length, &probe->uds_negative_response_code);
        return link_ecu_probe_begin_reads(probe);
    }
    if (uds_result != LINK_UDS_RESULT_OK) {
        probe->uds_failure = uds_result;
        return link_ecu_probe_fail(probe, LINK_ECU_PROBE_RESULT_UDS_ERROR);
    }
    return link_ecu_probe_begin_reads(probe);
}

static LinkEcuProbeResult link_ecu_probe_accept_did(
    LinkEcuProbe *probe,
    const LinkElm327Response *response)
{
    uint8_t pdu[LINK_ECU_PROBE_PDU_CAPACITY];
    size_t pdu_length = 0U;
    LinkElm327CanResult elm_result;
    LinkUdsDidRecord record;
    LinkUdsResult uds_result;
    LinkEcuProbeDidResult *result;
    const uint16_t did = probe->profile.dids[probe->did_index].did;

    result = &probe->did_results[probe->did_index];
    result->elm_result = response->result;

    if (response->result == LINK_ELM327_RESULT_NO_DATA) {
        result->status = LINK_ECU_PROBE_READ_NO_RESPONSE;
        return link_ecu_probe_advance_did(probe);
    }
    if (response->result != LINK_ELM327_RESULT_OK) {
        result->status = LINK_ECU_PROBE_READ_INVALID_RESPONSE;
        return link_ecu_probe_advance_did(probe);
    }

    elm_result = link_elm327_can_decode_pdu(
        response, pdu, sizeof(pdu), &pdu_length);
    result->elm_can_result = elm_result;
    if (elm_result != LINK_ELM327_CAN_RESULT_OK) {
        result->status = LINK_ECU_PROBE_READ_INVALID_RESPONSE;
        return link_ecu_probe_advance_did(probe);
    }

    uds_result = link_uds_decode_read_did_response(
        pdu, pdu_length, did, &record);
    result->uds_result = uds_result;
    if (uds_result == LINK_UDS_RESULT_NEGATIVE_RESPONSE) {
        result->status = LINK_ECU_PROBE_READ_NEGATIVE_RESPONSE;
        link_ecu_probe_record_negative_response(
            LINK_UDS_SERVICE_READ_DATA_BY_IDENTIFIER,
            pdu, pdu_length, &result->negative_response_code);
        return link_ecu_probe_advance_did(probe);
    }
    if (uds_result != LINK_UDS_RESULT_OK) {
        result->status = LINK_ECU_PROBE_READ_INVALID_RESPONSE;
        return link_ecu_probe_advance_did(probe);
    }

    result->status = LINK_ECU_PROBE_READ_AVAILABLE;
    result->data_length = record.data_length;
    if (result->data_length > sizeof(result->data)) {
        result->data_length = sizeof(result->data);
        result->truncated = true;
    }
    if (result->data_length != 0U) {
        memcpy(result->data, record.data, result->data_length);
    }
    return link_ecu_probe_advance_did(probe);
}

static LinkEcuProbeResult link_ecu_probe_accept_dtc(
    LinkEcuProbe *probe,
    const LinkElm327Response *response)
{
    uint8_t pdu[LINK_ECU_PROBE_PDU_CAPACITY];
    size_t pdu_length = 0U;
    LinkElm327CanResult elm_result;
    LinkUdsResult uds_result;

    probe->dtc_elm_result = response->result;
    if (response->result == LINK_ELM327_RESULT_NO_DATA) {
        probe->dtc_status = LINK_ECU_PROBE_READ_NO_RESPONSE;
        return link_ecu_probe_complete(probe);
    }
    if (response->result != LINK_ELM327_RESULT_OK) {
        probe->dtc_status = LINK_ECU_PROBE_READ_INVALID_RESPONSE;
        return link_ecu_probe_complete(probe);
    }

    elm_result = link_elm327_can_decode_pdu(
        response, pdu, sizeof(pdu), &pdu_length);
    probe->dtc_elm_can_result = elm_result;
    if (elm_result != LINK_ELM327_CAN_RESULT_OK) {
        probe->dtc_status = LINK_ECU_PROBE_READ_INVALID_RESPONSE;
        return link_ecu_probe_complete(probe);
    }

    uds_result = link_uds_decode_report_dtcs_by_status_mask_response(
        pdu, pdu_length, &probe->dtcs);
    probe->dtc_uds_result = uds_result;
    if (uds_result == LINK_UDS_RESULT_NEGATIVE_RESPONSE) {
        probe->dtc_status = LINK_ECU_PROBE_READ_NEGATIVE_RESPONSE;
        link_ecu_probe_record_negative_response(
            LINK_UDS_SERVICE_READ_DTC_INFORMATION,
            pdu, pdu_length, &probe->dtc_negative_response_code);
        return link_ecu_probe_complete(probe);
    }
    if (uds_result != LINK_UDS_RESULT_OK) {
        probe->dtc_status = LINK_ECU_PROBE_READ_INVALID_RESPONSE;
        return link_ecu_probe_complete(probe);
    }

    probe->dtc_status = LINK_ECU_PROBE_READ_AVAILABLE;
    return link_ecu_probe_complete(probe);
}

LinkEcuProbeResult link_ecu_probe_accept(
    LinkEcuProbe *probe,
    const LinkElm327Response *response)
{
    LinkElm327CanResult channel_result;

    if (probe == NULL || response == NULL ||
        !link_ecu_probe_profile_is_valid(&probe->profile)) {
        return LINK_ECU_PROBE_RESULT_INVALID_ARGUMENT;
    }

    switch (probe->stage) {
    case LINK_ECU_PROBE_STAGE_CONFIGURE_CHANNEL:
        channel_result = link_elm327_can_channel_accept(&probe->channel, response);
        if (channel_result != LINK_ELM327_CAN_RESULT_OK) {
            probe->elm_can_failure = channel_result;
            probe->elm_failure = probe->channel.elm_failure;
            return link_ecu_probe_fail(probe, LINK_ECU_PROBE_RESULT_CHANNEL_ERROR);
        }
        if (probe->channel.stage == LINK_ELM327_CAN_STAGE_COMPLETE) {
            if (probe->profile.tester_present) {
                probe->stage = LINK_ECU_PROBE_STAGE_TESTER_PRESENT;
                return LINK_ECU_PROBE_RESULT_OK;
            }
            return link_ecu_probe_begin_reads(probe);
        }
        return LINK_ECU_PROBE_RESULT_OK;

    case LINK_ECU_PROBE_STAGE_TESTER_PRESENT:
        return link_ecu_probe_accept_tester_present(probe, response);

    case LINK_ECU_PROBE_STAGE_READ_DID:
        if (probe->did_index >= probe->profile.did_count) {
            return link_ecu_probe_fail(probe, LINK_ECU_PROBE_RESULT_FAILED_STATE);
        }
        return link_ecu_probe_accept_did(probe, response);

    case LINK_ECU_PROBE_STAGE_READ_DTC_INFORMATION:
        return link_ecu_probe_accept_dtc(probe, response);

    case LINK_ECU_PROBE_STAGE_COMPLETE:
    case LINK_ECU_PROBE_STAGE_FAILED:
        return LINK_ECU_PROBE_RESULT_FAILED_STATE;
    }

    return LINK_ECU_PROBE_RESULT_FAILED_STATE;
}

size_t link_ecu_probe_did_result_count(const LinkEcuProbe *probe)
{
    return probe == NULL ? 0U : probe->profile.did_count;
}

const LinkEcuProbeDidResult *link_ecu_probe_did_result_at(
    const LinkEcuProbe *probe,
    size_t index)
{
    if (probe == NULL || index >= probe->profile.did_count) {
        return NULL;
    }
    return &probe->did_results[index];
}
