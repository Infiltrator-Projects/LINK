/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file link-discover-ui.c
 * @brief Shared native Win32 shell for LINK-family Discover applications.
 *
 * LINK owns the Windows interaction model. Product repositories provide only
 * identity, version, copyright, website and the canonical application image.
 * The J2534 transport, safety classifier, bounded OBD inventory and evidence
 * writer remain in link-discover.c and are compiled into this shell exactly
 * once. Keeping presentation and transport in one LINK implementation prevents
 * MBLINK and JAGLINK from drifting into subtly different diagnostic tools.
 *
 * The shell intentionally uses native Win32 controls with Common Controls v6,
 * Segoe UI typography, DPI-aware layout, a real menu bar and a Task Dialog
 * About experience. The executable is expected to be self-contained when built
 * with MSVC; CMake therefore selects the static MSVC runtime for product faces.
 */

#define WIN32_LEAN_AND_MEAN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#ifdef _MSC_VER
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

/**
 * Convert UTF-8 product/status text to UTF-16 before handing it to Windows.
 *
 * The original shell mixed UTF-8 source literals with the ANSI Win32 API,
 * producing mojibake such as "a€”" on normal English Windows installations.
 * Conversion is centralised here so backend status messages can remain UTF-8
 * without depending on the machine's active ANSI code page.
 */
static int utf8_to_wide(const char *source, wchar_t *destination,
                        size_t destination_count)
{
    int converted;

    if (destination == NULL || destination_count == 0U) {
        return 0;
    }
    destination[0] = L'\0';
    if (source == NULL) {
        return 1;
    }

    converted = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                    source, -1, destination,
                                    (int)destination_count);
    if (converted == 0) {
        /* Defensive compatibility path for unexpected legacy ANSI strings. */
        converted = MultiByteToWideChar(CP_ACP, 0, source, -1, destination,
                                        (int)destination_count);
    }
    return converted != 0;
}

static BOOL set_window_text_utf8(HWND window, const char *text)
{
    wchar_t wide[1024];

    if (!utf8_to_wide(text != NULL ? text : "", wide,
                      sizeof(wide) / sizeof(wide[0]))) {
        return FALSE;
    }
    return SetWindowTextW(window, wide);
}

/*
 * Reuse the proven backend while giving this translation unit ownership of the
 * visible shell. Renaming the backend entry points avoids duplicate symbols.
 * SetWindowTextA is redirected only while compiling the backend so its UTF-8
 * status text is rendered correctly by Windows.
 */
#define SetWindowTextA set_window_text_utf8
#define WinMain link_discover_backend_WinMain
#define window_proc link_discover_backend_window_proc
#define create_controls link_discover_backend_create_controls
#include "link-discover.c"
#undef create_controls
#undef window_proc
#undef WinMain
#undef SetWindowTextA

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
#define IDC_BROWSE 1010

static HWND g_brand_icon;
static HWND g_brand_title;
static HWND g_brand_subtitle;
static HWND g_header_rule;
static HWND g_connection_group;
static HWND g_dll_label;
static HWND g_browse_button;
static HWND g_connect_button;
static HWND g_inventory_button;
static HWND g_stop_button;
static HWND g_export_button;
static HWND g_log_label;
static HWND g_note_label;
static HWND g_add_note_button;
static HFONT g_ui_font;
static HFONT g_title_font;
static HFONT g_subtitle_font;
static HFONT g_status_font;
static HICON g_product_icon;
static BOOL g_product_icon_owned;

static int window_dpi(HWND window)
{
    HDC dc = GetDC(window);
    int dpi = 96;

    if (dc != NULL) {
        dpi = GetDeviceCaps(dc, LOGPIXELSX);
        ReleaseDC(window, dc);
    }
    return dpi > 0 ? dpi : 96;
}

static int scale_px(HWND window, int logical_pixels)
{
    return MulDiv(logical_pixels, window_dpi(window), 96);
}

