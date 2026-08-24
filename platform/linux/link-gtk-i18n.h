// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LINK_GTK_I18N_H
#define LINK_GTK_I18N_H

#include <gtk/gtk.h>

/* Translate one human-readable screen string for the selected UI language. */
const char *link_gtk_i18n_translate_text(const char *text);

GtkWidget *link_gtk_i18n_label_new(const char *text);
void link_gtk_i18n_label_set_text(GtkLabel *label, const char *text);
GtkWidget *link_gtk_i18n_button_new_with_label(const char *text);
void link_gtk_i18n_button_set_label(GtkButton *button, const char *text);

/*
 * Product GTK faces may force-include this header as well as the shared shell.
 * This keeps ordinary literal UI text in product code while routing what the
 * operator actually sees through the same tiny English/German/Polish table.
 */
#define gtk_label_new link_gtk_i18n_label_new
#define gtk_label_set_text link_gtk_i18n_label_set_text
#define gtk_button_new_with_label link_gtk_i18n_button_new_with_label
#define gtk_button_set_label link_gtk_i18n_button_set_label

#endif
