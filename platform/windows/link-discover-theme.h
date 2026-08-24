/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef LINK_DISCOVER_THEME_H
#define LINK_DISCOVER_THEME_H

/*
 * Shared Win32 presentation policy for LINK-family Discover applications.
 * Product repositories supply only palette values through compile definitions;
 * LINK owns how those values are applied to native Windows controls.
 *
 * This header is force-included for the Discover translation unit so it can
 * intercept the final DefWindowProcA fallback used by both the shared shell and
 * the textually-included backend without duplicating UI code in MBLINK/JAGLINK.
 */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wchar.h>

#ifndef LINK_THEME_BACKGROUND
#define LINK_THEME_BACKGROUND RGB(18, 21, 25)
#endif
#ifndef LINK_THEME_PANEL
#define LINK_THEME_PANEL RGB(28, 32, 37)
#endif
#ifndef LINK_THEME_INPUT
#define LINK_THEME_INPUT RGB(14, 17, 21)
#endif
#ifndef LINK_THEME_TEXT
#define LINK_THEME_TEXT RGB(235, 239, 242)
#endif
#ifndef LINK_THEME_MUTED
#define LINK_THEME_MUTED RGB(150, 158, 166)
#endif
#ifndef LINK_THEME_ACCENT
#define LINK_THEME_ACCENT RGB(190, 199, 207)
#endif
#ifndef LINK_THEME_ACCENT_TEXT
#define LINK_THEME_ACCENT_TEXT RGB(16, 19, 22)
#endif

#define LINK_THEME_ID_CONNECT 1002
#define LINK_THEME_ID_STATUS 1009

static HBRUSH link_theme_background_brush;
static HBRUSH link_theme_panel_brush;
static HBRUSH link_theme_input_brush;
static HBRUSH link_theme_accent_brush;

static void link_theme_ensure_brushes(void)
{
    if (link_theme_background_brush == NULL)
        link_theme_background_brush = CreateSolidBrush((COLORREF)LINK_THEME_BACKGROUND);
    if (link_theme_panel_brush == NULL)
        link_theme_panel_brush = CreateSolidBrush((COLORREF)LINK_THEME_PANEL);
    if (link_theme_input_brush == NULL)
        link_theme_input_brush = CreateSolidBrush((COLORREF)LINK_THEME_INPUT);
    if (link_theme_accent_brush == NULL)
        link_theme_accent_brush = CreateSolidBrush((COLORREF)LINK_THEME_ACCENT);
}

static void link_theme_apply_nonclient(HWND window)
{
    typedef HRESULT (WINAPI *DwmSetWindowAttributeFn)(HWND, DWORD, LPCVOID, DWORD);
    HMODULE module = LoadLibraryA("dwmapi.dll");
    DwmSetWindowAttributeFn set_attribute = NULL;
    BOOL dark = TRUE;
    COLORREF caption = (COLORREF)LINK_THEME_BACKGROUND;
    COLORREF text = (COLORREF)LINK_THEME_TEXT;
    COLORREF border = (COLORREF)LINK_THEME_ACCENT;

    if (module == NULL) return;
    {
        FARPROC proc = GetProcAddress(module, "DwmSetWindowAttribute");
        if (proc != NULL) memcpy(&set_attribute, &proc, sizeof(set_attribute));
    }
    if (set_attribute != NULL) {
        /* 20 is DWMWA_USE_IMMERSIVE_DARK_MODE on supported Windows 10/11. */
        (void)set_attribute(window, 20U, &dark, (DWORD)sizeof(dark));
        /* Windows 11 caption/border colours; unsupported systems ignore them. */
        (void)set_attribute(window, 35U, &caption, (DWORD)sizeof(caption));
        (void)set_attribute(window, 36U, &text, (DWORD)sizeof(text));
        (void)set_attribute(window, 34U, &border, (DWORD)sizeof(border));
    }
    FreeLibrary(module);
}

static void link_theme_apply_control_theme(HWND control)
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

