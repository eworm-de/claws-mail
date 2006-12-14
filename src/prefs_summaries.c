/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2005-2006 Colin Leroy <colin@colino.net> & The Claws Mail Team
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

#include "defs.h"

#include <stdio.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "prefs_common.h"
#include "prefs_gtk.h"
#include "prefs_summary_column.h"
#include "prefs_folder_column.h"

#include "gtk/menu.h"
#include "gtk/gtkutils.h"
#include "gtk/prefswindow.h"

#include "manage_window.h"

typedef struct _SummariesPage
{
	PrefsPage page;

	GtkWidget *window;

	GtkWidget *chkbtn_transhdr;
	GtkWidget *chkbtn_folder_unread;
	GtkWidget *spinbtn_ng_abbrev_len;
	GtkWidget *chkbtn_useaddrbook;
	GtkWidget *chkbtn_threadsubj;
	GtkWidget *button_datefmt;
	GtkWidget *entry_datefmt;

	GtkWidget *checkbtn_always_show_msg;
	GtkWidget *checkbtn_mark_as_read_on_newwin;
	GtkWidget *spinbtn_mark_as_read_delay;
	GtkWidget *checkbtn_immedexec;
	GtkWidget *checkbtn_ask_mark_all_read;
 	GtkWidget *optmenu_select_on_entry;
 	GtkWidget *optmenu_nextunreadmsgdialog;

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

	time_format[0].txt  = _("the full abbreviated weekday name");
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

	datefmt_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_container_set_border_width(GTK_CONTAINER(datefmt_win), 8);
	gtk_window_set_title(GTK_WINDOW(datefmt_win), _("Date format"));
	gtk_window_set_position(GTK_WINDOW(datefmt_win), GTK_WIN_POS_CENTER);
	gtk_widget_set_size_request(datefmt_win, 440, 280);

	vbox1 = gtk_vbox_new(FALSE, 10);
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
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(datefmt_list_view),
				     prefs_common.use_stripes_everywhere);
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
	
	/* gtk_clist_set_column_width(GTK_CLIST(datefmt_clist), 0, 80); */
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(datefmt_list_view));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);

	g_signal_connect(G_OBJECT(datefmt_list_view), "row_activated", 
			 G_CALLBACK(date_format_select_row),
			 datefmt_win);
	
	table = gtk_table_new(2, 2, FALSE);
	gtk_widget_show(table);
	gtk_box_pack_start(GTK_BOX(vbox1), table, FALSE, FALSE, 0);
	gtk_table_set_row_spacings(GTK_TABLE(table), 4);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8);

	label1 = gtk_label_new(_("Date format"));
	gtk_widget_show(label1);
	gtk_table_attach(GTK_TABLE(table), label1, 0, 1, 0, 1,
			 GTK_FILL, 0, 0, 0);
	gtk_label_set_justify(GTK_LABEL(label1), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(label1), 0, 0.5);

	datefmt_entry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(datefmt_entry), 256);
	gtk_widget_show(datefmt_entry);
	gtk_table_attach(GTK_TABLE(table), datefmt_entry, 1, 2, 0, 1,
			 (GTK_EXPAND | GTK_FILL), 0, 0, 0);

	/* we need the "sample" entry box; add it as data so callbacks can
	 * get the entry box */
	g_object_set_data(G_OBJECT(datefmt_win), "datefmt_sample",
			  datefmt_entry);

	label2 = gtk_label_new(_("Example"));
	gtk_widget_show(label2);
	gtk_table_attach(GTK_TABLE(table), label2, 0, 1, 1, 2,
			 GTK_FILL, 0, 0, 0);
	gtk_label_set_justify(GTK_LABEL(label2), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(label2), 0, 0.5);

	label3 = gtk_label_new("");
	gtk_widget_show(label3);
	gtk_table_attach(GTK_TABLE(table), label3, 1, 2, 1, 2,
			 (GTK_EXPAND | GTK_FILL), 0, 0, 0);
	gtk_label_set_justify(GTK_LABEL(label3), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(label3), 0, 0.5);

	gtkut_stock_button_set_create(&confirm_area, &cancel_btn, GTK_STOCK_CANCEL,
				      &ok_btn, GTK_STOCK_OK, NULL, NULL);

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

	gtk_window_set_position(GTK_WINDOW(datefmt_win), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(datefmt_win), TRUE);

	gtk_widget_show(datefmt_win);
	manage_window_set_transient(GTK_WINDOW(datefmt_win));

	gtk_widget_grab_focus(ok_btn);

	return datefmt_win;
}

static struct KeybindDialog {
	GtkWidget *window;
	GtkWidget *combo;
} keybind;

static void prefs_keybind_select		(void);
static gint prefs_keybind_deleted		(GtkWidget	*widget,
						 GdkEventAny	*event,
						 gpointer	 data);
static gboolean prefs_keybind_key_pressed	(GtkWidget	*widget,
						 GdkEventKey	*event,
						 gpointer	 data);
static void prefs_keybind_cancel		(void);
static void prefs_keybind_apply_clicked		(GtkWidget	*widget);


