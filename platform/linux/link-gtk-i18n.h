// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LINK_GTK_I18N_H
#define LINK_GTK_I18N_H

#include <gtk/gtk.h>

GtkWidget *link_gtk_i18n_label_new(const char *text);
void link_gtk_i18n_label_set_text(GtkLabel *label, const char *text);
GtkWidget *link_gtk_i18n_button_new_with_label(const char *text);
void link_gtk_i18n_button_set_label(GtkButton *button, const char *text);

/* Apply translation only to the shared shell translation unit. */
#define gtk_label_new link_gtk_i18n_label_new
#define gtk_label_set_text link_gtk_i18n_label_set_text
#define gtk_button_new_with_label link_gtk_i18n_button_new_with_label
#define gtk_button_set_label link_gtk_i18n_button_set_label

#endif
