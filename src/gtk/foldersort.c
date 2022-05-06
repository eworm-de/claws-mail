/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2004-2022 the Claws Mail Team
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

#include "config.h"
#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

#include "foldersort.h"
#include "inc.h"
#include "utils.h"
#include "prefs_common.h"

enum {
	FOLDERSORT_COL_NAME,
	FOLDERSORT_COL_PTR,
	N_FOLDERSORT_COLS
};

typedef struct _FolderSortDialog FolderSortDialog;

struct _FolderSortDialog
{
	GtkWidget *window;
	GtkWidget *moveup_btn;
	GtkWidget *movedown_btn;
	GtkWidget *folderlist;
	gint rows;
};

static void destroy_dialog(FolderSortDialog *dialog)
{
	inc_unlock();
	gtk_widget_destroy(dialog->window);

	g_free(dialog);
}

static void ok_clicked(GtkWidget *widget, FolderSortDialog *dialog)
{
	Folder *folder;
	GtkTreeModel *model;
	GtkTreeIter iter;
	int i;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(dialog->folderlist));

	for (i = 0; i < dialog->rows; i++) {
		if (!gtk_tree_model_iter_nth_child(model, &iter, NULL, i))
			continue;

		gtk_tree_model_get(model, &iter, FOLDERSORT_COL_PTR, &folder, -1);

		folder_set_sort(folder, dialog->rows - i);
	}

	destroy_dialog(dialog);
}

static void cancel_clicked(GtkWidget *widget, FolderSortDialog *dialog)
{
	destroy_dialog(dialog);
}

static void set_selected(FolderSortDialog *dialog)
{
	GtkTreeSelection *sel;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreePath *path;
	gint *indices;
	gint selected;

	/* Get row number of the selected row */
	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->folderlist));
	if (!gtk_tree_selection_get_selected(sel, &model, &iter))
		return;
	path = gtk_tree_model_get_path(model, &iter);
	indices = gtk_tree_path_get_indices(path);
	selected = indices[0];
	gtk_tree_path_free(path);

	if (selected >= 0) {
		gtk_widget_set_sensitive(dialog->moveup_btn, selected > 0);
		gtk_widget_set_sensitive(dialog->movedown_btn, selected < (dialog->rows - 1));
	} else {
		gtk_widget_set_sensitive(dialog->moveup_btn, FALSE);
		gtk_widget_set_sensitive(dialog->movedown_btn, FALSE);
	}
}

static void moveup_clicked(GtkWidget *widget, FolderSortDialog *dialog)
{
	GtkTreeSelection *sel;
	GtkTreeModel *model;
	GtkTreeIter iter, previter;

	/* Get currently selected iter */
	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->folderlist));
	if (!gtk_tree_selection_get_selected(sel, &model, &iter))
		return;

	/* Now get the iter above it, if any */
	previter = iter;
	if (!gtk_tree_model_iter_previous(model, &previter)) {
		/* No previous iter, are we already on top? */
		return;
	}

	gtk_list_store_move_before(GTK_LIST_STORE(model), &iter, &previter);

	set_selected(dialog);
}

static void movedown_clicked(GtkWidget *widget, FolderSortDialog *dialog)
{
	GtkTreeSelection *sel;
	GtkTreeModel *model;
	GtkTreeIter iter, nextiter;

	/* Get currently selected iter */
	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->folderlist));
	if (!gtk_tree_selection_get_selected(sel, &model, &iter))
		return;

	/* Now get the iter above it, if any */
	nextiter = iter;
	if (!gtk_tree_model_iter_next(model, &nextiter)) {
		/* No next iter, are we already on the bottom? */
		return;
	}

	gtk_list_store_move_after(GTK_LIST_STORE(model), &iter, &nextiter);

	set_selected(dialog);
}

static void folderlist_cursor_changed_cb(GtkTreeView *view, gpointer user_data)
{
	FolderSortDialog *dialog = (FolderSortDialog *)user_data;
	set_selected(dialog);
}

