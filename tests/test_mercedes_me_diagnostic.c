// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/mercedes_me_diagnostic.h"

#include <stdio.h>
#include <string.h>

#define CHECK(e) do { \
    if (!(e)) { \
        fprintf(stderr, "check failed: %s at %s:%d\n", #e, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

int main(void)
{
    static const uint8_t request_payload[] = { 0x22U, 0xf1U, 0x90U };
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
        .response_selection =
            LINK_DIAGNOSTIC_RESPONSE_SELECT_LOWEST_CAN_ID_CACHED
    };
    LinkMercedesMeDiagnosticPlan plan;

    CHECK(link_mercedes_me_build_diagnostic_plan(
              &request, true, &plan) == LINK_MERCEDES_ME_NATIVE_OK);
    CHECK(plan.config_command_size == 16U);
    CHECK(memcmp(plan.config_command,
                 "I0107E007E800AA\r", 16U) == 0);
    CHECK(plan.transaction_command_size == 12U);
    CHECK(memcmp(plan.transaction_command,
                 "i0107E0IvGQ\r", 12U) == 0);

    request.padding_enabled = true;
    request.padding = 0xCCU;
    CHECK(link_mercedes_me_build_diagnostic_plan(
              &request, false, &plan) == LINK_MERCEDES_ME_NATIVE_OK);
    CHECK(memcmp(plan.config_command,
                 "I0107E007E881CC\r", 16U) == 0);

    request.extended_id = true;
    request.request_can_id = UINT32_C(0x18daf110);
    request.response_can_id = UINT32_C(0x18da10f1);
    CHECK(link_mercedes_me_build_diagnostic_plan(
              &request, true, &plan) == LINK_MERCEDES_ME_NATIVE_RANGE);

    request.extended_id = false;
    request.request_can_id = UINT32_C(0x7e0);
    request.response_can_id = UINT32_C(0x7e8);
    request.response_can_id_known = false;
    CHECK(link_mercedes_me_build_diagnostic_plan(
              &request, true, &plan) == LINK_MERCEDES_ME_NATIVE_RANGE);

    return 0;
}
