// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file fault_scan.h
 * @brief Product-neutral presentation state for diagnostic fault scans.
 *
 * An empty fault list is only clean when acquisition completed successfully.
 * Callers must preserve not-scanned, in-progress and failed outcomes rather
 * than presenting an empty or incomplete result as a clean vehicle.
 */
#ifndef LINK_FAULT_SCAN_H
#define LINK_FAULT_SCAN_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum LinkFaultScanPresentationState {
    LINK_FAULT_SCAN_NOT_SCANNED = 0,
    LINK_FAULT_SCAN_IN_PROGRESS,
    LINK_FAULT_SCAN_FAILED,
    LINK_FAULT_SCAN_CLEAN,
    LINK_FAULT_SCAN_FAULTS_PRESENT
} LinkFaultScanPresentationState;

static inline LinkFaultScanPresentationState
link_fault_scan_presentation_state(
    bool started,
    bool active,
    bool complete,
    bool failed,
    size_t fault_count)
{
    if (failed) return LINK_FAULT_SCAN_FAILED;
    if (complete) {
        return fault_count == 0U
            ? LINK_FAULT_SCAN_CLEAN
            : LINK_FAULT_SCAN_FAULTS_PRESENT;
    }
    if (started || active) return LINK_FAULT_SCAN_IN_PROGRESS;
    return LINK_FAULT_SCAN_NOT_SCANNED;
}

static inline const char *link_fault_scan_presentation_state_name(
    LinkFaultScanPresentationState state)
{
    switch (state) {
    case LINK_FAULT_SCAN_NOT_SCANNED: return "not-scanned";
    case LINK_FAULT_SCAN_IN_PROGRESS: return "in-progress";
    case LINK_FAULT_SCAN_FAILED: return "failed";
    case LINK_FAULT_SCAN_CLEAN: return "clean";
    case LINK_FAULT_SCAN_FAULTS_PRESENT: return "faults-present";
    }
    return "unknown";
}

#ifdef __cplusplus
}
#endif

#endif
