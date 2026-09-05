// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LINK_UNITS_H
#define LINK_UNITS_H

#include "link/obd2.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum LinkMeasurementSystem {
    LINK_MEASUREMENT_SYSTEM_METRIC = 0,
    LINK_MEASUREMENT_SYSTEM_US_CUSTOMARY
} LinkMeasurementSystem;

/*
 * Dimension-specific display choices.
 *
 * Canonical diagnostic values never change: temperatures remain Celsius,
 * pressure kPa, speed km/h, distance km, fuel volume litres, fuel economy
 * L/100 km, fuel rate L/h and air mass g/s. These enums only select the
 * presentation conversion used by product faces.
 */
typedef enum LinkTemperatureUnit {
    LINK_TEMPERATURE_CELSIUS = 0,
    LINK_TEMPERATURE_FAHRENHEIT
} LinkTemperatureUnit;

typedef enum LinkPressureUnit {
    LINK_PRESSURE_KPA = 0,
    LINK_PRESSURE_BAR,
    LINK_PRESSURE_PSI
} LinkPressureUnit;

typedef enum LinkSpeedUnit {
    LINK_SPEED_KMH = 0,
    LINK_SPEED_MPH
} LinkSpeedUnit;

typedef enum LinkDistanceUnit {
    LINK_DISTANCE_KM = 0,
    LINK_DISTANCE_MILES
} LinkDistanceUnit;

typedef enum LinkFuelVolumeUnit {
    LINK_FUEL_VOLUME_LITRES = 0,
    LINK_FUEL_VOLUME_US_GALLONS,
    LINK_FUEL_VOLUME_IMPERIAL_GALLONS
} LinkFuelVolumeUnit;

typedef enum LinkFuelEconomyUnit {
    LINK_FUEL_ECONOMY_L_PER_100KM = 0,
    LINK_FUEL_ECONOMY_KM_PER_L,
    LINK_FUEL_ECONOMY_MPG_US,
    LINK_FUEL_ECONOMY_MPG_IMPERIAL
} LinkFuelEconomyUnit;

typedef enum LinkFuelRateUnit {
    LINK_FUEL_RATE_L_PER_HOUR = 0,
    LINK_FUEL_RATE_US_GAL_PER_HOUR,
    LINK_FUEL_RATE_IMPERIAL_GAL_PER_HOUR
} LinkFuelRateUnit;

typedef enum LinkAirMassUnit {
    LINK_AIR_MASS_G_PER_SECOND = 0,
    LINK_AIR_MASS_LB_PER_MINUTE
} LinkAirMassUnit;

typedef struct LinkUnitPreferences {
    LinkTemperatureUnit temperature;
    LinkPressureUnit pressure;
    LinkSpeedUnit speed;
    LinkDistanceUnit distance;
    LinkFuelVolumeUnit fuel_volume;
    LinkFuelEconomyUnit fuel_economy;
    LinkFuelRateUnit fuel_rate;
    LinkAirMassUnit air_mass;
} LinkUnitPreferences;

const char *link_measurement_system_key(LinkMeasurementSystem system);
bool link_measurement_system_from_key(
    const char *key, LinkMeasurementSystem *system);

void link_unit_preferences_metric(LinkUnitPreferences *preferences);
void link_unit_preferences_us_customary(LinkUnitPreferences *preferences);
bool link_unit_preferences_from_measurement_system(
    LinkMeasurementSystem system, LinkUnitPreferences *preferences);

bool link_units_convert_temperature(
    double celsius, LinkTemperatureUnit unit,
    double *display_value, const char **display_unit);
bool link_units_convert_pressure(
    double kpa, LinkPressureUnit unit,
    double *display_value, const char **display_unit);
bool link_units_convert_speed(
    double kmh, LinkSpeedUnit unit,
    double *display_value, const char **display_unit);
bool link_units_convert_distance(
    double kilometres, LinkDistanceUnit unit,
    double *display_value, const char **display_unit);
bool link_units_convert_fuel_volume(
    double litres, LinkFuelVolumeUnit unit,
    double *display_value, const char **display_unit);
bool link_units_convert_fuel_economy(
    double litres_per_100km, LinkFuelEconomyUnit unit,
    double *display_value, const char **display_unit);
bool link_units_convert_fuel_rate(
    double litres_per_hour, LinkFuelRateUnit unit,
    double *display_value, const char **display_unit);
bool link_units_convert_air_mass(
    double grams_per_second, LinkAirMassUnit unit,
    double *display_value, const char **display_unit);

/* Convert one canonical OBD-II scalar using explicit dimension preferences. */
bool link_units_convert_obd2_with_preferences(
    LinkObd2Unit unit,
    double canonical_value,
    const LinkUnitPreferences *preferences,
    double *display_value,
    const char **display_unit);

/*
 * Compatibility convenience for callers that only expose Metric/US customary.
 * Diagnostic/CSV evidence remains canonical. New product faces should prefer
 * LinkUnitPreferences when they expose per-dimension choices.
 */
bool link_units_convert_obd2(
    LinkObd2Unit unit,
    double canonical_value,
    LinkMeasurementSystem system,
    double *display_value,
    const char **display_unit);

#ifdef __cplusplus
}
#endif
#endif
