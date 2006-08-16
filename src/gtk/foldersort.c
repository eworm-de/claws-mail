/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2006 Hiroyuki Yamamoto and the Sylpheed-Claws Team
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

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

#include "foldersort.h"
#include "inc.h"
#include "utils.h"

typedef struct _FolderSortDialog FolderSortDialog;

struct _FolderSortDialog
{
	GtkWidget *window;
	GtkWidget *moveup_btn;
	GtkWidget *movedown_btn;
	GtkWidget *folderlist;

	gint rows, selected;
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
	int i;

	for (i = 0; i < dialog->rows; i++) {
		folder = gtk_clist_get_row_data(GTK_CLIST(dialog->folderlist), i);

		folder_set_sort(folder, dialog->rows - i);
	}

	destroy_dialog(dialog);
}

static void cancel_clicked(GtkWidget *widget, FolderSortDialog *dialog)
{
	destroy_dialog(dialog);
}

static void set_selected(FolderSortDialog *dialog, gint row)
{
	if (row >= 0) {
		gtk_widget_set_sensitive(dialog->moveup_btn, row > 0);
		gtk_widget_set_sensitive(dialog->movedown_btn, row < (dialog->rows - 1));
	} else {
		gtk_widget_set_sensitive(dialog->moveup_btn, FALSE);
		gtk_widget_set_sensitive(dialog->movedown_btn, FALSE);
	}

	dialog->selected = row;
}

static void moveup_clicked(GtkWidget *widget, FolderSortDialog *dialog)
{
	g_return_if_fail(dialog->selected > 0);

	gtk_clist_swap_rows(GTK_CLIST(dialog->folderlist), dialog->selected, dialog->selected - 1);
}

static void movedown_clicked(GtkWidget *widget, FolderSortDialog *dialog)
{
	g_return_if_fail(dialog->selected < (dialog->rows - 1));

	gtk_clist_swap_rows(GTK_CLIST(dialog->folderlist), dialog->selected, dialog->selected + 1);
}

static void row_selected(GtkCList *clist, gint row, gint column, GdkEventButton *event, FolderSortDialog *dialog)
{
	set_selected(dialog, row);
}

static void row_unselected(GtkCList *clist, gint row, gint column, GdkEventButton *event, FolderSortDialog *dialog)
{
	set_selected(dialog, -1);
}

static void row_moved(GtkCList *clist, gint srcpos, gint destpos, FolderSortDialog *dialog)
{
	if (dialog->selected == -1)
		return;
	else if (srcpos == dialog->selected)
		set_selected(dialog, destpos);
	else if (srcpos < dialog->selected && destpos >= dialog->selected)
		set_selected(dialog, dialog->selected - 1);
	else if (srcpos > dialog->selected && destpos <= dialog->selected)
		set_selected(dialog, dialog->selected + 1);
}

static gint delete_event(GtkWidget *widget, GdkEventAny *event, FolderSortDialog *dialog)
{
	destroy_dialog(dialog);
	return TRUE;
}