static void folderlist_row_inserted_cb(GtkTreeModel *model,
		GtkTreePath *path, GtkTreeIter *iter,
		gpointer user_data)
{
	FolderSortDialog *dialog = (FolderSortDialog *)user_data;
	GtkTreeSelection *sel;

	/* The only time "row-inserted" signal should be fired is when
	 * a row is reordered using drag&drop. So we want to select
	 * the recently moved row, and call set_selected(), to update
	 * the up/down button sensitivity. */
	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->folderlist));
	gtk_tree_selection_select_iter(sel, iter);

	set_selected(dialog);
}

static gint delete_event(GtkWidget *widget, GdkEventAny *event, FolderSortDialog *dialog)
{
	destroy_dialog(dialog);
	return TRUE;
}

static gboolean key_pressed(GtkWidget *widget, GdkEventKey *event, FolderSortDialog *dialog)
{
	if (event && event->keyval == GDK_KEY_Escape)
		destroy_dialog(dialog);
	return FALSE;
}

static void foldersort_size_allocate_cb(GtkWidget *widget, GtkAllocation *allocation)
{
	cm_return_if_fail(allocation != NULL);

	gtk_window_get_size(GTK_WINDOW(widget),
		&prefs_common.foldersortwin_width, &prefs_common.foldersortwin_height);
}

void foldersort_open()
{
	FolderSortDialog *dialog = g_new0(FolderSortDialog, 1);
	GList *flist;

	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *vbox1;
	GtkWidget *label1;
	GtkWidget *hbox;
	GtkWidget *hbox2;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;
	GtkWidget *confirm_area;
	GtkWidget *moveup_btn;
	GtkWidget *movedown_btn;
	GtkWidget *btn_vbox;
	GtkWidget *scrolledwindow1;
	GtkWidget *folderlist;
	GtkTreeViewColumn *col;
	GtkListStore *store;
	GtkCellRenderer *rdr;
	GtkTreeSelection *selector;
	GtkTreeIter iter;
	static GdkGeometry geometry;

	window = gtkut_window_new(GTK_WINDOW_TOPLEVEL, "foldersort");
	g_object_set_data(G_OBJECT(window), "window", window);
	gtk_container_set_border_width(GTK_CONTAINER(window), 8);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_title(GTK_WINDOW(window), _("Set mailbox order"));
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DIALOG);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(delete_event), dialog);
	g_signal_connect (G_OBJECT(window), "size_allocate",
			 G_CALLBACK (foldersort_size_allocate_cb), NULL);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(key_pressed), dialog);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_show(vbox);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	gtkut_stock_button_set_create(&confirm_area, &cancel_btn, NULL, _("_Cancel"),
				      &ok_btn, NULL, _("_OK"),
				      NULL, NULL, NULL);
	gtk_widget_show(confirm_area);
	gtk_box_pack_end(GTK_BOX(vbox), confirm_area, FALSE, FALSE, 0);
	gtk_widget_grab_focus(ok_btn);

	g_signal_connect(G_OBJECT(ok_btn), "clicked",
                         G_CALLBACK(ok_clicked), dialog);
	g_signal_connect(G_OBJECT(cancel_btn), "clicked",
                         G_CALLBACK(cancel_clicked), dialog);

	vbox1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_widget_show(vbox1);
	gtk_box_pack_start(GTK_BOX(vbox), vbox1, TRUE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(vbox1), 2);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox, FALSE, FALSE, 0);

	label1 = gtk_label_new(_
		("Move mailboxes up or down to change the sort order "
		 "in the Folder list."));
	gtk_widget_show(label1);
	gtk_widget_set_size_request(GTK_WIDGET(label1), 392, -1);
	gtk_label_set_line_wrap(GTK_LABEL(label1), TRUE);
	gtk_box_pack_start(GTK_BOX(hbox), label1, FALSE, FALSE, 0);

	hbox2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show(hbox2);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox2, TRUE, TRUE, 0);

	scrolledwindow1 = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(scrolledwindow1);
	gtk_widget_set_size_request(scrolledwindow1, -1, 150);
	gtk_box_pack_start(GTK_BOX(hbox2), scrolledwindow1,
			   TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW (scrolledwindow1),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);

	/* Create the list store */
	store = gtk_list_store_new(N_FOLDERSORT_COLS,
			G_TYPE_STRING, G_TYPE_POINTER, -1);

	/* Create the view widget */
	folderlist = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	g_object_unref(store);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(folderlist), TRUE);
	gtk_tree_view_set_reorderable(GTK_TREE_VIEW(folderlist), TRUE);
	selector = gtk_tree_view_get_selection(GTK_TREE_VIEW(folderlist));
	gtk_tree_selection_set_mode(selector, GTK_SELECTION_BROWSE);

	/* The only column for the view widget */
	rdr = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes(_("Mailboxes"),
			rdr, "markup", FOLDERSORT_COL_NAME, NULL);
	gtk_tree_view_column_set_min_width(col, 80);
	gtk_tree_view_append_column(GTK_TREE_VIEW(folderlist), col);

	gtk_widget_show(folderlist);
	gtk_container_add(GTK_CONTAINER(scrolledwindow1), folderlist);

	btn_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_widget_show(btn_vbox);
	gtk_box_pack_start(GTK_BOX(hbox2), btn_vbox, FALSE, FALSE, 0);

	moveup_btn = gtkut_stock_button("go-up", _("_Up"));
	gtk_widget_show(moveup_btn);
	gtk_box_pack_start(GTK_BOX(btn_vbox), moveup_btn, FALSE, FALSE, 0);
	CLAWS_SET_TIP(moveup_btn,
			_("Move the selected mailbox up"));

	movedown_btn =  gtkut_stock_button("go-down", _("_Down"));
	gtk_widget_show(movedown_btn);
	gtk_box_pack_start(GTK_BOX(btn_vbox), movedown_btn, FALSE, FALSE, 0);
	CLAWS_SET_TIP(movedown_btn,
			_("Move the selected mailbox down"));

	if (!geometry.min_height) {
		geometry.min_width = 400;
		geometry.min_height = 300;
	}

	gtk_window_set_geometry_hints(GTK_WINDOW(window), NULL, &geometry,
				      GDK_HINT_MIN_SIZE);
	gtk_window_set_default_size(GTK_WINDOW(window),
				    prefs_common.foldersortwin_width,
				    prefs_common.foldersortwin_height);

	dialog->window = window;
	dialog->moveup_btn = moveup_btn;
	dialog->movedown_btn = movedown_btn;
	dialog->folderlist = folderlist;

	gtk_widget_show(window);
	gtk_widget_set_sensitive(moveup_btn, FALSE);
	gtk_widget_set_sensitive(movedown_btn, FALSE);

	/* Connect up the signals for the up/down buttons */
	g_signal_connect(G_OBJECT(moveup_btn), "clicked",
                         G_CALLBACK(moveup_clicked), dialog);
	g_signal_connect(G_OBJECT(movedown_btn), "clicked",
                         G_CALLBACK(movedown_clicked), dialog);

