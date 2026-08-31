// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/diagnostic_flow.h"

#include "infiltratr/core.h"

#include <string.h>

static void flow_clear_action(LinkDiagnosticFlowAction *action)
{
    if (action != NULL) {
        memset(action, 0, sizeof(*action));
        action->kind = LINK_DIAGNOSTIC_FLOW_ACTION_NONE;
        action->dtc_kind = LINK_OBD2_DTC_STORED;
    }
}

static void flow_clear_event(LinkDiagnosticFlowEvent *event)
{
    if (event != NULL) {
        memset(event, 0, sizeof(*event));
        event->kind = LINK_DIAGNOSTIC_FLOW_EVENT_NONE;
        event->dtc_kind = LINK_OBD2_DTC_STORED;
    }
}

static LinkDiagnosticFlowResult flow_fail_elm(
    LinkDiagnosticFlow *flow,
    LinkElm327Result failure)
{
    flow->elm_failure = failure;
    flow->failure = LINK_DIAGNOSTIC_FLOW_RESULT_ELM_ERROR;
    flow->stage = LINK_DIAGNOSTIC_FLOW_FAILED;
    flow->awaiting_response = false;
    return flow->failure;
}

static LinkDiagnosticFlowResult flow_fail_obd2(
    LinkDiagnosticFlow *flow,
    LinkObd2Result failure)
{
    flow->obd2_failure = failure;
    flow->failure = LINK_DIAGNOSTIC_FLOW_RESULT_OBD2_ERROR;
    flow->stage = LINK_DIAGNOSTIC_FLOW_FAILED;
    flow->awaiting_response = false;
    return flow->failure;
}

static LinkDiagnosticFlowResult flow_fail_scheduler(
    LinkDiagnosticFlow *flow,
    LinkSchedulerResult failure)
{
    flow->scheduler_failure = failure;
    flow->failure = LINK_DIAGNOSTIC_FLOW_RESULT_SCHEDULER_ERROR;
    flow->stage = LINK_DIAGNOSTIC_FLOW_FAILED;
    flow->awaiting_response = false;
    return flow->failure;
}

static LinkObd2DtcKind flow_stage_dtc_kind(LinkDiagnosticFlowStage stage)
{
    switch (stage) {
    case LINK_DIAGNOSTIC_FLOW_SCANNING_PENDING_DTCS:
        return LINK_OBD2_DTC_PENDING;
    case LINK_DIAGNOSTIC_FLOW_SCANNING_PERMANENT_DTCS:
        return LINK_OBD2_DTC_PERMANENT;
    case LINK_DIAGNOSTIC_FLOW_SCANNING_STORED_DTCS:
    default:
        return LINK_OBD2_DTC_STORED;
    }
}

static uint8_t flow_dtc_request_service(LinkObd2DtcKind kind)
{
    switch (kind) {
    case LINK_OBD2_DTC_STORED: return UINT8_C(0x03);
    case LINK_OBD2_DTC_PENDING: return UINT8_C(0x07);
    case LINK_OBD2_DTC_PERMANENT: return UINT8_C(0x0a);
    }
    return 0U;
}

static LinkObd2DtcList *flow_dtc_list(
    LinkDiagnosticFlow *flow,
    LinkObd2DtcKind kind)
{
    if (flow == NULL) {
        return NULL;
    }
    switch (kind) {
    case LINK_OBD2_DTC_STORED:
        return &flow->stored_dtcs;
    case LINK_OBD2_DTC_PENDING:
        return &flow->pending_dtcs;
    case LINK_OBD2_DTC_PERMANENT:
        return &flow->permanent_dtcs;
    }
    return NULL;
}

static LinkDiagnosticFlowStage flow_next_dtc_stage(LinkObd2DtcKind kind)
{
    switch (kind) {
    case LINK_OBD2_DTC_STORED:
        return LINK_DIAGNOSTIC_FLOW_SCANNING_PENDING_DTCS;
    case LINK_OBD2_DTC_PENDING:
        return LINK_DIAGNOSTIC_FLOW_SCANNING_PERMANENT_DTCS;
    case LINK_OBD2_DTC_PERMANENT:
        return LINK_DIAGNOSTIC_FLOW_READING_READINESS;
    }
    return LINK_DIAGNOSTIC_FLOW_FAILED;
}

static const uint8_t flow_freeze_candidates[] = {
    UINT8_C(0x04), /* calculated load */
    UINT8_C(0x05), /* coolant temperature */
    UINT8_C(0x0b), /* manifold pressure */
    UINT8_C(0x0c), /* engine RPM */
    UINT8_C(0x0d), /* vehicle speed */
    UINT8_C(0x0f), /* intake-air temperature */
    UINT8_C(0x10), /* mass-air flow */
    UINT8_C(0x11)  /* absolute throttle position */
};

static bool flow_next_freeze_candidate(
    LinkDiagnosticFlow *flow, uint8_t *pid)
{
    const size_t count =
        sizeof(flow_freeze_candidates) / sizeof(flow_freeze_candidates[0]);

    if (flow == NULL || pid == NULL) return false;
    while (flow->freeze_frame_candidate_index < count) {
        const uint8_t candidate =
            flow_freeze_candidates[flow->freeze_frame_candidate_index++];
        if (link_obd2_pid_set_contains(&flow->supported_pids, candidate)) {
            *pid = candidate;
            return true;
        }
    }
    return false;
}

static bool flow_has_freeze_candidate(const LinkDiagnosticFlow *flow)
{
    const size_t count =
        sizeof(flow_freeze_candidates) / sizeof(flow_freeze_candidates[0]);
    size_t index;

    if (flow == NULL) return false;
    for (index = flow->freeze_frame_candidate_index; index < count; ++index) {
        if (link_obd2_pid_set_contains(
                &flow->supported_pids, flow_freeze_candidates[index])) {
            return true;
        }
    }
    return false;
}

