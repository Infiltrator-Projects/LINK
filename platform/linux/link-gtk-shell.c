// SPDX-License-Identifier: GPL-3.0-or-later
#include "link-gtk-shell.h"
#include "link/linux_serial.h"
#include "link/elm327_session.h"
#include "link/i18n.h"
#include "link/workspace.h"

#include <stdio.h>
#include <string.h>

typedef struct LinkGtkShell {
    const LinkGtkShellDescriptor *descriptor;
    GtkWindow *window;
    GtkWidget *body;
    GtkWidget *title;
    GtkWidget *summary;
    GtkWidget *nav_list;
    GtkWidget *brand_subtitle;
    GtkWidget *language_label;
    GtkWidget *language_combo;
    GtkWidget *adapter_label;
    GtkWidget *refresh_button;
    GtkWidget *about_button;
    GtkWidget *device_combo;
    GtkWidget *status;
    GtkWidget *link_button;
    LinkLinuxSerialTransport serial;
    LinkTransport transport;
    LinkElm327Session session;
    LinkDiagnosticFlow flow;
    bool session_initialized;
    bool diagnostics_active;
    bool diagnostics_ready;
    bool session_event_pending;
    size_t current_section;
} LinkGtkShell;

static const char *const selectable_locales[] = {
    "en-AU", "de-DE", "pl-PL"
};

static const char *const selectable_locale_names[] = {
    "English", "Deutsch", "Polski", NULL
};

static const char link_gtk_base_css[] =
    ".link-root { background: transparent; }"
    ".link-sidebar { background: rgba(0,0,0,0.20); border-right: 1px solid rgba(255,255,255,0.12); padding: 16px; }"
    ".link-brand-header { padding-bottom: 8px; }"
    ".link-nav-list { background: transparent; }"
    ".link-nav-row { margin: 4px 0; padding: 3px 5px; border-radius: 11px; border: 1px solid transparent; background: transparent; }"
    ".link-nav-row:hover { background: rgba(255,255,255,0.06); border-color: rgba(255,255,255,0.12); }"
    ".link-nav-row:selected { background: rgba(255,255,255,0.11); border-color: rgba(255,255,255,0.30); }"
    ".link-about-button { margin-top: 6px; }"
    ".link-language-label { opacity: 0.72; font-size: 11px; font-weight: 700; }"
    ".link-connection-bar { padding: 12px; border-radius: 14px; }"
    ".link-link-button { font-weight: 800; padding: 8px 18px; }"
    ".link-connection-status { font-weight: 700; }"
    ".link-brand { font-size: 28px; font-weight: 900; letter-spacing: 3px; }"
    ".link-brand-subtitle { font-size: 11px; font-weight: 800; }"
    ".link-brand-version { opacity: 0.7; font-size: 11px; }"
    ".link-section-title { font-weight: 800; }"
    ".link-section-summary { opacity: 0.68; font-size: 11px; }"
    ".link-content-title { font-size: 30px; font-weight: 900; }"
    ".link-content-summary { opacity: 0.78; font-size: 14px; }";

static uint64_t monotonic_ms(void)
{
    const gint64 value = g_get_monotonic_time();
    return value <= 0 ? 0U : (uint64_t)(value / 1000);
}

static GtkWidget *left_label(const char *text, const char *css)
{
    GtkWidget *label = gtk_label_new(text != NULL ? text : "");
    gtk_label_set_xalign(GTK_LABEL(label), 0.0F);
    gtk_label_set_wrap(GTK_LABEL(label), TRUE);
    if (css != NULL) gtk_widget_add_css_class(label, css);
    return label;
}

