/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2006-2007 Andrej Kacian and the Claws Mail team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include "gtkutils.h"

typedef struct _combobox_sel_by_data_ctx {
	GtkComboBox *combobox;
	gint data;
} ComboboxSelCtx;

static gboolean _select_by_data_func(GtkTreeModel *model,	GtkTreePath *path,
		GtkTreeIter *iter, ComboboxSelCtx *ctx)
{
	GtkComboBox *combobox = ctx->combobox;
	gint data = ctx->data;
	gint curdata;

	gtk_tree_model_get(model, iter, 1, &curdata, -1);
	if (data == curdata) {
		gtk_combo_box_set_active_iter(combobox, iter);
		return TRUE;
	}

	return FALSE;
}

void combobox_select_by_data(GtkComboBox *combobox, gint data)
{
	GtkTreeModel *model;
	ComboboxSelCtx *ctx = NULL;
	g_return_if_fail(combobox != NULL);

	model = gtk_combo_box_get_model(combobox);

	ctx = g_new(ComboboxSelCtx,
			sizeof(ComboboxSelCtx));
	ctx->combobox = combobox;
	ctx->data = data;

	gtk_tree_model_foreach(model, (GtkTreeModelForeachFunc)_select_by_data_func, ctx);
	g_free(ctx);
}

gint combobox_get_active_data(GtkComboBox *combobox)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gint data;

	g_return_val_if_fail(combobox != NULL, -1);

	gtk_combo_box_get_active_iter(combobox, &iter);

	model = gtk_combo_box_get_model(combobox);

	gtk_tree_model_get(model, &iter, 1, &data, -1);

	return data;
}

void combobox_unset_popdown_strings(GtkComboBox *combobox)
{
	GtkTreeModel *model;
	gint count, i;

	g_return_if_fail(combobox != NULL);

	model = gtk_combo_box_get_model(combobox);
	count = gtk_tree_model_iter_n_children(model, NULL);
	for (i = 0; i < count; i++)
		gtk_combo_box_remove_text(combobox, 0);
}

void combobox_set_popdown_strings(GtkComboBox *combobox,
				 GList       *list)
{
	GList *cur;

	g_return_if_fail(combobox != NULL);

	for (cur = list; cur != NULL; cur = g_list_next(cur))
		gtk_combo_box_append_text(combobox, (const gchar*) cur->data);
}

gboolean combobox_set_value_from_arrow_key(GtkComboBox *combobox,
				 guint keyval)
/* used from key_press events upon gtk_combo_box_entry with one text column 
   (gtk_combo_box_new_text() and with GtkComboBoxEntry's for instance),
   make sure that up and down arrow keys behave the same as old with old
   gtk_combo widgets:
    when pressing Up:
	  if the current text in entry widget is not found in combo list,
	    get last value from combo list
	  if the current text in entry widget exists in combo list,
	    get prev value from combo list
    when pressing Down:
	  if the current text in entry widget is not found in combo list,
	    get first value from combo list
	  if the current text in entry widget exists in combo list,
	    get next value from combo list
*/
{
	gboolean valid = FALSE;

	g_return_val_if_fail(combobox != NULL, FALSE);

	/* reproduce the behaviour of old gtk_combo_box */
	GtkTreeModel *model = gtk_combo_box_get_model(combobox);
	GtkTreeIter iter;

	if (gtk_combo_box_get_active_iter(combobox, &iter)) {
		/* if current text is in list, get prev or next one */

		if (keyval == GDK_Up) {
			gchar *text = gtk_combo_box_get_active_text(combobox);
			if (!text)
				text = gtk_editable_get_chars(GTK_EDITABLE(gtk_bin_get_child(GTK_BIN(combobox))),0,-1);
			valid = gtkut_tree_model_text_iter_prev(model, &iter, text);
			g_free(text);
		} else
		if (keyval == GDK_Down)
			valid = gtk_tree_model_iter_next(model, &iter);

		if (valid)
			gtk_combo_box_set_active_iter(combobox, &iter);

	} else {
		/* current text is not in list, get first or next one */

		if (keyval == GDK_Up)
			valid = gtkut_tree_model_get_iter_last(model, &iter);
		else
		if (keyval == GDK_Down)
			valid = gtk_tree_model_get_iter_first(model, &iter);

		if (valid)
			gtk_combo_box_set_active_iter(combobox, &iter);
	}

	/* return TRUE if value could be set */
	return valid;
}