/** Create Segoe UI using the window's current logical DPI. */
static HFONT make_font(HWND window, int point_size, int weight)
{
    HDC dc = GetDC(window);
    int dpi = dc != NULL ? GetDeviceCaps(dc, LOGPIXELSY) : 96;
    int height = -MulDiv(point_size, dpi, 72);

    if (dc != NULL) {
        ReleaseDC(window, dc);
    }
    return CreateFontA(height, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
}

static void apply_font(HWND control, HFONT font)
{
    if (control != NULL && font != NULL) {
        SendMessageA(control, WM_SETFONT, (WPARAM)font, TRUE);
    }
}

/**
 * Load resource 1 supplied by the product face. The generic application icon
 * is a last-resort fallback only; release CI requires product artwork.
 */
static HICON load_product_icon(HINSTANCE instance)
{
    HICON icon = (HICON)LoadImageA(instance, MAKEINTRESOURCEA(1), IMAGE_ICON,
                                   64, 64, LR_DEFAULTCOLOR);

    if (icon != NULL) {
        g_product_icon_owned = TRUE;
        return icon;
    }
    g_product_icon_owned = FALSE;
    return LoadIconA(NULL, IDI_APPLICATION);
}

static HMENU create_main_menu(void)
{
    HMENU menu = CreateMenu();
    HMENU file_menu = CreatePopupMenu();
    HMENU help_menu = CreatePopupMenu();

    if (menu == NULL || file_menu == NULL || help_menu == NULL) {
        if (file_menu != NULL) {
            DestroyMenu(file_menu);
        }
        if (help_menu != NULL) {
            DestroyMenu(help_menu);
        }
        if (menu != NULL) {
            DestroyMenu(menu);
        }
        return NULL;
    }

    AppendMenuA(file_menu, MF_STRING, IDM_FILE_EXPORT,
                "&Export evidence...\tCtrl+E");
    AppendMenuA(file_menu, MF_SEPARATOR, 0U, NULL);
    AppendMenuA(file_menu, MF_STRING, IDM_FILE_EXIT, "E&xit");
    AppendMenuA(help_menu, MF_STRING, IDM_HELP_ABOUT,
                "&About " LINK_PRODUCT_NAME "...");
    AppendMenuA(menu, MF_POPUP, (UINT_PTR)file_menu, "&File");
    AppendMenuA(menu, MF_POPUP, (UINT_PTR)help_menu, "&Help");
    return menu;
}

static HRESULT CALLBACK about_callback(HWND window, UINT notification,
                                       WPARAM wparam, LPARAM lparam,
                                       LONG_PTR reference_data)
{
    (void)wparam;
    (void)reference_data;

    if (notification == TDN_HYPERLINK_CLICK && lparam != 0) {
        (void)ShellExecuteW(window, L"open", (LPCWSTR)lparam,
                            NULL, NULL, SW_SHOWNORMAL);
    }
    return S_OK;
}

/**
 * Present the standard LINK-family About experience using Task Dialogs.
 * TaskDialogIndirect gives the application a current Windows 10/11 visual
 * treatment while retaining the product icon and platform-native behaviour.
 */
static void show_about(void)
{
    char title_utf8[160];
    char content_utf8[1024];
    wchar_t title[160];
    wchar_t product_name[160];
    wchar_t content[1024];
    TASKDIALOGCONFIG config;
    HRESULT result;

    (void)snprintf(title_utf8, sizeof(title_utf8),
                   "About %s Discover", LINK_PRODUCT_NAME);
    (void)snprintf(content_utf8, sizeof(content_utf8),
                   "Version %s\n%s\n\n"
                   "OpenPort 2.0 / SAE J2534 read-only discovery and evidence capture.\n"
                   "Unsafe and unknown diagnostic services are denied before transmission.\n\n"
                   "<a href=\"%s\">Project website</a>\n\n"
                   "%s\nGPL-3.0-or-later",
                   LINK_PRODUCT_VERSION,
                   LINK_PRODUCT_SUBTITLE,
                   LINK_PRODUCT_WEBSITE,
                   LINK_PRODUCT_COPYRIGHT);
    title_utf8[sizeof(title_utf8) - 1U] = '\0';
    content_utf8[sizeof(content_utf8) - 1U] = '\0';

    if (!utf8_to_wide(title_utf8, title,
                      sizeof(title) / sizeof(title[0])) ||
        !utf8_to_wide(LINK_PRODUCT_NAME, product_name,
                      sizeof(product_name) / sizeof(product_name[0])) ||
        !utf8_to_wide(content_utf8, content,
                      sizeof(content) / sizeof(content[0]))) {
        MessageBoxA(g_app.window, "Unable to prepare About information.",
                    LINK_PRODUCT_NAME " Discover", MB_OK | MB_ICONERROR);
        return;
    }

    memset(&config, 0, sizeof(config));
    config.cbSize = sizeof(config);
    config.hwndParent = g_app.window;
    config.dwFlags = TDF_USE_HICON_MAIN |
                     TDF_ENABLE_HYPERLINKS |
                     TDF_POSITION_RELATIVE_TO_WINDOW |
                     TDF_SIZE_TO_CONTENT;
    config.dwCommonButtons = TDCBF_CLOSE_BUTTON;
    config.pszWindowTitle = title;
    config.pszMainInstruction = product_name;
    config.pszContent = content;
    config.hMainIcon = g_product_icon;
    config.pfCallback = about_callback;

    result = TaskDialogIndirect(&config, NULL, NULL, NULL);
    if (FAILED(result)) {
        /* Task Dialogs require Common Controls v6; keep a safe fallback. */
        (void)MessageBoxW(g_app.window, content, title,
                          MB_OK | MB_ICONINFORMATION);
    }
}

static void browse_for_j2534_dll(void)
{
    OPENFILENAMEA dialog;
    char path[MAX_PATH];

    GetWindowTextA(g_app.dll_edit, path, (int)sizeof(path));
    path[sizeof(path) - 1U] = '\0';
    memset(&dialog, 0, sizeof(dialog));
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = g_app.window;
    dialog.lpstrFilter =
        "J2534 DLL (*.dll)\0*.dll\0All files (*.*)\0*.*\0\0";
    dialog.lpstrFile = path;
    dialog.nMaxFile = (DWORD)sizeof(path);
    dialog.lpstrDefExt = "dll";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameA(&dialog)) {
        SetWindowTextA(g_app.dll_edit, path);
    }
}