static void load_css(const char *css)
{
    GtkCssProvider *provider;
    if (css == NULL) return;
    provider = gtk_css_provider_new();
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    gtk_css_provider_load_from_data(provider, css, -1);
    G_GNUC_END_IGNORE_DEPRECATIONS
    gtk_style_context_add_provider_for_display(gdk_display_get_default(),
                                                GTK_STYLE_PROVIDER(provider),
                                                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

static void clear_box(GtkWidget *box)
{
    GtkWidget *child = gtk_widget_get_first_child(box);
    while (child != NULL) {
        gtk_box_remove(GTK_BOX(box), child);
        child = gtk_widget_get_first_child(box);
    }
}

static char *language_config_path(void)
{
    char *directory = g_build_filename(g_get_user_config_dir(),
                                       "the-first-infiltrator", NULL);
    char *path;
    if (directory == NULL) return NULL;
    (void)g_mkdir_with_parents(directory, 0700);
    path = g_build_filename(directory, "link-language.ini", NULL);
    g_free(directory);
    return path;
}

static void save_selected_locale(const char *locale)
{
    GKeyFile *key_file;
    char *path;
    char *data;
    gsize length = 0U;
    if (locale == NULL) return;
    path = language_config_path();
    if (path == NULL) return;
    key_file = g_key_file_new();
    g_key_file_set_string(key_file, "ui", "language", locale);
    data = g_key_file_to_data(key_file, &length, NULL);
    if (data != NULL) {
        (void)g_file_set_contents(path, data, (gssize)length, NULL);
        g_free(data);
    }
    g_key_file_unref(key_file);
    g_free(path);
}

static void initialise_selected_locale(void)
{
    GKeyFile *key_file;
    char *path;
    char *locale = NULL;

    /* Let the existing adapter establish the OS locale once, then honour a
       manual saved choice. Subsequent screen translations do not reset it. */
    (void)link_gtk_i18n_translate_text("");

    path = language_config_path();
    if (path == NULL) return;
    key_file = g_key_file_new();
    if (g_key_file_load_from_file(key_file, path, G_KEY_FILE_NONE, NULL)) {
        locale = g_key_file_get_string(key_file, "ui", "language", NULL);
        if (locale != NULL) (void)link_i18n_set_locale(locale);
    }
    g_free(locale);
    g_key_file_unref(key_file);
    g_free(path);
}

static guint selected_locale_index(void)
{
    const char *locale = link_i18n_locale();
    if (locale != NULL && strncmp(locale, "de", 2U) == 0) return 1U;
    if (locale != NULL && strncmp(locale, "pl", 2U) == 0) return 2U;
    return 0U;
}

static void render_current_section(LinkGtkShell *shell)
{
    const LinkWorkspaceSectionDescriptor *section;
    if (shell == NULL || shell->body == NULL) return;
    section = link_workspace_section_at(shell->current_section);
    if (section == NULL) return;
    if (shell->title != NULL) gtk_label_set_text(GTK_LABEL(shell->title), section->title);
    if (shell->summary != NULL) gtk_label_set_text(GTK_LABEL(shell->summary), section->summary);
    clear_box(shell->body);
    if (shell->descriptor->render_section != NULL) {
        shell->descriptor->render_section(shell->current_section,
                                          shell->body,
                                          shell->descriptor->context);
    }
}

static void rebuild_navigation(LinkGtkShell *shell)
{
    GtkWidget *child;
    size_t index;
    if (shell == NULL || shell->nav_list == NULL) return;

    child = gtk_widget_get_first_child(shell->nav_list);
    while (child != NULL) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_list_box_remove(GTK_LIST_BOX(shell->nav_list), child);
        child = next;
    }

    for (index = 0U; index < link_workspace_section_count(); ++index) {
        const LinkWorkspaceSectionDescriptor *section = link_workspace_section_at(index);
        GtkWidget *row;
        GtkWidget *box;
        if (section == NULL) continue;
        row = gtk_list_box_row_new();
        box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        gtk_widget_add_css_class(row, "link-nav-row");
        gtk_box_append(GTK_BOX(box), left_label(section->title, "link-section-title"));
        gtk_box_append(GTK_BOX(box), left_label(section->summary, "link-section-summary"));
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
        g_object_set_data(G_OBJECT(row), "link-section",
                          GUINT_TO_POINTER((unsigned int)index));
        gtk_list_box_append(GTK_LIST_BOX(shell->nav_list), row);
        if (index == shell->current_section)
            gtk_list_box_select_row(GTK_LIST_BOX(shell->nav_list), GTK_LIST_BOX_ROW(row));
    }
}

static void refresh_devices(LinkGtkShell *shell)
{
    char paths[32][256];
    size_t count;
    size_t index;
    GtkStringList *model = gtk_string_list_new(NULL);
    count = link_linux_serial_discover(paths, 32U);
    for (index = 0U; index < count; ++index) gtk_string_list_append(model, paths[index]);
    gtk_drop_down_set_model(GTK_DROP_DOWN(shell->device_combo), G_LIST_MODEL(model));
    if (count != 0U) gtk_drop_down_set_selected(GTK_DROP_DOWN(shell->device_combo), 0U);
    g_object_unref(model);
}

static const char *selected_device(LinkGtkShell *shell)
{
    GObject *item = gtk_drop_down_get_selected_item(GTK_DROP_DOWN(shell->device_combo));
    return item != NULL ? gtk_string_object_get_string(GTK_STRING_OBJECT(item)) : NULL;
}

static void notify_connection(LinkGtkShell *shell, bool connected, const char *identity)
{
    if (shell->descriptor->connection_changed != NULL) {
        shell->descriptor->connection_changed(&shell->transport,
                                              connected,
                                              identity,
                                              shell->descriptor->context);
    }
}

static void notify_diagnostic(LinkGtkShell *shell,
                              const LinkDiagnosticFlowEvent *event)
{
    if (shell->descriptor->diagnostic_changed != NULL) {
        shell->descriptor->diagnostic_changed(
            shell->diagnostics_active ? &shell->flow : NULL,
            event,
            shell->diagnostics_active,
            shell->diagnostics_ready,
            shell->descriptor->context);
    }
    render_current_section(shell);
}

static void set_connection_state(LinkGtkShell *shell, bool connected, const char *message)
{
    gtk_label_set_text(GTK_LABEL(shell->status), message);
    gtk_button_set_label(GTK_BUTTON(shell->link_button), connected ? "LINK DOWN" : "LINK UP");
    gtk_widget_set_sensitive(shell->device_combo, !connected);
}

static const char *diagnostic_stage_message(const LinkGtkShell *shell)
{
    if (shell == NULL) return "Diagnostic state unavailable";
    switch (shell->flow.stage) {
    case LINK_DIAGNOSTIC_FLOW_IDLE:
        return "Linked · diagnostic session idle";
    case LINK_DIAGNOSTIC_FLOW_INITIALIZING:
        return "Linked · initialising ELM327 adapter";
    case LINK_DIAGNOSTIC_FLOW_DISCOVERING_PIDS:
        return "Linked · discovering supported OBD-II PIDs";
    case LINK_DIAGNOSTIC_FLOW_MANUFACTURER_EXTENSION:
        return "Linked · manufacturer extension pending";
    case LINK_DIAGNOSTIC_FLOW_RESTORING_AFTER_MANUFACTURER:
        return "Linked · restoring standard OBD-II channel";
    case LINK_DIAGNOSTIC_FLOW_SCANNING_STORED_DTCS:
        return "Linked · scanning stored OBD-II faults";
    case LINK_DIAGNOSTIC_FLOW_SCANNING_PENDING_DTCS:
        return "Linked · scanning pending OBD-II faults";
    case LINK_DIAGNOSTIC_FLOW_SCANNING_PERMANENT_DTCS:
        return "Linked · scanning permanent OBD-II faults";
    case LINK_DIAGNOSTIC_FLOW_LIVE:
    case LINK_DIAGNOSTIC_FLOW_READING_LIVE:
        return "Linked · live OBD-II polling active";
    case LINK_DIAGNOSTIC_FLOW_FAILED:
        return "Linked · diagnostic session failed · LINK DOWN / LINK UP to retry";
    }
    return "Linked · diagnostics active";
}

static void refresh_visible_language(LinkGtkShell *shell)
{
    const bool connected = shell != NULL &&
        shell->transport.is_connected != NULL &&
        shell->transport.is_connected(shell->transport.context);
    if (shell == NULL) return;

    if (shell->window != NULL)
        gtk_window_set_title(shell->window,
            link_gtk_i18n_translate_text(shell->descriptor->window_title));
    if (shell->brand_subtitle != NULL)
        gtk_label_set_text(GTK_LABEL(shell->brand_subtitle), shell->descriptor->brand_subtitle);
    if (shell->language_label != NULL)
        gtk_label_set_text(GTK_LABEL(shell->language_label), "Language");
    if (shell->adapter_label != NULL)
        gtk_label_set_text(GTK_LABEL(shell->adapter_label), "Adapter");
    if (shell->refresh_button != NULL)
        gtk_button_set_label(GTK_BUTTON(shell->refresh_button), "Refresh");
    if (shell->about_button != NULL)
        gtk_button_set_label(GTK_BUTTON(shell->about_button), "About");

    rebuild_navigation(shell);
    if (shell->status != NULL && shell->link_button != NULL) {
        if (connected)
            set_connection_state(shell, true,
                (shell->diagnostics_active || shell->diagnostics_ready ||
                 shell->flow.stage == LINK_DIAGNOSTIC_FLOW_FAILED)
                    ? diagnostic_stage_message(shell)
                    : "Linked · diagnostic session idle");
        else
            set_connection_state(shell, false, "Disconnected");
    }
    render_current_section(shell);
}

static void language_changed(GObject *object, GParamSpec *spec, gpointer user_data)
{
    LinkGtkShell *shell = user_data;
    guint selected;
    (void)spec;
    if (shell == NULL || object == NULL) return;
    selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(object));
    if (selected >= sizeof(selectable_locales) / sizeof(selectable_locales[0])) return;
    if (!link_i18n_set_locale(selectable_locales[selected])) return;
    save_selected_locale(selectable_locales[selected]);
    refresh_visible_language(shell);
}