static void prefs_keybind_select(void)
{
	GtkWidget *window;
	GtkWidget *vbox1;
	GtkWidget *hbox1;
	GtkWidget *label;
	GtkWidget *combo;
	GtkWidget *confirm_area;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_container_set_border_width (GTK_CONTAINER (window), 8);
	gtk_window_set_title (GTK_WINDOW (window), _("Select key bindings"));
	gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal (GTK_WINDOW (window), TRUE);
	gtk_window_set_resizable(GTK_WINDOW (window), FALSE);
	manage_window_set_transient (GTK_WINDOW (window));

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_container_add (GTK_CONTAINER (window), vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), 2);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);

	label = gtk_label_new
		(_("Select preset:"));
	gtk_box_pack_start (GTK_BOX (hbox1), label, FALSE, FALSE, 0);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);

	combo = gtk_combo_new ();
	gtk_box_pack_start (GTK_BOX (hbox1), combo, TRUE, TRUE, 0);
	gtkut_combo_set_items (GTK_COMBO (combo),
			       _("Default"),
			       "Mew / Wanderlust",
			       "Mutt",
			       _("Old Sylpheed"),
			       NULL);
	gtk_editable_set_editable(GTK_EDITABLE(GTK_COMBO (combo)->entry), FALSE);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);

	label = gtk_label_new
		(_("You can also modify each menu shortcut by pressing\n"
		   "any key(s) when focusing the mouse pointer on the item."));
	gtk_box_pack_start (GTK_BOX (hbox1), label, FALSE, FALSE, 0);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtkut_widget_set_small_font_size (label);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);

	gtkut_stock_button_set_create (&confirm_area, &cancel_btn, GTK_STOCK_CANCEL,
				       &ok_btn, GTK_STOCK_OK,
				       NULL, NULL);
	gtk_box_pack_end (GTK_BOX (hbox1), confirm_area, FALSE, FALSE, 0);
	gtk_widget_grab_focus (ok_btn);

	MANAGE_WINDOW_SIGNALS_CONNECT(window);
	g_signal_connect (G_OBJECT (window), "delete_event",
			  G_CALLBACK (prefs_keybind_deleted), NULL);
	g_signal_connect (G_OBJECT (window), "key_press_event",
			  G_CALLBACK (prefs_keybind_key_pressed), NULL);
	g_signal_connect (G_OBJECT (ok_btn), "clicked",
			  G_CALLBACK (prefs_keybind_apply_clicked),
			  NULL);
	g_signal_connect (G_OBJECT (cancel_btn), "clicked",
			  G_CALLBACK (prefs_keybind_cancel),
			  NULL);

	gtk_widget_show_all(window);

	keybind.window = window;
	keybind.combo = combo;
}

static gboolean prefs_keybind_key_pressed(GtkWidget *widget, GdkEventKey *event,
					  gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		prefs_keybind_cancel();
	return FALSE;
}

static gint prefs_keybind_deleted(GtkWidget *widget, GdkEventAny *event,
				  gpointer data)
{
	prefs_keybind_cancel();
	return TRUE;
}

static void prefs_keybind_cancel(void)
{
	gtk_widget_destroy(keybind.window);
	keybind.window = NULL;
	keybind.combo = NULL;
}
  
struct KeyBind {
	const gchar *accel_path;
	const gchar *accel_key;
};

static void prefs_keybind_apply(struct KeyBind keybind[], gint num)
{
	gint i;
	guint key;
	GdkModifierType mods;

	for (i = 0; i < num; i++) {
		const gchar *accel_key
			= keybind[i].accel_key ? keybind[i].accel_key : "";
		gtk_accelerator_parse(accel_key, &key, &mods);
		gtk_accel_map_change_entry(keybind[i].accel_path,
					   key, mods, TRUE);
	}
}

