// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/parameter.h"
#include "link/parameter_store.h"

#include "infiltratr/core.h"
#include "infiltratr/format.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#define OBD_KEY(pid_value) { LINK_PARAMETER_PROTOCOL_OBD2, LINK_PARAMETER_MODULE_STANDARD_OBD2, (pid_value) }
typedef struct { LinkParameterDefinition definition; LinkObd2UnitCode expected_unit; } LinkObd2ParameterEntry;

static const LinkObd2ParameterEntry link_obd2_parameters[] = {
    { { OBD_KEY(0x0cU), "obd2.engine.rpm", "RPM", "Engine speed", " rpm", 0U, false, 0.0, 0.0 }, LINK_OBD2_UNIT_RPM },
    { { OBD_KEY(0x0dU), "obd2.vehicle.speed", "SPEED", "Vehicle speed", " km/h", 0U, false, 0.0, 0.0 }, LINK_OBD2_UNIT_KMH },
    { { OBD_KEY(0x0bU), "obd2.engine.map", "MAP", "Manifold pressure", " kPa", 0U, false, 0.0, 0.0 }, LINK_OBD2_UNIT_KPA },
    { { OBD_KEY(0x11U), "obd2.engine.throttle", "THR VALVE", "Absolute throttle valve position", "%", 0U, true, 0.0, 100.0 }, LINK_OBD2_UNIT_PERCENT },
    { { OBD_KEY(0x04U), "obd2.engine.load", "LOAD", "Calculated engine load", "%", 0U, true, 0.0, 100.0 }, LINK_OBD2_UNIT_PERCENT },
    { { OBD_KEY(0x10U), "obd2.engine.maf", "MAF", "Mass air flow", " g/s", 1U, false, 0.0, 0.0 }, LINK_OBD2_UNIT_GRAMS_PER_SECOND },
    { { OBD_KEY(0x1fU), "obd2.engine.runtime", "RUN TIME", "Engine run time since start", " s", 0U, false, 0.0, 0.0 }, LINK_OBD2_UNIT_SECONDS },
    { { OBD_KEY(0x21U), "obd2.emissions.mil_distance", "MIL DIST", "Distance travelled with MIL on", " km", 0U, false, 0.0, 0.0 }, LINK_OBD2_UNIT_KILOMETRES },
    { { OBD_KEY(0x24U), "obd2.exhaust.o2_sensor1_lambda", "LAMBDA", "Oxygen sensor 1 equivalence ratio", " λ", 3U, false, 0.0, 0.0 }, LINK_OBD2_UNIT_RATIO },
    { { OBD_KEY(0x05U), "obd2.engine.coolant", "ECT", "Coolant temperature", " °C", 0U, false, 0.0, 0.0 }, LINK_OBD2_UNIT_CELSIUS },
    { { OBD_KEY(0x0fU), "obd2.engine.intake_air", "IAT", "Intake air temperature", " °C", 0U, false, 0.0, 0.0 }, LINK_OBD2_UNIT_CELSIUS },
    { { OBD_KEY(0x23U), "obd2.diesel.rail_pressure", "RAIL", "Fuel rail gauge pressure", " kPa", 0U, false, 0.0, 0.0 }, LINK_OBD2_UNIT_KPA },
    { { OBD_KEY(0x2fU), "obd2.fuel.tank_level", "FUEL LVL", "Fuel tank level input", "%", 1U, true, 0.0, 100.0 }, LINK_OBD2_UNIT_PERCENT },
    { { OBD_KEY(0x30U), "obd2.maintenance.warmups_since_clear", "WARMUPS", "Warm-ups since codes cleared", "", 0U, false, 0.0, 0.0 }, LINK_OBD2_UNIT_COUNT },
    { { OBD_KEY(0x31U), "obd2.maintenance.distance_since_clear", "CLEAR DIST", "Distance since codes cleared", " km", 0U, false, 0.0, 0.0 }, LINK_OBD2_UNIT_KILOMETRES },
    { { OBD_KEY(0x2cU), "obd2.diesel.egr_command", "EGR CMD", "Commanded EGR", "%", 1U, true, 0.0, 100.0 }, LINK_OBD2_UNIT_PERCENT },
    { { OBD_KEY(0x2dU), "obd2.diesel.egr_error", "EGR ERR", "EGR error", "%", 1U, false, 0.0, 0.0 }, LINK_OBD2_UNIT_PERCENT },
    { { OBD_KEY(0x33U), "obd2.engine.barometric_pressure", "BARO", "Barometric pressure", " kPa", 0U, false, 0.0, 0.0 }, LINK_OBD2_UNIT_KPA },
    { { OBD_KEY(0x3cU), "obd2.aftertreatment.catalyst_temp_b1s1", "CAT T1", "Catalyst temperature B1S1", " °C", 1U, false, 0.0, 0.0 }, LINK_OBD2_UNIT_CELSIUS },
    { { OBD_KEY(0x3eU), "obd2.aftertreatment.catalyst_temp_b1s2", "CAT T2", "Catalyst temperature B1S2", " °C", 1U, false, 0.0, 0.0 }, LINK_OBD2_UNIT_CELSIUS },
    { { OBD_KEY(0x42U), "obd2.electrical.control_module_voltage", "VOLT", "Control module voltage", " V", 2U, false, 0.0, 0.0 }, LINK_OBD2_UNIT_VOLTS },
    { { OBD_KEY(0x45U), "obd2.engine.relative_throttle", "THR REL", "Relative throttle position", "%", 1U, true, 0.0, 100.0 }, LINK_OBD2_UNIT_PERCENT },
    { { OBD_KEY(0x46U), "obd2.environment.ambient_air", "AMBIENT", "Ambient air temperature", " °C", 1U, false, 0.0, 0.0 }, LINK_OBD2_UNIT_CELSIUS },
    { { OBD_KEY(0x47U), "obd2.engine.throttle_b", "THR B", "Absolute throttle position B", "%", 1U, true, 0.0, 100.0 }, LINK_OBD2_UNIT_PERCENT },
    { { OBD_KEY(0x48U), "obd2.engine.throttle_c", "THR C", "Absolute throttle position C", "%", 1U, true, 0.0, 100.0 }, LINK_OBD2_UNIT_PERCENT },
    { { OBD_KEY(0x49U), "obd2.driver.accelerator_pedal_d", "PEDAL D", "Accelerator pedal position D", "%", 1U, true, 0.0, 100.0 }, LINK_OBD2_UNIT_PERCENT },
    { { OBD_KEY(0x4aU), "obd2.driver.accelerator_pedal_e", "PEDAL E", "Accelerator pedal position E", "%", 1U, true, 0.0, 100.0 }, LINK_OBD2_UNIT_PERCENT },
    { { OBD_KEY(0x4bU), "obd2.driver.accelerator_pedal_f", "PEDAL F", "Accelerator pedal position F", "%", 1U, true, 0.0, 100.0 }, LINK_OBD2_UNIT_PERCENT },
    { { OBD_KEY(0x4cU), "obd2.engine.commanded_throttle_actuator", "THR CMD", "Commanded throttle actuator", "%", 1U, true, 0.0, 100.0 }, LINK_OBD2_UNIT_PERCENT },
    { { OBD_KEY(0x4dU), "obd2.emissions.mil_runtime", "MIL TIME", "Time run with MIL on", " min", 0U, false, 0.0, 0.0 }, LINK_OBD2_UNIT_MINUTES },
    { { OBD_KEY(0x5cU), "obd2.engine.oil_temperature", "OIL T", "Engine oil temperature", " °C", 0U, false, 0.0, 0.0 }, LINK_OBD2_UNIT_CELSIUS },
    { { OBD_KEY(0x5eU), "obd2.engine.fuel_rate", "FUEL", "Engine fuel rate", " L/h", 2U, false, 0.0, 0.0 }, LINK_OBD2_UNIT_LITRES_PER_HOUR },
    { { OBD_KEY(0x78U), "obd2.aftertreatment.egt_b1s1", "EGT1", "Exhaust gas temperature B1S1", " °C", 1U, false, 0.0, 0.0 }, LINK_OBD2_UNIT_CELSIUS },
    { { OBD_KEY(0x7aU), "obd2.dpf.bank1_delta_pressure", "DPF ΔP", "DPF bank 1 differential pressure", " kPa", 2U, false, 0.0, 0.0 }, LINK_OBD2_UNIT_KPA },
    { { OBD_KEY(0x7cU), "obd2.dpf.bank1_inlet_temperature", "DPF IN", "DPF bank 1 inlet temperature", " °C", 1U, false, 0.0, 0.0 }, LINK_OBD2_UNIT_CELSIUS }
};

