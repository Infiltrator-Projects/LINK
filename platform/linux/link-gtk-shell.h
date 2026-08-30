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
    /*
     * Optional low-cost progress edge.  Return true only when product-owned
     * state visible in the workspace changed (for example, a newly discovered
     * ECU or newly captured DTC).  This avoids repainting a long manufacturer
     * sweep for every expected NO DATA response.
     */
    bool (*progress_changed)(void *context);
    void (*finished)(bool complete, void *context);
} LinkGtkManufacturerExtension;

/**
 * Optional product-supplied Linux transport provider.
 *
 * The normal shell owns the serial/BlueZ provider.  Tests and specialist
 * products may instead expose a deterministic byte-stream transport (for
 * example a captured ELM327 replay) without duplicating the GTK workspace.
 * The provider is selected only when this descriptor is non-NULL.
 */
typedef struct LinkGtkTransportProvider {
    size_t (*discover)(char paths[][256], size_t capacity, void *context);
    bool (*configure)(const char *device,
                      unsigned int baud_rate,
                      LinkTransport *transport,
                      void *context);
    bool (*probe_elm327)(char *identity,
                         size_t identity_capacity,
                         void *context);
    void (*pump)(void *context);
} LinkGtkTransportProvider;

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
    /*
     * Optional product-owned runtime polling policy. Returning false keeps a
     * capability-advertised PID visible but removes it from routine dispatch.
     * The callback is re-evaluated before live scheduling so UI changes take
     * effect without reconnecting or reaching into shell internals.
     */
    bool (*polling_enabled)(uint8_t pid, void *context);
    /*
     * Optional product UI revision. Increment when a presentation preference
     * changes (for example temperature or pressure units) so disconnected GTK
     * pages are rebuilt once with the new formatting while navigation remains
     * cached and churn-free.
     */
    uint64_t (*presentation_revision)(void *context);
    void (*append_session_state_json)(GString *json, void *context);
    /*
     * Optional product action that requests a fresh manufacturer-extension
     * pass after the callback marks product-owned state (for example, selecting
     * an exhaustive sweep). The Linux shell waits for any in-flight live sample,
     * preserves the transport and standard OBD model, pauses live polling, runs
     * the extension, restores the adapter channel and resumes live polling.
     */
    const char *diagnostic_restart_action_label;
    void (*diagnostic_restart_action)(void *context);
    const LinkGtkManufacturerExtension *manufacturer_extension;
    const LinkGtkTransportProvider *transport_provider;
    void *transport_provider_context;
    bool auto_connect;
    void *context;
} LinkGtkShellDescriptor;

int link_gtk_shell_run(int argc, char **argv,
                       const LinkGtkShellDescriptor *descriptor);

#ifdef __cplusplus
}
#endif

#endif
