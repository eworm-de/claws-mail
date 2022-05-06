/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2005-2021 the Claws Mail Team and Colin Leroy <colin@colino.net>
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

#include "prefs_common.h"
#include "prefs_gtk.h"

#include "gtk/gtkutils.h"
#include "gtk/prefswindow.h"
#include "gtk/menu.h"

#include "manage_window.h"

#include "quote_fmt.h"
#include "prefs_template.h"
#include "alertpanel.h"
#include "combobox.h"

typedef struct _WritingPage
{
	PrefsPage page;

	GtkWidget *window;

	GtkWidget *checkbtn_autoextedit;
	GtkWidget *checkbtn_reply_account_autosel;
	GtkWidget *checkbtn_forward_account_autosel;
	GtkWidget *checkbtn_reedit_account_autosel;
	GtkWidget *spinbtn_undolevel;
	GtkWidget *checkbtn_reply_with_quote;
	GtkWidget *checkbtn_default_reply_list;
	GtkWidget *checkbtn_forward_as_attachment;
	GtkWidget *checkbtn_redirect_keep_from;
	GtkWidget *checkbtn_autosave;
	GtkWidget *spinbtn_autosave_length;
	GtkWidget *checkbtn_autosave_encrypted;
	GtkWidget *checkbtn_warn_large_insert;
	GtkWidget *spinbtn_warn_large_insert_size;
	GtkWidget *optmenu_dnd_insert_or_attach;
} WritingPage;

