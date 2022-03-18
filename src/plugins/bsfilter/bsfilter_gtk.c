/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2009 Colin Leroy <colin@colino.net> and 
 * the Claws Mail team
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
#include "claws-features.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>

#include "defs.h"

#include <gtk/gtk.h>
#include "gtk/gtkutils.h"

#include "common/claws.h"
#include "common/version.h"
#include "plugin.h"
#include "common/utils.h"
#include "prefs.h"
#include "folder.h"
#include "prefs_gtk.h"
#include "foldersel.h"
#include "statusbar.h"
#include "bsfilter.h"
#include "menu.h"
#include "addressbook.h"
#include "combobox.h"

struct BsfilterPage
{
	PrefsPage page;

	GtkWidget *process_emails;
	GtkWidget *receive_spam;
	GtkWidget *save_folder;
	GtkWidget *save_folder_select;
	GtkWidget *max_size;
	GtkWidget *bspath;
	GtkWidget *whitelist_ab;
	GtkWidget *whitelist_ab_folder_combo;
	GtkWidget *learn_from_whitelist_chkbtn;
	GtkWidget *mark_as_read;
};

static void foldersel_cb(GtkWidget *widget, gpointer data)
{
	GtkWidget *entry = (GtkWidget *) data;
	FolderItem *item;
	gchar *item_id;
	gint newpos = 0;
	
	item = foldersel_folder_sel(NULL, FOLDER_SEL_MOVE, NULL, FALSE, NULL);
	if (item && (item_id = folder_item_get_identifier(item)) != NULL) {
		gtk_editable_delete_text(GTK_EDITABLE(entry), 0, -1);
		gtk_editable_insert_text(GTK_EDITABLE(entry), item_id, strlen(item_id), &newpos);
		g_free(item_id);
	}
}

#ifndef USE_ALT_ADDRBOOK
static void bsfilter_whitelist_ab_select_cb(GtkWidget *widget, gpointer data)
{
	struct BsfilterPage *page = (struct BsfilterPage *) data;
	const gchar *folderpath = NULL;
	gchar *new_path = NULL;

	folderpath = gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN((page->whitelist_ab_folder_combo)))));
	new_path = addressbook_folder_selection(folderpath);
	if (new_path) {
		gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN((page->whitelist_ab_folder_combo)))), new_path);
		g_free(new_path);
	} 
}
#endif

