/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2001 Hiroyuki Yamamoto
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "intl.h"
#include "defs.h"
#include "folder.h"
#include "prefs_folder_item.h"
#include "summaryview.h"
#include "prefs.h"
#include "manage_window.h"

PrefsFolderItem tmp_prefs;

struct PrefsFolderItemDialog
{
	FolderItem *item;
	GtkWidget *window;
	GtkWidget *checkbtn_request_return_receipt;
	GtkWidget *checkbtn_default_to;
	GtkWidget *entry_default_to;
};

static PrefParam param[] = {
	{"sort_by_number", "FALSE", &tmp_prefs.sort_by_number, P_BOOL,
	 NULL, NULL, NULL},
	{"sort_by_size", "FALSE", &tmp_prefs.sort_by_size, P_BOOL,
	 NULL, NULL, NULL},
	{"sort_by_date", "FALSE", &tmp_prefs.sort_by_date, P_BOOL,
	 NULL, NULL, NULL},
	{"sort_by_from", "FALSE", &tmp_prefs.sort_by_from, P_BOOL,
	 NULL, NULL, NULL},
	{"sort_by_subject", "FALSE", &tmp_prefs.sort_by_subject, P_BOOL,
	 NULL, NULL, NULL},
	{"sort_by_score", "FALSE", &tmp_prefs.sort_by_score, P_BOOL,
	 NULL, NULL, NULL},
	{"sort_descending", "FALSE", &tmp_prefs.sort_descending, P_BOOL,
	 NULL, NULL, NULL},
	{"enable_thread", "TRUE", &tmp_prefs.enable_thread, P_BOOL,
	 NULL, NULL, NULL},
	{"kill_score", "-9999", &tmp_prefs.kill_score, P_INT,
	 NULL, NULL, NULL},
	{"important_score", "1", &tmp_prefs.important_score, P_INT,
	 NULL, NULL, NULL},
	{"request_return_receipt", "", &tmp_prefs.request_return_receipt, P_BOOL,
	 NULL, NULL, NULL},
	{"enable_default_to", "", &tmp_prefs.enable_default_to, P_BOOL,
	 NULL, NULL, NULL},
	{"default_to", "", &tmp_prefs.default_to, P_STRING,
	 NULL, NULL, NULL},
	{NULL, NULL, NULL, P_OTHER, NULL, NULL, NULL}
};

void prefs_folder_item_delete_cb(GtkWidget *widget, GdkEventAny *event, struct PrefsFolderItemDialog *dialog);
void prefs_folder_item_cancel_cb(GtkWidget *widget, struct PrefsFolderItemDialog *dialog);
void prefs_folder_item_ok_cb(GtkWidget *widget, struct PrefsFolderItemDialog *dialog);
void prefs_folder_item_default_to_cb(GtkWidget *widget, struct PrefsFolderItemDialog *dialog);

void prefs_folder_item_read_config(FolderItem * item)
{
	gchar * id;

	id = folder_item_get_identifier(item);

	prefs_read_config(param, id, FOLDERITEM_RC);
	g_free(id);

	* item->prefs = tmp_prefs;
}

void prefs_folder_item_save_config(FolderItem * item)
{	
	gchar * id;

	tmp_prefs = * item->prefs;

	id = folder_item_get_identifier(item);

	prefs_save_config(param, id, FOLDERITEM_RC);
	g_free(id);
}

void prefs_folder_item_set_config(FolderItem * item,
				  int sort_type, gint sort_mode)
{
	tmp_prefs = * item->prefs;

	tmp_prefs.sort_by_number = FALSE;
	tmp_prefs.sort_by_size = FALSE;
	tmp_prefs.sort_by_date = FALSE;
	tmp_prefs.sort_by_from = FALSE;
	tmp_prefs.sort_by_subject = FALSE;
	tmp_prefs.sort_by_score = FALSE;

	switch (sort_mode) {
	case SORT_BY_NUMBER:
		tmp_prefs.sort_by_number = TRUE;
		break;
	case SORT_BY_SIZE:
		tmp_prefs.sort_by_size = TRUE;
		break;
	case SORT_BY_DATE:
		tmp_prefs.sort_by_date = TRUE;
		break;
	case SORT_BY_FROM:
		tmp_prefs.sort_by_from = TRUE;
		break;
	case SORT_BY_SUBJECT:
		tmp_prefs.sort_by_subject = TRUE;
		break;
	case SORT_BY_SCORE:
		tmp_prefs.sort_by_score = TRUE;
		break;
	}
	tmp_prefs.sort_descending = (sort_type == GTK_SORT_DESCENDING);

	* item->prefs = tmp_prefs;
}

PrefsFolderItem * prefs_folder_item_new(void)
{
	PrefsFolderItem * prefs;

	prefs = g_new0(PrefsFolderItem, 1);

	tmp_prefs.sort_by_number = FALSE;
	tmp_prefs.sort_by_size = FALSE;
	tmp_prefs.sort_by_date = FALSE;
	tmp_prefs.sort_by_from = FALSE;
	tmp_prefs.sort_by_subject = FALSE;
	tmp_prefs.sort_by_score = FALSE;
	tmp_prefs.sort_descending = FALSE;
	tmp_prefs.kill_score = -9999;
	tmp_prefs.important_score = 9999;

	tmp_prefs.request_return_receipt = FALSE;
	tmp_prefs.enable_default_to = FALSE;
	tmp_prefs.default_to = NULL;

	* prefs = tmp_prefs;
	
	return prefs;
}

void prefs_folder_item_free(PrefsFolderItem * prefs)
{
	g_free(prefs->default_to);
	if (prefs->scoring != NULL)
		prefs_scoring_free(prefs->scoring);
	g_free(prefs);
}