static LinkDiagnosticFlowResult flow_emit_command(
    LinkDiagnosticFlow *flow,
    LinkDiagnosticFlowAction *action,
    const char *command,
    uint64_t timeout_ms)
{
    if (flow == NULL || action == NULL || command == NULL || command[0] == '\0') {
        return LINK_DIAGNOSTIC_FLOW_RESULT_INVALID_ARGUMENT;
    }
    if (strlen(command) >= sizeof(action->command)) {
        return flow_fail_elm(flow, LINK_ELM327_RESULT_COMMAND_TOO_LONG);
    }
    infiltratr_copy_string(action->command, sizeof(action->command), command);
    action->kind = LINK_DIAGNOSTIC_FLOW_ACTION_SEND_COMMAND;
    action->timeout_ms = timeout_ms;
    flow->awaiting_response = true;
    return LINK_DIAGNOSTIC_FLOW_RESULT_OK;
}

static LinkDiagnosticFlowResult flow_fail_invalid_state(LinkDiagnosticFlow *flow)
{
    flow->failure = LINK_DIAGNOSTIC_FLOW_RESULT_INVALID_STATE;
    flow->stage = LINK_DIAGNOSTIC_FLOW_FAILED;
    flow->awaiting_response = false;
    return flow->failure;
}

static LinkDiagnosticFlowResult flow_next_initialization_action(
    LinkDiagnosticFlow *flow,
    LinkDiagnosticFlowAction *action)
{
    const char *command = link_elm327_init_command(&flow->initialization);

    if (command == NULL) {
        action->kind = LINK_DIAGNOSTIC_FLOW_ACTION_FAILED;
        return flow_fail_invalid_state(flow);
    }
    return flow_emit_command(flow, action, command, flow->config.init_timeout_ms);
}

static LinkDiagnosticFlowResult flow_next_pid_discovery_action(
    LinkDiagnosticFlow *flow,
    LinkDiagnosticFlowAction *action)
{
    char command[LINK_ELM327_MAX_COMMAND];
    const LinkObd2Result result = link_obd2_build_supported_pid_request(
        flow->supported_pid_base, command, sizeof(command));

    if (result != LINK_OBD2_RESULT_OK) {
        return flow_fail_obd2(flow, result);
    }
    return flow_emit_command(flow, action, command, flow->config.query_timeout_ms);
}

static LinkDiagnosticFlowResult flow_next_vin_action(
    LinkDiagnosticFlow *flow,
    LinkDiagnosticFlowAction *action)
{
    char command[LINK_ELM327_MAX_COMMAND];
    const LinkObd2Result result =
        link_obd2_build_vin_request(command, sizeof(command));

    if (result != LINK_OBD2_RESULT_OK) {
        return flow_fail_obd2(flow, result);
    }
    return flow_emit_command(flow, action, command, flow->config.query_timeout_ms);
}

static LinkDiagnosticFlowResult flow_next_dtc_action(
    LinkDiagnosticFlow *flow,
    LinkDiagnosticFlowAction *action)
{
    char command[LINK_ELM327_MAX_COMMAND];
    const LinkObd2DtcKind kind = flow_stage_dtc_kind(flow->stage);
    const LinkObd2Result result =
        link_obd2_build_dtc_request(kind, command, sizeof(command));

    if (result != LINK_OBD2_RESULT_OK) {
        return flow_fail_obd2(flow, result);
    }
    action->dtc_kind = kind;
    return flow_emit_command(flow, action, command, flow->config.query_timeout_ms);
}

static LinkDiagnosticFlowResult flow_next_readiness_action(
    LinkDiagnosticFlow *flow,
    LinkDiagnosticFlowAction *action)
{
    char command[LINK_ELM327_MAX_COMMAND];
    const LinkObd2Result result =
        link_obd2_build_live_pid_request(
            UINT8_C(0x01), command, sizeof(command));
    if (result != LINK_OBD2_RESULT_OK)
        return flow_fail_obd2(flow, result);
    return flow_emit_command(
        flow, action, command, flow->config.query_timeout_ms);
}

static LinkDiagnosticFlowResult flow_next_freeze_action(
    LinkDiagnosticFlow *flow,
    LinkDiagnosticFlowAction *action)
{
    char command[LINK_ELM327_MAX_COMMAND];
    uint8_t pid;
    LinkObd2Result result;

    if (flow == NULL || action == NULL)
        return LINK_DIAGNOSTIC_FLOW_RESULT_INVALID_ARGUMENT;
    if (!flow_next_freeze_candidate(flow, &pid))
        return LINK_DIAGNOSTIC_FLOW_RESULT_INVALID_STATE;

    result = link_obd2_build_freeze_pid_request(
        pid, flow->freeze_frame_number, command, sizeof(command));
    if (result != LINK_OBD2_RESULT_OK)
        return flow_fail_obd2(flow, result);
    flow->active_pid = pid;
    action->pid = pid;
    return flow_emit_command(
        flow, action, command, flow->config.query_timeout_ms);
}

static void flow_finish_standard_context(
    LinkDiagnosticFlow *flow,
    LinkDiagnosticFlowEvent *event)
{
    if (flow == NULL || event == NULL) return;
    flow->standard_diagnostic_context_complete = true;
    flow->freeze_frame_complete = true;
    event->kind =
        LINK_DIAGNOSTIC_FLOW_EVENT_DIAGNOSTIC_CONTEXT_COMPLETE;
    event->diagnostic_context_complete = true;

    if (flow->config.manufacturer_extension_after_standard_dtcs) {
        flow->stage = LINK_DIAGNOSTIC_FLOW_MANUFACTURER_EXTENSION;
        event->became_ready = false;
    } else if (flow->config.preserve_live_response_headers) {
        flow->stage = LINK_DIAGNOSTIC_FLOW_CONFIGURING_LIVE_HEADERS;
        event->became_ready = false;
    } else {
        flow->stage = LINK_DIAGNOSTIC_FLOW_LIVE;
        event->became_ready = true;
    }
}

