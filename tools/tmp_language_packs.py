from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise SystemExit(f"missing patch marker: {label}")
    return text.replace(old, new, 1)


header = Path("include/link/i18n.h")
text = header.read_text()
if "link_i18n_load_language_pack" not in text:
    marker = "/** Number of selectable built-in LINK locales. */"
    block = r'''/** Select a built-in or discovered BCP-47 locale. */
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

'''
    text = replace_once(text, marker, block + marker, "i18n public API")
    text = text.replace(
        "Select a BCP-47 locale. The UI exposes 15 built-in choices; en-AU is canonical.",
        "Select a built-in BCP-47 locale. en-AU is canonical; use link_i18n_select_locale for discovered packs.",
    )
    header.write_text(text)


source = Path("src/core/language_pack.c")
source.write_text(r'''// SPDX-License-Identifier: GPL-3.0-or-later
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
    file = fopen(path, "rb");
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
        (void)snprintf(selected_pack_locale, sizeof(selected_pack_locale), "%s", pack->locale);
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
''')


cmake = Path("CMakeLists.txt")
text = cmake.read_text()
if "src/core/language_pack.c" not in text:
    text = replace_once(
        text,
        "    src/core/i18n_platform.c\n",
        "    src/core/i18n_platform.c\n    src/core/language_pack.c\n",
        "language-pack core source",
    )
    cmake.write_text(text)


platform = Path("src/core/i18n_platform.c")
text = platform.read_text()
text = text.replace("return link_i18n_set_locale(utf8);", "return link_i18n_select_locale(utf8);")
text = text.replace("return link_i18n_set_locale(locale);", "return link_i18n_select_locale(locale);")
platform.write_text(text)


gtk = Path("platform/linux/link-gtk-shell.c")
text = gtk.read_text()
text = text.replace("link_i18n_supported_locale_count()", "link_i18n_installed_locale_count()")
text = text.replace("link_i18n_supported_locale_name(", "link_i18n_installed_locale_name(")
text = text.replace("link_i18n_supported_locale(", "link_i18n_installed_locale(")
text = text.replace("link_i18n_locale()", "link_i18n_selected_locale()")
text = text.replace("link_i18n_set_locale(locale)", "link_i18n_select_locale(locale)")
text = text.replace(
    'gtk_label_set_text(GTK_LABEL(shell->language_label), "Language");',
    'gtk_label_set_text(GTK_LABEL(shell->language_label), "🌐");',
)
text = text.replace(
    'strncmp(locale, "ar", 2U) == 0 ? GTK_TEXT_DIR_RTL : GTK_TEXT_DIR_LTR',
    'link_i18n_selected_locale_is_rtl() ? GTK_TEXT_DIR_RTL : GTK_TEXT_DIR_LTR',
)
if "scan_language_pack_directories" not in text:
    marker = "static void save_selected_locale(const char *locale)\n"
    scan = r'''static void scan_language_pack_directories(const LinkGtkShell *shell)
{
    char *slug;
    char *system_path;
    char *user_path;
    char *exe_path;
    char *exe_dir;
    char *portable_path;
    if (shell == NULL || shell->descriptor == NULL) return;
    slug = g_ascii_strdown(shell->descriptor->brand_name != NULL ? shell->descriptor->brand_name : "link", -1);
    if (slug == NULL) return;
    system_path = g_build_filename("/usr/share", slug, "Languages", NULL);
    if (system_path != NULL) {
        (void)link_i18n_scan_language_directory(system_path);
        g_free(system_path);
    }
    exe_path = g_file_read_link("/proc/self/exe", NULL);
    if (exe_path != NULL) {
        exe_dir = g_path_get_dirname(exe_path);
        portable_path = g_build_filename(exe_dir, "Languages", NULL);
        if (portable_path != NULL) {
            (void)link_i18n_scan_language_directory(portable_path);
            g_free(portable_path);
        }
        g_free(exe_dir);
        g_free(exe_path);
    }
    user_path = g_build_filename(g_get_user_data_dir(), slug, "Languages", NULL);
    if (user_path != NULL) {
        (void)link_i18n_scan_language_directory(user_path);
        g_free(user_path);
    }
    g_free(slug);
}

'''
    text = replace_once(text, marker, scan + marker, "GTK language scan helper")
