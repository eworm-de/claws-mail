/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2005-2024 the Claws Mail Team and Colin Leroy
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
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#include "defs.h"

#include <stdio.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "foldersel.h"
#include "prefs_common.h"
#include "prefs_gtk.h"
#include "prefs_summary_open.h"
#include "prefs_summary_column.h"
#include "prefs_folder_column.h"

#include "gtk/menu.h"
#include "gtk/gtkutils.h"
#include "gtk/prefswindow.h"
#include "gtk/combobox.h"

#include "manage_window.h"

typedef struct _SummariesPage
{
	PrefsPage page;

	GtkWidget *window;

	GtkWidget *optmenu_folder_unread;
	GtkWidget *spinbtn_ng_abbrev_len;
	GtkWidget *checkbtn_useaddrbook;
	GtkWidget *checkbtn_show_tooltips;
	GtkWidget *checkbtn_threadsubj;
	GtkWidget *button_datefmt;
	GtkWidget *entry_datefmt;

	GtkWidget *checkbtn_reopen_last_folder;
	GtkWidget *checkbtn_startup_folder;
	GtkWidget *startup_folder_entry;
	GtkWidget *startup_folder_select;
	GtkWidget *checkbtn_always_show_msg;
	GtkWidget *checkbtn_show_on_folder_open;
	GtkWidget *checkbtn_show_on_search_results;
	GtkWidget *checkbtn_show_on_prevnext;
	GtkWidget *checkbtn_show_on_deletemove;
	GtkWidget *checkbtn_show_on_directional;
	GtkWidget *checkbtn_mark_as_read_on_newwin;
	GtkWidget *spinbtn_mark_as_read_delay;
	GtkWidget *checkbtn_immedexec;
	GtkWidget *checkbtn_ask_mark_all_read;
	GtkWidget *checkbtn_run_processingrules_mark_all_read;
	GtkWidget *checkbtn_ask_override_colorlabel;
  	GtkWidget *optmenu_sort_key;
  	GtkWidget *optmenu_sort_type;
	GtkWidget *optmenu_summaryfromshow;
	GtkWidget *optmenu_nextunreadmsgdialog;
	GtkWidget *checkbtn_folder_default_thread;
	GtkWidget *checkbtn_folder_default_thread_collapsed;
	GtkWidget *checkbtn_folder_default_hide_read_threads;
	GtkWidget *checkbtn_folder_default_hide_read_msgs;
	GtkWidget *checkbtn_folder_default_hide_del_msgs;
	GtkWidget *checkbtn_summary_col_lock;

} SummariesPage;

enum {
	DATEFMT_FMT,
	DATEFMT_TXT,
	N_DATEFMT_COLUMNS
};

static void date_format_ok_btn_clicked		(GtkButton	*button,
						 GtkWidget     **widget);
static void date_format_cancel_btn_clicked	(GtkButton	*button,
						 GtkWidget     **widget);
static gboolean date_format_key_pressed		(GtkWidget	*keywidget,
						 GdkEventKey	*event,
						 GtkWidget     **widget);
static gboolean date_format_on_delete		(GtkWidget	*dialogwidget,
						 GdkEventAny	*event,
						 GtkWidget     **widget);
static void date_format_entry_on_change		(GtkEditable	*editable,
						 GtkLabel	*example);
static void date_format_select_row	        (GtkTreeView *list_view,
						 GtkTreePath *path,
						 GtkTreeViewColumn *column,
						 GtkWidget *date_format);
static void mark_as_read_toggled		(GtkToggleButton *button,
						 GtkWidget *spinbtn);
static void always_show_msg_toggled		(GtkToggleButton *button,
						 gpointer user_data);

static void reopen_last_folder_toggled(GtkToggleButton *toggle_btn, GtkWidget *widget)
{
	gboolean is_active;

	is_active = gtk_toggle_button_get_active(toggle_btn);
	gtk_widget_set_sensitive(widget, !is_active);
	if (is_active)
		prefs_common.goto_folder_on_startup = FALSE;
}

static void startup_folder_toggled(GtkToggleButton *toggle_btn, GtkWidget *widget)
{
	gboolean is_active;

	is_active = gtk_toggle_button_get_active(toggle_btn);
	gtk_widget_set_sensitive(widget, !is_active);
	if (is_active)
		prefs_common.goto_last_folder_on_startup = FALSE;
}

static void foldersel_cb(GtkWidget *widget, gpointer data)
{
	GtkWidget *entry = (GtkWidget *) data;
	FolderItem *item;
	gchar *item_id;
	gint newpos = 0;
	
	item = foldersel_folder_sel(NULL, FOLDER_SEL_ALL, NULL, FALSE, NULL);
	if (item && (item_id = folder_item_get_identifier(item)) != NULL) {
		gtk_editable_delete_text(GTK_EDITABLE(entry), 0, -1);
		gtk_editable_insert_text(GTK_EDITABLE(entry), item_id, strlen(item_id), &newpos);
		g_free(item_id);
	}
}

