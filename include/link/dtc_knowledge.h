// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file dtc_knowledge.h
 * @brief Presentation-neutral generic diagnostic trouble-code knowledge.
 *
 * This layer never replaces the raw ECU code. It classifies a five-character
 * SAE-style DTC, resolves LINK's complete pinned generic OBD-II catalogue, and
 * formats ISO 14229 DTC status bytes for every product face.
 *
 * The generic catalogue is generated from a pinned CC0 OBDex snapshot. SAE
 * J2012 itself is not vendored. Manufacturer-specific definitions stay in the
 * owning product repository (for example MBLINK or JAGLINK).
 */
#ifndef LINK_DTC_KNOWLEDGE_H
#define LINK_DTC_KNOWLEDGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LINK_DTC_CODE_LENGTH 6U
#define LINK_DTC_TITLE_LENGTH 256U
#define LINK_DTC_CATEGORY_LENGTH 48U
#define LINK_DTC_STATUS_TEXT_LENGTH 192U

typedef enum {
    LINK_DTC_SYSTEM_UNKNOWN = 0,
    LINK_DTC_SYSTEM_POWERTRAIN,
    LINK_DTC_SYSTEM_CHASSIS,
    LINK_DTC_SYSTEM_BODY,
    LINK_DTC_SYSTEM_NETWORK
} LinkDtcSystem;

typedef enum {
    LINK_DTC_ORIGIN_UNKNOWN = 0,
    LINK_DTC_ORIGIN_STANDARD_GENERIC,
    LINK_DTC_ORIGIN_MANUFACTURER_SPECIFIC
} LinkDtcOrigin;

typedef enum {
    LINK_DTC_SOURCE_UNKNOWN = 0,
    LINK_DTC_SOURCE_STANDARD_GENERIC
} LinkDtcSource;

typedef struct {
    char code[LINK_DTC_CODE_LENGTH];
    bool definition_known;
    LinkDtcSystem system;
    LinkDtcOrigin origin;
    LinkDtcSource source;
    char title[LINK_DTC_TITLE_LENGTH];
    char category[LINK_DTC_CATEGORY_LENGTH];
} LinkDtcKnowledge;

/**
 * Resolve one SAE-style five-character DTC.
 *
 * Returns false only when the input is syntactically invalid. For a valid
 * manufacturer-specific, reserved, or otherwise unmapped DTC, returns true
 * with definition_known=false while preserving the normalized raw code and
 * its system/origin classification.
 */
bool link_dtc_resolve(const char *code, LinkDtcKnowledge *knowledge);

/** Number of generic definitions compiled into the current LINK catalogue. */
size_t link_dtc_catalogue_definition_count(void);

/** Upstream OBDex commit used to generate the compiled catalogue. */
const char *link_dtc_catalogue_snapshot(void);

const char *link_dtc_system_name(LinkDtcSystem system);
const char *link_dtc_origin_name(LinkDtcOrigin origin);
const char *link_dtc_source_name(LinkDtcSource source);

/** Format an ISO 14229 DTC status byte into deterministic human-readable text. */
bool link_dtc_format_uds_status(uint8_t status, char *buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif
