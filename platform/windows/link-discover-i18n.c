// SPDX-License-Identifier: GPL-3.0-or-later
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "link/i18n.h"

#include <stdio.h>
#include <string.h>

#ifndef LINK_PRODUCT_NAME
#define LINK_PRODUCT_NAME "LINK"
#endif
#ifndef LINK_PRODUCT_SLUG
#define LINK_PRODUCT_SLUG "link"
#endif
#ifndef LINK_PRODUCT_WINDOW_CLASS
#define LINK_PRODUCT_WINDOW_CLASS "LINKDiscoverWindow"
#endif
#ifndef LINK_PRODUCT_VERSION
#define LINK_PRODUCT_VERSION "unknown"
#endif
#ifndef LINK_PRODUCT_COPYRIGHT
#define LINK_PRODUCT_COPYRIGHT "Copyright (C) 2026 The First Infiltrator"
#endif
#ifndef LINK_PRODUCT_SUBTITLE
#define LINK_PRODUCT_SUBTITLE "Vehicle Diagnostics"
#endif
#ifndef LINK_PRODUCT_WEBSITE
#define LINK_PRODUCT_WEBSITE "https://github.com/The-First-Infiltrator/LINK"
#endif

#define IDM_FILE_EXPORT 41001
#define IDM_FILE_EXIT 41002
#define IDM_HELP_ABOUT 42001
#define IDM_LANGUAGE_ENGLISH 43001
#define IDM_LANGUAGE_GERMAN 43002
#define IDM_LANGUAGE_POLISH 43003

static int link_win_i18n_initialised;
static WNDPROC original_main_window_proc;
static HWND main_window;

static const char *const semantic_keys[] = {
    "discover.vehicle_interface",
    "discover.j2534_dll",
    "discover.browse",
    "discover.connect_passive",
    "discover.inventory",
    "discover.stop",
    "discover.export_evidence",
    "discover.status_disconnected",
    "discover.session_log",
    "discover.annotation",
    "discover.add_annotation",
    "discover.no_dll",
    "discover.evidence_failed",
    "discover.connect_first",
    "discover.no_evidence",
    "discover.file",
    "discover.exit",
    "discover.help",
    "language.label"
};

static int utf8_to_wide_i18n(const char *source, wchar_t *destination,
                             size_t destination_count)
{
    if (destination == NULL || destination_count == 0U) return 0;
    destination[0] = L'\0';
    if (source == NULL) return 1;
    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, source, -1,
                               destination, (int)destination_count) != 0;
}

static int wide_to_utf8_i18n(const wchar_t *source, char *destination,
                             size_t destination_count)
{
    if (destination == NULL || destination_count == 0U) return 0;
    destination[0] = '\0';
    if (source == NULL) return 1;
    return WideCharToMultiByte(CP_UTF8, 0, source, -1, destination,
                               (int)destination_count, NULL, NULL) != 0;
}

static BOOL set_window_text_utf8_i18n(HWND window, const char *text)
{
    wchar_t wide[1024];
    if (window == NULL ||
        !utf8_to_wide_i18n(text != NULL ? text : "", wide,
                           sizeof(wide) / sizeof(wide[0]))) return FALSE;
    return SetWindowTextW(window, wide);
}

static BOOL append_menu_utf8(HMENU menu, UINT flags, UINT_PTR item,
                             const char *text)
{
    wchar_t wide[512];
    if (text == NULL) return AppendMenuW(menu, flags, item, NULL);
    if (!utf8_to_wide_i18n(text, wide, sizeof(wide) / sizeof(wide[0])))
        return FALSE;
    return AppendMenuW(menu, flags, item, wide);
}

static void registry_path(char *path, size_t capacity)
{
    if (path == NULL || capacity == 0U) return;
    (void)snprintf(path, capacity,
                   "Software\\The First Infiltrator\\%s", LINK_PRODUCT_SLUG);
    path[capacity - 1U] = '\0';
}