static void bsfilter_create_widget_func(PrefsPage * _page,
					    GtkWindow * window,
					    gpointer data)
{
	struct BsfilterPage *page = (struct BsfilterPage *) _page;
	BsfilterConfig *config;

	GtkWidget *vbox1, *vbox2;
	GtkWidget *hbox_max_size;
	GtkWidget *hbox_process_emails, *hbox_save_spam;
	GtkWidget *hbox_bspath, *hbox_whitelist;
	GtkWidget *hbox_mark_as_read;

	GtkWidget *max_size_label;
	GtkAdjustment *max_size_spinbtn_adj;
	GtkWidget *max_size_spinbtn;
	GtkWidget *max_size_kb_label;

	GtkWidget *process_emails_checkbtn;

	GtkWidget *save_spam_checkbtn;
	GtkWidget *save_spam_folder_entry;
	GtkWidget *save_spam_folder_select;

	GtkWidget *whitelist_ab_checkbtn;
	GtkWidget *learn_from_whitelist_chkbtn;
	GtkWidget *bspath_label;
	GtkWidget *bspath_entry;

	GtkWidget *mark_as_read_checkbtn;

	GtkWidget *whitelist_ab_folder_combo;
	GtkWidget *whitelist_ab_select_btn;

	vbox1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, FALSE, 0);

	hbox_process_emails = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show(hbox_process_emails);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox_process_emails, TRUE, TRUE, 0);

	process_emails_checkbtn = gtk_check_button_new_with_label(
			_("Process messages on receiving"));
	gtk_widget_show(process_emails_checkbtn);
	gtk_box_pack_start(GTK_BOX(hbox_process_emails), process_emails_checkbtn, TRUE, TRUE, 0);

	hbox_max_size = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show(hbox_max_size);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox_max_size, TRUE, TRUE, 0);

	max_size_label = gtk_label_new(_("Maximum size"));
	gtk_widget_show(max_size_label);
	gtk_box_pack_start(GTK_BOX(hbox_max_size), max_size_label, FALSE, FALSE, 0);

	max_size_spinbtn_adj = gtk_adjustment_new(250, 0, 10000, 10, 10, 0);
	max_size_spinbtn = gtk_spin_button_new(GTK_ADJUSTMENT(max_size_spinbtn_adj), 1, 0);
	gtk_widget_show(max_size_spinbtn);
	gtk_box_pack_start(GTK_BOX(hbox_max_size), max_size_spinbtn, FALSE, FALSE, 0);
	CLAWS_SET_TIP(max_size_spinbtn,
			_("Messages larger than this will not be checked"));
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(max_size_spinbtn), TRUE);

	max_size_kb_label = gtk_label_new(_("KiB"));
	gtk_widget_show(max_size_kb_label);
	gtk_box_pack_start(GTK_BOX(hbox_max_size), max_size_kb_label, FALSE, FALSE, 0);

	hbox_save_spam = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show(hbox_save_spam);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox_save_spam, TRUE, TRUE, 0);

	save_spam_checkbtn = gtk_check_button_new_with_label(_("Save spam in"));
	gtk_widget_show(save_spam_checkbtn);
	gtk_box_pack_start(GTK_BOX(hbox_save_spam), save_spam_checkbtn, FALSE, FALSE, 0);

	save_spam_folder_entry = gtk_entry_new();
	gtk_widget_show (save_spam_folder_entry);
	gtk_box_pack_start (GTK_BOX (hbox_save_spam), save_spam_folder_entry, TRUE, TRUE, 0);
	CLAWS_SET_TIP(save_spam_folder_entry,
			_("Folder for storing identified spam. Leave empty to use the trash folder."));

	save_spam_folder_select = gtkut_get_browse_directory_btn(_("_Browse"));
	gtk_widget_show (save_spam_folder_select);
	gtk_box_pack_start (GTK_BOX (hbox_save_spam), save_spam_folder_select, FALSE, FALSE, 0);
	CLAWS_SET_TIP(save_spam_folder_select,
			_("Click this button to select a folder for storing spam"));

	hbox_whitelist = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show(hbox_whitelist);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox_whitelist, TRUE, TRUE, 0);

	whitelist_ab_checkbtn = gtk_check_button_new_with_label(_("Whitelist senders found in address book/folder"));
	gtk_widget_show(whitelist_ab_checkbtn);
	gtk_box_pack_start(GTK_BOX(hbox_whitelist), whitelist_ab_checkbtn, FALSE, FALSE, 0);
	CLAWS_SET_TIP(whitelist_ab_checkbtn,
			_("Messages coming from your address book contacts will be received in the normal folder even if detected as spam"));

	whitelist_ab_folder_combo = combobox_text_new(TRUE, _("Any"), NULL);
	gtk_widget_set_size_request(whitelist_ab_folder_combo, 100, -1);
	gtk_box_pack_start (GTK_BOX (hbox_whitelist), whitelist_ab_folder_combo, TRUE, TRUE, 0);

	whitelist_ab_select_btn = gtk_button_new_with_label(_("Select..."));
	gtk_widget_show (whitelist_ab_select_btn);
	gtk_box_pack_start (GTK_BOX (hbox_whitelist), whitelist_ab_select_btn, FALSE, FALSE, 0);
	CLAWS_SET_TIP(whitelist_ab_select_btn,
			_("Click this button to select a book or folder in the address book"));

	learn_from_whitelist_chkbtn = gtk_check_button_new_with_label(_("Learn whitelisted emails as ham"));
	CLAWS_SET_TIP(learn_from_whitelist_chkbtn,
			_("If Bsfilter thought an email was spam or unsure, but it was whitelisted, "
			  "learn it as ham."));
	gtk_widget_show(learn_from_whitelist_chkbtn);
	gtk_box_pack_start (GTK_BOX (vbox2), learn_from_whitelist_chkbtn, TRUE, TRUE, 0);

	hbox_bspath = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show(hbox_bspath);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox_bspath, FALSE, FALSE, 0);

	bspath_label = gtk_label_new(_("Bsfilter call"));
	gtk_widget_show(bspath_label);
	gtk_box_pack_start(GTK_BOX(hbox_bspath), bspath_label, FALSE, FALSE, 0);

	bspath_entry = gtk_entry_new();
	gtk_widget_show(bspath_entry);
	gtk_box_pack_start(GTK_BOX(hbox_bspath), bspath_entry, FALSE, FALSE, 0);
	CLAWS_SET_TIP(bspath_entry,
			_("Path to bsfilter executable"));

	hbox_mark_as_read = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show(hbox_mark_as_read);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox_mark_as_read, TRUE, TRUE, 0);

	mark_as_read_checkbtn = gtk_check_button_new_with_label(_("Mark spam as read"));
	gtk_widget_show(mark_as_read_checkbtn);
	gtk_box_pack_start(GTK_BOX(hbox_mark_as_read), mark_as_read_checkbtn, FALSE, FALSE, 0);

	SET_TOGGLE_SENSITIVITY(save_spam_checkbtn, save_spam_folder_entry);
	SET_TOGGLE_SENSITIVITY(save_spam_checkbtn, save_spam_folder_select);
	SET_TOGGLE_SENSITIVITY(whitelist_ab_checkbtn, whitelist_ab_folder_combo);
	SET_TOGGLE_SENSITIVITY(whitelist_ab_checkbtn, whitelist_ab_select_btn);
	SET_TOGGLE_SENSITIVITY(whitelist_ab_checkbtn, learn_from_whitelist_chkbtn);
	SET_TOGGLE_SENSITIVITY(save_spam_checkbtn, mark_as_read_checkbtn);

	config = bsfilter_get_config();

	g_signal_connect(G_OBJECT(save_spam_folder_select), "clicked",
			G_CALLBACK(foldersel_cb), save_spam_folder_entry);
