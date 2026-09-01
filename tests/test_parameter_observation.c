// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/parameter.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "check failed: %s at %s:%d\n", #expr, __FILE__, __LINE__); \
    return 1; } } while (0)

int main(void)
{
    const LinkParameterDefinition *rpm =
        link_parameter_obd2_definition(UINT8_C(0x0c));
    LinkParameterObservation observation;
    LinkParameterSample sample;
    char text[64];

    CHECK(rpm != NULL);
    CHECK(strcmp(link_parameter_value_status_name(
                     LINK_PARAMETER_VALUE_STATUS_NOT_RECEIVED),
                 "not-received") == 0);
    CHECK(strcmp(link_parameter_provenance_name(
                     LINK_PARAMETER_PROVENANCE_ODX_DESCRIPTION),
                 "odx-description") == 0);

    CHECK(link_parameter_observation_init(
        rpm, UINT64_C(1234), LINK_PARAMETER_VALUE_STATUS_VALID,
        LINK_PARAMETER_PROVENANCE_LIVE_OBD2,
        true, UINT32_C(0x7e8), false, 2345.0, &observation));
    CHECK(link_parameter_observation_is_valid(&observation));
    CHECK(link_parameter_observation_has_value(&observation));
    CHECK(observation.responder_id == UINT32_C(0x7e8));
    CHECK(link_parameter_format_observation(
        &observation, text, sizeof(text)));
    CHECK(strstr(text, "2345") != NULL);

    CHECK(!link_parameter_observation_init(
        rpm, 0U, LINK_PARAMETER_VALUE_STATUS_VALID,
        LINK_PARAMETER_PROVENANCE_LIVE_OBD2,
        false, 0U, false, NAN, &observation));

    CHECK(link_parameter_observation_init(
        rpm, UINT64_C(2000), LINK_PARAMETER_VALUE_STATUS_INVALID,
        LINK_PARAMETER_PROVENANCE_MANUFACTURER_BACKEND,
        false, 0U, false, NAN, &observation));
    CHECK(!link_parameter_observation_has_value(&observation));
    CHECK(link_parameter_format_observation(
        &observation, text, sizeof(text)));

    sample.definition = rpm;
    sample.timestamp_ms = UINT64_C(3000);
    sample.available = false;
    sample.value = 0.0;
    CHECK(link_parameter_observation_from_sample(
        &sample, LINK_PARAMETER_PROVENANCE_LIVE_OBD2,
        false, 0U, false, &observation));
    CHECK(observation.status == LINK_PARAMETER_VALUE_STATUS_NOT_AVAILABLE);

    puts("parameter observation tests passed");
    return 0;
}
