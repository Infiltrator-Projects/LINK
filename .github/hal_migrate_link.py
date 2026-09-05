#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""One-shot migration of reusable MBLINK behaviour into LINK.

The workflow that invokes this script removes both itself and this file before
committing, so these migration mechanics never become product source.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    target = ROOT / path
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(text, encoding="utf-8")


def replace_once(path: str, old: str, new: str) -> None:
    text = read(path)
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{path}: expected one exact replacement, found {count}: {old[:120]!r}")
    write(path, text.replace(old, new, 1))


def regex_once(path: str, pattern: str, replacement: str) -> None:
    text = read(path)
    updated, count = re.subn(pattern, replacement, text, count=1, flags=re.S)
    if count != 1:
        raise SystemExit(f"{path}: expected one regex replacement, found {count}: {pattern[:120]!r}")
    write(path, updated)


UNITS_H = r'''// SPDX-License-Identifier: GPL-3.0-or-later
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
'''

UNITS_C = r'''// SPDX-License-Identifier: GPL-3.0-or-later
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
'''

TEST_UNITS = r'''// SPDX-License-Identifier: GPL-3.0-or-later
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
'''

SESSION_TRACE_H = r'''// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LINK_SESSION_TRACE_H
#define LINK_SESSION_TRACE_H

#include "link/diagnostic_flow.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LINK_SESSION_TRACE_MAX_GRAPHS 16U
#define LINK_SESSION_TRACE_GRAPH_HISTORY_CAPACITY 48U
#define LINK_SESSION_TRACE_LOG_CAPACITY 24U
#define LINK_SESSION_TRACE_LOG_MESSAGE_CAPACITY 160U

typedef struct LinkSessionTrace {
    uint8_t graph_pids[LINK_SESSION_TRACE_MAX_GRAPHS];
    size_t graph_count;
    double graph_history[LINK_SESSION_TRACE_MAX_GRAPHS]
                        [LINK_SESSION_TRACE_GRAPH_HISTORY_CAPACITY];
    uint8_t graph_history_count[LINK_SESSION_TRACE_MAX_GRAPHS];
    uint8_t graph_history_next[LINK_SESSION_TRACE_MAX_GRAPHS];
    char session_log[LINK_SESSION_TRACE_LOG_CAPACITY]
                    [LINK_SESSION_TRACE_LOG_MESSAGE_CAPACITY];
    uint64_t session_log_time_ms[LINK_SESSION_TRACE_LOG_CAPACITY];
    uint8_t session_log_count;
    uint8_t session_log_next;
    uint64_t session_log_started_ms;
} LinkSessionTrace;

bool link_session_trace_init(
    LinkSessionTrace *trace, const uint8_t *graph_pids, size_t graph_count);
size_t link_session_trace_graph_index(
    const LinkSessionTrace *trace, uint8_t pid);
void link_session_trace_reset_graph(LinkSessionTrace *trace);
void link_session_trace_record_graph(
    LinkSessionTrace *trace, uint8_t pid, double value);
void link_session_trace_format_sparkline(
    const double *history, size_t count, size_t next,
    char *output, size_t output_size);

void link_session_trace_clear_log(LinkSessionTrace *trace, uint64_t now_ms);
void link_session_trace_append_log(
    LinkSessionTrace *trace, uint64_t now_ms, const char *message);
size_t link_session_trace_log_ordered_slot(
    const LinkSessionTrace *trace, size_t ordered_index);

/* Human-readable generic flow milestones; live samples deliberately return NULL. */
const char *link_diagnostic_flow_event_text(LinkDiagnosticFlowEventKind kind);

#ifdef __cplusplus
}
#endif
#endif
'''

