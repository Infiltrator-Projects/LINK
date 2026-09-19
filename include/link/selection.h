// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file selection.h
 * @brief Vehicle-scoped, protocol-neutral diagnostic channel selection.
 *
 * LINK owns the in-memory identity model. Platform/product layers remain free
 * to persist the selected keys with native storage, but the meaning of a
 * selection is shared across Apple, Linux and Windows.
 */
#ifndef LINK_SELECTION_H
#define LINK_SELECTION_H

#include "link/parameter.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LINK_PARAMETER_SELECTION_MAX_ITEMS 256U
#define LINK_PARAMETER_SELECTION_VEHICLE_ID_CAPACITY 64U

typedef enum LinkParameterSelectionResult {
    LINK_PARAMETER_SELECTION_RESULT_OK = 0,
    LINK_PARAMETER_SELECTION_RESULT_INVALID_ARGUMENT,
    LINK_PARAMETER_SELECTION_RESULT_FULL
} LinkParameterSelectionResult;

typedef struct LinkParameterSelection {
    char vehicle_identifier[LINK_PARAMETER_SELECTION_VEHICLE_ID_CAPACITY];
    LinkParameterKey keys[LINK_PARAMETER_SELECTION_MAX_ITEMS];
    size_t count;
} LinkParameterSelection;

/** Initialise an empty selection for one vehicle identity. */
bool link_parameter_selection_init(
    LinkParameterSelection *selection,
    const char *vehicle_identifier);

/**
 * Change vehicle identity. A different identity atomically clears the selected
 * keys so choices from one vehicle can never bleed into another vehicle.
 */
bool link_parameter_selection_set_vehicle(
    LinkParameterSelection *selection,
    const char *vehicle_identifier);

/** True when the exact protocol/module/identifier key is selected. */
bool link_parameter_selection_contains(
    const LinkParameterSelection *selection,
    const LinkParameterKey *key);

/** Add or remove an exact key. Adding an already-selected key is idempotent. */
LinkParameterSelectionResult link_parameter_selection_set(
    LinkParameterSelection *selection,
    const LinkParameterKey *key,
    bool selected);

/** Number of selected keys. */
size_t link_parameter_selection_count(
    const LinkParameterSelection *selection);

/** Copy the key at index; false when out of range. */
bool link_parameter_selection_at(
    const LinkParameterSelection *selection,
    size_t index,
    LinkParameterKey *key);

/**
 * Copy selected standard OBD-II PIDs in selection order.
 * Returns the number copied; capacity may be zero with pids == NULL.
 */
size_t link_parameter_selection_copy_obd2_pids(
    const LinkParameterSelection *selection,
    uint8_t *pids,
    size_t capacity);

#ifdef __cplusplus
}
#endif

#endif
