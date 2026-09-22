// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/uds_dtc_lifecycle.h"

#include <stdio.h>

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #c); \
    return 1; \
} } while (0)

static LinkUdsDtcLifecycleDefinition definition(void)
{
    LinkUdsDtcLifecycleDefinition value =
        LINK_UDS_DTC_LIFECYCLE_DEFINITION_INIT;
    value.code = UINT32_C(0x123456);
    value.functional_group_identifier = 0x33U;
    value.severity = 0x20U;
    value.functional_unit = 1U;
    value.increment_step = 64U;
    value.decrement_step = 64U;
    value.confirmation_threshold_cycles = 2U;
    value.aging_threshold_cycles = 3U;
    return value;
}

static int drive_failed_cycle(
    const LinkUdsDtcLifecycleDefinition *def,
    LinkUdsDtcLifecycleState *state)
{
    link_uds_dtc_lifecycle_begin_operation_cycle(state);
    CHECK(state->fault_detection_counter == 0);

    CHECK(link_uds_dtc_lifecycle_report_test(
        def, state, LINK_UDS_DTC_TEST_FAILED));
    CHECK(state->fault_detection_counter == 64);
    CHECK(!state->failed_this_cycle);
    CHECK((state->status & LINK_UDS_DTC_STATUS_TEST_FAILED) == 0U);

    CHECK(link_uds_dtc_lifecycle_report_test(
        def, state, LINK_UDS_DTC_TEST_FAILED));
    CHECK(state->fault_detection_counter == 127);
    CHECK(state->failed_this_cycle);
    CHECK((state->status & LINK_UDS_DTC_STATUS_TEST_FAILED) != 0U);
    CHECK((state->status & LINK_UDS_DTC_STATUS_PENDING_DTC) != 0U);

    CHECK(link_uds_dtc_lifecycle_end_operation_cycle(def, state));
    return 0;
}

static int drive_passed_cycle(
    const LinkUdsDtcLifecycleDefinition *def,
    LinkUdsDtcLifecycleState *state)
{
    link_uds_dtc_lifecycle_begin_operation_cycle(state);
    CHECK(state->fault_detection_counter == 0);

    CHECK(link_uds_dtc_lifecycle_report_test(
        def, state, LINK_UDS_DTC_TEST_PASSED));
    CHECK(state->fault_detection_counter == -64);
    CHECK(!state->passed_this_cycle);

    CHECK(link_uds_dtc_lifecycle_report_test(
        def, state, LINK_UDS_DTC_TEST_PASSED));
    CHECK(state->fault_detection_counter == -128);
    CHECK(state->passed_this_cycle);

    CHECK(link_uds_dtc_lifecycle_end_operation_cycle(def, state));
    return 0;
}