gint prefs_folder_item_get_sort_mode(FolderItem * item)
{
	tmp_prefs = * item->prefs;

	if (tmp_prefs.sort_by_number)
		return SORT_BY_NUMBER;
	if (tmp_prefs.sort_by_size)
		return SORT_BY_SIZE;
	if (tmp_prefs.sort_by_date)
		return SORT_BY_DATE;
	if (tmp_prefs.sort_by_from)
		return SORT_BY_FROM;
	if (tmp_prefs.sort_by_subject)
		return SORT_BY_SUBJECT;
	if (tmp_prefs.sort_by_score)
		return SORT_BY_SCORE;
	return SORT_BY_NONE;
}

gint prefs_folder_item_get_sort_type(FolderItem * item)
{
	tmp_prefs = * item->prefs;

	if (tmp_prefs.sort_descending)
		return GTK_SORT_DESCENDING;
	else
		return GTK_SORT_ASCENDING;
}

void prefs_folder_item_create(FolderItem *item) {
	struct PrefsFolderItemDialog *dialog;
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;
	GtkWidget *confirm_area;
	GtkWidget *hbox;
	
	GtkWidget *checkbtn_request_return_receipt;
	GtkWidget *checkbtn_default_to;
	GtkWidget *entry_default_to;

	dialog = g_new0(struct PrefsFolderItemDialog, 1);
	dialog->item = item;

	/* Window */
	window = gtk_window_new (GTK_WINDOW_DIALOG);
	gtk_window_set_title (GTK_WINDOW(window),
			      _("Folder Property"));
	gtk_container_set_border_width (GTK_CONTAINER (window), 8);
	gtk_window_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
	gtk_window_set_policy (GTK_WINDOW (window), FALSE, TRUE, FALSE);
	gtk_signal_connect (GTK_OBJECT(window), "delete_event",
			    GTK_SIGNAL_FUNC(prefs_folder_item_delete_cb), dialog);
	gtk_signal_connect (GTK_OBJECT(window), "focus_in_event",
			    GTK_SIGNAL_FUNC(manage_window_focus_in), NULL);
	gtk_signal_connect (GTK_OBJECT(window), "focus_out_event",
			    GTK_SIGNAL_FUNC(manage_window_focus_out), NULL);

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_widget_show(vbox);
	gtk_container_add(GTK_CONTAINER (window), vbox);

	/* Ok and Cancle Buttons */
	gtkut_button_set_create(&confirm_area, &ok_btn, _("OK"),
				&cancel_btn, _("Cancel"), NULL, NULL);
	gtk_widget_show(confirm_area);
	gtk_box_pack_end(GTK_BOX(vbox), confirm_area, FALSE, FALSE, 0);
	gtk_widget_grab_default(ok_btn);
	gtk_signal_connect (GTK_OBJECT(ok_btn), "clicked",
			    GTK_SIGNAL_FUNC(prefs_folder_item_ok_cb), dialog);
	gtk_signal_connect (GTK_OBJECT(cancel_btn), "clicked",
			    GTK_SIGNAL_FUNC(prefs_folder_item_cancel_cb), dialog);

	/* Request Return Receipt */
	PACK_CHECK_BUTTON(vbox, checkbtn_request_return_receipt,
			   _("Request Return Receipt"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_request_return_receipt),
	    item->prefs->request_return_receipt);

	/* Default To */
	hbox = gtk_hbox_new(FALSE, 8);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON(hbox, checkbtn_default_to,
			   _("Default To: "));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_default_to), item->prefs->enable_default_to);
	gtk_signal_connect(GTK_OBJECT(checkbtn_default_to), "toggled",
			    GTK_SIGNAL_FUNC(prefs_folder_item_default_to_cb), dialog);

	entry_default_to = gtk_entry_new();
	gtk_widget_show(entry_default_to);
	gtk_box_pack_start(GTK_BOX(hbox), entry_default_to, FALSE, FALSE, 0);
	gtk_editable_set_editable(GTK_EDITABLE(entry_default_to), item->prefs->enable_default_to);
	gtk_entry_set_text(GTK_ENTRY(entry_default_to), item->prefs->default_to);

	dialog->window = window;
	dialog->checkbtn_request_return_receipt = checkbtn_request_return_receipt;
	dialog->checkbtn_default_to = checkbtn_default_to;
	dialog->entry_default_to = entry_default_to;

	gtk_widget_show(window);
}

void prefs_folder_item_destroy(struct PrefsFolderItemDialog *dialog) {
	gtk_widget_destroy(dialog->window);
	g_free(dialog);
}

void prefs_folder_item_cancel_cb(GtkWidget *widget, struct PrefsFolderItemDialog *dialog) {
	prefs_folder_item_destroy(dialog);
}

void prefs_folder_item_delete_cb(GtkWidget *widget, GdkEventAny *event, struct PrefsFolderItemDialog *dialog) {
	prefs_folder_item_destroy(dialog);
}

void prefs_folder_item_ok_cb(GtkWidget *widget, struct PrefsFolderItemDialog *dialog) {
	PrefsFolderItem *prefs = dialog->item->prefs;

	prefs->request_return_receipt = 
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->checkbtn_request_return_receipt));
	prefs->enable_default_to = 
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->checkbtn_default_to));
	g_free(prefs->default_to);
	prefs->default_to = 
	    gtk_editable_get_chars(GTK_EDITABLE(dialog->entry_default_to), 0, -1);

	prefs_folder_item_save_config(dialog->item);
	prefs_folder_item_destroy(dialog);
}

void prefs_folder_item_default_to_cb(GtkWidget *widget, struct PrefsFolderItemDialog *dialog) {
	gtk_editable_set_editable(GTK_EDITABLE(dialog->entry_default_to),
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->checkbtn_default_to)));
}
