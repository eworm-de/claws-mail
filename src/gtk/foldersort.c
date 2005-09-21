/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2004 Hiroyuki Yamamoto and the Sylpheed-Claws Team
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

void foldersort_open()
{
	FolderSortDialog *dialog = g_new0(FolderSortDialog, 1);
	GList *flist;

	/* BEGIN GLADE CODE */

	GtkWidget *window;
	GtkWidget *table1;
	GtkWidget *label1;
	GtkWidget *hbuttonbox1;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;
	GtkWidget *vbox1;
	GtkWidget *moveup_btn;
	GtkWidget *movedown_btn;
	GtkWidget *scrolledwindow1;
	GtkWidget *folderlist;
	GtkWidget *label2;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_object_set_data(G_OBJECT(window), "window", window);
	gtk_container_set_border_width(GTK_CONTAINER(window), 4);
	gtk_window_set_title(GTK_WINDOW(window),
			     _("Set folder sortorder"));
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	gtk_window_set_default_size(GTK_WINDOW(window), 400, 300);

	table1 = gtk_table_new(3, 2, FALSE);
	gtk_widget_show(table1);
	gtk_container_add(GTK_CONTAINER(window), table1);
	gtk_table_set_row_spacings(GTK_TABLE(table1), 4);
	gtk_table_set_col_spacings(GTK_TABLE(table1), 4);

	label1 =
	    gtk_label_new(_
			  ("Move folders up or down to change\nthe sort order in the folderview"));
	gtk_widget_show(label1);
	gtk_table_attach(GTK_TABLE(table1), label1, 0, 1, 0, 1,
			 (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);
	gtk_label_set_justify(GTK_LABEL(label1), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(label1), 0, 0.5);

	hbuttonbox1 = gtk_hbutton_box_new();
	gtk_widget_show(hbuttonbox1);
	gtk_table_attach(GTK_TABLE(table1), hbuttonbox1, 0, 1, 2, 3,
			 (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(hbuttonbox1),
				  GTK_BUTTONBOX_END);
	gtk_button_box_set_spacing(GTK_BUTTON_BOX(hbuttonbox1), 0);
	gtk_button_box_set_child_size(GTK_BUTTON_BOX(hbuttonbox1), 0, 0);
	gtk_button_box_set_child_ipadding(GTK_BUTTON_BOX(hbuttonbox1), 0,
					  0);

	ok_btn = gtk_button_new_from_stock(GTK_STOCK_OK);
	gtk_widget_show(ok_btn);
	gtk_container_add(GTK_CONTAINER(hbuttonbox1), ok_btn);
	GTK_WIDGET_SET_FLAGS(ok_btn, GTK_CAN_DEFAULT);

	cancel_btn = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
	gtk_widget_show(cancel_btn);
	gtk_container_add(GTK_CONTAINER(hbuttonbox1), cancel_btn);
	GTK_WIDGET_SET_FLAGS(cancel_btn, GTK_CAN_DEFAULT);

	vbox1 = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(vbox1);
	gtk_table_attach(GTK_TABLE(table1), vbox1, 1, 2, 1, 2,
			 (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (GTK_FILL), 0, 0);

	moveup_btn = gtk_button_new_from_stock(GTK_STOCK_GO_UP);
	gtk_widget_show(moveup_btn);
	gtk_box_pack_start(GTK_BOX(vbox1), moveup_btn, FALSE, FALSE, 0);

	movedown_btn =  gtk_button_new_from_stock(GTK_STOCK_GO_DOWN);
	gtk_widget_show(movedown_btn);
	gtk_box_pack_start(GTK_BOX(vbox1), movedown_btn, FALSE, FALSE, 0);

	scrolledwindow1 = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(scrolledwindow1);
	gtk_table_attach(GTK_TABLE(table1), scrolledwindow1, 0, 1, 1, 2,
			 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW
				       (scrolledwindow1),
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

	/* END GLADE CODE */

	dialog->window = window;
	dialog->moveup_btn = moveup_btn;
	dialog->movedown_btn = movedown_btn;
	dialog->folderlist = folderlist;

	gtk_widget_show(window);
	gtk_widget_set_sensitive(moveup_btn, FALSE);
	gtk_widget_set_sensitive(movedown_btn, FALSE);
	gtk_clist_set_reorderable(GTK_CLIST(folderlist), TRUE);

	g_signal_connect(G_OBJECT(ok_btn), "clicked",
                         G_CALLBACK(ok_clicked), dialog);
	g_signal_connect(G_OBJECT(cancel_btn), "clicked",
                         G_CALLBACK(cancel_clicked), dialog);
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