SESSION_TRACE_C = r'''// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/session_trace.h"

#include <stdio.h>
#include <string.h>

bool link_session_trace_init(
    LinkSessionTrace *trace, const uint8_t *graph_pids, size_t graph_count)
{
    if (trace == NULL || graph_count > LINK_SESSION_TRACE_MAX_GRAPHS ||
        (graph_count != 0U && graph_pids == NULL)) {
        return false;
    }
    memset(trace, 0, sizeof(*trace));
    if (graph_count != 0U)
        memcpy(trace->graph_pids, graph_pids, graph_count);
    trace->graph_count = graph_count;
    return true;
}

size_t link_session_trace_graph_index(
    const LinkSessionTrace *trace, uint8_t pid)
{
    size_t index;
    if (trace == NULL) return 0U;
    for (index = 0U; index < trace->graph_count; ++index) {
        if (trace->graph_pids[index] == pid) return index;
    }
    return trace->graph_count;
}

void link_session_trace_reset_graph(LinkSessionTrace *trace)
{
    if (trace == NULL) return;
    memset(trace->graph_history, 0, sizeof(trace->graph_history));
    memset(trace->graph_history_count, 0, sizeof(trace->graph_history_count));
    memset(trace->graph_history_next, 0, sizeof(trace->graph_history_next));
}

void link_session_trace_record_graph(
    LinkSessionTrace *trace, uint8_t pid, double value)
{
    const size_t graph = link_session_trace_graph_index(trace, pid);
    uint8_t slot;
    if (trace == NULL || graph >= trace->graph_count) return;

    slot = trace->graph_history_next[graph];
    trace->graph_history[graph][slot] = value;
    trace->graph_history_next[graph] = (uint8_t)(
        (slot + 1U) % LINK_SESSION_TRACE_GRAPH_HISTORY_CAPACITY);
    if (trace->graph_history_count[graph] <
        LINK_SESSION_TRACE_GRAPH_HISTORY_CAPACITY) {
        ++trace->graph_history_count[graph];
    }
}

static size_t bounded_length(const char *text, size_t maximum)
{
    size_t length = 0U;
    if (text == NULL) return 0U;
    while (length < maximum && text[length] != '\0') ++length;
    return length;
}

static void append_text(char *output, size_t output_size, const char *text)
{
    const size_t used =
        output != NULL ? bounded_length(output, output_size) : output_size;
    if (output == NULL || output_size == 0U || used >= output_size) return;
    (void)snprintf(output + used, output_size - used, "%s", text);
}

void link_session_trace_format_sparkline(
    const double *history, size_t count, size_t next,
    char *output, size_t output_size)
{
    static const char *const blocks[] = {
        "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"
    };
    double minimum;
    double maximum;
    size_t start;
    size_t index;

    if (output == NULL || output_size == 0U) return;
    output[0] = '\0';
    if (history == NULL || count == 0U) return;
    if (count > LINK_SESSION_TRACE_GRAPH_HISTORY_CAPACITY)
        count = LINK_SESSION_TRACE_GRAPH_HISTORY_CAPACITY;

    start = count < LINK_SESSION_TRACE_GRAPH_HISTORY_CAPACITY ? 0U : next;
    minimum = history[start];
    maximum = history[start];
    for (index = 1U; index < count; ++index) {
        const double value = history[
            (start + index) % LINK_SESSION_TRACE_GRAPH_HISTORY_CAPACITY];
        if (value < minimum) minimum = value;
        if (value > maximum) maximum = value;
    }

    for (index = 0U; index < count; ++index) {
        const double value = history[
            (start + index) % LINK_SESSION_TRACE_GRAPH_HISTORY_CAPACITY];
        unsigned int level = 3U;
        if (maximum > minimum) {
            const double scaled =
                ((value - minimum) / (maximum - minimum)) * 7.0;
            level = (unsigned int)(scaled + 0.5);
            if (level > 7U) level = 7U;
        }
        append_text(output, output_size, blocks[level]);
    }
}

void link_session_trace_clear_log(LinkSessionTrace *trace, uint64_t now_ms)
{
    if (trace == NULL) return;
    memset(trace->session_log, 0, sizeof(trace->session_log));
    memset(trace->session_log_time_ms, 0, sizeof(trace->session_log_time_ms));
    trace->session_log_count = 0U;
    trace->session_log_next = 0U;
    trace->session_log_started_ms = now_ms;
}

void link_session_trace_append_log(
    LinkSessionTrace *trace, uint64_t now_ms, const char *message)
{
    uint8_t slot;
    size_t length;
    if (trace == NULL || message == NULL || message[0] == '\0') return;

    slot = trace->session_log_next;
    if (trace->session_log_started_ms == 0U)
        trace->session_log_started_ms = now_ms;

    length = strlen(message);
    if (length >= LINK_SESSION_TRACE_LOG_MESSAGE_CAPACITY)
        length = LINK_SESSION_TRACE_LOG_MESSAGE_CAPACITY - 1U;
    memcpy(trace->session_log[slot], message, length);
    trace->session_log[slot][length] = '\0';
    trace->session_log_time_ms[slot] =
        now_ms >= trace->session_log_started_ms
            ? now_ms - trace->session_log_started_ms : 0U;
    trace->session_log_next = (uint8_t)(
        (slot + 1U) % LINK_SESSION_TRACE_LOG_CAPACITY);
    if (trace->session_log_count < LINK_SESSION_TRACE_LOG_CAPACITY)
        ++trace->session_log_count;
}

size_t link_session_trace_log_ordered_slot(
    const LinkSessionTrace *trace, size_t ordered_index)
{
    size_t start;
    if (trace == NULL || ordered_index >= trace->session_log_count)
        return LINK_SESSION_TRACE_LOG_CAPACITY;
    start = trace->session_log_count < LINK_SESSION_TRACE_LOG_CAPACITY
        ? 0U : trace->session_log_next;
    return (start + ordered_index) % LINK_SESSION_TRACE_LOG_CAPACITY;
}

const char *link_diagnostic_flow_event_text(LinkDiagnosticFlowEventKind kind)
{
    switch (kind) {
    case LINK_DIAGNOSTIC_FLOW_EVENT_ADAPTER_IDENTIFIED:
        return "Adapter identified";
    case LINK_DIAGNOSTIC_FLOW_EVENT_PROTOCOL_IDENTIFIED:
        return "OBD protocol identified";
    case LINK_DIAGNOSTIC_FLOW_EVENT_PID_DISCOVERY_COMPLETE:
        return "Standard PID discovery complete";
    case LINK_DIAGNOSTIC_FLOW_EVENT_STANDARD_VIN:
        return "Standard VIN read complete";
    case LINK_DIAGNOSTIC_FLOW_EVENT_DTC_LIST:
        return "Standard DTC inventory updated";
    case LINK_DIAGNOSTIC_FLOW_EVENT_READINESS:
        return "Readiness monitors captured";
    case LINK_DIAGNOSTIC_FLOW_EVENT_FREEZE_FRAME_SAMPLE:
        return "Freeze-frame sample captured";
    case LINK_DIAGNOSTIC_FLOW_EVENT_DIAGNOSTIC_CONTEXT_COMPLETE:
        return "Diagnostic context complete";
    case LINK_DIAGNOSTIC_FLOW_EVENT_LIVE_NO_DATA:
        return "Live PID returned no data";
    case LINK_DIAGNOSTIC_FLOW_EVENT_LIVE_UNSUPPORTED:
        return "Live PID reported unsupported";
    case LINK_DIAGNOSTIC_FLOW_EVENT_NONE:
    case LINK_DIAGNOSTIC_FLOW_EVENT_LIVE_SAMPLE:
    case LINK_DIAGNOSTIC_FLOW_EVENT_LIVE_STRUCTURED:
        return NULL;
    }
    return NULL;
}
'''

