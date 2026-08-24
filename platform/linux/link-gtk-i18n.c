// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtk/gtk.h>

#include "link/i18n.h"

#include <string.h>

static int link_gtk_i18n_initialised;

static void ensure_locale(void)
{
    if (link_gtk_i18n_initialised) return;
    link_i18n_init();
    (void)link_i18n_set_system_locale();
    link_gtk_i18n_initialised = 1;
}

static const char *translation_key(const char *text)
{
    static const struct {
        const char *text;
        const char *key;
    } mappings[] = {
        {"Refresh", "common.refresh"},
        {"Disconnected", "connection.disconnected"},
        {"LINK UP", "connection.link_up"},
        {"LINK DOWN", "connection.link_down"},
        {"Adapter", "common.adapter"},
        {"About", "common.about"},
        {"No ELM327 serial device detected", "connection.no_device"},
        {"Invalid adapter configuration", "connection.invalid_config"},
        {"Unable to open adapter · check dialout permissions", "connection.open_failed"},
        {"Device opened but ELM327 identity handshake failed", "connection.handshake_failed"},
        {"ELM327-compatible adapter", "connection.adapter_generic"},
        {"Diagnostic state unavailable", "diagnostics.unavailable"},
        {"Linked · diagnostic session idle", "diagnostics.idle"},
        {"Linked · initialising ELM327 adapter", "diagnostics.initialising"},
        {"Linked · discovering supported OBD-II PIDs", "diagnostics.discovering_pids"},
        {"Linked · manufacturer extension pending", "diagnostics.manufacturer_pending"},
        {"Linked · restoring standard OBD-II channel", "diagnostics.restoring"},
        {"Linked · scanning stored OBD-II faults", "diagnostics.stored_dtcs"},
        {"Linked · scanning pending OBD-II faults", "diagnostics.pending_dtcs"},
        {"Linked · scanning permanent OBD-II faults", "diagnostics.permanent_dtcs"},
        {"Linked · live OBD-II polling active", "diagnostics.live"},
        {"Linked · diagnostic session failed · LINK DOWN / LINK UP to retry", "diagnostics.failed"},
        {"Linked · diagnostics active", "diagnostics.active"},
        {"Linked · diagnostics ready", "diagnostics.ready"}
    };
    size_t index;
    if (text == NULL) return NULL;
    for (index = 0U; index < sizeof(mappings) / sizeof(mappings[0]); ++index) {
        if (strcmp(text, mappings[index].text) == 0) return mappings[index].key;
    }
    return NULL;
}

static const char *translate(const char *text)
{
    const char *key;
    static char dynamic[320];
    static const char prefix[] = "Linked · ";
    static const char suffix[] = " · starting diagnostics";
    size_t length;
    size_t prefix_length;
    size_t suffix_length;

    ensure_locale();
    if (text == NULL) return "";
    key = translation_key(text);
    if (key != NULL) return link_i18n_tr(key);

    length = strlen(text);
    prefix_length = sizeof(prefix) - 1U;
    suffix_length = sizeof(suffix) - 1U;
    if (length > prefix_length + suffix_length &&
        strncmp(text, prefix, prefix_length) == 0 &&
        strcmp(text + length - suffix_length, suffix) == 0) {
        char identity[160];
        size_t identity_length = length - prefix_length - suffix_length;
        InfiltratrI18nArgument argument;
        if (identity_length >= sizeof(identity)) identity_length = sizeof(identity) - 1U;
        memcpy(identity, text + prefix_length, identity_length);
        identity[identity_length] = '\0';
        argument.name = "identity";
        argument.value = identity;
        (void)link_i18n_format(dynamic, sizeof(dynamic),
                               "connection.linked_starting", &argument, 1U);
        return dynamic;
    }
    return text;
}

GtkWidget *link_gtk_i18n_label_new(const char *text)
{
    return gtk_label_new(translate(text));
}

void link_gtk_i18n_label_set_text(GtkLabel *label, const char *text)
{
    gtk_label_set_text(label, translate(text));
}

GtkWidget *link_gtk_i18n_button_new_with_label(const char *text)
{
    return gtk_button_new_with_label(translate(text));
}

void link_gtk_i18n_button_set_label(GtkButton *button, const char *text)
{
    gtk_button_set_label(button, translate(text));
}
