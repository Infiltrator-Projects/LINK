// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file diagnostic_flow.h
 * @brief Shared portable diagnostic workflow for MBLINK/JAGLINK front ends.
 *
 * This state machine owns the product-neutral sequence around an already-open
 * ELM327 session: adapter initialisation, standard OBD-II capability discovery,
 * the bounded read-only DTC inventory, standard live-data scheduling and live
 * PID decoding. Manufacturer-specific discovery is represented by explicit
 * extension points; recurring manufacturer live work is scheduled by the same
 * LINK queue as standard OBD so only one owner ever feeds the adapter.
 */
#ifndef LINK_DIAGNOSTIC_FLOW_H
#define LINK_DIAGNOSTIC_FLOW_H

#include "link/elm327.h"
#include "link/obd2.h"
#include "link/scheduler.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LINK_DIAGNOSTIC_FLOW_DEFAULT_INIT_TIMEOUT_MS UINT64_C(4000)
#define LINK_DIAGNOSTIC_FLOW_DEFAULT_QUERY_TIMEOUT_MS UINT64_C(8000)
#define LINK_DIAGNOSTIC_FLOW_DEFAULT_LIVE_TIMEOUT_MS UINT64_C(2000)
#define LINK_DIAGNOSTIC_FLOW_MAX_FREEZE_SAMPLES 8U

typedef enum {
    LINK_DIAGNOSTIC_FLOW_RESULT_OK = 0,
    LINK_DIAGNOSTIC_FLOW_RESULT_INVALID_ARGUMENT,
    LINK_DIAGNOSTIC_FLOW_RESULT_INVALID_STATE,
    LINK_DIAGNOSTIC_FLOW_RESULT_ELM_ERROR,
    LINK_DIAGNOSTIC_FLOW_RESULT_OBD2_ERROR,
    LINK_DIAGNOSTIC_FLOW_RESULT_SCHEDULER_ERROR
} LinkDiagnosticFlowResult;

typedef enum {
    LINK_DIAGNOSTIC_FLOW_IDLE = 0,
    LINK_DIAGNOSTIC_FLOW_INITIALIZING,
    LINK_DIAGNOSTIC_FLOW_CONFIGURING_PID_DISCOVERY_HEADERS,
    LINK_DIAGNOSTIC_FLOW_DISCOVERING_PIDS,
    LINK_DIAGNOSTIC_FLOW_RESTORING_PID_DISCOVERY_HEADERS,
    LINK_DIAGNOSTIC_FLOW_READING_STANDARD_VIN,
    LINK_DIAGNOSTIC_FLOW_MANUFACTURER_EXTENSION,
    LINK_DIAGNOSTIC_FLOW_RESTORING_AFTER_MANUFACTURER,
    LINK_DIAGNOSTIC_FLOW_SCANNING_STORED_DTCS,
    LINK_DIAGNOSTIC_FLOW_SCANNING_PENDING_DTCS,
    LINK_DIAGNOSTIC_FLOW_SCANNING_PERMANENT_DTCS,
    LINK_DIAGNOSTIC_FLOW_READING_READINESS,
    LINK_DIAGNOSTIC_FLOW_READING_FREEZE_FRAME,
    LINK_DIAGNOSTIC_FLOW_CONFIGURING_LIVE_HEADERS,
    LINK_DIAGNOSTIC_FLOW_LIVE,
    LINK_DIAGNOSTIC_FLOW_READING_LIVE,
    LINK_DIAGNOSTIC_FLOW_FAILED
} LinkDiagnosticFlowStage;

typedef enum {
    LINK_DIAGNOSTIC_FLOW_ACTION_NONE = 0,
    LINK_DIAGNOSTIC_FLOW_ACTION_SEND_COMMAND,
    LINK_DIAGNOSTIC_FLOW_ACTION_WAIT,
    LINK_DIAGNOSTIC_FLOW_ACTION_MANUFACTURER_EXTENSION,
    LINK_DIAGNOSTIC_FLOW_ACTION_SCHEDULED_MANUFACTURER_JOB,
    LINK_DIAGNOSTIC_FLOW_ACTION_READY,
    LINK_DIAGNOSTIC_FLOW_ACTION_FAILED
} LinkDiagnosticFlowActionKind;

