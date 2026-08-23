// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/dtc_knowledge.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    const char *code;
    const char *category;
    const char *title;
} LinkDtcCatalogueEntry;

#include "dtc_catalogue.inc"

static bool valid_hex(char value)
{
    return (value >= '0' && value <= '9') || (value >= 'A' && value <= 'F');
}

static LinkDtcSystem system_from_code(char value)
{
    switch (value) {
    case 'P': return LINK_DTC_SYSTEM_POWERTRAIN;
    case 'C': return LINK_DTC_SYSTEM_CHASSIS;
    case 'B': return LINK_DTC_SYSTEM_BODY;
    case 'U': return LINK_DTC_SYSTEM_NETWORK;
    default: return LINK_DTC_SYSTEM_UNKNOWN;
    }
}

static LinkDtcOrigin origin_from_code(const char code[LINK_DTC_CODE_LENGTH])
{
    if (code[0] == 'P') {
        if (code[1] == '0' || code[1] == '2') return LINK_DTC_ORIGIN_STANDARD_GENERIC;
        if (code[1] == '1') return LINK_DTC_ORIGIN_MANUFACTURER_SPECIFIC;
        if (code[1] == '3') {
            return code[2] >= '4' ? LINK_DTC_ORIGIN_STANDARD_GENERIC
                                  : LINK_DTC_ORIGIN_MANUFACTURER_SPECIFIC;
        }
    } else {
        if (code[1] == '0' || code[1] == '3') return LINK_DTC_ORIGIN_STANDARD_GENERIC;
        if (code[1] == '1' || code[1] == '2') return LINK_DTC_ORIGIN_MANUFACTURER_SPECIFIC;
    }
    return LINK_DTC_ORIGIN_UNKNOWN;
}

static void copy_text(char *destination, size_t size, const char *source)
{
    if (destination == NULL || size == 0U) return;
    if (source == NULL) source = "";
    (void)snprintf(destination, size, "%s", source);
}

static const LinkDtcCatalogueEntry *catalogue_find(const char code[LINK_DTC_CODE_LENGTH])
{
    size_t low = 0U;
    size_t high = sizeof(link_dtc_catalogue) / sizeof(link_dtc_catalogue[0]);

    while (low < high) {
        size_t middle = low + (high - low) / 2U;
        int comparison = strcmp(code, link_dtc_catalogue[middle].code);
        if (comparison == 0) return &link_dtc_catalogue[middle];
        if (comparison < 0) high = middle;
        else low = middle + 1U;
    }
    return NULL;
}

bool link_dtc_resolve(const char *code, LinkDtcKnowledge *knowledge)
{
    LinkDtcKnowledge resolved = {0};
    const LinkDtcCatalogueEntry *entry;
    size_t index;

    if (code == NULL || knowledge == NULL || strlen(code) != 5U) return false;
    for (index = 0U; index < 5U; ++index) {
        resolved.code[index] = (char)toupper((unsigned char)code[index]);
    }
    resolved.code[5] = '\0';

    if (system_from_code(resolved.code[0]) == LINK_DTC_SYSTEM_UNKNOWN ||
        resolved.code[1] < '0' || resolved.code[1] > '3' ||
        !valid_hex(resolved.code[2]) || !valid_hex(resolved.code[3]) ||
        !valid_hex(resolved.code[4])) {
        return false;
    }

    resolved.system = system_from_code(resolved.code[0]);
    resolved.origin = origin_from_code(resolved.code);
    resolved.source = LINK_DTC_SOURCE_UNKNOWN;

    /* Never let the shared catalogue assign a generic meaning to a
       manufacturer-specific range. Product faces own those definitions. */
    if (resolved.origin != LINK_DTC_ORIGIN_STANDARD_GENERIC) {
        *knowledge = resolved;
        return true;
    }

    entry = catalogue_find(resolved.code);
    if (entry != NULL) {
        resolved.definition_known = true;
        resolved.source = LINK_DTC_SOURCE_STANDARD_GENERIC;
        copy_text(resolved.title, sizeof(resolved.title), entry->title);
        copy_text(resolved.category, sizeof(resolved.category), entry->category);
    }

    *knowledge = resolved;
    return true;
}

size_t link_dtc_catalogue_definition_count(void)
{
    return sizeof(link_dtc_catalogue) / sizeof(link_dtc_catalogue[0]);
}

const char *link_dtc_catalogue_snapshot(void)
{
    return LINK_DTC_CATALOGUE_SNAPSHOT;
}

const char *link_dtc_system_name(LinkDtcSystem system)
{
    switch (system) {
    case LINK_DTC_SYSTEM_POWERTRAIN: return "Powertrain";
    case LINK_DTC_SYSTEM_CHASSIS: return "Chassis";
    case LINK_DTC_SYSTEM_BODY: return "Body";
    case LINK_DTC_SYSTEM_NETWORK: return "Network";
    case LINK_DTC_SYSTEM_UNKNOWN: return "Unknown";
    }
    return "Unknown";
}

const char *link_dtc_origin_name(LinkDtcOrigin origin)
{
    switch (origin) {
    case LINK_DTC_ORIGIN_STANDARD_GENERIC: return "Standard generic";
    case LINK_DTC_ORIGIN_MANUFACTURER_SPECIFIC: return "Manufacturer-specific";
    case LINK_DTC_ORIGIN_UNKNOWN: return "Unknown";
    }
    return "Unknown";
}

const char *link_dtc_source_name(LinkDtcSource source)
{
    switch (source) {
    case LINK_DTC_SOURCE_STANDARD_GENERIC: return "OBDex CC0 generic definition";
    case LINK_DTC_SOURCE_UNKNOWN: return "Unmapped";
    }
    return "Unmapped";
}

bool link_dtc_format_uds_status(uint8_t status, char *buffer, size_t buffer_size)
{
    static const struct {
        uint8_t mask;
        const char *text;
    } bits[] = {
        {0x01U, "Test failed"},
        {0x02U, "Failed this operation cycle"},
        {0x04U, "Pending"},
        {0x08U, "Confirmed"},
        {0x10U, "Test not completed since last clear"},
        {0x20U, "Failed since last clear"},
        {0x40U, "Test not completed this operation cycle"},
        {0x80U, "Warning indicator requested"}
    };
    size_t index;
    size_t used = 0U;

    if (buffer == NULL || buffer_size == 0U) return false;
    buffer[0] = '\0';
    if (status == 0U) {
        return snprintf(buffer, buffer_size, "No status bits set") > 0 &&
               strlen(buffer) < buffer_size;
    }

    for (index = 0U; index < sizeof(bits) / sizeof(bits[0]); ++index) {
        int written;
        if ((status & bits[index].mask) == 0U) continue;
        written = snprintf(buffer + used, buffer_size - used, "%s%s",
                           used == 0U ? "" : " · ", bits[index].text);
        if (written < 0 || (size_t)written >= buffer_size - used) {
            buffer[0] = '\0';
            return false;
        }
        used += (size_t)written;
    }
    return true;
}
