// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/obd2.h"

#include <ctype.h>
#include <math.h>
#include <string.h>

typedef enum LinkObd2FormulaKind {
    LINK_OBD2_FORMULA_NONE = 0,
    LINK_OBD2_FORMULA_PERCENT_A,
    LINK_OBD2_FORMULA_TEMP_A,
    LINK_OBD2_FORMULA_A,
    LINK_OBD2_FORMULA_U16_DIV4,
    LINK_OBD2_FORMULA_U16_DIV100,
    LINK_OBD2_FORMULA_TRIM_A,
    LINK_OBD2_FORMULA_A_X3,
    LINK_OBD2_FORMULA_TIMING_ADVANCE,
    LINK_OBD2_FORMULA_O2_VOLTAGE_TRIM,
    LINK_OBD2_FORMULA_U16,
    LINK_OBD2_FORMULA_U16_X_079,
    LINK_OBD2_FORMULA_U16_X10,
    LINK_OBD2_FORMULA_LAMBDA_VOLTAGE,
    LINK_OBD2_FORMULA_EVAP_SIGNED_QUARTER,
    LINK_OBD2_FORMULA_LAMBDA_CURRENT,
    LINK_OBD2_FORMULA_U16_DIV10_MINUS40,
    LINK_OBD2_FORMULA_U16_DIV1000,
    LINK_OBD2_FORMULA_U16_PERCENT,
    LINK_OBD2_FORMULA_U16_LAMBDA,
    LINK_OBD2_FORMULA_A_X10,
    LINK_OBD2_FORMULA_U16_DIV200,
    LINK_OBD2_FORMULA_U16_MINUS32767,
    LINK_OBD2_FORMULA_TWO_TRIMS_13,
    LINK_OBD2_FORMULA_TWO_TRIMS_24,
    LINK_OBD2_FORMULA_INJECTION_TIMING,
    LINK_OBD2_FORMULA_U16_DIV20,
    LINK_OBD2_FORMULA_A_MINUS125,
    LINK_OBD2_FORMULA_TORQUE_FIVE,
    LINK_OBD2_FORMULA_MAF_TWO,
    LINK_OBD2_FORMULA_TEMP_TWO,
    LINK_OBD2_FORMULA_EGT_FOUR,
    LINK_OBD2_FORMULA_FUEL_RATE_TWO,
    LINK_OBD2_FORMULA_U16_DIV5,
    LINK_OBD2_FORMULA_U16_DIV32,
    LINK_OBD2_FORMULA_ODOMETER
} LinkObd2FormulaKind;

typedef struct LinkObd2CatalogueEntry {
    LinkObd2PidDefinition definition;
    LinkObd2FormulaKind formula_kind;
} LinkObd2CatalogueEntry;

#include "pid_catalogue.inc"
#include "pid_standard_supplement.inc"

static const LinkObd2ServiceDefinition link_obd2_services[] = {
    { UINT8_C(0x01), "Current diagnostic data", true, true },
    { UINT8_C(0x02), "Freeze-frame diagnostic data", true, true },
    { UINT8_C(0x03), "Stored emissions DTCs", true, false },
    { UINT8_C(0x04), "Clear emissions diagnostic information", false, false },
    { UINT8_C(0x05), "Oxygen-sensor monitor results", true, true },
    { UINT8_C(0x06), "Non-continuous monitor results", true, true },
    { UINT8_C(0x07), "Pending emissions DTCs", true, false },
    { UINT8_C(0x08), "Control on-board test/component", false, true },
    { UINT8_C(0x09), "Vehicle information", true, true },
    { UINT8_C(0x0A), "Permanent emissions DTCs", true, false }
};

static size_t obd2_base_count(void)
{
    return sizeof(link_obd2_catalogue) / sizeof(link_obd2_catalogue[0]);
}

static size_t obd2_supplement_count(void)
{
    return sizeof(link_obd2_standard_supplement) /
           sizeof(link_obd2_standard_supplement[0]);
}