static LinkDiagnosticFlowResult flow_accept_readiness(
    LinkDiagnosticFlow *flow,
    const LinkElm327Response *response,
    LinkDiagnosticFlowEvent *event)
{
    LinkObd2Result result = LINK_OBD2_RESULT_OK;
    uint8_t nrc = 0U;

    flow->readiness_attempted = true;
    flow->readiness_available = false;
    event->kind = LINK_DIAGNOSTIC_FLOW_EVENT_READINESS;
    event->context_response_available =
        response->result != LINK_ELM327_RESULT_NO_DATA;

    if (event->context_response_available) {
        result = link_obd2_decode_readiness(response, &flow->readiness);
        if (result == LINK_OBD2_RESULT_OK) {
            flow->readiness_available = true;
        } else if (link_obd2_is_negative_response(
                       response, UINT8_C(0x01), &nrc) ||
                   result == LINK_OBD2_RESULT_UNEXPECTED_RESPONSE ||
                   result == LINK_OBD2_RESULT_MALFORMED_RESPONSE ||
                   result == LINK_OBD2_RESULT_ELM_ERROR) {
            event->context_response_available = false;
        } else {
            return flow_fail_obd2(flow, result);
        }
    }

    /*
     * Mode 02 is meaningful only when a stored DTC exists. Candidate PIDs are
     * additionally gated by the vehicle's Mode 01 capability inventory; each
     * Mode 02 request is still optional because ECUs may expose a smaller
     * freeze-frame set than their live-data set.
     */
    flow->freeze_frame_requested = flow->stored_dtcs.count != 0U;
    flow->freeze_frame_candidate_index = 0U;
    flow->freeze_frame_number = 0U;
    if (flow->freeze_frame_requested &&
        flow_has_freeze_candidate(flow)) {
        flow->stage = LINK_DIAGNOSTIC_FLOW_READING_FREEZE_FRAME;
        return LINK_DIAGNOSTIC_FLOW_RESULT_OK;
    }

    flow_finish_standard_context(flow, event);
    return LINK_DIAGNOSTIC_FLOW_RESULT_OK;
}

static LinkDiagnosticFlowResult flow_accept_freeze_frame(
    LinkDiagnosticFlow *flow,
    const LinkElm327Response *response,
    LinkDiagnosticFlowEvent *event)
{
    LinkObd2Sample sample;
    LinkObd2Result result = LINK_OBD2_RESULT_OK;
    event->kind = LINK_DIAGNOSTIC_FLOW_EVENT_FREEZE_FRAME_SAMPLE;
    event->context_response_available =
        response->result != LINK_ELM327_RESULT_NO_DATA;
    if (event->context_response_available) {
        result = link_obd2_decode_freeze_pid(
            response, flow->active_pid, flow->freeze_frame_number, &sample);
        if (result == LINK_OBD2_RESULT_OK) {
            if (flow->freeze_frame_sample_count <
                LINK_DIAGNOSTIC_FLOW_MAX_FREEZE_SAMPLES) {
                flow->freeze_frame_samples[
                    flow->freeze_frame_sample_count++] = sample;
            }
            event->sample = sample;
        } else if (result == LINK_OBD2_RESULT_UNEXPECTED_RESPONSE ||
                   result == LINK_OBD2_RESULT_MALFORMED_RESPONSE ||
                   result == LINK_OBD2_RESULT_ELM_ERROR) {
            event->context_response_available = false;
        } else {
            return flow_fail_obd2(flow, result);
        }
    }

    if (flow_has_freeze_candidate(flow)) {
        flow->stage = LINK_DIAGNOSTIC_FLOW_READING_FREEZE_FRAME;
        return LINK_DIAGNOSTIC_FLOW_RESULT_OK;
    }

    flow_finish_standard_context(flow, event);
    return LINK_DIAGNOSTIC_FLOW_RESULT_OK;
}

static LinkDiagnosticFlowResult flow_next_live_action(
    LinkDiagnosticFlow *flow,
    uint64_t now_ms,
    LinkDiagnosticFlowAction *action)
{
    char command[LINK_ELM327_MAX_COMMAND];
    LinkSchedulerDispatch dispatch;
    const LinkSchedulerNextResult next =
        link_scheduler_next(&flow->scheduler, now_ms, &dispatch);
    LinkObd2Result obd2_result;

    if (next == LINK_SCHEDULER_NEXT_EMPTY ||
        next == LINK_SCHEDULER_NEXT_PAUSED) {
        action->kind = LINK_DIAGNOSTIC_FLOW_ACTION_READY;
        return LINK_DIAGNOSTIC_FLOW_RESULT_OK;
    }
    if (next == LINK_SCHEDULER_NEXT_WAITING) {
        action->kind = LINK_DIAGNOSTIC_FLOW_ACTION_WAIT;
        action->wait_ms = dispatch.wait_ms;
        return LINK_DIAGNOSTIC_FLOW_RESULT_OK;
    }
    if (next != LINK_SCHEDULER_NEXT_READY || !dispatch.pid_valid) {
        return flow_fail_scheduler(flow, LINK_SCHEDULER_RESULT_INVALID_ARGUMENT);
    }

    obd2_result = link_obd2_build_live_pid_request(
        dispatch.pid, command, sizeof(command));
    if (obd2_result != LINK_OBD2_RESULT_OK) {
        return flow_fail_obd2(flow, obd2_result);
    }

    /* Reserve the slot before transmission so retries cannot over-poll a PID. */
    if (link_scheduler_mark_dispatched(&flow->scheduler, dispatch.index, now_ms) !=
        LINK_SCHEDULER_RESULT_OK) {
        return flow_fail_scheduler(flow, LINK_SCHEDULER_RESULT_INVALID_ARGUMENT);
    }

    flow->active_pid = dispatch.pid;
    flow->active_schedule_index = dispatch.index;
    flow->stage = LINK_DIAGNOSTIC_FLOW_READING_LIVE;
    action->pid = dispatch.pid;
    return flow_emit_command(flow, action, command, flow->config.live_timeout_ms);
}