typedef enum {
    LINK_DIAGNOSTIC_FLOW_EVENT_NONE = 0,
    LINK_DIAGNOSTIC_FLOW_EVENT_ADAPTER_IDENTIFIED,
    LINK_DIAGNOSTIC_FLOW_EVENT_PID_DISCOVERY_COMPLETE,
    LINK_DIAGNOSTIC_FLOW_EVENT_STANDARD_VIN,
    LINK_DIAGNOSTIC_FLOW_EVENT_DTC_LIST,
    LINK_DIAGNOSTIC_FLOW_EVENT_READINESS,
    LINK_DIAGNOSTIC_FLOW_EVENT_FREEZE_FRAME_SAMPLE,
    LINK_DIAGNOSTIC_FLOW_EVENT_DIAGNOSTIC_CONTEXT_COMPLETE,
    LINK_DIAGNOSTIC_FLOW_EVENT_LIVE_SAMPLE,
    LINK_DIAGNOSTIC_FLOW_EVENT_LIVE_STRUCTURED,
    LINK_DIAGNOSTIC_FLOW_EVENT_LIVE_NO_DATA,
    LINK_DIAGNOSTIC_FLOW_EVENT_LIVE_UNSUPPORTED
} LinkDiagnosticFlowEventKind;

typedef struct {
    /*
     * Optional vehicle-first startup hook. When enabled, Mode 09 VIN is read
     * immediately after ELM initialisation and the manufacturer extension runs
     * before the broader Mode 01 capability and fault-context work.
     */
    bool manufacturer_extension_after_standard_vin;
    bool manufacturer_extension_after_pid_discovery;
    bool manufacturer_extension_after_standard_dtcs;
    bool restore_adapter_after_manufacturer_extension;
    bool preserve_pid_discovery_response_headers;
    bool preserve_live_response_headers;
    uint64_t init_timeout_ms;
    uint64_t query_timeout_ms;
    uint64_t live_timeout_ms;
} LinkDiagnosticFlowConfig;

#define LINK_DIAGNOSTIC_FLOW_CONFIG_INIT \
    { \
        .manufacturer_extension_after_standard_vin = false, \
        .manufacturer_extension_after_pid_discovery = false, \
        .manufacturer_extension_after_standard_dtcs = false, \
        .restore_adapter_after_manufacturer_extension = false, \
        .preserve_pid_discovery_response_headers = false, \
        .preserve_live_response_headers = false, \
        .init_timeout_ms = LINK_DIAGNOSTIC_FLOW_DEFAULT_INIT_TIMEOUT_MS, \
        .query_timeout_ms = LINK_DIAGNOSTIC_FLOW_DEFAULT_QUERY_TIMEOUT_MS, \
        .live_timeout_ms = LINK_DIAGNOSTIC_FLOW_DEFAULT_LIVE_TIMEOUT_MS \
    }

typedef struct {
    LinkDiagnosticFlowActionKind kind;
    char command[LINK_ELM327_MAX_COMMAND];
    uint64_t timeout_ms;
    uint64_t wait_ms;
    uint8_t pid;
    uint32_t manufacturer_job_token;
    LinkObd2DtcKind dtc_kind;
} LinkDiagnosticFlowAction;

typedef struct {
    LinkDiagnosticFlowEventKind kind;
    LinkObd2DtcKind dtc_kind;
    const LinkObd2DtcList *dtc_list;
    const char *vin;
    bool vin_available;
    LinkObd2Sample sample;
    LinkObd2ResponderSampleList responder_samples;
    LinkObd2DecodedPid decoded;
    LinkObd2ResponderDecodedPidList responder_decoded;
    bool became_ready;
    bool dtc_response_available;
    bool dtc_negative_response;
    uint8_t dtc_negative_response_code;
    bool context_response_available;
    bool diagnostic_context_complete;
} LinkDiagnosticFlowEvent;

typedef struct {
    LinkDiagnosticFlowConfig config;
    LinkDiagnosticFlowStage stage;
    LinkDiagnosticFlowResult failure;
    LinkElm327Result elm_failure;
    LinkObd2Result obd2_failure;
    LinkSchedulerResult scheduler_failure;
    LinkElm327InitState initialization;
    LinkObd2PidSet supported_pids;
    LinkObd2ResponderPidSetList supported_pid_responders;
    char standard_vin[LINK_OBD2_VIN_LENGTH + 1U];
    bool standard_vin_attempted;
    bool standard_vin_available;
    LinkObd2DtcList stored_dtcs;
    LinkObd2DtcList pending_dtcs;
    LinkObd2DtcList permanent_dtcs;
    LinkObd2Readiness readiness;
    bool readiness_attempted;
    bool readiness_available;
    LinkObd2Sample freeze_frame_samples[LINK_DIAGNOSTIC_FLOW_MAX_FREEZE_SAMPLES];
    size_t freeze_frame_sample_count;
    bool freeze_frame_requested;
    bool freeze_frame_complete;
    size_t freeze_frame_candidate_index;
    uint8_t freeze_frame_number;
    LinkScheduler scheduler;
    uint8_t supported_pid_base;
    size_t active_schedule_index;
    uint8_t active_pid;
    uint32_t active_manufacturer_job_token;
    bool scheduled_manufacturer_job_active;
    bool awaiting_response;
    bool standard_dtc_inventory_complete;
    bool standard_diagnostic_context_complete;
} LinkDiagnosticFlow;