TEST_SESSION_TRACE = r'''// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/session_trace.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL: %s\n", #x); return 1; } } while (0)

int main(void)
{
    static const uint8_t pids[] = {0x0cU, 0x0dU};
    LinkSessionTrace trace;
    char spark[128];

    CHECK(link_session_trace_init(&trace, pids, 2U));
    CHECK(link_session_trace_graph_index(&trace, 0x0cU) == 0U);
    CHECK(link_session_trace_graph_index(&trace, 0x0dU) == 1U);
    CHECK(link_session_trace_graph_index(&trace, 0x05U) == 2U);

    link_session_trace_record_graph(&trace, 0x0cU, 10.0);
    link_session_trace_record_graph(&trace, 0x0cU, 20.0);
    CHECK(trace.graph_history_count[0] == 2U);
    link_session_trace_format_sparkline(
        trace.graph_history[0], trace.graph_history_count[0],
        trace.graph_history_next[0], spark, sizeof(spark));
    CHECK(spark[0] != '\0');

    link_session_trace_clear_log(&trace, 1000U);
    link_session_trace_append_log(&trace, 1200U, "first");
    link_session_trace_append_log(&trace, 1400U, "second");
    CHECK(trace.session_log_count == 2U);
    CHECK(link_session_trace_log_ordered_slot(&trace, 0U) == 0U);
    CHECK(strcmp(trace.session_log[0], "first") == 0);
    CHECK(trace.session_log_time_ms[1] == 400U);

    CHECK(strcmp(
        link_diagnostic_flow_event_text(
            LINK_DIAGNOSTIC_FLOW_EVENT_PID_DISCOVERY_COMPLETE),
        "Standard PID discovery complete") == 0);
    CHECK(link_diagnostic_flow_event_text(
        LINK_DIAGNOSTIC_FLOW_EVENT_LIVE_SAMPLE) == NULL);

    puts("LINK shared session trace passed");
    return 0;
}
'''

DISCOVER_DECLS = r'''
/*
 * Standards-defined physical addressing helpers. Product plans remain free to
 * add manufacturer-specific routes, but should not reimplement the eight
 * legislated 11-bit OBD slots or ISO 15765 normal-fixed 29-bit UDS mapping.
 */
#define LINK_DISCOVER_STANDARD_OBD11_TARGET_COUNT 8U
#define LINK_DISCOVER_STANDARD_UDS29_TARGET_COUNT 255U
int link_discover_standard_obd11_target_at(
    size_t index, uint32_t bitrate, link_discover_sweep_target *target);
int link_discover_standard_uds29_target_at(
    size_t index, uint32_t bitrate, link_discover_sweep_target *target);
int link_discover_standard_uds29_target(
    uint8_t diagnostic_target, uint32_t bitrate,
    link_discover_sweep_target *target);
'''

DISCOVER_IMPL = r'''
int link_discover_standard_obd11_target_at(
    size_t index, uint32_t bitrate, link_discover_sweep_target *target)
{
    if (target == NULL || bitrate == 0U ||
        index >= LINK_DISCOVER_STANDARD_OBD11_TARGET_COUNT) {
        return 0;
    }
    memset(target, 0, sizeof(*target));
    target->tx_can_id = UINT32_C(0x7e0) + (uint32_t)index;
    target->rx_can_id = UINT32_C(0x7e8) + (uint32_t)index;
    target->bitrate = bitrate;
    target->extended_id = false;
    return 1;
}

int link_discover_standard_uds29_target(
    uint8_t diagnostic_target, uint32_t bitrate,
    link_discover_sweep_target *target)
{
    if (target == NULL || bitrate == 0U || diagnostic_target == UINT8_C(0xf1))
        return 0;
    memset(target, 0, sizeof(*target));
    target->tx_can_id = UINT32_C(0x18da00f1) |
        ((uint32_t)diagnostic_target << 8U);
    target->rx_can_id = UINT32_C(0x18daf100) |
        (uint32_t)diagnostic_target;
    target->bitrate = bitrate;
    target->extended_id = true;
    return 1;
}

int link_discover_standard_uds29_target_at(
    size_t index, uint32_t bitrate, link_discover_sweep_target *target)
{
    unsigned int diagnostic_target;
    if (index >= LINK_DISCOVER_STANDARD_UDS29_TARGET_COUNT) return 0;
    diagnostic_target = (unsigned int)index;
    if (diagnostic_target >= 0xf1U) ++diagnostic_target;
    return link_discover_standard_uds29_target(
        (uint8_t)diagnostic_target, bitrate, target);
}

'''

TEST_DISCOVER_TARGETS = r'''// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/discover.h"
#include <stdio.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL: %s\n", #x); return 1; } } while (0)

int main(void)
{
    link_discover_sweep_target target;

    CHECK(link_discover_standard_obd11_target_at(0U, 500000U, &target));
    CHECK(target.tx_can_id == 0x7e0U && target.rx_can_id == 0x7e8U);
    CHECK(!target.extended_id && target.bitrate == 500000U);
    CHECK(link_discover_standard_obd11_target_at(7U, 500000U, &target));
    CHECK(target.tx_can_id == 0x7e7U && target.rx_can_id == 0x7efU);
    CHECK(!link_discover_standard_obd11_target_at(8U, 500000U, &target));

    CHECK(link_discover_standard_uds29_target_at(0U, 500000U, &target));
    CHECK(target.tx_can_id == 0x18da00f1U && target.rx_can_id == 0x18daf100U);
    CHECK(target.extended_id);
    CHECK(link_discover_standard_uds29_target_at(0xf1U, 500000U, &target));
    CHECK(target.tx_can_id == 0x18daf2f1U && target.rx_can_id == 0x18daf1f2U);
    CHECK(!link_discover_standard_uds29_target(0xf1U, 500000U, &target));
    CHECK(!link_discover_standard_uds29_target_at(
        LINK_DISCOVER_STANDARD_UDS29_TARGET_COUNT, 500000U, &target));

    puts("LINK standards-defined discovery targets passed");
    return 0;
}
'''