static int test_prefailed_reporting_confirmation_and_aging(void)
{
    LinkUdsDtcLifecycleDefinition def = definition();
    LinkUdsDtcLifecycleState state = LINK_UDS_DTC_LIFECYCLE_STATE_INIT;
    uint8_t counter = 0U;

    CHECK(link_uds_dtc_lifecycle_definition_valid(&def));
    CHECK(state.status == 0x50U);
    CHECK(!link_uds_dtc_lifecycle_reportable_fault_counter(&state, &counter));

    /*
     * A partial failed monitor execution produces a reportable positive FDC,
     * but does not yet assert testFailed/pending.
     */
    link_uds_dtc_lifecycle_begin_operation_cycle(&state);
    CHECK(link_uds_dtc_lifecycle_report_test(
        &def, &state, LINK_UDS_DTC_TEST_FAILED));
    CHECK(state.fault_detection_counter == 64);
    CHECK(link_uds_dtc_lifecycle_reportable_fault_counter(&state, &counter));
    CHECK(counter == 64U);
    CHECK((state.status & LINK_UDS_DTC_STATUS_TEST_FAILED) == 0U);
    CHECK(link_uds_dtc_lifecycle_end_operation_cycle(&def, &state));
    CHECK(state.failure_cycle_count == 0U);

    /* First complete failed cycle reaches +127 and becomes pending. */
    CHECK(drive_failed_cycle(&def, &state) == 0);
    CHECK(state.failure_cycle_count == 1U);
    CHECK((state.status & LINK_UDS_DTC_STATUS_CONFIRMED_DTC) == 0U);
    CHECK(!link_uds_dtc_lifecycle_reportable_fault_counter(&state, &counter));

    /* A second failed cycle confirms it. FDC was reset at cycle start. */
    CHECK(drive_failed_cycle(&def, &state) == 0);
    CHECK(state.failure_cycle_count == 2U);
    CHECK((state.status & LINK_UDS_DTC_STATUS_CONFIRMED_DTC) != 0U);

    /* Three fully passed operation cycles age the confirmed DTC out. */
    CHECK(drive_passed_cycle(&def, &state) == 0);
    CHECK(state.aging_counter == 1U);
    CHECK((state.status & LINK_UDS_DTC_STATUS_CONFIRMED_DTC) != 0U);

    CHECK(drive_passed_cycle(&def, &state) == 0);
    CHECK(state.aging_counter == 2U);
    CHECK((state.status & LINK_UDS_DTC_STATUS_CONFIRMED_DTC) != 0U);

    CHECK(drive_passed_cycle(&def, &state) == 0);
    CHECK(state.aging_counter == 3U);
    CHECK((state.status & LINK_UDS_DTC_STATUS_CONFIRMED_DTC) == 0U);
    CHECK((state.status & LINK_UDS_DTC_STATUS_PENDING_DTC) == 0U);
    CHECK(state.fault_detection_counter == -128);

    link_uds_dtc_lifecycle_clear(&state);
    CHECK(state.status == 0x50U);
    CHECK(state.fault_detection_counter == 0);
    CHECK(state.aging_counter == 0U);
    CHECK(state.failure_cycle_count == 0U);
    return 0;
}

static int test_saturation_unexecuted_cycle_and_validation(void)
{
    LinkUdsDtcLifecycleDefinition def = definition();
    LinkUdsDtcLifecycleState state = LINK_UDS_DTC_LIFECYCLE_STATE_INIT;

    link_uds_dtc_lifecycle_begin_operation_cycle(&state);
    for (unsigned int i = 0U; i < 8U; ++i) {
        CHECK(link_uds_dtc_lifecycle_report_test(
            &def, &state, LINK_UDS_DTC_TEST_FAILED));
    }
    CHECK(state.fault_detection_counter == 127);

    CHECK(link_uds_dtc_lifecycle_end_operation_cycle(&def, &state));
    CHECK(state.failure_cycle_count == 1U);

    /* An operation cycle in which the monitor never runs must not age it. */
    link_uds_dtc_lifecycle_begin_operation_cycle(&state);
    CHECK(state.fault_detection_counter == 0);
    CHECK(link_uds_dtc_lifecycle_end_operation_cycle(&def, &state));
    CHECK(state.failure_cycle_count == 1U);
    CHECK(state.aging_counter == 0U);

    link_uds_dtc_lifecycle_begin_operation_cycle(&state);
    for (unsigned int i = 0U; i < 8U; ++i) {
        CHECK(link_uds_dtc_lifecycle_report_test(
            &def, &state, LINK_UDS_DTC_TEST_PASSED));
    }
    CHECK(state.fault_detection_counter == -128);

    def.increment_step = 0U;
    CHECK(!link_uds_dtc_lifecycle_definition_valid(&def));
    return 0;
}

int main(void)
{
    CHECK(test_prefailed_reporting_confirmation_and_aging() == 0);
    CHECK(test_saturation_unexecuted_cycle_and_validation() == 0);
    puts("UDS DTC lifecycle tests passed");
    return 0;
}
