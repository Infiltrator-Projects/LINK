// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LINK_GTK_ABOUT_H
#define LINK_GTK_ABOUT_H

#include "link/about.h"
#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

void link_gtk_show_about(GtkWindow *parent,
                         const LinkAboutInfo *info,
                         const char *emblem_resource);

#ifdef __cplusplus
}
#endif

#endif