static GtkWidget *date_format_create(GtkButton *button, void *data)
{
	static GtkWidget *datefmt_win = NULL;

	GtkWidget *vbox1;
	GtkWidget *scrolledwindow1;
	GtkWidget *datefmt_list_view;
	GtkWidget *table;
	GtkWidget *label1;
	GtkWidget *label2;
	GtkWidget *label3;
	GtkWidget *confirm_area;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;
	GtkWidget *datefmt_entry;
	GtkListStore *store;

	struct {
		gchar *fmt;
		gchar *txt;
	} time_format[] = {
		{ "%a", NULL },
		{ "%A", NULL },
		{ "%b", NULL },
		{ "%B", NULL },
		{ "%c", NULL },
		{ "%C", NULL },
		{ "%d", NULL },
		{ "%H", NULL },
		{ "%I", NULL },
		{ "%j", NULL },
		{ "%m", NULL },
		{ "%M", NULL },
		{ "%p", NULL },
		{ "%S", NULL },
		{ "%w", NULL },
		{ "%x", NULL },
		{ "%y", NULL },
		{ "%Y", NULL },
		{ "%Z", NULL }
	};

	gint i;
	const gint TIME_FORMAT_ELEMS =
		sizeof time_format / sizeof time_format[0];

	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;

	time_format[0].txt  = _("the abbreviated weekday name");
	time_format[1].txt  = _("the full weekday name");
	time_format[2].txt  = _("the abbreviated month name");
	time_format[3].txt  = _("the full month name");
	time_format[4].txt  = _("the preferred date and time for the current locale");
	time_format[5].txt  = _("the century number (year/100)");
	time_format[6].txt  = _("the day of the month as a decimal number");
	time_format[7].txt  = _("the hour as a decimal number using a 24-hour clock");
	time_format[8].txt  = _("the hour as a decimal number using a 12-hour clock");
	time_format[9].txt  = _("the day of the year as a decimal number");
	time_format[10].txt = _("the month as a decimal number");
	time_format[11].txt = _("the minute as a decimal number");
	time_format[12].txt = _("either AM or PM");
	time_format[13].txt = _("the second as a decimal number");
	time_format[14].txt = _("the day of the week as a decimal number");
	time_format[15].txt = _("the preferred date for the current locale");
	time_format[16].txt = _("the last two digits of a year");
	time_format[17].txt = _("the year as a decimal number");
	time_format[18].txt = _("the time zone or name or abbreviation");

	if (datefmt_win) return datefmt_win;

	store = gtk_list_store_new(N_DATEFMT_COLUMNS,
				   G_TYPE_STRING,
				   G_TYPE_STRING,
				   -1);

	for (i = 0; i < TIME_FORMAT_ELEMS; i++) {
		GtkTreeIter iter;

		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
				   DATEFMT_FMT, time_format[i].fmt,
				   DATEFMT_TXT, time_format[i].txt,
				   -1);
	}

	datefmt_win = gtkut_window_new(GTK_WINDOW_TOPLEVEL, "prefs_summaries");
	gtk_container_set_border_width(GTK_CONTAINER(datefmt_win), 8);
	gtk_window_set_title(GTK_WINDOW(datefmt_win), _("Date format"));
	gtk_window_set_position(GTK_WINDOW(datefmt_win), GTK_WIN_POS_CENTER);
	gtk_window_set_type_hint(GTK_WINDOW(datefmt_win), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_widget_set_size_request(datefmt_win, 440, 280);

	vbox1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	gtk_widget_show(vbox1);
	gtk_container_add(GTK_CONTAINER(datefmt_win), vbox1);

	scrolledwindow1 = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy
		(GTK_SCROLLED_WINDOW(scrolledwindow1),
		 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_show(scrolledwindow1);
	gtk_box_pack_start(GTK_BOX(vbox1), scrolledwindow1, TRUE, TRUE, 0);

	datefmt_list_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	g_object_unref(G_OBJECT(store));
	gtk_widget_show(datefmt_list_view);
	gtk_container_add(GTK_CONTAINER(scrolledwindow1), datefmt_list_view);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes
			(_("Specifier"), renderer, "text", DATEFMT_FMT,
			 NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(datefmt_list_view), column);
	
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes
			(_("Description"), renderer, "text", DATEFMT_TXT,
			 NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(datefmt_list_view), column);
	
	/* gtk_cmclist_set_column_width(GTK_CMCLIST(datefmt_clist), 0, 80); */
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(datefmt_list_view));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);

	g_signal_connect(G_OBJECT(datefmt_list_view), "row_activated", 
			 G_CALLBACK(date_format_select_row),
			 datefmt_win);
	
	table = gtk_grid_new();
	gtk_widget_show(table);
	gtk_box_pack_start(GTK_BOX(vbox1), table, FALSE, FALSE, 0);
	gtk_grid_set_row_spacing(GTK_GRID(table), 4);
	gtk_grid_set_column_spacing(GTK_GRID(table), 8);

	label1 = gtk_label_new(_("Date format"));
	gtk_widget_show(label1);
	gtk_label_set_justify(GTK_LABEL(label1), GTK_JUSTIFY_LEFT);
	gtk_label_set_xalign(GTK_LABEL(label1), 0.0);
	gtk_grid_attach(GTK_GRID(table), label1, 0, 0, 1, 1);

	datefmt_entry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(datefmt_entry), 256);
	gtk_widget_show(datefmt_entry);
	gtk_grid_attach(GTK_GRID(table), datefmt_entry, 1, 0, 1, 1);
	gtk_widget_set_hexpand(datefmt_entry, TRUE);
	gtk_widget_set_halign(datefmt_entry, GTK_ALIGN_FILL);

	/* we need the "sample" entry box; add it as data so callbacks can
	 * get the entry box */
	g_object_set_data(G_OBJECT(datefmt_win), "datefmt_sample",
			  datefmt_entry);

	label2 = gtk_label_new(_("Example"));
	gtk_widget_show(label2);
	gtk_label_set_justify(GTK_LABEL(label2), GTK_JUSTIFY_LEFT);
	gtk_label_set_xalign(GTK_LABEL(label2), 0.0);
	gtk_grid_attach(GTK_GRID(table), label2, 0, 1, 1, 1);

	label3 = gtk_label_new("");
	gtk_widget_show(label3);
	gtk_label_set_justify(GTK_LABEL(label3), GTK_JUSTIFY_LEFT);
	gtk_label_set_xalign(GTK_LABEL(label3), 0.0);
	gtk_grid_attach(GTK_GRID(table), label3, 1, 1, 1, 1);
	gtk_widget_set_hexpand(label3, TRUE);
	gtk_widget_set_halign(label3, GTK_ALIGN_FILL);

	gtkut_stock_button_set_create(&confirm_area, &cancel_btn, NULL, _("_Cancel"),
				      &ok_btn, NULL, _("_OK"), NULL, NULL, NULL);

	gtk_box_pack_start(GTK_BOX(vbox1), confirm_area, FALSE, FALSE, 0);
	gtk_widget_show(confirm_area);
	gtk_widget_grab_default(ok_btn);

	/* set the current format */
	gtk_entry_set_text(GTK_ENTRY(datefmt_entry), prefs_common.date_format);
	date_format_entry_on_change(GTK_EDITABLE(datefmt_entry),
				    GTK_LABEL(label3));

	g_signal_connect(G_OBJECT(ok_btn), "clicked",
			 G_CALLBACK(date_format_ok_btn_clicked),
			 &datefmt_win);
	g_signal_connect(G_OBJECT(cancel_btn), "clicked",
			 G_CALLBACK(date_format_cancel_btn_clicked),
			 &datefmt_win);
	g_signal_connect(G_OBJECT(datefmt_win), "key_press_event",
			 G_CALLBACK(date_format_key_pressed),
			 &datefmt_win);
	g_signal_connect(G_OBJECT(datefmt_win), "delete_event",
			 G_CALLBACK(date_format_on_delete),
			 &datefmt_win);
	g_signal_connect(G_OBJECT(datefmt_entry), "changed",
			 G_CALLBACK(date_format_entry_on_change),
			 label3);

	gtk_widget_show(datefmt_win);
	manage_window_set_transient(GTK_WINDOW(datefmt_win));
	gtk_window_set_modal(GTK_WINDOW(datefmt_win), TRUE);

	gtk_widget_grab_focus(ok_btn);

	return datefmt_win;
}