APPLE_PROFILE_DECLS = r'''
/**
 * Extract the cached standard Mode 01 PID set for one exact responder from a
 * product profile. The profile dictionary may contain arbitrary manufacturer
 * fields; LINK only owns the `liveResponders` standard-capability member.
 */
FOUNDATION_EXPORT NSArray<NSNumber *> *LinkVehicleProfileCachedPIDs(
    NSDictionary * _Nullable profile,
    uint32_t responderCANIdentifier,
    BOOL extendedID);

/** Merge standard responder/PID capability evidence while preserving product fields. */
- (BOOL)mergeStandardCapabilitiesFromDiagnosticFlow:
    (const LinkDiagnosticFlow *)flow
                                             forVIN:(NSString *)vin;
- (BOOL)mergeStandardCapabilitiesFromFlowEvent:
    (const LinkDiagnosticFlowEvent *)event
                                        forVIN:(NSString *)vin;
'''

APPLE_PROFILE_IMPL = r'''
static NSMutableArray<NSMutableDictionary *> *
LinkVehicleMutableResponders(NSDictionary *profile)
{
    NSMutableArray<NSMutableDictionary *> *responders =
        [[NSMutableArray alloc] init];
    NSArray *stored = [profile[@"liveResponders"] isKindOfClass:[NSArray class]]
        ? profile[@"liveResponders"] : @[];
    for (id value in stored) {
        if ([value isKindOfClass:[NSDictionary class]])
            [responders addObject:[(NSDictionary *)value mutableCopy]];
    }
    return responders;
}

static NSMutableDictionary *LinkVehicleResponderEntry(
    NSMutableArray<NSMutableDictionary *> *responders,
    uint32_t responder,
    BOOL extended,
    BOOL *created)
{
    for (NSMutableDictionary *candidate in responders) {
        NSNumber *rx = candidate[@"rx"];
        NSNumber *isExtended = candidate[@"extended"];
        if ([rx isKindOfClass:[NSNumber class]] &&
            [isExtended isKindOfClass:[NSNumber class]] &&
            rx.unsignedIntValue == responder &&
            isExtended.boolValue == extended) {
            return candidate;
        }
    }
    NSMutableDictionary *entry = [@{
        @"rx": @(responder), @"extended": @(extended), @"pids": @[]
    } mutableCopy];
    [responders addObject:entry];
    if (created != NULL) *created = YES;
    return entry;
}

NSArray<NSNumber *> *LinkVehicleProfileCachedPIDs(
    NSDictionary *profile,
    uint32_t responderCANIdentifier,
    BOOL extendedID)
{
    if (![profile isKindOfClass:[NSDictionary class]]) return @[];
    const uint32_t maximum = extendedID
        ? UINT32_C(0x1fffffff) : UINT32_C(0x7ff);
    if (responderCANIdentifier > maximum) return @[];

    NSArray *responders = [profile[@"liveResponders"] isKindOfClass:[NSArray class]]
        ? profile[@"liveResponders"] : @[];
    NSMutableOrderedSet<NSNumber *> *pids =
        [[NSMutableOrderedSet alloc] init];
    for (id value in responders) {
        if (![value isKindOfClass:[NSDictionary class]]) continue;
        NSDictionary *responder = (NSDictionary *)value;
        NSNumber *rx = responder[@"rx"];
        NSNumber *extended = responder[@"extended"];
        if (![rx isKindOfClass:[NSNumber class]] ||
            ![extended isKindOfClass:[NSNumber class]] ||
            rx.unsignedIntValue != responderCANIdentifier ||
            extended.boolValue != extendedID) {
            continue;
        }
        NSArray *storedPIDs = [responder[@"pids"] isKindOfClass:[NSArray class]]
            ? responder[@"pids"] : @[];
        for (id pid in storedPIDs) {
            if (![pid isKindOfClass:[NSNumber class]]) continue;
            const unsigned int value = [pid unsignedIntValue];
            if (value <= UINT8_MAX) [pids addObject:@(value)];
        }
    }
    return [[pids array] sortedArrayUsingSelector:@selector(compare:)];
}

- (BOOL)mergeStandardCapabilitiesFromDiagnosticFlow:
    (const LinkDiagnosticFlow *)flow
                                             forVIN:(NSString *)vin
{
    if (flow == NULL || flow->supported_pid_responders.count == 0U ||
        !LinkVehicleSessionValidVIN(vin)) {
        return NO;
    }
    NSDictionary *existing = [self profileForVIN:vin];
    if (existing == nil) return NO;

    NSMutableDictionary *profile = [existing mutableCopy];
    NSMutableArray<NSMutableDictionary *> *responders =
        LinkVehicleMutableResponders(profile);
    BOOL changed = NO;

    for (size_t index = 0U;
         index < flow->supported_pid_responders.count; ++index) {
        const LinkObd2ResponderPidSet *set =
            &flow->supported_pid_responders.entries[index];
        BOOL created = NO;
        NSMutableDictionary *match = LinkVehicleResponderEntry(
            responders, set->responder_id, set->extended_id, &created);
        if (created) changed = YES;

        NSMutableArray<NSNumber *> *pids = [[NSMutableArray alloc] init];
        for (unsigned int pid = 0U; pid <= UINT8_MAX; ++pid) {
            if (link_obd2_pid_set_contains(
                    &set->supported_pids, (uint8_t)pid)) {
                [pids addObject:@(pid)];
            }
        }
        NSArray *previous = [match[@"pids"] isKindOfClass:[NSArray class]]
            ? match[@"pids"] : @[];
        if (![previous isEqualToArray:pids]) {
            match[@"pids"] = [pids copy];
            changed = YES;
        }
    }

    if (!changed) return NO;
    profile[@"updatedAt"] = @([[NSDate date] timeIntervalSince1970]);
    profile[@"liveResponders"] = [responders copy];
    [self saveProfile:[profile copy] forVIN:vin];
    return YES;
}

- (BOOL)mergeStandardCapabilitiesFromFlowEvent:
    (const LinkDiagnosticFlowEvent *)event
                                        forVIN:(NSString *)vin
{
    if (event == NULL ||
        (event->kind != LINK_DIAGNOSTIC_FLOW_EVENT_LIVE_SAMPLE &&
         event->kind != LINK_DIAGNOSTIC_FLOW_EVENT_LIVE_STRUCTURED) ||
        (event->responder_samples.count == 0U &&
         event->responder_decoded.count == 0U) ||
        !LinkVehicleSessionValidVIN(vin)) {
        return NO;
    }
    NSDictionary *existing = [self profileForVIN:vin];
    if (existing == nil) return NO;

    NSMutableDictionary *profile = [existing mutableCopy];
    NSMutableArray<NSMutableDictionary *> *responders =
        LinkVehicleMutableResponders(profile);
    BOOL changed = NO;

    for (size_t index = 0U; index < event->responder_samples.count; ++index) {
        const LinkObd2ResponderSample *sample =
            &event->responder_samples.samples[index];
        if (!sample->responder_id_available) continue;
        BOOL created = NO;
        NSMutableDictionary *match = LinkVehicleResponderEntry(
            responders, sample->responder_id, sample->extended_id, &created);
        if (created) changed = YES;
        NSMutableOrderedSet<NSNumber *> *pids =
            [[NSMutableOrderedSet alloc] initWithArray:
                [match[@"pids"] isKindOfClass:[NSArray class]]
                    ? match[@"pids"] : @[]];
        NSNumber *pid = @(sample->sample.pid);
        if (![pids containsObject:pid]) {
            [pids addObject:pid];
            match[@"pids"] = [[pids array]
                sortedArrayUsingSelector:@selector(compare:)];
            changed = YES;
        }
    }

    for (size_t index = 0U; index < event->responder_decoded.count; ++index) {
        const LinkObd2ResponderDecodedPid *entry =
            &event->responder_decoded.entries[index];
        if (!entry->responder_id_available || entry->decoded.definition == NULL)
            continue;
        BOOL created = NO;
        NSMutableDictionary *match = LinkVehicleResponderEntry(
            responders, entry->responder_id, entry->extended_id, &created);
        if (created) changed = YES;
        NSMutableOrderedSet<NSNumber *> *pids =
            [[NSMutableOrderedSet alloc] initWithArray:
                [match[@"pids"] isKindOfClass:[NSArray class]]
                    ? match[@"pids"] : @[]];
        NSNumber *pid = @(entry->decoded.definition->pid);
        if (![pids containsObject:pid]) {
            [pids addObject:pid];
            match[@"pids"] = [[pids array]
                sortedArrayUsingSelector:@selector(compare:)];
            changed = YES;
        }
    }

    if (!changed) return NO;
    profile[@"updatedAt"] = @([[NSDate date] timeIntervalSince1970]);
    profile[@"liveResponders"] = [responders copy];
    [self saveProfile:[profile copy] forVIN:vin];
    return YES;
}

'''