const char *link_parameter_protocol_name(LinkParameterProtocol protocol)
{
    switch (protocol) { case LINK_PARAMETER_PROTOCOL_OBD2: return "obd2"; case LINK_PARAMETER_PROTOCOL_UDS: return "uds"; }
    return "unknown";
}

bool link_parameter_key_is_valid(const LinkParameterKey *key)
{
    if (key == NULL) return false;
    switch (key->protocol) {
    case LINK_PARAMETER_PROTOCOL_OBD2: return key->module == LINK_PARAMETER_MODULE_STANDARD_OBD2 && key->identifier <= UINT8_MAX;
    case LINK_PARAMETER_PROTOCOL_UDS: return key->identifier <= UINT16_MAX;
    }
    return false;
}

bool link_parameter_key_equal(const LinkParameterKey *left, const LinkParameterKey *right)
{
    return left != NULL && right != NULL && left->protocol == right->protocol && left->module == right->module && left->identifier == right->identifier;
}

bool link_parameter_definition_is_valid(const LinkParameterDefinition *definition)
{
    if (definition == NULL || !link_parameter_key_is_valid(&definition->key) || definition->stable_key == NULL || definition->stable_key[0] == '\0' || definition->short_name == NULL || definition->short_name[0] == '\0' || definition->name == NULL || definition->name[0] == '\0' || definition->suffix == NULL || definition->decimal_places > 9U) return false;
    return !definition->clamp || (isfinite(definition->minimum) && isfinite(definition->maximum) && definition->minimum <= definition->maximum);
}