static BOOL CALLBACK link_theme_prepare_child(HWND child, LPARAM parameter)
{
    char class_name[32];
    LONG_PTR style;
    UINT type;
    (void)parameter;

    class_name[0] = '\0';
    (void)GetClassNameA(child, class_name, (int)sizeof(class_name));
    link_theme_apply_control_theme(child);

    if (_stricmp(class_name, "Button") != 0) return TRUE;
    style = GetWindowLongPtrA(child, GWL_STYLE);
    type = (UINT)(style & BS_TYPEMASK);
    if (type == BS_PUSHBUTTON || type == BS_DEFPUSHBUTTON) {
        style &= ~(LONG_PTR)BS_TYPEMASK;
        style |= BS_OWNERDRAW;
        (void)SetWindowLongPtrA(child, GWL_STYLE, style);
        (void)SetWindowPos(child, NULL, 0, 0, 0, 0,
                           SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                           SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
    return TRUE;
}

static COLORREF link_theme_button_fill(const DRAWITEMSTRUCT *item)
{
    if (item != NULL && item->CtlID == LINK_THEME_ID_CONNECT)
        return (COLORREF)LINK_THEME_ACCENT;
    return (COLORREF)LINK_THEME_PANEL;
}

static COLORREF link_theme_button_text(const DRAWITEMSTRUCT *item)
{
    if (item == NULL) return (COLORREF)LINK_THEME_TEXT;
    if ((item->itemState & ODS_DISABLED) != 0U)
        return (COLORREF)LINK_THEME_MUTED;
    if (item->CtlID == LINK_THEME_ID_CONNECT)
        return (COLORREF)LINK_THEME_ACCENT_TEXT;
    return (COLORREF)LINK_THEME_TEXT;
}

static LRESULT link_theme_draw_button(const DRAWITEMSTRUCT *item)
{
    RECT rect;
    char text[256];
    HBRUSH fill;
    HBRUSH border;
    COLORREF fill_color;
    COLORREF text_color;
    int old_mode;
    COLORREF old_text;

    if (item == NULL || item->CtlType != ODT_BUTTON) return FALSE;
    rect = item->rcItem;
    fill_color = link_theme_button_fill(item);
    text_color = link_theme_button_text(item);
    fill = CreateSolidBrush(fill_color);
    border = CreateSolidBrush((COLORREF)LINK_THEME_ACCENT);
    if (fill != NULL) {
        FillRect(item->hDC, &rect, fill);
        DeleteObject(fill);
    }
    if (border != NULL) {
        FrameRect(item->hDC, &rect, border);
        DeleteObject(border);
    }
    if ((item->itemState & ODS_SELECTED) != 0U)
        InflateRect(&rect, -2, -2);

    text[0] = '\0';
    (void)GetWindowTextA(item->hwndItem, text, (int)sizeof(text));
    old_mode = SetBkMode(item->hDC, TRANSPARENT);
    old_text = SetTextColor(item->hDC, text_color);
    (void)DrawTextA(item->hDC, text, -1, &rect,
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    (void)SetTextColor(item->hDC, old_text);
    (void)SetBkMode(item->hDC, old_mode);
    if ((item->itemState & ODS_FOCUS) != 0U) {
        InflateRect(&rect, -3, -3);
        DrawFocusRect(item->hDC, &rect);
    }
    return TRUE;
}

static LRESULT WINAPI link_theme_def_window_proc(HWND window, UINT message,
                                                  WPARAM wparam, LPARAM lparam)
{
    HDC dc;
    HWND control;
    RECT rect;
    int control_id;

    link_theme_ensure_brushes();
    switch (message) {
    case WM_SHOWWINDOW:
        if (wparam != 0U) {
            link_theme_apply_nonclient(window);
            (void)EnumChildWindows(window, link_theme_prepare_child, 0);
            InvalidateRect(window, NULL, TRUE);
        }
        break;

    case WM_ERASEBKGND:
        dc = (HDC)wparam;
        if (dc != NULL && link_theme_background_brush != NULL) {
            GetClientRect(window, &rect);
            FillRect(dc, &rect, link_theme_background_brush);
            return 1;
        }
        break;

    case WM_CTLCOLORSTATIC:
        dc = (HDC)wparam;
        control = (HWND)lparam;
        control_id = control != NULL ? GetDlgCtrlID(control) : 0;
        SetTextColor(dc, (COLORREF)LINK_THEME_TEXT);
        SetBkMode(dc, TRANSPARENT);
        if (control_id == LINK_THEME_ID_STATUS && link_theme_panel_brush != NULL) {
            SetBkMode(dc, OPAQUE);
            SetBkColor(dc, (COLORREF)LINK_THEME_PANEL);
            return (LRESULT)(INT_PTR)link_theme_panel_brush;
        }
        return (LRESULT)(INT_PTR)link_theme_background_brush;

    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
        dc = (HDC)wparam;
        SetTextColor(dc, (COLORREF)LINK_THEME_TEXT);
        SetBkColor(dc, (COLORREF)LINK_THEME_INPUT);
        return (LRESULT)(INT_PTR)link_theme_input_brush;

    case WM_CTLCOLORBTN:
        dc = (HDC)wparam;
        SetTextColor(dc, (COLORREF)LINK_THEME_TEXT);
        SetBkColor(dc, (COLORREF)LINK_THEME_BACKGROUND);
        return (LRESULT)(INT_PTR)link_theme_background_brush;

    case WM_DRAWITEM:
        if (link_theme_draw_button((const DRAWITEMSTRUCT *)lparam)) return TRUE;
        break;

    default:
        break;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

/* Route the Discover fallback through LINK's product-aware theme handler. */
#define DefWindowProcA link_theme_def_window_proc

#endif
