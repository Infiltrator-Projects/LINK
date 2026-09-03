/* SPDX-License-Identifier: GPL-3.0-or-later */
#define WIN32_LEAN_AND_MEAN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0602
#endif

#include "link-windows-about.h"

#include <commctrl.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#ifndef LINK_THEME_BACKGROUND
#define LINK_THEME_BACKGROUND RGB(18, 21, 25)
#endif
#ifndef LINK_THEME_PANEL
#define LINK_THEME_PANEL RGB(28, 32, 37)
#endif
#ifndef LINK_THEME_TEXT
#define LINK_THEME_TEXT RGB(235, 239, 242)
#endif
#ifndef LINK_THEME_ACCENT
#define LINK_THEME_ACCENT RGB(190, 199, 207)
#endif
#ifndef LINK_PRODUCT_FONT_UI
#define LINK_PRODUCT_FONT_UI "Segoe UI"
#endif

static HBRUSH link_windows_about_background_brush;
static HFONT link_windows_about_font;
static WNDPROC link_windows_about_original_proc;


static int link_windows_about_has_text(const char *value)
{
    return value != NULL && value[0] != '\0';
}

static int link_windows_about_utf8_to_wide(
    const char *source, wchar_t *destination, size_t destination_count)
{
    int converted;

    if (destination == NULL || destination_count == 0U) return 0;
    destination[0] = L'\0';
    if (source == NULL) return 1;

    converted = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, source, -1,
        destination, (int)destination_count);
    if (converted == 0) {
        converted = MultiByteToWideChar(
            CP_ACP, 0, source, -1,
            destination, (int)destination_count);
    }
    return converted != 0;
}

static void link_windows_about_append(
    char *buffer, size_t capacity, const char *text)
{
    size_t used;

    if (buffer == NULL || capacity == 0U ||
        !link_windows_about_has_text(text)) return;
    used = strlen(buffer);
    if (used >= capacity - 1U) return;
    (void)snprintf(buffer + used, capacity - used, "%s", text);
    buffer[capacity - 1U] = '\0';
}

static void link_windows_about_field(
    char *buffer, size_t capacity,
    const char *label, const char *value)
{
    if (!link_windows_about_has_text(value)) return;
    if (buffer[0] != '\0') link_windows_about_append(buffer, capacity, "\n");
    if (link_windows_about_has_text(label)) {
        link_windows_about_append(buffer, capacity, label);
        link_windows_about_append(buffer, capacity, ": ");
    }
    link_windows_about_append(buffer, capacity, value);
}

static void link_windows_about_apply_nonclient(HWND window)
{
    typedef HRESULT (WINAPI *DwmSetWindowAttributeFn)(
        HWND, DWORD, LPCVOID, DWORD);
    HMODULE module = LoadLibraryA("dwmapi.dll");
    DwmSetWindowAttributeFn set_attribute = NULL;
    BOOL dark = TRUE;
    COLORREF caption = (COLORREF)LINK_THEME_BACKGROUND;
    COLORREF text = (COLORREF)LINK_THEME_TEXT;
    COLORREF border = (COLORREF)LINK_THEME_ACCENT;

    if (module == NULL) return;
    {
        FARPROC proc = GetProcAddress(module, "DwmSetWindowAttribute");
        if (proc != NULL)
            memcpy(&set_attribute, &proc, sizeof(set_attribute));
    }
    if (set_attribute != NULL) {
        (void)set_attribute(window, 20U, &dark, (DWORD)sizeof(dark));
        (void)set_attribute(window, 35U, &caption, (DWORD)sizeof(caption));
        (void)set_attribute(window, 36U, &text, (DWORD)sizeof(text));
        (void)set_attribute(window, 34U, &border, (DWORD)sizeof(border));
    }
    FreeLibrary(module);
}

