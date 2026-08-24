// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LINK_GTK_SHELL_H
#define LINK_GTK_SHELL_H

#include "link/transport.h"
#include <gtk/gtk.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LinkGtkShellDescriptor {
    const char *app_id;
    const char *window_title;
    const char *brand_name;
    const char *brand_subtitle;
    const char *version;
    const char *emblem_resource;
    const char *css;
    void (*render_section)(size_t section, GtkWidget *body, void *context);
    void (*show_about)(GtkWindow *window, void *context);
    void (*connection_changed)(LinkTransport *transport,
                               bool connected,
                               const char *adapter_identity,
                               void *context);
    void *context;
} LinkGtkShellDescriptor;

int link_gtk_shell_run(int argc, char **argv,
                       const LinkGtkShellDescriptor *descriptor);

#ifdef __cplusplus
}
#endif

#endif
