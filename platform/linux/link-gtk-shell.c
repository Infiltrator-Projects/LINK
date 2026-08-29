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
    GtkWidget *save_session_button;
    GtkWidget *diagnostic_restart_button;
    LinkLinuxSerialTransport serial;
    LinkTransport transport;
    LinkElm327Session session;
    LinkDiagnosticFlow flow;
    bool session_initialized;
    bool diagnostics_active;
    bool diagnostics_ready;
    bool session_event_pending;
    bool manufacturer_extension_active;
    size_t current_section;
    char adapter_identity[160];
    char adapter_device[256];
    GString *exchange_json;
    size_t trace_record_count;
    size_t exchange_count;
    bool exchange_truncated;
    uint64_t exchange_started_ms;
    uint64_t attempt_started_ms;
    unsigned int capture_attempt_count;
    unsigned int current_capture_attempt;
    bool capture_attempt_linked;
    unsigned int diagnostic_retry_count;
    bool diagnostic_retry_pending;
    uint64_t diagnostic_retry_at_ms;
    bool diagnostic_had_failure;
    guint render_source_id;
    bool render_in_progress;
    bool render_pending;
    bool diagnostic_restart_pending;
    bool native_adapter_mode;
    size_t native_receive_chunks;
} LinkGtkShell;

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
    ".link-save-session-button { font-weight: 700; padding: 8px 14px; }"
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

#define LINK_GTK_SESSION_TRACE_LIMIT (16U * 1024U * 1024U)

static void set_connection_state(
    LinkGtkShell *shell,
    bool connected,
    const char *message);

static const char *session_status_name(LinkElm327SessionStatus status)
{
    switch (status) {
    case LINK_ELM327_SESSION_IDLE: return "idle";
    case LINK_ELM327_SESSION_WAITING: return "waiting";
    case LINK_ELM327_SESSION_COMPLETE: return "complete";
    case LINK_ELM327_SESSION_TIMED_OUT: return "timed-out";
    case LINK_ELM327_SESSION_RESYNCHRONIZING: return "resynchronizing";
    case LINK_ELM327_SESSION_RESYNCHRONIZED: return "resynchronized";
    case LINK_ELM327_SESSION_CANCELLED: return "cancelled";
    case LINK_ELM327_SESSION_FAILED: return "failed";
    }
    return "unknown";
}

static void json_string(GString *out, const char *text)
{
    const unsigned char *p =
        (const unsigned char *)(text != NULL ? text : "");
    g_string_append_c(out, '"');
    for (; *p != 0U; ++p) {
        if (*p == '"') g_string_append(out, "\\\"");
        else if (*p == '\\') g_string_append(out, "\\\\");
        else if (*p == '\n') g_string_append(out, "\\n");
        else if (*p == '\r') g_string_append(out, "\\r");
        else if (*p == '\t') g_string_append(out, "\\t");
        else if (*p < 0x20U)
            g_string_append_printf(out, "\\u%04x", (unsigned int)*p);
        else
            g_string_append_c(out, (char)*p);
    }
    g_string_append_c(out, '"');
}

static void json_hex(GString *out, const uint8_t *data, size_t count)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t index;
    g_string_append_c(out, '"');
    for (index = 0U; data != NULL && index < count; ++index) {
        g_string_append_c(out, hex[data[index] >> 4U]);
        g_string_append_c(out, hex[data[index] & 15U]);
    }
    g_string_append_c(out, '"');
}

/*
 * A diagnostic investigation intentionally survives LINK DOWN / LINK UP.
 * Individual physical connection attempts are separated by explicit trace
 * records instead of erasing earlier evidence.  This is important for cold
 * ELM327/protocol-acquisition faults which may disappear on a second attempt.
 */
static void reset_session_capture(LinkGtkShell *shell)
{
    if (shell->exchange_json == NULL)
        shell->exchange_json = g_string_sized_new(8192U);
    else
        g_string_truncate(shell->exchange_json, 0U);

    shell->trace_record_count = 0U;
    shell->exchange_count = 0U;
    shell->exchange_truncated = false;
    shell->exchange_started_ms = monotonic_ms();
    shell->attempt_started_ms = 0U;
    shell->capture_attempt_count = 0U;
    shell->current_capture_attempt = 0U;
    shell->capture_attempt_linked = false;
    shell->adapter_identity[0] = '\0';
    shell->adapter_device[0] = '\0';
    shell->diagnostic_retry_count = 0U;
    shell->diagnostic_retry_pending = false;
    shell->diagnostic_retry_at_ms = 0U;
    shell->diagnostic_had_failure = false;
    shell->native_adapter_mode = false;
    shell->native_receive_chunks = 0U;
}

static bool append_trace_record(LinkGtkShell *shell, GString *record)
{
    size_t separator = 0U;
    if (shell == NULL || record == NULL) return false;
    if (shell->exchange_truncated) return false;
    if (shell->trace_record_count != 0U) separator = 1U;
    if (shell->exchange_json->len + record->len + separator >
        LINK_GTK_SESSION_TRACE_LIMIT) {
        shell->exchange_truncated = true;
        return false;
    }
    if (separator != 0U) g_string_append_c(shell->exchange_json, ',');
    g_string_append_len(
        shell->exchange_json, record->str, (gssize)record->len);
    ++shell->trace_record_count;
    return true;
}

static uint64_t investigation_elapsed_ms(const LinkGtkShell *shell)
{
    const uint64_t now = monotonic_ms();
    if (shell == NULL || now < shell->exchange_started_ms) return 0U;
    return now - shell->exchange_started_ms;
}

static uint64_t attempt_elapsed_ms(const LinkGtkShell *shell)
{
    const uint64_t now = monotonic_ms();
    if (shell == NULL || shell->attempt_started_ms == 0U ||
        now < shell->attempt_started_ms) {
        return 0U;
    }
    return now - shell->attempt_started_ms;
}

static void begin_capture_attempt(LinkGtkShell *shell, const char *device)
{
    GString *record;
    if (shell == NULL || shell->current_capture_attempt != 0U) return;

    ++shell->capture_attempt_count;
    shell->current_capture_attempt = shell->capture_attempt_count;
    shell->attempt_started_ms = monotonic_ms();
    shell->capture_attempt_linked = false;
    shell->diagnostic_retry_count = 0U;
    shell->diagnostic_retry_pending = false;
    shell->diagnostic_retry_at_ms = 0U;
    shell->diagnostic_had_failure = false;
    shell->adapter_identity[0] = '\0';
    (void)snprintf(
        shell->adapter_device, sizeof(shell->adapter_device), "%s",
        device != NULL ? device : "");

    record = g_string_sized_new(384U);
    g_string_append_printf(
        record,
        "{\"record_type\":\"attempt\",\"event\":\"start\","
        "\"attempt\":%u,\"investigation_elapsed_ms\":%llu,"
        "\"adapter_device\":",
        shell->current_capture_attempt,
        (unsigned long long)investigation_elapsed_ms(shell));
    json_string(record, shell->adapter_device);
    g_string_append_c(record, '}');
    (void)append_trace_record(shell, record);
    g_string_free(record, TRUE);
}