static void prefs_compose_writing_create_widget(PrefsPage *_page, GtkWindow *window, 
			       	  gpointer data)
{
	WritingPage *prefs_writing = (WritingPage *) _page;
	
	GtkWidget *vbox1;
	GtkWidget *vbox2;
	GtkWidget *checkbtn_autoextedit;
	GtkWidget *frame;
	GtkWidget *hbox_autosel;
	GtkWidget *checkbtn_reply_account_autosel;
	GtkWidget *checkbtn_forward_account_autosel;
	GtkWidget *checkbtn_reedit_account_autosel;

	GtkWidget *hbox_undolevel;
	GtkWidget *label_undolevel;
	GtkAdjustment *spinbtn_undolevel_adj;
	GtkWidget *spinbtn_undolevel;

	GtkWidget *hbox_warn_large_insert;
	GtkWidget *checkbtn_warn_large_insert;
	GtkAdjustment *spinbtn_warn_large_insert_adj;
	GtkWidget *spinbtn_warn_large_insert_size;
	GtkWidget *label_warn_large_insert_size;

	GtkWidget *checkbtn_reply_with_quote;
	GtkWidget *checkbtn_default_reply_list;

	GtkWidget *checkbtn_forward_as_attachment;
	GtkWidget *checkbtn_redirect_keep_from;

	GtkWidget *hbox_autosave;
	GtkWidget *checkbtn_autosave;
	GtkAdjustment *spinbtn_autosave_adj;
	GtkWidget *spinbtn_autosave_length;
	GtkWidget *label_autosave_length;

	GtkWidget *hbox_autosave_encrypted;
	GtkWidget *checkbtn_autosave_encrypted;

	GtkWidget *hbox_dnd_insert_or_attach;
	GtkWidget *label_dnd_insert_or_attach;
	GtkWidget *optmenu_dnd_insert_or_attach;
	GtkListStore *menu;
	GtkTreeIter iter;
	gchar *text;

	vbox1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	/* Account autoselection */
	PACK_FRAME(vbox1, frame, _("Automatic account selection"));

	hbox_autosel = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, VSPACING_NARROW);
	gtk_widget_show (hbox_autosel);
	gtk_container_add (GTK_CONTAINER (frame), hbox_autosel);
	gtk_container_set_border_width (GTK_CONTAINER (hbox_autosel), 8);

	PACK_CHECK_BUTTON (hbox_autosel, checkbtn_reply_account_autosel,
			   _("when replying"));
	PACK_CHECK_BUTTON (hbox_autosel, checkbtn_forward_account_autosel,
			   _("when forwarding"));
	PACK_CHECK_BUTTON (hbox_autosel, checkbtn_reedit_account_autosel,
			   _("when re-editing"));

	/* Editing */
	vbox2 = gtkut_get_options_frame(vbox1, &frame, _("Editing"));

	/* Editing: automatically start the text editor */
	PACK_CHECK_BUTTON (vbox2, checkbtn_autoextedit,
			   _("Automatically launch the external editor"));

	/* Editing: automatically save draft */
	hbox_autosave = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox_autosave);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox_autosave, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON (hbox_autosave, checkbtn_autosave,
			   _("Automatically save message to Drafts folder every"));

	spinbtn_autosave_adj = GTK_ADJUSTMENT(gtk_adjustment_new (50, 0, 1000, 1, 10, 0));
	spinbtn_autosave_length = gtk_spin_button_new
		(GTK_ADJUSTMENT (spinbtn_autosave_adj), 1, 0);
	gtk_widget_show (spinbtn_autosave_length);
	gtk_box_pack_start (GTK_BOX (hbox_autosave), spinbtn_autosave_length, FALSE, FALSE, 0);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbtn_autosave_length), TRUE);

	label_autosave_length = gtk_label_new(_("characters"));
	gtk_widget_show (label_autosave_length);
	gtk_box_pack_start (GTK_BOX (hbox_autosave), label_autosave_length, FALSE, FALSE, 0);

	/* Editing: automatically save draft when encrypted */
	hbox_autosave_encrypted = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_box_pack_start(GTK_BOX(hbox_autosave_encrypted), gtk_label_new("   "), FALSE, FALSE, 0);
	gtk_widget_show_all (hbox_autosave_encrypted);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox_autosave_encrypted, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON (hbox_autosave_encrypted, checkbtn_autosave_encrypted,
			   _("Even if message is to be encrypted"));

	/* Editing: undo level */
	hbox_undolevel = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox_undolevel);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox_undolevel, FALSE, FALSE, 0);

	label_undolevel = gtk_label_new (_("Undo level"));
	gtk_widget_show (label_undolevel);
	gtk_box_pack_start (GTK_BOX (hbox_undolevel), label_undolevel, FALSE, FALSE, 0);

	spinbtn_undolevel_adj = GTK_ADJUSTMENT(gtk_adjustment_new (50, 0, 100, 1, 10, 0));
	spinbtn_undolevel = gtk_spin_button_new
		(GTK_ADJUSTMENT (spinbtn_undolevel_adj), 1, 0);
	gtk_widget_show (spinbtn_undolevel);
	gtk_box_pack_start (GTK_BOX (hbox_undolevel), spinbtn_undolevel, FALSE, FALSE, 0);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbtn_undolevel), TRUE);

	/* Editing: warn when inserting large files in message body */
	hbox_warn_large_insert = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox_warn_large_insert);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox_warn_large_insert, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON (hbox_warn_large_insert, checkbtn_warn_large_insert,
			   _("Warn when inserting a file larger than"));

	spinbtn_warn_large_insert_adj = GTK_ADJUSTMENT(gtk_adjustment_new (50, 0, 10000, 1, 10, 0));
	spinbtn_warn_large_insert_size = gtk_spin_button_new
		(GTK_ADJUSTMENT (spinbtn_warn_large_insert_adj), 1, 0);
	gtk_widget_show (spinbtn_warn_large_insert_size);
	gtk_box_pack_start (GTK_BOX (hbox_warn_large_insert),
			spinbtn_warn_large_insert_size, FALSE, FALSE, 0);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbtn_warn_large_insert_size),
			TRUE);

	label_warn_large_insert_size = gtk_label_new(_("KiB into message body"));
	gtk_widget_show (label_warn_large_insert_size);
	gtk_box_pack_start (GTK_BOX (hbox_warn_large_insert),
			label_warn_large_insert_size, FALSE, FALSE, 0);

	/* Replying */
	vbox2 = gtkut_get_options_frame(vbox1, &frame, _("Replying"));

	PACK_CHECK_BUTTON (vbox2, checkbtn_reply_with_quote,
			   _("Reply will quote by default"));

	PACK_CHECK_BUTTON (vbox2, checkbtn_default_reply_list,
			   _("Reply button invokes mailing list reply"));

	vbox2 = gtkut_get_options_frame(vbox1, &frame, _("Forwarding"));

	PACK_CHECK_BUTTON (vbox2, checkbtn_forward_as_attachment,
			   _("Forward as attachment"));

	text = g_strdup_printf(_("Keep the original '%s' header when redirecting"),
			prefs_common_translated_header_name("From"));
	PACK_CHECK_BUTTON (vbox2, checkbtn_redirect_keep_from, text);
	g_free(text);

	/* dnd insert or attach */
	label_dnd_insert_or_attach = gtk_label_new (_("When dropping files into the Compose window"));
	gtk_label_set_xalign(GTK_LABEL(label_dnd_insert_or_attach), 0.0);
	gtk_widget_show (label_dnd_insert_or_attach);

	optmenu_dnd_insert_or_attach = gtkut_sc_combobox_create(NULL, FALSE);
	gtk_widget_show (optmenu_dnd_insert_or_attach);

	menu = GTK_LIST_STORE(gtk_combo_box_get_model(
				GTK_COMBO_BOX(optmenu_dnd_insert_or_attach)));
	COMBOBOX_ADD (menu, _("Ask"), COMPOSE_DND_ASK);
	COMBOBOX_ADD (menu, _("Insert"), COMPOSE_DND_INSERT);
	COMBOBOX_ADD (menu, _("Attach"), COMPOSE_DND_ATTACH);

	hbox_dnd_insert_or_attach = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
	gtk_widget_show(hbox_dnd_insert_or_attach);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox_dnd_insert_or_attach, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox_dnd_insert_or_attach),
			label_dnd_insert_or_attach, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox_dnd_insert_or_attach),
			optmenu_dnd_insert_or_attach, FALSE, FALSE, 0);
	
	SET_TOGGLE_SENSITIVITY (checkbtn_autosave, spinbtn_autosave_length);
	SET_TOGGLE_SENSITIVITY (checkbtn_autosave, label_autosave_length);
	SET_TOGGLE_SENSITIVITY (checkbtn_autosave, checkbtn_autosave_encrypted);

	SET_TOGGLE_SENSITIVITY (checkbtn_warn_large_insert, spinbtn_warn_large_insert_size);
	SET_TOGGLE_SENSITIVITY (checkbtn_warn_large_insert, label_warn_large_insert_size);

	prefs_writing->checkbtn_autoextedit = checkbtn_autoextedit;
	prefs_writing->checkbtn_reply_account_autosel   = checkbtn_reply_account_autosel;
	prefs_writing->checkbtn_forward_account_autosel = checkbtn_forward_account_autosel;
	prefs_writing->checkbtn_reedit_account_autosel  = checkbtn_reedit_account_autosel;

	prefs_writing->spinbtn_undolevel     = spinbtn_undolevel;

	prefs_writing->checkbtn_autosave     = checkbtn_autosave;
	prefs_writing->spinbtn_autosave_length = spinbtn_autosave_length;

	prefs_writing->checkbtn_autosave_encrypted     = checkbtn_autosave_encrypted;

	prefs_writing->checkbtn_warn_large_insert = checkbtn_warn_large_insert;
	prefs_writing->spinbtn_warn_large_insert_size = spinbtn_warn_large_insert_size;
	
	prefs_writing->checkbtn_forward_as_attachment =
		checkbtn_forward_as_attachment;
	prefs_writing->checkbtn_redirect_keep_from = checkbtn_redirect_keep_from;
	prefs_writing->checkbtn_reply_with_quote     = checkbtn_reply_with_quote;
	prefs_writing->checkbtn_default_reply_list = checkbtn_default_reply_list;

	prefs_writing->optmenu_dnd_insert_or_attach = optmenu_dnd_insert_or_attach;

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefs_writing->checkbtn_autoextedit),
		prefs_common.auto_exteditor);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefs_writing->checkbtn_forward_as_attachment),
		prefs_common.forward_as_attachment);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefs_writing->checkbtn_redirect_keep_from),
		prefs_common.redirect_keep_from);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefs_writing->checkbtn_autosave),
		prefs_common.autosave);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefs_writing->checkbtn_autosave_encrypted),
		prefs_common.autosave_encrypted);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(prefs_writing->spinbtn_autosave_length),
		prefs_common.autosave_length);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(prefs_writing->spinbtn_undolevel),
		prefs_common.undolevels);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefs_writing->checkbtn_warn_large_insert),
		prefs_common.warn_large_insert);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(prefs_writing->spinbtn_warn_large_insert_size),
		prefs_common.warn_large_insert_size);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefs_writing->checkbtn_reply_account_autosel),
		prefs_common.reply_account_autosel);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefs_writing->checkbtn_forward_account_autosel),
		prefs_common.forward_account_autosel);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefs_writing->checkbtn_reedit_account_autosel),
		prefs_common.reedit_account_autosel);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefs_writing->checkbtn_reply_with_quote),
			prefs_common.reply_with_quote);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefs_writing->checkbtn_default_reply_list),
		prefs_common.default_reply_list);
	combobox_select_by_data(GTK_COMBO_BOX(optmenu_dnd_insert_or_attach),
		prefs_common.compose_dnd_mode);

	prefs_writing->page.widget = vbox1;
}

