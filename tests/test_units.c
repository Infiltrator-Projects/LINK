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

    {
        char text[128];
        LinkObd2Sample sample = {
            .pid = UINT8_C(0x05),
            .value = 100.0,
            .unit = LINK_OBD2_UNIT_CELSIUS
        };
        link_unit_preferences_us_customary(&preferences);
        CHECK(link_units_format_obd2_with_preferences(
            &sample, &preferences, text, sizeof(text)));
        CHECK(strcmp(text, "212.0 °F") == 0);

        sample.value = 250.0;
        sample.unit = LINK_OBD2_UNIT_KPA;
        preferences.pressure = LINK_PRESSURE_BAR;
        CHECK(link_units_format_obd2_with_preferences(
            &sample, &preferences, text, sizeof(text)));
        CHECK(strcmp(text, "2.50 bar") == 0);

        LinkObd2DecodedPid decoded = {0};
        decoded.signal_count = 2U;
        decoded.signals[0].label = "A";
        decoded.signals[0].value = 12.5;
        decoded.signals[0].unit = "kPa";
        decoded.signals[1].label = "B";
        decoded.signals[1].value = 3.0;
        decoded.signals[1].unit = "";
        CHECK(link_obd2_format_decoded_summary(
            &decoded, 2U, 8U, text, sizeof(text)));
        CHECK(strcmp(text, "A 12.50 kPa · B 3.00") == 0);

        memset(&decoded, 0, sizeof(decoded));
        decoded.raw_length = 3U;
        decoded.raw[0] = UINT8_C(0xAA);
        decoded.raw[1] = UINT8_C(0x01);
        decoded.raw[2] = UINT8_C(0xFF);
        CHECK(link_obd2_format_decoded_summary(
            &decoded, 2U, 2U, text, sizeof(text)));
        CHECK(strcmp(text, "RAW AA 01 …") == 0);
    }

    puts("LINK dimension-aware measurement conversion passed");
    return 0;
}