static void fail_diagnostics(LinkGtkShell *shell,
                             LinkDiagnosticFlowResult failure)
{
    if (shell == NULL) return;
    link_diagnostic_flow_fail(&shell->flow, failure);
    shell->diagnostics_active = false;
    shell->diagnostics_ready = false;
    set_connection_state(shell, true,
                         "Linked · diagnostic session failed · LINK DOWN / LINK UP to retry");
    if (shell->descriptor->diagnostic_changed != NULL) {
        shell->descriptor->diagnostic_changed(&shell->flow,
                                              NULL,
                                              false,
                                              false,
                                              shell->descriptor->context);
    }
    render_current_section(shell);
}

static void session_event(void *context,
                          const LinkElm327Session *session)
{
    LinkGtkShell *shell = context;
    (void)session;
    if (shell != NULL) shell->session_event_pending = true;
}

static bool drive_diagnostics(LinkGtkShell *shell)
{
    LinkDiagnosticFlowAction action;
    LinkDiagnosticFlowResult result;

    if (shell == NULL || !shell->session_initialized || !shell->diagnostics_active)
        return false;
    if (shell->session.status == LINK_ELM327_SESSION_WAITING) return true;

    result = link_diagnostic_flow_next_action(&shell->flow, monotonic_ms(), &action);
    if (result != LINK_DIAGNOSTIC_FLOW_RESULT_OK) {
        fail_diagnostics(shell, result);
        return false;
    }

    switch (action.kind) {
    case LINK_DIAGNOSTIC_FLOW_ACTION_NONE:
    case LINK_DIAGNOSTIC_FLOW_ACTION_WAIT:
        return true;

    case LINK_DIAGNOSTIC_FLOW_ACTION_SEND_COMMAND: {
        LinkElm327SessionOpResult op;
        set_connection_state(shell, true, diagnostic_stage_message(shell));
        op = link_elm327_session_begin(&shell->session,
                                       action.command,
                                       monotonic_ms(),
                                       action.timeout_ms);
        if (op != LINK_ELM327_SESSION_OP_OK) {
            fail_diagnostics(shell, LINK_DIAGNOSTIC_FLOW_RESULT_ELM_ERROR);
            return false;
        }
        notify_diagnostic(shell, NULL);
        return true;
    }

    case LINK_DIAGNOSTIC_FLOW_ACTION_READY:
        shell->diagnostics_ready = true;
        set_connection_state(shell, true, "Linked · diagnostics ready");
        notify_diagnostic(shell, NULL);
        return true;

    case LINK_DIAGNOSTIC_FLOW_ACTION_MANUFACTURER_EXTENSION:
        fail_diagnostics(shell, LINK_DIAGNOSTIC_FLOW_RESULT_INVALID_STATE);
        return false;

    case LINK_DIAGNOSTIC_FLOW_ACTION_FAILED:
        fail_diagnostics(shell, shell->flow.failure);
        return false;
    }
    fail_diagnostics(shell, LINK_DIAGNOSTIC_FLOW_RESULT_INVALID_STATE);
    return false;
}