static void load_saved_locale(void)
{
    HKEY key;
    char path[256];
    char value[32];
    DWORD size = (DWORD)sizeof(value);
    DWORD type = 0U;
    registry_path(path, sizeof(path));
    if (RegOpenKeyExA(HKEY_CURRENT_USER, path, 0U, KEY_READ, &key) != ERROR_SUCCESS)
        return;
    if (RegQueryValueExA(key, "Language", NULL, &type,
                         (LPBYTE)value, &size) == ERROR_SUCCESS &&
        (type == REG_SZ || type == REG_EXPAND_SZ)) {
        value[sizeof(value) - 1U] = '\0';
        if (strcmp(value, "en-AU") == 0 ||
            strcmp(value, "de-DE") == 0 ||
            strcmp(value, "pl-PL") == 0)
            (void)link_i18n_set_locale(value);
    }
    RegCloseKey(key);
}

static void save_locale(const char *locale)
{
    HKEY key;
    DWORD disposition;
    char path[256];
    if (locale == NULL) return;
    registry_path(path, sizeof(path));
    if (RegCreateKeyExA(HKEY_CURRENT_USER, path, 0U, NULL, 0U,
                        KEY_WRITE, NULL, &key, &disposition) != ERROR_SUCCESS)
        return;
    (void)disposition;
    (void)RegSetValueExA(key, "Language", 0U, REG_SZ,
                         (const BYTE *)locale,
                         (DWORD)(strlen(locale) + 1U));
    RegCloseKey(key);
}

void link_win_i18n_init(void)
{
    if (link_win_i18n_initialised) return;
    link_i18n_init();
    (void)link_i18n_set_system_locale();
    load_saved_locale();
    link_win_i18n_initialised = 1;
}

static void ensure_locale(void)
{
    link_win_i18n_init();
}

static const char *selected_locale(void)
{
    const char *locale;
    ensure_locale();
    locale = link_i18n_locale();
    if (locale != NULL && strncmp(locale, "de", 2U) == 0) return "de-DE";
    if (locale != NULL && strncmp(locale, "pl", 2U) == 0) return "pl-PL";
    return "en-AU";
}

static UINT selected_language_command(void)
{
    const char *locale = selected_locale();
    if (strcmp(locale, "de-DE") == 0) return IDM_LANGUAGE_GERMAN;
    if (strcmp(locale, "pl-PL") == 0) return IDM_LANGUAGE_POLISH;
    return IDM_LANGUAGE_ENGLISH;
}

static const char *semantic_key_from_any_language(const char *text)
{
    static char matched_key[64];
    static const char *const locales[] = {"en-AU", "de-DE", "pl-PL"};
    char original[32];
    size_t locale_index;
    size_t key_index;
    const char *current;

    if (text == NULL || text[0] == '\0') return NULL;
    current = selected_locale();
    (void)snprintf(original, sizeof(original), "%s", current);
    matched_key[0] = '\0';

    for (locale_index = 0U;
         locale_index < sizeof(locales) / sizeof(locales[0]) && matched_key[0] == '\0';
         ++locale_index) {
        (void)link_i18n_set_locale(locales[locale_index]);
        for (key_index = 0U;
             key_index < sizeof(semantic_keys) / sizeof(semantic_keys[0]);
             ++key_index) {
            if (strcmp(text, link_i18n_tr(semantic_keys[key_index])) == 0) {
                (void)snprintf(matched_key, sizeof(matched_key), "%s",
                               semantic_keys[key_index]);
                break;
            }
        }
    }
    (void)link_i18n_set_locale(original);
    return matched_key[0] != '\0' ? matched_key : NULL;
}

static const char *product_subtitle(void)
{
    const char *locale = selected_locale();
    if (strcmp(LINK_PRODUCT_SUBTITLE, "Mercedes-Benz Diagnostics") == 0) {
        if (strcmp(locale, "de-DE") == 0) return "Mercedes-Benz-Diagnose";
        if (strcmp(locale, "pl-PL") == 0) return "Diagnostyka Mercedes-Benz";
    }
    if (strcmp(LINK_PRODUCT_SUBTITLE, "Jaguar X400 Diagnostics") == 0) {
        if (strcmp(locale, "de-DE") == 0) return "Jaguar-X400-Diagnose";
        if (strcmp(locale, "pl-PL") == 0) return "Diagnostyka Jaguar X400";
    }
    if (strcmp(locale, "de-DE") == 0 &&
        strcmp(LINK_PRODUCT_SUBTITLE, "Vehicle Diagnostics") == 0)
        return "Fahrzeugdiagnose";
    if (strcmp(locale, "pl-PL") == 0 &&
        strcmp(LINK_PRODUCT_SUBTITLE, "Vehicle Diagnostics") == 0)
        return "Diagnostyka pojazdu";
    return LINK_PRODUCT_SUBTITLE;
}

