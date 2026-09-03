// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/units.h"
#include <string.h>

const char *link_measurement_system_key(LinkMeasurementSystem system)
{
    switch (system) {
    case LINK_MEASUREMENT_SYSTEM_DEFAULT: return "system";
    case LINK_MEASUREMENT_SYSTEM_METRIC: return "metric";
    case LINK_MEASUREMENT_SYSTEM_US_CUSTOMARY: return "us-customary";
    }
    return "system";
}

bool link_measurement_system_from_key(const char *key, LinkMeasurementSystem *system)
{
    if (key == NULL || system == NULL) return false;
    if (strcmp(key, "system") == 0) { *system = LINK_MEASUREMENT_SYSTEM_DEFAULT; return true; }
    if (strcmp(key, "metric") == 0) { *system = LINK_MEASUREMENT_SYSTEM_METRIC; return true; }
    if (strcmp(key, "us-customary") == 0) { *system = LINK_MEASUREMENT_SYSTEM_US_CUSTOMARY; return true; }
    return false;
}

LinkMeasurementSystem link_measurement_system_resolve(
    LinkMeasurementSystem requested, bool host_uses_metric)
{
    if (requested == LINK_MEASUREMENT_SYSTEM_DEFAULT)
        return host_uses_metric ? LINK_MEASUREMENT_SYSTEM_METRIC
                                : LINK_MEASUREMENT_SYSTEM_US_CUSTOMARY;
    return requested == LINK_MEASUREMENT_SYSTEM_US_CUSTOMARY
        ? LINK_MEASUREMENT_SYSTEM_US_CUSTOMARY
        : LINK_MEASUREMENT_SYSTEM_METRIC;
}

bool link_units_convert_obd2(
    LinkObd2Unit unit,
    double canonical_value,
    LinkMeasurementSystem system,
    double *display_value,
    const char **display_unit)
{
    if (display_value == NULL || display_unit == NULL ||
        (system != LINK_MEASUREMENT_SYSTEM_METRIC &&
         system != LINK_MEASUREMENT_SYSTEM_US_CUSTOMARY))
        return false;

    *display_value = canonical_value;
    *display_unit = link_obd2_unit_name(unit);
    if (system == LINK_MEASUREMENT_SYSTEM_METRIC) return true;

    switch (unit) {
    case LINK_OBD2_UNIT_CELSIUS:
        *display_value = canonical_value * 9.0 / 5.0 + 32.0;
        *display_unit = "degF";
        break;
    case LINK_OBD2_UNIT_KPA:
        *display_value = canonical_value * 0.14503773773020923;
        *display_unit = "psi";
        break;
    case LINK_OBD2_UNIT_KMH:
        *display_value = canonical_value * 0.621371192237334;
        *display_unit = "mph";
        break;
    case LINK_OBD2_UNIT_KILOMETRES:
        *display_value = canonical_value * 0.621371192237334;
        *display_unit = "mi";
        break;
    case LINK_OBD2_UNIT_LITRES_PER_HOUR:
        *display_value = canonical_value * 0.2641720523581484;
        *display_unit = "US gal/h";
        break;
    case LINK_OBD2_UNIT_GRAMS_PER_SECOND:
        *display_value = canonical_value * 0.1322773573109265;
        *display_unit = "lb/min";
        break;
    case LINK_OBD2_UNIT_KILOGRAMS_PER_HOUR:
        *display_value = canonical_value * 2.2046226218487757;
        *display_unit = "lb/h";
        break;
    case LINK_OBD2_UNIT_NONE:
    case LINK_OBD2_UNIT_PERCENT:
    case LINK_OBD2_UNIT_RPM:
    case LINK_OBD2_UNIT_VOLTS:
    case LINK_OBD2_UNIT_SECONDS:
    case LINK_OBD2_UNIT_MINUTES:
    case LINK_OBD2_UNIT_COUNT:
    case LINK_OBD2_UNIT_RATIO:
        break;
    }
    return true;
}
