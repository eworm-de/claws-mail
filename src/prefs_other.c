/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2005-2025 the Claws Mail team and Colin Leroy
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
#include "config.h"
#include "claws-features.h"
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
#include "combobox.h"
#ifndef PASSWORD_CRYPTO_OLD
#include "password.h"
#include "password_gtk.h"
#endif

#include "manage_window.h"
#ifdef HAVE_LIBETPAN
#include "imap-thread.h"
#endif

#include "file-utils.h"

typedef struct _OtherPage
{
	PrefsPage page;

	GtkWidget *window;

	GtkWidget *keys_preset_combo;
	GtkWidget *keys_preset_hbox;
	GtkWidget *checkbtn_addaddrbyclick;
	GtkWidget *checkbtn_confonexit;
	GtkWidget *checkbtn_cleanonexit;
	GtkWidget *checkbtn_askonclean;
	GtkWidget *checkbtn_warnqueued;
	GtkWidget *spinbtn_iotimeout;
	GtkWidget *checkbtn_gtk_enable_accels;
	GtkWidget *checkbtn_askonfilter;
	GtkWidget *checkbtn_use_shred;
	GtkWidget *checkbtn_real_time_sync;
	GtkWidget *entry_attach_save_chmod;
	GtkWidget *flush_metadata_faster_radiobtn;
	GtkWidget *flush_metadata_safer_radiobtn;
	GtkWidget *checkbtn_transhdr;
#ifndef PASSWORD_CRYPTO_OLD
	GtkWidget *checkbtn_use_passphrase;
#endif
} OtherPage;

#ifndef PASSWORD_CRYPTO_OLD
static void prefs_change_primary_passphrase(GtkButton *button, gpointer data);
#endif

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
		if (key == 0 && mods == 0) {
			g_message("Failed parsing accelerator '%s' for path '%s'\n",
				  accel_key, keybind[i].accel_path);
		}
		gtk_accel_map_change_entry(keybind[i].accel_path,
					   key, mods, TRUE);
	}
}

