// SPDX-License-Identifier: GPL-3.0-or-later
#include "link-gtk-shell.h"

#include <stdio.h>
#include <string.h>

typedef struct LinkGtkShell {
    const LinkGtkShellDescriptor *descriptor;
    GtkWindow *window;
    GtkWidget *body;
    GtkWidget *title;
    GtkWidget *summary;
    GtkWidget *device_combo;
    GtkWidget *status;
    GtkWidget *link_button;
    LinkLinuxSerialTransport serial;
} LinkGtkShell;

static GtkWidget *left_label(const char *text, const char *css)
{
    GtkWidget *label = gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0F);
    gtk_label_set_wrap(GTK_LABEL(label), TRUE);
    if (css != NULL) gtk_widget_add_css_class(label, css);
    return label;
}

static void clear_box(GtkWidget *box)
{
    GtkWidget *child = gtk_widget_get_first_child(box);
    while (child != NULL) {
        gtk_box_remove(GTK_BOX(box), child);
        child = gtk_widget_get_first_child(box);
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
    if (item == NULL) return NULL;
    return gtk_string_object_get_string(GTK_STRING_OBJECT(item));
}

static void set_connection_state(LinkGtkShell *shell, bool connected, const char *message)
{
    gtk_label_set_text(GTK_LABEL(shell->status), message);
    gtk_button_set_label(GTK_BUTTON(shell->link_button), connected ? "LINK DOWN" : "LINK UP");
    gtk_widget_set_sensitive(shell->device_combo, !connected);
}

static void link_clicked(GtkButton *button, gpointer user_data)
{
    LinkGtkShell *shell = user_data;
    const char *device;
    (void)button;
    if (link_linux_serial_is_open(&shell->serial)) {
        link_linux_serial_close(&shell->serial);
        set_connection_state(shell, false, "Disconnected");
        return;
    }
    device = selected_device(shell);
    if (device == NULL || device[0] == '\0') {
        set_connection_state(shell, false, "No ELM327 serial device detected");
        return;
    }
    if (!link_linux_serial_open(&shell->serial, device, 38400U)) {
        set_connection_state(shell, false, "Unable to open adapter (check permissions / baud rate)");
        return;
    }
    set_connection_state(shell, true, "Serial adapter open · ready for LINK diagnostic session");
}

static void refresh_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    refresh_devices((LinkGtkShell *)user_data);
}

static void select_section(GtkListBox *list, GtkListBoxRow *row, gpointer user_data)
{
    LinkGtkShell *shell = user_data;
    size_t index;
    (void)list;
    if (row == NULL) return;
    index = (size_t)GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(row), "link-section"));
    if (index >= shell->descriptor->section_count) return;
    gtk_label_set_text(GTK_LABEL(shell->title), shell->descriptor->section_titles[index]);
    gtk_label_set_text(GTK_LABEL(shell->summary), shell->descriptor->section_summaries[index]);
    clear_box(shell->body);
    if (shell->descriptor->render_section != NULL)
        shell->descriptor->render_section(index, shell->body, shell->descriptor->context);
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
    GtkWidget *refresh = gtk_button_new_with_label("Refresh");
    shell->device_combo = gtk_drop_down_new(NULL, NULL);
    shell->status = left_label("Disconnected", "link-connection-status");
    shell->link_button = gtk_button_new_with_label("LINK UP");
    gtk_widget_set_hexpand(shell->status, TRUE);
    gtk_widget_add_css_class(bar, "link-connection-bar");
    gtk_widget_add_css_class(shell->link_button, "link-link-button");
    g_signal_connect(refresh, "clicked", G_CALLBACK(refresh_clicked), shell);
    g_signal_connect(shell->link_button, "clicked", G_CALLBACK(link_clicked), shell);
    gtk_box_append(GTK_BOX(bar), left_label("Adapter", NULL));
    gtk_box_append(GTK_BOX(bar), shell->device_combo);
    gtk_box_append(GTK_BOX(bar), refresh);
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
    GtkWidget *list = gtk_list_box_new();
    GtkWidget *about = gtk_button_new_with_label("About");
    GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    size_t index;

    shell->window = GTK_WINDOW(window);
    gtk_window_set_title(shell->window, d->window_title);
    gtk_window_set_default_size(shell->window, 1180, 760);
    if (d->css != NULL) {
        GtkCssProvider *provider = gtk_css_provider_new();
        G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        gtk_css_provider_load_from_data(provider, d->css, -1);
        G_GNUC_END_IGNORE_DEPRECATIONS
        gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_unref(provider);
    }
    if (d->emblem_resource != NULL) {
        GtkWidget *image = gtk_image_new_from_resource(d->emblem_resource);
        gtk_image_set_pixel_size(GTK_IMAGE(image), 58);
        gtk_box_append(GTK_BOX(brand), image);
    }
    gtk_box_append(GTK_BOX(brand), left_label(d->brand_name, "link-brand"));
    gtk_box_append(GTK_BOX(sidebar), brand);
    gtk_box_append(GTK_BOX(sidebar), left_label(d->brand_subtitle, "link-brand-subtitle"));
    gtk_box_append(GTK_BOX(sidebar), left_label(d->version, "link-brand-version"));

    gtk_list_box_set_selection_mode(GTK_LIST_BOX(list), GTK_SELECTION_SINGLE);
    for (index = 0U; index < d->section_count; ++index) {
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        gtk_box_append(GTK_BOX(box), left_label(d->section_titles[index], "link-section-title"));
        gtk_box_append(GTK_BOX(box), left_label(d->section_summaries[index], "link-section-summary"));
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
        g_object_set_data(G_OBJECT(row), "link-section", GUINT_TO_POINTER((unsigned int)index));
        gtk_list_box_append(GTK_LIST_BOX(list), row);
    }
    g_signal_connect(list, "row-selected", G_CALLBACK(select_section), shell);
    gtk_widget_set_vexpand(list, TRUE);
    gtk_box_append(GTK_BOX(sidebar), list);
    g_signal_connect(about, "clicked", G_CALLBACK(about_clicked), shell);
    gtk_box_append(GTK_BOX(sidebar), about);

    shell->title = left_label(d->section_count ? d->section_titles[0] : "Diagnostics", "link-content-title");
    shell->summary = left_label(d->section_count ? d->section_summaries[0] : "", "link-content-summary");
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
    gtk_widget_set_margin_top(sidebar, 16);
    gtk_widget_set_margin_bottom(sidebar, 16);
    gtk_widget_set_margin_start(sidebar, 16);
    gtk_widget_set_margin_end(sidebar, 16);

    gtk_box_append(GTK_BOX(root), sidebar);
    gtk_box_append(GTK_BOX(root), main);
    gtk_window_set_child(shell->window, root);
    if (d->section_count != 0U && d->render_section != NULL)
        d->render_section(0U, shell->body, d->context);
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
    link_linux_serial_init(&shell.serial);
    application = gtk_application_new(descriptor->app_id, G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(application, "activate", G_CALLBACK(activate), &shell);
    status = g_application_run(G_APPLICATION(application), argc, argv);
    link_linux_serial_close(&shell.serial);
    g_object_unref(application);
    return status;
}