static void prefs_keybind_apply_clicked(GtkWidget *widget)
{
	GtkEntry *entry = GTK_ENTRY(GTK_COMBO(keybind.combo)->entry);
	const gchar *text;
	struct KeyBind *menurc;
	gint n_menurc;

	static struct KeyBind default_menurc[] = {
		{"<Main>/File/Empty all Trash folders",		"<shift>D"},
		{"<Main>/File/Save as...",			"<control>S"},
		{"<Main>/File/Print...",			"<control>P"},
		{"<Main>/File/Work offline",			"<control>W"},
		{"<Main>/File/Synchronise folders",		"<control><shift>S"},
		{"<Main>/File/Exit",				"<control>Q"},

		{"<Main>/Edit/Copy",				"<control>C"},
		{"<Main>/Edit/Select all",			"<control>A"},
		{"<Main>/Edit/Find in current message...",	"<control>F"},
		{"<Main>/Edit/Search folder...",		"<shift><control>F"},
		{"<Main>/Edit/Quick search",			"slash"},

		{"<Main>/View/Show or hide/Message View",	"V"},
		{"<Main>/View/Thread view",			"<control>T"},
		{"<Main>/View/Go to/Previous message",		"P"},
		{"<Main>/View/Go to/Next message",		"N"},
		{"<Main>/View/Go to/Previous unread message",	"<shift>P"},
		{"<Main>/View/Go to/Next unread message",	"<shift>N"},
		{"<Main>/View/Go to/Other folder...",		"G"},
		{"<Main>/View/Open in new window",		"<control><alt>N"},
		{"<Main>/View/Message source",			"<control>U"},
		{"<Main>/View/All headers",			"<control>H"},
		{"<Main>/View/Update summary",			"<control><alt>U"},

		{"<Main>/Message/Receive/Get from current account",
								"<control>I"},
		{"<Main>/Message/Receive/Get from all accounts","<shift><control>I"},
		{"<Main>/Message/Compose an email message",	"<control>M"},
		{"<Main>/Message/Reply",			"<control>R"},
		{"<Main>/Message/Reply to/all",			"<shift><control>R"},
		{"<Main>/Message/Reply to/sender",		""},
		{"<Main>/Message/Reply to/mailing list",	"<control>L"},
		{"<Main>/Message/Forward",			"<control><alt>F"},
		/* {"<Main>/Message/Forward as attachment",	 ""}, */
		{"<Main>/Message/Move...",			"<control>O"},
		{"<Main>/Message/Copy...",			"<shift><control>O"},
		{"<Main>/Message/Move to trash",		"<control>D"},
		{"<Main>/Message/Mark/Mark",			"<shift>asterisk"},
		{"<Main>/Message/Mark/Unmark",			"U"},
		{"<Main>/Message/Mark/Mark as unread",		"<shift>exclam"},
		{"<Main>/Message/Mark/Mark as read",		""},

		{"<Main>/Tools/Address book",			"<shift><control>A"},
		{"<Main>/Tools/Execute",			"X"},
		{"<Main>/Tools/Log window",			"<shift><control>L"},

		{"<Compose>/Message/Send",				"<control>Return"},
		{"<Compose>/Message/Send later",			"<shift><control>S"},
		{"<Compose>/Message/Attach file",			"<control>M"},
		{"<Compose>/Message/Insert file",			"<control>I"},
		{"<Compose>/Message/Insert signature",			"<control>G"},
		{"<Compose>/Message/Save",				"<control>S"},
		{"<Compose>/Message/Close",				"<control>W"},
		{"<Compose>/Edit/Undo",					"<control>Z"},
		{"<Compose>/Edit/Redo",					"<control>Y"},
		{"<Compose>/Edit/Cut",					"<control>X"},
		{"<Compose>/Edit/Copy",					"<control>C"},
		{"<Compose>/Edit/Paste",				"<control>V"},
		{"<Compose>/Edit/Select all",				"<control>A"},
		{"<Compose>/Edit/Advanced/Move a character backward",	"<control>B"},
		{"<Compose>/Edit/Advanced/Move a character forward",	"<control>F"},
		{"<Compose>/Edit/Advanced/Move a word backward,"	""},
		{"<Compose>/Edit/Advanced/Move a word forward",		""},
		{"<Compose>/Edit/Advanced/Move to beginning of line",	""},
		{"<Compose>/Edit/Advanced/Move to end of line",		"<control>E"},
		{"<Compose>/Edit/Advanced/Move to previous line",	"<control>P"},
		{"<Compose>/Edit/Advanced/Move to next line",		"<control>N"},
		{"<Compose>/Edit/Advanced/Delete a character backward",	"<control>H"},
		{"<Compose>/Edit/Advanced/Delete a character forward",	"<control>D"},
		{"<Compose>/Edit/Advanced/Delete a word backward",	""},
		{"<Compose>/Edit/Advanced/Delete a word forward",	""},
		{"<Compose>/Edit/Advanced/Delete line",			"<control>U"},
		{"<Compose>/Edit/Advanced/Delete entire line",		""},
		{"<Compose>/Edit/Advanced/Delete to end of line",	"<control>K"},
		{"<Compose>/Edit/Wrap current paragraph",		"<control>L"},
		{"<Compose>/Edit/Wrap all long lines",			"<control><alt>L"},
		{"<Compose>/Edit/Auto wrapping",			"<shift><control>L"},
		{"<Compose>/Edit/Edit with external editor",		"<shift><control>X"},
		{"<Compose>/Tools/Address book",			"<shift><control>A"},
	};

	static struct KeyBind mew_wl_menurc[] = {
		{"<Main>/File/Empty all Trash folders",		"<shift>D"},
		{"<Main>/File/Save as...",			"Y"},
		{"<Main>/File/Print...",			"<shift>numbersign"},
		{"<Main>/File/Exit",				"<shift>Q"},

		{"<Main>/Edit/Copy",				"<control>C"},
		{"<Main>/Edit/Select all",			"<control>A"},
		{"<Main>/Edit/Find in current message...",	"<control>F"},
		{"<Main>/Edit/Search folder...",		"<control>S"},

		{"<Main>/View/Show or hide/Message View",	""},
		{"<Main>/View/Thread view",			"<shift>T"},
		{"<Main>/View/Go to/Previous message",		"P"},
		{"<Main>/View/Go to/Next message",		"N"},
		{"<Main>/View/Go to/Previous unread message",	"<shift>P"},
		{"<Main>/View/Go to/Next unread message",	"<shift>N"},
		{"<Main>/View/Go to/Other folder...",		"G"},
		{"<Main>/View/Open in new window",		"<control><alt>N"},
		{"<Main>/View/Message source",			"<control>U"},
		{"<Main>/View/All headers",			"<shift>H"},
		{"<Main>/View/Update summary",			"<shift>S"},

		{"<Main>/Message/Receive/Get from current account",
								"<control>I"},
		{"<Main>/Message/Receive/Get from all accounts","<shift><control>I"},
		{"<Main>/Message/Compose an email message",	"W"},
		{"<Main>/Message/Reply",			"<control>R"},
		{"<Main>/Message/Reply to/all",			"<shift>A"},
		{"<Main>/Message/Reply to/sender",		""},
		{"<Main>/Message/Reply to/mailing list",	"<control>L"},
		{"<Main>/Message/Forward",			"F"},
		/* {"<Main>/Message/Forward as attachment", "<shift>F"}, */
		{"<Main>/Message/Move...",			"O"},
		{"<Main>/Message/Copy...",			"<shift>O"},
		{"<Main>/Message/Delete",			"D"},
		{"<Main>/Message/Mark/Mark",			"<shift>asterisk"},
		{"<Main>/Message/Mark/Unmark",			"U"},
		{"<Main>/Message/Mark/Mark as unread",		"<shift>exclam"},
		{"<Main>/Message/Mark/Mark as read",		"<shift>R"},

		{"<Main>/Tools/Address book",			"<shift><control>A"},
		{"<Main>/Tools/Execute",			"X"},
		{"<Main>/Tools/Log window",			"<shift><control>L"},

		{"<Compose>/Message/Close",				"<alt>W"},
		{"<Compose>/Edit/Select all",				""},
		{"<Compose>/Edit/Advanced/Move a word backward,"	"<alt>B"},
		{"<Compose>/Edit/Advanced/Move a word forward",		"<alt>F"},
		{"<Compose>/Edit/Advanced/Move to beginning of line",	"<control>A"},
		{"<Compose>/Edit/Advanced/Delete a word backward",	"<control>W"},
		{"<Compose>/Edit/Advanced/Delete a word forward",	"<alt>D"},
	};

	static struct KeyBind mutt_menurc[] = {
		{"<Main>/File/Empty all Trash folders",		""},
		{"<Main>/File/Save as...",			"S"},
		{"<Main>/File/Print...",			"P"},
		{"<Main>/File/Exit",				"Q"},

		{"<Main>/Edit/Copy",				"<control>C"},
		{"<Main>/Edit/Select all",			"<control>A"},
		{"<Main>/Edit/Find in current message...",	"<control>F"},
		{"<Main>/Edit/Search messages...",		"slash"},

		{"<Main>/View/Show or hide/Message view",	"V"},
		{"<Main>/View/Thread view",			"<control>T"},
		{"<Main>/View/Go to/Previous message",		""},
		{"<Main>/View/Go to/Next message",		""},
		{"<Main>/View/Go to/Previous unread message",	""},
		{"<Main>/View/Go to/Next unread message",	""},
		{"<Main>/View/Go to/Other folder...",		"C"},
		{"<Main>/View/Open in new window",		"<control><alt>N"},
		{"<Main>/View/Message source",			"<control>U"},
		{"<Main>/View/All headers",			"<control>H"},
		{"<Main>/View/Update summary",			"<control><alt>U"},

		{"<Main>/Message/Receive/Get from current account",
								"<control>I"},
		{"<Main>/Message/Receive/Get from all accounts","<shift><control>I"},
		{"<Main>/Message/Compose an email message",		"M"},
		{"<Main>/Message/Reply",			"R"},
		{"<Main>/Message/Reply to/all",			"G"},
		{"<Main>/Message/Reply to/sender",		""},
		{"<Main>/Message/Reply to/mailing list",	"<control>L"},
		{"<Main>/Message/Forward",			"F"},
		{"<Main>/Message/Forward as attachment",	""},
		{"<Main>/Message/Move...",			"<control>O"},
		{"<Main>/Message/Copy...",			"<shift>C"},
		{"<Main>/Message/Delete",			"D"},
		{"<Main>/Message/Mark/Mark",			"<shift>F"},
		{"<Main>/Message/Mark/Unmark",			"U"},
		{"<Main>/Message/Mark/Mark as unread",		"<shift>N"},
		{"<Main>/Message/Mark/Mark as read",		""},

		{"<Main>/Tools/Address book",			"<shift><control>A"},
		{"<Main>/Tools/Execute",			"X"},
		{"<Main>/Tools/Log window",			"<shift><control>L"},

		{"<Compose>/Message/Close",				"<alt>W"},
		{"<Compose>/Edit/Select all",				""},
		{"<Compose>/Edit/Advanced/Move a word backward",	"<alt>B"},
		{"<Compose>/Edit/Advanced/Move a word forward",		"<alt>F"},
		{"<Compose>/Edit/Advanced/Move to beginning of line",	"<control>A"},
		{"<Compose>/Edit/Advanced/Delete a word backward",	"<control>W"},
		{"<Compose>/Edit/Advanced/Delete a word forward",	"<alt>D"},
	};

	static struct KeyBind old_sylpheed_menurc[] = {
		{"<Main>/File/Empty all Trash folders",		""},
		{"<Main>/File/Save as...",			""},
		{"<Main>/File/Print...",			"<alt>P"},
		{"<Main>/File/Exit",				"<alt>Q"},

		{"<Main>/Edit/Copy",				"<control>C"},
		{"<Main>/Edit/Select all",			"<control>A"},
		{"<Main>/Edit/Find in current message...",	"<control>F"},
		{"<Main>/Edit/Search folder...",		"<control>S"},

		{"<Main>/View/Show or hide/Message View",	""},
		{"<Main>/View/Thread view",			"<control>T"},
		{"<Main>/View/Go to/Previous message",		"P"},
		{"<Main>/View/Go to/Next message",		"N"},
		{"<Main>/View/Go to/Previous unread message",	"<shift>P"},
		{"<Main>/View/Go to/Next unread message",	"<shift>N"},
		{"<Main>/View/Go to/Other folder...",		"<alt>G"},
		{"<Main>/View/Open in new window",		"<shift><control>N"},
		{"<Main>/View/Message source",			"<control>U"},
		{"<Main>/View/All headers",			"<control>H"},
		{"<Main>/View/Update summary",			"<alt>U"},

		{"<Main>/Message/Receive/Get from current account",
								"<alt>I"},
		{"<Main>/Message/Receive/Get from all accounts","<shift><alt>I"},
		{"<Main>/Message/Compose an email message",	"<alt>N"},
		{"<Main>/Message/Reply",			"<alt>R"},
		{"<Main>/Message/Reply to/all",			"<shift><alt>R"},
		{"<Main>/Message/Reply to/sender",		"<control><alt>R"},
		{"<Main>/Message/Reply to/mailing list",	"<control>L"},
		{"<Main>/Message/Forward",			 "<shift><alt>F"},
		/* "(menu-path \"<Main>/Message/Forward as attachment", "<shift><control>F"}, */
		{"<Main>/Message/Move...",			"<alt>O"},
		{"<Main>/Message/Copy...",			""},
		{"<Main>/Message/Delete",			"<alt>D"},
		{"<Main>/Message/Mark/Mark",			"<shift>asterisk"},
		{"<Main>/Message/Mark/Unmark",			"U"},
		{"<Main>/Message/Mark/Mark as unread",		"<shift>exclam"},
		{"<Main>/Message/Mark/Mark as read",		""},

		{"<Main>/Tools/Address book",			"<alt>A"},
		{"<Main>/Tools/Execute",			"<alt>X"},
		{"<Main>/Tools/Log window",			"<alt>L"},

		{"<Compose>/Message/Close",				"<alt>W"},
		{"<Compose>/Edit/Select all",				""},
		{"<Compose>/Edit/Advanced/Move a word backward",	"<alt>B"},
		{"<Compose>/Edit/Advanced/Move a word forward",		"<alt>F"},
		{"<Compose>/Edit/Advanced/Move to beginning of line",	"<control>A"},
		{"<Compose>/Edit/Advanced/Delete a word backward",	"<control>W"},
		{"<Compose>/Edit/Advanced/Delete a word forward",	"<alt>D"},
	};
  
	text = gtk_entry_get_text(entry);
  
	if (!strcmp(text, _("Default"))) {
		menurc = default_menurc;
		n_menurc = G_N_ELEMENTS(default_menurc);
	} else if (!strcmp(text, "Mew / Wanderlust")) {
		menurc = mew_wl_menurc;
		n_menurc = G_N_ELEMENTS(mew_wl_menurc);
	} else if (!strcmp(text, "Mutt")) {
		menurc = mutt_menurc;
		n_menurc = G_N_ELEMENTS(mutt_menurc);
	} else if (!strcmp(text, _("Old Sylpheed"))) {
	        menurc = old_sylpheed_menurc;
		n_menurc = G_N_ELEMENTS(old_sylpheed_menurc);
	} else {
		return;
	}

	/* prefs_keybind_apply(empty_menurc, G_N_ELEMENTS(empty_menurc)); */
	prefs_keybind_apply(menurc, n_menurc);

	gtk_widget_destroy(keybind.window);
	keybind.window = NULL;
	keybind.combo = NULL;
}