static void prefs_keybind_preset_changed(GtkComboBox *widget)
{
	gchar *text;
	struct KeyBind *menurc;
	gint n_menurc;

	/* make sure to keep the table below in sync with the ones in mainwindow.c, messageview.c */
	static struct KeyBind default_menurc[] = {
		/* main */
		{"<Actions>/Menu/File/EmptyTrashes",			"<shift>D"},
		{"<Actions>/Menu/File/SaveAs",				"<control>S"},
		{"<Actions>/Menu/File/SavePartAs",			"Y"},
		{"<Actions>/Menu/File/Print",				"<control>P"},
		{"<Actions>/Menu/File/SynchroniseFolders",		"<shift><control>S"},
		{"<Actions>/Menu/File/Exit",				"<control>Q"},
		{"<Actions>/Menu/File/OfflineMode",			"<control>W"},

		{"<Actions>/Menu/Edit/Copy",				"<control>C"},
		{"<Actions>/Menu/Edit/SelectAll",			"<control>A"},
		{"<Actions>/Menu/Edit/Find",				"<control>F"},
		{"<Actions>/Menu/Edit/SearchFolder",			"<shift><control>F"},
		{"<Actions>/Menu/Edit/QuickSearch",			"slash"},

		{"<Actions>/Menu/View/ShowHide/MenuBar",		"<control>F12"},
		{"<Actions>/Menu/View/ShowHide/MessageView",		"V"},
		{"<Actions>/Menu/View/ThreadView",			"<control>T"},
		{"<Actions>/Menu/View/FullScreen",			"F11"},
		{"<Actions>/Menu/View/AllHeaders",			"<control>H"},
		{"<Actions>/Menu/View/Quotes/CollapseAll",		"<shift><control>Q"},

		{"<Actions>/Menu/View/Goto/Prev",			"P"},
		{"<Actions>/Menu/View/Goto/Next",			"N"},
		{"<Actions>/Menu/View/Goto/PrevUnread",			"<shift>P"},
		{"<Actions>/Menu/View/Goto/NextUnread",			"<shift>N"},
		{"<Actions>/Menu/View/Goto/PrevHistory",		"<alt>Left"},
		{"<Actions>/Menu/View/Goto/NextHistory",		"<alt>Right"},
		{"<Actions>/Menu/View/Goto/ParentMessage",		"<control>Up"},
		{"<Actions>/Menu/View/Goto/NextUnreadFolder",		"<shift>G"},
		{"<Actions>/Menu/View/Goto/Folder",			"G"},
		{"<Actions>/Menu/View/Goto/NextPart",			"A"},
		{"<Actions>/Menu/View/Goto/PrevPart",			"Z"},
		{"<Actions>/Menu/View/OpenNewWindow",			"<control><alt>N"},
		{"<Actions>/Menu/View/MessageSource",			"<control>U"},
		{"<Actions>/Menu/View/Part/AsText",			"T"},
		{"<Actions>/Menu/View/Part/Open",			"L"},
		{"<Actions>/Menu/View/Part/OpenWith",			"O"},
		{"<Actions>/Menu/View/UpdateSummary",			"<control><alt>U"},

		{"<Actions>/Menu/Message/Receive/CurrentAccount",	"<control>I"},
		{"<Actions>/Menu/Message/Receive/AllAccounts",		"<shift><control>I"},
		{"<Actions>/Menu/Message/ComposeEmail",			"<control>M"},
		{"<Actions>/Menu/Message/Reply",			"<control>R"},
		{"<Actions>/Menu/Message/ReplyTo/All",			"<shift><control>R"},
		{"<Actions>/Menu/Message/ReplyTo/Sender",		""},
		{"<Actions>/Menu/Message/ReplyTo/List",			"<control>L"},
		{"<Actions>/Menu/Message/Forward",			"<control><alt>F"},
		{"<Actions>/Menu/Message/Move",				"<control>O"},
		{"<Actions>/Menu/Message/Copy",				"<shift><control>O"},
		{"<Actions>/Menu/Message/Trash",			"<control>D"},
		{"<Actions>/Menu/Message/Marks/Mark",			"<shift>asterisk"},
		{"<Actions>/Menu/Message/Marks/Unmark",			"U"},
		{"<Actions>/Menu/Message/Marks/MarkUnread",		"<shift>exclam"},
		{"<Actions>/Menu/Message/Marks/MarkRead",		""},

		{"<Actions>/Menu/Tools/AddressBook",			"<shift><control>A"},
		{"<Actions>/Menu/Tools/ListUrls",			"<shift><control>U"}, 
		{"<Actions>/Menu/Tools/Execute",			"X"},
		{"<Actions>/Menu/Tools/Expunge",			"<control>E"}, 
		{"<Actions>/Menu/Tools/NetworkLog",			"<shift><control>L"},

		/* compose */
		{"<Actions>/Menu/Message/Send",				"<control>Return"},
		{"<Actions>/Menu/Message/SendLater",			"<shift><control>S"},
		{"<Actions>/Menu/Message/AttachFile",			"<control>M"},
		{"<Actions>/Menu/Message/InsertFile",			"<control>I"},
		{"<Actions>/Menu/Message/InsertSig",			"<control>G"},
		{"<Actions>/Menu/Message/Save",				"<control>S"},
		{"<Actions>/Menu/Message/Close",			"<control>W"},
		{"<Actions>/Menu/Edit/Undo",				"<control>Z"},
		{"<Actions>/Menu/Edit/Redo",				"<control>Y"},
		{"<Actions>/Menu/Edit/Cut",				"<control>X"},
		{"<Actions>/Menu/Edit/Copy",				"<control>C"},
		{"<Actions>/Menu/Edit/Paste",				"<control>V"},
		{"<Actions>/Menu/Edit/SelectAll",			"<control>A"},
		{"<Actions>/Menu/Edit/Advanced/BackChar",		"<control>B"},
		{"<Actions>/Menu/Edit/Advanced/ForwChar",		"<control>F"},
		{"<Actions>/Menu/Edit/Advanced/BackWord",		""},
		{"<Actions>/Menu/Edit/Advanced/ForwWord",		""},
		{"<Actions>/Menu/Edit/Advanced/BegLine",		""},
		{"<Actions>/Menu/Edit/Advanced/EndLine",		"<control>E"},
		{"<Actions>/Menu/Edit/Advanced/PrevLine",		"<control>P"},
		{"<Actions>/Menu/Edit/Advanced/NextLine",		"<control>N"},
		{"<Actions>/Menu/Edit/Advanced/DelBackChar",		"<control>H"},
		{"<Actions>/Menu/Edit/Advanced/DelForwChar",		"<control>D"},
		{"<Actions>/Menu/Edit/Advanced/DelBackWord",		""},
		{"<Actions>/Menu/Edit/Advanced/DelForwWord",		""},
		{"<Actions>/Menu/Edit/Advanced/DelLine",		"<control>U"},
		{"<Actions>/Menu/Edit/Advanced/DelEndLine",		"<control>K"},
		{"<Actions>/Menu/Edit/WrapPara",			"<control>L"},
		{"<Actions>/Menu/Edit/WrapAllLines",			"<control><alt>L"},
		{"<Actions>/Menu/Edit/AutoWrap",			"<shift><control>L"},
		{"<Actions>/Menu/Edit/ExtEditor",			"<shift><control>X"},
	};

	static struct KeyBind mew_wl_menurc[] = {
		/* main */
		{"<Actions>/Menu/File/EmptyTrashes",			"<shift>D"},
		{"<Actions>/Menu/File/SaveAs",				"Y"},
		{"<Actions>/Menu/File/Print",				"<control>numbersign"},
		{"<Actions>/Menu/File/Exit",				"<shift>Q"},

		{"<Actions>/Menu/Edit/Copy",				"<control>C"},
		{"<Actions>/Menu/Edit/SelectAll",			"<control>A"},
		{"<Actions>/Menu/Edit/Find",				"<control>F"},
		{"<Actions>/Menu/Edit/SearchFolder",			"<control>S"},
		{"<Actions>/Menu/Edit/QuickSearch",			"slash"},

		{"<Actions>/Menu/View/ShowHide/MessageView",		""},
		{"<Actions>/Menu/View/ThreadView",			"<shift>T"},
		{"<Actions>/Menu/View/Goto/Prev",			"P"},
		{"<Actions>/Menu/View/Goto/Next",			"N"},
		{"<Actions>/Menu/View/Goto/PrevUnread",			"<shift>P"},
		{"<Actions>/Menu/View/Goto/NextUnread",			"<shift>N"},
		{"<Actions>/Menu/View/Goto/Folder",			"G"},
		{"<Actions>/Menu/View/OpenNewWindow",			"<control><alt>N"},
		{"<Actions>/Menu/View/MessageSource",			"<control>U"},
		{"<Actions>/Menu/View/AllHeaders",			"<shift>H"},
		{"<Actions>/Menu/View/UpdateSummary",			"<shift>S"},

		{"<Actions>/Menu/Message/Receive/CurrentAccount",
									"<control>I"},
		{"<Actions>/Menu/Message/Receive/AllAccounts",		"<shift><control>I"},
		{"<Actions>/Menu/Message/ComposeEmail",			"W"},
		{"<Actions>/Menu/Message/Reply",			"<control>R"},
		{"<Actions>/Menu/Message/ReplyTo/All",			"<shift>A"},
		{"<Actions>/Menu/Message/ReplyTo/Sender",		""},
		{"<Actions>/Menu/Message/ReplyTo/List",			"<control>L"},
		{"<Actions>/Menu/Message/Forward",			"F"},
		{"<Actions>/Menu/Message/Move",				"O"},
		{"<Actions>/Menu/Message/Copy",				"<shift>O"},
		{"<Actions>/Menu/Message/Trash",			"D"},
		{"<Actions>/Menu/Message/Marks/Mark",			"<shift>asterisk"},
		{"<Actions>/Menu/Message/Marks/Unmark",			"U"},
		{"<Actions>/Menu/Message/Marks/MarkUnread",		"<shift>exclam"},
		{"<Actions>/Menu/Message/Marks/MarkRead",		"<shift>R"},

		{"<Actions>/Menu/Tools/AddressBook",			"<shift><control>A"},
		{"<Actions>/Menu/Tools/Execute",			"X"},
		{"<Actions>/Menu/Tools/NetworkLog",			"<shift><control>L"},

		/* compose */
		{"<Actions>/Menu/Message/Close",			"<alt>W"},
		{"<Actions>/Menu/Edit/SelectAll",			""},
		{"<Actions>/Menu/Edit/Advanced/BackChar",		"<alt>B"},
		{"<Actions>/Menu/Edit/Advanced/ForwChar",		"<alt>F"},
		{"<Actions>/Menu/Edit/Advanced/BackWord",		""},
		{"<Actions>/Menu/Edit/Advanced/ForwWord",		""},
		{"<Actions>/Menu/Edit/Advanced/BegLine",		"<control>A"},
		{"<Actions>/Menu/Edit/Advanced/DelBackWord",		"<control>W"},
		{"<Actions>/Menu/Edit/Advanced/DelForwWord",		"<alt>D"},
	};

	static struct KeyBind mutt_menurc[] = {
		/* main */
		{"<Actions>/Menu/File/SaveAs",				"S"}, /* save-message */
		{"<Actions>/Menu/File/Print",				"P"}, /* print-message */
		{"<Actions>/Menu/File/Exit",				"Q"}, /* quit */

		{"<Actions>/Menu/Edit/Copy",				"<control>C"}, /* - */
		{"<Actions>/Menu/Edit/SelectAll",			"<control>A"}, /* - */
		{"<Actions>/Menu/Edit/Find",				"<alt>B"}, /* <esc>B: search in message bodies */
		{"<Actions>/Menu/Edit/SearchFolder",			"slash"}, /* search */
		{"<Actions>/Menu/Edit/QuickSearch",			"L"}, /* limit */

		{"<Actions>/Menu/View/ShowHide/MessageView",		"V"}, /* - */
		{"<Actions>/Menu/View/ThreadView",			"<control>T"}, /* - */
		{"<Actions>/Menu/View/Goto/Prev",			"K"}, /* previous-entry */
		{"<Actions>/Menu/View/Goto/Next",			"J"}, /* next-entry */
		{"<Actions>/Menu/View/Goto/PrevUnread",			"<alt>U"}, /* <esc>Tab: previous-new-then-unread */
		{"<Actions>/Menu/View/Goto/NextUnread",			"U"}, /* Tab: next-new-then-unread */
		{"<Actions>/Menu/View/Goto/Folder",			"C"}, /* change-folder */
		{"<Actions>/Menu/View/OpenNewWindow",			"<control><alt>N"}, /* - */
		{"<Actions>/Menu/View/MessageSource",			"E"}, /* edit the raw message */
		{"<Actions>/Menu/View/AllHeaders",			"H"}, /* display-toggle-weed */
		{"<Actions>/Menu/View/UpdateSummary",			"<control><alt>U"}, /* - */

		{"<Actions>/Menu/Message/Receive/CurrentAccount",	"<control>I"}, /* - */
		{"<Actions>/Menu/Message/Receive/AllAccounts",		"<shift>G"}, /* fetch-mail */
		{"<Actions>/Menu/Message/ComposeEmail",			"M"}, /* mail */
		{"<Actions>/Menu/Message/Reply",			"R"}, /* reply */
		{"<Actions>/Menu/Message/ReplyTo/All",			"G"}, /* group-reply */
		{"<Actions>/Menu/Message/ReplyTo/List",			"<shift>L"}, /* list-reply */
		{"<Actions>/Menu/Message/Forward",			"F"}, /* forward-message */
		{"<Actions>/Menu/Message/Move",				"<control>O"}, /* - */
		{"<Actions>/Menu/Message/Copy",				"<shift>C"}, /* copy-message */
		{"<Actions>/Menu/Message/Trash",			"D"}, /* delete-message */
		{"<Actions>/Menu/Message/Marks/Mark",			"<shift>F"}, /* flag-message */
		{"<Actions>/Menu/Message/Marks/Unmark",			"<shift><control>F"}, /* - */
		{"<Actions>/Menu/Message/Marks/MarkUnread",		"<shift>N"}, /* toggle-new */
		{"<Actions>/Menu/Message/Marks/MarkRead",		"<control>R"}, /* read-thread */

		{"<Actions>/Menu/Tools/AddressBook",			"<shift><control>A"}, /* - */
		{"<Actions>/Menu/Tools/Execute",			"dollar"}, /* sync-mailbox */
		{"<Actions>/Menu/Tools/NetworkLog",			"<shift><control>L"}, /* - */

		/* compose */
		{"<Actions>/Menu/Message/Close",			"<alt>W"}, /* - */
		{"<Actions>/Menu/Edit/Advanced/BackWord",		"<alt>B"}, /* - */
		{"<Actions>/Menu/Edit/Advanced/ForwWord",		"<alt>F"}, /* - */
		{"<Actions>/Menu/Edit/Advanced/BegLine",		"<control>A"}, /* - */
		{"<Actions>/Menu/Edit/Advanced/DelBackWord",		"<control>W"}, /* - */
		{"<Actions>/Menu/Edit/Advanced/DelForwWord",		"<alt>D"}, /* - */
	};

	text = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(widget));

	if (!strcmp(text, _("Default"))) {
		menurc = default_menurc;
		n_menurc = G_N_ELEMENTS(default_menurc);
	} else if (!strcmp(text, "Mew / Wanderlust")) {
		menurc = mew_wl_menurc;
		n_menurc = G_N_ELEMENTS(mew_wl_menurc);
	} else if (!strcmp(text, "Mutt")) {
		menurc = mutt_menurc;
		n_menurc = G_N_ELEMENTS(mutt_menurc);
	} else {
		g_free(text);
		return;
	}
	g_free(text);

	prefs_keybind_apply(menurc, n_menurc);
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
	GtkWidget *checkbtn_gtk_enable_accels;
	GtkWidget *keys_preset_hbox;
	GtkWidget *keys_preset_label;
	GtkWidget *keys_preset_combo;

	GtkWidget *label_iotimeout;
	GtkWidget *spinbtn_iotimeout;
	GtkAdjustment *spinbtn_iotimeout_adj;

	GtkWidget *vbox2;
	GtkWidget *checkbtn_transhdr;
	GtkWidget *checkbtn_askonclean;
	GtkWidget *checkbtn_askonfilter;
	GtkWidget *checkbtn_use_shred;
	GtkWidget *checkbtn_real_time_sync;
	GtkWidget *label_attach_save_chmod;
	GtkWidget *entry_attach_save_chmod;

	GtkWidget *frame_metadata;
	GtkWidget *vbox_metadata;
	GtkWidget *metadata_label;
	GtkWidget *flush_metadata_faster_radiobtn;
	GtkWidget *flush_metadata_safer_radiobtn;

