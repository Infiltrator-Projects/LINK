// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LINK_PARAMETER_H
#define LINK_PARAMETER_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define LINK_PARAMETER_MODULE_STANDARD_OBD2 0U
typedef enum { LINK_PARAMETER_PROTOCOL_OBD2 = 0, LINK_PARAMETER_PROTOCOL_UDS } LinkParameterProtocol;
typedef struct { LinkParameterProtocol protocol; uint32_t module; uint32_t identifier; } LinkParameterKey;
typedef struct { LinkParameterKey key; const char *stable_key; const char *short_name; const char *name; const char *suffix; unsigned int decimal_places; bool clamp; double minimum; double maximum; } LinkParameterDefinition;
typedef struct { const LinkParameterDefinition *definition; uint64_t timestamp_ms; bool available; double value; } LinkParameterSample;
typedef enum { LINK_OBD2_UNIT_NONE = 0, LINK_OBD2_UNIT_PERCENT, LINK_OBD2_UNIT_CELSIUS, LINK_OBD2_UNIT_KPA, LINK_OBD2_UNIT_RPM, LINK_OBD2_UNIT_KMH, LINK_OBD2_UNIT_GRAMS_PER_SECOND, LINK_OBD2_UNIT_VOLTS, LINK_OBD2_UNIT_LITRES_PER_HOUR } LinkObd2UnitCode;
const char *link_parameter_protocol_name(LinkParameterProtocol protocol);
bool link_parameter_key_is_valid(const LinkParameterKey *key);
bool link_parameter_key_equal(const LinkParameterKey *left, const LinkParameterKey *right);
bool link_parameter_definition_is_valid(const LinkParameterDefinition *definition);
bool link_parameter_sample_is_valid(const LinkParameterSample *sample);
bool link_parameter_format_value(const LinkParameterDefinition *definition, bool available, double value, char *buffer, size_t buffer_size);
bool link_parameter_format_sample(const LinkParameterSample *sample, char *buffer, size_t buffer_size);
size_t link_parameter_obd2_definition_count(void);
const LinkParameterDefinition *link_parameter_obd2_definition_at(size_t index);
const LinkParameterDefinition *link_parameter_obd2_definition(uint8_t pid);
const LinkParameterDefinition *link_parameter_obd2_definition_for_stable_key(const char *stable_key);
bool link_parameter_obd2_expected_unit(uint8_t pid, LinkObd2UnitCode *unit);
bool link_parameter_from_obd2_scalar(uint8_t pid, LinkObd2UnitCode unit, double value, uint64_t timestamp_ms, LinkParameterSample *parameter);
#ifdef __cplusplus
}
#endif
#endif