static LinkDiagnosticFlowResult flow_accept_initialization(
    LinkDiagnosticFlow *flow,
    const LinkElm327Response *response,
    LinkDiagnosticFlowStage stage,
    LinkDiagnosticFlowEvent *event)
{
    const LinkElm327Result result =
        link_elm327_init_accept(&flow->initialization, response);

    if (result != LINK_ELM327_RESULT_OK ||
        flow->initialization.stage == LINK_ELM327_INIT_FAILED) {
        return flow_fail_elm(flow, result);
    }
    if (flow->initialization.adapter_id[0] != '\0') {
        event->kind = LINK_DIAGNOSTIC_FLOW_EVENT_ADAPTER_IDENTIFIED;
    }
    if (flow->initialization.stage != LINK_ELM327_INIT_COMPLETE) {
        return LINK_DIAGNOSTIC_FLOW_RESULT_OK;
    }

    if (stage == LINK_DIAGNOSTIC_FLOW_RESTORING_AFTER_MANUFACTURER) {
        /* Manufacturer probing may change adapter state; resume only after the
         * standard ELM setup has been replayed in full. */
        flow->stage = flow->standard_diagnostic_context_complete
            ? (flow->config.preserve_live_response_headers
                ? LINK_DIAGNOSTIC_FLOW_CONFIGURING_LIVE_HEADERS
                : LINK_DIAGNOSTIC_FLOW_LIVE)
            : (flow->standard_dtc_inventory_complete
                ? LINK_DIAGNOSTIC_FLOW_READING_READINESS
                : LINK_DIAGNOSTIC_FLOW_SCANNING_STORED_DTCS);
    } else {
        flow->supported_pid_base = 0x00U;
        flow->stage = flow->config.preserve_pid_discovery_response_headers
            ? LINK_DIAGNOSTIC_FLOW_CONFIGURING_PID_DISCOVERY_HEADERS
            : LINK_DIAGNOSTIC_FLOW_DISCOVERING_PIDS;
    }
    return LINK_DIAGNOSTIC_FLOW_RESULT_OK;
}

static LinkDiagnosticFlowResult flow_accept_pid_discovery_header_configuration(
    LinkDiagnosticFlow *flow,
    const LinkElm327Response *response,
    bool enabling)
{
    if (response->result != LINK_ELM327_RESULT_OK || !response->ok_seen) {
        return flow_fail_elm(
            flow,
            response->result != LINK_ELM327_RESULT_OK
                ? response->result
                : LINK_ELM327_RESULT_MALFORMED_RESPONSE);
    }
    flow->stage = enabling
        ? LINK_DIAGNOSTIC_FLOW_DISCOVERING_PIDS
        : LINK_DIAGNOSTIC_FLOW_READING_STANDARD_VIN;
    return LINK_DIAGNOSTIC_FLOW_RESULT_OK;
}

static LinkDiagnosticFlowResult flow_accept_live_header_configuration(
    LinkDiagnosticFlow *flow,
    const LinkElm327Response *response)
{
    if (response->result != LINK_ELM327_RESULT_OK || !response->ok_seen) {
        return flow_fail_elm(
            flow,
            response->result != LINK_ELM327_RESULT_OK
                ? response->result
                : LINK_ELM327_RESULT_MALFORMED_RESPONSE);
    }
    flow->stage = LINK_DIAGNOSTIC_FLOW_LIVE;
    return LINK_DIAGNOSTIC_FLOW_RESULT_OK;
}

static LinkDiagnosticFlowResult flow_accept_pid_discovery(
    LinkDiagnosticFlow *flow,
    const LinkElm327Response *response,
    LinkDiagnosticFlowEvent *event)
{
    bool has_more = false;
    const LinkObd2Result result =
        link_obd2_accept_supported_pid_responders(
            response,
            flow->supported_pid_base,
            &flow->supported_pids,
            &flow->supported_pid_responders,
            &has_more);

    if (result != LINK_OBD2_RESULT_OK) {
        return flow_fail_obd2(flow, result);
    }
    if (has_more && flow->supported_pid_base <= 0xc0U) {
        flow->supported_pid_base = (uint8_t)(flow->supported_pid_base + 0x20U);
    } else {
        flow->stage = flow->config.preserve_pid_discovery_response_headers
            ? LINK_DIAGNOSTIC_FLOW_RESTORING_PID_DISCOVERY_HEADERS
            : LINK_DIAGNOSTIC_FLOW_READING_STANDARD_VIN;
        event->kind = LINK_DIAGNOSTIC_FLOW_EVENT_PID_DISCOVERY_COMPLETE;
    }
    return LINK_DIAGNOSTIC_FLOW_RESULT_OK;
}

static LinkDiagnosticFlowResult flow_accept_standard_vin(
    LinkDiagnosticFlow *flow,
    const LinkElm327Response *response,
    LinkDiagnosticFlowEvent *event)
{
    LinkObd2Result result = LINK_OBD2_RESULT_OK;

    flow->standard_vin_attempted = true;
    flow->standard_vin_available = false;
    flow->standard_vin[0] = '\0';

    /* A missing or malformed VIN is not fatal: many older ECUs do not
     * implement mode 09 even though the rest of diagnostics is usable. */
    if (response->result != LINK_ELM327_RESULT_NO_DATA) {
        result = link_obd2_decode_vin(response, flow->standard_vin);
        if (result == LINK_OBD2_RESULT_OK) {
            flow->standard_vin_available = true;
        } else if (result != LINK_OBD2_RESULT_UNEXPECTED_RESPONSE &&
                   result != LINK_OBD2_RESULT_MALFORMED_RESPONSE &&
                   result != LINK_OBD2_RESULT_ELM_ERROR) {
            return flow_fail_obd2(flow, result);
        }
    }

    event->kind = LINK_DIAGNOSTIC_FLOW_EVENT_STANDARD_VIN;
    event->vin_available = flow->standard_vin_available;
    event->vin = flow->standard_vin_available ? flow->standard_vin : NULL;
    flow->stage = flow->config.manufacturer_extension_after_pid_discovery
        ? LINK_DIAGNOSTIC_FLOW_MANUFACTURER_EXTENSION
        : LINK_DIAGNOSTIC_FLOW_SCANNING_STORED_DTCS;
    return LINK_DIAGNOSTIC_FLOW_RESULT_OK;
}