const char *link_diagnostic_flow_result_name(LinkDiagnosticFlowResult result);
const char *link_diagnostic_flow_stage_name(LinkDiagnosticFlowStage stage);

/** Reset the flow to idle and apply validated/defaulted configuration. */
LinkDiagnosticFlowResult link_diagnostic_flow_init(
    LinkDiagnosticFlow *flow,
    const LinkDiagnosticFlowConfig *config);

/** Start a fresh standard diagnostic sequence. */
LinkDiagnosticFlowResult link_diagnostic_flow_start(LinkDiagnosticFlow *flow);

/** Return the next transport/platform action and advance only when necessary. */
LinkDiagnosticFlowResult link_diagnostic_flow_next_action(
    LinkDiagnosticFlow *flow,
    uint64_t now_ms,
    LinkDiagnosticFlowAction *action);

/** Consume one normalised ELM327 response for the previously emitted command. */
LinkDiagnosticFlowResult link_diagnostic_flow_accept_response(
    LinkDiagnosticFlow *flow,
    const LinkElm327Response *response,
    uint64_t now_ms,
    LinkDiagnosticFlowEvent *event);

/**
 * Register one recurring product/manufacturer live transaction in LINK's
 * single adapter scheduler. The token is opaque to LINK and must be non-zero.
 * Registration is accepted after standard live scheduling has been created;
 * duplicate tokens are rejected.
 */
LinkDiagnosticFlowResult link_diagnostic_flow_register_live_manufacturer_job(
    LinkDiagnosticFlow *flow,
    uint32_t token,
    uint32_t interval_ms,
    LinkSchedulerPriority priority,
    uint64_t first_due_ms);
LinkDiagnosticFlowResult link_diagnostic_flow_set_live_manufacturer_job_enabled(
    LinkDiagnosticFlow *flow,
    uint32_t token,
    bool enabled);

/**
 * Resume the standard sequence after product/manufacturer discovery or a
 * scheduled manufacturer transaction. If the configuration requests full
 * restoration, the complete ELM initialisation sequence runs again before the
 * next unfinished standard phase.
 */
LinkDiagnosticFlowResult link_diagnostic_flow_resume_after_manufacturer(
    LinkDiagnosticFlow *flow);

/**
 * Enter the manufacturer extension from an already-live diagnostic session.
 * Manual/product-initiated work uses this API. Recurring manufacturer live work
 * should instead be registered with
 * link_diagnostic_flow_register_live_manufacturer_job() so it cannot compete
 * with a second polling loop.
 */
LinkDiagnosticFlowResult link_diagnostic_flow_begin_live_manufacturer_extension(
    LinkDiagnosticFlow *flow);

/**
 * Abandon one timed-out live request after the transport has been
 * resynchronised. Discovery, DTC inventory and the scheduler are preserved.
 * The timed-out PID is deferred by one full interval so recovery cannot
 * immediately hammer the same request again.
 */
LinkDiagnosticFlowResult link_diagnostic_flow_recover_live_timeout(
    LinkDiagnosticFlow *flow,
    uint64_t now_ms);

/** Put the portable flow into a deterministic failed state after transport/UI failure. */
void link_diagnostic_flow_fail(
    LinkDiagnosticFlow *flow,
    LinkDiagnosticFlowResult failure);

const LinkObd2PidSet *link_diagnostic_flow_supported_pids(
    const LinkDiagnosticFlow *flow);
const LinkObd2PidSet *link_diagnostic_flow_supported_pids_for_responder(
    const LinkDiagnosticFlow *flow,
    uint32_t responder_id,
    bool extended_id);
const LinkObd2DtcList *link_diagnostic_flow_dtcs(
    const LinkDiagnosticFlow *flow,
    LinkObd2DtcKind kind);
/** Readiness snapshot captured after standard fault inventory, when available. */
const LinkObd2Readiness *link_diagnostic_flow_readiness(
    const LinkDiagnosticFlow *flow);
/** Bounded Mode 02 frame-zero samples captured for the current investigation. */
const LinkObd2Sample *link_diagnostic_flow_freeze_frame_samples(
    const LinkDiagnosticFlow *flow,
    size_t *count);
bool link_diagnostic_flow_standard_context_complete(
    const LinkDiagnosticFlow *flow);
const char *link_diagnostic_flow_adapter_identifier(
    const LinkDiagnosticFlow *flow);
const char *link_diagnostic_flow_standard_vin(
    const LinkDiagnosticFlow *flow);

#ifdef __cplusplus
}
#endif

#endif
