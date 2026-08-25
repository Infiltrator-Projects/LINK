// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/i18n.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dirent.h>
#endif

#define LINK_LANGUAGE_PACK_MAX_PACKS 64U
#define LINK_LANGUAGE_PACK_MAX_FILES 128U
#define LINK_LANGUAGE_PACK_PATH_CAPACITY 1024U

typedef struct LinkLanguagePackEntry {
    char *key;
    char *value;
} LinkLanguagePackEntry;

typedef struct LinkLanguagePack {
    char locale[32];
    char native_name[128];
    bool rtl;
    LinkLanguagePackEntry *entries;
    size_t entry_count;
} LinkLanguagePack;

static LinkLanguagePack packs[LINK_LANGUAGE_PACK_MAX_PACKS];
static size_t pack_count;
static char selected_pack_locale[32];

static char *duplicate_text(const char *text)
{
    size_t length;
    char *copy;
    if (text == NULL) return NULL;
    length = strlen(text);
    copy = malloc(length + 1U);
    if (copy == NULL) return NULL;
    memcpy(copy, text, length + 1U);
    return copy;
}

static char *trim_text(char *text)
{
    char *end;
    if (text == NULL) return NULL;
    while (*text != '\0' && isspace((unsigned char)*text) != 0) ++text;
    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1]) != 0) --end;
    *end = '\0';
    return text;
}

static char *unquote_text(char *text)
{
    size_t length;
    text = trim_text(text);
    if (text == NULL) return NULL;
    length = strlen(text);
    if (length >= 2U && ((text[0] == '"' && text[length - 1U] == '"') ||
                        (text[0] == '\'' && text[length - 1U] == '\''))) {
        text[length - 1U] = '\0';
        ++text;
    }
    return text;
}

static void normalise_tag(const char *input, char *output, size_t capacity)
{
    size_t out = 0U;
    if (output == NULL || capacity == 0U) return;
    output[0] = '\0';
    if (input == NULL) return;
    while (*input != '\0' && *input != '.' && *input != '@' && out + 1U < capacity) {
        unsigned char c = (unsigned char)*input++;
        if (c == '_') c = '-';
        output[out++] = (char)tolower(c);
    }
    output[out] = '\0';
}

static bool tag_equal(const char *left, const char *right)
{
    char a[64];
    char b[64];
    normalise_tag(left, a, sizeof(a));
    normalise_tag(right, b, sizeof(b));
    return a[0] != '\0' && strcmp(a, b) == 0;
}

static void free_pack(LinkLanguagePack *pack)
{
    size_t index;
    if (pack == NULL) return;
    for (index = 0U; index < pack->entry_count; ++index) {
        free(pack->entries[index].key);
        free(pack->entries[index].value);
    }
    free(pack->entries);
    memset(pack, 0, sizeof(*pack));
}

static LinkLanguagePack *find_pack(const char *locale)
{
    size_t index;
    for (index = 0U; index < pack_count; ++index) {
        if (tag_equal(packs[index].locale, locale)) return &packs[index];
    }
    return NULL;
}

static const LinkLanguagePack *find_pack_const(const char *locale)
{
    return find_pack(locale);
}

static bool set_entry(LinkLanguagePack *pack, const char *key, const char *value)
{
    size_t index;
    LinkLanguagePackEntry *grown;
    char *key_copy;
    char *value_copy;
    if (pack == NULL || key == NULL || key[0] == '\0' || value == NULL) return false;
    for (index = 0U; index < pack->entry_count; ++index) {
        if (strcmp(pack->entries[index].key, key) == 0) {
            value_copy = duplicate_text(value);
            if (value_copy == NULL) return false;
            free(pack->entries[index].value);
            pack->entries[index].value = value_copy;
            return true;
        }
    }
    key_copy = duplicate_text(key);
    value_copy = duplicate_text(value);
    if (key_copy == NULL || value_copy == NULL) {
        free(key_copy);
        free(value_copy);
        return false;
    }
    grown = realloc(pack->entries, (pack->entry_count + 1U) * sizeof(*grown));
    if (grown == NULL) {
        free(key_copy);
        free(value_copy);
        return false;
    }
    pack->entries = grown;
    pack->entries[pack->entry_count].key = key_copy;
    pack->entries[pack->entry_count].value = value_copy;
    ++pack->entry_count;
    return true;
}