/**
 * Reflow the workspace for the current DPI and client size. Fixed minimum
 * dimensions protect the log and annotation editor from collapsing.
 */
static void layout_controls(HWND window)
{
    RECT client;
    int dpi;
    int width;
    int height;
    int margin;
    int content_width;
    int log_top;
    int note_y;
    int log_height;

    GetClientRect(window, &client);
    dpi = window_dpi(window);
    width = client.right - client.left;
    height = client.bottom - client.top;
    margin = MulDiv(20, dpi, 96);
    content_width = width - (margin * 2);
    log_top = MulDiv(300, dpi, 96);
    note_y = height - MulDiv(66, dpi, 96);
    log_height = note_y - log_top - MulDiv(36, dpi, 96);
    if (log_height < MulDiv(120, dpi, 96)) {
        log_height = MulDiv(120, dpi, 96);
    }

#define PX(value) MulDiv((value), dpi, 96)
    MoveWindow(g_brand_icon, margin, PX(18), PX(64), PX(64), TRUE);
    MoveWindow(g_brand_title, margin + PX(80), PX(14),
               content_width - PX(80), PX(38), TRUE);
    MoveWindow(g_brand_subtitle, margin + PX(82), PX(52),
               content_width - PX(82), PX(24), TRUE);
    MoveWindow(g_header_rule, margin, PX(88), content_width, PX(2), TRUE);

    MoveWindow(g_connection_group, margin, PX(104), content_width, PX(142), TRUE);
    MoveWindow(g_dll_label, margin + PX(16), PX(127), PX(230), PX(20), TRUE);
    MoveWindow(g_app.dll_edit, margin + PX(16), PX(151),
               content_width - PX(126), PX(28), TRUE);
    MoveWindow(g_browse_button, width - margin - PX(96), PX(150),
               PX(80), PX(30), TRUE);

    MoveWindow(g_connect_button, margin + PX(16), PX(193), PX(184), PX(34), TRUE);
    MoveWindow(g_inventory_button, margin + PX(210), PX(193), PX(204), PX(34), TRUE);
    MoveWindow(g_stop_button, margin + PX(424), PX(193), PX(86), PX(34), TRUE);
    MoveWindow(g_export_button, margin + PX(520), PX(193), PX(146), PX(34), TRUE);

    MoveWindow(g_app.status, margin, PX(258), content_width, PX(30), TRUE);
    MoveWindow(g_log_label, margin, PX(284), PX(190), PX(20), TRUE);
    MoveWindow(g_app.log, margin, log_top, content_width, log_height, TRUE);
    MoveWindow(g_note_label, margin, note_y - PX(22), PX(190), PX(20), TRUE);
    MoveWindow(g_app.note, margin, note_y, content_width - PX(154), PX(30), TRUE);
    MoveWindow(g_add_note_button, width - margin - PX(144), note_y,
               PX(144), PX(30), TRUE);
#undef PX
}