if "scan_language_pack_directories(shell);" not in text:
    text = replace_once(
        text,
        "    shell->window = GTK_WINDOW(window);\n",
        "    scan_language_pack_directories(shell);\n    initialise_selected_locale();\n    shell->window = GTK_WINDOW(window);\n",
        "GTK language scan call",
    )
if "GtkWidget *language_row;" not in text:
    text = replace_once(text, "    GtkStringList *language_model;\n", "    GtkStringList *language_model;\n    GtkWidget *language_row;\n", "GTK language row declaration")
text = text.replace(
    '    shell->language_label = left_label("Language", "link-language-label");\n    language_model = gtk_string_list_new(NULL);',
    '    shell->language_label = left_label("🌐", "link-language-label");\n    language_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);\n    language_model = gtk_string_list_new(NULL);',
)
text = text.replace(
    "    gtk_box_append(GTK_BOX(sidebar), shell->language_label);\n    gtk_box_append(GTK_BOX(sidebar), shell->language_combo);\n",
    "    gtk_box_append(GTK_BOX(language_row), shell->language_label);\n    gtk_box_append(GTK_BOX(language_row), shell->language_combo);\n    gtk_widget_set_hexpand(shell->language_combo, TRUE);\n    gtk_box_append(GTK_BOX(sidebar), language_row);\n",
)
gtk.write_text(text)


gtki18n = Path("platform/linux/link-gtk-i18n.c")
text = gtki18n.read_text()
if "packed_translation" not in text:
    needle = "    ensure_locale();\n    if (text == NULL) return NULL;\n"
    replacement = "    ensure_locale();\n    if (text == NULL) return NULL;\n    {\n        const char *packed_translation = link_i18n_text(text);\n        if (packed_translation != NULL && strcmp(packed_translation, text) != 0)\n            return packed_translation;\n    }\n"
    text = replace_once(text, needle, replacement, "GTK packed literal translation")
text = text.replace("return link_i18n_tr(key);", "return link_i18n_text(key);")
gtki18n.write_text(text)


win = Path("platform/windows/link-discover-i18n.c")
text = win.read_text()
text = text.replace("link_i18n_supported_locale_count()", "link_i18n_installed_locale_count()")
text = text.replace("link_i18n_supported_locale_name(", "link_i18n_installed_locale_name(")
text = text.replace("link_i18n_supported_locale(", "link_i18n_installed_locale(")
text = text.replace("link_i18n_set_locale(", "link_i18n_select_locale(")
text = text.replace("link_i18n_tr(", "link_i18n_text(")
text = text.replace("link_i18n_format(", "link_i18n_format_text(")
old_selected = '''static const char *selected_locale(void)
{
    const char *locale;
    ensure_locale();
    locale = link_i18n_locale();
    if (locale != NULL && strncmp(locale, "de", 2U) == 0) return "de-DE";
    if (locale != NULL && strncmp(locale, "pl", 2U) == 0) return "pl-PL";
    return "en-AU";
}'''
new_selected = '''static const char *selected_locale(void)
{
    ensure_locale();
    return link_i18n_selected_locale();
}'''
if old_selected in text:
    text = text.replace(old_selected, new_selected, 1)
if "scan_language_pack_directories(void)" not in text:
    marker = "static void load_saved_locale(void)\n"
    helper = r'''static void scan_language_pack_directories(void)
{
    char executable[MAX_PATH];
    char *slash;
    char path[MAX_PATH * 2];
    char local_app_data[MAX_PATH];
    DWORD length;
    int written;

    length = GetModuleFileNameA(NULL, executable, (DWORD)sizeof(executable));
    if (length > 0U && length < (DWORD)sizeof(executable)) {
        slash = strrchr(executable, '\\');
        if (slash != NULL) {
            *slash = '\0';
            written = snprintf(path, sizeof(path), "%s\\Languages", executable);
            if (written > 0 && (size_t)written < sizeof(path))
                (void)link_i18n_scan_language_directory(path);
        }
    }
    length = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    if (length > 0U && length < (DWORD)sizeof(local_app_data)) {
        written = snprintf(path, sizeof(path), "%s\\%s\\Languages", local_app_data, LINK_PRODUCT_NAME);
        if (written > 0 && (size_t)written < sizeof(path))
            (void)link_i18n_scan_language_directory(path);
    }
}

'''
    text = replace_once(text, marker, helper + marker, "Windows language scan helper")
