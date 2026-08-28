// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file diagnostic_flow.h
 * @brief Shared portable diagnostic workflow for MBLINK/JAGLINK front ends.
 *
 * This state machine owns the product-neutral sequence around an already-open
 * ELM327 session: adapter initialisation, standard OBD-II capability discovery,
 * the bounded read-only DTC inventory, standard live-data scheduling and live
 * PID decoding.  Manufacturer-specific discovery is represented by one explicit
 * extension point.  Platform code remains responsible only for transporting the
 * commands/actions and presenting events.
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
    LINK_DIAGNOSTIC_FLOW_DISCOVERING_PIDS,
    LINK_DIAGNOSTIC_FLOW_READING_STANDARD_VIN,
    LINK_DIAGNOSTIC_FLOW_MANUFACTURER_EXTENSION,
    LINK_DIAGNOSTIC_FLOW_RESTORING_AFTER_MANUFACTURER,
    LINK_DIAGNOSTIC_FLOW_SCANNING_STORED_DTCS,
    LINK_DIAGNOSTIC_FLOW_SCANNING_PENDING_DTCS,
    LINK_DIAGNOSTIC_FLOW_SCANNING_PERMANENT_DTCS,
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
    LINK_DIAGNOSTIC_FLOW_ACTION_READY,
    LINK_DIAGNOSTIC_FLOW_ACTION_FAILED
} LinkDiagnosticFlowActionKind;

typedef enum {
    LINK_DIAGNOSTIC_FLOW_EVENT_NONE = 0,
    LINK_DIAGNOSTIC_FLOW_EVENT_ADAPTER_IDENTIFIED,
    LINK_DIAGNOSTIC_FLOW_EVENT_PID_DISCOVERY_COMPLETE,
    LINK_DIAGNOSTIC_FLOW_EVENT_STANDARD_VIN,
    LINK_DIAGNOSTIC_FLOW_EVENT_DTC_LIST,
    LINK_DIAGNOSTIC_FLOW_EVENT_LIVE_SAMPLE,
    LINK_DIAGNOSTIC_FLOW_EVENT_LIVE_NO_DATA,
    LINK_DIAGNOSTIC_FLOW_EVENT_LIVE_UNSUPPORTED
} LinkDiagnosticFlowEventKind;

typedef struct {
    bool manufacturer_extension_after_pid_discovery;
    bool manufacturer_extension_after_standard_dtcs;
    bool restore_adapter_after_manufacturer_extension;
    bool preserve_live_response_headers;
    uint64_t init_timeout_ms;
    uint64_t query_timeout_ms;
    uint64_t live_timeout_ms;
} LinkDiagnosticFlowConfig;

#define LINK_DIAGNOSTIC_FLOW_CONFIG_INIT \
    { \
        .manufacturer_extension_after_pid_discovery = false, \
        .manufacturer_extension_after_standard_dtcs = false, \
        .restore_adapter_after_manufacturer_extension = false, \
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
    LinkObd2DtcKind dtc_kind;
} LinkDiagnosticFlowAction;

typedef struct {
    LinkDiagnosticFlowEventKind kind;
    LinkObd2DtcKind dtc_kind;
    const LinkObd2DtcList *dtc_list;
    const char *vin;
    bool vin_available;
    LinkObd2Sample sample;
    bool became_ready;
    bool dtc_response_available;
    bool dtc_negative_response;
    uint8_t dtc_negative_response_code;
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
    char standard_vin[LINK_OBD2_VIN_LENGTH + 1U];
    bool standard_vin_attempted;
    bool standard_vin_available;
    LinkObd2DtcList stored_dtcs;
    LinkObd2DtcList pending_dtcs;
    LinkObd2DtcList permanent_dtcs;
    LinkScheduler scheduler;
    uint8_t supported_pid_base;
    size_t active_schedule_index;
    uint8_t active_pid;
    bool awaiting_response;
    bool standard_dtc_inventory_complete;
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
 * Resume the standard sequence after product/manufacturer discovery.  If the
 * configuration requests restoration, the complete ELM initialisation sequence
 * runs again before the read-only standard DTC inventory.
 */
LinkDiagnosticFlowResult link_diagnostic_flow_resume_after_manufacturer(
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
const LinkObd2DtcList *link_diagnostic_flow_dtcs(
    const LinkDiagnosticFlow *flow,
    LinkObd2DtcKind kind);
const char *link_diagnostic_flow_adapter_identifier(
    const LinkDiagnosticFlow *flow);
const char *link_diagnostic_flow_standard_vin(
    const LinkDiagnosticFlow *flow);

#ifdef __cplusplus
}
#endif

#endif
