/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file link-discover-ui.c
 * @brief Native Win32 presentation shell for LINK-family Discover products.
 *
 * The J2534 transport, safety policy, evidence writer and bounded inventory
 * remain in link-discover.c.  This translation unit deliberately wraps that
 * proven backend rather than copying it: MBLINK, JAGLINK and future product
 * faces therefore share one Windows interaction model while supplying only
 * identity, version, copyright and icon resources.
 */
#define WinMain link_discover_backend_WinMain
#define window_proc link_discover_backend_window_proc
#define create_controls link_discover_backend_create_controls
#include "link-discover.c"
#undef create_controls
#undef window_proc
#undef WinMain

#include <shellapi.h>

#ifndef LINK_PRODUCT_VERSION
#define LINK_PRODUCT_VERSION "unknown"
#endif
#ifndef LINK_PRODUCT_COPYRIGHT
#define LINK_PRODUCT_COPYRIGHT "Copyright (C) 2026 The First Infiltrator"
#endif
#ifndef LINK_PRODUCT_SUBTITLE
#define LINK_PRODUCT_SUBTITLE "Vehicle Diagnostics"
#endif

#define IDM_FILE_EXPORT 41001
#define IDM_FILE_EXIT 41002
#define IDM_HELP_ABOUT 42001
#define IDC_BROWSE 1010

static HWND g_brand_icon;
static HWND g_brand_title;
static HWND g_brand_subtitle;
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
static HICON g_product_icon;
static BOOL g_product_icon_owned;

/** Create a Segoe UI font using logical pixels so the shell scales with DPI. */
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
 * Load product resource 1 when supplied by the face.  A standard application
 * icon is only a defensive fallback for the generic LINK reference target;
 * released product faces are expected to provide their own resource.
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
        if (file_menu != NULL) DestroyMenu(file_menu);
        if (help_menu != NULL) DestroyMenu(help_menu);
        if (menu != NULL) DestroyMenu(menu);
        return NULL;
    }

    AppendMenuA(file_menu, MF_STRING, IDM_FILE_EXPORT, "&Export evidence...\tCtrl+E");
    AppendMenuA(file_menu, MF_SEPARATOR, 0U, NULL);
    AppendMenuA(file_menu, MF_STRING, IDM_FILE_EXIT, "E&xit");
    AppendMenuA(help_menu, MF_STRING, IDM_HELP_ABOUT, "&About " LINK_PRODUCT_NAME "...");
    AppendMenuA(menu, MF_POPUP, (UINT_PTR)file_menu, "&File");
    AppendMenuA(menu, MF_POPUP, (UINT_PTR)help_menu, "&Help");
    return menu;
}

static void show_about(void)
{
    char details[768];

    (void)snprintf(details, sizeof(details),
                   "Version %s\r\n%s\r\n\r\n"
                   "OpenPort 2.0 / SAE J2534 read-only discovery and evidence capture.\r\n"
                   "Unsafe and unknown diagnostic services are denied before transmission.\r\n\r\n"
                   "%s\r\nGPL-3.0-or-later",
                   LINK_PRODUCT_VERSION, LINK_PRODUCT_SUBTITLE,
                   LINK_PRODUCT_COPYRIGHT);
    details[sizeof(details) - 1U] = '\0';
    (void)ShellAboutA(g_app.window, LINK_PRODUCT_NAME " Discover", details,
                      g_product_icon);
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
    dialog.lpstrFilter = "J2534 DLL (*.dll)\0*.dll\0All files (*.*)\0*.*\0\0";
    dialog.lpstrFile = path;
    dialog.nMaxFile = (DWORD)sizeof(path);
    dialog.lpstrDefExt = "dll";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameA(&dialog)) {
        SetWindowTextA(g_app.dll_edit, path);
    }
}

/**
 * Keep the useful controls fluid when the window grows.  The minimum client
 * area enforced by WM_GETMINMAXINFO protects the diagnostic log and button
 * labels from becoming unusably small.
 */
