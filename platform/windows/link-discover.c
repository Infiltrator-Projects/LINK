/* SPDX-License-Identifier: GPL-3.0-or-later */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "link/discover.h"

#ifndef LINK_PRODUCT_NAME
#define LINK_PRODUCT_NAME "LINK"
#endif
#ifndef LINK_PRODUCT_SLUG
#define LINK_PRODUCT_SLUG "link"
#endif
#ifndef LINK_PRODUCT_WINDOW_CLASS
#define LINK_PRODUCT_WINDOW_CLASS "LINKDiscoverWindow"
#endif

#define J2534_STATUS_NOERROR 0UL
#define J2534_ERR_TIMEOUT 0x09UL
#define J2534_CAN 5UL
#define J2534_ISO15765 6UL
#define J2534_FLOW_CONTROL_FILTER 3UL
#define J2534_ISO15765_FRAME_PAD 0x00000040UL
#define J2534_MAX_DATA 4128U

#define IDC_DLL 1001
#define IDC_CONNECT 1002
#define IDC_INVENTORY 1003
#define IDC_STOP 1004
#define IDC_EXPORT 1005
#define IDC_NOTE 1006
#define IDC_ADDNOTE 1007
#define IDC_LOG 1008
#define IDC_STATUS 1009
#define WM_LINK_LOG (WM_APP + 1)

typedef struct PASSTHRU_MSG_ {
    unsigned long ProtocolID;
    unsigned long RxStatus;
    unsigned long TxFlags;
    unsigned long Timestamp;
    unsigned long DataSize;
    unsigned long ExtraDataIndex;
    unsigned char Data[J2534_MAX_DATA];
} PASSTHRU_MSG;

typedef unsigned long (WINAPI *PassThruOpenFn)(void *, unsigned long *);
typedef unsigned long (WINAPI *PassThruCloseFn)(unsigned long);
typedef unsigned long (WINAPI *PassThruConnectFn)(unsigned long, unsigned long, unsigned long, unsigned long, unsigned long *);
typedef unsigned long (WINAPI *PassThruDisconnectFn)(unsigned long);
typedef unsigned long (WINAPI *PassThruReadMsgsFn)(unsigned long, PASSTHRU_MSG *, unsigned long *, unsigned long);
typedef unsigned long (WINAPI *PassThruWriteMsgsFn)(unsigned long, PASSTHRU_MSG *, unsigned long *, unsigned long);
typedef unsigned long (WINAPI *PassThruStartMsgFilterFn)(unsigned long, unsigned long, PASSTHRU_MSG *, PASSTHRU_MSG *, PASSTHRU_MSG *, unsigned long *);

typedef struct j2534_api_ {
    HMODULE dll;
    PassThruOpenFn open;
    PassThruCloseFn close;
    PassThruConnectFn connect;
    PassThruDisconnectFn disconnect;
    PassThruReadMsgsFn read_msgs;
    PassThruWriteMsgsFn write_msgs;
    PassThruStartMsgFilterFn start_filter;
} j2534_api;

typedef struct app_state_ {
    HWND window;
    HWND dll_edit;
    HWND status;
    HWND log;
    HWND note;
    j2534_api api;
    unsigned long device_id;
    unsigned long channel_id;
    HANDLE reader_thread;
    HANDLE stop_event;
    CRITICAL_SECTION evidence_lock;
    link_evidence_writer *evidence;
    char evidence_path[MAX_PATH];
    int connected;
} app_state;

static app_state g_app;

static uint64_t unix_time_ns(void)
{
    FILETIME ft;
    ULARGE_INTEGER ticks;
    const uint64_t epoch_delta = 116444736000000000ULL;
    GetSystemTimePreciseAsFileTime(&ft);
    ticks.LowPart = ft.dwLowDateTime;
    ticks.HighPart = ft.dwHighDateTime;
    return (ticks.QuadPart - epoch_delta) * 100ULL;
}

