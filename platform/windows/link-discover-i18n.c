// SPDX-License-Identifier: GPL-3.0-or-later
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "link/i18n.h"

#include <stdio.h>
#include <string.h>

static int link_win_i18n_initialised;

void link_win_i18n_init(void)
{
    if (link_win_i18n_initialised) return;
    link_i18n_init();
    (void)link_i18n_set_system_locale();
    link_win_i18n_initialised = 1;
}

static void ensure_locale(void)
{
    link_win_i18n_init();
}

static const char *literal_key(const char *text)
{
    static const struct {
        const char *text;
        const char *key;
    } mappings[] = {
        {"Vehicle interface", "discover.vehicle_interface"},
        {"J2534 FunctionLibrary DLL", "discover.j2534_dll"},
        {"Browse...", "discover.browse"},
        {"Connect passive 500 kbit/s", "discover.connect_passive"},
        {"Read-only OBD inventory", "discover.inventory"},
        {"Stop", "discover.stop"},
        {"Export evidence...", "discover.export_evidence"},
        {"DISCONNECTED - deny-by-default safety policy active", "discover.status_disconnected"},
        {"Diagnostic session log", "discover.session_log"},
        {"Evidence annotation", "discover.annotation"},
        {"Add annotation", "discover.add_annotation"},
        {"No J2534 FunctionLibrary DLL is selected.", "discover.no_dll"},
        {"Cannot create the evidence JSONL file.", "discover.evidence_failed"},
        {"Connect to the OpenPort/J2534 device first.", "discover.connect_first"},
        {"No evidence has been recorded yet.", "discover.no_evidence"}
    };
    size_t index;
    if (text == NULL) return NULL;
    for (index = 0U; index < sizeof(mappings) / sizeof(mappings[0]); ++index) {
        if (strcmp(text, mappings[index].text) == 0) return mappings[index].key;
    }
    return NULL;
}

const char *link_win_i18n_translate_text(const char *text)
{
    const char *key;
    static char buffer[512];
    static const char about_prefix[] = "&About ";
    size_t length;

    ensure_locale();
    if (text == NULL) return NULL;
    key = literal_key(text);
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

HWND link_win_i18n_create_window_a(const char *class_name,
                                   const char *window_name,
                                   DWORD style,
                                   int x, int y, int width, int height,
                                   HWND parent, HMENU menu,
                                   HINSTANCE instance, LPVOID parameter)
{
    return CreateWindowExA(0U, class_name, link_win_i18n_translate_text(window_name), style,
                           x, y, width, height, parent, menu, instance, parameter);
}

HWND link_win_i18n_create_window_ex_a(DWORD extended_style,
                                      const char *class_name,
                                      const char *window_name,
                                      DWORD style,
                                      int x, int y, int width, int height,
                                      HWND parent, HMENU menu,
                                      HINSTANCE instance, LPVOID parameter)
{
    return CreateWindowExA(extended_style, class_name,
                           link_win_i18n_translate_text(window_name),
                           style, x, y, width, height,
                           parent, menu, instance, parameter);
}

BOOL link_win_i18n_append_menu_a(HMENU menu, UINT flags,
                                 UINT_PTR item, const char *text)
{
    return AppendMenuA(menu, flags, item, link_win_i18n_translate_text(text));
}

int link_win_i18n_message_box_a(HWND window, const char *text,
                                const char *caption, UINT type)
{
    return MessageBoxA(window, link_win_i18n_translate_text(text), caption, type);
}
