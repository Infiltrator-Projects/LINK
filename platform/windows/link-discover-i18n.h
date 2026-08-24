// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LINK_DISCOVER_I18N_H
#define LINK_DISCOVER_I18N_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

HWND link_win_i18n_create_window_a(const char *class_name,
                                   const char *window_name,
                                   DWORD style,
                                   int x, int y, int width, int height,
                                   HWND parent, HMENU menu,
                                   HINSTANCE instance, LPVOID parameter);
HWND link_win_i18n_create_window_ex_a(DWORD extended_style,
                                      const char *class_name,
                                      const char *window_name,
                                      DWORD style,
                                      int x, int y, int width, int height,
                                      HWND parent, HMENU menu,
                                      HINSTANCE instance, LPVOID parameter);
BOOL link_win_i18n_append_menu_a(HMENU menu, UINT flags,
                                 UINT_PTR item, const char *text);
int link_win_i18n_message_box_a(HWND window, const char *text,
                                const char *caption, UINT type);

#undef CreateWindowA
#undef CreateWindowExA
#undef AppendMenuA
#undef MessageBoxA
#define CreateWindowA(class_name, window_name, style, x, y, width, height, parent, menu, instance, parameter) \
    link_win_i18n_create_window_a((class_name), (window_name), (style), (x), (y), (width), (height), (parent), (menu), (instance), (parameter))
#define CreateWindowExA(extended_style, class_name, window_name, style, x, y, width, height, parent, menu, instance, parameter) \
    link_win_i18n_create_window_ex_a((extended_style), (class_name), (window_name), (style), (x), (y), (width), (height), (parent), (menu), (instance), (parameter))
#define AppendMenuA(menu, flags, item, text) \
    link_win_i18n_append_menu_a((menu), (flags), (item), (text))
#define MessageBoxA(window, text, caption, type) \
    link_win_i18n_message_box_a((window), (text), (caption), (type))

#endif