static void mark_capture_attempt_linked(
    LinkGtkShell *shell,
    const char *identity)
{
    GString *record;
    if (shell == NULL || shell->current_capture_attempt == 0U) return;

    shell->capture_attempt_linked = true;
    (void)snprintf(
        shell->adapter_identity, sizeof(shell->adapter_identity), "%s",
        identity != NULL ? identity : "");

    record = g_string_sized_new(448U);
    g_string_append_printf(
        record,
        "{\"record_type\":\"attempt\",\"event\":\"adapter-verified\","
        "\"attempt\":%u,\"investigation_elapsed_ms\":%llu,"
        "\"attempt_elapsed_ms\":%llu,\"adapter_device\":",
        shell->current_capture_attempt,
        (unsigned long long)investigation_elapsed_ms(shell),
        (unsigned long long)attempt_elapsed_ms(shell));
    json_string(record, shell->adapter_device);
    g_string_append(record, ",\"adapter_identity\":");
    json_string(record, shell->adapter_identity);
    g_string_append_c(record, '}');
    (void)append_trace_record(shell, record);
    g_string_free(record, TRUE);
}

static void end_capture_attempt(LinkGtkShell *shell, const char *outcome)
{
    GString *record;
    if (shell == NULL || shell->current_capture_attempt == 0U) return;

    record = g_string_sized_new(1024U);
    g_string_append_printf(
        record,
        "{\"record_type\":\"attempt\",\"event\":\"end\","
        "\"attempt\":%u,\"investigation_elapsed_ms\":%llu,"
        "\"attempt_elapsed_ms\":%llu,\"outcome\":",
        shell->current_capture_attempt,
        (unsigned long long)investigation_elapsed_ms(shell),
        (unsigned long long)attempt_elapsed_ms(shell));
    json_string(record, outcome != NULL ? outcome : "ended");
    g_string_append(record, ",\"adapter_device\":");
    json_string(record, shell->adapter_device);
    g_string_append(record, ",\"adapter_identity\":");
    json_string(record, shell->adapter_identity);
    g_string_append_printf(
        record,
        ",\"adapter_verified\":%s,\"diagnostics_active\":%s,"
        "\"diagnostics_ready\":%s,\"manufacturer_extension_active\":%s,"
        "\"diagnostic_had_failure\":%s,\"automatic_retries\":%u,"
        "\"diagnostic_stage\":",
        shell->capture_attempt_linked ? "true" : "false",
        shell->diagnostics_active ? "true" : "false",
        shell->diagnostics_ready ? "true" : "false",
        shell->manufacturer_extension_active ? "true" : "false",
        shell->diagnostic_had_failure ? "true" : "false",
        shell->diagnostic_retry_count);
    json_string(
        record,
        shell->capture_attempt_linked
            ? (shell->native_adapter_mode
                ? "native-transport-capture"
                : link_diagnostic_flow_stage_name(shell->flow.stage))
            : "not-started");
    if (shell->flow.stage == LINK_DIAGNOSTIC_FLOW_FAILED) {
        g_string_append(record, ",\"diagnostic_failure\":");
        json_string(record, link_diagnostic_flow_result_name(shell->flow.failure));
    }
    if (shell->capture_attempt_linked &&
        shell->descriptor->append_session_state_json != NULL) {
        g_string_append(record, ",\"product_state\":");
        shell->descriptor->append_session_state_json(
            record, shell->descriptor->context);
    }
    g_string_append_c(record, '}');
    (void)append_trace_record(shell, record);
    g_string_free(record, TRUE);

    shell->current_capture_attempt = 0U;
    shell->attempt_started_ms = 0U;
    shell->capture_attempt_linked = false;
}

static void native_transport_receive(
    void *context, const uint8_t *data, size_t size)
{
    LinkGtkShell *shell = context;
    GString *record;
    if (shell == NULL || data == NULL || size == 0U ||
        shell->current_capture_attempt == 0U) return;

    record = g_string_sized_new(256U + size * 2U);
    g_string_append_printf(
        record,
        "{\"record_type\":\"native-rx\",\"attempt\":%u,"
        "\"investigation_elapsed_ms\":%llu,\"attempt_elapsed_ms\":%llu,"
        "\"adapter_kind\":\"mercedes-me-native\",\"raw_hex\":",
        shell->current_capture_attempt,
        (unsigned long long)investigation_elapsed_ms(shell),
        (unsigned long long)attempt_elapsed_ms(shell));
    json_hex(record, data, size);
    g_string_append_c(record, '}');
    if (append_trace_record(shell, record)) ++shell->native_receive_chunks;
    g_string_free(record, TRUE);
}

static void record_session_exchange(LinkGtkShell *shell)
{
    const LinkElm327Session *session;
    const LinkElm327Response *response;
    GString *record;

    if (shell == NULL || shell->current_capture_attempt == 0U) return;
    session = &shell->session;
    if (session->status != LINK_ELM327_SESSION_COMPLETE &&
        session->status != LINK_ELM327_SESSION_TIMED_OUT &&
        session->status != LINK_ELM327_SESSION_CANCELLED &&
        session->status != LINK_ELM327_SESSION_FAILED) {
        return;
    }
    if (shell->exchange_truncated) return;

    record = g_string_sized_new(640U + session->parser.raw_length * 2U);
    g_string_append_printf(
        record,
        "{\"record_type\":\"elm-exchange\",\"attempt\":%u,"
        "\"investigation_elapsed_ms\":%llu,\"attempt_elapsed_ms\":%llu,"
        "\"sequence\":%llu,\"status\":",
        shell->current_capture_attempt,
        (unsigned long long)investigation_elapsed_ms(shell),
        (unsigned long long)attempt_elapsed_ms(shell),
        (unsigned long long)session->sequence);
    json_string(record, session_status_name(session->status));
    g_string_append(record, ",\"command\":");
    json_string(record, session->parser.command);
    g_string_append(record, ",\"raw_hex\":");
    json_hex(record, session->parser.raw, session->parser.raw_length);
    g_string_append(record, ",\"elm_result\":");
    json_string(record, link_elm327_result_name(session->elm_result));

    response = session->status == LINK_ELM327_SESSION_COMPLETE
        ? link_elm327_session_response(session) : NULL;
    if (response != NULL) {
        g_string_append(record, ",\"response\":{\"result\":");
        json_string(record, link_elm327_result_name(response->result));
        g_string_append(record, ",\"text\":");
        json_string(record, response->text);
        g_string_append_c(record, '}');
    } else {
        g_string_append(record, ",\"response\":null");
    }
    g_string_append_c(record, '}');

    if (append_trace_record(shell, record)) ++shell->exchange_count;
    g_string_free(record, TRUE);
}