static void prefs_compose_writing_save(PrefsPage *_page)
{
	WritingPage *page = (WritingPage *) _page;

	prefs_common.auto_exteditor = 
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_autoextedit));
	prefs_common.forward_as_attachment =
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_forward_as_attachment));
	prefs_common.redirect_keep_from =
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_redirect_keep_from));
	prefs_common.autosave = 
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_autosave));
	prefs_common.autosave_encrypted = 
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_autosave_encrypted));
	prefs_common.autosave_length =
		gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(page->spinbtn_autosave_length));
	prefs_common.undolevels = 
		gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(page->spinbtn_undolevel));
	prefs_common.warn_large_insert = 
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_warn_large_insert));
	prefs_common.warn_large_insert_size =
		gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(page->spinbtn_warn_large_insert_size));
		
	prefs_common.reply_account_autosel =
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_reply_account_autosel));
	prefs_common.forward_account_autosel =
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_forward_account_autosel));
	prefs_common.reedit_account_autosel =
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_reedit_account_autosel));
	prefs_common.reply_with_quote = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(page->checkbtn_reply_with_quote));
	prefs_common.default_reply_list =
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_default_reply_list));
	
	prefs_common.compose_dnd_mode = combobox_get_active_data(
			GTK_COMBO_BOX(page->optmenu_dnd_insert_or_attach));
}

static void prefs_compose_writing_destroy_widget(PrefsPage *_page)
{
}

WritingPage *prefs_writing;

void prefs_compose_writing_init(void)
{
	WritingPage *page;
	static gchar *path[3];

	path[0] = _("Compose");
	path[1] = _("Writing");
	path[2] = NULL;

	page = g_new0(WritingPage, 1);
	page->page.path = path;
	page->page.create_widget = prefs_compose_writing_create_widget;
	page->page.destroy_widget = prefs_compose_writing_destroy_widget;
	page->page.save_page = prefs_compose_writing_save;
	page->page.weight = 190.0;
	prefs_gtk_register_page((PrefsPage *) page);
	prefs_writing = page;
}

void prefs_compose_writing_done(void)
{
	prefs_gtk_unregister_page((PrefsPage *) prefs_writing);
	g_free(prefs_writing);
}