static uint16_t obd2_u16(const uint8_t *data)
{
    return (uint16_t)(((uint16_t)data[0] << 8U) | (uint16_t)data[1]);
}

static uint32_t obd2_u32(const uint8_t *data)
{
    return ((uint32_t)data[0] << 24U) |
           ((uint32_t)data[1] << 16U) |
           ((uint32_t)data[2] << 8U) |
           (uint32_t)data[3];
}

static bool obd2_add_signal(
    LinkObd2DecodedPid *decoded,
    const char *label,
    double value,
    const char *unit)
{
    LinkObd2DecodedSignal *signal;
    if (decoded == NULL || !isfinite(value) ||
        decoded->signal_count >= LINK_OBD2_MAX_DECODED_SIGNALS) {
        return false;
    }
    signal = &decoded->signals[decoded->signal_count++];
    signal->label = label != NULL ? label : "value";
    signal->value = value;
    signal->unit = unit != NULL ? unit : "";
    return true;
}

static bool obd2_sensor_supported(uint8_t flags, unsigned int index)
{
    if (index >= 8U) return false;
    return (flags & (uint8_t)(UINT8_C(1) << index)) != 0U;
}

size_t link_obd2_service_definition_count(void)
{
    return sizeof(link_obd2_services) / sizeof(link_obd2_services[0]);
}

const LinkObd2ServiceDefinition *link_obd2_service_definition_at(size_t index)
{
    return index < link_obd2_service_definition_count()
        ? &link_obd2_services[index] : NULL;
}

const LinkObd2ServiceDefinition *link_obd2_service_definition(uint8_t mode)
{
    size_t index;
    for (index = 0U; index < link_obd2_service_definition_count(); ++index) {
        if (link_obd2_services[index].mode == mode) {
            return &link_obd2_services[index];
        }
    }
    return NULL;
}

size_t link_obd2_pid_definition_count(void)
{
    return obd2_base_count() + obd2_supplement_count();
}

const LinkObd2PidDefinition *link_obd2_pid_definition_at(size_t index)
{
    const size_t base_count = obd2_base_count();
    if (index < base_count) return &link_obd2_catalogue[index].definition;
    index -= base_count;
    return index < obd2_supplement_count()
        ? &link_obd2_standard_supplement[index].definition : NULL;
}

const LinkObd2PidDefinition *link_obd2_pid_definition(uint8_t mode, uint8_t pid)
{
    size_t index;
    for (index = 0U; index < link_obd2_pid_definition_count(); ++index) {
        const LinkObd2PidDefinition *definition =
            link_obd2_pid_definition_at(index);
        if (definition != NULL && definition->mode == mode &&
            definition->pid == pid) {
            return definition;
        }
    }
    return NULL;
}

const char *link_obd2_pid_catalogue_snapshot(void)
{
    return LINK_OBD2_CATALOGUE_SNAPSHOT "+link-standard-supplement-v1";
}

static const LinkObd2CatalogueEntry *obd2_catalogue_entry(
    uint8_t mode,
    uint8_t pid)
{
    size_t index;
    for (index = 0U; index < obd2_base_count(); ++index) {
        if (link_obd2_catalogue[index].definition.mode == mode &&
            link_obd2_catalogue[index].definition.pid == pid) {
            return &link_obd2_catalogue[index];
        }
    }
    for (index = 0U; index < obd2_supplement_count(); ++index) {
        if (link_obd2_standard_supplement[index].definition.mode == mode &&
            link_obd2_standard_supplement[index].definition.pid == pid) {
            return &link_obd2_standard_supplement[index];
        }
    }
    return NULL;
}