static void prefs_summaries_create_widget(PrefsPage *_page, GtkWindow *window, 
			       	  gpointer data)
{
	SummariesPage *prefs_summaries = (SummariesPage *) _page;
	
	GtkWidget *notebook;
	GtkWidget *hbox0, *hbox1, *hbox2;
	GtkWidget *vbox1, *vbox2, *vbox3, *vbox4;
	GtkWidget *frame_new_folders;
	GtkWidget *optmenu_folder_unread;
	GtkWidget *label_ng_abbrev;
	GtkWidget *spinbtn_ng_abbrev_len;
	GtkAdjustment *spinbtn_ng_abbrev_len_adj;
	GtkWidget *checkbtn_useaddrbook;
	GtkWidget *checkbtn_show_tooltips;
	GtkWidget *checkbtn_threadsubj;
	GtkWidget *label_datefmt;
	GtkWidget *button_datefmt;
	GtkWidget *entry_datefmt;
	GtkWidget *button_dispitem;
	GtkWidget *checkbtn_reopen_last_folder;
	GtkWidget *checkbtn_startup_folder;
	GtkWidget *startup_folder_entry;
	GtkWidget *startup_folder_select;
	GtkWidget *checkbtn_always_show_msg;
	GtkWidget *checkbtn_show_on_folder_open;
	GtkWidget *checkbtn_show_on_search_results;
	GtkWidget *checkbtn_show_on_prevnext;
	GtkWidget *checkbtn_show_on_deletemove;
	GtkWidget *checkbtn_show_on_directional;
	GtkWidget *spinbtn_mark_as_read_delay;
	GtkAdjustment *spinbtn_mark_as_read_delay_adj;
	GtkWidget *checkbtn_immedexec;
	GtkWidget *checkbtn_ask_mark_all_read;
	GtkWidget *checkbtn_run_processingrules_mark_all_read;
	GtkWidget *checkbtn_ask_override_colorlabel;
	GtkWidget *label, *label_fill;
	GtkListStore *menu;
	GtkTreeIter iter;
 	GtkWidget *optmenu_summaryfromshow;
 	GtkWidget *optmenu_nextunreadmsgdialog;
	GtkWidget *button_edit_actions;
	GtkWidget *radio_mark_as_read_on_select;
	GtkWidget *radio_mark_as_read_on_new_win;
	GtkWidget *optmenu_sort_key;
	GtkWidget *optmenu_sort_type;
	GtkWidget *checkbtn_folder_default_thread;
	GtkWidget *checkbtn_folder_default_thread_collapsed;
	GtkWidget *checkbtn_folder_default_hide_read_threads;
	GtkWidget *checkbtn_folder_default_hide_read_msgs;
	GtkWidget *checkbtn_folder_default_hide_del_msgs;
	GtkWidget *checkbtn_summary_col_lock;

	notebook = gtk_notebook_new();
	gtk_widget_show(notebook);

	vbox1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox1,
				 gtk_label_new(_("Folder list")));
	
	hbox0 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox0);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox0, FALSE, TRUE, 0);

	label = gtk_label_new(_("Displayed columns"));
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(hbox0), label, FALSE, FALSE, 0);
	button_dispitem = gtk_button_new_with_mnemonic(_("_Edit"));
	gtk_widget_show (button_dispitem);
	gtk_box_pack_start (GTK_BOX (hbox0), button_dispitem, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (button_dispitem), "clicked",
			  G_CALLBACK (prefs_folder_column_open),
			  NULL);

	hbox0 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox0);
	gtk_box_pack_start(GTK_BOX (vbox1), hbox0, FALSE, FALSE, 0);

	label = gtk_label_new (_("Display message count next to folder name"));
	gtk_widget_show (label);
	gtk_box_pack_start(GTK_BOX(hbox0), label, FALSE, FALSE, 0);

	optmenu_folder_unread = gtkut_sc_combobox_create(NULL, FALSE);
	menu = GTK_LIST_STORE(gtk_combo_box_get_model(
				GTK_COMBO_BOX(optmenu_folder_unread)));
	gtk_widget_show (optmenu_folder_unread);
 	
	COMBOBOX_ADD (menu, _("No"), 0);
	COMBOBOX_ADD (menu, _("Unread messages"), 1);
	COMBOBOX_ADD (menu, _("Unread and Total messages"), 2);

	gtk_box_pack_start(GTK_BOX(hbox0), optmenu_folder_unread, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON
		(vbox1, checkbtn_reopen_last_folder,
		 _("Open last opened folder at start-up"));

	hbox0 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show(hbox0);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox0, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON
		(hbox0, checkbtn_startup_folder,
		 _("Open selected folder at start-up"));

	startup_folder_entry = gtk_entry_new();
	gtk_widget_show(startup_folder_entry);
	gtk_box_pack_start(GTK_BOX(hbox0), startup_folder_entry, TRUE, TRUE, 0);
	startup_folder_select = gtkut_get_browse_directory_btn(_("_Browse"));
	gtk_widget_show(startup_folder_select);
	gtk_box_pack_start(GTK_BOX(hbox0), startup_folder_select, FALSE, FALSE, 0);

	SET_TOGGLE_SENSITIVITY(checkbtn_startup_folder, startup_folder_entry)
	SET_TOGGLE_SENSITIVITY(checkbtn_startup_folder, startup_folder_select)

	g_signal_connect(G_OBJECT(checkbtn_reopen_last_folder), "toggled",
			 G_CALLBACK(reopen_last_folder_toggled), checkbtn_startup_folder);
	g_signal_connect(G_OBJECT(checkbtn_startup_folder), "toggled",
			 G_CALLBACK(startup_folder_toggled), checkbtn_reopen_last_folder);
	g_signal_connect(G_OBJECT(startup_folder_select), "clicked",
			 G_CALLBACK(foldersel_cb), startup_folder_entry);

	PACK_CHECK_BUTTON
		(vbox1, checkbtn_run_processingrules_mark_all_read,
		 _("Run processing rules before marking all messages in a folder as read or unread"));

	hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start(GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);

	label_ng_abbrev = gtk_label_new
		(_("Abbreviate newsgroup names longer than"));
	gtk_widget_show (label_ng_abbrev);
	gtk_box_pack_start (GTK_BOX (hbox1), label_ng_abbrev, FALSE, FALSE, 0);

	spinbtn_ng_abbrev_len_adj = GTK_ADJUSTMENT(gtk_adjustment_new (16, 0, 999, 1, 10, 0));
	spinbtn_ng_abbrev_len = gtk_spin_button_new
		(GTK_ADJUSTMENT (spinbtn_ng_abbrev_len_adj), 1, 0);
	gtk_widget_show (spinbtn_ng_abbrev_len);
	gtk_box_pack_start (GTK_BOX (hbox1), spinbtn_ng_abbrev_len,
			    FALSE, FALSE, 0);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbtn_ng_abbrev_len),
				     TRUE);

	label_ng_abbrev = gtk_label_new (_("letters"));
	gtk_widget_show (label_ng_abbrev);
	gtk_box_pack_start (GTK_BOX (hbox1), label_ng_abbrev, FALSE, FALSE, 0);

	vbox1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox1,
				 gtk_label_new(_("Message list")));

	hbox0 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox0);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox0, FALSE, TRUE, 0);
	
	label = gtk_label_new(_("Displayed columns"));
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(hbox0), label, FALSE, FALSE, 0);
	button_dispitem = gtk_button_new_with_mnemonic(_("_Edit"));
	gtk_widget_show (button_dispitem);
	gtk_box_pack_start (GTK_BOX (hbox0), button_dispitem, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (button_dispitem), "clicked",
			  G_CALLBACK (prefs_summary_column_open),
			  NULL);

	PACK_SPACER(hbox0, hbox1, 4);
	PACK_CHECK_BUTTON(hbox0, checkbtn_summary_col_lock, _("Lock column headers"));

	hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);

	label = gtk_label_new (_("Displayed in From column"));
	gtk_widget_show (label);
	gtk_box_pack_start(GTK_BOX(hbox1), label, FALSE, FALSE, 0);

	optmenu_summaryfromshow = gtkut_sc_combobox_create(NULL, FALSE);
	menu = GTK_LIST_STORE(gtk_combo_box_get_model(
				GTK_COMBO_BOX(optmenu_summaryfromshow)));
	gtk_widget_show (optmenu_summaryfromshow);

	COMBOBOX_ADD (menu, _("Name"), SHOW_NAME);
	COMBOBOX_ADD (menu, _("Address"), SHOW_ADDR);
	COMBOBOX_ADD (menu, _("Name and Address"), SHOW_BOTH);

	gtk_box_pack_start(GTK_BOX(hbox1), optmenu_summaryfromshow, FALSE, FALSE, 0);
	hbox2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox2, FALSE, TRUE, 0);

	label_datefmt = gtk_label_new (_("Date format"));
	gtk_widget_show (label_datefmt);
	gtk_box_pack_start (GTK_BOX (hbox2), label_datefmt, FALSE, FALSE, 0);

	entry_datefmt = gtk_entry_new ();
	gtk_widget_show (entry_datefmt);
	gtk_box_pack_start (GTK_BOX (hbox2), entry_datefmt, FALSE, FALSE, 0);

	button_datefmt = gtkut_stock_button("dialog-information", _("_Information"));

	gtk_widget_show (button_datefmt);
	gtk_box_pack_start (GTK_BOX (hbox2), button_datefmt, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (button_datefmt), "clicked",
			  G_CALLBACK (date_format_create), NULL);

	label_fill = gtk_label_new(" ");
	gtk_box_pack_start(GTK_BOX(hbox2), label_fill, TRUE, FALSE, 0);

	CLAWS_SET_TIP(button_datefmt,
			     _("Date format help"));

	hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, TRUE, 0);

	button_edit_actions = gtk_button_new_with_label(_("Set message selection when entering a folder"));
	gtk_widget_show (button_edit_actions);
	gtk_box_pack_start (GTK_BOX (hbox1), button_edit_actions,
			  FALSE, TRUE, 0);
	g_signal_connect (G_OBJECT (button_edit_actions), "clicked",
			  G_CALLBACK (prefs_summary_open_open),
			  NULL);

	/* Open message on select policy */
	vbox4 = gtkut_get_options_frame(vbox1, NULL, _("Open message when selected"));

	PACK_CHECK_BUTTON(vbox4, checkbtn_always_show_msg,
			_("Always"));
	PACK_CHECK_BUTTON(vbox4, checkbtn_show_on_folder_open,
			_("When opening a folder"));
	PACK_CHECK_BUTTON(vbox4, checkbtn_show_on_search_results,
			_("When displaying search results"));
	PACK_CHECK_BUTTON(vbox4, checkbtn_show_on_prevnext,
			_("When selecting next or previous message using shortcuts"));
	PACK_CHECK_BUTTON(vbox4, checkbtn_show_on_deletemove,
			_("When deleting or moving messages"));
	PACK_CHECK_BUTTON(vbox4, checkbtn_show_on_directional,
			_("When using directional keys"));

	vbox3 = gtkut_get_options_frame(vbox1, NULL, _("Mark message as read"));

	radio_mark_as_read_on_select = gtk_radio_button_new_with_label(NULL,
			_("when selected, after"));

	hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_box_pack_start (GTK_BOX (hbox1), radio_mark_as_read_on_select, FALSE, FALSE, 0);

	spinbtn_mark_as_read_delay_adj = GTK_ADJUSTMENT(gtk_adjustment_new (0, 0, 60, 1, 10, 0));
	spinbtn_mark_as_read_delay = gtk_spin_button_new
			(GTK_ADJUSTMENT (spinbtn_mark_as_read_delay_adj), 1, 0);
	gtk_box_pack_start (GTK_BOX (hbox1), spinbtn_mark_as_read_delay,
			    FALSE, FALSE, 0);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbtn_mark_as_read_delay),
				     TRUE);
	gtk_box_pack_start (GTK_BOX (hbox1), gtk_label_new
			(_("seconds")), FALSE, FALSE, 0);

	gtk_box_pack_start (GTK_BOX (vbox3), hbox1, FALSE, FALSE, 0);

	radio_mark_as_read_on_new_win = gtk_radio_button_new_with_label_from_widget(
			GTK_RADIO_BUTTON(radio_mark_as_read_on_select),
			_("only when opened in a new window, or replied to"));
	gtk_box_pack_start (GTK_BOX (vbox3), radio_mark_as_read_on_new_win,
			FALSE, FALSE, 0);
	gtk_widget_show_all(vbox3);

	/* Next Unread Message Dialog */
	hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);

	label = gtk_label_new (_("Show \"no unread (or new) message\" dialog"));
	gtk_widget_show (label);
	gtk_box_pack_start(GTK_BOX(hbox1), label, FALSE, FALSE, 0);
	
	optmenu_nextunreadmsgdialog = gtkut_sc_combobox_create(NULL, FALSE);
	menu = GTK_LIST_STORE(gtk_combo_box_get_model(
				GTK_COMBO_BOX(optmenu_nextunreadmsgdialog)));
	gtk_widget_show (optmenu_nextunreadmsgdialog);

	COMBOBOX_ADD (menu, _("Always"), NEXTUNREADMSGDIALOG_ALWAYS);
	COMBOBOX_ADD (menu, _("Assume 'Yes'"), NEXTUNREADMSGDIALOG_ASSUME_YES);
	COMBOBOX_ADD (menu, _("Assume 'No'"), NEXTUNREADMSGDIALOG_ASSUME_NO);

	gtk_box_pack_start(GTK_BOX(hbox1), optmenu_nextunreadmsgdialog, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON
		(vbox1, checkbtn_useaddrbook,
		 _("Display sender using address book"));

	PACK_CHECK_BUTTON
		(vbox1, checkbtn_threadsubj,
		 _("Thread using subject in addition to standard headers"));

	PACK_CHECK_BUTTON
		(vbox1, checkbtn_immedexec,
		 _("Execute immediately when moving or deleting messages"));
	CLAWS_SET_TIP(checkbtn_immedexec,
			     _("When unchecked moving, copying and deleting of messages"
			       " is deferred until you use '/Tools/Execute'"));

	PACK_CHECK_BUTTON
		(vbox1, checkbtn_ask_mark_all_read,
		 _("Confirm when marking all messages as read or unread"));
	PACK_CHECK_BUTTON
		(vbox1, checkbtn_ask_override_colorlabel,
		 _("Confirm when changing color labels"));
	
	PACK_CHECK_BUTTON
		(vbox1, checkbtn_show_tooltips,
		 _("Show tooltips"));

	hbox2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox2, FALSE, FALSE, 0);

	vbox1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox1,
				 gtk_label_new(_("Defaults")));

	vbox2 = gtkut_get_options_frame(vbox1, &frame_new_folders, _("New folders"));

	hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
	gtk_widget_show(hbox1);
	gtk_box_pack_start(GTK_BOX (vbox2), hbox1, FALSE, FALSE, 0);

	label = gtk_label_new(_("Sort by"));
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(hbox1), label, FALSE, FALSE, 0);

	optmenu_sort_key = gtkut_sc_combobox_create(NULL, FALSE);
	menu = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(optmenu_sort_key)));
	gtk_widget_show(optmenu_sort_key);

	COMBOBOX_ADD(menu, _("Number"), SORT_BY_NUMBER);
	COMBOBOX_ADD(menu, _("Size"), SORT_BY_SIZE);
	COMBOBOX_ADD(menu, _("Date"), SORT_BY_DATE);
	COMBOBOX_ADD(menu, _("Thread date"), SORT_BY_THREAD_DATE);
	COMBOBOX_ADD(menu, _("From"), SORT_BY_FROM);
	COMBOBOX_ADD(menu, _("To"), SORT_BY_TO);
	COMBOBOX_ADD(menu, _("Subject"), SORT_BY_SUBJECT);
	COMBOBOX_ADD(menu, _("Color label"), SORT_BY_LABEL);
	COMBOBOX_ADD(menu, _("Tag"), SORT_BY_TAGS);
	COMBOBOX_ADD(menu, _("Mark"), SORT_BY_MARK);
	COMBOBOX_ADD(menu, _("Status"), SORT_BY_STATUS);
	COMBOBOX_ADD(menu, _("Attachment"), SORT_BY_MIME);
	COMBOBOX_ADD(menu, _("Score"), SORT_BY_SCORE);
	COMBOBOX_ADD(menu, _("Locked"), SORT_BY_LOCKED);
	COMBOBOX_ADD(menu, _("Don't sort"), SORT_BY_NONE);

	gtk_box_pack_start(GTK_BOX(hbox1), optmenu_sort_key, FALSE, FALSE, 0);

	optmenu_sort_type = gtkut_sc_combobox_create(NULL, FALSE);
	menu = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(optmenu_sort_type)));
	gtk_widget_show(optmenu_sort_type);

	COMBOBOX_ADD(menu, _("Ascending"), SORT_ASCENDING);
	COMBOBOX_ADD(menu, _("Descending"), SORT_DESCENDING);

	gtk_box_pack_start(GTK_BOX(hbox1), optmenu_sort_type, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON
		(vbox2, checkbtn_folder_default_thread,
		 _("Thread view"));
	PACK_CHECK_BUTTON
		(vbox2, checkbtn_folder_default_thread_collapsed,
		 _("Collapse all threads"));
	PACK_CHECK_BUTTON
		(vbox2, checkbtn_folder_default_hide_read_threads,
		 _("Hide read threads"));
	PACK_CHECK_BUTTON
		(vbox2, checkbtn_folder_default_hide_read_msgs,
		 _("Hide read messages"));
	PACK_CHECK_BUTTON
		(vbox2, checkbtn_folder_default_hide_del_msgs,
		 _("Hide deleted messages"));

	prefs_summaries->optmenu_folder_unread = optmenu_folder_unread;
	prefs_summaries->spinbtn_ng_abbrev_len = spinbtn_ng_abbrev_len;
	prefs_summaries->checkbtn_useaddrbook = checkbtn_useaddrbook;
	prefs_summaries->checkbtn_show_tooltips = checkbtn_show_tooltips;
	prefs_summaries->checkbtn_threadsubj = checkbtn_threadsubj;
	prefs_summaries->entry_datefmt = entry_datefmt;
	prefs_summaries->checkbtn_reopen_last_folder = checkbtn_reopen_last_folder;
	prefs_summaries->checkbtn_startup_folder = checkbtn_startup_folder;
	prefs_summaries->startup_folder_entry = startup_folder_entry;

	prefs_summaries->checkbtn_always_show_msg = checkbtn_always_show_msg;
	prefs_summaries->checkbtn_show_on_folder_open = checkbtn_show_on_folder_open;
	prefs_summaries->checkbtn_show_on_search_results = checkbtn_show_on_search_results;
	prefs_summaries->checkbtn_show_on_prevnext = checkbtn_show_on_prevnext;
	prefs_summaries->checkbtn_show_on_deletemove = checkbtn_show_on_deletemove;
	prefs_summaries->checkbtn_show_on_directional = checkbtn_show_on_directional;

	prefs_summaries->checkbtn_mark_as_read_on_newwin = radio_mark_as_read_on_new_win;
	prefs_summaries->spinbtn_mark_as_read_delay = spinbtn_mark_as_read_delay;
	prefs_summaries->checkbtn_immedexec = checkbtn_immedexec;
	prefs_summaries->checkbtn_ask_mark_all_read = checkbtn_ask_mark_all_read;
	prefs_summaries->checkbtn_run_processingrules_mark_all_read = checkbtn_run_processingrules_mark_all_read;
	prefs_summaries->checkbtn_ask_override_colorlabel = checkbtn_ask_override_colorlabel;
	prefs_summaries->optmenu_sort_key = optmenu_sort_key;
	prefs_summaries->optmenu_sort_type = optmenu_sort_type;
	prefs_summaries->optmenu_nextunreadmsgdialog = optmenu_nextunreadmsgdialog;
	prefs_summaries->optmenu_summaryfromshow = optmenu_summaryfromshow;

	prefs_summaries->checkbtn_folder_default_thread = checkbtn_folder_default_thread;
	prefs_summaries->checkbtn_folder_default_thread_collapsed = checkbtn_folder_default_thread_collapsed;
	prefs_summaries->checkbtn_folder_default_hide_read_threads = checkbtn_folder_default_hide_read_threads;
	prefs_summaries->checkbtn_folder_default_hide_read_msgs = checkbtn_folder_default_hide_read_msgs;
	prefs_summaries->checkbtn_folder_default_hide_del_msgs = checkbtn_folder_default_hide_del_msgs;
	prefs_summaries->checkbtn_summary_col_lock = checkbtn_summary_col_lock;

	prefs_summaries->page.widget = vbox1;
	g_signal_connect(G_OBJECT(checkbtn_always_show_msg), "toggled",
			G_CALLBACK(always_show_msg_toggled), prefs_summaries);

	g_signal_connect(G_OBJECT(radio_mark_as_read_on_select), "toggled",
			 G_CALLBACK(mark_as_read_toggled),
			 spinbtn_mark_as_read_delay);

	prefs_summaries->window			= GTK_WIDGET(window);
	
	combobox_select_by_data(GTK_COMBO_BOX(optmenu_folder_unread),
			prefs_common.display_folder_unread);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_useaddrbook),
			prefs_common.use_addr_book);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_show_tooltips),
			prefs_common.show_tooltips);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_threadsubj),
			prefs_common.thread_by_subject);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbtn_ng_abbrev_len),
			prefs_common.ng_abbrev_len);
	gtk_entry_set_text(GTK_ENTRY(entry_datefmt), 
			prefs_common.date_format?prefs_common.date_format:"");	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_reopen_last_folder),
			prefs_common.goto_last_folder_on_startup);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_startup_folder),
			prefs_common.goto_folder_on_startup);
	gtk_entry_set_text(GTK_ENTRY(startup_folder_entry),
			   prefs_common.startup_folder?prefs_common.startup_folder:"");

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_always_show_msg),
			prefs_common.always_show_msg);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_show_on_folder_open),
			prefs_common.open_selected_on_folder_open);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_show_on_search_results),
			prefs_common.open_selected_on_search_results);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_show_on_prevnext),
			prefs_common.open_selected_on_prevnext);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_show_on_deletemove),
			prefs_common.open_selected_on_deletemove);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_show_on_directional),
			prefs_common.open_selected_on_directional);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_mark_as_read_on_new_win),
			prefs_common.mark_as_read_on_new_window);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbtn_mark_as_read_delay),
			prefs_common.mark_as_read_delay);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_immedexec),
			prefs_common.immediate_exec);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_ask_mark_all_read),
			prefs_common.ask_mark_all_read);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_run_processingrules_mark_all_read),
			prefs_common.run_processingrules_before_mark_all);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_ask_override_colorlabel),
			prefs_common.ask_override_colorlabel);

	combobox_select_by_data(GTK_COMBO_BOX(optmenu_sort_key),
			prefs_common.default_sort_key);
	combobox_select_by_data(GTK_COMBO_BOX(optmenu_sort_type),
			prefs_common.default_sort_type);

	combobox_select_by_data(GTK_COMBO_BOX(optmenu_summaryfromshow),
			prefs_common.summary_from_show);

	combobox_select_by_data(GTK_COMBO_BOX(optmenu_nextunreadmsgdialog),
			prefs_common.next_unread_msg_dialog);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_folder_default_thread),
			prefs_common.folder_default_thread);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_folder_default_thread_collapsed),
			prefs_common.folder_default_thread_collapsed);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_folder_default_hide_read_threads),
			prefs_common.folder_default_hide_read_threads);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_folder_default_hide_read_msgs),
			prefs_common.folder_default_hide_read_msgs);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_folder_default_hide_del_msgs),
			prefs_common.folder_default_hide_del_msgs);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_summary_col_lock),
			prefs_common.summary_col_lock);

	gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), 0);
		
	prefs_summaries->page.widget = notebook;
}

