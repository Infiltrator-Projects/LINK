// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file mercedes_me_data_ids.h
 * @brief Archived Mercedes me Adapter diagnostic/live-data identity catalogue.
 *
 * These identifiers are independently reimplemented interoperability facts
 * recovered from the official Mercedes me Adapter 4.7.61 application. They
 * identify model keys/literals only; they do not imply a CAN/UDS DID, byte
 * layout, scaling, availability on every vehicle, or permission to transmit.
 */
#ifndef LINK_MERCEDES_ME_DATA_IDS_H
#define LINK_MERCEDES_ME_DATA_IDS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LinkMercedesMeDataIdDefinition {
    const char *symbol;
    const char *data_id;
} LinkMercedesMeDataIdDefinition;

/** Number of preserved official-app constants in the current evidence set. */
size_t link_mercedes_me_data_id_count(void);

/** Stable catalogue access; returns NULL for an out-of-range index. */
const LinkMercedesMeDataIdDefinition *link_mercedes_me_data_id_at(
    size_t index);

/** Exact, case-sensitive lookup by the official constant/symbol name. */
const LinkMercedesMeDataIdDefinition *link_mercedes_me_data_id_find_symbol(
    const char *symbol);

/**
 * Number of constants carrying an exact application literal.
 *
 * This is a count rather than a boolean because unit literals such as "km"
 * legitimately occur under more than one official constant name.
 */
size_t link_mercedes_me_data_id_literal_match_count(const char *data_id);

/**
 * Return the Nth exact application-literal match, or NULL if unavailable.
 * match_index is zero-based within the matching subset.
 */
const LinkMercedesMeDataIdDefinition *link_mercedes_me_data_id_literal_match(
    const char *data_id,
    size_t match_index);

#ifdef __cplusplus
}
#endif
#endif