bool link_i18n_load_language_pack(const char *path)
{
    FILE *file;
    char line[4096];
    LinkLanguagePack candidate;
    bool valid = true;
    bool have_locale = false;
    bool have_name = false;
    bool have_direction = false;
    bool have_version = false;
    LinkLanguagePack *existing;

    if (path == NULL || path[0] == '\0') return false;
#ifdef _WIN32
    if (fopen_s(&file, path, "rb") != 0) file = NULL;
#else
    file = fopen(path, "rb");
#endif
    if (file == NULL) return false;
    memset(&candidate, 0, sizeof(candidate));

    while (fgets(line, (int)sizeof(line), file) != NULL) {
        char *cursor;
        char *equals;
        char *key;
        char *value;
        size_t length;
        if (strchr(line, '\n') == NULL && feof(file) == 0) {
            valid = false;
            break;
        }
        cursor = trim_text(line);
        length = strlen(cursor);
        while (length > 0U && (cursor[length - 1U] == '\n' || cursor[length - 1U] == '\r'))
            cursor[--length] = '\0';
        cursor = trim_text(cursor);
        if (cursor[0] == '\0' || cursor[0] == '#' || cursor[0] == ';') continue;
        equals = strchr(cursor, '=');
        if (equals == NULL) {
            valid = false;
            break;
        }
        *equals = '\0';
        key = trim_text(cursor);
        value = unquote_text(equals + 1);
        if (strcmp(key, "locale") == 0) {
            if (strlen(value) >= sizeof(candidate.locale)) { valid = false; break; }
            (void)snprintf(candidate.locale, sizeof(candidate.locale), "%s", value);
            have_locale = candidate.locale[0] != '\0';
        } else if (strcmp(key, "name") == 0) {
            if (strlen(value) >= sizeof(candidate.native_name)) { valid = false; break; }
            (void)snprintf(candidate.native_name, sizeof(candidate.native_name), "%s", value);
            have_name = candidate.native_name[0] != '\0';
        } else if (strcmp(key, "direction") == 0) {
            if (strcmp(value, "rtl") == 0) candidate.rtl = true;
            else if (strcmp(value, "ltr") == 0) candidate.rtl = false;
            else { valid = false; break; }
            have_direction = true;
        } else if (strcmp(key, "version") == 0) {
            char *end = NULL;
            unsigned long parsed = strtoul(value, &end, 10);
            if (end == value || end == NULL || *end != '\0' || parsed != 1UL) {
                valid = false;
                break;
            }
            have_version = true;
        } else if (!set_entry(&candidate, key, value)) {
            valid = false;
            break;
        }
    }
    if (ferror(file) != 0) valid = false;
    fclose(file);

    if (!valid || !have_locale || !have_name || !have_direction || !have_version ||
        candidate.entry_count == 0U) {
        free_pack(&candidate);
        return false;
    }

    existing = find_pack(candidate.locale);
    if (existing != NULL) {
        free_pack(existing);
        *existing = candidate;
        return true;
    }
    if (pack_count >= LINK_LANGUAGE_PACK_MAX_PACKS) {
        free_pack(&candidate);
        return false;
    }
    packs[pack_count++] = candidate;
    return true;
}

static int compare_paths(const void *left, const void *right)
{
    const char *const *a = left;
    const char *const *b = right;
    return strcmp(*a, *b);
}

size_t link_i18n_scan_language_directory(const char *directory)
{
    char *paths[LINK_LANGUAGE_PACK_MAX_FILES];
    size_t path_count = 0U;
    size_t loaded = 0U;
    size_t index;
    if (directory == NULL || directory[0] == '\0') return 0U;
#ifdef _WIN32
    {
        char pattern[LINK_LANGUAGE_PACK_PATH_CAPACITY];
        WIN32_FIND_DATAA data;
        HANDLE find;
        int written = snprintf(pattern, sizeof(pattern), "%s\\*.lang", directory);
        if (written <= 0 || (size_t)written >= sizeof(pattern)) return 0U;
        find = FindFirstFileA(pattern, &data);
        if (find == INVALID_HANDLE_VALUE) return 0U;
        do {
            if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0U &&
                path_count < LINK_LANGUAGE_PACK_MAX_FILES) {
                char full[LINK_LANGUAGE_PACK_PATH_CAPACITY];
                written = snprintf(full, sizeof(full), "%s\\%s", directory, data.cFileName);
                if (written > 0 && (size_t)written < sizeof(full)) {
                    char *copy = duplicate_text(full);
                    if (copy != NULL) paths[path_count++] = copy;
                }
            }
        } while (FindNextFileA(find, &data) != 0);
        FindClose(find);
    }
#else
    {
        DIR *dir = opendir(directory);
        struct dirent *entry;
        if (dir == NULL) return 0U;
        while ((entry = readdir(dir)) != NULL && path_count < LINK_LANGUAGE_PACK_MAX_FILES) {
            const char *name = entry->d_name;
            size_t length = strlen(name);
            if (length > 5U && strcmp(name + length - 5U, ".lang") == 0) {
                char full[LINK_LANGUAGE_PACK_PATH_CAPACITY];
                int written = snprintf(full, sizeof(full), "%s/%s", directory, name);
                if (written > 0 && (size_t)written < sizeof(full)) {
                    char *copy = duplicate_text(full);
                    if (copy != NULL) paths[path_count++] = copy;
                }
            }
        }
        closedir(dir);
    }
