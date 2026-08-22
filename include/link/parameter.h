// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file parameter.h
 * @brief Protocol-neutral diagnostic parameter identity and formatting.
 *
 * LINK deliberately separates a parameter's stable identity and presentation
 * metadata from the transport that produced a sample. Product faces may keep
 * references to definitions returned by this API for the lifetime of the
 * process; the built-in definition table is immutable static storage.
 */
#ifndef LINK_PARAMETER_H
#define LINK_PARAMETER_H

#include "link/obd2.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Reserved module identifier for standards-based OBD-II values. */
#define LINK_PARAMETER_MODULE_STANDARD_OBD2 0U

typedef enum LinkParameterProtocol {
    LINK_PARAMETER_PROTOCOL_OBD2 = 0,
    LINK_PARAMETER_PROTOCOL_UDS
} LinkParameterProtocol;

/**
 * Canonical identity of one diagnostic value.
 *
 * `module` is protocol-specific. Standard OBD-II values use
 * LINK_PARAMETER_MODULE_STANDARD_OBD2; manufacturer layers may allocate their
 * own endpoint identifiers without changing the generic store contract.
 */
typedef struct LinkParameterKey {
    LinkParameterProtocol protocol;
    uint32_t module;
    uint32_t identifier;
} LinkParameterKey;

/** Immutable metadata describing how a diagnostic scalar is presented. */
typedef struct LinkParameterDefinition {
    LinkParameterKey key;
    const char *stable_key;
    const char *short_name;
    const char *name;
    const char *suffix;
    unsigned int decimal_places;
    bool clamp;
    double minimum;
    double maximum;
} LinkParameterDefinition;

/**
 * Timestamped scalar observation.
 *
 * `definition` is borrowed and must outlive the sample. Built-in definitions
 * satisfy that requirement because they have static lifetime.
 */
typedef struct LinkParameterSample {
    const LinkParameterDefinition *definition;
    uint64_t timestamp_ms;
    bool available;
    double value;
} LinkParameterSample;

/**
 * Compatibility name for the canonical OBD-II unit enum.
 *
 * OBD-II owns the unit identifiers. Parameter import reuses that type instead
 * of redeclaring the same LINK_OBD2_UNIT_* enumerators in a second public
 * header, which keeps combined OBD-II/parameter consumers valid C11.
 */
typedef LinkObd2Unit LinkObd2UnitCode;

/** Returns a stable diagnostic name for `protocol`, or "unknown". */
const char *link_parameter_protocol_name(LinkParameterProtocol protocol);

/** Validates key range and protocol invariants. NULL is invalid. */
bool link_parameter_key_is_valid(const LinkParameterKey *key);

/** Compares two valid-or-invalid keys by value; NULL operands are unequal. */
bool link_parameter_key_equal(const LinkParameterKey *left,
                              const LinkParameterKey *right);

/** Validates definition pointers, identity and formatting constraints. */
bool link_parameter_definition_is_valid(
    const LinkParameterDefinition *definition);

/** Validates a sample and its borrowed definition. */
bool link_parameter_sample_is_valid(const LinkParameterSample *sample);

/**
 * Formats a scalar according to `definition`.
 *
 * Returns false for invalid arguments or insufficient output capacity. On
 * success `buffer` is always NUL-terminated. An unavailable value is rendered
 * using LINK's canonical unavailable representation rather than the numeric
 * value supplied by the caller.
 */
bool link_parameter_format_value(
    const LinkParameterDefinition *definition,
    bool available,
    double value,
    char *buffer,
    size_t buffer_size);

/** Convenience wrapper around link_parameter_format_value() for a sample. */
bool link_parameter_format_sample(const LinkParameterSample *sample,
                                  char *buffer,
                                  size_t buffer_size);

/** Number of immutable built-in standard OBD-II parameter definitions. */
size_t link_parameter_obd2_definition_count(void);

/** Returns the built-in definition at `index`, or NULL when out of range. */
const LinkParameterDefinition *link_parameter_obd2_definition_at(size_t index);

/** Returns the built-in definition for `pid`, or NULL when unsupported. */
const LinkParameterDefinition *link_parameter_obd2_definition(uint8_t pid);

/** Returns a built-in OBD-II definition by stable key, or NULL if unknown. */
const LinkParameterDefinition *link_parameter_obd2_definition_for_stable_key(
    const char *stable_key);

/** Reports the expected unit for a supported standard OBD-II PID. */
bool link_parameter_obd2_expected_unit(uint8_t pid, LinkObd2UnitCode *unit);

/**
 * Converts a decoded OBD-II scalar into a protocol-neutral LINK sample.
 *
 * The supplied unit must match LINK's definition for the PID. `parameter`
 * receives a borrowed pointer to immutable definition storage on success.
 */
bool link_parameter_from_obd2_scalar(
    uint8_t pid,
    LinkObd2UnitCode unit,
    double value,
    uint64_t timestamp_ms,
    LinkParameterSample *parameter);

#ifdef __cplusplus
}
#endif

#endif