static LinkDiagnosticFlowResult flow_accept_dtc_inventory(
    LinkDiagnosticFlow *flow,
    const LinkElm327Response *response,
    LinkDiagnosticFlowStage stage,
    uint64_t now_ms,
    LinkDiagnosticFlowEvent *event)
{
    const LinkObd2DtcKind kind = flow_stage_dtc_kind(stage);
    LinkObd2DtcList decoded = {0};
    LinkObd2DtcList *target = flow_dtc_list(flow, kind);
    LinkObd2Result result = LINK_OBD2_RESULT_OK;
    uint8_t negative_response_code = 0U;

    if (target == NULL) {
        return flow_fail_invalid_state(flow);
    }
    event->dtc_response_available =
        response->result != LINK_ELM327_RESULT_NO_DATA;
    if (event->dtc_response_available) {
        result = link_obd2_decode_dtcs(response, kind, &decoded);
    }
    if (result != LINK_OBD2_RESULT_OK) {
        const uint8_t request_service = flow_dtc_request_service(kind);

        /* A standards-compliant negative response means this DTC class is
         * unsupported, not that the whole diagnostic session failed. */
        if (!link_obd2_is_negative_response(
                response, request_service, &negative_response_code)) {
            return flow_fail_obd2(flow, result);
        }
        event->dtc_response_available = false;
        event->dtc_negative_response = true;
        event->dtc_negative_response_code = negative_response_code;
    }

    *target = decoded;
    event->kind = LINK_DIAGNOSTIC_FLOW_EVENT_DTC_LIST;
    event->dtc_kind = kind;
    event->dtc_list = target;
    flow->stage = flow_next_dtc_stage(kind);

    if (kind == LINK_OBD2_DTC_PERMANENT) {
        const LinkSchedulerResult schedule_result =
            link_scheduler_configure_standard_obd2_bits(
                &flow->scheduler, flow->supported_pids.bits, now_ms);

        if (schedule_result != LINK_SCHEDULER_RESULT_OK) {
            return flow_fail_scheduler(flow, schedule_result);
        }
        flow->standard_dtc_inventory_complete = true;
        /*
         * Readiness and Mode 02 freeze-frame context are part of the same
         * standard fault investigation. Do not declare the investigation
         * ready until those optional, capability-gated context requests have
         * been attempted.
         */
        flow->stage = LINK_DIAGNOSTIC_FLOW_READING_READINESS;
        event->became_ready = false;
    }
    return LINK_DIAGNOSTIC_FLOW_RESULT_OK;
}

static LinkDiagnosticFlowResult flow_accept_live_sample(
    LinkDiagnosticFlow *flow,
    const LinkElm327Response *response,
    LinkDiagnosticFlowEvent *event)
{
    LinkObd2Result result;
    LinkObd2ResponderSampleList responders;

    flow->stage = LINK_DIAGNOSTIC_FLOW_LIVE;
    if (response->result == LINK_ELM327_RESULT_NO_DATA) {
        event->kind = LINK_DIAGNOSTIC_FLOW_EVENT_LIVE_NO_DATA;
        event->sample.pid = flow->active_pid;
        return LINK_DIAGNOSTIC_FLOW_RESULT_OK;
    }

    result = link_obd2_decode_live_pid_responders(
        response, flow->active_pid, &responders);
    if (result == LINK_OBD2_RESULT_UNSUPPORTED_PID) {
        event->kind = LINK_DIAGNOSTIC_FLOW_EVENT_LIVE_UNSUPPORTED;
        event->sample.pid = flow->active_pid;
        return LINK_DIAGNOSTIC_FLOW_RESULT_OK;
    }
    if (result != LINK_OBD2_RESULT_OK) {
        return flow_fail_obd2(flow, result);
    }

    event->kind = LINK_DIAGNOSTIC_FLOW_EVENT_LIVE_SAMPLE;
    event->responder_samples = responders;
    event->sample = responders.samples[0].sample;
    return LINK_DIAGNOSTIC_FLOW_RESULT_OK;
}

const char *link_diagnostic_flow_result_name(LinkDiagnosticFlowResult result)
{
    switch (result) {
    case LINK_DIAGNOSTIC_FLOW_RESULT_OK: return "ok";
    case LINK_DIAGNOSTIC_FLOW_RESULT_INVALID_ARGUMENT: return "invalid-argument";
    case LINK_DIAGNOSTIC_FLOW_RESULT_INVALID_STATE: return "invalid-state";
    case LINK_DIAGNOSTIC_FLOW_RESULT_ELM_ERROR: return "elm-error";
    case LINK_DIAGNOSTIC_FLOW_RESULT_OBD2_ERROR: return "obd2-error";
    case LINK_DIAGNOSTIC_FLOW_RESULT_SCHEDULER_ERROR: return "scheduler-error";
    }
    return "unknown";
}

