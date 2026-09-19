// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/selection.h"

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

    puts("LINK vehicle-scoped parameter selection passed");
    return 0;
}
