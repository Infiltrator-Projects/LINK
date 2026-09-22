// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file uds_dtc_lifecycle.h
 * @brief Portable fault-debounce, confirmation and aging reference engine.
 *
 * ISO 14229 defines externally visible DTC status/counter semantics but leaves
 * the internal fault-detection algorithm vehicle-manufacturer specific. This
 * module therefore provides a configurable allocation-free reference engine;
 * products remain free to replace it with their own diagnostic monitor.
 */
#ifndef LINK_UDS_DTC_LIFECYCLE_H
#define LINK_UDS_DTC_LIFECYCLE_H

#include "link/uds_dtc.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LINK_UDS_DTC_TEST_PASSED = 0,
    LINK_UDS_DTC_TEST_FAILED = 1
} LinkUdsDtcTestResult;

typedef struct {
    uint32_t code;
    uint8_t functional_group_identifier;
    uint8_t severity;
    uint8_t functional_unit;

    /*
     * Step size for LINK's ISO-scaled reference counter. The wire-scale
     * thresholds remain fixed at +127 failed and -128 passed.
     */
    uint8_t increment_step;
    uint8_t decrement_step;

    /* Operation-cycle confirmation and healing policy. */
    uint8_t confirmation_threshold_cycles;
    uint8_t aging_threshold_cycles;
} LinkUdsDtcLifecycleDefinition;

#define LINK_UDS_DTC_LIFECYCLE_DEFINITION_INIT {     0U, 0U, 0U, 0U,     INT8_C(127), INT8_C(-128),     1U, 1U, 1U, 0U }

typedef struct {
    uint8_t status;
    int8_t fault_detection_counter;
    uint8_t aging_counter;
    uint8_t failure_cycle_count;
    bool tested_this_cycle;
    bool failed_this_cycle;
    bool passed_this_cycle;
} LinkUdsDtcLifecycleState;

#define LINK_UDS_DTC_LIFECYCLE_STATE_INIT {     LINK_UDS_DTC_STATUS_TEST_NOT_COMPLETED_SINCE_LAST_CLEAR |         LINK_UDS_DTC_STATUS_TEST_NOT_COMPLETED_THIS_OPERATION_CYCLE,     0, 0U, 0U, false, false }

bool link_uds_dtc_lifecycle_definition_valid(
    const LinkUdsDtcLifecycleDefinition *definition);

void link_uds_dtc_lifecycle_clear(LinkUdsDtcLifecycleState *state);

void link_uds_dtc_lifecycle_begin_operation_cycle(
    LinkUdsDtcLifecycleState *state);

bool link_uds_dtc_lifecycle_report_test(
    const LinkUdsDtcLifecycleDefinition *definition,
    LinkUdsDtcLifecycleState *state,
    LinkUdsDtcTestResult result);

bool link_uds_dtc_lifecycle_end_operation_cycle(
    const LinkUdsDtcLifecycleDefinition *definition,
    LinkUdsDtcLifecycleState *state);

/**
 * Return the wire-format byte for 0x19/0x14 when this DTC is reportable.
 *
 * ISO 14229 reportDTCFaultDetectionCounter includes only positive prefailed
 * values 1..126. +127 represents fully failed and is not included; zero and
 * negative internal values are likewise omitted.
 */
bool link_uds_dtc_lifecycle_reportable_fault_counter(
    const LinkUdsDtcLifecycleState *state,
    uint8_t *counter);

#ifdef __cplusplus
}
#endif

#endif