void prefs_summaries_create_widget(PrefsPage *_page, GtkWindow *window, 
			       	  gpointer data)
{
	SummariesPage *prefs_summaries = (SummariesPage *) _page;
	
	GtkWidget *vbox1;
	GtkWidget *chkbtn_transhdr;
	GtkWidget *chkbtn_folder_unread;
	GtkWidget *hbox1;
	GtkWidget *label_ng_abbrev;
	GtkWidget *spinbtn_ng_abbrev_len;
	GtkObject *spinbtn_ng_abbrev_len_adj;
	GtkWidget *vbox2;
	GtkWidget *chkbtn_useaddrbook;
	GtkWidget *chkbtn_threadsubj;
	GtkWidget *vbox3;
	GtkWidget *label_datefmt;
	GtkWidget *button_datefmt;
	GtkWidget *entry_datefmt;
	GtkTooltips *tip_datefmt;
	GtkWidget *hbox_dispitem;
	GtkWidget *frame_dispitem;
	GtkWidget *button_dispitem;

	GtkWidget *vbox4;
	GtkWidget *hbox2;
	GtkWidget *checkbtn_always_show_msg;
	GtkWidget *checkbtn_mark_as_read_on_newwin;
	GtkWidget *spinbtn_mark_as_read_delay;
	GtkObject *spinbtn_mark_as_read_delay_adj;
	GtkWidget *checkbtn_immedexec;
	GtkWidget *checkbtn_ask_mark_all_read;
	GtkTooltips *immedexec_tooltip;
	GtkWidget *label;
	GtkWidget *menu;
	GtkWidget *menuitem;
	GtkWidget *button_keybind;
 	GtkWidget *optmenu_select_on_entry;
 	GtkWidget *optmenu_nextunreadmsgdialog;
	GtkWidget *table;
	GtkWidget *vbox5;
	GtkTooltips *tooltips;

	tooltips = gtk_tooltips_new();

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	vbox2 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, TRUE, 0);

	PACK_CHECK_BUTTON
		(vbox2, chkbtn_transhdr,
		 _("Translate header names"));
	gtk_tooltips_set_tip(tooltips, chkbtn_transhdr,
			     _("The display of standard headers (such as 'From:', 'Subject:') "
			     "will be translated into your language."), NULL);

	PACK_CHECK_BUTTON (vbox2, chkbtn_folder_unread,
			   _("Display unread number next to folder name"));

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, TRUE, 0);

	label_ng_abbrev = gtk_label_new
		(_("Abbreviate newsgroup names longer than"));
	gtk_widget_show (label_ng_abbrev);
	gtk_box_pack_start (GTK_BOX (hbox1), label_ng_abbrev, FALSE, FALSE, 0);

	spinbtn_ng_abbrev_len_adj = gtk_adjustment_new (16, 0, 999, 1, 10, 10);
	spinbtn_ng_abbrev_len = gtk_spin_button_new
		(GTK_ADJUSTMENT (spinbtn_ng_abbrev_len_adj), 1, 0);
	gtk_widget_show (spinbtn_ng_abbrev_len);
	gtk_box_pack_start (GTK_BOX (hbox1), spinbtn_ng_abbrev_len,
			    FALSE, FALSE, 0);
	gtk_widget_set_size_request (spinbtn_ng_abbrev_len, 56, -1);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbtn_ng_abbrev_len),
				     TRUE);

	label_ng_abbrev = gtk_label_new (_("letters"));
	gtk_widget_show (label_ng_abbrev);
	gtk_box_pack_start (GTK_BOX (hbox1), label_ng_abbrev, FALSE, FALSE, 0);

	/* ---- Summary ---- */

	vbox3 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox3);
	gtk_container_add (GTK_CONTAINER (vbox2), vbox3);
	gtk_container_set_border_width (GTK_CONTAINER (vbox3), 0);

	PACK_CHECK_BUTTON
		(vbox3, chkbtn_useaddrbook,
		 _("Display sender using address book"));
	PACK_CHECK_BUTTON
		(vbox3, chkbtn_threadsubj,
		 _("Thread using subject in addition to standard headers"));

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox3), hbox1, FALSE, TRUE, 0);

	label_datefmt = gtk_label_new (_("Date format"));
	gtk_widget_show (label_datefmt);
	gtk_box_pack_start (GTK_BOX (hbox1), label_datefmt, FALSE, FALSE, 0);

	entry_datefmt = gtk_entry_new ();
	gtk_widget_show (entry_datefmt);
	gtk_box_pack_start (GTK_BOX (hbox1), entry_datefmt, TRUE, TRUE, 0);

	button_datefmt = gtk_button_new_with_label (" ... ");

	gtk_widget_show (button_datefmt);
	gtk_box_pack_start (GTK_BOX (hbox1), button_datefmt, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (button_datefmt), "clicked",
			  G_CALLBACK (date_format_create), NULL);
	tip_datefmt = gtk_tooltips_new();
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tip_datefmt),
			     button_datefmt,
			     _("Date format help"), NULL);

	PACK_VSPACER(vbox2, vbox3, VSPACING_NARROW);

	PACK_FRAME(vbox3, frame_dispitem, _("Set displayed columns"));

	hbox_dispitem = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox_dispitem);
	gtk_container_add (GTK_CONTAINER (frame_dispitem), hbox_dispitem);
	gtk_container_set_border_width (GTK_CONTAINER (hbox_dispitem), 8);

	button_dispitem = gtk_button_new_with_label
		(_(" Folder list... "));
	gtk_widget_show (button_dispitem);
	gtk_box_pack_start (GTK_BOX (hbox_dispitem), button_dispitem, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (button_dispitem), "clicked",
			  G_CALLBACK (prefs_folder_column_open),
			  NULL);
	
	button_dispitem = gtk_button_new_with_label
		(_(" Message list... "));
	gtk_widget_show (button_dispitem);
	gtk_box_pack_start (GTK_BOX (hbox_dispitem), button_dispitem, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (button_dispitem), "clicked",
			  G_CALLBACK (prefs_summary_column_open),
			  NULL);

	vbox4 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox4);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox4, FALSE, FALSE, 0);

	/* PACK_CHECK_BUTTON (vbox2, checkbtn_emacs,
			   _("Emulate the behavior of mouse operation of\n"
			     "Emacs-based mailer"));
	gtk_label_set_justify (GTK_LABEL (GTK_BIN (checkbtn_emacs)->child),
			       GTK_JUSTIFY_LEFT);   */

	immedexec_tooltip = gtk_tooltips_new();

	PACK_CHECK_BUTTON
		(vbox4, checkbtn_immedexec,
		 _("Execute immediately when moving or deleting messages"));
	gtk_tooltips_set_tip(GTK_TOOLTIPS(immedexec_tooltip), checkbtn_immedexec,
			     _("Messages will be marked until execution"
		   	       " if this is turned off"),
			     NULL);

	PACK_CHECK_BUTTON
		(vbox4, checkbtn_ask_mark_all_read,
		 _("Confirm before marking all mails in a folder as read"));

	PACK_CHECK_BUTTON
		(vbox4, checkbtn_always_show_msg,
		 _("Always open message when selected"));

	PACK_CHECK_BUTTON
		(vbox4, checkbtn_mark_as_read_on_newwin,
		 _("Only mark message as read when opened in a new window"));

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox4), hbox1, FALSE, TRUE, 0);

	gtk_box_pack_start (GTK_BOX (hbox1), gtk_label_new
		(_("Mark messages as read after ")), FALSE, FALSE, 0);

	spinbtn_mark_as_read_delay_adj = gtk_adjustment_new (0, 0, 60, 1, 10, 10);
	spinbtn_mark_as_read_delay = gtk_spin_button_new
		(GTK_ADJUSTMENT (spinbtn_mark_as_read_delay_adj), 1, 0);
	gtk_box_pack_start (GTK_BOX (hbox1), spinbtn_mark_as_read_delay,
			    FALSE, FALSE, 0);
	gtk_widget_set_size_request (spinbtn_mark_as_read_delay, 56, -1);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbtn_mark_as_read_delay),
				     TRUE);
	gtk_box_pack_start (GTK_BOX (hbox1), gtk_label_new
		(_(" seconds")), FALSE, FALSE, 0);
	gtk_widget_show_all(hbox1);

	vbox5 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox5);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox5, FALSE, FALSE, 0);

	table = gtk_table_new(1, 1, FALSE);
	gtk_widget_show(table);
	gtk_container_add (GTK_CONTAINER (vbox5), table);
	gtk_table_set_row_spacings(GTK_TABLE(table), 4);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8);

	label = gtk_label_new (_("When entering a folder"));
	gtk_widget_show (label);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
			(GtkAttachOptions) (GTK_FILL),
			(GtkAttachOptions) (0), 0, 0);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_RIGHT);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);

 	optmenu_select_on_entry = gtk_option_menu_new ();
 	gtk_widget_show (optmenu_select_on_entry);

	gtk_table_attach(GTK_TABLE(table), optmenu_select_on_entry, 1, 2, 0, 1,
			(GtkAttachOptions) (GTK_FILL),
			(GtkAttachOptions) (0), 0, 0);

	menu = gtk_menu_new ();
	MENUITEM_ADD (menu, menuitem, _("Do nothing"), SELECTONENTRY_NOTHING);
	MENUITEM_ADD (menu, menuitem, _("Select first unread (or new or marked) message"),
		      SELECTONENTRY_UNM);
	MENUITEM_ADD (menu, menuitem, _("Select first unread (or marked or new) message"),
		      SELECTONENTRY_UMN);
	MENUITEM_ADD (menu, menuitem, _("Select first new (or unread or marked) message"),
		      SELECTONENTRY_NUM);
	MENUITEM_ADD (menu, menuitem, _("Select first new (or marked or unread) message"),
		      SELECTONENTRY_NMU);
	MENUITEM_ADD (menu, menuitem, _("Select first marked (or new or unread) message"),
		      SELECTONENTRY_MNU);
	MENUITEM_ADD (menu, menuitem, _("Select first marked (or unread or new) message"),
		      SELECTONENTRY_MUN);

	gtk_option_menu_set_menu (GTK_OPTION_MENU (optmenu_select_on_entry), menu);

	/* Next Unread Message Dialog */
	table = gtk_table_new(1, 1, FALSE);
	gtk_widget_show(table);
	gtk_container_add (GTK_CONTAINER (vbox5), table);
	gtk_table_set_row_spacings(GTK_TABLE(table), 4);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8);

	label = gtk_label_new (_("Show \"no unread (or new) message\" dialog"));
	gtk_widget_show (label);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2,
			(GtkAttachOptions) (GTK_FILL),
			(GtkAttachOptions) (0), 0, 0);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_RIGHT);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);

 	optmenu_nextunreadmsgdialog = gtk_option_menu_new ();
 	gtk_widget_show (optmenu_nextunreadmsgdialog);

	gtk_table_attach(GTK_TABLE(table), optmenu_nextunreadmsgdialog, 1, 2, 1, 2,
			(GtkAttachOptions) (GTK_FILL),
			(GtkAttachOptions) (0), 0, 0);

	menu = gtk_menu_new ();
	MENUITEM_ADD (menu, menuitem, _("Always"), NEXTUNREADMSGDIALOG_ALWAYS);
	MENUITEM_ADD (menu, menuitem, _("Assume 'Yes'"), 
		      NEXTUNREADMSGDIALOG_ASSUME_YES);
	MENUITEM_ADD (menu, menuitem, _("Assume 'No'"), 
		      NEXTUNREADMSGDIALOG_ASSUME_NO);

	gtk_option_menu_set_menu (GTK_OPTION_MENU (optmenu_nextunreadmsgdialog), menu);

	hbox2 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox2, FALSE, FALSE, 0);

	button_keybind = gtk_button_new_with_label (_(" Set key bindings... "));
	gtk_widget_show (button_keybind);
	gtk_box_pack_start (GTK_BOX (hbox2), button_keybind, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (button_keybind), "clicked",
			  G_CALLBACK (prefs_keybind_select), NULL);


	prefs_summaries->window			= GTK_WIDGET(window);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkbtn_transhdr),
			prefs_common.trans_hdr);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkbtn_folder_unread),
			prefs_common.display_folder_unread);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkbtn_useaddrbook),
			prefs_common.use_addr_book);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkbtn_threadsubj),
			prefs_common.thread_by_subject);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbtn_ng_abbrev_len),
			prefs_common.ng_abbrev_len);
	gtk_entry_set_text(GTK_ENTRY(entry_datefmt), 
			prefs_common.date_format?prefs_common.date_format:"");	

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_always_show_msg),
			prefs_common.always_show_msg);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_mark_as_read_on_newwin),
			prefs_common.mark_as_read_on_new_window);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbtn_mark_as_read_delay),
			prefs_common.mark_as_read_delay);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_immedexec),
			prefs_common.immediate_exec);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_ask_mark_all_read),
			prefs_common.ask_mark_all_read);

	gtk_option_menu_set_history(GTK_OPTION_MENU(optmenu_select_on_entry),
			prefs_common.select_on_entry);
	gtk_option_menu_set_history(GTK_OPTION_MENU(optmenu_nextunreadmsgdialog),
			prefs_common.next_unread_msg_dialog);

	prefs_summaries->chkbtn_transhdr = chkbtn_transhdr;
	prefs_summaries->chkbtn_folder_unread = chkbtn_folder_unread;
	prefs_summaries->spinbtn_ng_abbrev_len = spinbtn_ng_abbrev_len;
	prefs_summaries->chkbtn_useaddrbook = chkbtn_useaddrbook;
	prefs_summaries->chkbtn_threadsubj = chkbtn_threadsubj;
	prefs_summaries->entry_datefmt = entry_datefmt;

	prefs_summaries->checkbtn_always_show_msg = checkbtn_always_show_msg;
	prefs_summaries->checkbtn_mark_as_read_on_newwin = checkbtn_mark_as_read_on_newwin;
	prefs_summaries->spinbtn_mark_as_read_delay = spinbtn_mark_as_read_delay;
	prefs_summaries->checkbtn_immedexec = checkbtn_immedexec;
	prefs_summaries->checkbtn_ask_mark_all_read = checkbtn_ask_mark_all_read;
	prefs_summaries->optmenu_select_on_entry = optmenu_select_on_entry;
	prefs_summaries->optmenu_nextunreadmsgdialog = optmenu_nextunreadmsgdialog;

	prefs_summaries->page.widget = vbox1;
}

