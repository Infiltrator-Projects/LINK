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
#define LINK_PRODUCT_WEBSITE "https://github.com/Infiltrator-Projects/LINK"
#endif

#define IDM_FILE_EXPORT 41001
#define IDM_FILE_EXIT 41002
#define IDM_HELP_ABOUT 42001
#define IDM_LANGUAGE_FIRST 43000
#define IDM_LANGUAGE_CAPACITY 64

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

static void scan_language_pack_directories(void)
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
        (void)link_i18n_select_locale(value);
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
    scan_language_pack_directories();
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
    return (locale != NULL && locale[0] != '\0') ? locale : "en-AU";
}

static UINT selected_language_command(void)
{
    const char *locale = selected_locale();
    size_t index;
    size_t count = link_i18n_installed_locale_count();
    for (index = 0U; index < count; ++index) {
        const char *candidate = link_i18n_installed_locale(index);
        if (candidate != NULL && strcmp(locale, candidate) == 0)
  return IDM_LANGUAGE_FIRST + (UINT)index;
    }
    for (index = 0U; index < count; ++index) {
        const char *candidate = link_i18n_installed_locale(index);
        if (candidate != NULL && locale[0] == candidate[0] && locale[1] == candidate[1])
  return IDM_LANGUAGE_FIRST + (UINT)index;
    }
    return IDM_LANGUAGE_FIRST;
}

static const char *semantic_key_from_any_language(const char *text)
{
    static char matched_key[64];
    char original[32];
    size_t locale_index;
    size_t key_index;
    const char *current;

    if (text == NULL || text[0] == '\0') return NULL;
    current = selected_locale();
    (void)snprintf(original, sizeof(original), "%s", current);
    matched_key[0] = '\0';

    for (locale_index = 0U;
         locale_index < link_i18n_installed_locale_count() && matched_key[0] == '\0';
         ++locale_index) {
        const char *candidate = link_i18n_installed_locale(locale_index);
        if (candidate == NULL || !link_i18n_select_locale(candidate)) continue;
        for (key_index = 0U;
   key_index < sizeof(semantic_keys) / sizeof(semantic_keys[0]);
   ++key_index) {
  if (strcmp(text, link_i18n_text(semantic_keys[key_index])) == 0) {
      (void)snprintf(matched_key, sizeof(matched_key), "%s",
                     semantic_keys[key_index]);
      break;
  }
        }
    }
    (void)link_i18n_select_locale(original);
    return matched_key[0] != '\0' ? matched_key : NULL;
}

static int locale_is(const char *language)
{
    const char *locale = selected_locale();
    const size_t length = strlen(language);
    return strncmp(locale, language, length) == 0 &&
 (locale[length] == '\0' || locale[length] == '-');
}

static const char *product_subtitle(void)
{
    const int mercedes = strcmp(LINK_PRODUCT_SUBTITLE, "Mercedes-Benz Diagnostics") == 0;
    const int jaguar = strcmp(LINK_PRODUCT_SUBTITLE, "Jaguar X400 Diagnostics") == 0;
    if (locale_is("de")) return mercedes ? "Mercedes-Benz-Diagnose" : jaguar ? "Jaguar-X400-Diagnose" : "Fahrzeugdiagnose";
    if (locale_is("fr")) return mercedes ? "Diagnostic Mercedes-Benz" : jaguar ? "Diagnostic Jaguar X400" : "Diagnostic du véhicule";
    if (locale_is("es")) return mercedes ? "Diagnóstico Mercedes-Benz" : jaguar ? "Diagnóstico Jaguar X400" : "Diagnóstico del vehículo";
    if (locale_is("it")) return mercedes ? "Diagnostica Mercedes-Benz" : jaguar ? "Diagnostica Jaguar X400" : "Diagnostica del veicolo";
    if (locale_is("pl")) return mercedes ? "Diagnostyka Mercedes-Benz" : jaguar ? "Diagnostyka Jaguar X400" : "Diagnostyka pojazdu";
    if (locale_is("pt")) return mercedes ? "Diagnóstico Mercedes-Benz" : jaguar ? "Diagnóstico Jaguar X400" : "Diagnóstico do veículo";
    if (locale_is("zh")) return mercedes ? "梅赛德斯-奔驰诊断" : jaguar ? "Jaguar X400 诊断" : "车辆诊断";
    if (locale_is("hi")) return mercedes ? "Mercedes-Benz निदान" : jaguar ? "Jaguar X400 निदान" : "वाहन निदान";
    if (locale_is("ar")) return mercedes ? "تشخيص Mercedes-Benz" : jaguar ? "تشخيص Jaguar X400" : "تشخيص المركبة";
    if (locale_is("ja")) return mercedes ? "Mercedes-Benz診断" : jaguar ? "Jaguar X400診断" : "車両診断";
    if (locale_is("ko")) return mercedes ? "Mercedes-Benz 진단" : jaguar ? "Jaguar X400 진단" : "차량 진단";
    if (locale_is("id")) return mercedes ? "Diagnostik Mercedes-Benz" : jaguar ? "Diagnostik Jaguar X400" : "Diagnostik kendaraan";
    return LINK_PRODUCT_SUBTITLE;
}