static void process_session_event(LinkGtkShell *shell)
{
    LinkElm327SessionStatus status;
    if (shell == NULL || !shell->session_initialized || !shell->session_event_pending) return;
    shell->session_event_pending = false;
    status = shell->session.status;

    if (status == LINK_ELM327_SESSION_COMPLETE) {
        const LinkElm327Response *response = link_elm327_session_response(&shell->session);
        LinkDiagnosticFlowEvent event;
        LinkDiagnosticFlowResult result;
        if (response == NULL) {
            fail_diagnostics(shell, LINK_DIAGNOSTIC_FLOW_RESULT_ELM_ERROR);
            return;
        }
        result = link_diagnostic_flow_accept_response(&shell->flow,
                                                      response,
                                                      monotonic_ms(),
                                                      &event);
        if (result != LINK_DIAGNOSTIC_FLOW_RESULT_OK) {
            fail_diagnostics(shell, result);
            return;
        }
        if (event.became_ready ||
            event.kind == LINK_DIAGNOSTIC_FLOW_EVENT_LIVE_SAMPLE ||
            event.kind == LINK_DIAGNOSTIC_FLOW_EVENT_LIVE_NO_DATA ||
            event.kind == LINK_DIAGNOSTIC_FLOW_EVENT_LIVE_UNSUPPORTED) {
            shell->diagnostics_ready = true;
        }
        set_connection_state(shell, true, diagnostic_stage_message(shell));
        notify_diagnostic(shell, &event);
        (void)drive_diagnostics(shell);
        return;
    }

    if (status == LINK_ELM327_SESSION_TIMED_OUT ||
        status == LINK_ELM327_SESSION_FAILED) {
        shell->flow.elm_failure = shell->session.elm_result;
        fail_diagnostics(shell, LINK_DIAGNOSTIC_FLOW_RESULT_ELM_ERROR);
        return;
    }
    if (status == LINK_ELM327_SESSION_CANCELLED) {
        fail_diagnostics(shell, LINK_DIAGNOSTIC_FLOW_RESULT_INVALID_STATE);
    }
}