SWIFT_SHARED_MODELS = r'''
// MARK: - Shared diagnostic presentation models

struct LinkDiagnosticParameter: Identifiable {
    let id: String
    let protocolName: String
    let moduleIdentifier: UInt32
    let parameterIdentifier: UInt32
    let shortName: String
    let title: String
    let suffix: String
    let formattedValue: String
    let value: Double?
    let structuredValue: String?
    let rawHex: String?
    let vehicleSupported: Bool
    let favourite: Bool
    let pollingEnabled: Bool
    let history: [Double]
    let sourceLabel: String?
    let qualityNote: String?

    var isAvailable: Bool { value != nil || !(structuredValue ?? "").isEmpty }
    var isSupported: Bool { vehicleSupported }
    var presentationValue: String {
        if value != nil {
            return formattedValue == "N/A" ? "Decode error" : formattedValue
        }
        if let structuredValue, !structuredValue.isEmpty { return structuredValue }
        if !vehicleSupported { return "Not advertised" }
        if !pollingEnabled { return "Not polled" }
        return "Waiting for sample"
    }
    var hasLiveValue: Bool { pollingEnabled && isAvailable }
    var pidText: String {
        let value = String(parameterIdentifier, radix: 16, uppercase: true)
        return "0x" + (value.count < 2 ? "0\(value)" : value)
    }
    var sourceText: String {
        protocolName.lowercased() == "obd2"
            ? "SAE OBD-II · \(pidText)"
            : "\(protocolName.uppercased()) · \(pidText)"
    }
}

struct LinkDiagnosticModule: Identifiable {
    let id: String
    let name: String
    let designation: String
    let network: String
    let kind: String
    let protocolName: String
    let requestCANIdentifier: UInt32
    let responseCANIdentifier: UInt32
    let extendedID: Bool
    let identityText: String?
    let partNumber: String?
    let softwareNumber: String?
    let hardwareNumber: String?
    let faultStatus: String
    let faultCount: Int
    let faults: [String]
    let evidenceDetails: [String]
    let obdAdvertisedPIDCount: Int
    let livePIDCount: Int

    var addressText: String {
        if extendedID {
            return String(format: "0x%08X → 0x%08X",
                          requestCANIdentifier, responseCANIdentifier)
        }
        return String(format: "0x%03X → 0x%03X",
                      requestCANIdentifier, responseCANIdentifier)
    }

    var faultCountLabel: String {
        if faultCount > 0 { return "\(faultCount) fault\(faultCount == 1 ? "" : "s")" }
        if faultStatus == "Checked · no faults" { return "0 faults" }
        return "faults unknown"
    }
}

struct LinkPIDConfigurationItem: Identifiable {
    let id: String
    let pid: UInt8
    let shortName: String
    let title: String
    let pollingEnabled: Bool
    let favourite: Bool
    let advertised: Bool
}

struct LinkSavedVehicleProfileSummary: Identifiable {
    let id: String
    let vin: String
    let displayName: String
    let moduleCount: Int
    let responderCount: Int
    let updatedAt: Date?
}

struct LinkDiagnosticFault: Identifiable {
    let code: String
    let title: String
    let system: String
    let category: String
    let origin: String
    let source: String
    let state: String
    let definitionKnown: Bool

    var id: String { "\(state):\(code)" }
    var displayText: String { "\(code) — \(title)" }
}

struct LinkVehicleFact: Identifiable {
    let label: String
    let value: String
    var monospaced = false
    var id: String { label }
}

struct LinkInfoRow: View {
    @Environment(\.horizontalSizeClass) private var horizontalSizeClass
    @Environment(\.linkDiagnosticTheme) private var theme

    let label: String
    let value: String
    var monospaced = false

    private var valueText: some View {
        Text(LocalizedStringKey(value))
            .font(monospaced ? theme.typography.subheadline : theme.typography.subheadlineBold)
            .foregroundStyle(theme.primaryText)
            .fixedSize(horizontal: false, vertical: true)
            .textSelection(.enabled)
    }

    var body: some View {
        Group {
            if horizontalSizeClass == .compact {
                VStack(alignment: .leading, spacing: 5) {
                    Text(LocalizedStringKey(label))
                        .font(theme.typography.captionBold)
                        .foregroundStyle(theme.mutedText)
                        .textCase(.uppercase)
                        .tracking(0.45)
                    valueText
                        .multilineTextAlignment(.leading)
                        .frame(maxWidth: .infinity, alignment: .leading)
                }
            } else {
                HStack(alignment: .firstTextBaseline, spacing: 14) {
                    Text(LocalizedStringKey(label))
                        .font(theme.typography.subheadline)
                        .foregroundStyle(theme.mutedText)
                        .fixedSize(horizontal: true, vertical: false)
                    Spacer(minLength: 16)
                    valueText
                        .multilineTextAlignment(.trailing)
                        .frame(maxWidth: 420, alignment: .trailing)
                }
            }
        }
        .padding(.vertical, 6)
    }
}

private struct LinkVehicleFactTile: View {
    @Environment(\.linkDiagnosticTheme) private var theme
    let fact: LinkVehicleFact

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            Text(LocalizedStringKey(fact.label)).textCase(.uppercase)
                .font(theme.typography.caption2Bold)
                .tracking(0.8)
                .foregroundStyle(theme.mutedText)
            Text(fact.value)
                .font(fact.monospaced ? theme.typography.subheadline : theme.typography.subheadlineBold)
                .foregroundStyle(theme.primaryText)
                .lineLimit(3)
                .minimumScaleFactor(0.8)
                .textSelection(.enabled)
        }
        .frame(maxWidth: .infinity, minHeight: 64, alignment: .topLeading)
        .padding(12)
        .background(
            RoundedRectangle(cornerRadius: 12, style: .continuous)
                .fill(theme.panelRaised))
        .overlay(
            RoundedRectangle(cornerRadius: 12, style: .continuous)
                .stroke(theme.border.opacity(0.75), lineWidth: 1))
    }
}

struct LinkVehicleFactGrid: View {
    let facts: [LinkVehicleFact]
    private let columns = [
        GridItem(.adaptive(minimum: 132, maximum: 260), spacing: 10)
    ]

    var body: some View {
        LazyVGrid(columns: columns, alignment: .leading, spacing: 10) {
            ForEach(facts) { fact in LinkVehicleFactTile(fact: fact) }
        }
    }
}

struct LinkMetricTile: View {
    @Environment(\.linkDiagnosticTheme) private var theme
    let parameter: LinkDiagnosticParameter

    var body: some View {
        VStack(alignment: .leading, spacing: 9) {
            HStack {
                Text(LocalizedStringKey(parameter.shortName)).textCase(.uppercase)
                    .font(theme.typography.caption2Bold)
                    .tracking(0.7)
                    .foregroundStyle(theme.secondaryText)
                Spacer()
                Text(parameter.pidText)
                    .font(theme.typography.caption2)
                    .foregroundStyle(theme.mutedText)
            }
            Text(parameter.presentationValue)
                .font(theme.typography.title2)
                .monospacedDigit()
                .foregroundStyle(parameter.hasLiveValue ? theme.primaryText : theme.mutedText)
                .minimumScaleFactor(0.65)
                .lineLimit(1)
            Text(LocalizedStringKey(parameter.title))
                .font(theme.typography.caption)
                .foregroundStyle(theme.mutedText)
                .lineLimit(2)
            if let source = parameter.sourceLabel {
                Label(source, systemImage: "cpu")
                    .font(theme.typography.caption2Bold)
                    .foregroundStyle(theme.secondaryText)
                    .lineLimit(2)
            }
            if let qualityNote = parameter.qualityNote {
                Text(qualityNote)
                    .font(theme.typography.caption2)
                    .foregroundStyle(theme.warning)
                    .lineLimit(2)
            }
        }
        .frame(maxWidth: .infinity, minHeight: 132, alignment: .topLeading)
        .padding(14)
        .background(
            RoundedRectangle(cornerRadius: 16, style: .continuous)
                .fill(theme.panelRaised))
        .overlay(
            RoundedRectangle(cornerRadius: 16, style: .continuous)
                .stroke(theme.border, lineWidth: 1))
    }
}

'''