#ifndef PASSWORD_CRYPTO_OLD
	GtkWidget *vbox_passphrase;
	GtkWidget *frame_passphrase;
	GtkWidget *checkbtn_use_passphrase;
	GtkWidget *button_change_passphrase;
#endif

	gchar *shred_binary = NULL;

	vbox1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, VSPACING);
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

	hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 32);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox_exit), hbox1, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON (hbox1, checkbtn_cleanonexit,
			   _("Empty trash on exit"));

	PACK_CHECK_BUTTON (vbox_exit, checkbtn_warnqueued,
			   _("Warn if there are queued messages"));

	vbox_keys = gtkut_get_options_frame(vbox1, &frame_keys, _("Keyboard shortcuts"));

	PACK_CHECK_BUTTON(vbox_keys, checkbtn_gtk_enable_accels,
			_("Enable keyboard shortcuts"));

	keys_preset_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (keys_preset_hbox);
	gtk_box_pack_start (GTK_BOX (vbox_keys), keys_preset_hbox, FALSE, FALSE, 0);

	keys_preset_label = gtk_label_new
		(_("Select preset keyboard shortcuts:"));
	gtk_box_pack_start (GTK_BOX (keys_preset_hbox), keys_preset_label, FALSE, FALSE, 0);
	gtk_label_set_justify (GTK_LABEL (keys_preset_label), GTK_JUSTIFY_LEFT);

	keys_preset_combo = combobox_text_new(FALSE,
			       _("Current"),
			       _("Default"),
			       "Mew / Wanderlust",
			       "Mutt",
			       NULL);
	gtk_box_pack_start (GTK_BOX (keys_preset_hbox), keys_preset_combo, FALSE, FALSE, 0);
	gtk_widget_show_all(frame_keys);
	SET_TOGGLE_SENSITIVITY (checkbtn_gtk_enable_accels, keys_preset_hbox);

	vbox_metadata = gtkut_get_options_frame(vbox1, &frame_metadata, _("Metadata handling"));
	metadata_label = gtk_label_new(_("Safer mode asks the OS to write metadata to disk directly;\n"
					 "it avoids data loss after crashes but can take some time"));
	gtk_label_set_xalign(GTK_LABEL(metadata_label), 0.0);
	gtk_label_set_yalign(GTK_LABEL(metadata_label), 0.0);
	gtk_box_pack_start (GTK_BOX (vbox_metadata), metadata_label, FALSE, FALSE, 0);
	flush_metadata_safer_radiobtn = gtk_radio_button_new_with_label(NULL, _("Safer"));
	flush_metadata_faster_radiobtn = gtk_radio_button_new_with_label_from_widget(
					   GTK_RADIO_BUTTON(flush_metadata_safer_radiobtn), _("Faster"));
	hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox_metadata), hbox1, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox1), flush_metadata_safer_radiobtn, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox1), flush_metadata_faster_radiobtn, FALSE, FALSE, 0);
	
	if (prefs_common.flush_metadata)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(flush_metadata_safer_radiobtn), TRUE);
	else
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(flush_metadata_faster_radiobtn), TRUE);

	gtk_widget_show_all(frame_metadata);

	hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);

	label_iotimeout = gtk_label_new (_("Socket I/O timeout"));
	gtk_widget_show (label_iotimeout);
	gtk_box_pack_start (GTK_BOX (hbox1), label_iotimeout, FALSE, FALSE, 0);

	spinbtn_iotimeout_adj = GTK_ADJUSTMENT(gtk_adjustment_new (60, 0, 1000, 1, 10, 0));
	spinbtn_iotimeout = gtk_spin_button_new
		(GTK_ADJUSTMENT (spinbtn_iotimeout_adj), 1, 0);
	gtk_widget_show (spinbtn_iotimeout);
	gtk_box_pack_start (GTK_BOX (hbox1), spinbtn_iotimeout,
			    FALSE, FALSE, 0);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbtn_iotimeout), TRUE);

	label_iotimeout = gtk_label_new (_("seconds"));
	gtk_widget_show (label_iotimeout);
	gtk_box_pack_start (GTK_BOX (hbox1), label_iotimeout, FALSE, FALSE, 0);

	vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON(vbox2, checkbtn_transhdr,
			   _("Translate header names"));
	CLAWS_SET_TIP(checkbtn_transhdr,
			     _("The display of standard headers (such as 'From:', 'Subject:') "
			     "will be translated into your language"));
	PACK_CHECK_BUTTON (vbox2, checkbtn_askonclean, 
			   _("Ask before emptying trash"));
	PACK_CHECK_BUTTON (vbox2, checkbtn_askonfilter,
			   _("Ask about account specific filtering rules when "
			     "filtering manually"));
	shred_binary = g_find_program_in_path("shred");
	if (shred_binary) {
		PACK_CHECK_BUTTON (vbox2, checkbtn_use_shred,
				   _("Use secure file deletion if possible"));
		g_free(shred_binary);
	} else {
		PACK_CHECK_BUTTON (vbox2, checkbtn_use_shred,
				   _("Use secure file deletion if possible\n"
				     "(the 'shred' program is not available)"));
		gtk_widget_set_sensitive(checkbtn_use_shred, FALSE);
	}
	CLAWS_SET_TIP(checkbtn_use_shred,
			_("Use the 'shred' program to overwrite files with random data before "
			  "deleting them. This slows down deletion. Be sure to "
			  "read shred's man page for caveats"));
	PACK_CHECK_BUTTON (vbox2, checkbtn_real_time_sync,
			   _("Synchronise offline folders as soon as possible"));


	hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show(hbox1);
	gtk_box_pack_start(GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);

	label_attach_save_chmod = gtk_label_new (_("Save attachments with chmod"));
	gtk_widget_show(label_attach_save_chmod);
	gtk_box_pack_start(GTK_BOX (hbox1), label_attach_save_chmod, FALSE, FALSE, 0);

	entry_attach_save_chmod = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(entry_attach_save_chmod), 5);
	gtk_widget_set_tooltip_text(entry_attach_save_chmod,
			_("By default attachments are saved with chmod value 600: "
			  "readable and writeable by the user only. If this is too "
			  "restrictive for you, set a chmod value here, otherwise leave "
			  "blank to use the default"));
	gtk_widget_show(entry_attach_save_chmod);
	gtk_box_pack_start(GTK_BOX(hbox1), entry_attach_save_chmod, FALSE, FALSE, 0);
	if (prefs_common.attach_save_chmod) {
		gchar *buf;

		buf = g_strdup_printf("%o", prefs_common.attach_save_chmod);
		gtk_entry_set_text(GTK_ENTRY(entry_attach_save_chmod), buf);
		g_free(buf);
	}

