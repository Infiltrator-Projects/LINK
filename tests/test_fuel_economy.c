// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/fuel_economy.h"

#include <math.h>
#include <stdio.h>

static int failures = 0;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        failures++; \
    } \
} while (0)

static bool near_value(double value, double expected, double tolerance)
{
    return fabs(value - expected) <= tolerance;
}

static LinkObd2Sample sample(uint8_t pid, double value, LinkObd2Unit unit)
{
    LinkObd2Sample result = {0};
    result.pid = pid;
    result.value = value;
    result.unit = unit;
    return result;
}

static void test_sae_instantaneous(void)
{
    LinkFuelEconomy economy;
    LinkFuelEconomySnapshot snapshot;
    LinkObd2Sample speed = sample(0x0dU, 80.0, LINK_OBD2_UNIT_KMH);
    LinkObd2Sample fuel = sample(0x5eU, 6.0, LINK_OBD2_UNIT_LITRES_PER_HOUR);

    link_fuel_economy_init(&economy);
    CHECK(link_fuel_economy_observe_obd2(&economy, &speed, 1000U));
    CHECK(link_fuel_economy_observe_obd2(&economy, &fuel, 1000U));
    snapshot = link_fuel_economy_snapshot(&economy, 1000U);
    CHECK(snapshot.instantaneous_available);
    CHECK(near_value(snapshot.instantaneous_l_per_100km, 7.5, 0.0001));
    CHECK(snapshot.instantaneous_source == LINK_FUEL_ECONOMY_SOURCE_SAE_OBD2);
    CHECK(snapshot.fuel_rate_available);
    CHECK(near_value(snapshot.fuel_rate_l_per_hour, 6.0, 0.0001));
}

static void test_factory_priority(void)
{
    static const char factory_rate_provenance[] = "verified factory rate";
    static const char factory_direct_provenance[] = "verified factory direct";
    LinkFuelEconomy economy;
    LinkFuelEconomySnapshot snapshot;
    LinkObd2Sample speed = sample(0x0dU, 80.0, LINK_OBD2_UNIT_KMH);
    LinkObd2Sample fuel = sample(0x5eU, 6.0, LINK_OBD2_UNIT_LITRES_PER_HOUR);

    link_fuel_economy_init(&economy);
    CHECK(link_fuel_economy_observe_obd2(&economy, &speed, 100U));
    CHECK(link_fuel_economy_observe_obd2(&economy, &fuel, 100U));
    CHECK(link_fuel_economy_observe_factory_rate(
        &economy, 4.0, 200U, factory_rate_provenance));
    snapshot = link_fuel_economy_snapshot(&economy, 200U);
    CHECK(snapshot.instantaneous_source == LINK_FUEL_ECONOMY_SOURCE_FACTORY_RATE);
    CHECK(near_value(snapshot.instantaneous_l_per_100km, 5.0, 0.0001));
    CHECK(snapshot.factory_provenance == factory_rate_provenance);

    CHECK(link_fuel_economy_observe_factory_instant(
        &economy, 4.8, 300U, factory_direct_provenance));
    snapshot = link_fuel_economy_snapshot(&economy, 300U);
    CHECK(snapshot.instantaneous_source == LINK_FUEL_ECONOMY_SOURCE_FACTORY_DIRECT);
    CHECK(near_value(snapshot.instantaneous_l_per_100km, 4.8, 0.0001));
    CHECK(snapshot.factory_provenance == factory_direct_provenance);
}

static void test_stationary_and_stale(void)
{
    LinkFuelEconomy economy;
    LinkFuelEconomySnapshot snapshot;
    LinkObd2Sample speed = sample(0x0dU, 0.0, LINK_OBD2_UNIT_KMH);
    LinkObd2Sample fuel = sample(0x5eU, 0.75, LINK_OBD2_UNIT_LITRES_PER_HOUR);

    link_fuel_economy_init(&economy);
    CHECK(link_fuel_economy_observe_obd2(&economy, &speed, 1000U));
    CHECK(link_fuel_economy_observe_obd2(&economy, &fuel, 1000U));
    snapshot = link_fuel_economy_snapshot(&economy, 1000U);
    CHECK(!snapshot.instantaneous_available);
    CHECK(snapshot.fuel_rate_available);
    CHECK(near_value(snapshot.fuel_rate_l_per_hour, 0.75, 0.0001));

    snapshot = link_fuel_economy_snapshot(
        &economy, 1000U + LINK_FUEL_ECONOMY_STALE_MS + 1U);
    CHECK(!snapshot.instantaneous_available);
    CHECK(!snapshot.fuel_rate_available);
}