void prefs_summaries_save(PrefsPage *_page)
{
	SummariesPage *page = (SummariesPage *) _page;
	GtkWidget *menu;
	GtkWidget *menuitem;

	prefs_common.trans_hdr = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(page->chkbtn_transhdr));
	prefs_common.display_folder_unread = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(page->chkbtn_folder_unread));
	prefs_common.use_addr_book = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(page->chkbtn_useaddrbook));
	prefs_common.thread_by_subject = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(page->chkbtn_threadsubj));
	prefs_common.ng_abbrev_len = gtk_spin_button_get_value_as_int(
			GTK_SPIN_BUTTON(page->spinbtn_ng_abbrev_len));
	
	g_free(prefs_common.date_format); 
	prefs_common.date_format = gtk_editable_get_chars(
			GTK_EDITABLE(page->entry_datefmt), 0, -1);	

	prefs_common.always_show_msg = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->checkbtn_always_show_msg));
	prefs_common.mark_as_read_on_new_window = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->checkbtn_mark_as_read_on_newwin));
	prefs_common.immediate_exec = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->checkbtn_immedexec));
	prefs_common.ask_mark_all_read = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->checkbtn_ask_mark_all_read));
	prefs_common.mark_as_read_delay = gtk_spin_button_get_value_as_int(
			GTK_SPIN_BUTTON(page->spinbtn_mark_as_read_delay));

	menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(page->optmenu_select_on_entry));
	menuitem = gtk_menu_get_active(GTK_MENU(menu));
	prefs_common.select_on_entry = GPOINTER_TO_INT
		(g_object_get_data(G_OBJECT(menuitem), MENU_VAL_ID));
	
	menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(page->optmenu_nextunreadmsgdialog));
	menuitem = gtk_menu_get_active(GTK_MENU(menu));
	prefs_common.next_unread_msg_dialog = GPOINTER_TO_INT
		(g_object_get_data(G_OBJECT(menuitem), MENU_VAL_ID));
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

	g_return_if_fail(widget != NULL);
	g_return_if_fail(*widget != NULL);
	g_return_if_fail(prefs_summaries->entry_datefmt != NULL);

	datefmt_sample = GTK_WIDGET(g_object_get_data
				    (G_OBJECT(*widget), "datefmt_sample"));
	g_return_if_fail(datefmt_sample != NULL);

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
	g_return_if_fail(widget != NULL);
	g_return_if_fail(*widget != NULL);

	gtk_widget_destroy(*widget);
	*widget = NULL;
}

