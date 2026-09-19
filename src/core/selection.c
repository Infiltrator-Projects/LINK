// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/selection.h"

#include "infiltratr/core.h"

#include <string.h>

static bool vehicle_identifier_valid(const char *vehicle_identifier)
{
    size_t length;
    if (vehicle_identifier == NULL) return false;
    length = strlen(vehicle_identifier);
    return length < LINK_PARAMETER_SELECTION_VEHICLE_ID_CAPACITY;
}

bool link_parameter_selection_init(
    LinkParameterSelection *selection,
    const char *vehicle_identifier)
{
    if (selection == NULL || !vehicle_identifier_valid(vehicle_identifier))
        return false;
    memset(selection, 0, sizeof(*selection));
    infiltratr_copy_string(
        selection->vehicle_identifier,
        sizeof(selection->vehicle_identifier),
        vehicle_identifier);
    return true;
}

bool link_parameter_selection_set_vehicle(
    LinkParameterSelection *selection,
    const char *vehicle_identifier)
{
    if (selection == NULL || !vehicle_identifier_valid(vehicle_identifier))
        return false;
    if (strcmp(selection->vehicle_identifier, vehicle_identifier) == 0)
        return true;
    memset(selection->keys, 0, sizeof(selection->keys));
    selection->count = 0U;
    infiltratr_copy_string(
        selection->vehicle_identifier,
        sizeof(selection->vehicle_identifier),
        vehicle_identifier);
    return true;
}

bool link_parameter_selection_contains(
    const LinkParameterSelection *selection,
    const LinkParameterKey *key)
{
    size_t index;
    if (selection == NULL || !link_parameter_key_is_valid(key))
        return false;
    for (index = 0U; index < selection->count; ++index) {
        if (link_parameter_key_equal(&selection->keys[index], key))
            return true;
    }
    return false;
}

LinkParameterSelectionResult link_parameter_selection_set(
    LinkParameterSelection *selection,
    const LinkParameterKey *key,
    bool selected)
{
    size_t index;
    if (selection == NULL || !link_parameter_key_is_valid(key))
        return LINK_PARAMETER_SELECTION_RESULT_INVALID_ARGUMENT;

    for (index = 0U; index < selection->count; ++index) {
        if (!link_parameter_key_equal(&selection->keys[index], key))
            continue;
        if (selected) return LINK_PARAMETER_SELECTION_RESULT_OK;
        if (index + 1U < selection->count) {
            memmove(
                &selection->keys[index],
                &selection->keys[index + 1U],
                (selection->count - index - 1U) *
                    sizeof(selection->keys[0]));
        }
        --selection->count;
        memset(
            &selection->keys[selection->count],
            0,
            sizeof(selection->keys[selection->count]));
        return LINK_PARAMETER_SELECTION_RESULT_OK;
    }

    if (!selected) return LINK_PARAMETER_SELECTION_RESULT_OK;
    if (selection->count >= LINK_PARAMETER_SELECTION_MAX_ITEMS)
        return LINK_PARAMETER_SELECTION_RESULT_FULL;
    selection->keys[selection->count++] = *key;
    return LINK_PARAMETER_SELECTION_RESULT_OK;
}

size_t link_parameter_selection_count(
    const LinkParameterSelection *selection)
{
    return selection != NULL ? selection->count : 0U;
}

bool link_parameter_selection_at(
    const LinkParameterSelection *selection,
    size_t index,
    LinkParameterKey *key)
{
    if (selection == NULL || key == NULL || index >= selection->count)
        return false;
    *key = selection->keys[index];
    return true;
}

size_t link_parameter_selection_copy_obd2_pids(
    const LinkParameterSelection *selection,
    uint8_t *pids,
    size_t capacity)
{
    size_t index;
    size_t copied = 0U;
    if (selection == NULL || (capacity != 0U && pids == NULL))
        return 0U;
    for (index = 0U; index < selection->count && copied < capacity; ++index) {
        const LinkParameterKey *key = &selection->keys[index];
        if (key->protocol != LINK_PARAMETER_PROTOCOL_OBD2 ||
            key->module != LINK_PARAMETER_MODULE_STANDARD_OBD2 ||
            key->identifier > UINT8_MAX) {
            continue;
        }
        pids[copied++] = (uint8_t)key->identifier;
    }
    return copied;
}
