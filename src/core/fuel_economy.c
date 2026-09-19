// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/fuel_economy.h"

#include "infiltratr/format.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static bool finite_nonnegative(double value)
{
    return isfinite(value) && value >= 0.0;
}

static bool fresh(bool valid, uint64_t timestamp_ms, uint64_t now_ms)
{
    return valid && now_ms >= timestamp_ms &&
           now_ms - timestamp_ms <= LINK_FUEL_ECONOMY_STALE_MS;
}

static LinkFuelEconomySource merge_source(LinkFuelEconomySource current,
                                          LinkFuelEconomySource next)
{
    if (next == LINK_FUEL_ECONOMY_SOURCE_NONE) return current;
    if (current == LINK_FUEL_ECONOMY_SOURCE_NONE) return next;
    if (current == next) return current;
    return LINK_FUEL_ECONOMY_SOURCE_MIXED;
}

static bool speed_now(const LinkFuelEconomy *economy,
                      uint64_t now_ms,
                      double *speed_kmh)
{
    if (economy == NULL || speed_kmh == NULL ||
        !fresh(economy->speed_valid, economy->speed_timestamp_ms, now_ms)) {
        return false;
    }
    *speed_kmh = economy->speed_kmh;
    return true;
}

static bool rate_now(const LinkFuelEconomy *economy,
                     uint64_t now_ms,
                     double *litres_per_hour,
                     LinkFuelEconomySource *source,
                     const char **provenance)
{
    if (economy == NULL || litres_per_hour == NULL || source == NULL) {
        return false;
    }
    if (fresh(economy->factory_rate_valid,
              economy->factory_rate_timestamp_ms, now_ms)) {
        *litres_per_hour = economy->factory_rate_l_per_hour;
        *source = LINK_FUEL_ECONOMY_SOURCE_FACTORY_RATE;
        if (provenance != NULL) *provenance = economy->factory_rate_provenance;
        return true;
    }
    if (fresh(economy->sae_rate_valid,
              economy->sae_rate_timestamp_ms, now_ms)) {
        *litres_per_hour = economy->sae_rate_l_per_hour;
        *source = LINK_FUEL_ECONOMY_SOURCE_SAE_OBD2;
        if (provenance != NULL) *provenance = NULL;
        return true;
    }
    if (fresh(economy->estimated_rate_valid,
              economy->estimated_rate_timestamp_ms, now_ms)) {
        *litres_per_hour = economy->estimated_rate_l_per_hour;
        *source = LINK_FUEL_ECONOMY_SOURCE_ESTIMATED;
        if (provenance != NULL) *provenance = economy->estimated_provenance;
        return true;
    }
    return false;
}

static bool integration_rate_now(const LinkFuelEconomy *economy,
                                 uint64_t now_ms,
                                 double speed_kmh,
                                 double *litres_per_hour,
                                 LinkFuelEconomySource *source)
{
    const char *unused = NULL;
    if (economy == NULL || litres_per_hour == NULL || source == NULL) {
        return false;
    }
    if (speed_kmh >= LINK_FUEL_ECONOMY_MIN_SPEED_KMH &&
        fresh(economy->factory_direct_valid,
              economy->factory_direct_timestamp_ms, now_ms)) {
        const double rate =
            economy->factory_direct_l_per_100km * speed_kmh / 100.0;
        if (!finite_nonnegative(rate)) return false;
        *litres_per_hour = rate;
        *source = LINK_FUEL_ECONOMY_SOURCE_FACTORY_DIRECT;
        return true;
    }
    return rate_now(economy, now_ms, litres_per_hour, source, &unused);
}