static void layout_controls(HWND window)
{
    RECT client;
    int width;
    int height;
    int margin = 18;
    int content_width;
    int log_top = 292;
    int note_y;
    int log_height;

    GetClientRect(window, &client);
    width = client.right - client.left;
    height = client.bottom - client.top;
    content_width = width - (margin * 2);
    note_y = height - 62;
    log_height = note_y - log_top - 34;
    if (log_height < 120) log_height = 120;

    MoveWindow(g_brand_icon, margin, 16, 56, 56, TRUE);
    MoveWindow(g_brand_title, margin + 70, 13, content_width - 70, 34, TRUE);
    MoveWindow(g_brand_subtitle, margin + 72, 48, content_width - 72, 24, TRUE);

    MoveWindow(g_connection_group, margin, 88, content_width, 144, TRUE);
    MoveWindow(g_dll_label, margin + 16, 110, 230, 20, TRUE);
    MoveWindow(g_app.dll_edit, margin + 16, 134, content_width - 126, 26, TRUE);
    MoveWindow(g_browse_button, width - margin - 100, 133, 84, 28, TRUE);

    MoveWindow(g_connect_button, margin + 16, 174, 176, 32, TRUE);
    MoveWindow(g_inventory_button, margin + 202, 174, 198, 32, TRUE);
    MoveWindow(g_stop_button, margin + 410, 174, 86, 32, TRUE);
    MoveWindow(g_export_button, margin + 506, 174, 142, 32, TRUE);

    MoveWindow(g_app.status, margin, 244, content_width, 28, TRUE);
    MoveWindow(g_log_label, margin, 274, 160, 20, TRUE);
    MoveWindow(g_app.log, margin, log_top, content_width, log_height, TRUE);
    MoveWindow(g_note_label, margin, note_y - 22, 160, 20, TRUE);
    MoveWindow(g_app.note, margin, note_y, content_width - 150, 28, TRUE);
    MoveWindow(g_add_note_button, width - margin - 140, note_y, 140, 28, TRUE);
}