static void test_trip_integration(void)
{
    LinkFuelEconomy economy;
    LinkFuelEconomySnapshot snapshot;
    LinkObd2Sample speed = sample(0x0dU, 60.0, LINK_OBD2_UNIT_KMH);
    LinkObd2Sample fuel = sample(0x5eU, 6.0, LINK_OBD2_UNIT_LITRES_PER_HOUR);

    link_fuel_economy_init(&economy);
    link_fuel_economy_reset_trip(&economy, 0U);
    CHECK(link_fuel_economy_observe_obd2(&economy, &speed, 0U));
    CHECK(link_fuel_economy_observe_obd2(&economy, &fuel, 0U));
    link_fuel_economy_tick(&economy, 1000U);
    snapshot = link_fuel_economy_snapshot(&economy, 1000U);
    CHECK(snapshot.average_available);
    CHECK(snapshot.average_source == LINK_FUEL_ECONOMY_SOURCE_SAE_OBD2);
    CHECK(near_value(snapshot.average_l_per_100km, 10.0, 0.0001));
    CHECK(near_value(snapshot.trip_distance_km, 1.0 / 60.0, 0.00001));
    CHECK(near_value(snapshot.trip_fuel_litres, 1.0 / 600.0, 0.00001));
}

static void test_factory_counters(void)
{
    static const char provenance[] = "factory counter";
    LinkFuelEconomy economy;
    LinkFuelEconomySnapshot snapshot;

    link_fuel_economy_init(&economy);
    link_fuel_economy_reset_trip(&economy, 0U);
    CHECK(link_fuel_economy_observe_factory_total_fuel(
        &economy, 100.0, 0U, provenance));
    CHECK(link_fuel_economy_observe_factory_total_distance(
        &economy, 1000.0, 0U, provenance));
    CHECK(link_fuel_economy_observe_factory_total_fuel(
        &economy, 101.0, 1000U, provenance));
    CHECK(link_fuel_economy_observe_factory_total_distance(
        &economy, 1010.0, 1000U, provenance));
    snapshot = link_fuel_economy_snapshot(&economy, 1000U);
    CHECK(snapshot.average_available);
    CHECK(snapshot.average_source == LINK_FUEL_ECONOMY_SOURCE_FACTORY_COUNTERS);
    CHECK(near_value(snapshot.average_l_per_100km, 10.0, 0.0001));
    CHECK(near_value(snapshot.trip_fuel_litres, 1.0, 0.0001));
    CHECK(near_value(snapshot.trip_distance_km, 10.0, 0.0001));
    CHECK(snapshot.factory_provenance == provenance);
}

static void test_invalid_inputs(void)
{
    LinkFuelEconomy economy;
    LinkObd2Sample wrong_speed = sample(0x0dU, 50.0, LINK_OBD2_UNIT_RPM);
    LinkObd2Sample other = sample(0x0cU, 1000.0, LINK_OBD2_UNIT_RPM);

    link_fuel_economy_init(&economy);
    CHECK(!link_fuel_economy_observe_obd2(&economy, &wrong_speed, 0U));
    CHECK(!link_fuel_economy_observe_obd2(&economy, &other, 0U));
    CHECK(!link_fuel_economy_observe_factory_rate(&economy, -1.0, 0U, NULL));
    CHECK(!link_fuel_economy_observe_factory_instant(&economy, NAN, 0U, NULL));
    CHECK(!link_fuel_economy_observe_factory_total_fuel(&economy, INFINITY, 0U, NULL));
}

int main(void)
{
    test_sae_instantaneous();
    test_factory_priority();
    test_stationary_and_stale();
    test_trip_integration();
    test_factory_counters();
    test_invalid_inputs();
    if (failures != 0) {
        fprintf(stderr, "%d fuel-economy test(s) failed\n", failures);
        return 1;
    }
    return 0;
}