#ifndef USE_ALT_ADDRBOOK
	g_signal_connect(G_OBJECT (whitelist_ab_select_btn), "clicked",
			 G_CALLBACK(bsfilter_whitelist_ab_select_cb), page);
#else
	gtk_widget_set_sensitive(whitelist_ab_select_btn, FALSE);
#endif

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(max_size_spinbtn), (float) config->max_size);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(process_emails_checkbtn), config->process_emails);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(save_spam_checkbtn), config->receive_spam);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(whitelist_ab_checkbtn), config->whitelist_ab);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(learn_from_whitelist_chkbtn), config->learn_from_whitelist);
	if (config->whitelist_ab_folder != NULL) {
		/* translate "Any" (stored UNtranslated) */
		if (strcasecmp(config->whitelist_ab_folder, "Any") == 0)
			gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN((whitelist_ab_folder_combo)))),
					config->whitelist_ab_folder);
		else
		/* backward compatibility (when translated "Any" was stored) */
		if (g_utf8_collate(config->whitelist_ab_folder, _("Any")) == 0)
			gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN((whitelist_ab_folder_combo)))),
					config->whitelist_ab_folder);
		else
			gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN((whitelist_ab_folder_combo)))),
					config->whitelist_ab_folder);
	}
	if (config->save_folder != NULL)
		gtk_entry_set_text(GTK_ENTRY(save_spam_folder_entry), config->save_folder);
	if (config->bspath != NULL)
		gtk_entry_set_text(GTK_ENTRY(bspath_entry), config->bspath);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mark_as_read_checkbtn), config->mark_as_read);

	page->max_size = max_size_spinbtn;
	page->process_emails = process_emails_checkbtn;

	page->receive_spam = save_spam_checkbtn;
	page->save_folder = save_spam_folder_entry;
	page->save_folder_select = save_spam_folder_select;

	page->whitelist_ab = whitelist_ab_checkbtn;
	page->whitelist_ab_folder_combo = whitelist_ab_folder_combo;
	page->learn_from_whitelist_chkbtn = learn_from_whitelist_chkbtn;
	page->bspath = bspath_entry;

	page->mark_as_read = mark_as_read_checkbtn;

	page->page.widget = vbox1;
}