static void obd2_decode_ascii(
    const uint8_t *data,
    size_t length,
    LinkObd2DecodedPid *decoded)
{
    size_t source_index;
    size_t target_index = 0U;
    if (data == NULL || decoded == NULL) return;
    for (source_index = 0U;
         source_index < length &&
         target_index + 1U < sizeof(decoded->text);
         ++source_index) {
        const unsigned char ch = data[source_index];
        if (ch == 0U) continue;
        decoded->text[target_index++] =
            isprint((int)ch) ? (char)ch : '.';
    }
    while (target_index != 0U &&
           decoded->text[target_index - 1U] == ' ') {
        --target_index;
    }
    decoded->text[target_index] = '\0';
    decoded->text_available = target_index != 0U;
}

static LinkObd2Result obd2_decode_formula(
    const LinkObd2CatalogueEntry *entry,
    const uint8_t *data,
    LinkObd2DecodedPid *decoded)
{
    const LinkObd2PidDefinition *definition = &entry->definition;
    const double a = definition->bytes > 0U ? (double)data[0] : 0.0;
    const double b = definition->bytes > 1U ? (double)data[1] : 0.0;
    const double c = definition->bytes > 2U ? (double)data[2] : 0.0;
    const double d = definition->bytes > 3U ? (double)data[3] : 0.0;
    const double e = definition->bytes > 4U ? (double)data[4] : 0.0;
    const double f = definition->bytes > 5U ? (double)data[5] : 0.0;
    const double g = definition->bytes > 6U ? (double)data[6] : 0.0;
    const double h = definition->bytes > 7U ? (double)data[7] : 0.0;
    const double i = definition->bytes > 8U ? (double)data[8] : 0.0;
    const char *unit = definition->unit;

    switch (entry->formula_kind) {
    case LINK_OBD2_FORMULA_NONE:
        return LINK_OBD2_RESULT_OK;
    case LINK_OBD2_FORMULA_PERCENT_A:
        (void)obd2_add_signal(decoded, "value", a * 100.0 / 255.0, unit);
        break;
    case LINK_OBD2_FORMULA_TEMP_A:
        (void)obd2_add_signal(decoded, "value", a - 40.0, unit);
        break;
    case LINK_OBD2_FORMULA_A:
        (void)obd2_add_signal(decoded, "value", a, unit);
        break;
    case LINK_OBD2_FORMULA_U16_DIV4:
        (void)obd2_add_signal(decoded, "value", (double)obd2_u16(data) / 4.0, unit);
        break;
    case LINK_OBD2_FORMULA_U16_DIV100:
        (void)obd2_add_signal(decoded, "value", (double)obd2_u16(data) / 100.0, unit);
        break;
    case LINK_OBD2_FORMULA_TRIM_A:
        (void)obd2_add_signal(decoded, "value", a * 100.0 / 128.0 - 100.0, unit);
        break;
    case LINK_OBD2_FORMULA_A_X3:
        (void)obd2_add_signal(decoded, "value", a * 3.0, unit);
        break;
    case LINK_OBD2_FORMULA_TIMING_ADVANCE:
        (void)obd2_add_signal(decoded, "value", a / 2.0 - 64.0, unit);
        break;
    case LINK_OBD2_FORMULA_O2_VOLTAGE_TRIM:
        (void)obd2_add_signal(decoded, "voltage", a / 200.0, "V");
        (void)obd2_add_signal(decoded, "trim", b * 100.0 / 128.0 - 100.0, "%");
        break;
    case LINK_OBD2_FORMULA_U16:
        (void)obd2_add_signal(decoded, "value", (double)obd2_u16(data), unit);
        break;
    case LINK_OBD2_FORMULA_U16_X_079:
        (void)obd2_add_signal(decoded, "value", (double)obd2_u16(data) * 0.079, unit);
        break;
    case LINK_OBD2_FORMULA_U16_X10:
        (void)obd2_add_signal(decoded, "value", (double)obd2_u16(data) * 10.0, unit);
        break;
    case LINK_OBD2_FORMULA_LAMBDA_VOLTAGE:
        (void)obd2_add_signal(decoded, "lambda", ((a * 256.0) + b) * 2.0 / 65536.0, "ratio");
        (void)obd2_add_signal(decoded, "voltage", ((c * 256.0) + d) * 8.0 / 65536.0, "V");
        break;
    case LINK_OBD2_FORMULA_EVAP_SIGNED_QUARTER:
        (void)obd2_add_signal(decoded, "value", (double)obd2_u16(data) / 4.0 - 8192.0, unit);
        break;
    case LINK_OBD2_FORMULA_LAMBDA_CURRENT:
        (void)obd2_add_signal(decoded, "lambda", ((a * 256.0) + b) * 2.0 / 65536.0, "ratio");
        (void)obd2_add_signal(decoded, "current", ((c * 256.0) + d) / 256.0 - 128.0, "mA");
        break;
    case LINK_OBD2_FORMULA_U16_DIV10_MINUS40:
        (void)obd2_add_signal(decoded, "value", (double)obd2_u16(data) / 10.0 - 40.0, unit);
        break;
    case LINK_OBD2_FORMULA_U16_DIV1000:
        (void)obd2_add_signal(decoded, "value", (double)obd2_u16(data) / 1000.0, unit);
        break;
    case LINK_OBD2_FORMULA_U16_PERCENT:
        (void)obd2_add_signal(decoded, "value", (double)obd2_u16(data) * 100.0 / 255.0, unit);
        break;
    case LINK_OBD2_FORMULA_U16_LAMBDA:
        (void)obd2_add_signal(decoded, "value", (double)obd2_u16(data) * 2.0 / 65536.0, unit);
        break;
    case LINK_OBD2_FORMULA_A_X10:
        (void)obd2_add_signal(decoded, "value", a * 10.0, unit);
        break;
    case LINK_OBD2_FORMULA_U16_DIV200:
        (void)obd2_add_signal(decoded, "value", (double)obd2_u16(data) / 200.0, unit);
        break;
    case LINK_OBD2_FORMULA_U16_MINUS32767:
        (void)obd2_add_signal(decoded, "value", (double)obd2_u16(data) - 32767.0, unit);
        break;
    case LINK_OBD2_FORMULA_TWO_TRIMS_13:
        (void)obd2_add_signal(decoded, "bank 1", a * 100.0 / 128.0 - 100.0, "%");
        (void)obd2_add_signal(decoded, "bank 3", b * 100.0 / 128.0 - 100.0, "%");
        break;
    case LINK_OBD2_FORMULA_TWO_TRIMS_24:
        (void)obd2_add_signal(decoded, "bank 2", a * 100.0 / 128.0 - 100.0, "%");
        (void)obd2_add_signal(decoded, "bank 4", b * 100.0 / 128.0 - 100.0, "%");
        break;
    case LINK_OBD2_FORMULA_INJECTION_TIMING:
        (void)obd2_add_signal(decoded, "value", ((double)obd2_u16(data) - 26880.0) / 128.0, unit);
        break;
    case LINK_OBD2_FORMULA_U16_DIV20:
        (void)obd2_add_signal(decoded, "value", (double)obd2_u16(data) / 20.0, unit);
        break;
    case LINK_OBD2_FORMULA_A_MINUS125:
        (void)obd2_add_signal(decoded, "value", a - 125.0, unit);
        break;
    case LINK_OBD2_FORMULA_TORQUE_FIVE:
        (void)obd2_add_signal(decoded, "idle", a - 125.0, "%");
        (void)obd2_add_signal(decoded, "point 2", b - 125.0, "%");
        (void)obd2_add_signal(decoded, "point 3", c - 125.0, "%");
        (void)obd2_add_signal(decoded, "point 4", d - 125.0, "%");
        (void)obd2_add_signal(decoded, "point 5", e - 125.0, "%");
        break;
    case LINK_OBD2_FORMULA_MAF_TWO:
        if (obd2_sensor_supported(data[0], 0U))
            (void)obd2_add_signal(decoded, "sensor A", ((b * 256.0) + c) / 32.0, "g/s");
        if (obd2_sensor_supported(data[0], 1U))
            (void)obd2_add_signal(decoded, "sensor B", ((d * 256.0) + e) / 32.0, "g/s");
        break;
    case LINK_OBD2_FORMULA_TEMP_TWO:
        if (obd2_sensor_supported(data[0], 0U))
            (void)obd2_add_signal(decoded, "sensor 1", b - 40.0, "°C");
        if (obd2_sensor_supported(data[0], 1U))
            (void)obd2_add_signal(decoded, "sensor 2", c - 40.0, "°C");
        break;
    case LINK_OBD2_FORMULA_EGT_FOUR: {
        const double values[4] = {
            ((b * 256.0) + c) / 10.0 - 40.0,
            ((d * 256.0) + e) / 10.0 - 40.0,
            ((f * 256.0) + g) / 10.0 - 40.0,
            ((h * 256.0) + i) / 10.0 - 40.0
        };
        static const char *labels[4] = {"sensor 1", "sensor 2", "sensor 3", "sensor 4"};
        unsigned int index;
        for (index = 0U; index < 4U; ++index) {
            if (obd2_sensor_supported(data[0], index))
                (void)obd2_add_signal(decoded, labels[index], values[index], "°C");
        }
        break;
    }
    case LINK_OBD2_FORMULA_FUEL_RATE_TWO:
        (void)obd2_add_signal(decoded, "engine fuel rate", ((a * 256.0) + b) / 50.0, "g/s");
        (void)obd2_add_signal(decoded, "vehicle fuel rate", ((c * 256.0) + d) / 50.0, "g/s");
        break;
    case LINK_OBD2_FORMULA_U16_DIV5:
        (void)obd2_add_signal(decoded, "value", (double)obd2_u16(data) / 5.0, unit);
        break;
    case LINK_OBD2_FORMULA_U16_DIV32:
        (void)obd2_add_signal(decoded, "value", (double)obd2_u16(data) / 32.0, unit);
        break;
    case LINK_OBD2_FORMULA_ODOMETER:
        (void)obd2_add_signal(decoded, "value", (double)obd2_u32(data) / 10.0, unit);
        break;
    }
    return LINK_OBD2_RESULT_OK;
}

