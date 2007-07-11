/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2005-2007 Colin Leroy <colin@colino.net> & The Claws Mail Team
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

#include "gtk/gtkutils.h"
#include "gtk/prefswindow.h"

#include "manage_window.h"
#ifdef HAVE_LIBETPAN
#include "imap-thread.h"
#endif

typedef struct _OtherPage
{
	PrefsPage page;

	GtkWidget *window;

	GtkWidget *checkbtn_addaddrbyclick;
	GtkWidget *checkbtn_confonexit;
	GtkWidget *checkbtn_cleanonexit;
	GtkWidget *checkbtn_askonclean;
	GtkWidget *checkbtn_warnqueued;
	GtkWidget *spinbtn_iotimeout;
	GtkWidget *checkbtn_gtk_can_change_accels;
	GtkWidget *checkbtn_askonfilter;
	GtkWidget *checkbtn_real_time_sync;
} OtherPage;

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

	window = gtkut_window_new(GTK_WINDOW_TOPLEVEL, "prefs_other");
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

static void prefs_other_create_widget(PrefsPage *_page, GtkWindow *window, 
			       	  gpointer data)
{
	OtherPage *prefs_other = (OtherPage *) _page;
	
	GtkWidget *vbox1;
	GtkWidget *hbox1;

	GtkWidget *frame_addr;
	GtkWidget *vbox_addr;
	GtkWidget *checkbtn_addaddrbyclick;
	
	GtkWidget *frame_exit;
	GtkWidget *vbox_exit;
	GtkWidget *checkbtn_confonexit;
	GtkWidget *checkbtn_cleanonexit;
	GtkWidget *checkbtn_warnqueued;

	GtkWidget *frame_keys;
	GtkWidget *vbox_keys;
	GtkWidget *checkbtn_gtk_can_change_accels;
	GtkTooltips *gtk_can_change_accels_tooltip;
	GtkWidget *button_keybind;

	GtkWidget *label_iotimeout;
	GtkWidget *spinbtn_iotimeout;
	GtkObject *spinbtn_iotimeout_adj;

	GtkWidget *vbox2;
	GtkWidget *checkbtn_askonclean;
	GtkWidget *checkbtn_askonfilter;
	GtkWidget *checkbtn_real_time_sync;
	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	vbox_addr = gtkut_get_options_frame(vbox1, &frame_addr, _("Address book"));

	PACK_CHECK_BUTTON
		(vbox_addr, checkbtn_addaddrbyclick,
		 _("Add address to destination when double-clicked"));

	/* On Exit */
	vbox_exit = gtkut_get_options_frame(vbox1, &frame_exit, _("On exit"));

	PACK_CHECK_BUTTON (vbox_exit, checkbtn_confonexit,
			   _("Confirm on exit"));

	hbox1 = gtk_hbox_new (FALSE, 32);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox_exit), hbox1, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON (hbox1, checkbtn_cleanonexit,
			   _("Empty trash on exit"));

	PACK_CHECK_BUTTON (vbox_exit, checkbtn_warnqueued,
			   _("Warn if there are queued messages"));

	vbox_keys = gtkut_get_options_frame(vbox1, &frame_keys, _("Keyboard shortcuts"));

	PACK_CHECK_BUTTON(vbox_keys, checkbtn_gtk_can_change_accels,
			_("Enable customisable menu shortcuts"));
	gtk_can_change_accels_tooltip = gtk_tooltips_new();
	gtk_tooltips_set_tip(GTK_TOOLTIPS(gtk_can_change_accels_tooltip),
			checkbtn_gtk_can_change_accels,
			_("If checked, you can change the keyboard shortcuts of "
				"most of the menu items by focusing on the menu "
				"item and pressing a key combination.\n"
				"Uncheck this option if you want to lock all "
				"existing menu shortcuts."),
			NULL);

	button_keybind = gtk_button_new_with_label (_(" Set key bindings... "));
	gtk_widget_show (button_keybind);
	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox_keys), hbox1, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox1), button_keybind, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (button_keybind), "clicked",
			  G_CALLBACK (prefs_keybind_select), NULL);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);

	label_iotimeout = gtk_label_new (_("Socket I/O timeout"));
	gtk_widget_show (label_iotimeout);
	gtk_box_pack_start (GTK_BOX (hbox1), label_iotimeout, FALSE, FALSE, 0);

	spinbtn_iotimeout_adj = gtk_adjustment_new (60, 0, 1000, 1, 10, 10);
	spinbtn_iotimeout = gtk_spin_button_new
		(GTK_ADJUSTMENT (spinbtn_iotimeout_adj), 1, 0);
	gtk_widget_show (spinbtn_iotimeout);
	gtk_box_pack_start (GTK_BOX (hbox1), spinbtn_iotimeout,
			    FALSE, FALSE, 0);
	gtk_widget_set_size_request (spinbtn_iotimeout, 64, -1);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbtn_iotimeout), TRUE);

	label_iotimeout = gtk_label_new (_("seconds"));
	gtk_widget_show (label_iotimeout);
	gtk_box_pack_start (GTK_BOX (hbox1), label_iotimeout, FALSE, FALSE, 0);

	vbox2 = gtk_vbox_new (FALSE, 8);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON (vbox2, checkbtn_askonclean, 
			   _("Ask before emptying trash"));
	PACK_CHECK_BUTTON (vbox2, checkbtn_askonfilter,
			   _("Ask about account specific filtering rules when "
			     "filtering manually"));
	PACK_CHECK_BUTTON (vbox2, checkbtn_real_time_sync,
			   _("Synchronise offline folders as soon as possible"));

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_addaddrbyclick), 
		prefs_common.add_address_by_click);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_confonexit), 
		prefs_common.confirm_on_exit);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_cleanonexit), 
		prefs_common.clean_on_exit);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_askonclean), 
		prefs_common.ask_on_clean);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_warnqueued), 
		prefs_common.warn_queued_on_exit);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_gtk_can_change_accels),
		prefs_common.gtk_can_change_accels);

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbtn_iotimeout),
		prefs_common.io_timeout_secs);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_askonfilter), 
		prefs_common.ask_apply_per_account_filtering_rules);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_real_time_sync), 
		prefs_common.real_time_sync);

	prefs_other->checkbtn_addaddrbyclick = checkbtn_addaddrbyclick;
	prefs_other->checkbtn_confonexit = checkbtn_confonexit;
	prefs_other->checkbtn_cleanonexit = checkbtn_cleanonexit;
	prefs_other->checkbtn_askonclean = checkbtn_askonclean;
	prefs_other->checkbtn_warnqueued = checkbtn_warnqueued;
	prefs_other->spinbtn_iotimeout = spinbtn_iotimeout;
	prefs_other->checkbtn_gtk_can_change_accels = checkbtn_gtk_can_change_accels;
	prefs_other->checkbtn_askonfilter = checkbtn_askonfilter;
	prefs_other->checkbtn_real_time_sync = checkbtn_real_time_sync;

	prefs_other->page.widget = vbox1;
}

