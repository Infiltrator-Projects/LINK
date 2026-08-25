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

/** Select a built-in BCP-47 locale. en-AU is canonical; use link_i18n_select_locale for discovered packs. */
bool link_i18n_set_locale(const char *locale);

/**
 * Select the operating-system/user locale where the platform exposes one.
 * Windows uses the user locale; POSIX-family builds consult LC_ALL,
 * LC_MESSAGES and LANG in that order. Apple UI code may subsequently override
 * this with the first preferred-language tag supplied by Foundation.
 */
bool link_i18n_set_system_locale(void);

/** Return the currently requested normalised locale. */
const char *link_i18n_locale(void);

/** Translate one LINK-owned semantic key with English fallback. */
const char *link_i18n_tr(const char *key);

/** Translate and interpolate a LINK-owned semantic key. */
size_t link_i18n_format(char *destination, size_t capacity, const char *key,
                        const InfiltratrI18nArgument *arguments,
                        size_t argument_count);

/** Select a built-in or discovered BCP-47 locale. */
bool link_i18n_select_locale(const char *locale);

/** Current selected locale, including an external pack. */
const char *link_i18n_selected_locale(void);

/** Translate using an external pack first, then the built-in catalogue. */
const char *link_i18n_text(const char *key);

/** Translate and interpolate using external-pack override semantics. */
size_t link_i18n_format_text(char *destination, size_t capacity, const char *key,
                             const InfiltratrI18nArgument *arguments,
                             size_t argument_count);

/** Load one UTF-8 data-only .lang file. */
bool link_i18n_load_language_pack(const char *path);

/** Scan one directory for *.lang files. Returns the number loaded. */
size_t link_i18n_scan_language_directory(const char *directory);

/** Release all externally loaded packs. Compiled en-AU remains available. */
void link_i18n_clear_language_packs(void);

/** Number of languages visible after combining built-ins and discovered packs. */
size_t link_i18n_installed_locale_count(void);

/** BCP-47 locale tag for one installed language. */
const char *link_i18n_installed_locale(size_t index);

/** Native-language label for one installed language. */
const char *link_i18n_installed_locale_name(size_t index);

/** Whether one installed language requests right-to-left layout. */
bool link_i18n_installed_locale_is_rtl(size_t index);

/** Whether the currently selected language requests right-to-left layout. */
bool link_i18n_selected_locale_is_rtl(void);

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