static bool start_diagnostics(LinkGtkShell *shell)
{
    LinkDiagnosticFlowConfig config = LINK_DIAGNOSTIC_FLOW_CONFIG_INIT;
    if (shell == NULL) return false;

    if (!link_elm327_session_init(&shell->session,
                                  &shell->transport,
                                  session_event,
                                  shell)) {
        return false;
    }
    shell->session_initialized = true;
    if (link_elm327_session_connect(&shell->session) != LINK_TRANSPORT_OK) {
        link_elm327_session_deinit(&shell->session);
        shell->session_initialized = false;
        return false;
    }
    if (link_diagnostic_flow_init(&shell->flow, &config) != LINK_DIAGNOSTIC_FLOW_RESULT_OK ||
        link_diagnostic_flow_start(&shell->flow) != LINK_DIAGNOSTIC_FLOW_RESULT_OK) {
        link_elm327_session_deinit(&shell->session);
        shell->session_initialized = false;
        return false;
    }
    shell->diagnostics_active = true;
    shell->diagnostics_ready = false;
    shell->session_event_pending = false;
    set_connection_state(shell, true, diagnostic_stage_message(shell));
    notify_diagnostic(shell, NULL);
    return drive_diagnostics(shell);
}

static void stop_diagnostics(LinkGtkShell *shell)
{
    if (shell == NULL) return;
    if (shell->session_initialized) {
        if (link_elm327_session_is_connected(&shell->session))
            link_elm327_session_disconnect(&shell->session);
        link_elm327_session_deinit(&shell->session);
        shell->session_initialized = false;
    }
    shell->diagnostics_active = false;
    shell->diagnostics_ready = false;
    shell->session_event_pending = false;
    memset(&shell->flow, 0, sizeof(shell->flow));
    if (shell->descriptor->diagnostic_changed != NULL) {
        shell->descriptor->diagnostic_changed(NULL,
                                              NULL,
                                              false,
                                              false,
                                              shell->descriptor->context);
    }
    render_current_section(shell);
}