static void prefs_other_save(PrefsPage *_page)
{
	OtherPage *page = (OtherPage *) _page;
	gboolean gtk_can_change_accels;

	prefs_common.add_address_by_click = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->checkbtn_addaddrbyclick));
	prefs_common.confirm_on_exit = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->checkbtn_confonexit));
	prefs_common.clean_on_exit = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->checkbtn_cleanonexit)); 
	prefs_common.ask_on_clean = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->checkbtn_askonclean));
	prefs_common.warn_queued_on_exit = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->checkbtn_warnqueued)); 
	prefs_common.io_timeout_secs = gtk_spin_button_get_value_as_int(
		GTK_SPIN_BUTTON(page->spinbtn_iotimeout));
	sock_set_io_timeout(prefs_common.io_timeout_secs);
#ifdef HAVE_LIBETPAN
	imap_main_set_timeout(prefs_common.io_timeout_secs);
#endif
	prefs_common.ask_apply_per_account_filtering_rules = 
		gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(page->checkbtn_askonfilter)); 
	prefs_common.real_time_sync = 
		gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(page->checkbtn_real_time_sync)); 

	gtk_can_change_accels = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->checkbtn_gtk_can_change_accels));

	if (prefs_common.gtk_can_change_accels != gtk_can_change_accels) {

		prefs_common.gtk_can_change_accels = gtk_can_change_accels;

		gtk_settings_set_long_property(gtk_settings_get_default(),
				"gtk-can-change-accels",
				(glong)prefs_common.gtk_can_change_accels,
				"XProperty");

		/* gtk_can_change_accels value changed : we have (only if changed)
		 * to apply the gtk property to all widgets : */
		gtk_rc_reparse_all_for_settings(gtk_settings_get_default(), TRUE);
	}
}

static void prefs_other_destroy_widget(PrefsPage *_page)
{
}

OtherPage *prefs_other;

void prefs_other_init(void)
{
	OtherPage *page;
	static gchar *path[2];

	path[0] = _("Other");
	path[1] = NULL;

	page = g_new0(OtherPage, 1);
	page->page.path = path;
	page->page.create_widget = prefs_other_create_widget;
	page->page.destroy_widget = prefs_other_destroy_widget;
	page->page.save_page = prefs_other_save;
	page->page.weight = 5.0;
	prefs_gtk_register_page((PrefsPage *) page);
	prefs_other = page;

	gtk_settings_set_long_property(gtk_settings_get_default(),
			"gtk-can-change-accels",
			(glong)prefs_common.gtk_can_change_accels,
			"XProperty");
}

void prefs_other_done(void)
{
	prefs_gtk_unregister_page((PrefsPage *) prefs_other);
	g_free(prefs_other);
}