static GString *build_session_json(const LinkGtkShell *shell)
{
    GString *out;
    GDateTime *now;
    char *timestamp;

    out = g_string_sized_new(
        3072U +
        (shell->exchange_json != NULL ? shell->exchange_json->len : 0U));
    now = g_date_time_new_now_utc();
    timestamp = now != NULL ? g_date_time_format_iso8601(now) : NULL;

    g_string_append(
        out,
        "{\n\"schema\":\"link-diagnostic-investigation/v2\","
        "\n\"generated_utc\":");
    json_string(out, timestamp != NULL ? timestamp : "unknown");
    g_string_append(out, ",\n\"product\":");
    json_string(out, shell->descriptor->brand_name);
    g_string_append(out, ",\n\"version\":");
    json_string(out, shell->descriptor->version);
    g_string_append_printf(
        out,
        ",\n\"attempts_started\":%u,\n\"active_attempt\":%u",
        shell->capture_attempt_count,
        shell->current_capture_attempt);
    g_string_append(out, ",\n\"latest_adapter_device\":");
    json_string(out, shell->adapter_device);
    g_string_append(out, ",\n\"latest_adapter_identity\":");
    json_string(out, shell->adapter_identity);
    g_string_append_printf(
        out,
        ",\n\"native_adapter_mode\":%s,"
        "\n\"native_receive_chunks\":%zu",
        shell->native_adapter_mode ? "true" : "false",
        shell->native_receive_chunks);
    g_string_append(out, ",\n\"diagnostic_stage\":");
    json_string(out, link_diagnostic_flow_stage_name(shell->flow.stage));
    g_string_append_printf(
        out,
        ",\n\"session_complete\":%s,"
        "\n\"diagnostics_active\":%s,"
        "\n\"diagnostics_ready\":%s,"
        "\n\"manufacturer_extension_active\":%s,"
        "\n\"diagnostic_had_failure\":%s,"
        "\n\"automatic_retries\":%u",
        shell->diagnostics_ready && !shell->manufacturer_extension_active
            ? "true" : "false",
        shell->diagnostics_active ? "true" : "false",
        shell->diagnostics_ready ? "true" : "false",
        shell->manufacturer_extension_active ? "true" : "false",
        shell->diagnostic_had_failure ? "true" : "false",
        shell->diagnostic_retry_count);
    if (shell->flow.stage == LINK_DIAGNOSTIC_FLOW_FAILED) {
        g_string_append(out, ",\n\"diagnostic_failure\":");
        json_string(
            out, link_diagnostic_flow_result_name(shell->flow.failure));
    }
    if (shell->descriptor->append_session_state_json != NULL) {
        g_string_append(out, ",\n\"product_state\":");
        shell->descriptor->append_session_state_json(
            out, shell->descriptor->context);
    }
    g_string_append_printf(
        out,
        ",\n\"trace\":{\"records_captured\":%zu,"
        "\"elm_exchanges\":%zu,\"truncated\":%s,\"records\":[",
        shell->trace_record_count,
        shell->exchange_count,
        shell->exchange_truncated ? "true" : "false");
    if (shell->exchange_json != NULL) {
        g_string_append_len(
            out, shell->exchange_json->str,
            (gssize)shell->exchange_json->len);
    }
    g_string_append(out, "]}\n}\n");

    g_free(timestamp);
    if (now != NULL) g_date_time_unref(now);
    return out;
}

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
static void save_session_response(GtkNativeDialog *dialog,int response,gpointer data)
{
    LinkGtkShell *shell=data;if(response==GTK_RESPONSE_ACCEPT){GFile *f=gtk_file_chooser_get_file(GTK_FILE_CHOOSER(dialog));char *path=f!=NULL?g_file_get_path(f):NULL;GString *json=build_session_json(shell);GError *err=NULL;char msg[512];
        if(path!=NULL&&g_file_set_contents(path,json->str,(gssize)json->len,&err))(void)snprintf(msg,sizeof(msg),"Session saved · %s",path);else (void)snprintf(msg,sizeof(msg),"Session save failed%s%s",err!=NULL?" · ":"",err!=NULL?err->message:"");
        set_connection_state(shell,shell->transport.is_connected!=NULL&&shell->transport.is_connected(shell->transport.context),msg);if(err!=NULL)g_error_free(err);g_string_free(json,TRUE);g_free(path);if(f!=NULL)g_object_unref(f);}
    g_object_unref(dialog);
}
static void save_session_clicked(GtkButton *button,gpointer data)
{
    LinkGtkShell *shell=data;GDateTime *now=g_date_time_new_now_local();char *stamp=now!=NULL?g_date_time_format(now,"%Y%m%d-%H%M%S"):g_strdup("session");char *name=g_strdup_printf("%s-session-%s.json",shell->descriptor->brand_name,stamp);GtkFileChooserNative *d; (void)button;
    d=gtk_file_chooser_native_new("Save Diagnostic Session",shell->window,GTK_FILE_CHOOSER_ACTION_SAVE,"Save","Cancel");gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(d),name);g_signal_connect(d,"response",G_CALLBACK(save_session_response),shell);gtk_native_dialog_show(GTK_NATIVE_DIALOG(d));g_free(name);g_free(stamp);if(now!=NULL)g_date_time_unref(now);
}

G_GNUC_END_IGNORE_DEPRECATIONS

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