bool link_parameter_sample_is_valid(const LinkParameterSample *sample)
{
    return sample != NULL && link_parameter_definition_is_valid(sample->definition) && (!sample->available || isfinite(sample->value));
}

bool link_parameter_format_value(const LinkParameterDefinition *definition, bool available, double value, char *buffer, size_t buffer_size)
{
    InfiltratrScalarFormatOptions options = INFILTRATR_SCALAR_FORMAT_OPTIONS_INIT;
    long double display_value = (long double)value;
    if (!link_parameter_definition_is_valid(definition) || buffer == NULL || buffer_size == 0U) return false;
    options.decimal_places = definition->decimal_places;
    options.clamp = definition->clamp;
    options.minimum = (long double)definition->minimum;
    options.maximum = (long double)definition->maximum;
    options.suffix = definition->suffix;

    /*
     * SAE OBD-II rail pressure remains canonical in kPa in the decoded sample,
     * telemetry history and CSV evidence. Human presentation auto-scales large
     * rail values to MPa so common-rail diesel pressure is immediately legible.
     */
    if (available &&
        definition->key.protocol == LINK_PARAMETER_PROTOCOL_OBD2 &&
        definition->key.module == LINK_PARAMETER_MODULE_STANDARD_OBD2 &&
        definition->key.identifier == UINT32_C(0x23) &&
        value >= 1000.0) {
        display_value /= 1000.0L;
        options.decimal_places = 1U;
        options.suffix = " MPa";
    }
    return infiltratr_format_scalar(available, display_value, &options, buffer, buffer_size);
}

bool link_parameter_format_sample(const LinkParameterSample *sample, char *buffer, size_t buffer_size)
{
    return sample != NULL && link_parameter_definition_is_valid(sample->definition) && link_parameter_format_value(sample->definition, sample->available, sample->value, buffer, buffer_size);
}

size_t link_parameter_obd2_definition_count(void) { return INFILTRATR_ARRAY_LENGTH(link_obd2_parameters); }
const LinkParameterDefinition *link_parameter_obd2_definition_at(size_t index) { return index < link_parameter_obd2_definition_count() ? &link_obd2_parameters[index].definition : NULL; }

static const LinkObd2ParameterEntry *link_parameter_obd2_entry(uint8_t pid)
{
    size_t index;
    for (index = 0U; index < link_parameter_obd2_definition_count(); ++index) if (link_obd2_parameters[index].definition.key.identifier == (uint32_t)pid) return &link_obd2_parameters[index];
    return NULL;
}

