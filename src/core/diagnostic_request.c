// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/diagnostic_request.h"

#include <string.h>

static bool policy_valid(LinkDiagnosticResponseSelectionPolicy policy)
{
    return policy >= LINK_DIAGNOSTIC_RESPONSE_SELECT_FIRST &&
           policy <= LINK_DIAGNOSTIC_RESPONSE_MERGE_ELIMINATE_DUPLICATES;
}

static bool payload_equal(const LinkDiagnosticResponseView *left,
                          const LinkDiagnosticResponseView *right)
{
    if (left == NULL || right == NULL ||
        left->payload_size != right->payload_size) {
        return false;
    }
    if (left->payload_size == 0U) return true;
    if (left->payload == NULL || right->payload == NULL) return false;
    return memcmp(left->payload, right->payload, left->payload_size) == 0;
}

static void selection_clear(LinkDiagnosticResponseSelection *selection)
{
    if (selection != NULL) memset(selection, 0, sizeof(*selection));
}

const char *link_diagnostic_request_result_name(
    LinkDiagnosticRequestResult result)
{
    switch (result) {
    case LINK_DIAGNOSTIC_REQUEST_OK: return "ok";
    case LINK_DIAGNOSTIC_REQUEST_INVALID_ARGUMENT: return "invalid-argument";
    case LINK_DIAGNOSTIC_REQUEST_RANGE: return "range";
    case LINK_DIAGNOSTIC_REQUEST_UNSUPPORTED: return "unsupported";
    case LINK_DIAGNOSTIC_REQUEST_NO_RESPONSE: return "no-response";
    case LINK_DIAGNOSTIC_REQUEST_NO_NUMERIC_VALUE: return "no-numeric-value";
    }
    return "unknown";
}

const char *link_diagnostic_execution_mode_name(
    LinkDiagnosticExecutionMode mode)
{
    switch (mode) {
    case LINK_DIAGNOSTIC_EXECUTION_UNSUPPORTED: return "unsupported";
    case LINK_DIAGNOSTIC_EXECUTION_ELM_COMMAND_SURFACE:
        return "elm-command-surface";
    case LINK_DIAGNOSTIC_EXECUTION_NATIVE_ISOTP: return "native-isotp";
    }
    return "unknown";
}

const char *link_diagnostic_response_selection_name(
    LinkDiagnosticResponseSelectionPolicy policy)
{
    switch (policy) {
    case LINK_DIAGNOSTIC_RESPONSE_SELECT_FIRST: return "select-first";
    case LINK_DIAGNOSTIC_RESPONSE_SELECT_LOWEST_CAN_ID_CACHED:
        return "select-lowest-can-id-cached";
    case LINK_DIAGNOSTIC_RESPONSE_SELECT_MAXIMUM: return "select-maximum";
    case LINK_DIAGNOSTIC_RESPONSE_MERGE_ELIMINATE_DUPLICATES:
        return "merge-eliminate-duplicates";
    }
    return "unknown";
}

LinkDiagnosticExecutionMode link_diagnostic_execution_mode_for_adapter(
    LinkAdapterKind kind)
{
    LinkAdapterCapabilities capabilities;

    if (!link_adapter_capabilities(kind, &capabilities))
        return LINK_DIAGNOSTIC_EXECUTION_UNSUPPORTED;
    if ((capabilities.flags & LINK_ADAPTER_CAP_NATIVE_DIAGNOSTIC) != 0U &&
        (capabilities.flags & LINK_ADAPTER_CAP_ISOTP) != 0U) {
        return LINK_DIAGNOSTIC_EXECUTION_NATIVE_ISOTP;
    }
    if ((capabilities.flags & LINK_ADAPTER_CAP_ELM_COMMAND_SURFACE) != 0U)
        return LINK_DIAGNOSTIC_EXECUTION_ELM_COMMAND_SURFACE;
    return LINK_DIAGNOSTIC_EXECUTION_UNSUPPORTED;
}

bool link_diagnostic_request_is_valid(
    const LinkDiagnosticRequestDefinition *request)
{
    if (request == NULL || request->payload == NULL ||
        request->payload_size == 0U ||
        request->request_can_id > LINK_DIAGNOSTIC_MAX_CAN_ID ||
        (request->response_can_id_known &&
         request->response_can_id > LINK_DIAGNOSTIC_MAX_CAN_ID) ||
        !policy_valid(request->response_selection)) {
        return false;
    }

    if (!request->extended_id &&
        request->request_can_id > UINT32_C(0x7ff)) {
        return false;
    }
    if (!request->extended_id && request->response_can_id_known &&
        request->response_can_id > UINT32_C(0x7ff)) {
        return false;
    }
    return true;
}