LinkObd2Result link_obd2_decode_pid_payload(
    uint8_t mode,
    uint8_t pid,
    const uint8_t *data,
    size_t data_length,
    LinkObd2DecodedPid *decoded)
{
    const LinkObd2CatalogueEntry *entry;
    size_t copy_length;

    if (data == NULL || decoded == NULL)
        return LINK_OBD2_RESULT_INVALID_ARGUMENT;

    entry = obd2_catalogue_entry(mode, pid);
    if (entry == NULL)
        return LINK_OBD2_RESULT_UNSUPPORTED_PID;
    if (data_length < entry->definition.bytes)
        return LINK_OBD2_RESULT_MALFORMED_RESPONSE;

    memset(decoded, 0, sizeof(*decoded));
    decoded->definition = &entry->definition;
    copy_length = data_length;
    if (copy_length > sizeof(decoded->raw))
        copy_length = sizeof(decoded->raw);
    memcpy(decoded->raw, data, copy_length);
    decoded->raw_length = copy_length;

    if (entry->definition.value_kind == LINK_OBD2_VALUE_ASCII) {
        obd2_decode_ascii(data, entry->definition.bytes, decoded);
    } else if (entry->definition.value_kind == LINK_OBD2_VALUE_DTC &&
               entry->definition.bytes >= 2U) {
        if (link_obd2_decode_dtc_pair(
                data[0], data[1], decoded->text) == LINK_OBD2_RESULT_OK) {
            decoded->text_available = true;
        }
    }

    return obd2_decode_formula(entry, data, decoded);
}
