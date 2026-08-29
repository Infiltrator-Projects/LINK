// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/mercedes_me_diagnostic.h"

#include <string.h>

LinkMercedesMeNativeResult link_mercedes_me_build_diagnostic_plan(
    const LinkDiagnosticRequestDefinition *request,
    bool allow_raw_can_responses,
    LinkMercedesMeDiagnosticPlan *plan)
{
    LinkDiagnosticRequestResult supported;
    LinkMercedesMeNativeResult result;
    int padding;

    if (plan == NULL || request == NULL)
        return LINK_MERCEDES_ME_NATIVE_INVALID_ARGUMENT;

    memset(plan, 0, sizeof(*plan));
    supported = link_diagnostic_request_supported_by_adapter(
        request, LINK_ADAPTER_KIND_MERCEDES_ME_NATIVE);
    if (supported == LINK_DIAGNOSTIC_REQUEST_INVALID_ARGUMENT)
        return LINK_MERCEDES_ME_NATIVE_INVALID_ARGUMENT;
    if (supported == LINK_DIAGNOSTIC_REQUEST_RANGE)
        return LINK_MERCEDES_ME_NATIVE_RANGE;
    if (supported != LINK_DIAGNOSTIC_REQUEST_OK)
        return LINK_MERCEDES_ME_NATIVE_RANGE;

    if (!request->response_can_id_known || request->extended_id)
        return LINK_MERCEDES_ME_NATIVE_RANGE;

    padding = request->padding_enabled
        ? (int)request->padding
        : LINK_MERCEDES_ME_ISOTP_PADDING_OFF;

    result = link_mercedes_me_build_isotp_config(
        request->request_can_id,
        request->response_can_id,
        allow_raw_can_responses ? 1 : 0,
        padding,
        plan->config_command,
        sizeof(plan->config_command),
        &plan->config_command_size);
    if (result != LINK_MERCEDES_ME_NATIVE_OK) {
        memset(plan, 0, sizeof(*plan));
        return result;
    }

    result = link_mercedes_me_build_isotp_transceive(
        request->request_can_id,
        request->payload,
        request->payload_size,
        plan->transaction_command,
        sizeof(plan->transaction_command),
        &plan->transaction_command_size);
    if (result != LINK_MERCEDES_ME_NATIVE_OK) {
        memset(plan, 0, sizeof(*plan));
        return result;
    }

    return LINK_MERCEDES_ME_NATIVE_OK;
}

LinkMercedesMeNativeResult link_mercedes_me_secure_diagnostic_command(
    const uint8_t session_key[LINK_MERCEDES_ME_SESSION_KEY_SIZE],
    const uint8_t *command,
    size_t command_size,
    uint8_t *wire,
    size_t wire_capacity,
    size_t *wire_size)
{
    if (session_key == NULL || command == NULL || command_size == 0U)
        return LINK_MERCEDES_ME_NATIVE_INVALID_ARGUMENT;
    return link_mercedes_me_secure_encode(
        session_key,
        command,
        command_size,
        wire,
        wire_capacity,
        wire_size);
}
