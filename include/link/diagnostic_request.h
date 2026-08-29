// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file diagnostic_request.h
 * @brief Transport-neutral ISO-TP request and multi-response selection policy.
 *
 * Manufacturer/product layers describe what should be requested here; adapter
 * providers decide how that request reaches the bus. This keeps Mercedes,
 * Jaguar and future vehicle definitions independent of ELM/J2534/native MCU
 * command syntax.
 */
#ifndef LINK_DIAGNOSTIC_REQUEST_H
#define LINK_DIAGNOSTIC_REQUEST_H

#include "link/transport.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LINK_DIAGNOSTIC_MAX_RESPONSES 32U
#define LINK_DIAGNOSTIC_MAX_CAN_ID UINT32_C(0x1fffffff)

typedef enum LinkDiagnosticRequestResult {
    LINK_DIAGNOSTIC_REQUEST_OK = 0,
    LINK_DIAGNOSTIC_REQUEST_INVALID_ARGUMENT,
    LINK_DIAGNOSTIC_REQUEST_RANGE,
    LINK_DIAGNOSTIC_REQUEST_UNSUPPORTED,
    LINK_DIAGNOSTIC_REQUEST_NO_RESPONSE,
    LINK_DIAGNOSTIC_REQUEST_NO_NUMERIC_VALUE
} LinkDiagnosticRequestResult;

typedef enum LinkDiagnosticExecutionMode {
    LINK_DIAGNOSTIC_EXECUTION_UNSUPPORTED = 0,
    LINK_DIAGNOSTIC_EXECUTION_ELM_COMMAND_SURFACE,
    LINK_DIAGNOSTIC_EXECUTION_NATIVE_ISOTP
} LinkDiagnosticExecutionMode;

typedef enum LinkDiagnosticResponseSelectionPolicy {
    LINK_DIAGNOSTIC_RESPONSE_SELECT_FIRST = 0,
    LINK_DIAGNOSTIC_RESPONSE_SELECT_LOWEST_CAN_ID_CACHED,
    LINK_DIAGNOSTIC_RESPONSE_SELECT_MAXIMUM,
    LINK_DIAGNOSTIC_RESPONSE_MERGE_ELIMINATE_DUPLICATES
} LinkDiagnosticResponseSelectionPolicy;

typedef struct LinkDiagnosticRequestDefinition {
    uint32_t request_can_id;
    uint32_t response_can_id;
    bool response_can_id_known;
    bool extended_id;
    const uint8_t *payload;
    size_t payload_size;
    uint32_t timeout_ms;
    uint32_t retry_interval_ms;
    uint32_t p2_star_ms;
    uint32_t p3_ms;
    bool padding_enabled;
    uint8_t padding;
    LinkDiagnosticResponseSelectionPolicy response_selection;
} LinkDiagnosticRequestDefinition;

typedef struct LinkDiagnosticResponseView {
    uint32_t can_id;
    bool can_id_available;
    const uint8_t *payload;
    size_t payload_size;
    uint64_t timestamp_ms;
} LinkDiagnosticResponseView;

typedef bool (*LinkDiagnosticResponseNumericValueFn)(
    void *context,
    const LinkDiagnosticResponseView *response,
    double *value);

typedef struct LinkDiagnosticResponseSelection {
    size_t indices[LINK_DIAGNOSTIC_MAX_RESPONSES];
    size_t count;
    bool cached_can_id_used;
    bool selected_can_id_available;
    uint32_t selected_can_id;
} LinkDiagnosticResponseSelection;

const char *link_diagnostic_request_result_name(
    LinkDiagnosticRequestResult result);
const char *link_diagnostic_execution_mode_name(
    LinkDiagnosticExecutionMode mode);
const char *link_diagnostic_response_selection_name(
    LinkDiagnosticResponseSelectionPolicy policy);

/** Choose the strongest execution path LINK currently exposes for this kind. */
LinkDiagnosticExecutionMode link_diagnostic_execution_mode_for_adapter(
    LinkAdapterKind kind);

bool link_diagnostic_request_is_valid(
    const LinkDiagnosticRequestDefinition *request);

/**
 * Check only proven adapter/provider constraints. A zero numeric capability
 * limit means "not established here" rather than unlimited hardware.
 */
LinkDiagnosticRequestResult link_diagnostic_request_supported_by_adapter(
    const LinkDiagnosticRequestDefinition *request,
    LinkAdapterKind kind);

/**
 * Apply a transport-independent response policy.
 *
 * SELECT_LOWEST_CAN_ID_CACHED reuses the cached responder when present and
 * otherwise selects the lowest available CAN identifier. SELECT_MAXIMUM uses
 * the caller's decoder so LINK never guesses a physical value from raw bytes.
 * MERGE_ELIMINATE_DUPLICATES retains the first copy of each byte-identical
 * payload, preserving input order.
 */
LinkDiagnosticRequestResult link_diagnostic_select_responses(
    const LinkDiagnosticResponseView *responses,
    size_t response_count,
    LinkDiagnosticResponseSelectionPolicy policy,
    bool cached_can_id_available,
    uint32_t cached_can_id,
    LinkDiagnosticResponseNumericValueFn numeric_value,
    void *numeric_context,
    LinkDiagnosticResponseSelection *selection);

#ifdef __cplusplus
}
#endif
#endif
