// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file fuel_economy.h
 * @brief Product-neutral fuel economy derivation with source provenance.
 *
 * LINK deliberately distinguishes factory values from standards-based OBD-II
 * values and estimates. Manufacturer repositories may feed verified factory
 * fuel-flow, fuel-used, distance or direct-consumption observations without
 * teaching LINK any Mercedes/Jaguar-specific address or payload layout.
 */
#ifndef LINK_FUEL_ECONOMY_H
#define LINK_FUEL_ECONOMY_H

#include "link/obd2.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LINK_FUEL_ECONOMY_STALE_MS UINT64_C(3000)
#define LINK_FUEL_ECONOMY_MAX_INTEGRATION_GAP_MS UINT64_C(5000)
#define LINK_FUEL_ECONOMY_MIN_SPEED_KMH 1.0

typedef enum LinkFuelEconomySource {
    LINK_FUEL_ECONOMY_SOURCE_NONE = 0,
    LINK_FUEL_ECONOMY_SOURCE_FACTORY_DIRECT,
    LINK_FUEL_ECONOMY_SOURCE_FACTORY_COUNTERS,
    LINK_FUEL_ECONOMY_SOURCE_FACTORY_RATE,
    LINK_FUEL_ECONOMY_SOURCE_SAE_OBD2,
    LINK_FUEL_ECONOMY_SOURCE_ESTIMATED,
    LINK_FUEL_ECONOMY_SOURCE_MIXED
} LinkFuelEconomySource;

typedef struct LinkFuelEconomySnapshot {
    bool instantaneous_available;
    double instantaneous_l_per_100km;
    LinkFuelEconomySource instantaneous_source;

    bool fuel_rate_available;
    double fuel_rate_l_per_hour;
    LinkFuelEconomySource fuel_rate_source;

    bool average_available;
    double average_l_per_100km;
    LinkFuelEconomySource average_source;

    double trip_fuel_litres;
    double trip_distance_km;
    bool moving;

    /** Borrowed provenance supplied with the selected factory observation. */
    const char *factory_provenance;
} LinkFuelEconomySnapshot;

typedef struct LinkFuelEconomy {
    bool speed_valid;
    double speed_kmh;
    uint64_t speed_timestamp_ms;

    bool sae_rate_valid;
    double sae_rate_l_per_hour;
    uint64_t sae_rate_timestamp_ms;

    bool factory_rate_valid;
    double factory_rate_l_per_hour;
    uint64_t factory_rate_timestamp_ms;
    const char *factory_rate_provenance;

    bool factory_direct_valid;
    double factory_direct_l_per_100km;
    uint64_t factory_direct_timestamp_ms;
    const char *factory_direct_provenance;

    bool estimated_rate_valid;
    double estimated_rate_l_per_hour;
    uint64_t estimated_rate_timestamp_ms;
    const char *estimated_provenance;

    bool integration_started;
    uint64_t integration_timestamp_ms;
    double integrated_trip_fuel_litres;
    double integrated_trip_distance_km;
    LinkFuelEconomySource integrated_trip_source;

    bool factory_total_fuel_valid;
    double factory_total_fuel_litres;
    uint64_t factory_total_fuel_timestamp_ms;
    const char *factory_total_fuel_provenance;
    double factory_counter_trip_fuel_litres;

    bool factory_total_distance_valid;
    double factory_total_distance_km;
    uint64_t factory_total_distance_timestamp_ms;
    const char *factory_total_distance_provenance;
    double factory_counter_trip_distance_km;
} LinkFuelEconomy;

void link_fuel_economy_init(LinkFuelEconomy *economy);
void link_fuel_economy_reset_trip(LinkFuelEconomy *economy,
                                  uint64_t now_ms);

/**
 * Feed one standard OBD-II sample. Only vehicle speed (01/0D) and engine fuel
 * rate (01/5E) are consumed; all other PIDs return false and leave state intact.
 */
bool link_fuel_economy_observe_obd2(LinkFuelEconomy *economy,
                                    const LinkObd2Sample *sample,
                                    uint64_t timestamp_ms);

/** Feed a verified manufacturer fuel-rate value in litres/hour. */
bool link_fuel_economy_observe_factory_rate(LinkFuelEconomy *economy,
                                            double litres_per_hour,
                                            uint64_t timestamp_ms,
                                            const char *provenance);

/** Feed a verified manufacturer instantaneous litres/100 km value. */
bool link_fuel_economy_observe_factory_instant(
    LinkFuelEconomy *economy,
    double litres_per_100km,
    uint64_t timestamp_ms,
    const char *provenance);

/**
 * Feed a monotonically increasing manufacturer total-fuel-used counter.
 * Counter rollback is treated as a new baseline rather than negative fuel use.
 */
bool link_fuel_economy_observe_factory_total_fuel(
    LinkFuelEconomy *economy,
    double total_litres,
    uint64_t timestamp_ms,
    const char *provenance);

/** Feed a monotonically increasing manufacturer total-distance counter. */
bool link_fuel_economy_observe_factory_total_distance(
    LinkFuelEconomy *economy,
    double total_km,
    uint64_t timestamp_ms,
    const char *provenance);

/**
 * Feed an explicitly estimated fuel rate. Estimates always rank below factory
 * and SAE measurements and are reported as estimated in the snapshot source.
 */
bool link_fuel_economy_observe_estimated_rate(LinkFuelEconomy *economy,
                                              double litres_per_hour,
                                              uint64_t timestamp_ms,
                                              const char *provenance);

/** Advance trip integration using the latest fresh observations. */
void link_fuel_economy_tick(LinkFuelEconomy *economy, uint64_t now_ms);

/** Resolve the best current source and trip average at @p now_ms. */
LinkFuelEconomySnapshot link_fuel_economy_snapshot(
    const LinkFuelEconomy *economy,
    uint64_t now_ms);

const char *link_fuel_economy_source_name(LinkFuelEconomySource source);
bool link_fuel_economy_source_is_factory(LinkFuelEconomySource source);

#ifdef __cplusplus
}
#endif

#endif