static const char *discover_word(void)
{
    if (locale_is("de")) return "Erkennung";
    if (locale_is("fr")) return "Découverte";
    if (locale_is("es")) return "Detección";
    if (locale_is("it")) return "Rilevamento";
    if (locale_is("pl")) return "Wykrywanie";
    if (locale_is("pt")) return "Detecção";
    if (locale_is("zh")) return "检测";
    if (locale_is("hi")) return "खोज";
    if (locale_is("ar")) return "اكتشاف";
    if (locale_is("ja")) return "検出";
    if (locale_is("ko")) return "검색";
    if (locale_is("id")) return "Deteksi";
    return "Discover";
}

static const char *version_word(void)
{
    if (locale_is("es")) return "Versión";
    if (locale_is("it")) return "Versione";
    if (locale_is("pl")) return "Wersja";
    if (locale_is("pt")) return "Versão";
    if (locale_is("zh")) return "版本";
    if (locale_is("hi")) return "संस्करण";
    if (locale_is("ar")) return "الإصدار";
    if (locale_is("ja")) return "バージョン";
    if (locale_is("ko")) return "버전";
    if (locale_is("id")) return "Versi";
    return "Version";
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
    if (key != NULL) return link_i18n_text(key);

    if (strcmp(text, "&File") == 0) {
        (void)snprintf(buffer, sizeof(buffer), "&%s", link_i18n_text("discover.file"));
        return buffer;
    }
    if (strcmp(text, "&Help") == 0) {
        (void)snprintf(buffer, sizeof(buffer), "&%s", link_i18n_text("discover.help"));
        return buffer;
    }
    if (strcmp(text, "E&xit") == 0) return link_i18n_text("discover.exit");
    if (strcmp(text, "&Export evidence...\tCtrl+E") == 0) {
        (void)snprintf(buffer, sizeof(buffer), "%s\tCtrl+E",
                       link_i18n_text("discover.export_evidence"));
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
        (void)link_i18n_format_text(buffer, sizeof(buffer), "discover.about",
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

    set_menu_text_by_position(root, 0U, link_i18n_text("discover.file"));
    set_menu_text_by_position(root, 1U, link_i18n_text("language.label"));
    set_menu_text_by_position(root, 2U, link_i18n_text("discover.help"));

    (void)snprintf(text, sizeof(text), "%s\tCtrl+E",
                   link_i18n_text("discover.export_evidence"));
    set_menu_text_by_command(file_menu, IDM_FILE_EXPORT, text);
    set_menu_text_by_command(file_menu, IDM_FILE_EXIT, link_i18n_text("discover.exit"));

    argument.name = "product";
    argument.value = LINK_PRODUCT_NAME;
    (void)link_i18n_format_text(text, sizeof(text), "discover.about", &argument, 1U);
    set_menu_text_by_command(help_menu, IDM_HELP_ABOUT, text);

    selected = selected_language_command();
    if (language_menu != NULL)
        (void)CheckMenuRadioItem(language_menu,
                                 IDM_LANGUAGE_FIRST, IDM_LANGUAGE_FIRST + (UINT)link_i18n_supported_locale_count() - 1U,
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
        strstr(utf8, " | ") == NULL) {
        char title[256];
        (void)snprintf(title, sizeof(title), "%s %s",
                       LINK_PRODUCT_NAME, discover_word());
        (void)set_window_text_utf8_i18n(child, title);
    } else if (strstr(utf8, " | ") != NULL &&
               strstr(utf8, LINK_PRODUCT_VERSION) != NULL) {
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
                   link_i18n_text("common.about"), LINK_PRODUCT_NAME);
    (void)snprintf(content, sizeof(content),
                   "%s %s\n%s\n\n%s\n%s\n\n%s\n%s\nGPL-3.0-or-later",
                   version_word(), LINK_PRODUCT_VERSION,
                   product_subtitle(),
                   link_i18n_text("discover.about_line"),
                   link_i18n_text("discover.about_safety"),
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
        const size_t count = link_i18n_installed_locale_count();
        const char *locale = NULL;
        if (command >= IDM_LANGUAGE_FIRST &&
  command < IDM_LANGUAGE_FIRST + (UINT)count &&
  command < IDM_LANGUAGE_FIRST + IDM_LANGUAGE_CAPACITY)
  locale = link_i18n_installed_locale((size_t)(command - IDM_LANGUAGE_FIRST));
        if (locale != NULL) {
            (void)link_i18n_select_locale(locale);
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
  size_t language_index;
  size_t count = link_i18n_installed_locale_count();
  if (count > IDM_LANGUAGE_CAPACITY) count = IDM_LANGUAGE_CAPACITY;
  for (language_index = 0U; language_index < count; ++language_index) {
      const char *name = link_i18n_installed_locale_name(language_index);
      if (name != NULL)
          (void)append_menu_utf8(language_menu, MF_STRING,
                                 IDM_LANGUAGE_FIRST + (UINT)language_index,
                                 name);
  }
  (void)snprintf(label, sizeof(label), "&%s",
                 link_i18n_text("language.label"));
  (void)append_menu_utf8(menu, MF_POPUP,
                         (UINT_PTR)language_menu, label);
  if (count > 0U)
      (void)CheckMenuRadioItem(language_menu,
                               IDM_LANGUAGE_FIRST,
                               IDM_LANGUAGE_FIRST + (UINT)count - 1U,
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