static void scan_language_pack_directories(const LinkGtkShell *shell)
{
    char *slug;
    char *system_path;
    char *user_path;
    char *exe_path;
    char *exe_dir;
    char *portable_path;
    if (shell == NULL || shell->descriptor == NULL) return;
    slug = g_ascii_strdown(shell->descriptor->brand_name != NULL ? shell->descriptor->brand_name : "link", -1);
    if (slug == NULL) return;
    system_path = g_build_filename("/usr/share", slug, "Languages", NULL);
    if (system_path != NULL) {
        (void)link_i18n_scan_language_directory(system_path);
        g_free(system_path);
    }
    exe_path = g_file_read_link("/proc/self/exe", NULL);
    if (exe_path != NULL) {
        exe_dir = g_path_get_dirname(exe_path);
        portable_path = g_build_filename(exe_dir, "Languages", NULL);
        if (portable_path != NULL) {
            (void)link_i18n_scan_language_directory(portable_path);
            g_free(portable_path);
        }
        g_free(exe_dir);
        g_free(exe_path);
    }
    user_path = g_build_filename(g_get_user_data_dir(), slug, "Languages", NULL);
    if (user_path != NULL) {
        (void)link_i18n_scan_language_directory(user_path);
        g_free(user_path);
    }
    g_free(slug);
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

    (void)link_gtk_i18n_translate_text("");

    path = language_config_path();
    if (path == NULL) return;
    key_file = g_key_file_new();
    if (g_key_file_load_from_file(key_file, path, G_KEY_FILE_NONE, NULL)) {
        locale = g_key_file_get_string(key_file, "ui", "language", NULL);
        if (locale != NULL) (void)link_i18n_select_locale(locale);
    }
    g_free(locale);
    g_key_file_unref(key_file);
    g_free(path);
}

static guint selected_locale_index(void)
{
    const char *locale = link_i18n_selected_locale();
    size_t index;
    for (index = 0U; index < link_i18n_installed_locale_count(); ++index) {
        const char *candidate = link_i18n_installed_locale(index);
        if (locale != NULL && candidate != NULL && strcmp(locale, candidate) == 0)
            return (guint)index;
    }
    return 0U;
}

/*
 * GTK page construction is intentionally serialized. Diagnostic traffic can
 * generate many model changes per second; rebuilding the widget subtree from
 * those callbacks while a user is navigating creates needless destruction and
 * recreation of interactive widgets. Navigation renders immediately, while
 * telemetry refreshes are coalesced through the main loop.
 */
static void render_current_section_now(LinkGtkShell *shell)
{
    const LinkWorkspaceSectionDescriptor *section;
    if (shell == NULL || shell->body == NULL) return;
    if (shell->render_in_progress) {
        shell->render_pending = true;
        return;
    }

    shell->render_in_progress = true;
    do {
        shell->render_pending = false;
        section = link_workspace_section_at(shell->current_section);
        if (section == NULL) break;
        if (shell->title != NULL)
            gtk_label_set_text(GTK_LABEL(shell->title), section->title);
        if (shell->summary != NULL)
            gtk_label_set_text(GTK_LABEL(shell->summary), section->summary);
        clear_box(shell->body);
        if (shell->descriptor->render_section != NULL) {
            shell->descriptor->render_section(
                shell->current_section, shell->body,
                shell->descriptor->context);
        }
    } while (shell->render_pending);
    shell->render_in_progress = false;
}

static gboolean render_current_section_deferred(gpointer user_data)
{
    LinkGtkShell *shell = user_data;
    if (shell == NULL) return G_SOURCE_REMOVE;
    shell->render_source_id = 0U;
    render_current_section_now(shell);
    return G_SOURCE_REMOVE;
}

static void queue_current_section_render(LinkGtkShell *shell)
{
    if (shell == NULL || shell->body == NULL) return;
    if (shell->render_in_progress) {
        shell->render_pending = true;
        return;
    }
    if (shell->render_source_id == 0U) {
        /*
         * 100 ms caps live GTK reconstruction at 10 Hz while preserving a
         * responsive diagnostic display. Multiple samples collapse into one
         * presentation update.
         */
        shell->render_source_id = g_timeout_add(
            100U, render_current_section_deferred, shell);
    }
}

static void render_navigation_selection(LinkGtkShell *shell)
{
    if (shell == NULL) return;
    if (shell->render_source_id != 0U) {
        g_source_remove(shell->render_source_id);
        shell->render_source_id = 0U;
    }
    render_current_section_now(shell);
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
    const LinkGtkTransportProvider *provider =
        shell != NULL && shell->descriptor != NULL
            ? shell->descriptor->transport_provider : NULL;
    if (provider != NULL && provider->discover != NULL) {
        count = provider->discover(
            paths, 32U, shell->descriptor->transport_provider_context);
    } else {
        count = link_linux_serial_discover(paths, 32U);
    }
    for (index = 0U; index < count; ++index) gtk_string_list_append(model, paths[index]);
    if (count == 0U)
        gtk_string_list_append(model, link_gtk_i18n_translate_text("No adapter"));
    gtk_drop_down_set_model(GTK_DROP_DOWN(shell->device_combo), G_LIST_MODEL(model));
    gtk_drop_down_set_selected(GTK_DROP_DOWN(shell->device_combo), 0U);
    g_object_unref(model);
}

static const char *selected_device(LinkGtkShell *shell)
{
    GObject *item = gtk_drop_down_get_selected_item(GTK_DROP_DOWN(shell->device_combo));
    const char *value = item != NULL ? gtk_string_object_get_string(GTK_STRING_OBJECT(item)) : NULL;
    const char *placeholder = link_gtk_i18n_translate_text("No adapter");
    if (value == NULL || (placeholder != NULL && strcmp(value, placeholder) == 0)) return NULL;
    return value;
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
    queue_current_section_render(shell);
}

static void set_connection_state(LinkGtkShell *shell, bool connected, const char *message)
{
    gtk_label_set_text(GTK_LABEL(shell->status), message);
    gtk_button_set_label(GTK_BUTTON(shell->link_button), connected ? "LINK DOWN" : "LINK UP");
    gtk_widget_set_sensitive(shell->device_combo, !connected);
    if (shell->diagnostic_restart_button != NULL)
        gtk_widget_set_sensitive(
            shell->diagnostic_restart_button,
            connected && shell->diagnostics_ready &&
            !shell->manufacturer_extension_active &&
            !shell->diagnostic_restart_pending);
    if (shell->save_session_button != NULL) {
        const char *label = shell->native_adapter_mode
            ? "SAVE NATIVE SESSION" : "SAVE SESSION";
        if (!shell->native_adapter_mode &&
            connected && shell->flow.stage == LINK_DIAGNOSTIC_FLOW_FAILED)
            label = "SAVE FAILED SESSION";
        else if (!shell->native_adapter_mode &&
                 connected && !shell->diagnostics_ready)
            label = "SAVE PARTIAL SESSION";
        gtk_button_set_label(GTK_BUTTON(shell->save_session_button), label);
    }
}