//	g_signal_connect(G_OBJECT(folderlist), "select-row",
//			 G_CALLBACK(row_selected), dialog);
//	g_signal_connect(G_OBJECT(folderlist), "unselect-row",
//			 G_CALLBACK(row_unselected), dialog);
//	g_signal_connect(G_OBJECT(folderlist), "row-move",
//			 G_CALLBACK(row_moved), dialog);

	/* Populate the list with mailboxes */
	dialog->rows = 0;
	for (flist = folder_get_list(); flist != NULL; flist = g_list_next(flist)) {
		Folder *folder = flist->data;

		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
				FOLDERSORT_COL_NAME, folder->name,
				FOLDERSORT_COL_PTR, folder,
				-1);
		dialog->rows++;
	}

	/* Connect up the signals for the folderlist */
	g_signal_connect(G_OBJECT(folderlist), "cursor-changed",
			G_CALLBACK(folderlist_cursor_changed_cb), dialog);
	g_signal_connect(G_OBJECT(store), "row-inserted",
			G_CALLBACK(folderlist_row_inserted_cb), dialog);

	/* Select the first row and adjust the sensitivity of
	 * the up/down buttons */
	if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter)) {
		gtk_tree_selection_select_iter(selector, &iter);
		set_selected(dialog);
	}

	inc_lock();
}