#ifndef PASSWORD_CRYPTO_OLD
	vbox_passphrase = gtkut_get_options_frame(vbox1, &frame_passphrase, _("Primary passphrase"));

	PACK_CHECK_BUTTON(vbox_passphrase, checkbtn_use_passphrase,
			_("Use a primary passphrase"));

	CLAWS_SET_TIP(checkbtn_use_passphrase,
			_("If checked, your saved account passwords will be protected "
				"by a primary passphrase. If no primary passphrase is set, "
				"you will be prompted to set one"));

	button_change_passphrase = gtk_button_new_with_label(
			_("Change primary passphrase"));
	gtk_widget_show (button_change_passphrase);
	hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox_passphrase), hbox1, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox1), button_change_passphrase,
			FALSE, FALSE, 0);
	SET_TOGGLE_SENSITIVITY (checkbtn_use_passphrase, button_change_passphrase);
	g_signal_connect (G_OBJECT (button_change_passphrase), "clicked",
			  G_CALLBACK (prefs_change_primary_passphrase), NULL);
#endif

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
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_gtk_enable_accels),
		prefs_common.gtk_enable_accels);
	gtk_widget_set_sensitive(keys_preset_hbox, prefs_common.gtk_enable_accels);

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbtn_iotimeout),
		prefs_common.io_timeout_secs);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_transhdr),
		prefs_common.trans_hdr);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_askonfilter), 
		prefs_common.ask_apply_per_account_filtering_rules);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_use_shred), 
		prefs_common.use_shred);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_real_time_sync), 
		prefs_common.real_time_sync);

