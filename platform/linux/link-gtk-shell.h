// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LINK_GTK_SHELL_H
#define LINK_GTK_SHELL_H

#include "link/diagnostic_flow.h"
#include "link/transport.h"
#include <gtk/gtk.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Product-owned manufacturer diagnostic extension for the shared Linux shell.
 *
 * LINK owns the surrounding SAE/OBD-II workflow and ELM327 session. A product
 * may supply this bounded command/response adapter to run its own read-only
 * manufacturer probe at LINK_DIAGNOSTIC_FLOW_MANUFACTURER_EXTENSION. The shell
 * restores the generic ELM327 channel before continuing the standard DTC/live
 * pass, so product-specific headers/protocol settings cannot leak into SAE OBD.
 */
typedef struct LinkGtkManufacturerExtension {
    bool (*begin)(void *context);
    bool (*next_command)(char *buffer,
                         size_t buffer_size,
                         size_t *written,
                         uint64_t *timeout_ms,
                         void *context);
    bool (*accept_response)(const LinkElm327Response *response,
                            bool *complete,
                            void *context);
    void (*finished)(bool complete, void *context);
} LinkGtkManufacturerExtension;

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
    void (*diagnostic_changed)(const LinkDiagnosticFlow *flow,
                               const LinkDiagnosticFlowEvent *event,
                               bool active,
                               bool ready,
                               void *context);
    void (*append_session_state_json)(GString *json, void *context);
    const LinkGtkManufacturerExtension *manufacturer_extension;
    void *context;
} LinkGtkShellDescriptor;

int link_gtk_shell_run(int argc, char **argv,
                       const LinkGtkShellDescriptor *descriptor);

#ifdef __cplusplus
}
#endif

#endif