static void integrate_to(LinkFuelEconomy *economy, uint64_t now_ms)
{
    uint64_t elapsed_ms;
    double speed_kmh;
    double litres_per_hour;
    double hours;
    double distance_delta;
    double fuel_delta;
    LinkFuelEconomySource source = LINK_FUEL_ECONOMY_SOURCE_NONE;

    if (economy == NULL) return;
    if (!economy->integration_started) {
        economy->integration_started = true;
        economy->integration_timestamp_ms = now_ms;
        return;
    }
    if (now_ms <= economy->integration_timestamp_ms) return;
    elapsed_ms = now_ms - economy->integration_timestamp_ms;
    economy->integration_timestamp_ms = now_ms;
    if (elapsed_ms > LINK_FUEL_ECONOMY_MAX_INTEGRATION_GAP_MS) return;
    if (!speed_now(economy, now_ms, &speed_kmh)) return;
    if (!integration_rate_now(economy, now_ms, speed_kmh,
                              &litres_per_hour, &source)) {
        return;
    }

    hours = (double)elapsed_ms / 3600000.0;
    distance_delta = speed_kmh * hours;
    fuel_delta = litres_per_hour * hours;
    if (!finite_nonnegative(distance_delta) || !finite_nonnegative(fuel_delta) ||
        !isfinite(economy->integrated_trip_distance_km + distance_delta) ||
        !isfinite(economy->integrated_trip_fuel_litres + fuel_delta)) {
        return;
    }
    economy->integrated_trip_distance_km += distance_delta;
    economy->integrated_trip_fuel_litres += fuel_delta;
    economy->integrated_trip_source =
        merge_source(economy->integrated_trip_source, source);
}

void link_fuel_economy_init(LinkFuelEconomy *economy)
{
    if (economy != NULL) memset(economy, 0, sizeof(*economy));
}

void link_fuel_economy_reset_trip(LinkFuelEconomy *economy,
                                  uint64_t now_ms)
{
    if (economy == NULL) return;
    economy->integration_started = true;
    economy->integration_timestamp_ms = now_ms;
    economy->integrated_trip_fuel_litres = 0.0;
    economy->integrated_trip_distance_km = 0.0;
    economy->integrated_trip_source = LINK_FUEL_ECONOMY_SOURCE_NONE;
    economy->factory_counter_trip_fuel_litres = 0.0;
    economy->factory_counter_trip_distance_km = 0.0;
}

bool link_fuel_economy_observe_obd2(LinkFuelEconomy *economy,
                                    const LinkObd2Sample *sample,
                                    uint64_t timestamp_ms)
{
    if (economy == NULL || sample == NULL || !finite_nonnegative(sample->value)) {
        return false;
    }
    if (sample->pid == 0x0dU) {
        if (sample->unit != LINK_OBD2_UNIT_KMH) return false;
        integrate_to(economy, timestamp_ms);
        economy->speed_valid = true;
        economy->speed_kmh = sample->value;
        economy->speed_timestamp_ms = timestamp_ms;
        return true;
    }
    if (sample->pid == 0x5eU) {
        if (sample->unit != LINK_OBD2_UNIT_LITRES_PER_HOUR) return false;
        integrate_to(economy, timestamp_ms);
        economy->sae_rate_valid = true;
        economy->sae_rate_l_per_hour = sample->value;
        economy->sae_rate_timestamp_ms = timestamp_ms;
        return true;
    }
    return false;
}

bool link_fuel_economy_observe_factory_rate(LinkFuelEconomy *economy,
                                            double litres_per_hour,
                                            uint64_t timestamp_ms,
                                            const char *provenance)
{
    if (economy == NULL || !finite_nonnegative(litres_per_hour)) return false;
    integrate_to(economy, timestamp_ms);
    economy->factory_rate_valid = true;
    economy->factory_rate_l_per_hour = litres_per_hour;
    economy->factory_rate_timestamp_ms = timestamp_ms;
    economy->factory_rate_provenance = provenance;
    return true;
}

bool link_fuel_economy_observe_factory_instant(
    LinkFuelEconomy *economy,
    double litres_per_100km,
    uint64_t timestamp_ms,
    const char *provenance)
{
    if (economy == NULL || !finite_nonnegative(litres_per_100km)) return false;
    integrate_to(economy, timestamp_ms);
    economy->factory_direct_valid = true;
    economy->factory_direct_l_per_100km = litres_per_100km;
    economy->factory_direct_timestamp_ms = timestamp_ms;
    economy->factory_direct_provenance = provenance;
    return true;
}

