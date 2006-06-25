/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2006 Andrej Kacian and the Sylpheed-Claws team
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
#include <gtk/gtk.h>

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