static void set_status(const char *text)
{
    SetWindowTextA(g_app.status, text != NULL ? text : "");
}

static void post_logf(const char *format, ...)
{
    va_list args;
    char stack[512];
    char *copy;
    int count;
    va_start(args, format);
    count = vsnprintf(stack, sizeof(stack), format, args);
    va_end(args);
    if (count < 0) return;
    stack[sizeof(stack) - 1U] = '\0';
    copy = _strdup(stack);
    if (copy == NULL) return;
    if (!PostMessageA(g_app.window, WM_LINK_LOG, 0U, (LPARAM)copy)) free(copy);
}

static uint32_t message_can_id(const PASSTHRU_MSG *msg)
{
    if (msg == NULL || msg->DataSize < 4UL) return 0U;
    return ((uint32_t)msg->Data[0] << 24U) |
           ((uint32_t)msg->Data[1] << 16U) |
           ((uint32_t)msg->Data[2] << 8U) |
           (uint32_t)msg->Data[3];
}

static void evidence_frame(const char *direction, const PASSTHRU_MSG *msg, const char *annotation)
{
    uint32_t id;
    const unsigned char *payload;
    size_t length;
    if (g_app.evidence == NULL || msg == NULL) return;
    id = message_can_id(msg);
    payload = msg->DataSize > 4UL ? msg->Data + 4U : msg->Data;
    length = msg->DataSize > 4UL ? (size_t)(msg->DataSize - 4UL) : (size_t)msg->DataSize;
    EnterCriticalSection(&g_app.evidence_lock);
    (void)link_evidence_write_frame(g_app.evidence, unix_time_ns(), direction,
                                    msg->ProtocolID == J2534_ISO15765 ? "ISO15765" : "CAN",
                                    id, payload, length, annotation);
    LeaveCriticalSection(&g_app.evidence_lock);
}

static int make_evidence_file(void)
{
    SYSTEMTIME st;
    char temp[MAX_PATH];
    DWORD n;
    if (g_app.evidence != NULL) return 1;
    n = GetTempPathA((DWORD)sizeof(temp), temp);
    if (n == 0U || n >= sizeof(temp)) return 0;
    GetLocalTime(&st);
    (void)snprintf(g_app.evidence_path, sizeof(g_app.evidence_path),
                   "%s" LINK_PRODUCT_NAME "-evidence-%04u%02u%02u-%02u%02u%02u.jsonl",
                   temp, (unsigned int)st.wYear, (unsigned int)st.wMonth,
                   (unsigned int)st.wDay, (unsigned int)st.wHour,
                   (unsigned int)st.wMinute, (unsigned int)st.wSecond);
    g_app.evidence_path[sizeof(g_app.evidence_path) - 1U] = '\0';
    g_app.evidence = link_evidence_open(g_app.evidence_path);
    return g_app.evidence != NULL;
}

static int read_registry_openport(char *path, size_t capacity)
{
    static const char *roots[] = {
        "SOFTWARE\\PassThruSupport.04.04",
        "SOFTWARE\\WOW6432Node\\PassThruSupport.04.04"
    };
    size_t root_index;
    for (root_index = 0U; root_index < sizeof(roots) / sizeof(roots[0]); ++root_index) {
        HKEY root;
        LONG rc = RegOpenKeyExA(HKEY_LOCAL_MACHINE, roots[root_index], 0U,
                                KEY_READ | KEY_WOW64_32KEY, &root);
        DWORD index = 0U;
        if (rc != ERROR_SUCCESS) continue;
        for (;;) {
            char subname[256];
            DWORD subname_len = (DWORD)sizeof(subname);
            HKEY sub;
            char name[256];
            DWORD name_len = (DWORD)sizeof(name);
            DWORD type = 0U;
            char library[MAX_PATH];
            DWORD library_len = (DWORD)sizeof(library);
            rc = RegEnumKeyExA(root, index++, subname, &subname_len, NULL, NULL, NULL, NULL);
            if (rc == ERROR_NO_MORE_ITEMS) break;
            if (rc != ERROR_SUCCESS) continue;
            if (RegOpenKeyExA(root, subname, 0U, KEY_READ | KEY_WOW64_32KEY, &sub) != ERROR_SUCCESS) continue;
            name[0] = '\0';
            library[0] = '\0';
            if (RegQueryValueExA(sub, "Name", NULL, &type, (LPBYTE)name, &name_len) == ERROR_SUCCESS &&
                (strstr(name, "OpenPort") != NULL || strstr(name, "openport") != NULL) &&
                RegQueryValueExA(sub, "FunctionLibrary", NULL, &type, (LPBYTE)library, &library_len) == ERROR_SUCCESS &&
                library[0] != '\0') {
                RegCloseKey(sub);
                RegCloseKey(root);
                if (strlen(library) + 1U > capacity) return 0;
                (void)strcpy_s(path, capacity, library);
                return 1;
            }
            RegCloseKey(sub);
        }
        RegCloseKey(root);
    }
    return 0;
}