#ifndef PASSWORD_CRYPTO_OLD
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_use_passphrase),
		prefs_common.use_primary_passphrase);
	gtk_widget_set_sensitive(button_change_passphrase,
			prefs_common.use_primary_passphrase);
#endif

	prefs_other->keys_preset_hbox = keys_preset_hbox;
	prefs_other->keys_preset_combo = keys_preset_combo;
	prefs_other->checkbtn_addaddrbyclick = checkbtn_addaddrbyclick;
	prefs_other->checkbtn_confonexit = checkbtn_confonexit;
	prefs_other->checkbtn_cleanonexit = checkbtn_cleanonexit;
	prefs_other->checkbtn_askonclean = checkbtn_askonclean;
	prefs_other->checkbtn_warnqueued = checkbtn_warnqueued;
	prefs_other->spinbtn_iotimeout = spinbtn_iotimeout;
	prefs_other->checkbtn_transhdr = checkbtn_transhdr;
	prefs_other->checkbtn_gtk_enable_accels = checkbtn_gtk_enable_accels;
	prefs_other->checkbtn_askonfilter = checkbtn_askonfilter;
	prefs_other->checkbtn_use_shred = checkbtn_use_shred;
	prefs_other->checkbtn_real_time_sync = checkbtn_real_time_sync;
	prefs_other->entry_attach_save_chmod = entry_attach_save_chmod;
	prefs_other->flush_metadata_safer_radiobtn = flush_metadata_safer_radiobtn;
	prefs_other->flush_metadata_faster_radiobtn = flush_metadata_faster_radiobtn;