static void prefs_summaries_save(PrefsPage *_page)
{
	SummariesPage *page = (SummariesPage *) _page;

	prefs_common.display_folder_unread = combobox_get_active_data(
			GTK_COMBO_BOX(page->optmenu_folder_unread));

	prefs_common.use_addr_book = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(page->checkbtn_useaddrbook));
	prefs_common.show_tooltips = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(page->checkbtn_show_tooltips));
	prefs_common.thread_by_subject = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(page->checkbtn_threadsubj));
	prefs_common.ng_abbrev_len = gtk_spin_button_get_value_as_int(
			GTK_SPIN_BUTTON(page->spinbtn_ng_abbrev_len));
	
	g_free(prefs_common.date_format); 
	prefs_common.date_format = gtk_editable_get_chars(
			GTK_EDITABLE(page->entry_datefmt), 0, -1);	

	prefs_common.goto_last_folder_on_startup = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->checkbtn_reopen_last_folder));
	prefs_common.goto_folder_on_startup = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->checkbtn_startup_folder));
	prefs_common.startup_folder = gtk_editable_get_chars(
			GTK_EDITABLE(page->startup_folder_entry), 0, -1);

	prefs_common.always_show_msg = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->checkbtn_always_show_msg));
	prefs_common.open_selected_on_folder_open = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->checkbtn_show_on_folder_open));
	prefs_common.open_selected_on_search_results = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->checkbtn_show_on_search_results));
	prefs_common.open_selected_on_prevnext = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->checkbtn_show_on_prevnext));
	prefs_common.open_selected_on_deletemove = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->checkbtn_show_on_deletemove));
	prefs_common.open_selected_on_directional = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->checkbtn_show_on_directional));

	prefs_common.mark_as_read_on_new_window = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->checkbtn_mark_as_read_on_newwin));
	prefs_common.immediate_exec = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->checkbtn_immedexec));
	prefs_common.ask_mark_all_read = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->checkbtn_ask_mark_all_read));
	prefs_common.run_processingrules_before_mark_all = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->checkbtn_run_processingrules_mark_all_read));
	prefs_common.ask_override_colorlabel = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->checkbtn_ask_override_colorlabel));
	prefs_common.mark_as_read_delay = gtk_spin_button_get_value_as_int(
			GTK_SPIN_BUTTON(page->spinbtn_mark_as_read_delay));
	prefs_common.summary_col_lock = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->checkbtn_summary_col_lock));

	prefs_common.default_sort_key = combobox_get_active_data(
			GTK_COMBO_BOX(page->optmenu_sort_key));
	prefs_common.default_sort_type = combobox_get_active_data(
			GTK_COMBO_BOX(page->optmenu_sort_type));
	prefs_common.summary_from_show = combobox_get_active_data(
			GTK_COMBO_BOX(page->optmenu_summaryfromshow));
	prefs_common.next_unread_msg_dialog = combobox_get_active_data(
			GTK_COMBO_BOX(page->optmenu_nextunreadmsgdialog));
	prefs_common.folder_default_thread =  gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(page->checkbtn_folder_default_thread));
	prefs_common.folder_default_thread_collapsed =  gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(page->checkbtn_folder_default_thread_collapsed));
	prefs_common.folder_default_hide_read_threads =  gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(page->checkbtn_folder_default_hide_read_threads));
	prefs_common.folder_default_hide_read_msgs =  gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(page->checkbtn_folder_default_hide_read_msgs));
	prefs_common.folder_default_hide_del_msgs =  gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(page->checkbtn_folder_default_hide_del_msgs));
	prefs_common.folder_default_hide_del_msgs =  gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(page->checkbtn_folder_default_hide_del_msgs));

	main_window_reflect_prefs_all();
}