static const char *discover_word(void)
{
    const char *locale = selected_locale();
    if (strcmp(locale, "de-DE") == 0) return "Erkennung";
    if (strcmp(locale, "pl-PL") == 0) return "Wykrywanie";
    return "Discover";
}

static const char *version_word(void)
{
    return strcmp(selected_locale(), "pl-PL") == 0 ? "Wersja" : "Version";
}

const char *link_win_i18n_translate_text(const char *text)
{
    const char *key;
    static char buffer[512];
    static const char about_prefix[] = "&About ";
    size_t length;

    ensure_locale();
    if (text == NULL) return NULL;

    key = semantic_key_from_any_language(text);
    if (key != NULL) return link_i18n_tr(key);

    if (strcmp(text, "&File") == 0) {
        (void)snprintf(buffer, sizeof(buffer), "&%s", link_i18n_tr("discover.file"));
        return buffer;
    }
    if (strcmp(text, "&Help") == 0) {
        (void)snprintf(buffer, sizeof(buffer), "&%s", link_i18n_tr("discover.help"));
        return buffer;
    }
    if (strcmp(text, "E&xit") == 0) return link_i18n_tr("discover.exit");
    if (strcmp(text, "&Export evidence...\tCtrl+E") == 0) {
        (void)snprintf(buffer, sizeof(buffer), "%s\tCtrl+E",
                       link_i18n_tr("discover.export_evidence"));
        return buffer;
    }

    length = strlen(text);
    if (length > sizeof(about_prefix) &&
        strncmp(text, about_prefix, sizeof(about_prefix) - 1U) == 0 &&
        length >= 3U && strcmp(text + length - 3U, "...") == 0) {
        char product[128];
        size_t product_length = length - (sizeof(about_prefix) - 1U) - 3U;
        InfiltratrI18nArgument argument;
        if (product_length >= sizeof(product)) product_length = sizeof(product) - 1U;
        memcpy(product, text + sizeof(about_prefix) - 1U, product_length);
        product[product_length] = '\0';
        argument.name = "product";
        argument.value = product;
        (void)link_i18n_format(buffer, sizeof(buffer), "discover.about",
                               &argument, 1U);
        return buffer;
    }
    return text;
}

static void set_menu_text_by_command(HMENU menu, UINT command,
                                     const char *text)
{
    MENUITEMINFOW info;
    wchar_t wide[512];
    if (menu == NULL || text == NULL ||
        !utf8_to_wide_i18n(text, wide, sizeof(wide) / sizeof(wide[0]))) return;
    memset(&info, 0, sizeof(info));
    info.cbSize = sizeof(info);
    info.fMask = MIIM_STRING;
    info.dwTypeData = wide;
    (void)SetMenuItemInfoW(menu, command, FALSE, &info);
}

static void set_menu_text_by_position(HMENU menu, UINT position,
                                      const char *text)
{
    MENUITEMINFOW info;
    wchar_t wide[512];
    if (menu == NULL || text == NULL ||
        !utf8_to_wide_i18n(text, wide, sizeof(wide) / sizeof(wide[0]))) return;
    memset(&info, 0, sizeof(info));
    info.cbSize = sizeof(info);
    info.fMask = MIIM_STRING;
    info.dwTypeData = wide;
    (void)SetMenuItemInfoW(menu, position, TRUE, &info);
}