#ifndef PASSWORD_CRYPTO_OLD
	prefs_other->checkbtn_use_passphrase = checkbtn_use_passphrase;
#endif
	prefs_other->page.widget = vbox1;
}

static void prefs_other_save(PrefsPage *_page)
{
	OtherPage *page = (OtherPage *) _page;
	GtkSettings *settings = gtk_settings_get_default();
	gboolean gtk_enable_accels;
	gchar *buf;

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
	prefs_common.flush_metadata = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->flush_metadata_safer_radiobtn));
	sock_set_io_timeout(prefs_common.io_timeout_secs);
#ifdef HAVE_LIBETPAN
	imap_main_set_timeout(prefs_common.io_timeout_secs);
#endif
	prefs_common.trans_hdr = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(page->checkbtn_transhdr));
	prefs_common.ask_apply_per_account_filtering_rules = 
		gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(page->checkbtn_askonfilter)); 
	prefs_common.use_shred = 
		gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(page->checkbtn_use_shred)); 
	prefs_common.real_time_sync = 
		gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(page->checkbtn_real_time_sync));

	buf = gtk_editable_get_chars(GTK_EDITABLE(page->entry_attach_save_chmod), 0, -1);
	prefs_common.attach_save_chmod = prefs_chmod_mode(buf);
	g_free(buf);

	prefs_keybind_preset_changed(GTK_COMBO_BOX(page->keys_preset_combo));