static void link_clicked(GtkButton *button, gpointer user_data)
{
    LinkGtkShell *shell = user_data;
    const char *device;
    char identity[160];
    (void)button;

    if (shell->transport.is_connected(shell->transport.context)) {
        stop_diagnostics(shell);
        if (shell->transport.is_connected(shell->transport.context))
            shell->transport.disconnect(shell->transport.context);
        set_connection_state(shell, false, "Disconnected");
        notify_connection(shell, false, "");
        return;
    }

    device = selected_device(shell);
    if (device == NULL || device[0] == '\0') {
        set_connection_state(shell, false, "No ELM327 serial device detected");
        return;
    }
    if (!link_linux_serial_configure(&shell->serial, device, 38400U)) {
        set_connection_state(shell, false, "Invalid adapter configuration");
        return;
    }
    if (shell->transport.connect(shell->transport.context) != LINK_TRANSPORT_OK) {
        set_connection_state(shell, false, "Unable to open adapter · check dialout permissions");
        return;
    }
    if (!link_linux_serial_probe_elm327(&shell->serial, identity, sizeof(identity))) {
        shell->transport.disconnect(shell->transport.context);
        set_connection_state(shell, false, "Device opened but ELM327 identity handshake failed");
        return;
    }
    if (identity[0] == '\0') (void)snprintf(identity, sizeof(identity), "ELM327-compatible adapter");

    {
        char message[256];
        (void)snprintf(message, sizeof(message), "Linked · %s · starting diagnostics", identity);
        set_connection_state(shell, true, message);
    }
    notify_connection(shell, true, identity);
    if (!start_diagnostics(shell)) {
        fail_diagnostics(shell, LINK_DIAGNOSTIC_FLOW_RESULT_ELM_ERROR);
    }
}

static void refresh_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    refresh_devices((LinkGtkShell *)user_data);
}

static gboolean pump_serial(gpointer user_data)
{
    LinkGtkShell *shell = user_data;
    link_linux_serial_pump(&shell->serial);
    if (shell->session_initialized) {
        (void)link_elm327_session_tick(&shell->session, monotonic_ms());
        if (shell->session_event_pending) process_session_event(shell);
        if (shell->diagnostics_active &&
            shell->session.status != LINK_ELM327_SESSION_WAITING) {
            (void)drive_diagnostics(shell);
        }
    }
    return G_SOURCE_CONTINUE;
}

static void select_section(GtkListBox *list, GtkListBoxRow *row, gpointer user_data)
{
    LinkGtkShell *shell = user_data;
    (void)list;
    if (row == NULL) return;
    shell->current_section =
        (size_t)GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(row), "link-section"));
    render_current_section(shell);
}

static void about_clicked(GtkButton *button, gpointer user_data)
{
    LinkGtkShell *shell = user_data;
    (void)button;
    if (shell->descriptor->show_about != NULL)
        shell->descriptor->show_about(shell->window, shell->descriptor->context);
}

static GtkWidget *build_connection_bar(LinkGtkShell *shell)
{
    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    shell->refresh_button = gtk_button_new_with_label("Refresh");
    shell->adapter_label = left_label("Adapter", NULL);
    shell->device_combo = gtk_drop_down_new(NULL, NULL);
    shell->status = left_label("Disconnected", "link-connection-status");
    shell->link_button = gtk_button_new_with_label("LINK UP");
    gtk_widget_set_hexpand(shell->status, TRUE);
    gtk_widget_add_css_class(bar, "link-connection-bar");
    gtk_widget_add_css_class(shell->link_button, "link-link-button");
    g_signal_connect(shell->refresh_button, "clicked", G_CALLBACK(refresh_clicked), shell);
    g_signal_connect(shell->link_button, "clicked", G_CALLBACK(link_clicked), shell);
    gtk_box_append(GTK_BOX(bar), shell->adapter_label);
    gtk_box_append(GTK_BOX(bar), shell->device_combo);
    gtk_box_append(GTK_BOX(bar), shell->refresh_button);
    gtk_box_append(GTK_BOX(bar), shell->status);
    gtk_box_append(GTK_BOX(bar), shell->link_button);
    refresh_devices(shell);
    return bar;
}