static void prefs_summaries_destroy_widget(PrefsPage *_page)
{
}

SummariesPage *prefs_summaries;

void prefs_summaries_init(void)
{
	SummariesPage *page;
	static gchar *path[3];

	path[0] = _("Display");
	path[1] = _("Summaries");
	path[2] = NULL;

	page = g_new0(SummariesPage, 1);
	page->page.path = path;
	page->page.create_widget = prefs_summaries_create_widget;
	page->page.destroy_widget = prefs_summaries_destroy_widget;
	page->page.save_page = prefs_summaries_save;
	page->page.weight = 140.0;
	prefs_gtk_register_page((PrefsPage *) page);
	prefs_summaries = page;
}

void prefs_summaries_done(void)
{
	prefs_gtk_unregister_page((PrefsPage *) prefs_summaries);
	g_free(prefs_summaries);
}

static void date_format_ok_btn_clicked(GtkButton *button, GtkWidget **widget)
{
	GtkWidget *datefmt_sample = NULL;
	gchar *text;

	cm_return_if_fail(widget != NULL);
	cm_return_if_fail(*widget != NULL);
	cm_return_if_fail(prefs_summaries->entry_datefmt != NULL);

	datefmt_sample = GTK_WIDGET(g_object_get_data
				    (G_OBJECT(*widget), "datefmt_sample"));
	cm_return_if_fail(datefmt_sample != NULL);

	text = gtk_editable_get_chars(GTK_EDITABLE(datefmt_sample), 0, -1);
	g_free(prefs_common.date_format);
	prefs_common.date_format = text;
	gtk_entry_set_text(GTK_ENTRY(prefs_summaries->entry_datefmt), text);

	gtk_widget_destroy(*widget);
	*widget = NULL;
}

