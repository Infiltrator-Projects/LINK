// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/i18n.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <stdlib.h>
#endif

bool link_i18n_set_system_locale(void)
{
#ifdef _WIN32
    wchar_t wide[LOCALE_NAME_MAX_LENGTH];
    char utf8[64];
    int count;
    if (GetUserDefaultLocaleName(wide, LOCALE_NAME_MAX_LENGTH) <= 0) return false;
    count = WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8,
                                (int)sizeof(utf8), NULL, NULL);
    if (count <= 0) return false;
    return link_i18n_select_locale(utf8);
#else
    const char *locale = getenv("LC_ALL");
    if (locale == NULL || locale[0] == '\0') locale = getenv("LC_MESSAGES");
    if (locale == NULL || locale[0] == '\0') locale = getenv("LANG");
    if (locale == NULL || locale[0] == '\0') return false;
    return link_i18n_select_locale(locale);
#endif
}
