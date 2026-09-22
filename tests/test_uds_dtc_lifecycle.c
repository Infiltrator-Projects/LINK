// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/uds_dtc_lifecycle.h"

#include <stdio.h>

#define CHECK(c) do { if (!(c)) {     fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #c);     return 1; } } while (0)

static LinkUdsDtcLifecycleDefinition definition(void)
{
    LinkUdsDtcLifecycleDefinition value =
        LINK_UDS_DTC_LIFECYCLE_DEFINITION_INIT;
    value.code = UINT32_C(0x123456);
    value.functional_group_identifier = 0x33U;
    value.severity = 0x20U;
    value.functional_unit = 1U;
    value.failed_threshold = 64;
    value.passed_threshold = -64;
    value.increment_step = 64U;
    value.decrement_step = 64U;
    value.confirmation_threshold_cycles = 2U;
    value.aging_threshold_cycles = 3U;
    return value;
}

static int test_fault_counter_confirmation_and_aging(void)
{
    LinkUdsDtcLifecycleDefinition def = definition();
    LinkUdsDtcLifecycleState state = LINK_UDS_DTC_LIFECYCLE_STATE_INIT;
    uint8_t counter = 0U;

    CHECK(link_uds_dtc_lifecycle_definition_valid(&def));
    CHECK(state.status == 0x50U);
    CHECK(!link_uds_dtc_lifecycle_reportable_fault_counter(&state, &counter));

    link_uds_dtc_lifecycle_begin_operation_cycle(&state);
    CHECK(link_uds_dtc_lifecycle_report_test(
        &def, &state, LINK_UDS_DTC_TEST_FAILED));
    CHECK(state.fault_detection_counter == 64);
    CHECK((state.status & LINK_UDS_DTC_STATUS_PENDING_DTC) != 0U);
    CHECK((state.status & LINK_UDS_DTC_STATUS_CONFIRMED_DTC) == 0U);
    CHECK(link_uds_dtc_lifecycle_reportable_fault_counter(&state, &counter));
    CHECK(counter == 64U);
    CHECK(link_uds_dtc_lifecycle_end_operation_cycle(&def, &state));
    CHECK(state.failure_cycle_count == 1U);

    link_uds_dtc_lifecycle_begin_operation_cycle(&state);
    CHECK(link_uds_dtc_lifecycle_report_test(
        &def, &state, LINK_UDS_DTC_TEST_FAILED));
    CHECK(state.fault_detection_counter == 127);
    CHECK(!link_uds_dtc_lifecycle_reportable_fault_counter(&state, &counter));
    CHECK(link_uds_dtc_lifecycle_end_operation_cycle(&def, &state));
    CHECK((state.status & LINK_UDS_DTC_STATUS_CONFIRMED_DTC) != 0U);

    /* Three completed pass cycles age the confirmed DTC out. */
    for (unsigned int cycle = 0U; cycle < 3U; ++cycle) {
        link_uds_dtc_lifecycle_begin_operation_cycle(&state);
        CHECK(link_uds_dtc_lifecycle_report_test(
            &def, &state, LINK_UDS_DTC_TEST_PASSED));
        CHECK(link_uds_dtc_lifecycle_end_operation_cycle(&def, &state));
        CHECK((state.status & LINK_UDS_DTC_STATUS_PENDING_DTC) == 0U);
    }
    CHECK(state.fault_detection_counter == -65);
    CHECK(state.aging_counter == 3U);
    CHECK((state.status & LINK_UDS_DTC_STATUS_CONFIRMED_DTC) == 0U);

    link_uds_dtc_lifecycle_clear(&state);
    CHECK(state.status == 0x50U);
    CHECK(state.fault_detection_counter == 0);
    CHECK(state.aging_counter == 0U);
    CHECK(state.failure_cycle_count == 0U);
    return 0;
}

static int test_saturation_and_unexecuted_cycle(void)
{
    LinkUdsDtcLifecycleDefinition def = definition();
    LinkUdsDtcLifecycleState state = LINK_UDS_DTC_LIFECYCLE_STATE_INIT;

    for (unsigned int i = 0U; i < 8U; ++i) {
        CHECK(link_uds_dtc_lifecycle_report_test(
            &def, &state, LINK_UDS_DTC_TEST_FAILED));
    }
    CHECK(state.fault_detection_counter == 127);

    link_uds_dtc_lifecycle_begin_operation_cycle(&state);
    CHECK(link_uds_dtc_lifecycle_end_operation_cycle(&def, &state));
    CHECK(state.aging_counter == 0U);

    for (unsigned int i = 0U; i < 8U; ++i) {
        CHECK(link_uds_dtc_lifecycle_report_test(
            &def, &state, LINK_UDS_DTC_TEST_PASSED));
    }
    CHECK(state.fault_detection_counter == -128);
    return 0;
}

int main(void)
{
    CHECK(test_fault_counter_confirmation_and_aging() == 0);
    CHECK(test_saturation_and_unexecuted_cycle() == 0);
    puts("UDS DTC lifecycle tests passed");
    return 0;
}
