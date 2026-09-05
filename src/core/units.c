// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/units.h"
#include <string.h>

static bool converted(
    double value, const char *unit,
    double *display_value, const char **display_unit)
{
    if (display_value == NULL || display_unit == NULL || unit == NULL)
        return false;
    *display_value = value;
    *display_unit = unit;
    return true;
}

const char *link_measurement_system_key(LinkMeasurementSystem system)
{
    return system == LINK_MEASUREMENT_SYSTEM_US_CUSTOMARY
        ? "us-customary" : "metric";
}

bool link_measurement_system_from_key(
    const char *key, LinkMeasurementSystem *system)
{
    if (key == NULL || system == NULL) return false;
    if (strcmp(key, "metric") == 0) {
        *system = LINK_MEASUREMENT_SYSTEM_METRIC;
        return true;
    }
    if (strcmp(key, "us-customary") == 0 ||
        strcmp(key, "us") == 0) {
        *system = LINK_MEASUREMENT_SYSTEM_US_CUSTOMARY;
        return true;
    }
    return false;
}

void link_unit_preferences_metric(LinkUnitPreferences *preferences)
{
    if (preferences == NULL) return;
    preferences->temperature = LINK_TEMPERATURE_CELSIUS;
    preferences->pressure = LINK_PRESSURE_KPA;
    preferences->speed = LINK_SPEED_KMH;
    preferences->distance = LINK_DISTANCE_KM;
    preferences->fuel_volume = LINK_FUEL_VOLUME_LITRES;
    preferences->fuel_economy = LINK_FUEL_ECONOMY_L_PER_100KM;
    preferences->fuel_rate = LINK_FUEL_RATE_L_PER_HOUR;
    preferences->air_mass = LINK_AIR_MASS_G_PER_SECOND;
}

void link_unit_preferences_us_customary(LinkUnitPreferences *preferences)
{
    if (preferences == NULL) return;
    preferences->temperature = LINK_TEMPERATURE_FAHRENHEIT;
    preferences->pressure = LINK_PRESSURE_PSI;
    preferences->speed = LINK_SPEED_MPH;
    preferences->distance = LINK_DISTANCE_MILES;
    preferences->fuel_volume = LINK_FUEL_VOLUME_US_GALLONS;
    preferences->fuel_economy = LINK_FUEL_ECONOMY_MPG_US;
    preferences->fuel_rate = LINK_FUEL_RATE_US_GAL_PER_HOUR;
    preferences->air_mass = LINK_AIR_MASS_LB_PER_MINUTE;
}

bool link_unit_preferences_from_measurement_system(
    LinkMeasurementSystem system, LinkUnitPreferences *preferences)
{
    if (preferences == NULL) return false;
    switch (system) {
    case LINK_MEASUREMENT_SYSTEM_METRIC:
        link_unit_preferences_metric(preferences);
        return true;
    case LINK_MEASUREMENT_SYSTEM_US_CUSTOMARY:
        link_unit_preferences_us_customary(preferences);
        return true;
    }
    return false;
}

bool link_units_convert_temperature(
    double celsius, LinkTemperatureUnit unit,
    double *display_value, const char **display_unit)
{
    switch (unit) {
    case LINK_TEMPERATURE_CELSIUS:
        return converted(celsius, "degC", display_value, display_unit);
    case LINK_TEMPERATURE_FAHRENHEIT:
        return converted(
            celsius * 9.0 / 5.0 + 32.0, "degF",
            display_value, display_unit);
    }
    return false;
}

bool link_units_convert_pressure(
    double kpa, LinkPressureUnit unit,
    double *display_value, const char **display_unit)
{
    switch (unit) {
    case LINK_PRESSURE_KPA:
        return converted(kpa, "kPa", display_value, display_unit);
    case LINK_PRESSURE_BAR:
        return converted(kpa / 100.0, "bar", display_value, display_unit);
    case LINK_PRESSURE_PSI:
        return converted(
            kpa * 0.14503773773020923, "psi",
            display_value, display_unit);
    }
    return false;
}

bool link_units_convert_speed(
    double kmh, LinkSpeedUnit unit,
    double *display_value, const char **display_unit)
{
    switch (unit) {
    case LINK_SPEED_KMH:
        return converted(kmh, "km/h", display_value, display_unit);
    case LINK_SPEED_MPH:
        return converted(
            kmh * 0.621371192237334, "mph",
            display_value, display_unit);
    }
    return false;
}

bool link_units_convert_distance(
    double kilometres, LinkDistanceUnit unit,
    double *display_value, const char **display_unit)
{
    switch (unit) {
    case LINK_DISTANCE_KM:
        return converted(kilometres, "km", display_value, display_unit);
    case LINK_DISTANCE_MILES:
        return converted(
            kilometres * 0.621371192237334, "mi",
            display_value, display_unit);
    }
    return false;
}