static bool manufacturer_extension_available(const LinkGtkShell *shell)
{
    const LinkGtkManufacturerExtension *extension;
    if (shell == NULL || shell->descriptor == NULL) return false;
    extension = shell->descriptor->manufacturer_extension;
    return extension != NULL && extension->begin != NULL &&
           extension->next_command != NULL && extension->accept_response != NULL;
}

static const char *diagnostic_stage_message(const LinkGtkShell *shell)
{
    if (shell == NULL) return "Diagnostic state unavailable";
    if (shell->native_adapter_mode)
        return "Linked · Mercedes me Adapter · native protocol capture";
    switch (shell->flow.stage) {
    case LINK_DIAGNOSTIC_FLOW_IDLE:
        return "Linked · diagnostic session idle";
    case LINK_DIAGNOSTIC_FLOW_INITIALIZING:
        return "Linked · initialising ELM327 adapter";
    case LINK_DIAGNOSTIC_FLOW_DISCOVERING_PIDS:
        return "Linked · discovering supported OBD-II PIDs";
    case LINK_DIAGNOSTIC_FLOW_READING_STANDARD_VIN:
        return "Linked · reading standard vehicle VIN";
    case LINK_DIAGNOSTIC_FLOW_MANUFACTURER_EXTENSION:
        return shell->manufacturer_extension_active
            ? "Linked · running factory diagnostic extension"
            : "Linked · manufacturer extension pending";
    case LINK_DIAGNOSTIC_FLOW_RESTORING_AFTER_MANUFACTURER:
        return "Linked · restoring standard OBD-II channel";
    case LINK_DIAGNOSTIC_FLOW_SCANNING_STORED_DTCS:
        return "Linked · scanning stored OBD-II faults";
    case LINK_DIAGNOSTIC_FLOW_SCANNING_PENDING_DTCS:
        return "Linked · scanning pending OBD-II faults";
    case LINK_DIAGNOSTIC_FLOW_SCANNING_PERMANENT_DTCS:
        return "Linked · scanning permanent OBD-II faults";
    case LINK_DIAGNOSTIC_FLOW_CONFIGURING_LIVE_HEADERS:
        return "Linked · enabling CAN responder headers";
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
        gtk_label_set_text(GTK_LABEL(shell->language_label), "🌐");
    if (shell->adapter_label != NULL)
        gtk_label_set_text(GTK_LABEL(shell->adapter_label), "Adapter");
    if (shell->refresh_button != NULL)
        gtk_button_set_label(GTK_BUTTON(shell->refresh_button), "Refresh");
    if (shell->about_button != NULL)
        gtk_button_set_label(GTK_BUTTON(shell->about_button), "About");
    if (shell->save_session_button != NULL)
        gtk_button_set_label(GTK_BUTTON(shell->save_session_button), "SAVE SESSION");
    if (shell->diagnostic_restart_button != NULL &&
        shell->descriptor->diagnostic_restart_action_label != NULL)
        gtk_button_set_label(
            GTK_BUTTON(shell->diagnostic_restart_button),
            shell->descriptor->diagnostic_restart_action_label);

    refresh_devices(shell);
    rebuild_navigation(shell);
    if (shell->status != NULL && shell->link_button != NULL) {
        if (connected)
            set_connection_state(shell, true,
                shell->native_adapter_mode
                    ? diagnostic_stage_message(shell)
                    : ((shell->diagnostics_active || shell->diagnostics_ready ||
                        shell->flow.stage == LINK_DIAGNOSTIC_FLOW_FAILED)
                        ? diagnostic_stage_message(shell)
                        : "Linked · diagnostic session idle"));
        else
            set_connection_state(shell, false, "Disconnected");
    }
    render_navigation_selection(shell);
}

static void language_changed(GObject *object, GParamSpec *spec, gpointer user_data)
{
    LinkGtkShell *shell = user_data;
    guint selected;
    const char *locale;
    (void)spec;
    if (shell == NULL || object == NULL) return;
    selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(object));
    if ((size_t)selected >= link_i18n_installed_locale_count()) return;
    locale = link_i18n_installed_locale((size_t)selected);
    if (locale == NULL || !link_i18n_select_locale(locale)) return;
    save_selected_locale(locale);
    if (shell->window != NULL)
        gtk_widget_set_direction(GTK_WIDGET(shell->window),
            link_i18n_selected_locale_is_rtl() ? GTK_TEXT_DIR_RTL : GTK_TEXT_DIR_LTR);
    refresh_visible_language(shell);
}

static void fail_diagnostics(LinkGtkShell *shell,
                             LinkDiagnosticFlowResult failure)
{
    if (shell == NULL) return;
    if (shell->manufacturer_extension_active &&
        shell->descriptor->manufacturer_extension != NULL &&
        shell->descriptor->manufacturer_extension->finished != NULL) {
        shell->descriptor->manufacturer_extension->finished(
            false, shell->descriptor->context);
    }
    shell->manufacturer_extension_active = false;
    link_diagnostic_flow_fail(&shell->flow, failure);
    shell->diagnostics_active = false;
    shell->diagnostics_ready = false;
    shell->diagnostic_had_failure = true;
    if (shell->diagnostic_retry_count < 1U &&
        shell->transport.is_connected != NULL &&
        shell->transport.is_connected(shell->transport.context)) {
        shell->diagnostic_retry_count++;
        shell->diagnostic_retry_pending = true;
        shell->diagnostic_retry_at_ms = monotonic_ms() + UINT64_C(300);
        set_connection_state(shell, true,
                             "Linked · diagnostic attempt failed · retrying automatically");
    } else {
        set_connection_state(shell, true,
                             "Linked · diagnostic session failed · SAVE FAILED SESSION or LINK DOWN / LINK UP");
    }
    if (shell->descriptor->diagnostic_changed != NULL) {
        shell->descriptor->diagnostic_changed(&shell->flow,
                                              NULL,
                                              false,
                                              false,
                                              shell->descriptor->context);
    }
    queue_current_section_render(shell);
}

static void session_event(void *context,
                          const LinkElm327Session *session)
{
    LinkGtkShell *shell = context;
    (void)session;
    if (shell != NULL) shell->session_event_pending = true;
}

static bool drive_diagnostics(LinkGtkShell *shell);

static bool finish_manufacturer_extension(LinkGtkShell *shell, bool complete)
{
    LinkDiagnosticFlowResult result;
    if (shell == NULL || !shell->manufacturer_extension_active) return false;

    shell->manufacturer_extension_active = false;
    if (shell->descriptor->manufacturer_extension != NULL &&
        shell->descriptor->manufacturer_extension->finished != NULL) {
        shell->descriptor->manufacturer_extension->finished(
            complete, shell->descriptor->context);
    }

    result = link_diagnostic_flow_resume_after_manufacturer(&shell->flow);
    if (result != LINK_DIAGNOSTIC_FLOW_RESULT_OK) {
        fail_diagnostics(shell, result);
        return false;
    }
    set_connection_state(shell, true, diagnostic_stage_message(shell));
    notify_diagnostic(shell, NULL);
    return drive_diagnostics(shell);
}