const LinkParameterDefinition *link_parameter_obd2_definition(uint8_t pid)
{
    const LinkObd2ParameterEntry *entry = link_parameter_obd2_entry(pid);
    return entry != NULL ? &entry->definition : NULL;
}

const LinkParameterDefinition *link_parameter_obd2_definition_for_stable_key(const char *stable_key)
{
    size_t index;
    if (stable_key == NULL || stable_key[0] == '\0') return NULL;
    for (index = 0U; index < link_parameter_obd2_definition_count(); ++index) if (infiltratr_string_equal(link_obd2_parameters[index].definition.stable_key, stable_key)) return &link_obd2_parameters[index].definition;
    return NULL;
}

bool link_parameter_obd2_expected_unit(uint8_t pid, LinkObd2UnitCode *unit)
{
    const LinkObd2ParameterEntry *entry = link_parameter_obd2_entry(pid);
    if (entry == NULL || unit == NULL) return false;
    *unit = entry->expected_unit;
    return true;
}

bool link_parameter_from_obd2_scalar(uint8_t pid, LinkObd2UnitCode unit, double value, uint64_t timestamp_ms, LinkParameterSample *parameter)
{
    const LinkObd2ParameterEntry *entry = link_parameter_obd2_entry(pid);
    if (entry == NULL || parameter == NULL || entry->expected_unit != unit || !isfinite(value)) return false;
    parameter->definition = &entry->definition;
    parameter->timestamp_ms = timestamp_ms;
    parameter->available = true;
    parameter->value = value;
    return true;
}

static size_t store_find_key(const LinkParameterStore *store, const LinkParameterKey *key)
{
    size_t index;
    if (store == NULL || !link_parameter_key_is_valid(key)) return SIZE_MAX;
    for (index = 0U; index < store->slot_count; ++index) if (store->slots[index].definition != NULL && link_parameter_key_equal(&store->slots[index].definition->key, key)) return index;
    return SIZE_MAX;
}

static size_t store_find_stable_key(const LinkParameterStore *store, const char *stable_key)
{
    size_t index;
    if (store == NULL || stable_key == NULL || stable_key[0] == '\0') return SIZE_MAX;
    for (index = 0U; index < store->slot_count; ++index) if (store->slots[index].definition != NULL && infiltratr_string_equal(store->slots[index].definition->stable_key, stable_key)) return index;
    return SIZE_MAX;
}

const char *link_parameter_store_result_name(LinkParameterStoreResult result)
{
    switch (result) {
    case LINK_PARAMETER_STORE_OK: return "ok";
    case LINK_PARAMETER_STORE_INVALID_ARGUMENT: return "invalid-argument";
    case LINK_PARAMETER_STORE_FULL: return "full";
    case LINK_PARAMETER_STORE_DUPLICATE_KEY: return "duplicate-key";
    case LINK_PARAMETER_STORE_DUPLICATE_STABLE_KEY: return "duplicate-stable-key";
    case LINK_PARAMETER_STORE_NOT_FOUND: return "not-found";
    case LINK_PARAMETER_STORE_DEFINITION_MISMATCH: return "definition-mismatch";
    }
    return "unknown";
}

void link_parameter_store_init(LinkParameterStore *store) { if (store != NULL) memset(store, 0, sizeof(*store)); }
void link_parameter_store_clear_samples(LinkParameterStore *store)
{
    size_t index;
    if (store == NULL) return;
    for (index = 0U; index < store->slot_count; ++index) { memset(&store->slots[index].latest, 0, sizeof(store->slots[index].latest)); store->slots[index].latest_valid = false; }
    memset(store->history, 0, sizeof(store->history));
    store->history_head = 0U; store->history_count = 0U; store->total_sample_count = 0U;
}

LinkParameterStoreResult link_parameter_store_register(LinkParameterStore *store, const LinkParameterDefinition *definition)
{
    if (store == NULL || !link_parameter_definition_is_valid(definition)) return LINK_PARAMETER_STORE_INVALID_ARGUMENT;
    if (store_find_key(store, &definition->key) != SIZE_MAX) return LINK_PARAMETER_STORE_DUPLICATE_KEY;
    if (store_find_stable_key(store, definition->stable_key) != SIZE_MAX) return LINK_PARAMETER_STORE_DUPLICATE_STABLE_KEY;
    if (store->slot_count >= LINK_PARAMETER_STORE_DEFINITION_CAPACITY) return LINK_PARAMETER_STORE_FULL;
    memset(&store->slots[store->slot_count], 0, sizeof(store->slots[store->slot_count]));
    store->slots[store->slot_count].definition = definition;
    store->slot_count++;
    return LINK_PARAMETER_STORE_OK;
}