static FARPROC load_symbol(HMODULE dll, const char *name)
{
    FARPROC symbol = GetProcAddress(dll, name);
    if (symbol == NULL) post_logf("Missing J2534 entry point: %s", name);
    return symbol;
}

static int load_j2534(const char *path)
{
    FARPROC p;
    memset(&g_app.api, 0, sizeof(g_app.api));
    g_app.api.dll = LoadLibraryA(path);
    if (g_app.api.dll == NULL) {
        post_logf("Cannot load J2534 DLL: %s (Win32 error %lu)", path, (unsigned long)GetLastError());
        return 0;
    }
#define LOAD_FN(field, name) do { p = load_symbol(g_app.api.dll, name); if (p == NULL) goto fail; memcpy(&g_app.api.field, &p, sizeof(g_app.api.field)); } while (0)
    LOAD_FN(open, "PassThruOpen");
    LOAD_FN(close, "PassThruClose");
    LOAD_FN(connect, "PassThruConnect");
    LOAD_FN(disconnect, "PassThruDisconnect");
    LOAD_FN(read_msgs, "PassThruReadMsgs");
    LOAD_FN(write_msgs, "PassThruWriteMsgs");
    LOAD_FN(start_filter, "PassThruStartMsgFilter");
#undef LOAD_FN
    return 1;
fail:
    FreeLibrary(g_app.api.dll);
    memset(&g_app.api, 0, sizeof(g_app.api));
    return 0;
}

static void disconnect_channel(void)
{
    if (g_app.channel_id != 0UL && g_app.api.disconnect != NULL) {
        (void)g_app.api.disconnect(g_app.channel_id);
        g_app.channel_id = 0UL;
    }
}

static void stop_reader(void)
{
    if (g_app.stop_event != NULL) SetEvent(g_app.stop_event);
    if (g_app.reader_thread != NULL) {
        (void)WaitForSingleObject(g_app.reader_thread, 2000U);
        CloseHandle(g_app.reader_thread);
        g_app.reader_thread = NULL;
    }
    if (g_app.stop_event != NULL) {
        CloseHandle(g_app.stop_event);
        g_app.stop_event = NULL;
    }
}

static void close_device(void)
{
    stop_reader();
    disconnect_channel();
    if (g_app.device_id != 0UL && g_app.api.close != NULL) {
        (void)g_app.api.close(g_app.device_id);
        g_app.device_id = 0UL;
    }
    if (g_app.api.dll != NULL) FreeLibrary(g_app.api.dll);
    memset(&g_app.api, 0, sizeof(g_app.api));
    g_app.connected = 0;
}