bool link_fuel_economy_observe_factory_total_fuel(
    LinkFuelEconomy *economy,
    double total_litres,
    uint64_t timestamp_ms,
    const char *provenance)
{
    double delta;
    if (economy == NULL || !finite_nonnegative(total_litres)) return false;
    integrate_to(economy, timestamp_ms);
    if (economy->factory_total_fuel_valid &&
        total_litres >= economy->factory_total_fuel_litres) {
        delta = total_litres - economy->factory_total_fuel_litres;
        if (finite_nonnegative(delta) &&
            isfinite(economy->factory_counter_trip_fuel_litres + delta)) {
            economy->factory_counter_trip_fuel_litres += delta;
        }
    }
    economy->factory_total_fuel_valid = true;
    economy->factory_total_fuel_litres = total_litres;
    economy->factory_total_fuel_timestamp_ms = timestamp_ms;
    economy->factory_total_fuel_provenance = provenance;
    return true;
}

bool link_fuel_economy_observe_factory_total_distance(
    LinkFuelEconomy *economy,
    double total_km,
    uint64_t timestamp_ms,
    const char *provenance)
{
    double delta;
    if (economy == NULL || !finite_nonnegative(total_km)) return false;
    integrate_to(economy, timestamp_ms);
    if (economy->factory_total_distance_valid &&
        total_km >= economy->factory_total_distance_km) {
        delta = total_km - economy->factory_total_distance_km;
        if (finite_nonnegative(delta) &&
            isfinite(economy->factory_counter_trip_distance_km + delta)) {
            economy->factory_counter_trip_distance_km += delta;
        }
    }
    economy->factory_total_distance_valid = true;
    economy->factory_total_distance_km = total_km;
    economy->factory_total_distance_timestamp_ms = timestamp_ms;
    economy->factory_total_distance_provenance = provenance;
    return true;
}

bool link_fuel_economy_observe_estimated_rate(LinkFuelEconomy *economy,
                                              double litres_per_hour,
                                              uint64_t timestamp_ms,
                                              const char *provenance)
{
    if (economy == NULL || !finite_nonnegative(litres_per_hour)) return false;
    integrate_to(economy, timestamp_ms);
    economy->estimated_rate_valid = true;
    economy->estimated_rate_l_per_hour = litres_per_hour;
    economy->estimated_rate_timestamp_ms = timestamp_ms;
    economy->estimated_provenance = provenance;
    return true;
}

void link_fuel_economy_tick(LinkFuelEconomy *economy, uint64_t now_ms)
{
    integrate_to(economy, now_ms);
}