static gboolean date_format_key_pressed(GtkWidget *keywidget, GdkEventKey *event,
					GtkWidget **widget)
{
	if (event && event->keyval == GDK_Escape)
		date_format_cancel_btn_clicked(NULL, widget);
	return FALSE;
}

static gboolean date_format_on_delete(GtkWidget *dialogwidget,
				      GdkEventAny *event, GtkWidget **widget)
{
	g_return_val_if_fail(widget != NULL, FALSE);
	g_return_val_if_fail(*widget != NULL, FALSE);

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

	cur_time = time(NULL);
	cal_time = localtime(&cur_time);
	buffer[0] = 0;
	text = gtk_editable_get_chars(editable, 0, -1);
	if (text)
		fast_strftime(buffer, sizeof buffer, text, cal_time); 
	g_free(text);

	text = conv_codeset_strdup(buffer,
				   conv_get_locale_charset_str(),
				   CS_UTF_8);
	if (!text)
		text = g_strdup(buffer);

	gtk_label_set_text(example, text);

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
	
	g_return_if_fail(date_format != NULL);

	/* only on double click */
	datefmt_sample = GTK_WIDGET(g_object_get_data(G_OBJECT(date_format), 
						      "datefmt_sample"));

	g_return_if_fail(datefmt_sample != NULL);

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
	strcat(new_format, &old_format[cur_pos]);

	gtk_entry_set_text(GTK_ENTRY(datefmt_sample), new_format);
	gtk_editable_set_position(GTK_EDITABLE(datefmt_sample), cur_pos + 2);

	g_free(new_format);
}
