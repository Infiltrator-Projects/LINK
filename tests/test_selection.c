// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/selection.h"
#include "link/scheduler.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL: %s\n", #x); return 1; } } while (0)

static LinkParameterKey obd(uint8_t pid)
{
    LinkParameterKey key = {
        LINK_PARAMETER_PROTOCOL_OBD2,
        LINK_PARAMETER_MODULE_STANDARD_OBD2,
        (uint32_t)pid
    };
    return key;
}

int main(void)
{
    LinkParameterSelection selection;
    LinkParameterKey rpm = obd(UINT8_C(0x0c));
    LinkParameterKey speed = obd(UINT8_C(0x0d));
    LinkParameterKey uds = {
        LINK_PARAMETER_PROTOCOL_UDS,
        UINT32_C(0x7e1),
        UINT32_C(0xf190)
    };
    uint8_t pids[8] = {0};

    CHECK(link_parameter_selection_init(&selection, ""));
    CHECK(selection.count == 0U);
    CHECK(link_parameter_selection_set_vehicle(
        &selection, "WDD2073032F000001"));
    CHECK(strcmp(
        selection.vehicle_identifier, "WDD2073032F000001") == 0);

    CHECK(link_parameter_selection_set(
        &selection, &rpm, true) == LINK_PARAMETER_SELECTION_RESULT_OK);
    CHECK(link_parameter_selection_set(
        &selection, &speed, true) == LINK_PARAMETER_SELECTION_RESULT_OK);
    CHECK(link_parameter_selection_set(
        &selection, &uds, true) == LINK_PARAMETER_SELECTION_RESULT_OK);
    CHECK(link_parameter_selection_contains(&selection, &rpm));
    CHECK(link_parameter_selection_contains(&selection, &uds));
    CHECK(link_parameter_selection_count(&selection) == 3U);
    CHECK(link_parameter_selection_copy_obd2_pids(
        &selection, pids, sizeof(pids)) == 2U);
    CHECK(pids[0] == UINT8_C(0x0c));
    CHECK(pids[1] == UINT8_C(0x0d));

    CHECK(link_parameter_selection_set(
        &selection, &rpm, false) == LINK_PARAMETER_SELECTION_RESULT_OK);
    CHECK(!link_parameter_selection_contains(&selection, &rpm));
    CHECK(link_parameter_selection_count(&selection) == 2U);

    CHECK(link_parameter_selection_set_vehicle(
        &selection, "WDD2073032F000002"));
    CHECK(link_parameter_selection_count(&selection) == 0U);
    CHECK(!link_parameter_selection_contains(&selection, &speed));

    CHECK(!link_parameter_selection_set_vehicle(
        &selection,
        "THIS-VEHICLE-IDENTIFIER-IS-DELIBERATELY-LONGER-THAN-THE-SHARED-CAPACITY-AND-MUST-FAIL"));

    {
        LinkPollingPolicy policy;
        LinkScheduler scheduler;
        const uint8_t source_pid = UINT8_C(0x01);
        const uint64_t first_field = UINT64_C(1) << 0U;
        const uint64_t second_field = UINT64_C(1) << 18U;

        link_polling_policy_init(&policy, false);
        link_scheduler_init(&scheduler);
        CHECK(link_scheduler_add(
            &scheduler, source_pid, 1000U,
            LINK_SCHEDULER_PRIORITY_NORMAL, 0U) ==
            LINK_SCHEDULER_RESULT_OK);

        link_polling_policy_set_field_mask(&policy, source_pid, first_field);
        CHECK(link_polling_policy_is_enabled(&policy, source_pid));
        CHECK(link_polling_policy_field_mask(&policy, source_pid) == first_field);
        CHECK(link_polling_policy_apply_to_scheduler(&policy, &scheduler) == 1U);
        CHECK(link_scheduler_enabled_standard_count(&scheduler) == 1U);

        link_polling_policy_set_field_mask(
            &policy, source_pid, first_field | second_field);
        CHECK(link_polling_policy_apply_to_scheduler(&policy, &scheduler) == 1U);
        CHECK(link_scheduler_enabled_standard_count(&scheduler) == 1U);

        link_polling_policy_set_field_mask(&policy, source_pid, second_field);
        CHECK(link_polling_policy_apply_to_scheduler(&policy, &scheduler) == 1U);
        CHECK(link_scheduler_enabled_standard_count(&scheduler) == 1U);

        link_polling_policy_set_field_mask(&policy, source_pid, UINT64_C(0));
        CHECK(!link_polling_policy_is_enabled(&policy, source_pid));
        CHECK(link_polling_policy_apply_to_scheduler(&policy, &scheduler) == 0U);
        CHECK(link_scheduler_enabled_standard_count(&scheduler) == 0U);
    }

    puts("LINK vehicle-scoped parameter selection passed");
    return 0;
}