static void activate(GtkApplication *application, gpointer user_data)
{
    LinkGtkShell *shell = user_data;
    const LinkGtkShellDescriptor *d = shell->descriptor;
    GtkWidget *window = gtk_application_window_new(application);
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *main = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *brand = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkStringList *language_model;

    shell->window = GTK_WINDOW(window);
    shell->current_section = 0U;
    gtk_window_set_title(shell->window,
                         link_gtk_i18n_translate_text(d->window_title));
    gtk_window_set_default_size(shell->window, 1180, 760);
    if (d->brand_name != NULL && d->brand_name[0] != '\0') {
        char *icon_name = g_ascii_strdown(d->brand_name, -1);
        if (icon_name != NULL && icon_name[0] != '\0')
            gtk_window_set_icon_name(shell->window, icon_name);
        g_free(icon_name);
    }
    load_css(link_gtk_base_css);
    load_css(d->css);
    gtk_widget_add_css_class(root, "link-root");
    gtk_widget_add_css_class(sidebar, "link-sidebar");
    gtk_widget_add_css_class(brand, "link-brand-header");
    if (d->emblem_resource != NULL) {
        GtkWidget *image = gtk_image_new_from_resource(d->emblem_resource);
        gtk_image_set_pixel_size(GTK_IMAGE(image), 58);
        gtk_box_append(GTK_BOX(brand), image);
    }
    gtk_box_append(GTK_BOX(brand), left_label(d->brand_name, "link-brand"));
    gtk_box_append(GTK_BOX(sidebar), brand);
    shell->brand_subtitle = left_label(d->brand_subtitle, "link-brand-subtitle");
    gtk_box_append(GTK_BOX(sidebar), shell->brand_subtitle);
    gtk_box_append(GTK_BOX(sidebar), left_label(d->version, "link-brand-version"));

    shell->nav_list = gtk_list_box_new();
    gtk_widget_add_css_class(shell->nav_list, "link-nav-list");
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(shell->nav_list), GTK_SELECTION_SINGLE);
    g_signal_connect(shell->nav_list, "row-selected", G_CALLBACK(select_section), shell);
    gtk_widget_set_vexpand(shell->nav_list, TRUE);
    gtk_box_append(GTK_BOX(sidebar), shell->nav_list);
    rebuild_navigation(shell);

    shell->language_label = left_label("Language", "link-language-label");
    language_model = gtk_string_list_new(selectable_locale_names);
    shell->language_combo = gtk_drop_down_new(G_LIST_MODEL(language_model), NULL);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(shell->language_combo), selected_locale_index());
    g_object_unref(language_model);
    gtk_box_append(GTK_BOX(sidebar), shell->language_label);
    gtk_box_append(GTK_BOX(sidebar), shell->language_combo);
    g_signal_connect(shell->language_combo, "notify::selected",
                     G_CALLBACK(language_changed), shell);

    shell->about_button = gtk_button_new_with_label("About");
    gtk_widget_add_css_class(shell->about_button, "link-about-button");
    g_signal_connect(shell->about_button, "clicked", G_CALLBACK(about_clicked), shell);
    gtk_box_append(GTK_BOX(sidebar), shell->about_button);

    {
        const LinkWorkspaceSectionDescriptor *first = link_workspace_section_at(0U);
        shell->title = left_label(first != NULL ? first->title : "Diagnostics", "link-content-title");
        shell->summary = left_label(first != NULL ? first->summary : "", "link-content-summary");
    }
    shell->body = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_box_append(GTK_BOX(content), shell->title);
    gtk_box_append(GTK_BOX(content), shell->summary);
    gtk_box_append(GTK_BOX(content), shell->body);
    gtk_widget_set_vexpand(content, TRUE);

    gtk_box_append(GTK_BOX(main), build_connection_bar(shell));
    gtk_box_append(GTK_BOX(main), content);
    gtk_widget_set_hexpand(main, TRUE);
    gtk_widget_set_margin_top(main, 18);
    gtk_widget_set_margin_bottom(main, 18);
    gtk_widget_set_margin_start(main, 18);
    gtk_widget_set_margin_end(main, 18);
    gtk_widget_set_size_request(sidebar, 320, -1);

    gtk_box_append(GTK_BOX(root), sidebar);
    gtk_box_append(GTK_BOX(root), main);
    gtk_window_set_child(shell->window, root);
    render_current_section(shell);
    (void)g_timeout_add(25U, pump_serial, shell);
    gtk_window_present(shell->window);
}

int link_gtk_shell_run(int argc, char **argv,
                       const LinkGtkShellDescriptor *descriptor)
{
    GtkApplication *application;
    LinkGtkShell shell = {0};
    int status;
    if (descriptor == NULL || descriptor->app_id == NULL) return 2;
    shell.descriptor = descriptor;
    initialise_selected_locale();
    link_linux_serial_init(&shell.serial);
    shell.transport = link_linux_serial_as_transport(&shell.serial);
    if (!link_transport_is_valid(&shell.transport)) return 3;
    application = gtk_application_new(descriptor->app_id, G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(application, "activate", G_CALLBACK(activate), &shell);
    status = g_application_run(G_APPLICATION(application), argc, argv);
    stop_diagnostics(&shell);
    if (shell.transport.is_connected(shell.transport.context))
        shell.transport.disconnect(shell.transport.context);
    g_object_unref(application);
    return status;
}