const char *link_diagnostic_flow_stage_name(LinkDiagnosticFlowStage stage)
{
    switch (stage) {
    case LINK_DIAGNOSTIC_FLOW_IDLE: return "idle";
    case LINK_DIAGNOSTIC_FLOW_INITIALIZING: return "initializing";
    case LINK_DIAGNOSTIC_FLOW_CONFIGURING_PID_DISCOVERY_HEADERS:
        return "configuring-pid-discovery-headers";
    case LINK_DIAGNOSTIC_FLOW_DISCOVERING_PIDS: return "discovering-pids";
    case LINK_DIAGNOSTIC_FLOW_RESTORING_PID_DISCOVERY_HEADERS:
        return "restoring-pid-discovery-headers";
    case LINK_DIAGNOSTIC_FLOW_READING_STANDARD_VIN: return "reading-standard-vin";
    case LINK_DIAGNOSTIC_FLOW_MANUFACTURER_EXTENSION: return "manufacturer-extension";
    case LINK_DIAGNOSTIC_FLOW_RESTORING_AFTER_MANUFACTURER: return "restoring-after-manufacturer";
    case LINK_DIAGNOSTIC_FLOW_SCANNING_STORED_DTCS: return "scanning-stored-dtcs";
    case LINK_DIAGNOSTIC_FLOW_SCANNING_PENDING_DTCS: return "scanning-pending-dtcs";
    case LINK_DIAGNOSTIC_FLOW_SCANNING_PERMANENT_DTCS: return "scanning-permanent-dtcs";
    case LINK_DIAGNOSTIC_FLOW_READING_READINESS: return "reading-readiness";
    case LINK_DIAGNOSTIC_FLOW_READING_FREEZE_FRAME: return "reading-freeze-frame";
    case LINK_DIAGNOSTIC_FLOW_CONFIGURING_LIVE_HEADERS: return "configuring-live-headers";
    case LINK_DIAGNOSTIC_FLOW_LIVE: return "live";
    case LINK_DIAGNOSTIC_FLOW_READING_LIVE: return "reading-live";
    case LINK_DIAGNOSTIC_FLOW_FAILED: return "failed";
    }
    return "unknown";
}

LinkDiagnosticFlowResult link_diagnostic_flow_init(
    LinkDiagnosticFlow *flow,
    const LinkDiagnosticFlowConfig *config)
{
    LinkDiagnosticFlowConfig resolved = LINK_DIAGNOSTIC_FLOW_CONFIG_INIT;

    if (flow == NULL) {
        return LINK_DIAGNOSTIC_FLOW_RESULT_INVALID_ARGUMENT;
    }
    if (config != NULL) {
        resolved = *config;
        if (resolved.manufacturer_extension_after_pid_discovery &&
            resolved.manufacturer_extension_after_standard_dtcs) {
            return LINK_DIAGNOSTIC_FLOW_RESULT_INVALID_ARGUMENT;
        }
        if (resolved.init_timeout_ms == 0U) {
            resolved.init_timeout_ms = LINK_DIAGNOSTIC_FLOW_DEFAULT_INIT_TIMEOUT_MS;
        }
        if (resolved.query_timeout_ms == 0U) {
            resolved.query_timeout_ms = LINK_DIAGNOSTIC_FLOW_DEFAULT_QUERY_TIMEOUT_MS;
        }
        if (resolved.live_timeout_ms == 0U) {
            resolved.live_timeout_ms = LINK_DIAGNOSTIC_FLOW_DEFAULT_LIVE_TIMEOUT_MS;
        }
    }

    memset(flow, 0, sizeof(*flow));
    flow->config = resolved;
    flow->stage = LINK_DIAGNOSTIC_FLOW_IDLE;
    flow->failure = LINK_DIAGNOSTIC_FLOW_RESULT_OK;
    flow->elm_failure = LINK_ELM327_RESULT_OK;
    flow->obd2_failure = LINK_OBD2_RESULT_OK;
    flow->scheduler_failure = LINK_SCHEDULER_RESULT_OK;
    link_obd2_pid_set_clear(&flow->supported_pids);
    link_scheduler_init(&flow->scheduler);
    return LINK_DIAGNOSTIC_FLOW_RESULT_OK;
}

LinkDiagnosticFlowResult link_diagnostic_flow_start(LinkDiagnosticFlow *flow)
{
    LinkDiagnosticFlowConfig config;

    if (flow == NULL) {
        return LINK_DIAGNOSTIC_FLOW_RESULT_INVALID_ARGUMENT;
    }
    config = flow->config;
    (void)link_diagnostic_flow_init(flow, &config);
    link_elm327_init_begin(&flow->initialization);
    flow->stage = LINK_DIAGNOSTIC_FLOW_INITIALIZING;
    return LINK_DIAGNOSTIC_FLOW_RESULT_OK;
}