NATIVE_INSTALLER_TEMPLATE = r'''#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -Eeuo pipefail

product_name="__LINK_NATIVE_PRODUCT_NAME__"
product_slug="__LINK_NATIVE_PRODUCT_SLUG__"
package_name="__LINK_NATIVE_PACKAGE_NAME__"
version="__LINK_NATIVE_VERSION__"
cmake_enable_option="__LINK_NATIVE_CMAKE_ENABLE_OPTION__"
cmake_profile_option="__LINK_NATIVE_CMAKE_PROFILE_OPTION__"
cmake_package_version_option="__LINK_NATIVE_CMAKE_PACKAGE_VERSION_OPTION__"
legacy_cleanup_paths="__LINK_NATIVE_LEGACY_CLEANUP_PATHS__"
native_version="${version}+native1"
mode="install"
extract_directory=""
output_path=""
jobs="$(getconf _NPROCESSORS_ONLN 2>/dev/null || printf '2')"

usage()
{
    cat <<EOF
${product_name} ${version} native Linux builder

Usage: ${product_name}-${version}-linux-native.run [options]

Without options, this checks and installs the required Debian/Ubuntu build
packages when possible, extracts the bundled source, compiles ${product_name}
for this machine, runs its test suite, creates the Debian-managed package
${package_name} ${native_version}, and installs it through APT.

Options:
  --build-only        Build and verify the native Debian package without installing it
  --output FILE       Native .deb destination for --build-only
  --extract DIR       Extract the complete bundled source and stop
  --jobs N            Parallel build jobs (default: detected CPU count)
  -h, --help          Show this help
EOF
}

while (($#)); do
    case "$1" in
        --build-only) mode="build-only"; shift ;;
        --output)
            [[ $# -ge 2 ]] || { echo '--output requires a file path.' >&2; exit 2; }
            output_path="$2"; shift 2 ;;
        --extract)
            [[ $# -ge 2 ]] || { echo '--extract requires a directory.' >&2; exit 2; }
            mode="extract"; extract_directory="$2"; shift 2 ;;
        --jobs)
            [[ $# -ge 2 ]] || { echo '--jobs requires a positive integer.' >&2; exit 2; }
            jobs="$2"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) printf 'Unknown option: %s\n\n' "$1" >&2; usage >&2; exit 2 ;;
    esac
done

[[ "$jobs" =~ ^[1-9][0-9]*$ ]] || { echo '--jobs must be a positive integer.' >&2; exit 2; }
if [[ -n "$output_path" && "$mode" != "build-only" ]]; then
    echo '--output is valid only with --build-only.' >&2
    exit 2
fi

payload_line="$(awk '/^__LINK_NATIVE_PAYLOAD_BELOW__$/ { print NR + 1; exit }' "$0")"
[[ -n "$payload_line" ]] || { echo "The embedded ${product_name} source payload is missing." >&2; exit 1; }

extract_payload()
{
    local destination="$1"
    mkdir -p -- "$destination"
    tail -n +"$payload_line" "$0" | gzip -dc | tar -xf - -C "$destination"
    test -f "$destination/CMakeLists.txt"
    test -f "$destination/src/link/VERSION"
    test -f "$destination/src/link/src/infiltratr-common/VERSION"
}

if [[ "$mode" == "extract" ]]; then
    [[ -n "$extract_directory" ]] || { echo 'An extraction directory is required.' >&2; exit 2; }
    if [[ -e "$extract_directory" && -n "$(find "$extract_directory" -mindepth 1 -maxdepth 1 -print -quit 2>/dev/null)" ]]; then
        echo "Extraction directory is not empty: $extract_directory" >&2
        exit 1
    fi
    extract_payload "$extract_directory"
    printf '%s %s source extracted to %s\n' "$product_name" "$version" "$extract_directory"
    exit 0
fi

prerequisites_ready()
{
    local command_name
    for command_name in cmake cpack cc pkg-config glib-compile-resources dpkg dpkg-deb dpkg-query; do
        command -v "$command_name" >/dev/null 2>&1 || return 1
    done
    pkg-config --atleast-version=4.6 gtk4 >/dev/null 2>&1 || return 1
    pkg-config --exists bluez >/dev/null 2>&1 || return 1
    pkg-config --exists libusb-1.0 >/dev/null 2>&1 || return 1
    return 0
}

run_as_root()
{
    if [[ $EUID -eq 0 ]]; then "$@"
    elif command -v sudo >/dev/null 2>&1; then sudo "$@"
    else echo 'Administrator access is required and sudo is unavailable.' >&2; return 1
    fi
}

install_build_dependencies()
{
    local -a packages=(build-essential cmake dpkg-dev pkg-config libgtk-4-dev libbluetooth-dev libusb-1.0-0-dev)
    command -v apt-get >/dev/null 2>&1 || {
        echo 'Required build prerequisites are missing and apt-get is unavailable.' >&2
        return 1
    }
    echo "Installing ${product_name} native-build prerequisites..."
    run_as_root env DEBIAN_FRONTEND=noninteractive apt-get update
    run_as_root env DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends "${packages[@]}"
}

if ! prerequisites_ready; then
    echo "One or more ${product_name} native-build prerequisites are missing."
    install_build_dependencies
fi
prerequisites_ready || {
    echo "${product_name} prerequisites are still incomplete after the installation attempt." >&2
    exit 1
}

work_directory="$(mktemp -d)"
cleanup() { rm -rf -- "$work_directory"; }
trap cleanup EXIT
source_directory="$work_directory/source"
build_directory="$work_directory/build"
package_directory="$work_directory/package"
extract_payload "$source_directory"

cmake \
    -S "$source_directory" \
    -B "$build_directory" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    "-D${cmake_enable_option}=ON" \
    "-D${cmake_profile_option}=native" \
    "-D${cmake_package_version_option}=${native_version}" \
    -DBUILD_TESTING=ON
cmake --build "$build_directory" --parallel "$jobs"
ctest --test-dir "$build_directory" --output-on-failure --parallel "$jobs"

mkdir -p "$package_directory"
cpack --config "$build_directory/CPackConfig.cmake" -G DEB -B "$package_directory"
package_path="$(find "$package_directory" -maxdepth 1 -type f -name '*.deb' -print -quit)"
[[ -n "$package_path" && -s "$package_path" ]] || { echo 'Native Debian package was not produced.' >&2; exit 1; }
[[ "$(dpkg-deb -f "$package_path" Package)" == "$package_name" ]] || { echo 'Native package has the wrong package name.' >&2; exit 1; }
[[ "$(dpkg-deb -f "$package_path" Version)" == "$native_version" ]] || { echo 'Native package has the wrong version.' >&2; exit 1; }
[[ "$(dpkg-deb -f "$package_path" Architecture)" == "$(dpkg --print-architecture)" ]] || { echo 'Native package has the wrong architecture.' >&2; exit 1; }

if [[ "$mode" == "build-only" ]]; then
    if [[ -z "$output_path" ]]; then
        output_path="$PWD/${package_name}_${native_version}_$(dpkg --print-architecture).deb"
    fi
    mkdir -p "$(dirname "$output_path")"
    install -m 0644 "$package_path" "$output_path"
    printf 'Native %s Debian package created: %s\n' "$product_name" "$output_path"
    exit 0
fi

(
    cd "$(dirname "$package_path")"
    run_as_root apt-get install -y "./$(basename "$package_path")"
)

if [[ -n "$legacy_cleanup_paths" ]]; then
    IFS=':' read -r -a cleanup_paths <<< "$legacy_cleanup_paths"
    for old_path in "${cleanup_paths[@]}"; do
        [[ -n "$old_path" ]] && run_as_root rm -rf -- "$old_path"
    done
fi

installed_version="$(dpkg-query -W -f='${Version}' "$package_name" 2>/dev/null || true)"
[[ "$installed_version" == "$native_version" ]] || {
    printf 'Native package installation verification failed: expected %s, found %s\n' "$native_version" "$installed_version" >&2
    exit 1
}

printf '%s %s was compiled locally, tested, packaged and installed as %s %s.\n' \
    "$product_name" "$version" "$package_name" "$native_version"
printf 'APT now owns the native installation and will offer only a genuinely newer release.\n'
exit 0
__LINK_NATIVE_PAYLOAD_BELOW__
'''