LinkFuelEconomySnapshot link_fuel_economy_snapshot(
    const LinkFuelEconomy *economy,
    uint64_t now_ms)
{
    LinkFuelEconomySnapshot snapshot;
    double speed_kmh = 0.0;
    double rate = 0.0;
    LinkFuelEconomySource rate_source = LINK_FUEL_ECONOMY_SOURCE_NONE;
    const char *rate_provenance = NULL;

    memset(&snapshot, 0, sizeof(snapshot));
    if (economy == NULL) return snapshot;

    if (speed_now(economy, now_ms, &speed_kmh)) {
        snapshot.moving = speed_kmh >= LINK_FUEL_ECONOMY_MIN_SPEED_KMH;
    }

    if (snapshot.moving &&
        fresh(economy->factory_direct_valid,
              economy->factory_direct_timestamp_ms, now_ms)) {
        snapshot.instantaneous_available = true;
        snapshot.instantaneous_l_per_100km =
            economy->factory_direct_l_per_100km;
        snapshot.instantaneous_source =
            LINK_FUEL_ECONOMY_SOURCE_FACTORY_DIRECT;
        snapshot.factory_provenance = economy->factory_direct_provenance;
    } else if (snapshot.moving &&
               rate_now(economy, now_ms, &rate, &rate_source,
                        &rate_provenance)) {
        const double consumption = 100.0 * rate / speed_kmh;
        if (finite_nonnegative(consumption)) {
            snapshot.instantaneous_available = true;
            snapshot.instantaneous_l_per_100km = consumption;
            snapshot.instantaneous_source = rate_source;
            if (link_fuel_economy_source_is_factory(rate_source))
                snapshot.factory_provenance = rate_provenance;
        }
    }

    if (rate_now(economy, now_ms, &rate, &rate_source, &rate_provenance)) {
        snapshot.fuel_rate_available = true;
        snapshot.fuel_rate_l_per_hour = rate;
        snapshot.fuel_rate_source = rate_source;
        if (snapshot.factory_provenance == NULL &&
            link_fuel_economy_source_is_factory(rate_source)) {
            snapshot.factory_provenance = rate_provenance;
        }
    } else if (snapshot.moving &&
               fresh(economy->factory_direct_valid,
                     economy->factory_direct_timestamp_ms, now_ms)) {
        rate = economy->factory_direct_l_per_100km * speed_kmh / 100.0;
        if (finite_nonnegative(rate)) {
            snapshot.fuel_rate_available = true;
            snapshot.fuel_rate_l_per_hour = rate;
            snapshot.fuel_rate_source =
                LINK_FUEL_ECONOMY_SOURCE_FACTORY_DIRECT;
            if (snapshot.factory_provenance == NULL)
                snapshot.factory_provenance = economy->factory_direct_provenance;
        }
    }

    if (economy->factory_counter_trip_distance_km > 0.0 &&
        economy->factory_counter_trip_fuel_litres >= 0.0) {
        const double average =
            100.0 * economy->factory_counter_trip_fuel_litres /
            economy->factory_counter_trip_distance_km;
        if (finite_nonnegative(average)) {
            snapshot.average_available = true;
            snapshot.average_l_per_100km = average;
            snapshot.average_source = LINK_FUEL_ECONOMY_SOURCE_FACTORY_COUNTERS;
            snapshot.trip_fuel_litres = economy->factory_counter_trip_fuel_litres;
            snapshot.trip_distance_km = economy->factory_counter_trip_distance_km;
            if (snapshot.factory_provenance == NULL)
                snapshot.factory_provenance = economy->factory_total_fuel_provenance;
        }
    }

    if (!snapshot.average_available && economy->integrated_trip_distance_km > 0.0) {
        const double average =
            100.0 * economy->integrated_trip_fuel_litres /
            economy->integrated_trip_distance_km;
        if (finite_nonnegative(average)) {
            snapshot.average_available = true;
            snapshot.average_l_per_100km = average;
            snapshot.average_source = economy->integrated_trip_source;
            snapshot.trip_fuel_litres = economy->integrated_trip_fuel_litres;
            snapshot.trip_distance_km = economy->integrated_trip_distance_km;
        }
    }

    if (!snapshot.average_available) {
        snapshot.trip_fuel_litres = economy->integrated_trip_fuel_litres;
        snapshot.trip_distance_km = economy->integrated_trip_distance_km;
    }
    return snapshot;
}

static bool format_value_unit(double value,
                              unsigned int decimals,
                              const char *unit,
                              char *buffer,
                              size_t capacity)
{
    char number[64];
    int written;
    if (buffer == NULL || capacity == 0U || unit == NULL ||
        !infiltratr_format_fixed_ascii(value, decimals, number, sizeof(number))) {
        if (buffer != NULL && capacity != 0U) buffer[0] = '\0';
        return false;
    }
    written = snprintf(buffer, capacity, "%s %s", number, unit);
    if (written < 0 || (size_t)written >= capacity) {
        buffer[0] = '\0';
        return false;
    }
    return true;
}

static bool format_fuel_economy_value(
    double litres_per_100km,
    LinkFuelEconomyUnit preference,
    char *buffer,
    size_t capacity)
{
    double display;
    const char *unit;
    if (!link_units_convert_fuel_economy(
            litres_per_100km, preference, &display, &unit)) {
        return false;
    }
    return format_value_unit(display, 1U, unit, buffer, capacity);
}

static bool format_fuel_rate_value(
    double litres_per_hour,
    LinkFuelRateUnit preference,
    char *buffer,
    size_t capacity)
{
    double display;
    const char *unit;
    if (!link_units_convert_fuel_rate(
            litres_per_hour, preference, &display, &unit)) {
        return false;
    }
    return format_value_unit(display, 2U, unit, buffer, capacity);
}