size_t link_parameter_store_definition_count(const LinkParameterStore *store) { return store != NULL ? store->slot_count : 0U; }
const LinkParameterDefinition *link_parameter_store_definition_at(const LinkParameterStore *store, size_t index) { return store != NULL && index < store->slot_count ? store->slots[index].definition : NULL; }
const LinkParameterDefinition *link_parameter_store_definition(const LinkParameterStore *store, const LinkParameterKey *key) { const size_t index = store_find_key(store, key); return index != SIZE_MAX ? store->slots[index].definition : NULL; }
const LinkParameterDefinition *link_parameter_store_definition_for_stable_key(const LinkParameterStore *store, const char *stable_key) { const size_t index = store_find_stable_key(store, stable_key); return index != SIZE_MAX ? store->slots[index].definition : NULL; }

LinkParameterStoreResult link_parameter_store_set_favourite(LinkParameterStore *store, const LinkParameterKey *key, bool favourite)
{
    const size_t index = store_find_key(store, key);
    if (store == NULL || !link_parameter_key_is_valid(key)) return LINK_PARAMETER_STORE_INVALID_ARGUMENT;
    if (index == SIZE_MAX) return LINK_PARAMETER_STORE_NOT_FOUND;
    store->slots[index].favourite = favourite;
    return LINK_PARAMETER_STORE_OK;
}

bool link_parameter_store_is_favourite(const LinkParameterStore *store, const LinkParameterKey *key) { const size_t index = store_find_key(store, key); return index != SIZE_MAX && store->slots[index].favourite; }

LinkParameterStoreResult link_parameter_store_record(LinkParameterStore *store, const LinkParameterSample *sample)
{
    size_t slot_index, history_index;
    if (store == NULL || !link_parameter_sample_is_valid(sample)) return LINK_PARAMETER_STORE_INVALID_ARGUMENT;
    slot_index = store_find_key(store, &sample->definition->key);
    if (slot_index == SIZE_MAX) return LINK_PARAMETER_STORE_NOT_FOUND;
    if (store->slots[slot_index].definition != sample->definition) return LINK_PARAMETER_STORE_DEFINITION_MISMATCH;
    store->slots[slot_index].latest = *sample; store->slots[slot_index].latest_valid = true;
    if (store->history_count < LINK_PARAMETER_STORE_HISTORY_CAPACITY) { history_index = (store->history_head + store->history_count) % LINK_PARAMETER_STORE_HISTORY_CAPACITY; store->history_count++; }
    else { history_index = store->history_head; store->history_head = (store->history_head + 1U) % LINK_PARAMETER_STORE_HISTORY_CAPACITY; }
    store->history[history_index] = *sample;
    store->total_sample_count = infiltratr_u64_add_saturating(store->total_sample_count, 1U);
    return LINK_PARAMETER_STORE_OK;
}

bool link_parameter_store_latest(const LinkParameterStore *store, const LinkParameterKey *key, LinkParameterSample *sample)
{
    const size_t index = store_find_key(store, key);
    if (store == NULL || sample == NULL || !link_parameter_key_is_valid(key) || index == SIZE_MAX || !store->slots[index].latest_valid) return false;
    *sample = store->slots[index].latest; return true;
}

size_t link_parameter_store_history_count(const LinkParameterStore *store) { return store != NULL ? store->history_count : 0U; }
uint64_t link_parameter_store_total_sample_count(const LinkParameterStore *store) { return store != NULL ? store->total_sample_count : 0U; }
bool link_parameter_store_history_at(const LinkParameterStore *store, size_t chronological_index, LinkParameterSample *sample)
{
    size_t physical_index;
    if (store == NULL || sample == NULL || chronological_index >= store->history_count) return false;
    physical_index = (store->history_head + chronological_index) % LINK_PARAMETER_STORE_HISTORY_CAPACITY;
    *sample = store->history[physical_index]; return true;
}