/** Construct the shared native Windows product face around the LINK backend. */
static void create_controls(HWND window)
{
    char detected[MAX_PATH] = "";
    char subtitle[256];
    HINSTANCE instance = (HINSTANCE)GetWindowLongPtrA(window, GWLP_HINSTANCE);

    g_ui_font = make_font(window, 9, FW_NORMAL);
    g_title_font = make_font(window, 22, FW_SEMIBOLD);
    g_subtitle_font = make_font(window, 10, FW_NORMAL);
    g_status_font = make_font(window, 9, FW_SEMIBOLD);

    g_brand_icon = CreateWindowA("STATIC", "",
                                 WS_CHILD | WS_VISIBLE | SS_ICON,
                                 0, 0, 0, 0, window, NULL, instance, NULL);
    if (g_product_icon != NULL) {
        SendMessageA(g_brand_icon, STM_SETICON, (WPARAM)g_product_icon, 0U);
    }

    g_brand_title = CreateWindowA("STATIC", LINK_PRODUCT_NAME " Discover",
                                   WS_CHILD | WS_VISIBLE | SS_LEFT,
                                   0, 0, 0, 0, window, NULL, instance, NULL);
    (void)snprintf(subtitle, sizeof(subtitle), "%s | Version %s",
                   LINK_PRODUCT_SUBTITLE, LINK_PRODUCT_VERSION);
    subtitle[sizeof(subtitle) - 1U] = '\0';
    g_brand_subtitle = CreateWindowA("STATIC", subtitle,
                                      WS_CHILD | WS_VISIBLE | SS_LEFT,
                                      0, 0, 0, 0, window, NULL, instance, NULL);
    g_header_rule = CreateWindowA("STATIC", "",
                                   WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ,
                                   0, 0, 0, 0, window, NULL, instance, NULL);
    apply_font(g_brand_title, g_title_font);
    apply_font(g_brand_subtitle, g_subtitle_font);

    g_connection_group = CreateWindowA("BUTTON", "Vehicle interface",
                                        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                                        0, 0, 0, 0, window, NULL, instance, NULL);
    g_dll_label = CreateWindowA("STATIC", "J2534 FunctionLibrary DLL",
                                WS_CHILD | WS_VISIBLE | SS_LEFT,
                                0, 0, 0, 0, window, NULL, instance, NULL);
    g_app.dll_edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                                     WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                         ES_AUTOHSCROLL,
                                     0, 0, 0, 0, window,
                                     (HMENU)(INT_PTR)IDC_DLL, instance, NULL);
    g_browse_button = CreateWindowA("BUTTON", "Browse...",
                                     WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                         BS_PUSHBUTTON,
                                     0, 0, 0, 0, window,
                                     (HMENU)(INT_PTR)IDC_BROWSE, instance, NULL);

    if (read_registry_openport(detected, sizeof(detected))) {
        SetWindowTextA(g_app.dll_edit, detected);
    } else {
        SetWindowTextA(g_app.dll_edit,
                       "C:\\Program Files (x86)\\OpenECU\\OpenPort 2.0\\op20pt32.dll");
    }

    g_connect_button = CreateWindowA("BUTTON", "Connect passive 500 kbit/s",
                                      WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                          BS_DEFPUSHBUTTON,
                                      0, 0, 0, 0, window,
                                      (HMENU)(INT_PTR)IDC_CONNECT, instance, NULL);
    g_inventory_button = CreateWindowA("BUTTON", "Read-only OBD inventory",
                                        WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                            BS_PUSHBUTTON,
                                        0, 0, 0, 0, window,
                                        (HMENU)(INT_PTR)IDC_INVENTORY, instance, NULL);
    g_stop_button = CreateWindowA("BUTTON", "Stop",
                                   WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                       BS_PUSHBUTTON,
                                   0, 0, 0, 0, window,
                                   (HMENU)(INT_PTR)IDC_STOP, instance, NULL);
    g_export_button = CreateWindowA("BUTTON", "Export evidence...",
                                     WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                         BS_PUSHBUTTON,
                                     0, 0, 0, 0, window,
                                     (HMENU)(INT_PTR)IDC_EXPORT, instance, NULL);

    g_app.status = CreateWindowExA(WS_EX_CLIENTEDGE, "STATIC",
                                    "DISCONNECTED - deny-by-default safety policy active",
                                    WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
                                    0, 0, 0, 0, window,
                                    (HMENU)(INT_PTR)IDC_STATUS, instance, NULL);
    g_log_label = CreateWindowA("STATIC", "Diagnostic session log",
                                WS_CHILD | WS_VISIBLE | SS_LEFT,
                                0, 0, 0, 0, window, NULL, instance, NULL);
    g_app.log = CreateWindowExA(WS_EX_CLIENTEDGE, "LISTBOX", NULL,
                                WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
                                    LBS_NOINTEGRALHEIGHT,
                                0, 0, 0, 0, window,
                                (HMENU)(INT_PTR)IDC_LOG, instance, NULL);
    g_note_label = CreateWindowA("STATIC", "Evidence annotation",
                                 WS_CHILD | WS_VISIBLE | SS_LEFT,
                                 0, 0, 0, 0, window, NULL, instance, NULL);
    g_app.note = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                                 WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                     ES_AUTOHSCROLL,
                                 0, 0, 0, 0, window,
                                 (HMENU)(INT_PTR)IDC_NOTE, instance, NULL);
    g_add_note_button = CreateWindowA("BUTTON", "Add annotation",
                                      WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                          BS_PUSHBUTTON,
                                      0, 0, 0, 0, window,
                                      (HMENU)(INT_PTR)IDC_ADDNOTE, instance, NULL);

    apply_font(g_connection_group, g_ui_font);
    apply_font(g_dll_label, g_ui_font);
    apply_font(g_app.dll_edit, g_ui_font);
    apply_font(g_browse_button, g_ui_font);
    apply_font(g_connect_button, g_ui_font);
    apply_font(g_inventory_button, g_ui_font);
    apply_font(g_stop_button, g_ui_font);
    apply_font(g_export_button, g_ui_font);
    apply_font(g_app.status, g_status_font);
    apply_font(g_log_label, g_ui_font);
    apply_font(g_app.log, g_ui_font);
    apply_font(g_note_label, g_ui_font);
    apply_font(g_app.note, g_ui_font);
    apply_font(g_add_note_button, g_ui_font);

    layout_controls(window);
}

