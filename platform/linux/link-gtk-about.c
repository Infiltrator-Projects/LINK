// SPDX-License-Identifier: GPL-3.0-or-later
#include "link-gtk-about.h"

static gboolean link_gtk_about_destroy_on_close(
    GtkWindow *window, gpointer user_data)
{
    (void)user_data;
    gtk_window_destroy(window);
    return TRUE;
}

static gboolean link_about_has_text(const char *value)
{
    return value != NULL && value[0] != '\0';
}

static char *link_gtk_about_comments(const LinkAboutInfo *info)
{
    GString *comments;

    if (info == NULL) return g_strdup("");
    comments = g_string_new(NULL);

    if (link_about_has_text(info->subtitle))
        g_string_append(comments, info->subtitle);
    if (link_about_has_text(info->description)) {
        if (comments->len != 0U) g_string_append(comments, "\n\n");
        g_string_append(comments, info->description);
    }
    if (link_about_has_text(info->release_date)) {
        if (comments->len != 0U) g_string_append(comments, "\n\n");
        g_string_append_printf(
            comments, "Release date: %s", info->release_date);
    }
    if (link_about_has_text(info->credits)) {
        if (comments->len != 0U) g_string_append(comments, "\n\n");
        g_string_append(comments, "Credits\n");
        g_string_append(comments, info->credits);
    }

    return g_string_free(comments, FALSE);
}

void link_gtk_show_about(GtkWindow *parent,
                         const LinkAboutInfo *info,
                         const char *emblem_resource)
{
    GtkWidget *widget;
    GtkAboutDialog *about;
    GdkTexture *logo = NULL;
    gchar **authors = NULL;
    char *comments;
    char *title;
    const char *license_text;

    if (info == NULL || !link_about_has_text(info->product_name)) return;

    widget = gtk_about_dialog_new();
    about = GTK_ABOUT_DIALOG(widget);
    title = g_strdup_printf("About %s", info->product_name);
    comments = link_gtk_about_comments(info);

    gtk_window_set_title(GTK_WINDOW(widget), title);
    gtk_window_set_transient_for(GTK_WINDOW(widget), parent);
    gtk_window_set_modal(GTK_WINDOW(widget), TRUE);
    gtk_window_set_destroy_with_parent(GTK_WINDOW(widget), TRUE);
    gtk_window_set_resizable(GTK_WINDOW(widget), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(widget), 560, 560);
    gtk_widget_set_size_request(widget, 520, 500);
    gtk_widget_add_css_class(widget, "link-about-dialog");

    gtk_about_dialog_set_program_name(about, info->product_name);
    if (link_about_has_text(info->version))
        gtk_about_dialog_set_version(about, info->version);
    if (comments[0] != '\0')
        gtk_about_dialog_set_comments(about, comments);

    if (link_about_has_text(info->authors)) {
        authors = g_strsplit(info->authors, "\n", -1);
        gtk_about_dialog_set_authors(
            about, (const char **)authors);
    }
    if (link_about_has_text(info->website)) {
        gtk_about_dialog_set_website(about, info->website);
        gtk_about_dialog_set_website_label(about, "Project website");
    }
    if (link_about_has_text(info->copyright))
        gtk_about_dialog_set_copyright(about, info->copyright);

    license_text = link_about_has_text(info->license_text)
        ? info->license_text : info->license_name;
    if (link_about_has_text(license_text)) {
        gtk_about_dialog_set_license(about, license_text);
        gtk_about_dialog_set_wrap_license(about, TRUE);
    }

    if (link_about_has_text(emblem_resource))
        logo = gdk_texture_new_from_resource(emblem_resource);
    if (logo != NULL) {
        gtk_about_dialog_set_logo(about, GDK_PAINTABLE(logo));
        g_object_unref(logo);
    }

    g_strfreev(authors);
    g_free(comments);
    g_free(title);
    g_signal_connect(widget, "close-request",
                     G_CALLBACK(link_gtk_about_destroy_on_close), NULL);
    gtk_window_present(GTK_WINDOW(widget));
}