static void retranslate_menu(HWND window)
{
    HMENU root = GetMenu(window);
    HMENU file_menu;
    HMENU language_menu;
    HMENU help_menu;
    char text[256];
    InfiltratrI18nArgument argument;
    UINT selected;

    if (root == NULL || GetMenuItemCount(root) < 3) return;
    file_menu = GetSubMenu(root, 0);
    language_menu = GetSubMenu(root, 1);
    help_menu = GetSubMenu(root, 2);

    set_menu_text_by_position(root, 0U, link_i18n_tr("discover.file"));
    set_menu_text_by_position(root, 1U, link_i18n_tr("language.label"));
    set_menu_text_by_position(root, 2U, link_i18n_tr("discover.help"));

    (void)snprintf(text, sizeof(text), "%s\tCtrl+E",
                   link_i18n_tr("discover.export_evidence"));
    set_menu_text_by_command(file_menu, IDM_FILE_EXPORT, text);
    set_menu_text_by_command(file_menu, IDM_FILE_EXIT, link_i18n_tr("discover.exit"));

    argument.name = "product";
    argument.value = LINK_PRODUCT_NAME;
    (void)link_i18n_format(text, sizeof(text), "discover.about", &argument, 1U);
    set_menu_text_by_command(help_menu, IDM_HELP_ABOUT, text);

    selected = selected_language_command();
    if (language_menu != NULL)
        (void)CheckMenuRadioItem(language_menu,
                                 IDM_LANGUAGE_ENGLISH, IDM_LANGUAGE_POLISH,
                                 selected, MF_BYCOMMAND);
    DrawMenuBar(window);
}

static BOOL CALLBACK retranslate_child(HWND child, LPARAM parameter)
{
    wchar_t wide[1024];
    char utf8[2048];
    const char *translated;
    (void)parameter;

    if (GetWindowTextW(child, wide, (int)(sizeof(wide) / sizeof(wide[0]))) <= 0)
        return TRUE;
    if (!wide_to_utf8_i18n(wide, utf8, sizeof(utf8))) return TRUE;

    translated = link_win_i18n_translate_text(utf8);
    if (translated != NULL && strcmp(translated, utf8) != 0)
        (void)set_window_text_utf8_i18n(child, translated);

    /* The two branded header strings are intentionally reconstructed rather
       than treated as diagnostic data. */
    if (strstr(utf8, LINK_PRODUCT_NAME) == utf8 &&
        (strstr(utf8, "Discover") != NULL ||
         strstr(utf8, "Erkennung") != NULL ||
         strstr(utf8, "Wykrywanie") != NULL)) {
        char title[256];
        (void)snprintf(title, sizeof(title), "%s %s",
                       LINK_PRODUCT_NAME, discover_word());
        (void)set_window_text_utf8_i18n(child, title);
    } else if (strstr(utf8, " | Version ") != NULL ||
               strstr(utf8, " | Wersja ") != NULL) {
        char subtitle[384];
        (void)snprintf(subtitle, sizeof(subtitle), "%s | %s %s",
                       product_subtitle(), version_word(), LINK_PRODUCT_VERSION);
        (void)set_window_text_utf8_i18n(child, subtitle);
    }
    return TRUE;
}

static void retranslate_main_window(HWND window)
{
    char title[384];
    (void)snprintf(title, sizeof(title), "%s %s - OpenPort 2.0 / J2534",
                   LINK_PRODUCT_NAME, discover_word());
    (void)set_window_text_utf8_i18n(window, title);
    (void)EnumChildWindows(window, retranslate_child, 0);
    retranslate_menu(window);
}

static void show_localised_about(HWND window)
{
    char title[256];
    char content[2048];
    wchar_t wide_title[256];
    wchar_t wide_content[2048];

    (void)snprintf(title, sizeof(title), "%s %s",
                   link_i18n_tr("common.about"), LINK_PRODUCT_NAME);
    (void)snprintf(content, sizeof(content),
                   "%s %s\n%s\n\n%s\n%s\n\n%s\n%s\nGPL-3.0-or-later",
                   version_word(), LINK_PRODUCT_VERSION,
                   product_subtitle(),
                   link_i18n_tr("discover.about_line"),
                   link_i18n_tr("discover.about_safety"),
                   LINK_PRODUCT_WEBSITE,
                   LINK_PRODUCT_COPYRIGHT);
    if (!utf8_to_wide_i18n(title, wide_title,
                           sizeof(wide_title) / sizeof(wide_title[0])) ||
        !utf8_to_wide_i18n(content, wide_content,
                           sizeof(wide_content) / sizeof(wide_content[0]))) return;
    (void)MessageBoxW(window, wide_content, wide_title,
                      MB_OK | MB_ICONINFORMATION);
}