#endif
    qsort(paths, path_count, sizeof(paths[0]), compare_paths);
    for (index = 0U; index < path_count; ++index) {
        if (link_i18n_load_language_pack(paths[index])) ++loaded;
        free(paths[index]);
    }
    return loaded;
}

void link_i18n_clear_language_packs(void)
{
    size_t index;
    for (index = 0U; index < pack_count; ++index) free_pack(&packs[index]);
    pack_count = 0U;
    selected_pack_locale[0] = '\0';
}

static bool is_builtin_locale(const char *locale)
{
    size_t index;
    for (index = 0U; index < link_i18n_supported_locale_count(); ++index) {
        if (tag_equal(link_i18n_supported_locale(index), locale)) return true;
    }
    return false;
}

bool link_i18n_select_locale(const char *locale)
{
    LinkLanguagePack *pack;
    if (locale == NULL || locale[0] == '\0') return false;
    pack = find_pack(locale);
    if (pack != NULL) {
        memcpy(selected_pack_locale, pack->locale, sizeof(selected_pack_locale));
        selected_pack_locale[sizeof(selected_pack_locale) - 1U] = '\0';
        if (!link_i18n_set_locale(pack->locale)) (void)link_i18n_set_locale("en-AU");
        return true;
    }
    selected_pack_locale[0] = '\0';
    return link_i18n_set_locale(locale);
}

const char *link_i18n_selected_locale(void)
{
    return selected_pack_locale[0] != '\0' ? selected_pack_locale : link_i18n_locale();
}

const char *link_i18n_text(const char *key)
{
    const LinkLanguagePack *pack;
    size_t index;
    if (key == NULL) return "";
    pack = find_pack_const(link_i18n_selected_locale());
    if (pack != NULL) {
        for (index = 0U; index < pack->entry_count; ++index) {
            if (strcmp(pack->entries[index].key, key) == 0) return pack->entries[index].value;
        }
    }
    return link_i18n_tr(key);
}

size_t link_i18n_format_text(char *destination, size_t capacity, const char *key,
                             const InfiltratrI18nArgument *arguments,
                             size_t argument_count)
{
    return infiltratr_i18n_format(destination, capacity, link_i18n_text(key),
                                  arguments, argument_count);
}

size_t link_i18n_installed_locale_count(void)
{
    size_t count = link_i18n_supported_locale_count();
    size_t index;
    for (index = 0U; index < pack_count; ++index) {
        if (!is_builtin_locale(packs[index].locale)) ++count;
    }
    return count;
}

const char *link_i18n_installed_locale(size_t index)
{
    const size_t builtins = link_i18n_supported_locale_count();
    size_t pack_index;
    if (index < builtins) return link_i18n_supported_locale(index);
    index -= builtins;
    for (pack_index = 0U; pack_index < pack_count; ++pack_index) {
        if (is_builtin_locale(packs[pack_index].locale)) continue;
        if (index == 0U) return packs[pack_index].locale;
        --index;
    }
    return NULL;
}

const char *link_i18n_installed_locale_name(size_t index)
{
    const size_t builtins = link_i18n_supported_locale_count();
    if (index < builtins) {
        const char *locale = link_i18n_supported_locale(index);
        const LinkLanguagePack *pack = find_pack_const(locale);
        return pack != NULL ? pack->native_name : link_i18n_supported_locale_name(index);
    }
    {
        const char *locale = link_i18n_installed_locale(index);
        const LinkLanguagePack *pack = find_pack_const(locale);
        return pack != NULL ? pack->native_name : NULL;
    }
}

bool link_i18n_installed_locale_is_rtl(size_t index)
{
    const char *locale = link_i18n_installed_locale(index);
    const LinkLanguagePack *pack = find_pack_const(locale);
    if (pack != NULL) return pack->rtl;
    return locale != NULL && (locale[0] == 'a' || locale[0] == 'A') &&
           (locale[1] == 'r' || locale[1] == 'R') &&
           (locale[2] == '\0' || locale[2] == '-' || locale[2] == '_');
}

bool link_i18n_selected_locale_is_rtl(void)
{
    const char *locale = link_i18n_selected_locale();
    const LinkLanguagePack *pack = find_pack_const(locale);
    if (pack != NULL) return pack->rtl;
    return locale != NULL && (locale[0] == 'a' || locale[0] == 'A') &&
           (locale[1] == 'r' || locale[1] == 'R') &&
           (locale[2] == '\0' || locale[2] == '-' || locale[2] == '_');
}