static bool drive_manufacturer_extension(LinkGtkShell *shell)
{
    const LinkGtkManufacturerExtension *extension;
    char command[LINK_ELM327_MAX_COMMAND] = {0};
    size_t written = 0U;
    uint64_t timeout_ms = LINK_DIAGNOSTIC_FLOW_DEFAULT_QUERY_TIMEOUT_MS;
    LinkElm327SessionOpResult op;

    if (shell == NULL || !shell->manufacturer_extension_active ||
        !manufacturer_extension_available(shell)) return false;
    if (shell->session.status == LINK_ELM327_SESSION_WAITING) return true;

    extension = shell->descriptor->manufacturer_extension;
    if (!extension->next_command(command, sizeof(command), &written,
                                 &timeout_ms, shell->descriptor->context) ||
        command[0] == '\0' || written == 0U || written >= sizeof(command)) {
        return finish_manufacturer_extension(shell, false);
    }
    if (timeout_ms == 0U) timeout_ms = LINK_DIAGNOSTIC_FLOW_DEFAULT_QUERY_TIMEOUT_MS;

    set_connection_state(shell, true, diagnostic_stage_message(shell));
    op = link_elm327_session_begin(&shell->session, command,
                                   monotonic_ms(), timeout_ms);
    if (op != LINK_ELM327_SESSION_OP_OK) {
        fail_diagnostics(shell, LINK_DIAGNOSTIC_FLOW_RESULT_ELM_ERROR);
        return false;
    }
    /*
     * Keep the compact connection status current, but do not rebuild the
     * product workspace for every raw manufacturer command.  Long bounded
     * scans otherwise create visible flicker and needless GTK churn.
     */
    return true;
}

static bool begin_manufacturer_extension(LinkGtkShell *shell)
{
    const LinkGtkManufacturerExtension *extension;
    if (shell == NULL || !manufacturer_extension_available(shell)) {
        return false;
    }
    extension = shell->descriptor->manufacturer_extension;
    if (!extension->begin(shell->descriptor->context)) {
        shell->manufacturer_extension_active = true;
        return finish_manufacturer_extension(shell, false);
    }
    shell->manufacturer_extension_active = true;
    set_connection_state(shell, true, diagnostic_stage_message(shell));
    notify_diagnostic(shell, NULL);
    return drive_manufacturer_extension(shell);
}

static void apply_polling_policy(LinkGtkShell *shell)
{
    size_t index;
    if (shell == NULL || shell->descriptor == NULL ||
        shell->descriptor->polling_enabled == NULL) return;

    for (index = 0U; index < shell->flow.scheduler.count; ++index) {
        const LinkSchedulerItem *item = &shell->flow.scheduler.items[index];
        if (!item->pid_valid) continue;
        (void)link_scheduler_set_enabled(
            &shell->flow.scheduler,
            item->pid,
            shell->descriptor->polling_enabled(
                item->pid, shell->descriptor->context));
    }
}