static void date_format_cancel_btn_clicked(GtkButton *button,
					   GtkWidget **widget)
{
	cm_return_if_fail(widget != NULL);
	cm_return_if_fail(*widget != NULL);

	gtk_widget_destroy(*widget);
	*widget = NULL;
}

static gboolean date_format_key_pressed(GtkWidget *keywidget, GdkEventKey *event,
					GtkWidget **widget)
{
	if (event && event->keyval == GDK_KEY_Escape)
		date_format_cancel_btn_clicked(NULL, widget);
	return FALSE;
}

static gboolean date_format_on_delete(GtkWidget *dialogwidget,
				      GdkEventAny *event, GtkWidget **widget)
{
	cm_return_val_if_fail(widget != NULL, FALSE);
	cm_return_val_if_fail(*widget != NULL, FALSE);

	*widget = NULL;
	return FALSE;
}

static void date_format_entry_on_change(GtkEditable *editable,
					GtkLabel *example)
{
	time_t cur_time;
	struct tm *cal_time;
	gchar buffer[100];
	gchar *text;
	struct tm lt;

	cur_time = time(NULL);
	cal_time = localtime_r(&cur_time, &lt);
	buffer[0] = 0;
	text = gtk_editable_get_chars(editable, 0, -1);
	if (text)
		fast_strftime(buffer, sizeof buffer, text, cal_time); 

	gtk_label_set_text(example, buffer);

	g_free(text);
}

