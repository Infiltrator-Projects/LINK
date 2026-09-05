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
    LinkUnitPreferences preferences;
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

    link_unit_preferences_metric(&preferences);
    CHECK(preferences.pressure == LINK_PRESSURE_KPA);
    CHECK(preferences.fuel_economy == LINK_FUEL_ECONOMY_L_PER_100KM);
    link_unit_preferences_us_customary(&preferences);
    CHECK(preferences.distance == LINK_DISTANCE_MILES);
    CHECK(preferences.air_mass == LINK_AIR_MASS_LB_PER_MINUTE);

    CHECK(link_units_convert_temperature(
        100.0, LINK_TEMPERATURE_FAHRENHEIT, &value, &unit));
    CHECK(CLOSE(value, 212.0) && strcmp(unit, "degF") == 0);

    CHECK(link_units_convert_pressure(
        250.0, LINK_PRESSURE_BAR, &value, &unit));
    CHECK(CLOSE(value, 2.5) && strcmp(unit, "bar") == 0);

    CHECK(link_units_convert_pressure(
        100.0, LINK_PRESSURE_PSI, &value, &unit));
    CHECK(CLOSE(value, 14.5037737730209) && strcmp(unit, "psi") == 0);

    CHECK(link_units_convert_speed(
        100.0, LINK_SPEED_MPH, &value, &unit));
    CHECK(CLOSE(value, 62.1371192237334) && strcmp(unit, "mph") == 0);

    CHECK(link_units_convert_distance(
        100.0, LINK_DISTANCE_MILES, &value, &unit));
    CHECK(CLOSE(value, 62.1371192237334) && strcmp(unit, "mi") == 0);

    CHECK(link_units_convert_fuel_volume(
        10.0, LINK_FUEL_VOLUME_IMPERIAL_GALLONS, &value, &unit));
    CHECK(CLOSE(value, 2.199692482990878) && strcmp(unit, "Imp gal") == 0);

    CHECK(link_units_convert_fuel_rate(
        10.0, LINK_FUEL_RATE_IMPERIAL_GAL_PER_HOUR, &value, &unit));
    CHECK(CLOSE(value, 2.199692482990878) && strcmp(unit, "Imp gal/h") == 0);

    CHECK(link_units_convert_air_mass(
        10.0, LINK_AIR_MASS_LB_PER_MINUTE, &value, &unit));
    CHECK(CLOSE(value, 1.322773573109265) && strcmp(unit, "lb/min") == 0);

    CHECK(link_units_convert_fuel_economy(
        8.0, LINK_FUEL_ECONOMY_KM_PER_L, &value, &unit));
    CHECK(CLOSE(value, 12.5) && strcmp(unit, "km/L") == 0);
    CHECK(link_units_convert_fuel_economy(
        8.0, LINK_FUEL_ECONOMY_MPG_US, &value, &unit));
    CHECK(CLOSE(value, 29.401822875) && strcmp(unit, "mpg (US)") == 0);
    CHECK(link_units_convert_fuel_economy(
        8.0, LINK_FUEL_ECONOMY_MPG_IMPERIAL, &value, &unit));
    CHECK(CLOSE(value, 35.310117) && strcmp(unit, "mpg (Imp)") == 0);
    CHECK(!link_units_convert_fuel_economy(
        0.0, LINK_FUEL_ECONOMY_MPG_US, &value, &unit));

    CHECK(link_units_convert_obd2(
        LINK_OBD2_UNIT_CELSIUS, 100.0,
        LINK_MEASUREMENT_SYSTEM_US_CUSTOMARY, &value, &unit));
    CHECK(CLOSE(value, 212.0) && strcmp(unit, "degF") == 0);

    CHECK(link_units_convert_obd2(
        LINK_OBD2_UNIT_KMH, 100.0,
        LINK_MEASUREMENT_SYSTEM_US_CUSTOMARY, &value, &unit));
    CHECK(CLOSE(value, 62.1371192237334) && strcmp(unit, "mph") == 0);

    CHECK(link_units_convert_obd2(
        LINK_OBD2_UNIT_KILOMETRES, 100.0,
        LINK_MEASUREMENT_SYSTEM_US_CUSTOMARY, &value, &unit));
    CHECK(CLOSE(value, 62.1371192237334) && strcmp(unit, "mi") == 0);

    link_unit_preferences_metric(&preferences);
    preferences.pressure = LINK_PRESSURE_BAR;
    preferences.fuel_rate = LINK_FUEL_RATE_IMPERIAL_GAL_PER_HOUR;
    CHECK(link_units_convert_obd2_with_preferences(
        LINK_OBD2_UNIT_KPA, 250.0, &preferences, &value, &unit));
    CHECK(CLOSE(value, 2.5) && strcmp(unit, "bar") == 0);
    CHECK(link_units_convert_obd2_with_preferences(
        LINK_OBD2_UNIT_LITRES_PER_HOUR, 10.0,
        &preferences, &value, &unit));
    CHECK(CLOSE(value, 2.199692482990878) && strcmp(unit, "Imp gal/h") == 0);

    puts("LINK dimension-aware measurement conversion passed");
    return 0;
}
