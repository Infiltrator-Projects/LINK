// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/units.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL: %s\n", #x); return 1; } } while (0)
#define CLOSE(a,b) (fabs((a) - (b)) < 0.0001)

int main(void)
{
    LinkMeasurementSystem system;
    double value;
    const char *unit;

    CHECK(link_measurement_system_from_key("metric", &system));
    CHECK(system == LINK_MEASUREMENT_SYSTEM_METRIC);
    CHECK(link_measurement_system_from_key("us-customary", &system));
    CHECK(system == LINK_MEASUREMENT_SYSTEM_US_CUSTOMARY);
    CHECK(link_measurement_system_from_key("us", &system));
    CHECK(system == LINK_MEASUREMENT_SYSTEM_US_CUSTOMARY);
    CHECK(!link_measurement_system_from_key("system", &system));
    CHECK(!link_measurement_system_from_key("bogus", &system));

    CHECK(link_units_convert_obd2(
        LINK_OBD2_UNIT_CELSIUS, 100.0,
        LINK_MEASUREMENT_SYSTEM_US_CUSTOMARY, &value, &unit));
    CHECK(CLOSE(value, 212.0) && strcmp(unit, "degF") == 0);

    CHECK(link_units_convert_obd2(
        LINK_OBD2_UNIT_KMH, 100.0,
        LINK_MEASUREMENT_SYSTEM_US_CUSTOMARY, &value, &unit));
    CHECK(CLOSE(value, 62.1371192237334) && strcmp(unit, "mph") == 0);

    CHECK(link_units_convert_obd2(
        LINK_OBD2_UNIT_KPA, 100.0,
        LINK_MEASUREMENT_SYSTEM_US_CUSTOMARY, &value, &unit));
    CHECK(CLOSE(value, 14.5037737730209) && strcmp(unit, "psi") == 0);

    CHECK(link_units_convert_obd2(
        LINK_OBD2_UNIT_LITRES_PER_HOUR, 10.0,
        LINK_MEASUREMENT_SYSTEM_US_CUSTOMARY, &value, &unit));
    CHECK(CLOSE(value, 2.641720523581484) &&
          strcmp(unit, "US gal/h") == 0);

    CHECK(link_units_convert_obd2(
        LINK_OBD2_UNIT_KILOMETRES, 100.0,
        LINK_MEASUREMENT_SYSTEM_US_CUSTOMARY, &value, &unit));
    CHECK(CLOSE(value, 100.0) && strcmp(unit, "km") == 0);

    CHECK(link_units_convert_obd2(
        LINK_OBD2_UNIT_CELSIUS, 20.0,
        LINK_MEASUREMENT_SYSTEM_METRIC, &value, &unit));
    CHECK(CLOSE(value, 20.0) && strcmp(unit, "degC") == 0);

    puts("LINK MBLINK-compatible measurement conversion passed");
    return 0;
}
