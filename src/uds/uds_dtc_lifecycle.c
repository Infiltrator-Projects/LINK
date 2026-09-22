// SPDX-License-Identifier: GPL-3.0-or-later
/** @file uds_dtc_lifecycle.c @brief Configurable DTC fault lifecycle engine. */
#include "link/uds_dtc_lifecycle.h"

#include <limits.h>

static int8_t saturating_add(int8_t value, uint8_t step)
{
    int value_int = (int)value;
    int step_int = (int)step;
    if (value_int > INT8_MAX - step_int) return INT8_MAX;
    return (int8_t)(value_int + step_int);
}

static int8_t saturating_subtract(int8_t value, uint8_t step)
{
    int value_int = (int)value;
    int step_int = (int)step;
    if (value_int < INT8_MIN + step_int) return INT8_MIN;
    return (int8_t)(value_int - step_int);
}

bool link_uds_dtc_lifecycle_definition_valid(
    const LinkUdsDtcLifecycleDefinition *definition)
{
    return definition != NULL &&
           definition->code <= UINT32_C(0x00ffffff) &&
           definition->increment_step != 0U &&
           definition->decrement_step != 0U &&
           definition->confirmation_threshold_cycles != 0U;
}

void link_uds_dtc_lifecycle_clear(LinkUdsDtcLifecycleState *state)
{
    if (state == NULL) return;
    state->status =
        LINK_UDS_DTC_STATUS_TEST_NOT_COMPLETED_SINCE_LAST_CLEAR |
        LINK_UDS_DTC_STATUS_TEST_NOT_COMPLETED_THIS_OPERATION_CYCLE;
    state->fault_detection_counter = 0;
    state->aging_counter = 0U;
    state->failure_cycle_count = 0U;
    state->tested_this_cycle = false;
    state->failed_this_cycle = false;
    state->passed_this_cycle = false;
}

void link_uds_dtc_lifecycle_begin_operation_cycle(
    LinkUdsDtcLifecycleState *state)
{
    if (state == NULL) return;
    state->status &= (uint8_t)(UINT8_C(0xff) ^ LINK_UDS_DTC_STATUS_TEST_FAILED_THIS_OPERATION_CYCLE);
    state->status |= LINK_UDS_DTC_STATUS_TEST_NOT_COMPLETED_THIS_OPERATION_CYCLE;
    state->fault_detection_counter = 0;
    state->tested_this_cycle = false;
    state->failed_this_cycle = false;
    state->passed_this_cycle = false;
}

bool link_uds_dtc_lifecycle_report_test(
    const LinkUdsDtcLifecycleDefinition *definition,
    LinkUdsDtcLifecycleState *state,
    LinkUdsDtcTestResult result)
{
    if (!link_uds_dtc_lifecycle_definition_valid(definition) ||
        state == NULL ||
        (result != LINK_UDS_DTC_TEST_PASSED &&
         result != LINK_UDS_DTC_TEST_FAILED)) {
        return false;
    }

    state->tested_this_cycle = true;
    state->status &=
        (uint8_t)(UINT8_C(0xff) ^ LINK_UDS_DTC_STATUS_TEST_NOT_COMPLETED_THIS_OPERATION_CYCLE);
    state->status &=
        (uint8_t)(UINT8_C(0xff) ^ LINK_UDS_DTC_STATUS_TEST_NOT_COMPLETED_SINCE_LAST_CLEAR);

    if (result == LINK_UDS_DTC_TEST_FAILED) {
        state->fault_detection_counter = saturating_add(
            state->fault_detection_counter, definition->increment_step);
        state->aging_counter = 0U;
        if (state->fault_detection_counter == INT8_MAX) {
            state->failed_this_cycle = true;
            state->passed_this_cycle = false;
            state->status |=
                LINK_UDS_DTC_STATUS_TEST_FAILED |
                LINK_UDS_DTC_STATUS_TEST_FAILED_THIS_OPERATION_CYCLE |
                LINK_UDS_DTC_STATUS_PENDING_DTC |
                LINK_UDS_DTC_STATUS_TEST_FAILED_SINCE_LAST_CLEAR;
        }
        return true;
    }

    state->fault_detection_counter = saturating_subtract(
        state->fault_detection_counter, definition->decrement_step);
    if (state->fault_detection_counter == INT8_MIN) {
        state->passed_this_cycle = true;
        state->status &= (uint8_t)(UINT8_C(0xff) ^ LINK_UDS_DTC_STATUS_TEST_FAILED);
    }
    return true;
}

bool link_uds_dtc_lifecycle_end_operation_cycle(
    const LinkUdsDtcLifecycleDefinition *definition,
    LinkUdsDtcLifecycleState *state)
{
    if (!link_uds_dtc_lifecycle_definition_valid(definition) ||
        state == NULL) {
        return false;
    }

    if (state->failed_this_cycle) {
        state->aging_counter = 0U;
        if (state->failure_cycle_count != UINT8_MAX) {
            state->failure_cycle_count++;
        }
        if (state->failure_cycle_count >=
            definition->confirmation_threshold_cycles) {
            state->status |= LINK_UDS_DTC_STATUS_CONFIRMED_DTC;
        }
        return true;
    }

    if (!state->passed_this_cycle) return true;

    state->failure_cycle_count = 0U;
    state->status &= (uint8_t)(UINT8_C(0xff) ^ LINK_UDS_DTC_STATUS_PENDING_DTC);

    if ((state->status & LINK_UDS_DTC_STATUS_CONFIRMED_DTC) != 0U &&
        definition->aging_threshold_cycles != 0U) {
        if (state->aging_counter != UINT8_MAX) {
            state->aging_counter++;
        }
        if (state->aging_counter >= definition->aging_threshold_cycles) {
            state->status &= (uint8_t)(UINT8_C(0xff) ^ LINK_UDS_DTC_STATUS_CONFIRMED_DTC);
            state->aging_counter = definition->aging_threshold_cycles;
        }
    } else if ((state->status & LINK_UDS_DTC_STATUS_CONFIRMED_DTC) == 0U) {
        state->aging_counter = 0U;
    }

    return true;
}

bool link_uds_dtc_lifecycle_reportable_fault_counter(
    const LinkUdsDtcLifecycleState *state,
    uint8_t *counter)
{
    if (state == NULL || counter == NULL ||
        state->fault_detection_counter <= 0 ||
        state->fault_detection_counter >= INT8_MAX) {
        return false;
    }
    *counter = (uint8_t)state->fault_detection_counter;
    return true;
}
