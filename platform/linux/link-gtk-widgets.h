// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LINK_GTK_WIDGETS_H
#define LINK_GTK_WIDGETS_H

#include <gtk/gtk.h>

static inline GtkWidget *link_gtk_left_label(const char *text, const char *css_class)
{
    GtkWidget *label = gtk_label_new(text != NULL ? text : "");
    gtk_label_set_xalign(GTK_LABEL(label), 0.0F);
    gtk_label_set_wrap(GTK_LABEL(label), TRUE);
    if (css_class != NULL) gtk_widget_add_css_class(label, css_class);
    return label;
}

static inline GtkWidget *link_gtk_card_new(const char *kicker, const char *title)
{
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_add_css_class(card, "link-card");
    gtk_widget_set_hexpand(card, TRUE);
    if (kicker != NULL)
        gtk_box_append(GTK_BOX(card), link_gtk_left_label(kicker, "link-card-kicker"));
    if (title != NULL)
        gtk_box_append(GTK_BOX(card), link_gtk_left_label(title, "link-card-title"));
    return card;
}

static inline void link_gtk_card_append_detail(GtkWidget *card,
                                               const char *label,
                                               const char *value)
{
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
    GtkWidget *name = link_gtk_left_label(label, "link-detail-label");
    GtkWidget *detail = link_gtk_left_label(value, "link-detail-value");

    gtk_widget_add_css_class(row, "link-detail-row");
    gtk_widget_set_size_request(name, 180, -1);
    gtk_widget_set_hexpand(detail, TRUE);
    gtk_widget_set_halign(detail, GTK_ALIGN_FILL);
    gtk_label_set_xalign(GTK_LABEL(detail), 1.0F);
    gtk_label_set_selectable(GTK_LABEL(detail), TRUE);
    gtk_box_append(GTK_BOX(row), name);
    gtk_box_append(GTK_BOX(row), detail);
    gtk_box_append(GTK_BOX(card), row);
}

static inline void link_gtk_card_append_note(GtkWidget *card, const char *text)
{
    GtkWidget *note = link_gtk_left_label(text, "link-card-note");
    gtk_widget_set_hexpand(note, TRUE);
    gtk_box_append(GTK_BOX(card), note);
}

static inline void link_gtk_card_append_status(GtkWidget *card,
                                               const char *text,
                                               const char *state_class)
{
    GtkWidget *status = link_gtk_left_label(text, "link-status-chip");
    gtk_widget_set_halign(status, GTK_ALIGN_START);
    if (state_class != NULL) gtk_widget_add_css_class(status, state_class);
    gtk_box_append(GTK_BOX(card), status);
}

#endif
