// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file i18n.h
 * @brief Process-wide localisation catalogue for LINK-family applications.
 *
 * LINK owns reusable automotive and shared-shell strings. Product repositories
 * retain only manufacturer-specific catalogues and presentation. The catalogue
 * engine itself is supplied by Infiltratr Common.
 */
#ifndef LINK_I18N_H
#define LINK_I18N_H

#include <stdbool.h>
#include <stddef.h>

#include "infiltratr/i18n.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Initialise the shared catalogue to en-AU. Safe to call repeatedly. */
void link_i18n_init(void);

/** Select a locale such as en-AU, en-US, de-DE, fr-FR, es-ES or it-IT. */
bool link_i18n_set_locale(const char *locale);

/** Return the currently requested normalised locale. */
const char *link_i18n_locale(void);

/** Translate one LINK-owned semantic key with English fallback. */
const char *link_i18n_tr(const char *key);

/** Translate and interpolate a LINK-owned semantic key. */
size_t link_i18n_format(char *destination, size_t capacity, const char *key,
                        const InfiltratrI18nArgument *arguments,
                        size_t argument_count);

/** Number of selectable built-in LINK locales. */
size_t link_i18n_supported_locale_count(void);

/** BCP-47 locale tag for one selectable built-in locale. */
const char *link_i18n_supported_locale(size_t index);

/** Native-language display label for one selectable built-in locale. */
const char *link_i18n_supported_locale_name(size_t index);

#ifdef __cplusplus
}
#endif

#endif
