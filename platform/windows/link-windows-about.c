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

static HRESULT CALLBACK link_windows_about_callback(
    HWND window, UINT notification, WPARAM wparam, LPARAM lparam,
    LONG_PTR reference_data)
{
    (void)wparam;
    (void)reference_data;
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