#ifndef PASSWORD_CRYPTO_OLD
	/* If we're disabling use of primary passphrase, we need to reencrypt
	 * all account passwords with hardcoded key. */
	if (!gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(page->checkbtn_use_passphrase))
			&& primary_passphrase_is_set()) {
		primary_passphrase_change(NULL, NULL);

		/* In case user did not finish the passphrase change process
		 * (e.g. did not enter a correct current primary passphrase),
		 * we need to enable the "use primary passphrase" checkbox again,
		 * since the old primary passphrase is still valid. */
		if (primary_passphrase_is_set()) {
			gtk_toggle_button_set_active(
				GTK_TOGGLE_BUTTON(page->checkbtn_use_passphrase), TRUE);
		}
	}

	if (gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(page->checkbtn_use_passphrase))
			&& !primary_passphrase_is_set()) {
		primary_passphrase_change_dialog();

		/* In case user cancelled the passphrase change dialog, we need
		 * to disable the "use primary passphrase" checkbox. */
		if (!primary_passphrase_is_set()) {
			gtk_toggle_button_set_active(
				GTK_TOGGLE_BUTTON(page->checkbtn_use_passphrase), FALSE);
		}
	}

	prefs_common.use_primary_passphrase =
		gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(page->checkbtn_use_passphrase));