static gboolean key_pressed(GtkWidget *widget, GdkEventKey *event, FolderSortDialog *dialog)
{
	if (event && event->keyval == GDK_Escape)
		destroy_dialog(dialog);
	return FALSE;
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
	GtkWidget *label2;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_object_set_data(G_OBJECT(window), "window", window);
	gtk_container_set_border_width(GTK_CONTAINER(window), 8);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_title(GTK_WINDOW(window), _("Set folder order"));
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	gtk_window_set_default_size(GTK_WINDOW(window), 400, 300);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(delete_event), dialog);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(key_pressed), dialog);

	vbox = gtk_vbox_new(FALSE, 6);
	gtk_widget_show(vbox);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	gtkut_stock_button_set_create(&confirm_area, &cancel_btn, GTK_STOCK_CANCEL,
				      &ok_btn, GTK_STOCK_OK,
				      NULL, NULL);
	gtk_widget_show(confirm_area);
	gtk_box_pack_end(GTK_BOX(vbox), confirm_area, FALSE, FALSE, 0);
	gtk_widget_grab_focus(ok_btn);

	g_signal_connect(G_OBJECT(ok_btn), "clicked",
                         G_CALLBACK(ok_clicked), dialog);
	g_signal_connect(G_OBJECT(cancel_btn), "clicked",
                         G_CALLBACK(cancel_clicked), dialog);

	vbox1 = gtk_vbox_new(FALSE, 8);
	gtk_widget_show(vbox1);
	gtk_box_pack_start(GTK_BOX(vbox), vbox1, TRUE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(vbox1), 2);

	hbox = gtk_hbox_new(FALSE, 8);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox, FALSE, FALSE, 0);

	label1 = gtk_label_new(_
		("Move folders up or down to change the sort order in the folder list."));
	gtk_widget_show(label1);
	gtk_label_set_line_wrap(GTK_LABEL(label1), TRUE);
	gtk_box_pack_start(GTK_BOX(hbox), label1, FALSE, FALSE, 0);

	hbox2 = gtk_hbox_new(FALSE, 8);
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

	folderlist = gtk_clist_new(1);
	gtk_widget_show(folderlist);
	gtk_container_add(GTK_CONTAINER(scrolledwindow1), folderlist);
	gtk_clist_set_column_width(GTK_CLIST(folderlist), 0, 80);
	gtk_clist_column_titles_show(GTK_CLIST(folderlist));

	label2 = gtk_label_new(_("Folders"));
	gtk_widget_show(label2);
	gtk_clist_set_column_widget(GTK_CLIST(folderlist), 0, label2);
	gtk_label_set_justify(GTK_LABEL(label2), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(label2), 0, 0.5);

	btn_vbox = gtk_vbox_new(FALSE, 8);
	gtk_widget_show(btn_vbox);
	gtk_box_pack_start(GTK_BOX(hbox2), btn_vbox, FALSE, FALSE, 0);

	moveup_btn = gtk_button_new_from_stock(GTK_STOCK_GO_UP);
	gtk_widget_show(moveup_btn);
	gtk_box_pack_start(GTK_BOX(btn_vbox), moveup_btn, FALSE, FALSE, 0);

	movedown_btn =  gtk_button_new_from_stock(GTK_STOCK_GO_DOWN);
	gtk_widget_show(movedown_btn);
	gtk_box_pack_start(GTK_BOX(btn_vbox), movedown_btn, FALSE, FALSE, 0);

	dialog->window = window;
	dialog->moveup_btn = moveup_btn;
	dialog->movedown_btn = movedown_btn;
	dialog->folderlist = folderlist;

	gtk_widget_show(window);
	gtk_widget_set_sensitive(moveup_btn, FALSE);
	gtk_widget_set_sensitive(movedown_btn, FALSE);
	gtk_clist_set_reorderable(GTK_CLIST(folderlist), TRUE);

	g_signal_connect(G_OBJECT(moveup_btn), "clicked",
                         G_CALLBACK(moveup_clicked), dialog);
	g_signal_connect(G_OBJECT(movedown_btn), "clicked",
                         G_CALLBACK(movedown_clicked), dialog);

	g_signal_connect(G_OBJECT(folderlist), "select-row",
			 G_CALLBACK(row_selected), dialog);
	g_signal_connect(G_OBJECT(folderlist), "unselect-row",
			 G_CALLBACK(row_unselected), dialog);
	g_signal_connect(G_OBJECT(folderlist), "row-move",
			 G_CALLBACK(row_moved), dialog);

	dialog->rows = 0;
	dialog->selected = -1;
	for (flist = folder_get_list(); flist != NULL; flist = g_list_next(flist)) {
		Folder *folder = flist->data;
		int row;
		gchar *text[1];

		text[0] = folder->name;
		row = gtk_clist_append(GTK_CLIST(folderlist), text);
		gtk_clist_set_row_data(GTK_CLIST(folderlist), row, folder);
		dialog->rows++;
	}

	inc_lock();
}