static bool format_fuel_volume_value(
    double litres,
    LinkFuelVolumeUnit preference,
    char *buffer,
    size_t capacity)
{
    double display;
    const char *unit;
    if (!link_units_convert_fuel_volume(
            litres, preference, &display, &unit)) {
        return false;
    }
    return format_value_unit(display, 2U, unit, buffer, capacity);
}

static bool format_distance_value(
    double kilometres,
    LinkDistanceUnit preference,
    char *buffer,
    size_t capacity)
{
    double display;
    const char *unit;
    if (!link_units_convert_distance(
            kilometres, preference, &display, &unit)) {
        return false;
    }
    return format_value_unit(display, 1U, unit, buffer, capacity);
}

bool link_fuel_economy_format_display(
    const LinkFuelEconomySnapshot *snapshot,
    const LinkUnitPreferences *preferences,
    bool connected,
    LinkFuelEconomyDisplay *display)
{
    char volume[48];
    char distance[48];
    int written;

    if (display != NULL) memset(display, 0, sizeof(*display));
    if (snapshot == NULL || preferences == NULL || display == NULL)
        return false;

    if (snapshot->instantaneous_available) {
        if (!format_fuel_economy_value(
                snapshot->instantaneous_l_per_100km,
                preferences->fuel_economy,
                display->instantaneous, sizeof(display->instantaneous))) {
            return false;
        }
    } else {
        const char *text = connected && !snapshot->moving
            ? "— · stationary / awaiting speed"
            : "Waiting for measured fuel data";
        if (snprintf(display->instantaneous, sizeof(display->instantaneous),
                     "%s", text) < 0) {
            return false;
        }
    }

    if (snapshot->average_available) {
        if (!format_fuel_economy_value(
                snapshot->average_l_per_100km,
                preferences->fuel_economy,
                display->average, sizeof(display->average))) {
            return false;
        }
    } else if (snprintf(display->average, sizeof(display->average),
                        "%s", "Waiting for trip distance") < 0) {
        return false;
    }

    if (snapshot->fuel_rate_available) {
        if (!format_fuel_rate_value(
                snapshot->fuel_rate_l_per_hour,
                preferences->fuel_rate,
                display->fuel_rate, sizeof(display->fuel_rate))) {
            return false;
        }
    } else if (snprintf(display->fuel_rate, sizeof(display->fuel_rate),
                        "%s", "Not available") < 0) {
        return false;
    }

    if (!format_fuel_volume_value(
            snapshot->trip_fuel_litres, preferences->fuel_volume,
            volume, sizeof(volume)) ||
        !format_distance_value(
            snapshot->trip_distance_km, preferences->distance,
            distance, sizeof(distance))) {
        return false;
    }
    written = snprintf(display->trip, sizeof(display->trip),
                       "%s over %s", volume, distance);
    if (written < 0 || (size_t)written >= sizeof(display->trip)) {
        memset(display, 0, sizeof(*display));
        return false;
    }
    return true;
}

const char *link_fuel_economy_source_name(LinkFuelEconomySource source)
{
    switch (source) {
    case LINK_FUEL_ECONOMY_SOURCE_NONE: return "none";
    case LINK_FUEL_ECONOMY_SOURCE_FACTORY_DIRECT: return "factory-direct";
    case LINK_FUEL_ECONOMY_SOURCE_FACTORY_COUNTERS: return "factory-counters";
    case LINK_FUEL_ECONOMY_SOURCE_FACTORY_RATE: return "factory-rate";
    case LINK_FUEL_ECONOMY_SOURCE_SAE_OBD2: return "sae-obd2";
    case LINK_FUEL_ECONOMY_SOURCE_ESTIMATED: return "estimated";
    case LINK_FUEL_ECONOMY_SOURCE_MIXED: return "mixed";
    }
    return "unknown";
}

bool link_fuel_economy_source_is_factory(LinkFuelEconomySource source)
{
    return source == LINK_FUEL_ECONOMY_SOURCE_FACTORY_DIRECT ||
           source == LINK_FUEL_ECONOMY_SOURCE_FACTORY_COUNTERS ||
           source == LINK_FUEL_ECONOMY_SOURCE_FACTORY_RATE;
}