# 1. Rich shared units.
write("include/link/units.h", UNITS_H)
write("src/core/units.c", UNITS_C)
write("tests/test_units.c", TEST_UNITS)

# 2. Shared session trace/history mechanics.
write("include/link/session_trace.h", SESSION_TRACE_H)
write("src/core/session_trace.c", SESSION_TRACE_C)
write("tests/test_session_trace.c", TEST_SESSION_TRACE)
replace_once(
    "CMakeLists.txt",
    "    src/core/telemetry.c\n    src/core/transport.c",
    "    src/core/telemetry.c\n    src/core/session_trace.c\n    src/core/transport.c")
replace_once(
    "CMakeLists.txt",
    "    link_add_test(link-test-units tests/test_units.c link-units)\n",
    "    link_add_test(link-test-units tests/test_units.c link-units)\n"
    "    link_add_test(link-test-session-trace tests/test_session_trace.c link-session-trace)\n")
replace_once(
    "platform/apple/LinkPortableCore.c",
    '#include "../../src/core/telemetry.c"\n#include "../../src/core/transport.c"',
    '#include "../../src/core/telemetry.c"\n#include "../../src/core/session_trace.c"\n#include "../../src/core/transport.c"')

# 3. Standards-defined discovery target constructors.
replace_once(
    "include/link/discover.h",
    "int link_discover_sweep_target_is_valid(\n    const link_discover_sweep_target *target);\n",
    DISCOVER_DECLS + "\nint link_discover_sweep_target_is_valid(\n    const link_discover_sweep_target *target);\n")
