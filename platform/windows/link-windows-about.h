/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef LINK_WINDOWS_ABOUT_H
#define LINK_WINDOWS_ABOUT_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "link/about.h"

#ifdef __cplusplus
extern "C" {
#endif

void link_windows_show_about(HWND parent,
                             HICON icon,
                             const LinkAboutInfo *info);

#ifdef __cplusplus
}
#endif

#endif