static bool drive_diagnostics(LinkGtkShell *shell)
{
    LinkDiagnosticFlowAction action;
    LinkDiagnosticFlowResult result;

    if (shell == NULL || !shell->session_initialized || !shell->diagnostics_active)
        return false;
    if (shell->diagnostic_restart_pending)
        return true;
    if (shell->manufacturer_extension_active)
        return drive_manufacturer_extension(shell);
    if (shell->session.status == LINK_ELM327_SESSION_WAITING) return true;

    apply_polling_policy(shell);
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
        if (!manufacturer_extension_available(shell)) {
            shell->manufacturer_extension_active = true;
            return finish_manufacturer_extension(shell, false);
        }
        return begin_manufacturer_extension(shell);

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
    record_session_exchange(shell);

    if (status == LINK_ELM327_SESSION_COMPLETE) {
        const LinkElm327Response *response = link_elm327_session_response(&shell->session);
        if (response == NULL) {
            fail_diagnostics(shell, LINK_DIAGNOSTIC_FLOW_RESULT_ELM_ERROR);
            return;
        }

        if (shell->manufacturer_extension_active) {
            bool complete = false;
            const LinkGtkManufacturerExtension *extension =
                shell->descriptor->manufacturer_extension;
            if (extension == NULL || extension->accept_response == NULL ||
                !extension->accept_response(response, &complete,
                                            shell->descriptor->context)) {
                (void)finish_manufacturer_extension(shell, false);
                return;
            }
            if (extension->progress_changed != NULL &&
                extension->progress_changed(shell->descriptor->context)) {
                queue_current_section_render(shell);
            }
            if (complete) {
                (void)finish_manufacturer_extension(shell, true);
            } else {
                (void)drive_manufacturer_extension(shell);
            }
            return;
        }

        {
            LinkDiagnosticFlowEvent event;
            LinkDiagnosticFlowResult result =
                link_diagnostic_flow_accept_response(&shell->flow,
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
        }
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

    if (manufacturer_extension_available(shell)) {
        config.manufacturer_extension_after_standard_dtcs = true;
        config.restore_adapter_after_manufacturer_extension = true;
    }

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
    shell->manufacturer_extension_active = false;
    set_connection_state(shell, true, diagnostic_stage_message(shell));
    notify_diagnostic(shell, NULL);
    return drive_diagnostics(shell);
}

static void stop_diagnostics(LinkGtkShell *shell)
{
    if (shell == NULL) return;
    if (shell->manufacturer_extension_active &&
        shell->descriptor->manufacturer_extension != NULL &&
        shell->descriptor->manufacturer_extension->finished != NULL) {
        shell->descriptor->manufacturer_extension->finished(
            false, shell->descriptor->context);
    }
    shell->manufacturer_extension_active = false;
    if (shell->session_initialized) {
        if (link_elm327_session_is_connected(&shell->session))
            link_elm327_session_disconnect(&shell->session);
        link_elm327_session_deinit(&shell->session);
        shell->session_initialized = false;
    }
    shell->diagnostics_active = false;
    shell->diagnostics_ready = false;
    shell->session_event_pending = false;
    shell->diagnostic_restart_pending = false;
    memset(&shell->flow, 0, sizeof(shell->flow));
    if (shell->descriptor->diagnostic_changed != NULL) {
        shell->descriptor->diagnostic_changed(NULL,
                                              NULL,
                                              false,
                                              false,
                                              shell->descriptor->context);
    }
    queue_current_section_render(shell);
}

static void link_clicked(GtkButton *button, gpointer user_data)
{
    LinkGtkShell *shell = user_data;
    const char *device;
    char identity[160];
    (void)button;

    if (shell->transport.is_connected(shell->transport.context)) {
        end_capture_attempt(shell, "user-disconnect");
        stop_diagnostics(shell);
        if (shell->transport.set_receiver != NULL)
            shell->transport.set_receiver(
                shell->transport.context, NULL, NULL);
        shell->native_adapter_mode = false;
        if (shell->transport.is_connected(shell->transport.context))
            shell->transport.disconnect(shell->transport.context);
        set_connection_state(shell, false, "Disconnected");
        notify_connection(shell, false, "");
        return;
    }

    device = selected_device(shell);
    if (device == NULL || device[0] == '\0') {
        set_connection_state(shell, false, "No diagnostic adapter detected");
        return;
    }

    begin_capture_attempt(shell, device);
    {
        const LinkGtkTransportProvider *provider =
            shell->descriptor->transport_provider;
        if (provider != NULL) {
            if (provider->configure == NULL ||
                !provider->configure(
                    device, 38400U, &shell->transport,
                    shell->descriptor->transport_provider_context) ||
                !link_transport_is_valid(&shell->transport)) {
                set_connection_state(shell, false,
                                     "Invalid replay/provider configuration");
                end_capture_attempt(shell, "provider-configuration-failed");
                return;
            }
        } else if (!link_linux_serial_configure(
                       &shell->serial, device, 38400U)) {
            set_connection_state(shell, false, "Invalid adapter configuration");
            end_capture_attempt(shell, "adapter-configuration-failed");
            return;
        }
    }
    if (shell->transport.connect(shell->transport.context) != LINK_TRANSPORT_OK) {
        set_connection_state(shell, false,
            shell->descriptor->transport_provider != NULL
                ? "Unable to open replay/provider transport"
                : "Unable to open adapter · check dialout permissions");
        end_capture_attempt(shell, "transport-connect-failed");
        return;
    }
    if (shell->descriptor->transport_provider != NULL) {
        const LinkGtkTransportProvider *provider =
            shell->descriptor->transport_provider;
        if (provider->probe_elm327 != NULL &&
            !provider->probe_elm327(
                identity, sizeof(identity),
                shell->descriptor->transport_provider_context)) {
            shell->transport.disconnect(shell->transport.context);
            set_connection_state(
                shell, false,
                "Replay/provider adapter identity handshake failed");
            end_capture_attempt(shell, "adapter-identity-failed");
            return;
        }
    } else if (!link_linux_serial_probe_adapter(
                   &shell->serial, identity, sizeof(identity))) {
        shell->transport.disconnect(shell->transport.context);
        set_connection_state(shell, false,
                             "Device opened but adapter identity handshake failed");
        end_capture_attempt(shell, "adapter-identity-failed");
        return;
    }
    if (identity[0] == '\0')
        (void)snprintf(
            identity, sizeof(identity), "Diagnostic adapter");
    mark_capture_attempt_linked(shell, identity);
    shell->native_adapter_mode =
        shell->descriptor->transport_provider == NULL &&
        link_linux_serial_native_protocol_mode(&shell->serial);

    if (shell->native_adapter_mode) {
        if (shell->transport.set_receiver != NULL)
            shell->transport.set_receiver(
                shell->transport.context, native_transport_receive, shell);
        set_connection_state(
            shell, true,
            "Linked · Mercedes me Adapter · native protocol capture");
        notify_connection(shell, true, identity);
        queue_current_section_render(shell);
        return;
    }

    {
        char message[256];
        (void)snprintf(message, sizeof(message), "Linked · %s · starting diagnostics", identity);
        set_connection_state(shell, true, message);
    }
    notify_connection(shell, true, identity);
    if (!start_diagnostics(shell))
        fail_diagnostics(shell, LINK_DIAGNOSTIC_FLOW_RESULT_ELM_ERROR);
}

static gboolean auto_link_idle(gpointer user_data)
{
    LinkGtkShell *shell = user_data;
    if (shell == NULL) return G_SOURCE_REMOVE;
    link_clicked(NULL, shell);
    return G_SOURCE_REMOVE;
}

static void refresh_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    refresh_devices((LinkGtkShell *)user_data);
}

static bool begin_requested_manufacturer_rescan(LinkGtkShell *shell)
{
    if (shell == NULL || !shell->diagnostic_restart_pending ||
        !shell->diagnostics_active || !shell->session_initialized ||
        shell->manufacturer_extension_active ||
        shell->flow.stage != LINK_DIAGNOSTIC_FLOW_LIVE ||
        shell->session.status == LINK_ELM327_SESSION_WAITING ||
        shell->session.status == LINK_ELM327_SESSION_RESYNCHRONIZING) {
        return false;
    }

    /*
     * A product rescan is a manufacturer-extension operation, not a new
     * transport session. Preserve the Bluetooth/USB link and the already
     * discovered standard OBD model; only suspend live polling while the
     * product-specific scan runs. finish_manufacturer_extension() restores the
     * ELM configuration and then returns to the existing live scheduler.
     */
    shell->diagnostic_restart_pending = false;
    shell->descriptor->diagnostic_restart_action(shell->descriptor->context);
    shell->diagnostic_retry_count = 0U;
    shell->diagnostic_retry_pending = false;
    shell->diagnostic_retry_at_ms = 0U;
    shell->diagnostics_ready = false;
    shell->flow.awaiting_response = false;
    shell->flow.stage = LINK_DIAGNOSTIC_FLOW_MANUFACTURER_EXTENSION;
    set_connection_state(shell, true,
                         "Linked · running requested factory rescan");
    return begin_manufacturer_extension(shell);
}

static void diagnostic_restart_action_clicked(
    GtkButton *button, gpointer user_data)
{
    LinkGtkShell *shell = user_data;
    (void)button;

    if (shell == NULL || shell->descriptor == NULL ||
        shell->descriptor->diagnostic_restart_action == NULL) return;
    if (shell->transport.is_connected == NULL ||
        !shell->transport.is_connected(shell->transport.context)) {
        set_connection_state(shell, false,
                             "Connect an adapter before rescanning");
        return;
    }
    if (!shell->diagnostics_active || !shell->diagnostics_ready ||
        shell->manufacturer_extension_active) {
        set_connection_state(shell, true,
                             "Linked · wait for current diagnostics before rescanning");
        return;
    }
    if (shell->diagnostic_restart_pending) return;

    /*
     * If a live PID request is in flight, queue the rescan. drive_diagnostics()
     * will not dispatch another PID while this flag is set, so the current
     * response becomes a clean hand-off point into the manufacturer extension.
     */
    shell->diagnostic_restart_pending = true;
    set_connection_state(shell, true,
                         "Linked · factory rescan queued after current sample");
    (void)begin_requested_manufacturer_rescan(shell);
}

static gboolean pump_serial(gpointer user_data)
{
    LinkGtkShell *shell = user_data;
    const uint64_t now_ms = monotonic_ms();
    if (shell->descriptor->transport_provider != NULL &&
        shell->descriptor->transport_provider->pump != NULL) {
        shell->descriptor->transport_provider->pump(
            shell->descriptor->transport_provider_context);
    } else {
        link_linux_serial_pump(&shell->serial);
    }
    if (shell->diagnostic_retry_pending && now_ms >= shell->diagnostic_retry_at_ms &&
        shell->transport.is_connected != NULL &&
        shell->transport.is_connected(shell->transport.context)) {
        shell->diagnostic_retry_pending = false;
        stop_diagnostics(shell);
        if (!start_diagnostics(shell))
            fail_diagnostics(shell, LINK_DIAGNOSTIC_FLOW_RESULT_ELM_ERROR);
    }
    if (shell->session_initialized) {
        (void)link_elm327_session_tick(&shell->session, monotonic_ms());
        if (shell->session_event_pending) process_session_event(shell);
        if (shell->diagnostic_restart_pending &&
            shell->flow.stage == LINK_DIAGNOSTIC_FLOW_LIVE &&
            shell->session.status != LINK_ELM327_SESSION_WAITING &&
            shell->session.status != LINK_ELM327_SESSION_RESYNCHRONIZING) {
            (void)begin_requested_manufacturer_rescan(shell);
        }
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
    size_t section;
    (void)list;
    if (shell == NULL || row == NULL) return;
    section = (size_t)GPOINTER_TO_UINT(
        g_object_get_data(G_OBJECT(row), "link-section"));
    if (section >= link_workspace_section_count()) return;
    if (section == shell->current_section) return;
    shell->current_section = section;
    render_navigation_selection(shell);
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
    shell->save_session_button = gtk_button_new_with_label("SAVE SESSION");
    if (shell->descriptor->diagnostic_restart_action_label != NULL &&
        shell->descriptor->diagnostic_restart_action != NULL) {
        shell->diagnostic_restart_button = gtk_button_new_with_label(
            shell->descriptor->diagnostic_restart_action_label);
        gtk_widget_set_sensitive(shell->diagnostic_restart_button, FALSE);
    }
    shell->link_button = gtk_button_new_with_label("LINK UP");
    gtk_widget_set_hexpand(shell->status, TRUE);
    gtk_widget_add_css_class(bar, "link-connection-bar");
    gtk_widget_add_css_class(shell->link_button, "link-link-button");
    gtk_widget_add_css_class(shell->save_session_button, "link-save-session-button");
    if (shell->diagnostic_restart_button != NULL)
        gtk_widget_add_css_class(shell->diagnostic_restart_button, "link-save-session-button");
    g_signal_connect(shell->refresh_button, "clicked", G_CALLBACK(refresh_clicked), shell);
    g_signal_connect(shell->save_session_button, "clicked", G_CALLBACK(save_session_clicked), shell);
    if (shell->diagnostic_restart_button != NULL)
        g_signal_connect(shell->diagnostic_restart_button, "clicked",
                         G_CALLBACK(diagnostic_restart_action_clicked), shell);
    g_signal_connect(shell->link_button, "clicked", G_CALLBACK(link_clicked), shell);
    gtk_box_append(GTK_BOX(bar), shell->adapter_label);
    gtk_box_append(GTK_BOX(bar), shell->device_combo);
    gtk_box_append(GTK_BOX(bar), shell->refresh_button);
    gtk_box_append(GTK_BOX(bar), shell->save_session_button);
    if (shell->diagnostic_restart_button != NULL)
        gtk_box_append(GTK_BOX(bar), shell->diagnostic_restart_button);
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
    GtkWidget *language_row;

    scan_language_pack_directories(shell);
    initialise_selected_locale();
    shell->window = GTK_WINDOW(window);
    shell->current_section = 0U;
    gtk_widget_set_direction(window,
        strncmp(link_i18n_selected_locale(), "ar", 2U) == 0 ? GTK_TEXT_DIR_RTL : GTK_TEXT_DIR_LTR);
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

    shell->language_label = left_label("🌐", "link-language-label");
    language_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    language_model = gtk_string_list_new(NULL);
    {
        size_t language_index;
        for (language_index = 0U;
             language_index < link_i18n_installed_locale_count();
             ++language_index) {
            const char *name = link_i18n_installed_locale_name(language_index);
            if (name != NULL) gtk_string_list_append(language_model, name);
        }
    }
    shell->language_combo = gtk_drop_down_new(G_LIST_MODEL(language_model), NULL);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(shell->language_combo), selected_locale_index());
    g_object_unref(language_model);
    gtk_box_append(GTK_BOX(language_row), shell->language_label);
    gtk_box_append(GTK_BOX(language_row), shell->language_combo);
    gtk_widget_set_hexpand(shell->language_combo, TRUE);
    gtk_box_append(GTK_BOX(sidebar), language_row);
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
    render_current_section_now(shell);
    (void)g_timeout_add(25U, pump_serial, shell);
    gtk_window_present(shell->window);
    if (d->auto_connect)
        (void)g_idle_add(auto_link_idle, shell);
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
    reset_session_capture(&shell);
    link_linux_serial_init(&shell.serial);
    shell.transport = link_linux_serial_as_transport(&shell.serial);
    if (!link_transport_is_valid(&shell.transport)) return 3;
    application = gtk_application_new(descriptor->app_id, G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(application, "activate", G_CALLBACK(activate), &shell);
    status = g_application_run(G_APPLICATION(application), argc, argv);
    if (shell.current_capture_attempt != 0U)
        end_capture_attempt(&shell, "application-exit");
    stop_diagnostics(&shell);
    if (shell.transport.is_connected(shell.transport.context))
        shell.transport.disconnect(shell.transport.context);
    if (shell.render_source_id != 0U) {
        g_source_remove(shell.render_source_id);
        shell.render_source_id = 0U;
    }
    if (shell.exchange_json != NULL) g_string_free(shell.exchange_json, TRUE);
    g_object_unref(application);
    return status;
}