bool link_units_convert_fuel_volume(
    double litres, LinkFuelVolumeUnit unit,
    double *display_value, const char **display_unit)
{
    switch (unit) {
    case LINK_FUEL_VOLUME_LITRES:
        return converted(litres, "L", display_value, display_unit);
    case LINK_FUEL_VOLUME_US_GALLONS:
        return converted(
            litres * 0.2641720523581484, "US gal",
            display_value, display_unit);
    case LINK_FUEL_VOLUME_IMPERIAL_GALLONS:
        return converted(
            litres * 0.2199692482990878, "Imp gal",
            display_value, display_unit);
    }
    return false;
}

bool link_units_convert_fuel_economy(
    double litres_per_100km, LinkFuelEconomyUnit unit,
    double *display_value, const char **display_unit)
{
    switch (unit) {
    case LINK_FUEL_ECONOMY_L_PER_100KM:
        return converted(
            litres_per_100km, "L/100 km", display_value, display_unit);
    case LINK_FUEL_ECONOMY_KM_PER_L:
        return litres_per_100km > 0.0 && converted(
            100.0 / litres_per_100km, "km/L", display_value, display_unit);
    case LINK_FUEL_ECONOMY_MPG_US:
        return litres_per_100km > 0.0 && converted(
            235.214583 / litres_per_100km, "mpg (US)",
            display_value, display_unit);
    case LINK_FUEL_ECONOMY_MPG_IMPERIAL:
        return litres_per_100km > 0.0 && converted(
            282.480936 / litres_per_100km, "mpg (Imp)",
            display_value, display_unit);
    }
    return false;
}

bool link_units_convert_fuel_rate(
    double litres_per_hour, LinkFuelRateUnit unit,
    double *display_value, const char **display_unit)
{
    switch (unit) {
    case LINK_FUEL_RATE_L_PER_HOUR:
        return converted(
            litres_per_hour, "L/h", display_value, display_unit);
    case LINK_FUEL_RATE_US_GAL_PER_HOUR:
        return converted(
            litres_per_hour * 0.2641720523581484, "US gal/h",
            display_value, display_unit);
    case LINK_FUEL_RATE_IMPERIAL_GAL_PER_HOUR:
        return converted(
            litres_per_hour * 0.2199692482990878, "Imp gal/h",
            display_value, display_unit);
    }
    return false;
}

bool link_units_convert_air_mass(
    double grams_per_second, LinkAirMassUnit unit,
    double *display_value, const char **display_unit)
{
    switch (unit) {
    case LINK_AIR_MASS_G_PER_SECOND:
        return converted(
            grams_per_second, "g/s", display_value, display_unit);
    case LINK_AIR_MASS_LB_PER_MINUTE:
        return converted(
            grams_per_second * 0.1322773573109265, "lb/min",
            display_value, display_unit);
    }
    return false;
}

bool link_units_convert_obd2_with_preferences(
    LinkObd2Unit unit,
    double canonical_value,
    const LinkUnitPreferences *preferences,
    double *display_value,
    const char **display_unit)
{
    if (preferences == NULL || display_value == NULL || display_unit == NULL)
        return false;

    switch (unit) {
    case LINK_OBD2_UNIT_CELSIUS:
        return link_units_convert_temperature(
            canonical_value, preferences->temperature,
            display_value, display_unit);
    case LINK_OBD2_UNIT_KPA:
        return link_units_convert_pressure(
            canonical_value, preferences->pressure,
            display_value, display_unit);
    case LINK_OBD2_UNIT_KMH:
        return link_units_convert_speed(
            canonical_value, preferences->speed,
            display_value, display_unit);
    case LINK_OBD2_UNIT_GRAMS_PER_SECOND:
        return link_units_convert_air_mass(
            canonical_value, preferences->air_mass,
            display_value, display_unit);
    case LINK_OBD2_UNIT_LITRES_PER_HOUR:
        return link_units_convert_fuel_rate(
            canonical_value, preferences->fuel_rate,
            display_value, display_unit);
    case LINK_OBD2_UNIT_KILOMETRES:
        return link_units_convert_distance(
            canonical_value, preferences->distance,
            display_value, display_unit);
    case LINK_OBD2_UNIT_NONE:
    case LINK_OBD2_UNIT_PERCENT:
    case LINK_OBD2_UNIT_RPM:
    case LINK_OBD2_UNIT_VOLTS:
    case LINK_OBD2_UNIT_SECONDS:
    case LINK_OBD2_UNIT_MINUTES:
    case LINK_OBD2_UNIT_COUNT:
    case LINK_OBD2_UNIT_RATIO:
    case LINK_OBD2_UNIT_KILOGRAMS_PER_HOUR:
        return converted(
            canonical_value, link_obd2_unit_name(unit),
            display_value, display_unit);
    }
    return false;
}

bool link_units_convert_obd2(
    LinkObd2Unit unit,
    double canonical_value,
    LinkMeasurementSystem system,
    double *display_value,
    const char **display_unit)
{
    LinkUnitPreferences preferences;
    if (!link_unit_preferences_from_measurement_system(system, &preferences))
        return false;
    return link_units_convert_obd2_with_preferences(
        unit, canonical_value, &preferences, display_value, display_unit);
}
