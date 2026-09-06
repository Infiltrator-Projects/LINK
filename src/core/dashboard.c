// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/dashboard.h"

#include <math.h>
#include <string.h>

const char *link_dashboard_presentation_mode_key(
    LinkDashboardPresentationMode mode)
{
    switch (mode) {
    case LINK_DASHBOARD_PRESENTATION_NUMBERS: return "numbers";
    case LINK_DASHBOARD_PRESENTATION_DIALS: return "dials";
    case LINK_DASHBOARD_PRESENTATION_COMBINED: return "combined";
    }
    return "numbers";
}

bool link_dashboard_presentation_mode_from_key(
    const char *key,
    LinkDashboardPresentationMode *mode)
{
    if (key == NULL || mode == NULL) return false;
    if (strcmp(key, "numbers") == 0) {
        *mode = LINK_DASHBOARD_PRESENTATION_NUMBERS;
        return true;
    }
    if (strcmp(key, "dials") == 0) {
        *mode = LINK_DASHBOARD_PRESENTATION_DIALS;
        return true;
    }
    if (strcmp(key, "combined") == 0) {
        *mode = LINK_DASHBOARD_PRESENTATION_COMBINED;
        return true;
    }
    return false;
}

bool link_dashboard_gauge_range_for_parameter(
    const LinkParameterDefinition *definition,
    LinkDashboardGaugeRange *range)
{
    /*
     * Dashboard scale is presentation metadata, not a decoder clamp. The
     * five scales below are the established Linux cockpit scales and are now
     * centralised here so every platform draws the same instrument.
     */
    static const struct {
        const char *stable_key;
        double minimum;
        double maximum;
    } reference_ranges[] = {
        { "obd2.engine.rpm", 0.0, 7000.0 },
        { "obd2.vehicle.speed", 0.0, 260.0 },
        { "obd2.engine.coolant", -40.0, 150.0 },
        { "obd2.diesel.rail_pressure", 0.0, 200000.0 },
        { "obd2.fuel.tank_level", 0.0, 100.0 }
    };
    size_t index;

    if (definition == NULL || range == NULL || definition->stable_key == NULL)
        return false;

    for (index = 0U; index < sizeof(reference_ranges) / sizeof(reference_ranges[0]); ++index) {
        if (strcmp(definition->stable_key, reference_ranges[index].stable_key) == 0) {
            range->minimum = reference_ranges[index].minimum;
            range->maximum = reference_ranges[index].maximum;
            return true;
        }
    }

    /*
     * Later SAE definitions already carry meaningful finite protocol ranges.
     * Reuse those when available; 0..0 means no dashboard scale is declared.
     */
    if (!isfinite(definition->minimum) || !isfinite(definition->maximum) ||
        definition->maximum <= definition->minimum) {
        return false;
    }
    range->minimum = definition->minimum;
    range->maximum = definition->maximum;
    return true;
}

bool link_dashboard_gauge_fraction(
    const LinkDashboardGaugeRange *range,
    double value,
    double *fraction)
{
    double result;
    if (range == NULL || fraction == NULL || !isfinite(value) ||
        !isfinite(range->minimum) || !isfinite(range->maximum) ||
        range->maximum <= range->minimum) {
        return false;
    }
    result = (value - range->minimum) / (range->maximum - range->minimum);
    if (result < 0.0) result = 0.0;
    if (result > 1.0) result = 1.0;
    *fraction = result;
    return true;
}