LinkDiagnosticFlowResult link_diagnostic_flow_next_action(
    LinkDiagnosticFlow *flow,
    uint64_t now_ms,
    LinkDiagnosticFlowAction *action)
{
    if (flow == NULL || action == NULL) {
        return LINK_DIAGNOSTIC_FLOW_RESULT_INVALID_ARGUMENT;
    }
    flow_clear_action(action);

    if (flow->stage == LINK_DIAGNOSTIC_FLOW_FAILED) {
        action->kind = LINK_DIAGNOSTIC_FLOW_ACTION_FAILED;
        return flow->failure;
    }
    if (flow->awaiting_response) {
        return LINK_DIAGNOSTIC_FLOW_RESULT_OK;
    }

    switch (flow->stage) {
    case LINK_DIAGNOSTIC_FLOW_IDLE:
        return LINK_DIAGNOSTIC_FLOW_RESULT_OK;

    case LINK_DIAGNOSTIC_FLOW_INITIALIZING:
    case LINK_DIAGNOSTIC_FLOW_RESTORING_AFTER_MANUFACTURER:
        return flow_next_initialization_action(flow, action);

    case LINK_DIAGNOSTIC_FLOW_CONFIGURING_PID_DISCOVERY_HEADERS:
        return flow_emit_command(
            flow, action, "ATH1", flow->config.init_timeout_ms);

    case LINK_DIAGNOSTIC_FLOW_DISCOVERING_PIDS:
        return flow_next_pid_discovery_action(flow, action);

    case LINK_DIAGNOSTIC_FLOW_RESTORING_PID_DISCOVERY_HEADERS:
        return flow_emit_command(
            flow, action, "ATH0", flow->config.init_timeout_ms);

    case LINK_DIAGNOSTIC_FLOW_READING_STANDARD_VIN:
        return flow_next_vin_action(flow, action);

    case LINK_DIAGNOSTIC_FLOW_MANUFACTURER_EXTENSION:
        action->kind = LINK_DIAGNOSTIC_FLOW_ACTION_MANUFACTURER_EXTENSION;
        return LINK_DIAGNOSTIC_FLOW_RESULT_OK;

    case LINK_DIAGNOSTIC_FLOW_SCANNING_STORED_DTCS:
    case LINK_DIAGNOSTIC_FLOW_SCANNING_PENDING_DTCS:
    case LINK_DIAGNOSTIC_FLOW_SCANNING_PERMANENT_DTCS:
        return flow_next_dtc_action(flow, action);

    case LINK_DIAGNOSTIC_FLOW_READING_READINESS:
        return flow_next_readiness_action(flow, action);

    case LINK_DIAGNOSTIC_FLOW_READING_FREEZE_FRAME:
        return flow_next_freeze_action(flow, action);

    case LINK_DIAGNOSTIC_FLOW_CONFIGURING_LIVE_HEADERS:
        return flow_emit_command(
            flow, action, "ATH1", flow->config.init_timeout_ms);

    case LINK_DIAGNOSTIC_FLOW_LIVE:
        return flow_next_live_action(flow, now_ms, action);

    case LINK_DIAGNOSTIC_FLOW_READING_LIVE:
        action->kind = LINK_DIAGNOSTIC_FLOW_ACTION_FAILED;
        return flow_fail_invalid_state(flow);

    case LINK_DIAGNOSTIC_FLOW_FAILED:
        break;
    }

    action->kind = LINK_DIAGNOSTIC_FLOW_ACTION_FAILED;
    return flow_fail_invalid_state(flow);
}

LinkDiagnosticFlowResult link_diagnostic_flow_accept_response(
    LinkDiagnosticFlow *flow,
    const LinkElm327Response *response,
    uint64_t now_ms,
    LinkDiagnosticFlowEvent *event)
{
    LinkDiagnosticFlowStage stage;

    if (flow == NULL || response == NULL || event == NULL) {
        return LINK_DIAGNOSTIC_FLOW_RESULT_INVALID_ARGUMENT;
    }
    flow_clear_event(event);
    if (flow->stage == LINK_DIAGNOSTIC_FLOW_FAILED) {
        return flow->failure;
    }
    if (!flow->awaiting_response) {
        return LINK_DIAGNOSTIC_FLOW_RESULT_INVALID_STATE;
    }

    stage = flow->stage;
    flow->awaiting_response = false;

    switch (stage) {
    case LINK_DIAGNOSTIC_FLOW_INITIALIZING:
    case LINK_DIAGNOSTIC_FLOW_RESTORING_AFTER_MANUFACTURER:
        return flow_accept_initialization(flow, response, stage, event);
    case LINK_DIAGNOSTIC_FLOW_CONFIGURING_PID_DISCOVERY_HEADERS:
        return flow_accept_pid_discovery_header_configuration(
            flow, response, true);
    case LINK_DIAGNOSTIC_FLOW_RESTORING_PID_DISCOVERY_HEADERS:
        return flow_accept_pid_discovery_header_configuration(
            flow, response, false);
    case LINK_DIAGNOSTIC_FLOW_CONFIGURING_LIVE_HEADERS:
        return flow_accept_live_header_configuration(flow, response);
    case LINK_DIAGNOSTIC_FLOW_DISCOVERING_PIDS:
        return flow_accept_pid_discovery(flow, response, event);
    case LINK_DIAGNOSTIC_FLOW_READING_STANDARD_VIN:
        return flow_accept_standard_vin(flow, response, event);
    case LINK_DIAGNOSTIC_FLOW_SCANNING_STORED_DTCS:
    case LINK_DIAGNOSTIC_FLOW_SCANNING_PENDING_DTCS:
    case LINK_DIAGNOSTIC_FLOW_SCANNING_PERMANENT_DTCS:
        return flow_accept_dtc_inventory(
            flow, response, stage, now_ms, event);
    case LINK_DIAGNOSTIC_FLOW_READING_READINESS:
        return flow_accept_readiness(flow, response, event);
    case LINK_DIAGNOSTIC_FLOW_READING_FREEZE_FRAME:
        return flow_accept_freeze_frame(flow, response, event);
    case LINK_DIAGNOSTIC_FLOW_READING_LIVE:
        return flow_accept_live_sample(flow, response, event);
    case LINK_DIAGNOSTIC_FLOW_IDLE:
    case LINK_DIAGNOSTIC_FLOW_MANUFACTURER_EXTENSION:
    case LINK_DIAGNOSTIC_FLOW_LIVE:
    case LINK_DIAGNOSTIC_FLOW_FAILED:
        break;
    }

    return flow_fail_invalid_state(flow);
}

LinkDiagnosticFlowResult link_diagnostic_flow_recover_live_timeout(
    LinkDiagnosticFlow *flow,
    uint64_t now_ms)
{
    LinkSchedulerItem *item;
    uint64_t deferred_due;

    if (flow == NULL) {
        return LINK_DIAGNOSTIC_FLOW_RESULT_INVALID_ARGUMENT;
    }
    if (flow->stage != LINK_DIAGNOSTIC_FLOW_READING_LIVE ||
        !flow->awaiting_response ||
        flow->active_schedule_index >= flow->scheduler.count) {
        return LINK_DIAGNOSTIC_FLOW_RESULT_INVALID_STATE;
    }

    item = &flow->scheduler.items[flow->active_schedule_index];
    deferred_due = infiltratr_u64_add_saturating(
        now_ms, (uint64_t)item->interval_ms);
    if (item->next_due_ms < deferred_due) {
        item->next_due_ms = deferred_due;
    }

    flow->awaiting_response = false;
    flow->stage = LINK_DIAGNOSTIC_FLOW_LIVE;
    flow->failure = LINK_DIAGNOSTIC_FLOW_RESULT_OK;
    flow->elm_failure = LINK_ELM327_RESULT_OK;
    flow->obd2_failure = LINK_OBD2_RESULT_OK;
    flow->scheduler_failure = LINK_SCHEDULER_RESULT_OK;
    return LINK_DIAGNOSTIC_FLOW_RESULT_OK;
}

