// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/dashboard.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

int main(void)
{
    LinkDashboardPresentationMode mode;
    LinkDashboardGaugeRange range;
    double fraction = -1.0;
    const LinkParameterDefinition *rpm =
        link_parameter_obd2_definition(UINT8_C(0x0c));

    CHECK(strcmp(link_dashboard_presentation_mode_key(
        LINK_DASHBOARD_PRESENTATION_NUMBERS), "numbers") == 0);
    CHECK(strcmp(link_dashboard_presentation_mode_key(
        LINK_DASHBOARD_PRESENTATION_DIALS), "dials") == 0);
    CHECK(strcmp(link_dashboard_presentation_mode_key(
        LINK_DASHBOARD_PRESENTATION_COMBINED), "combined") == 0);
    CHECK(link_dashboard_presentation_mode_from_key("combined", &mode));
    CHECK(mode == LINK_DASHBOARD_PRESENTATION_COMBINED);
    CHECK(!link_dashboard_presentation_mode_from_key("gauge", &mode));

    CHECK(rpm != NULL);
    CHECK(link_dashboard_gauge_range_for_parameter(rpm, &range));
    CHECK(fabs(range.minimum - 0.0) < 1e-12);
    CHECK(fabs(range.maximum - 7000.0) < 1e-12);
    CHECK(link_dashboard_gauge_fraction(&range, range.minimum, &fraction));
    CHECK(fabs(fraction) < 1e-12);
    CHECK(link_dashboard_gauge_fraction(&range, range.maximum, &fraction));
    CHECK(fabs(fraction - 1.0) < 1e-12);
    CHECK(link_dashboard_gauge_fraction(
        &range, (range.minimum + range.maximum) / 2.0, &fraction));
    CHECK(fabs(fraction - 0.5) < 1e-12);
    CHECK(link_dashboard_gauge_fraction(&range, range.maximum * 10.0, &fraction));
    CHECK(fabs(fraction - 1.0) < 1e-12);
    return 0;
}