static void destroy_ui_resources(void)
{
    if (g_title_font != NULL) {
        DeleteObject(g_title_font);
    }
    if (g_subtitle_font != NULL) {
        DeleteObject(g_subtitle_font);
    }
    if (g_status_font != NULL) {
        DeleteObject(g_status_font);
    }
    if (g_ui_font != NULL) {
        DeleteObject(g_ui_font);
    }
    g_title_font = NULL;
    g_subtitle_font = NULL;
    g_status_font = NULL;
    g_ui_font = NULL;

    if (g_product_icon_owned && g_product_icon != NULL) {
        DestroyIcon(g_product_icon);
    }
    g_product_icon = NULL;
}

static LRESULT CALLBACK window_proc(HWND window, UINT message,
                                    WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_CREATE:
        g_app.window = window;
        create_controls(window);
        return 0;
    case WM_SIZE:
        if (wparam != SIZE_MINIMIZED) {
            layout_controls(window);
        }
        return 0;
    case WM_GETMINMAXINFO: {
        MINMAXINFO *limits = (MINMAXINFO *)lparam;
        limits->ptMinTrackSize.x = scale_px(window, 800);
        limits->ptMinTrackSize.y = scale_px(window, 610);
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wparam)) {
        case IDM_FILE_EXPORT:
            export_evidence();
            return 0;
        case IDM_FILE_EXIT:
            SendMessageA(window, WM_CLOSE, 0U, 0U);
            return 0;
        case IDM_HELP_ABOUT:
            show_about();
            return 0;
        case IDC_BROWSE:
            browse_for_j2534_dll();
            return 0;
        default:
            break;
        }
        break;
    case WM_DESTROY:
        destroy_ui_resources();
        break;
    default:
        break;
    }

    return link_discover_backend_window_proc(window, message, wparam, lparam);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous,
                   LPSTR command_line, int show)
{
    INITCOMMONCONTROLSEX controls;
    WNDCLASSEXA cls;
    HWND window;
    MSG message;
    HMENU menu;

    (void)previous;
    (void)command_line;

    (void)SetProcessDPIAware();
    memset(&controls, 0, sizeof(controls));
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES;
    if (!InitCommonControlsEx(&controls)) {
        return 1;
    }

    memset(&g_app, 0, sizeof(g_app));
    InitializeCriticalSection(&g_app.evidence_lock);
    g_product_icon = load_product_icon(instance);

    memset(&cls, 0, sizeof(cls));
    cls.cbSize = sizeof(cls);
    cls.style = CS_HREDRAW | CS_VREDRAW;
    cls.lpfnWndProc = window_proc;
    cls.hInstance = instance;
    cls.hCursor = LoadCursor(NULL, IDC_ARROW);
    cls.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    cls.hIcon = g_product_icon;
    cls.hIconSm = g_product_icon;
    cls.lpszClassName = LINK_PRODUCT_WINDOW_CLASS;
    if (RegisterClassExA(&cls) == 0U) {
        destroy_ui_resources();
        DeleteCriticalSection(&g_app.evidence_lock);
        return 1;
    }

    menu = create_main_menu();
    window = CreateWindowExA(
        0U, cls.lpszClassName,
        LINK_PRODUCT_NAME " Discover - OpenPort 2.0 / J2534",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 980, 680,
        NULL, menu, instance, NULL);
    if (window == NULL) {
        if (menu != NULL) {
            DestroyMenu(menu);
        }
        destroy_ui_resources();
        DeleteCriticalSection(&g_app.evidence_lock);
        return 1;
    }

    SendMessageA(window, WM_SETICON, ICON_BIG, (LPARAM)g_product_icon);
    SendMessageA(window, WM_SETICON, ICON_SMALL, (LPARAM)g_product_icon);
    ShowWindow(window, show);
    UpdateWindow(window);

    while (GetMessageA(&message, NULL, 0U, 0U) > 0) {
        if ((GetKeyState(VK_CONTROL) & 0x8000) != 0 &&
            message.message == WM_KEYDOWN && message.wParam == 'E') {
            SendMessageA(window, WM_COMMAND, IDM_FILE_EXPORT, 0U);
            continue;
        }
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }
    return (int)message.wParam;
}