LinkDiagnosticRequestResult link_diagnostic_request_supported_by_adapter(
    const LinkDiagnosticRequestDefinition *request,
    LinkAdapterKind kind)
{
    LinkAdapterCapabilities capabilities;
    uint32_t required_can_flag;

    if (!link_diagnostic_request_is_valid(request))
        return LINK_DIAGNOSTIC_REQUEST_INVALID_ARGUMENT;
    if (!link_adapter_capabilities(kind, &capabilities))
        return LINK_DIAGNOSTIC_REQUEST_UNSUPPORTED;

    required_can_flag = request->extended_id
        ? LINK_ADAPTER_CAP_CAN_29BIT : LINK_ADAPTER_CAP_CAN_11BIT;

    if ((capabilities.flags & LINK_ADAPTER_CAP_ISOTP) == 0U ||
        (capabilities.flags & required_can_flag) == 0U) {
        return LINK_DIAGNOSTIC_REQUEST_UNSUPPORTED;
    }
    if (capabilities.max_isotp_payload != 0U &&
        request->payload_size > capabilities.max_isotp_payload) {
        return LINK_DIAGNOSTIC_REQUEST_RANGE;
    }
    return LINK_DIAGNOSTIC_REQUEST_OK;
}

static void selection_one(LinkDiagnosticResponseSelection *selection,
                          size_t index,
                          const LinkDiagnosticResponseView *response)
{
    selection->indices[0] = index;
    selection->count = 1U;
    if (response->can_id_available) {
        selection->selected_can_id_available = true;
        selection->selected_can_id = response->can_id;
    }
}

LinkDiagnosticRequestResult link_diagnostic_select_responses(
    const LinkDiagnosticResponseView *responses,
    size_t response_count,
    LinkDiagnosticResponseSelectionPolicy policy,
    bool cached_can_id_available,
    uint32_t cached_can_id,
    LinkDiagnosticResponseNumericValueFn numeric_value,
    void *numeric_context,
    LinkDiagnosticResponseSelection *selection)
{
    size_t index;

    if (selection == NULL || !policy_valid(policy) ||
        (response_count != 0U && responses == NULL) ||
        response_count > LINK_DIAGNOSTIC_MAX_RESPONSES ||
        (cached_can_id_available &&
         cached_can_id > LINK_DIAGNOSTIC_MAX_CAN_ID)) {
        return LINK_DIAGNOSTIC_REQUEST_INVALID_ARGUMENT;
    }

    selection_clear(selection);
    if (response_count == 0U) return LINK_DIAGNOSTIC_REQUEST_NO_RESPONSE;

    if (policy == LINK_DIAGNOSTIC_RESPONSE_SELECT_FIRST) {
        selection_one(selection, 0U, &responses[0]);
        return LINK_DIAGNOSTIC_REQUEST_OK;
    }

    if (policy == LINK_DIAGNOSTIC_RESPONSE_SELECT_LOWEST_CAN_ID_CACHED) {
        if (cached_can_id_available) {
            for (index = 0U; index < response_count; ++index) {
                if (responses[index].can_id_available &&
                    responses[index].can_id == cached_can_id) {
                    selection_one(selection, index, &responses[index]);
                    selection->cached_can_id_used = true;
                    return LINK_DIAGNOSTIC_REQUEST_OK;
                }
            }
        }

        {
            size_t best = 0U;
            bool found = false;
            uint32_t lowest = 0U;

            for (index = 0U; index < response_count; ++index) {
                if (!responses[index].can_id_available) continue;
                if (!found || responses[index].can_id < lowest) {
                    best = index;
                    lowest = responses[index].can_id;
                    found = true;
                }
            }
            if (found) selection_one(selection, best, &responses[best]);
            else selection_one(selection, 0U, &responses[0]);
        }
        return LINK_DIAGNOSTIC_REQUEST_OK;
    }

    if (policy == LINK_DIAGNOSTIC_RESPONSE_SELECT_MAXIMUM) {
        bool found = false;
        size_t best = 0U;
        double best_value = 0.0;

        if (numeric_value == NULL)
            return LINK_DIAGNOSTIC_REQUEST_INVALID_ARGUMENT;

        for (index = 0U; index < response_count; ++index) {
            double value = 0.0;
            if (!numeric_value(
                    numeric_context, &responses[index], &value)) {
                continue;
            }
            if (!found || value > best_value) {
                found = true;
                best = index;
                best_value = value;
            }
        }
        if (!found) return LINK_DIAGNOSTIC_REQUEST_NO_NUMERIC_VALUE;
        selection_one(selection, best, &responses[best]);
        return LINK_DIAGNOSTIC_REQUEST_OK;
    }

    for (index = 0U; index < response_count; ++index) {
        size_t previous;
        bool duplicate = false;

        for (previous = 0U; previous < selection->count; ++previous) {
            if (payload_equal(
                    &responses[index],
                    &responses[selection->indices[previous]])) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) selection->indices[selection->count++] = index;
    }

    return selection->count != 0U
        ? LINK_DIAGNOSTIC_REQUEST_OK
        : LINK_DIAGNOSTIC_REQUEST_NO_RESPONSE;
}