static void link_windows_about_apply_control_theme(HWND control)
{
    typedef HRESULT (WINAPI *SetWindowThemeFn)(HWND, LPCWSTR, LPCWSTR);
    HMODULE module = LoadLibraryA("uxtheme.dll");
    SetWindowThemeFn set_theme = NULL;

    if (module == NULL) return;
    {
        FARPROC proc = GetProcAddress(module, "SetWindowTheme");
        if (proc != NULL) memcpy(&set_theme, &proc, sizeof(set_theme));
    }
    if (set_theme != NULL)
        (void)set_theme(control, L"DarkMode_Explorer", NULL);
    FreeLibrary(module);
}

static HFONT link_windows_about_make_font(HWND window)
{
    wchar_t family[LF_FACESIZE];
    HDC dc = GetDC(window);
    int dpi = dc != NULL ? GetDeviceCaps(dc, LOGPIXELSY) : 96;
    int height = -MulDiv(10, dpi, 72);

    if (dc != NULL) ReleaseDC(window, dc);
    if (!link_windows_about_utf8_to_wide(
            LINK_PRODUCT_FONT_UI, family,
            sizeof(family) / sizeof(family[0]))) {
        (void)wcscpy_s(family, sizeof(family) / sizeof(family[0]),
                       L"Segoe UI");
    }

    return CreateFontW(
        height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, family);
}

static BOOL CALLBACK link_windows_about_prepare_child(
    HWND child, LPARAM parameter)
{
    HFONT font = (HFONT)parameter;
    link_windows_about_apply_control_theme(child);
    if (font != NULL)
        SendMessageW(child, WM_SETFONT, (WPARAM)font, TRUE);
    return TRUE;
}

static LRESULT CALLBACK link_windows_about_window_proc(
    HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    WNDPROC original = link_windows_about_original_proc;

    switch (message) {
    case WM_ERASEBKGND:
        if (link_windows_about_background_brush != NULL) {
            RECT rect;
            HDC dc = (HDC)wparam;
            if (dc != NULL) {
                GetClientRect(window, &rect);
                FillRect(dc, &rect, link_windows_about_background_brush);
                return 1;
            }
        }
        break;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
        if (link_windows_about_background_brush != NULL) {
            HDC dc = (HDC)wparam;
            if (dc != NULL) {
                SetTextColor(dc, (COLORREF)LINK_THEME_TEXT);
                SetBkColor(dc, (COLORREF)LINK_THEME_BACKGROUND);
                SetBkMode(dc, TRANSPARENT);
                return (LRESULT)(INT_PTR)link_windows_about_background_brush;
            }
        }
        break;
    case WM_NCDESTROY:
        if (original != NULL) {
            LRESULT result;
            (void)SetWindowLongPtrW(
                window, GWLP_WNDPROC, (LONG_PTR)original);
            link_windows_about_original_proc = NULL;
            result = CallWindowProcW(
                original, window, message, wparam, lparam);
            if (link_windows_about_font != NULL) {
                DeleteObject(link_windows_about_font);
                link_windows_about_font = NULL;
            }
            if (link_windows_about_background_brush != NULL) {
                DeleteObject(link_windows_about_background_brush);
                link_windows_about_background_brush = NULL;
            }
            return result;
        }
        break;
    default:
        break;
    }

    return original != NULL
        ? CallWindowProcW(original, window, message, wparam, lparam)
        : DefWindowProcW(window, message, wparam, lparam);
}

static HRESULT CALLBACK link_windows_about_callback(
    HWND window, UINT notification, WPARAM wparam, LPARAM lparam,
    LONG_PTR reference_data)
{
    (void)wparam;
    (void)reference_data;

    if (notification == TDN_CREATED) {
        link_windows_about_background_brush =
            CreateSolidBrush((COLORREF)LINK_THEME_BACKGROUND);
        link_windows_about_font = link_windows_about_make_font(window);
        link_windows_about_apply_nonclient(window);
        link_windows_about_apply_control_theme(window);
        link_windows_about_original_proc = (WNDPROC)SetWindowLongPtrW(
            window, GWLP_WNDPROC,
            (LONG_PTR)link_windows_about_window_proc);
        (void)EnumChildWindows(
            window, link_windows_about_prepare_child,
            (LPARAM)link_windows_about_font);
        InvalidateRect(window, NULL, TRUE);
    }

    if (notification == TDN_HYPERLINK_CLICKED && lparam != 0) {
        (void)ShellExecuteW(
            window, L"open", (LPCWSTR)lparam, NULL, NULL, SW_SHOWNORMAL);
    }
    return S_OK;
}

