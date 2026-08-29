// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/diagnostic_request.h"

#include <stdio.h>

#define CHECK(e) do { \
    if (!(e)) { \
        fprintf(stderr, "check failed: %s at %s:%d\n", #e, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

static bool numeric_value(
    void *context,
    const LinkDiagnosticResponseView *response,
    double *value)
{
    (void)context;
    if (response == NULL || value == NULL ||
        response->payload == NULL || response->payload_size == 0U) {
        return false;
    }
    *value = (double)response->payload[0];
    return true;
}

int main(void)
{
    static const uint8_t request_payload[] = { 0x22U, 0xf1U, 0x90U };
    static const uint8_t payload_a[] = { 1U, 2U };
    static const uint8_t payload_b[] = { 3U };
    static const uint8_t payload_duplicate[] = { 1U, 2U };
    LinkDiagnosticRequestDefinition request = {
        .request_can_id = UINT32_C(0x7e0),
        .response_can_id = UINT32_C(0x7e8),
        .response_can_id_known = true,
        .extended_id = false,
        .payload = request_payload,
        .payload_size = sizeof(request_payload),
        .timeout_ms = 1500U,
        .retry_interval_ms = 0U,
        .p2_star_ms = 0U,
        .p3_ms = 0U,
        .padding_enabled = false,
        .padding = 0U,
        .response_selection = LINK_DIAGNOSTIC_RESPONSE_SELECT_FIRST
    };
    LinkDiagnosticResponseView responses[] = {
        { UINT32_C(0x7e9), true, payload_a, sizeof(payload_a), UINT64_C(10) },
        { UINT32_C(0x7e8), true, payload_b, sizeof(payload_b), UINT64_C(11) },
        { UINT32_C(0x7ea), true, payload_duplicate,
          sizeof(payload_duplicate), UINT64_C(12) }
    };
    LinkDiagnosticResponseSelection selection;

    CHECK(link_diagnostic_request_is_valid(&request));
    CHECK(link_diagnostic_request_supported_by_adapter(
              &request, LINK_ADAPTER_KIND_MERCEDES_ME_NATIVE) ==
          LINK_DIAGNOSTIC_REQUEST_OK);

    CHECK(link_diagnostic_execution_mode_for_adapter(
              LINK_ADAPTER_KIND_ELM327) ==
          LINK_DIAGNOSTIC_EXECUTION_ELM_COMMAND_SURFACE);
    CHECK(link_diagnostic_execution_mode_for_adapter(
              LINK_ADAPTER_KIND_TACTRIX_OPENPORT2) ==
          LINK_DIAGNOSTIC_EXECUTION_ELM_COMMAND_SURFACE);
    CHECK(link_diagnostic_execution_mode_for_adapter(
              LINK_ADAPTER_KIND_MERCEDES_ME_NATIVE) ==
          LINK_DIAGNOSTIC_EXECUTION_NATIVE_ISOTP);
    CHECK(link_diagnostic_execution_mode_for_adapter(
              LINK_ADAPTER_KIND_STM32_LINK) ==
          LINK_DIAGNOSTIC_EXECUTION_NATIVE_ISOTP);

    CHECK(link_diagnostic_select_responses(
              responses, 3U,
              LINK_DIAGNOSTIC_RESPONSE_SELECT_LOWEST_CAN_ID_CACHED,
              false, 0U, NULL, NULL, &selection) ==
          LINK_DIAGNOSTIC_REQUEST_OK);
    CHECK(selection.count == 1U);
    CHECK(selection.indices[0] == 1U);
    CHECK(selection.selected_can_id_available);
    CHECK(selection.selected_can_id == UINT32_C(0x7e8));

    CHECK(link_diagnostic_select_responses(
              responses, 3U,
              LINK_DIAGNOSTIC_RESPONSE_SELECT_LOWEST_CAN_ID_CACHED,
              true, UINT32_C(0x7e9), NULL, NULL, &selection) ==
          LINK_DIAGNOSTIC_REQUEST_OK);
    CHECK(selection.cached_can_id_used);
    CHECK(selection.indices[0] == 0U);

    CHECK(link_diagnostic_select_responses(
              responses, 3U,
              LINK_DIAGNOSTIC_RESPONSE_SELECT_MAXIMUM,
              false, 0U, numeric_value, NULL, &selection) ==
          LINK_DIAGNOSTIC_REQUEST_OK);
    CHECK(selection.indices[0] == 1U);

    CHECK(link_diagnostic_select_responses(
              responses, 3U,
              LINK_DIAGNOSTIC_RESPONSE_MERGE_ELIMINATE_DUPLICATES,
              false, 0U, NULL, NULL, &selection) ==
          LINK_DIAGNOSTIC_REQUEST_OK);
    CHECK(selection.count == 2U);
    CHECK(selection.indices[0] == 0U);
    CHECK(selection.indices[1] == 1U);

    request.extended_id = true;
    request.request_can_id = UINT32_C(0x18daf110);
    request.response_can_id = UINT32_C(0x18da10f1);
    CHECK(link_diagnostic_request_supported_by_adapter(
              &request, LINK_ADAPTER_KIND_MERCEDES_ME_NATIVE) ==
          LINK_DIAGNOSTIC_REQUEST_UNSUPPORTED);
    CHECK(link_diagnostic_request_supported_by_adapter(
              &request, LINK_ADAPTER_KIND_TACTRIX_OPENPORT2) ==
          LINK_DIAGNOSTIC_REQUEST_OK);

    return 0;
}