if "scan_language_pack_directories();" not in text:
    text = replace_once(
        text,
        "    link_i18n_init();\n    (void)link_i18n_set_system_locale();\n",
        "    link_i18n_init();\n    scan_language_pack_directories();\n    (void)link_i18n_set_system_locale();\n",
        "Windows language scan call",
    )
win.write_text(text)


tests = Path("tests/test_i18n.c")
text = tests.read_text()
if "custom language pack load failed" not in text:
    needle = '    passed &= check(!link_i18n_set_locale("zz-ZZ"),\n'
    block = r'''    {
        const char *pack_path = "test-custom-language.lang";
        FILE *pack_file = fopen(pack_path, "wb");
        passed &= check(pack_file != NULL, "custom language pack file create failed");
        if (pack_file != NULL) {
            const char contents[] =
                "locale=sv-SE\n"
                "name=Svenska\n"
                "direction=ltr\n"
                "version=1\n"
                "nav.vehicle=Fordon\n"
                "Custom literal=Egen text\n";
            passed &= check(fwrite(contents, 1U, sizeof(contents) - 1U, pack_file) == sizeof(contents) - 1U,
                            "custom language pack write failed");
            fclose(pack_file);
            passed &= check(link_i18n_load_language_pack(pack_path),
                            "custom language pack load failed");
            passed &= check(link_i18n_installed_locale_count() == 16U,
                            "custom language pack did not extend installed registry");
            passed &= check(link_i18n_select_locale("sv-SE"),
                            "custom language selection failed");
            passed &= check(strcmp(link_i18n_selected_locale(), "sv-SE") == 0,
                            "custom language identity mismatch");
            passed &= check(strcmp(link_i18n_text("nav.vehicle"), "Fordon") == 0,
                            "custom language translation mismatch");
            passed &= check(strcmp(link_i18n_text("nav.faults"), "Faults") == 0,
                            "custom language en-AU fallback mismatch");
            passed &= check(strcmp(link_i18n_text("Custom literal"), "Egen text") == 0,
                            "custom literal translation mismatch");
            (void)remove(pack_path);
            link_i18n_clear_language_packs();
            passed &= check(link_i18n_installed_locale_count() == 15U,
                            "language pack clear did not restore built-in registry");
        }
    }

'''
    text = replace_once(text, needle, block + needle, "language-pack tests")
    tests.write_text(text)


Path("VERSION").write_text("0.13.5\n")
Path("locales/LANGUAGE_PACKS.md").write_text(r'''# LINK language packs

LINK-family applications support discoverable, Amiga-style, data-only language packs.
A translator can add a language without recompiling MBLINK, JAGLINK or LINK: place a
UTF-8 `.lang` file in a scanned `Languages` directory and restart the application.

## Format

```ini
locale=sv-SE
name=Svenska
direction=ltr
version=1
nav.vehicle=Fordon
nav.faults=Fel
Vehicle=Fordon
```

Required metadata is `locale`, `name`, `direction` (`ltr` or `rtl`) and `version=1`.
Every other `key=value` line is a translation. Keys can be semantic keys such as
`nav.vehicle` or exact human-readable English UI literals. Diagnostic data such as
VINs, DTCs, CAN identifiers, PIDs, numbers and measurements must not be translated.

Missing strings always fall back to compiled Australian English (`en-AU`). A pack for
an existing locale overrides the shipped catalogue. A pack for a new locale appears
in the language picker automatically.

## Search order

Later directories override earlier ones.

Linux applications scan:

1. `/usr/share/<product>/Languages`
2. `Languages` beside the executable
3. `~/.local/share/<product>/Languages`

Windows Discover applications scan:

1. `Languages` beside the executable
2. `%LOCALAPPDATA%\\<PRODUCT>\\Languages`

Apple applications use the same `.lang` format from their Application Support
`Languages` directory. Import is handled by the Apple UI because iOS does not allow
arbitrary writes into the signed application bundle.

Language packs are plain data and are never loaded as executable code.
''')
Path("locales/example.lang").write_text(
    "# Example LINK language pack\n"
    "locale=sv-SE\n"
    "name=Svenska\n"
    "direction=ltr\n"
    "version=1\n"
    "nav.vehicle=Fordon\n"
    "nav.faults=Fel\n"
    "nav.settings=Inställningar\n"
)