static DWORD WINAPI passive_reader(LPVOID unused)
{
    (void)unused;
    while (WaitForSingleObject(g_app.stop_event, 0U) == WAIT_TIMEOUT) {
        PASSTHRU_MSG msgs[16];
        unsigned long count = 16UL;
        unsigned long rc;
        unsigned long i;
        memset(msgs, 0, sizeof(msgs));
        rc = g_app.api.read_msgs(g_app.channel_id, msgs, &count, 100UL);
        if (rc != J2534_STATUS_NOERROR && rc != J2534_ERR_TIMEOUT) {
            post_logf("PassThruReadMsgs returned 0x%08lX", rc);
            Sleep(100U);
            continue;
        }
        for (i = 0UL; i < count; ++i) {
            const uint32_t id = message_can_id(&msgs[i]);
            evidence_frame("rx", &msgs[i], "passive 500 kbit/s capture");
            post_logf("RX %03lX  %lu bytes", (unsigned long)id,
                      msgs[i].DataSize > 4UL ? msgs[i].DataSize - 4UL : msgs[i].DataSize);
        }
    }
    return 0U;
}

static int connect_passive(void)
{
    unsigned long rc;
    stop_reader();
    disconnect_channel();
    rc = g_app.api.connect(g_app.device_id, J2534_CAN, 0UL, 500000UL, &g_app.channel_id);
    if (rc != J2534_STATUS_NOERROR) {
        post_logf("PassThruConnect(CAN 500000) failed: 0x%08lX", rc);
        g_app.channel_id = 0UL;
        return 0;
    }
    g_app.stop_event = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (g_app.stop_event == NULL) return 0;
    g_app.reader_thread = CreateThread(NULL, 0U, passive_reader, NULL, 0U, NULL);
    if (g_app.reader_thread == NULL) {
        CloseHandle(g_app.stop_event);
        g_app.stop_event = NULL;
        return 0;
    }
    g_app.connected = 1;
    set_status("PASSIVE CAN 500 kbit/s — read only");
    post_logf("Passive capture connected at 500 kbit/s. No transmit path is used by capture mode.");
    return 1;
}

static int send_read_only_obd(const unsigned char *payload, size_t payload_length)
{
    PASSTHRU_MSG tx;
    link_safety_result safety;
    unsigned long count = 1UL;
    unsigned long rc;
    safety = link_safety_classify(payload, payload_length);
    if (safety.decision != LINK_SAFETY_ALLOW_READ_ONLY) {
        post_logf("BLOCKED service 0x%02X: %s", (unsigned int)safety.service,
                  link_safety_reason_string(safety.reason));
        return 0;
    }
    if (payload_length + 4U > sizeof(tx.Data)) return 0;
    memset(&tx, 0, sizeof(tx));
    tx.ProtocolID = J2534_ISO15765;
    tx.TxFlags = J2534_ISO15765_FRAME_PAD;
    tx.Data[0] = 0x00U; tx.Data[1] = 0x00U; tx.Data[2] = 0x07U; tx.Data[3] = 0xDFU;
    memcpy(tx.Data + 4U, payload, payload_length);
    tx.DataSize = (unsigned long)(payload_length + 4U);
    rc = g_app.api.write_msgs(g_app.channel_id, &tx, &count, 250UL);
    if (rc != J2534_STATUS_NOERROR || count != 1UL) {
        post_logf("Read-only OBD request failed: 0x%08lX", rc);
        return 0;
    }
    evidence_frame("tx", &tx, "bounded standard OBD inventory request");
    return 1;
}

static void make_iso_msg(PASSTHRU_MSG *msg, uint32_t id)
{
    memset(msg, 0, sizeof(*msg));
    msg->ProtocolID = J2534_ISO15765;
    msg->DataSize = 4UL;
    msg->Data[0] = (unsigned char)((id >> 24U) & 0xFFU);
    msg->Data[1] = (unsigned char)((id >> 16U) & 0xFFU);
    msg->Data[2] = (unsigned char)((id >> 8U) & 0xFFU);
    msg->Data[3] = (unsigned char)(id & 0xFFU);
}

