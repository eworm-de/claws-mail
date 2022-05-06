/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2006-2012 Andrej Kacian and the Claws Mail team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 * 
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include "gtkutils.h"
#include "combobox.h"
#include "utils.h"

typedef struct _combobox_sel_by_data_ctx {
	GtkComboBox *combobox;
	gint data;
	const gchar *cdata;
} ComboboxSelCtx;

GtkWidget *combobox_text_new(const gboolean with_entry, const gchar *text, ...)
{
	GtkWidget *combo;
	va_list args;
	gchar *string;
	
	if(text == NULL)
		return NULL;
	
	if (with_entry)
		combo = gtk_combo_box_text_new_with_entry();
	else
		combo = gtk_combo_box_text_new();
	gtk_widget_show(combo);

	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), text);
	va_start(args, text);
	while ((string = va_arg(args, gchar*)) != NULL)
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), string);
	va_end(args);
		
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
	
	return combo;
}

static gboolean _select_by_data_func(GtkTreeModel *model,	GtkTreePath *path,
		GtkTreeIter *iter, ComboboxSelCtx *ctx)
{
	GtkComboBox *combobox = ctx->combobox;
	gint data = ctx->data;
	gint curdata;

	gtk_tree_model_get(GTK_TREE_MODEL(model), iter, COMBOBOX_DATA, &curdata, -1);
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

	cm_return_if_fail(combobox != NULL);
	cm_return_if_fail(GTK_IS_COMBO_BOX (combobox));

	model = gtk_combo_box_get_model(combobox);

	ctx = g_new(ComboboxSelCtx,
			sizeof(ComboboxSelCtx));
	ctx->combobox = combobox;
	ctx->data = data;

	gtk_tree_model_foreach(model, (GtkTreeModelForeachFunc)_select_by_data_func, ctx);
	g_free(ctx);
}

static gboolean _select_by_text_func(GtkTreeModel *model,	GtkTreePath *path,
		GtkTreeIter *iter, ComboboxSelCtx *ctx)
{
	GtkComboBox *combobox = ctx->combobox;
	const gchar *data = ctx->cdata;
	gchar *curdata;

	cm_return_val_if_fail(combobox != NULL, FALSE);
	cm_return_val_if_fail(GTK_IS_COMBO_BOX (combobox), FALSE);

	gtk_tree_model_get (GTK_TREE_MODEL(model), iter, 0, &curdata, -1);
	if (!g_utf8_collate(data, curdata)) {
		gtk_combo_box_set_active_iter(combobox, iter);
		g_free(curdata);
		return TRUE;
	} else {
		g_free(curdata);
	}

	return FALSE;
}

void combobox_select_by_text(GtkComboBox *combobox, const gchar *data)
{
	GtkTreeModel *model;
	ComboboxSelCtx *ctx = NULL;

	cm_return_if_fail(combobox != NULL);
	cm_return_if_fail(GTK_IS_COMBO_BOX (combobox));

	model = gtk_combo_box_get_model(combobox);

	ctx = g_new(ComboboxSelCtx,
			sizeof(ComboboxSelCtx));
	ctx->combobox = combobox;
	ctx->cdata = data;

	gtk_tree_model_foreach(model, (GtkTreeModelForeachFunc)_select_by_text_func, ctx);
	g_free(ctx);
}

gint combobox_get_active_data(GtkComboBox *combobox)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gint data;

	cm_return_val_if_fail(combobox != NULL, -1);
	cm_return_val_if_fail(GTK_IS_COMBO_BOX (combobox), -1);
	cm_return_val_if_fail(gtk_combo_box_get_active_iter(combobox, &iter), -1);

	model = gtk_combo_box_get_model(combobox);

	gtk_tree_model_get(model, &iter, COMBOBOX_DATA, &data, -1);

	return data;
}

void combobox_unset_popdown_strings(GtkComboBoxText *combobox)
{
	GtkTreeModel *model;
	gint count, i;

	cm_return_if_fail(combobox != NULL);
	cm_return_if_fail(GTK_IS_COMBO_BOX_TEXT (combobox));

	model = gtk_combo_box_get_model(GTK_COMBO_BOX(combobox));
	count = gtk_tree_model_iter_n_children(model, NULL);
	for (i = 0; i < count; i++)
		gtk_combo_box_text_remove(GTK_COMBO_BOX_TEXT(combobox), 0);
}

void combobox_set_popdown_strings(GtkComboBoxText *combobox,
				 GList       *list)
{
	GList *cur;

	cm_return_if_fail(combobox != NULL);
	cm_return_if_fail(GTK_IS_COMBO_BOX_TEXT (combobox));

	for (cur = list; cur != NULL; cur = g_list_next(cur))
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combobox),
						(const gchar*) cur->data);
}

gboolean combobox_set_value_from_arrow_key(GtkComboBox *combobox,
				 guint keyval)
/* used from key_press events upon gtk_combo_box_entry with one text column 
   (gtk_combo_box_text_new() and with GtkComboBoxEntry's for instance),
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

	cm_return_val_if_fail(combobox != NULL, FALSE);

	/* reproduce the behaviour of old gtk_combo_box */
	GtkTreeModel *model = gtk_combo_box_get_model(combobox);
	GtkTreeIter iter;

	if (gtk_combo_box_get_active_iter(combobox, &iter)) {
		/* if current text is in list, get prev or next one */

		if (keyval == GDK_KEY_Up) {
			gchar *text = NULL;
			gtk_tree_model_get(model, &iter, 0, &text, -1);
			valid = gtkut_tree_model_text_iter_prev(model, &iter, text);
			g_free(text);
		} else
		if (keyval == GDK_KEY_Down)
			valid = gtk_tree_model_iter_next(model, &iter);

		if (valid)
			gtk_combo_box_set_active_iter(combobox, &iter);

	} else {
		/* current text is not in list, get first or next one */

		if (keyval == GDK_KEY_Up)
			valid = gtkut_tree_model_get_iter_last(model, &iter);
		else
		if (keyval == GDK_KEY_Down)
			valid = gtk_tree_model_get_iter_first(model, &iter);

		if (valid)
			gtk_combo_box_set_active_iter(combobox, &iter);
	}

	/* return TRUE if value could be set */
	return valid;
}

static void store_set_sensitive(GtkTreeModel *model, GtkTreeIter *iter,
				const gboolean sensitive)
{
	if(GTK_IS_LIST_STORE(model)) {
		gtk_list_store_set(GTK_LIST_STORE(model), iter,
				   COMBOBOX_SENS, sensitive,
				   -1);
	} else {
		gtk_tree_store_set(GTK_TREE_STORE(model), iter,
				   COMBOBOX_SENS, sensitive,
				   -1);
	}
}

void combobox_set_sensitive(GtkComboBox *combobox, const guint index,
			    const gboolean sensitive)
{
	GtkTreeModel *model;
	GtkTreeIter iter, child;
	guint i;
	
	if((model = gtk_combo_box_get_model(combobox)) == NULL)
		return;

	if(gtk_tree_model_get_iter_first(model, &iter) == FALSE)
		return;
	for(i=0; i<index; i++) {
		if(gtk_tree_model_iter_next(model, &iter) == FALSE)
			return;
	}
	
	store_set_sensitive(model, &iter, sensitive);

	if(gtk_tree_model_iter_children(model, &child, &iter) == FALSE)
		return;
	
	do {
		store_set_sensitive(model, &child, sensitive);
	} while (gtk_tree_model_iter_next(model, &child) == TRUE);
}