static void date_format_select_row(GtkTreeView *list_view,
				   GtkTreePath *path,
				   GtkTreeViewColumn *column,
				   GtkWidget *date_format)
{
	gint cur_pos;
	gchar *format;
	const gchar *old_format;
	gchar *new_format;
	GtkWidget *datefmt_sample;
	GtkTreeIter iter;
	GtkTreeModel *model;
	
	cm_return_if_fail(date_format != NULL);

	/* only on double click */
	datefmt_sample = GTK_WIDGET(g_object_get_data(G_OBJECT(date_format), 
						      "datefmt_sample"));

	cm_return_if_fail(datefmt_sample != NULL);

	model = gtk_tree_view_get_model(list_view);

	/* get format from list */
	if (!gtk_tree_model_get_iter(model, &iter, path))
		return;

	gtk_tree_model_get(model, &iter, DATEFMT_FMT, &format, -1);		
	
	cur_pos = gtk_editable_get_position(GTK_EDITABLE(datefmt_sample));
	old_format = gtk_entry_get_text(GTK_ENTRY(datefmt_sample));

	/* insert the format into the text entry */
	new_format = g_malloc(strlen(old_format) + 3);

	strncpy(new_format, old_format, cur_pos);
	new_format[cur_pos] = '\0';
	strcat(new_format, format);
	g_free(format);
	strcat(new_format, &old_format[cur_pos]);

	gtk_entry_set_text(GTK_ENTRY(datefmt_sample), new_format);
	gtk_editable_set_position(GTK_EDITABLE(datefmt_sample), cur_pos + 2);

	g_free(new_format);
}

static void always_show_msg_toggled(GtkToggleButton *button,
		gpointer user_data)
{
	const SummariesPage *prefs_summaries = (SummariesPage *)user_data;
	gboolean state;

	cm_return_if_fail(prefs_summaries != NULL);

	state = gtk_toggle_button_get_active(button);

	gtk_widget_set_sensitive(prefs_summaries->checkbtn_show_on_folder_open, !state);
	gtk_widget_set_sensitive(prefs_summaries->checkbtn_show_on_search_results, !state);
	gtk_widget_set_sensitive(prefs_summaries->checkbtn_show_on_prevnext, !state);
	gtk_widget_set_sensitive(prefs_summaries->checkbtn_show_on_deletemove, !state);
	gtk_widget_set_sensitive(prefs_summaries->checkbtn_show_on_directional, !state);
}

static void mark_as_read_toggled(GtkToggleButton *button, GtkWidget *spinbtn)
{
       gtk_widget_set_sensitive(spinbtn,
               gtk_toggle_button_get_active(button));
}
 