static void install_obd_flow_filters(void)
{
    uint32_t i;
    for (i = 0U; i < 8U; ++i) {
        PASSTHRU_MSG mask;
        PASSTHRU_MSG pattern;
        PASSTHRU_MSG flow;
        unsigned long filter_id = 0UL;
        unsigned long rc;
        make_iso_msg(&mask, 0x7FFU);
        make_iso_msg(&pattern, 0x7E8U + i);
        make_iso_msg(&flow, 0x7E0U + i);
        rc = g_app.api.start_filter(g_app.channel_id, J2534_FLOW_CONTROL_FILTER,
                                    &mask, &pattern, &flow, &filter_id);
        if (rc != J2534_STATUS_NOERROR)
            post_logf("Flow-control filter %lu not installed (0x%08lX)", (unsigned long)i, rc);
    }
}

static void run_inventory(void)
{
    static const unsigned char queries[][2] = {
        {0x01U, 0x00U}, {0x09U, 0x00U}, {0x09U, 0x02U},
        {0x09U, 0x04U}, {0x09U, 0x06U}, {0x09U, 0x0AU}
    };
    size_t q;
    unsigned long rc;
    if (g_app.device_id == 0UL) {
        MessageBoxA(g_app.window, "Connect to the OpenPort/J2534 device first.", LINK_PRODUCT_NAME " Discover", MB_OK | MB_ICONINFORMATION);
        return;
    }
    stop_reader();
    disconnect_channel();
    rc = g_app.api.connect(g_app.device_id, J2534_ISO15765, 0UL, 500000UL, &g_app.channel_id);
    if (rc != J2534_STATUS_NOERROR) {
        post_logf("PassThruConnect(ISO15765 500000) failed: 0x%08lX", rc);
        (void)connect_passive();
        return;
    }
    install_obd_flow_filters();
    set_status("READ-ONLY OBD inventory in progress");
    post_logf("Starting bounded read-only OBD inventory (%lu requests maximum).",
              (unsigned long)(sizeof(queries) / sizeof(queries[0])));
    for (q = 0U; q < sizeof(queries) / sizeof(queries[0]); ++q) {
        DWORD deadline;
        if (!send_read_only_obd(queries[q], sizeof(queries[q]))) continue;
        deadline = GetTickCount() + 350U;
        while ((LONG)(deadline - GetTickCount()) > 0) {
            PASSTHRU_MSG rx[8];
            unsigned long count = 8UL;
            unsigned long i;
            unsigned long read_rc;
            memset(rx, 0, sizeof(rx));
            read_rc = g_app.api.read_msgs(g_app.channel_id, rx, &count, 50UL);
            if (read_rc != J2534_STATUS_NOERROR && read_rc != J2534_ERR_TIMEOUT) break;
            for (i = 0UL; i < count; ++i) {
                evidence_frame("rx", &rx[i], "bounded standard OBD inventory response");
                post_logf("OBD RX %03lX  %lu bytes", (unsigned long)message_can_id(&rx[i]),
                          rx[i].DataSize > 4UL ? rx[i].DataSize - 4UL : rx[i].DataSize);
            }
        }
    }
    post_logf("Read-only inventory complete. Unsafe and unknown services remain deny-by-default.");
    disconnect_channel();
    (void)connect_passive();
}

static void connect_device(void)
{
    char dll[MAX_PATH];
    unsigned long rc;
    close_device();
    GetWindowTextA(g_app.dll_edit, dll, (int)sizeof(dll));
    dll[sizeof(dll) - 1U] = '\0';
    if (dll[0] == '\0') {
        MessageBoxA(g_app.window, "No J2534 FunctionLibrary DLL is selected.", LINK_PRODUCT_NAME " Discover", MB_OK | MB_ICONWARNING);
        return;
    }
    if (!make_evidence_file()) {
        MessageBoxA(g_app.window, "Cannot create the evidence JSONL file.", LINK_PRODUCT_NAME " Discover", MB_OK | MB_ICONERROR);
        return;
    }
    if (!load_j2534(dll)) return;
    rc = g_app.api.open(NULL, &g_app.device_id);
    if (rc != J2534_STATUS_NOERROR) {
        post_logf("PassThruOpen failed: 0x%08lX", rc);
        close_device();
        return;
    }
    post_logf("J2534 device opened; evidence: %s", g_app.evidence_path);
    if (!connect_passive()) close_device();
}