static LRESULT CALLBACK language_window_proc(HWND window, UINT message,
                                             WPARAM wparam, LPARAM lparam)
{
    if (message == WM_COMMAND) {
        const UINT command = LOWORD(wparam);
        const char *locale = NULL;
        if (command == IDM_LANGUAGE_ENGLISH) locale = "en-AU";
        else if (command == IDM_LANGUAGE_GERMAN) locale = "de-DE";
        else if (command == IDM_LANGUAGE_POLISH) locale = "pl-PL";
        if (locale != NULL) {
            (void)link_i18n_set_locale(locale);
            save_locale(locale);
            retranslate_main_window(window);
            return 0;
        }
        if (command == IDM_HELP_ABOUT) {
            show_localised_about(window);
            return 0;
        }
    }
    return original_main_window_proc != NULL
        ? CallWindowProcA(original_main_window_proc, window, message, wparam, lparam)
        : DefWindowProcA(window, message, wparam, lparam);
}

static void attach_language_window_proc(HWND window, const char *class_name)
{
    if (window == NULL || class_name == NULL ||
        strcmp(class_name, LINK_PRODUCT_WINDOW_CLASS) != 0) return;
    main_window = window;
    original_main_window_proc = (WNDPROC)(LONG_PTR)SetWindowLongPtrA(
        window, GWLP_WNDPROC, (LONG_PTR)language_window_proc);
    retranslate_main_window(window);
}

HWND link_win_i18n_create_window_a(const char *class_name,
                                   const char *window_name,
                                   DWORD style,
                                   int x, int y, int width, int height,
                                   HWND parent, HMENU menu,
                                   HINSTANCE instance, LPVOID parameter)
{
    HWND window = CreateWindowExA(0U, class_name, window_name, style,
                                  x, y, width, height, parent, menu,
                                  instance, parameter);
    if (window != NULL)
        (void)set_window_text_utf8_i18n(window,
                                        link_win_i18n_translate_text(window_name));
    attach_language_window_proc(window, class_name);
    return window;
}

HWND link_win_i18n_create_window_ex_a(DWORD extended_style,
                                      const char *class_name,
                                      const char *window_name,
                                      DWORD style,
                                      int x, int y, int width, int height,
                                      HWND parent, HMENU menu,
                                      HINSTANCE instance, LPVOID parameter)
{
    HWND window = CreateWindowExA(extended_style, class_name, window_name, style,
                                  x, y, width, height, parent, menu,
                                  instance, parameter);
    if (window != NULL && window_name != NULL)
        (void)set_window_text_utf8_i18n(window,
                                        link_win_i18n_translate_text(window_name));
    attach_language_window_proc(window, class_name);
    return window;
}

BOOL link_win_i18n_append_menu_a(HMENU menu, UINT flags,
                                 UINT_PTR item, const char *text)
{
    if (text != NULL && strcmp(text, "&Help") == 0 &&
        (flags & MF_POPUP) != 0U) {
        HMENU language_menu = CreatePopupMenu();
        char label[128];
        if (language_menu != NULL) {
            (void)append_menu_utf8(language_menu, MF_STRING,
                                   IDM_LANGUAGE_ENGLISH, "English");
            (void)append_menu_utf8(language_menu, MF_STRING,
                                   IDM_LANGUAGE_GERMAN, "Deutsch");
            (void)append_menu_utf8(language_menu, MF_STRING,
                                   IDM_LANGUAGE_POLISH, "Polski");
            (void)snprintf(label, sizeof(label), "&%s",
                           link_i18n_tr("language.label"));
            (void)append_menu_utf8(menu, MF_POPUP,
                                   (UINT_PTR)language_menu, label);
            (void)CheckMenuRadioItem(language_menu,
                                     IDM_LANGUAGE_ENGLISH, IDM_LANGUAGE_POLISH,
                                     selected_language_command(), MF_BYCOMMAND);
        }
    }
    return append_menu_utf8(menu, flags, item,
                            link_win_i18n_translate_text(text));
}

int link_win_i18n_message_box_a(HWND window, const char *text,
                                const char *caption, UINT type)
{
    wchar_t wide_text[1024];
    wchar_t wide_caption[512];
    if (!utf8_to_wide_i18n(link_win_i18n_translate_text(text), wide_text,
                           sizeof(wide_text) / sizeof(wide_text[0])) ||
        !utf8_to_wide_i18n(caption != NULL ? caption : "", wide_caption,
                           sizeof(wide_caption) / sizeof(wide_caption[0])))
        return 0;
    return MessageBoxW(window, wide_text, wide_caption, type);
}