LinkDiagnosticFlowResult link_diagnostic_flow_begin_live_manufacturer_extension(
    LinkDiagnosticFlow *flow)
{
    if (flow == NULL) {
        return LINK_DIAGNOSTIC_FLOW_RESULT_INVALID_ARGUMENT;
    }
    if (flow->stage != LINK_DIAGNOSTIC_FLOW_LIVE ||
        flow->awaiting_response ||
        !flow->standard_diagnostic_context_complete) {
        return LINK_DIAGNOSTIC_FLOW_RESULT_INVALID_STATE;
    }

    flow->stage = LINK_DIAGNOSTIC_FLOW_MANUFACTURER_EXTENSION;
    return LINK_DIAGNOSTIC_FLOW_RESULT_OK;
}

LinkDiagnosticFlowResult link_diagnostic_flow_resume_after_manufacturer(
    LinkDiagnosticFlow *flow)
{
    if (flow == NULL) {
        return LINK_DIAGNOSTIC_FLOW_RESULT_INVALID_ARGUMENT;
    }
    if (flow->stage != LINK_DIAGNOSTIC_FLOW_MANUFACTURER_EXTENSION ||
        flow->awaiting_response) {
        return LINK_DIAGNOSTIC_FLOW_RESULT_INVALID_STATE;
    }

    if (flow->config.restore_adapter_after_manufacturer_extension) {
        link_elm327_init_begin(&flow->initialization);
        flow->stage = LINK_DIAGNOSTIC_FLOW_RESTORING_AFTER_MANUFACTURER;
    } else {
        flow->stage = flow->standard_diagnostic_context_complete
            ? (flow->config.preserve_live_response_headers
                ? LINK_DIAGNOSTIC_FLOW_CONFIGURING_LIVE_HEADERS
                : LINK_DIAGNOSTIC_FLOW_LIVE)
            : (flow->standard_dtc_inventory_complete
                ? LINK_DIAGNOSTIC_FLOW_READING_READINESS
                : LINK_DIAGNOSTIC_FLOW_SCANNING_STORED_DTCS);
    }
    return LINK_DIAGNOSTIC_FLOW_RESULT_OK;
}

void link_diagnostic_flow_fail(
    LinkDiagnosticFlow *flow,
    LinkDiagnosticFlowResult failure)
{
    if (flow == NULL) {
        return;
    }
    if (failure == LINK_DIAGNOSTIC_FLOW_RESULT_OK ||
        failure == LINK_DIAGNOSTIC_FLOW_RESULT_INVALID_ARGUMENT) {
        failure = LINK_DIAGNOSTIC_FLOW_RESULT_INVALID_STATE;
    }
    flow->failure = failure;
    flow->stage = LINK_DIAGNOSTIC_FLOW_FAILED;
    flow->awaiting_response = false;
}

const LinkObd2PidSet *link_diagnostic_flow_supported_pids(
    const LinkDiagnosticFlow *flow)
{
    return flow == NULL ? NULL : &flow->supported_pids;
}

const LinkObd2PidSet *link_diagnostic_flow_supported_pids_for_responder(
    const LinkDiagnosticFlow *flow,
    uint32_t responder_id,
    bool extended_id)
{
    return flow == NULL ? NULL : link_obd2_responder_pid_set_find(
        &flow->supported_pid_responders, responder_id, extended_id);
}

const LinkObd2DtcList *link_diagnostic_flow_dtcs(
    const LinkDiagnosticFlow *flow,
    LinkObd2DtcKind kind)
{
    if (flow == NULL) {
        return NULL;
    }
    switch (kind) {
    case LINK_OBD2_DTC_STORED:
        return &flow->stored_dtcs;
    case LINK_OBD2_DTC_PENDING:
        return &flow->pending_dtcs;
    case LINK_OBD2_DTC_PERMANENT:
        return &flow->permanent_dtcs;
    }
    return NULL;
}

const LinkObd2Readiness *link_diagnostic_flow_readiness(
    const LinkDiagnosticFlow *flow)
{
    return flow != NULL && flow->readiness_available
        ? &flow->readiness : NULL;
}

const LinkObd2Sample *link_diagnostic_flow_freeze_frame_samples(
    const LinkDiagnosticFlow *flow,
    size_t *count)
{
    if (count != NULL)
        *count = flow != NULL ? flow->freeze_frame_sample_count : 0U;
    return flow != NULL && flow->freeze_frame_sample_count != 0U
        ? flow->freeze_frame_samples : NULL;
}

bool link_diagnostic_flow_standard_context_complete(
    const LinkDiagnosticFlow *flow)
{
    return flow != NULL && flow->standard_diagnostic_context_complete;
}

const char *link_diagnostic_flow_adapter_identifier(
    const LinkDiagnosticFlow *flow)
{
    if (flow == NULL || flow->initialization.adapter_id[0] == '\0') {
        return NULL;
    }
    return flow->initialization.adapter_id;
}

const char *link_diagnostic_flow_standard_vin(
    const LinkDiagnosticFlow *flow)
{
    if (flow == NULL || !flow->standard_vin_available ||
        flow->standard_vin[0] == '\0') {
        return NULL;
    }
    return flow->standard_vin;
}