static void export_evidence(void)
{
    OPENFILENAMEA ofn;
    char path[MAX_PATH] = LINK_PRODUCT_NAME "-evidence.jsonl";
    if (g_app.evidence == NULL) {
        MessageBoxA(g_app.window, "No evidence has been recorded yet.", LINK_PRODUCT_NAME " Discover", MB_OK | MB_ICONINFORMATION);
        return;
    }
    EnterCriticalSection(&g_app.evidence_lock);
    (void)link_evidence_flush(g_app.evidence);
    LeaveCriticalSection(&g_app.evidence_lock);
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_app.window;
    ofn.lpstrFilter = "JSON Lines (*.jsonl)\0*.jsonl\0All files (*.*)\0*.*\0\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = (DWORD)sizeof(path);
    ofn.lpstrDefExt = "jsonl";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (GetSaveFileNameA(&ofn) && CopyFileA(g_app.evidence_path, path, FALSE))
        post_logf("Evidence exported to %s", path);
}

static void add_annotation(void)
{
    char text[512];
    if (g_app.evidence == NULL) return;
    GetWindowTextA(g_app.note, text, (int)sizeof(text));
    text[sizeof(text) - 1U] = '\0';
    if (text[0] == '\0') return;
    EnterCriticalSection(&g_app.evidence_lock);
    (void)link_evidence_write_annotation(g_app.evidence, unix_time_ns(), text);
    (void)link_evidence_flush(g_app.evidence);
    LeaveCriticalSection(&g_app.evidence_lock);
    post_logf("ANNOTATION: %s", text);
    SetWindowTextA(g_app.note, "");
}

static void create_controls(HWND window)
{
    HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    char detected[MAX_PATH] = "";
    HWND label;
    HWND button;
    label = CreateWindowA("STATIC", "J2534 FunctionLibrary DLL", WS_CHILD | WS_VISIBLE,
                          12, 12, 180, 20, window, NULL, NULL, NULL);
    SendMessageA(label, WM_SETFONT, (WPARAM)font, TRUE);
    g_app.dll_edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                     12, 34, 640, 24, window, (HMENU)(INT_PTR)IDC_DLL, NULL, NULL);
    SendMessageA(g_app.dll_edit, WM_SETFONT, (WPARAM)font, TRUE);
    if (read_registry_openport(detected, sizeof(detected))) SetWindowTextA(g_app.dll_edit, detected);
    else SetWindowTextA(g_app.dll_edit, "C:\\Program Files (x86)\\OpenECU\\OpenPort 2.0\\op20pt32.dll");
    button = CreateWindowA("BUTTON", "Connect passive 500k", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                           12, 68, 160, 30, window, (HMENU)(INT_PTR)IDC_CONNECT, NULL, NULL);
    SendMessageA(button, WM_SETFONT, (WPARAM)font, TRUE);
    button = CreateWindowA("BUTTON", "Read-only OBD inventory", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                           180, 68, 180, 30, window, (HMENU)(INT_PTR)IDC_INVENTORY, NULL, NULL);
    SendMessageA(button, WM_SETFONT, (WPARAM)font, TRUE);
    button = CreateWindowA("BUTTON", "Stop", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                           368, 68, 80, 30, window, (HMENU)(INT_PTR)IDC_STOP, NULL, NULL);
    SendMessageA(button, WM_SETFONT, (WPARAM)font, TRUE);
    button = CreateWindowA("BUTTON", "Export JSONL", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                           456, 68, 120, 30, window, (HMENU)(INT_PTR)IDC_EXPORT, NULL, NULL);
    SendMessageA(button, WM_SETFONT, (WPARAM)font, TRUE);
    g_app.status = CreateWindowA("STATIC", "DISCONNECTED — deny-by-default safety policy active", WS_CHILD | WS_VISIBLE,
                                 12, 108, 760, 22, window, (HMENU)(INT_PTR)IDC_STATUS, NULL, NULL);
    SendMessageA(g_app.status, WM_SETFONT, (WPARAM)font, TRUE);
    g_app.log = CreateWindowExA(WS_EX_CLIENTEDGE, "LISTBOX", NULL,
                                WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOINTEGRALHEIGHT,
                                12, 136, 760, 300, window, (HMENU)(INT_PTR)IDC_LOG, NULL, NULL);
    SendMessageA(g_app.log, WM_SETFONT, (WPARAM)font, TRUE);
    g_app.note = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                 12, 448, 620, 26, window, (HMENU)(INT_PTR)IDC_NOTE, NULL, NULL);
    SendMessageA(g_app.note, WM_SETFONT, (WPARAM)font, TRUE);
    button = CreateWindowA("BUTTON", "Add annotation", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                           640, 448, 132, 26, window, (HMENU)(INT_PTR)IDC_ADDNOTE, NULL, NULL);
    SendMessageA(button, WM_SETFONT, (WPARAM)font, TRUE);
}