/** Construct the shared, native Windows product shell around the LINK backend. */
static void create_controls(HWND window)
{
    char detected[MAX_PATH] = "";
    HINSTANCE instance = (HINSTANCE)GetWindowLongPtrA(window, GWLP_HINSTANCE);

    g_ui_font = make_font(window, 9, FW_NORMAL);
    g_title_font = make_font(window, 20, FW_SEMIBOLD);
    g_subtitle_font = make_font(window, 10, FW_NORMAL);

    g_brand_icon = CreateWindowA("STATIC", "", WS_CHILD | WS_VISIBLE | SS_ICON,
                                 0, 0, 0, 0, window, NULL, instance, NULL);
    if (g_product_icon != NULL) {
        SendMessageA(g_brand_icon, STM_SETICON, (WPARAM)g_product_icon, 0U);
    }
    g_brand_title = CreateWindowA("STATIC", LINK_PRODUCT_NAME,
                                   WS_CHILD | WS_VISIBLE | SS_LEFT,
                                   0, 0, 0, 0, window, NULL, instance, NULL);
    g_brand_subtitle = CreateWindowA("STATIC",
                                      LINK_PRODUCT_SUBTITLE "  ·  Discover " LINK_PRODUCT_VERSION,
                                      WS_CHILD | WS_VISIBLE | SS_LEFT,
                                      0, 0, 0, 0, window, NULL, instance, NULL);
    apply_font(g_brand_title, g_title_font);
    apply_font(g_brand_subtitle, g_subtitle_font);

    g_connection_group = CreateWindowA("BUTTON", "OpenPort / J2534 connection",
                                        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                                        0, 0, 0, 0, window, NULL, instance, NULL);
    g_dll_label = CreateWindowA("STATIC", "J2534 FunctionLibrary DLL",
                                WS_CHILD | WS_VISIBLE | SS_LEFT,
                                0, 0, 0, 0, window, NULL, instance, NULL);
    g_app.dll_edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                                     WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                     0, 0, 0, 0, window,
                                     (HMENU)(INT_PTR)IDC_DLL, instance, NULL);
    g_browse_button = CreateWindowA("BUTTON", "Browse...",
                                     WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                     0, 0, 0, 0, window,
                                     (HMENU)(INT_PTR)IDC_BROWSE, instance, NULL);

    if (read_registry_openport(detected, sizeof(detected))) {
        SetWindowTextA(g_app.dll_edit, detected);
    } else {
        SetWindowTextA(g_app.dll_edit,
                       "C:\\Program Files (x86)\\OpenECU\\OpenPort 2.0\\op20pt32.dll");
    }

    g_connect_button = CreateWindowA("BUTTON", "Connect passive 500 kbit/s",
                                      WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                                      0, 0, 0, 0, window,
                                      (HMENU)(INT_PTR)IDC_CONNECT, instance, NULL);
    g_inventory_button = CreateWindowA("BUTTON", "Read-only OBD inventory",
                                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                        0, 0, 0, 0, window,
                                        (HMENU)(INT_PTR)IDC_INVENTORY, instance, NULL);
    g_stop_button = CreateWindowA("BUTTON", "Stop",
                                   WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                   0, 0, 0, 0, window,
                                   (HMENU)(INT_PTR)IDC_STOP, instance, NULL);
    g_export_button = CreateWindowA("BUTTON", "Export evidence...",
                                     WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                     0, 0, 0, 0, window,
                                     (HMENU)(INT_PTR)IDC_EXPORT, instance, NULL);

    g_app.status = CreateWindowExA(WS_EX_CLIENTEDGE, "STATIC",
                                    "DISCONNECTED — deny-by-default safety policy active",
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
                                 WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                 0, 0, 0, 0, window,
                                 (HMENU)(INT_PTR)IDC_NOTE, instance, NULL);
    g_add_note_button = CreateWindowA("BUTTON", "Add annotation",
                                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
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
    apply_font(g_app.status, g_ui_font);
    apply_font(g_log_label, g_ui_font);
    apply_font(g_app.log, g_ui_font);
    apply_font(g_note_label, g_ui_font);
    apply_font(g_app.note, g_ui_font);
    apply_font(g_add_note_button, g_ui_font);

    layout_controls(window);
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
        layout_controls(window);
        return 0;
    case WM_GETMINMAXINFO: {
        MINMAXINFO *limits = (MINMAXINFO *)lparam;
        limits->ptMinTrackSize.x = 760;
        limits->ptMinTrackSize.y = 560;
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
        if (g_title_font != NULL) DeleteObject(g_title_font);
        if (g_subtitle_font != NULL) DeleteObject(g_subtitle_font);
        if (g_ui_font != NULL) DeleteObject(g_ui_font);
        g_title_font = NULL;
        g_subtitle_font = NULL;
        g_ui_font = NULL;
        if (g_product_icon_owned && g_product_icon != NULL) {
            DestroyIcon(g_product_icon);
        }
        g_product_icon = NULL;
        break;
    default:
        break;
    }

    return link_discover_backend_window_proc(window, message, wparam, lparam);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous,
                   LPSTR command_line, int show)
{
    WNDCLASSEXA cls;
    HWND window;
    MSG message;
    HMENU menu;

    (void)previous;
    (void)command_line;
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
        DeleteCriticalSection(&g_app.evidence_lock);
        return 1;
    }

    menu = create_main_menu();
    window = CreateWindowExA(
        0U, cls.lpszClassName,
        LINK_PRODUCT_NAME " Discover — OpenPort 2.0 / J2534",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 960, 650,
        NULL, menu, instance, NULL);
    if (window == NULL) {
        if (menu != NULL) DestroyMenu(menu);
        DeleteCriticalSection(&g_app.evidence_lock);
        return 1;
    }

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