#endif

	gtk_enable_accels = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->checkbtn_gtk_enable_accels));

	if (prefs_common.gtk_enable_accels != gtk_enable_accels) {
		prefs_common.gtk_enable_accels = gtk_enable_accels;

		g_object_set(G_OBJECT(settings), "gtk-enable-accels",
			     (glong)prefs_common.gtk_enable_accels,
			     NULL);
		g_object_set(G_OBJECT(settings), "gtk-enable-mnemonics",
			     (glong)prefs_common.gtk_enable_accels,
			     NULL);
	}
	
	if (prefs_common.gtk_enable_accels != gtk_enable_accels) {		
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
	GtkSettings *settings = gtk_settings_get_default();
	static gchar *path[3];

	path[0] = _("Other");
	path[1] = _("Miscellaneous");
	path[2] = NULL;

	page = g_new0(OtherPage, 1);
	page->page.path = path;
	page->page.create_widget = prefs_other_create_widget;
	page->page.destroy_widget = prefs_other_destroy_widget;
	page->page.save_page = prefs_other_save;
	page->page.weight = 5.0;
	prefs_gtk_register_page((PrefsPage *) page);
	prefs_other = page;

	g_object_set(G_OBJECT(settings), "gtk-enable-accels",
			(glong)prefs_common.gtk_enable_accels,
			NULL);
	g_object_set(G_OBJECT(settings), "gtk-enable-mnemonics",
			(glong)prefs_common.gtk_enable_accels,
			NULL);
}

void prefs_other_done(void)
{
	prefs_gtk_unregister_page((PrefsPage *) prefs_other);
	g_free(prefs_other);
}

#ifndef PASSWORD_CRYPTO_OLD
void prefs_change_primary_passphrase(GtkButton *button, gpointer data)
{
	/* Call the passphrase change dialog */
	primary_passphrase_change_dialog();
}
#endif