static void bsfilter_destroy_widget_func(PrefsPage *_page)
{
	debug_print("Destroying Bsfilter widget\n");
}

static void bsfilter_save_func(PrefsPage *_page)
{
	struct BsfilterPage *page = (struct BsfilterPage *) _page;
	BsfilterConfig *config;

	debug_print("Saving Bsfilter Page\n");

	config = bsfilter_get_config();

	/* process_emails */
	config->process_emails = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->process_emails));

	/* receive_spam */
	config->receive_spam = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->receive_spam));

	/* save_folder */
	g_free(config->save_folder);
	config->save_folder = gtk_editable_get_chars(GTK_EDITABLE(page->save_folder), 0, -1);

	/* whitelist_ab */
	config->whitelist_ab = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->whitelist_ab));
	g_free(config->whitelist_ab_folder);
	config->whitelist_ab_folder = gtk_editable_get_chars(
				GTK_EDITABLE(gtk_bin_get_child(GTK_BIN((page->whitelist_ab_folder_combo)))), 0, -1);
	/* store UNtranslated "Any" */
	if (g_utf8_collate(config->whitelist_ab_folder, _("Any")) == 0) {
		g_free(config->whitelist_ab_folder);
		config->whitelist_ab_folder = g_strdup("Any");
	}

	/* learn_from_whitelist_chkbtn */
	config->learn_from_whitelist = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->learn_from_whitelist_chkbtn));

	/* bspath */
	g_free(config->bspath);
	config->bspath = gtk_editable_get_chars(GTK_EDITABLE(page->bspath), 0, -1);

	/* max_size */
	config->max_size = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(page->max_size));

	/* mark_as_read */
	config->mark_as_read = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->mark_as_read));

	if (config->process_emails) {
		bsfilter_register_hook();
	} else {
		bsfilter_unregister_hook();
	}

	procmsg_register_spam_learner(bsfilter_learn);
	procmsg_spam_set_folder(config->save_folder, bsfilter_get_spam_folder);

	bsfilter_save_config();
}

typedef struct _BsCbData {
	gchar *message;
	gint total;
	gint done;
} BsCbData;

static gboolean gtk_message_callback(gpointer data)
{
	BsCbData *cbdata = (BsCbData *)data;

	if (cbdata->message)
		statusbar_print_all("%s", cbdata->message);
	else if (cbdata->total == 0) {
		statusbar_pop_all();
	}
	if (cbdata->total && cbdata->done)
		statusbar_progress_all(cbdata->done, cbdata->total, 10);
	else
		statusbar_progress_all(0,0,0);
	g_free(cbdata->message);
	g_free(cbdata);
	GTK_EVENTS_FLUSH();
	return FALSE;
}

static void gtk_safe_message_callback(gchar *message, gint total, gint done, gboolean thread_safe)
{
	BsCbData *cbdata = g_new0(BsCbData, 1);
	if (message)
		cbdata->message = g_strdup(message);
	cbdata->total = total;
	cbdata->done = done;
	if (thread_safe)
		g_timeout_add(0, gtk_message_callback, cbdata);
	else
		gtk_message_callback(cbdata);
}

static struct BsfilterPage bsfilter_page;

gint bsfilter_gtk_init(void)
{
	static gchar *path[3];

	path[0] = _("Plugins");
	path[1] = _("Bsfilter");
	path[2] = NULL;

	bsfilter_page.page.path = path;
	bsfilter_page.page.create_widget = bsfilter_create_widget_func;
	bsfilter_page.page.destroy_widget = bsfilter_destroy_widget_func;
	bsfilter_page.page.save_page = bsfilter_save_func;
	bsfilter_page.page.weight = 35.0;

	prefs_gtk_register_page((PrefsPage *) &bsfilter_page);
	bsfilter_set_message_callback(gtk_safe_message_callback);

	debug_print("Bsfilter GTK plugin loaded\n");
	return 0;	
}

void bsfilter_gtk_done(void)
{
        prefs_gtk_unregister_page((PrefsPage *) &bsfilter_page);
}