replace_once(
    "src/discover/sweep.c",
    "static int sweep_probe_is_valid(const link_discover_sweep_probe *probe)\n",
    DISCOVER_IMPL + "static int sweep_probe_is_valid(const link_discover_sweep_probe *probe)\n")
write("tests/test_discover_targets.c", TEST_DISCOVER_TARGETS)
replace_once(
    "CMakeLists.txt",
    "    link_add_test(link-test-units tests/test_units.c link-units)\n"
    "    link_add_test(link-test-session-trace tests/test_session_trace.c link-session-trace)\n",
    "    link_add_test(link-test-units tests/test_units.c link-units)\n"
    "    link_add_test(link-test-session-trace tests/test_session_trace.c link-session-trace)\n"
    "    link_add_test(link-test-discover-targets tests/test_discover_targets.c link-discover-targets)\n")

# 4. Vehicle capability learning/restoration belongs with the shared profile store.
replace_once(
    "platform/apple/LinkDiagnosticsController.h",
    "- (void)saveProfile:(NSDictionary *)profile forVIN:(NSString *)vin;\n"
    "- (void)removeProfileForVIN:(NSString *)vin;\n",
    "- (void)saveProfile:(NSDictionary *)profile forVIN:(NSString *)vin;\n"
    "- (void)removeProfileForVIN:(NSString *)vin;\n\n" + APPLE_PROFILE_DECLS)
replace_once(
    "platform/apple/LinkDiagnosticsController.m",
    "@implementation LinkVehicleProfileStore\n",
    APPLE_PROFILE_IMPL + "@implementation LinkVehicleProfileStore\n")

# 5. Shared Apple diagnostic models and reusable data/vehicle tiles.
ui = read("platform/apple/LinkDiagnosticUI.swift")
if "import Foundation\n" not in ui:
    ui = ui.replace("import SwiftUI\n", "import SwiftUI\nimport Foundation\n", 1)
marker = "/**\n * LINK-owned connection source chooser shared by product faces.\n"
if ui.count(marker) != 1:
    raise SystemExit("LinkDiagnosticUI.swift: connection chooser marker changed")
ui = ui.replace(marker, SWIFT_SHARED_MODELS + marker, 1)
write("platform/apple/LinkDiagnosticUI.swift", ui)

# 6. Parameterised native Linux self-builder template for every LINK face.
write("packaging/native-product-installer.sh.in", NATIVE_INSTALLER_TEMPLATE)

print("LINK migration source edits completed")