void link_windows_show_about(HWND parent,
                             HICON icon,
                             const LinkAboutInfo *info)
{
    char title_utf8[256];
    char content_utf8[8192] = "";
    wchar_t title[256];
    wchar_t product_name[256];
    wchar_t content[8192];
    TASKDIALOGCONFIG config;
    HRESULT result;

    if (info == NULL || !link_windows_about_has_text(info->product_name))
        return;

    (void)snprintf(
        title_utf8, sizeof(title_utf8), "About %s", info->product_name);
    title_utf8[sizeof(title_utf8) - 1U] = '\0';

    link_windows_about_field(
        content_utf8, sizeof(content_utf8), "Version", info->version);
    link_windows_about_field(
        content_utf8, sizeof(content_utf8), NULL, info->subtitle);
    link_windows_about_field(
        content_utf8, sizeof(content_utf8), NULL, info->description);
    link_windows_about_field(
        content_utf8, sizeof(content_utf8), "Release date", info->release_date);
    link_windows_about_field(
        content_utf8, sizeof(content_utf8), "Authors", info->authors);
    link_windows_about_field(
        content_utf8, sizeof(content_utf8), "Credits", info->credits);

    if (link_windows_about_has_text(info->website)) {
        char website[2048];
        (void)snprintf(
            website, sizeof(website),
            "<a href=\"%s\">Project website</a>", info->website);
        website[sizeof(website) - 1U] = '\0';
        link_windows_about_field(
            content_utf8, sizeof(content_utf8), NULL, website);
    }

    link_windows_about_field(
        content_utf8, sizeof(content_utf8), NULL, info->copyright);
    if (link_windows_about_has_text(info->license_text)) {
        link_windows_about_field(
            content_utf8, sizeof(content_utf8),
            info->license_name, info->license_text);
    } else {
        link_windows_about_field(
            content_utf8, sizeof(content_utf8),
            "Licence", info->license_name);
    }

    if (!link_windows_about_utf8_to_wide(
            title_utf8, title, sizeof(title) / sizeof(title[0])) ||
        !link_windows_about_utf8_to_wide(
            info->product_name, product_name,
            sizeof(product_name) / sizeof(product_name[0])) ||
        !link_windows_about_utf8_to_wide(
            content_utf8, content, sizeof(content) / sizeof(content[0]))) {
        (void)MessageBoxA(
            parent, "Unable to prepare About information.",
            "LINK About", MB_OK | MB_ICONERROR);
        return;
    }

    memset(&config, 0, sizeof(config));
    config.cbSize = sizeof(config);
    config.hwndParent = parent;
    config.dwFlags =
        TDF_ENABLE_HYPERLINKS |
        TDF_POSITION_RELATIVE_TO_WINDOW |
        TDF_SIZE_TO_CONTENT;
    if (icon != NULL) {
        config.dwFlags |= TDF_USE_HICON_MAIN;
        config.hMainIcon = icon;
    }
    config.dwCommonButtons = TDCBF_CLOSE_BUTTON;
    config.pszWindowTitle = title;
    config.pszMainInstruction = product_name;
    config.pszContent = content;
    config.pfCallback = link_windows_about_callback;

    result = TaskDialogIndirect(&config, NULL, NULL, NULL);
    if (FAILED(result)) {
        (void)MessageBoxW(
            parent, content, title, MB_OK | MB_ICONINFORMATION);
    }
}