static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_CREATE:
        g_app.window = window;
        create_controls(window);
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wparam)) {
        case IDC_CONNECT: connect_device(); return 0;
        case IDC_INVENTORY: run_inventory(); return 0;
        case IDC_STOP:
            close_device();
            set_status("DISCONNECTED — deny-by-default safety policy active");
            post_logf("Capture stopped.");
            return 0;
        case IDC_EXPORT: export_evidence(); return 0;
        case IDC_ADDNOTE: add_annotation(); return 0;
        default: break;
        }
        break;
    case WM_LINK_LOG: {
        char *text = (char *)lparam;
        LRESULT count;
        SendMessageA(g_app.log, LB_ADDSTRING, 0U, (LPARAM)text);
        count = SendMessageA(g_app.log, LB_GETCOUNT, 0U, 0U);
        if (count > 0) SendMessageA(g_app.log, LB_SETTOPINDEX, (WPARAM)(count - 1), 0U);
        free(text);
        return 0;
    }
    case WM_DESTROY:
        close_device();
        if (g_app.evidence != NULL) {
            EnterCriticalSection(&g_app.evidence_lock);
            link_evidence_close(g_app.evidence);
            g_app.evidence = NULL;
            LeaveCriticalSection(&g_app.evidence_lock);
        }
        DeleteCriticalSection(&g_app.evidence_lock);
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcA(window, message, wparam, lparam);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command_line, int show)
{
    WNDCLASSEXA cls;
    HWND window;
    MSG message;
    (void)previous;
    (void)command_line;
    memset(&g_app, 0, sizeof(g_app));
    InitializeCriticalSection(&g_app.evidence_lock);
    memset(&cls, 0, sizeof(cls));
    cls.cbSize = sizeof(cls);
    cls.style = CS_HREDRAW | CS_VREDRAW;
    cls.lpfnWndProc = window_proc;
    cls.hInstance = instance;
    cls.hCursor = LoadCursor(NULL, IDC_ARROW);
    cls.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    cls.lpszClassName = LINK_PRODUCT_WINDOW_CLASS;
    if (RegisterClassExA(&cls) == 0U) return 1;
    window = CreateWindowExA(0U, cls.lpszClassName,
                             LINK_PRODUCT_SLUG "-discover — OpenPort 2.0 / J2534 read-only discovery",
                             WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 810, 540,
                             NULL, NULL, instance, NULL);
    if (window == NULL) return 1;
    ShowWindow(window, show);
    UpdateWindow(window);
    while (GetMessageA(&message, NULL, 0U, 0U) > 0) {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }
    return (int)message.wParam;
}
