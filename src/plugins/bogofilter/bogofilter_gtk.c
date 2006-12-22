/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2006 Hiroyuki Yamamoto and the Claws Mail Team
 * This file Copyright (C) 2006 Colin Leroy <colin@colino.net>
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

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gtk/gtkutils.h>

#include "common/claws.h"
#include "common/version.h"
#include "plugin.h"
#include "common/utils.h"
#include "prefs.h"
#include "folder.h"
#include "prefs_gtk.h"
#include "foldersel.h"
#include "statusbar.h"
#include "bogofilter.h"
#include "menu.h"
#include "addressbook.h"

struct BogofilterPage
{
	PrefsPage page;

	GtkWidget *process_emails;
	GtkWidget *receive_spam;
	GtkWidget *save_folder;
	GtkWidget *save_folder_select;
	GtkWidget *save_unsure;
	GtkWidget *save_unsure_folder;
	GtkWidget *save_unsure_folder_select;
	GtkWidget *insert_header;
	GtkWidget *max_size;
	GtkWidget *bogopath;
	GtkWidget *whitelist_ab;
	GtkWidget *whitelist_ab_folder_combo;
};

/*!
 *\brief	Preset addressbook book/folder items
 */
static const gchar *whitelist_ab_folder_text [] = {
	N_("Any")
};

static void foldersel_cb(GtkWidget *widget, gpointer data)
{
	GtkWidget *entry = (GtkWidget *) data;
	FolderItem *item;
	gchar *item_id;
	gint newpos = 0;
	
	item = foldersel_folder_sel(NULL, FOLDER_SEL_MOVE, NULL);
	if (item && (item_id = folder_item_get_identifier(item)) != NULL) {
		gtk_editable_delete_text(GTK_EDITABLE(entry), 0, -1);
		gtk_editable_insert_text(GTK_EDITABLE(entry), item_id, strlen(item_id), &newpos);
		g_free(item_id);
	}
}

static void bogofilter_whitelist_ab_select_cb(GtkWidget *widget, gpointer data)
{
	struct BogofilterPage *page = (struct BogofilterPage *) data;
	gchar *folderpath = NULL;
	gboolean ret = FALSE;

	folderpath = (gchar *) gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(page->whitelist_ab_folder_combo)->entry));
	ret = addressbook_folder_selection(&folderpath);
	if ( ret != FALSE && folderpath != NULL)
		gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(page->whitelist_ab_folder_combo)->entry), folderpath);
}

static void bogofilter_create_widget_func(PrefsPage * _page,
					    GtkWindow * window,
					    gpointer data)
{
	struct BogofilterPage *page = (struct BogofilterPage *) _page;
	BogofilterConfig *config;

	GtkWidget *vbox1, *vbox2;
	GtkWidget *hbox_max_size;
	GtkWidget *hbox_process_emails, *hbox_save_spam, *hbox_save_unsure;
	GtkWidget *hbox_bogopath, *hbox_whitelist;

	GtkWidget *max_size_label;
	GtkObject *max_size_spinbtn_adj;
	GtkWidget *max_size_spinbtn;
	GtkWidget *max_size_kb_label;

	GtkWidget *process_emails_checkbtn;

	GtkWidget *save_spam_checkbtn;
	GtkWidget *save_spam_folder_entry;
	GtkWidget *save_spam_folder_select;

	GtkWidget *save_unsure_checkbtn;
	GtkWidget *save_unsure_folder_entry;
	GtkWidget *save_unsure_folder_select;

	GtkWidget *insert_header_chkbtn;
	GtkWidget *whitelist_ab_chkbtn;
	GtkWidget *bogopath_label;
	GtkWidget *bogopath_entry;

	GtkTooltips *tooltips;

	GtkWidget *whitelist_ab_folder_combo;
	GtkWidget *whitelist_ab_select_btn;
	GList *combo_items;
	gint i;

	tooltips = gtk_tooltips_new();

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	vbox2 = gtk_vbox_new (FALSE, 4);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, FALSE, 0);

	hbox_process_emails = gtk_hbox_new(FALSE, 8);
	gtk_widget_show(hbox_process_emails);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox_process_emails, TRUE, TRUE, 0);

	process_emails_checkbtn = gtk_check_button_new_with_label(
			_("Process messages on receiving"));
	gtk_widget_show(process_emails_checkbtn);
	gtk_box_pack_start(GTK_BOX(hbox_process_emails), process_emails_checkbtn, TRUE, TRUE, 0);

	hbox_max_size = gtk_hbox_new(FALSE, 8);
	gtk_widget_show(hbox_max_size);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox_max_size, TRUE, TRUE, 0);

	max_size_label = gtk_label_new(_("Maximum size"));
	gtk_widget_show(max_size_label);
	gtk_box_pack_start(GTK_BOX(hbox_max_size), max_size_label, FALSE, FALSE, 0);

	max_size_spinbtn_adj = gtk_adjustment_new(250, 0, 10000, 10, 10, 10);
	max_size_spinbtn = gtk_spin_button_new(GTK_ADJUSTMENT(max_size_spinbtn_adj), 1, 0);
	gtk_widget_show(max_size_spinbtn);
	gtk_box_pack_start(GTK_BOX(hbox_max_size), max_size_spinbtn, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(tooltips, max_size_spinbtn,
			_("Messages larger than this will not be checked"), NULL);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(max_size_spinbtn), TRUE);

	max_size_kb_label = gtk_label_new(_("kB"));
	gtk_widget_show(max_size_kb_label);
	gtk_box_pack_start(GTK_BOX(hbox_max_size), max_size_kb_label, FALSE, FALSE, 0);

	hbox_save_spam = gtk_hbox_new(FALSE, 8);
	gtk_widget_show(hbox_save_spam);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox_save_spam, TRUE, TRUE, 0);

	save_spam_checkbtn = gtk_check_button_new_with_label(_("Save spam in"));
	gtk_widget_show(save_spam_checkbtn);
	gtk_box_pack_start(GTK_BOX(hbox_save_spam), save_spam_checkbtn, FALSE, FALSE, 0);

	save_spam_folder_entry = gtk_entry_new();
	gtk_widget_show (save_spam_folder_entry);
	gtk_box_pack_start (GTK_BOX (hbox_save_spam), save_spam_folder_entry, TRUE, TRUE, 0);
	gtk_tooltips_set_tip(tooltips, save_spam_folder_entry,
			_("Folder for storing identified spam. Leave empty to use the default trash folder."),
			NULL);

	save_spam_folder_select = gtkut_get_browse_directory_btn(_("_Browse"));
	gtk_widget_show (save_spam_folder_select);
	gtk_box_pack_start (GTK_BOX (hbox_save_spam), save_spam_folder_select, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(tooltips, save_spam_folder_select,
			_("Click this button to select a folder for storing spam"),
			NULL);

	hbox_save_unsure = gtk_hbox_new(FALSE, 8);
	gtk_widget_show(hbox_save_unsure);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox_save_unsure, TRUE, TRUE, 0);

	save_unsure_checkbtn = gtk_check_button_new_with_label(_("When unsure, move in"));
	gtk_widget_show(save_unsure_checkbtn);
	gtk_box_pack_start(GTK_BOX(hbox_save_unsure), save_unsure_checkbtn, FALSE, FALSE, 0);

	save_unsure_folder_entry = gtk_entry_new();
	gtk_widget_show (save_unsure_folder_entry);
	gtk_box_pack_start (GTK_BOX (hbox_save_unsure), save_unsure_folder_entry, TRUE, TRUE, 0);
	gtk_tooltips_set_tip(tooltips, save_unsure_folder_entry,
			_("Folder for storing mail for which spam status is Unsure. Leave empty to use the default inbox folder."),
			NULL);

	save_unsure_folder_select = gtkut_get_browse_directory_btn(_("_Browse"));
	gtk_widget_show (save_unsure_folder_select);
	gtk_box_pack_start (GTK_BOX (hbox_save_unsure), save_unsure_folder_select, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(tooltips, save_unsure_folder_select,
			_("Click this button to select a folder for storing Unsure mails"),
			NULL);

	insert_header_chkbtn = gtk_check_button_new_with_label(_("Insert X-Bogosity header"));
	gtk_widget_show(insert_header_chkbtn);
	gtk_box_pack_start(GTK_BOX(vbox2), insert_header_chkbtn, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(tooltips, insert_header_chkbtn,
			_("Only done for messages in MH folders"),
			NULL);

	hbox_whitelist = gtk_hbox_new(FALSE, 8);
	gtk_widget_show(hbox_whitelist);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox_whitelist, TRUE, TRUE, 0);

	whitelist_ab_chkbtn = gtk_check_button_new_with_label(_("Whitelist senders present in addressbook/folder"));
	gtk_widget_show(whitelist_ab_chkbtn);
	gtk_box_pack_start(GTK_BOX(hbox_whitelist), whitelist_ab_chkbtn, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(tooltips, whitelist_ab_chkbtn,
			_("Messages coming from your addressbook contacts will be received in the normal folder even if detected as spam"), NULL);

	whitelist_ab_folder_combo = gtk_combo_new();
	gtk_widget_show(whitelist_ab_folder_combo);
	gtk_widget_set_size_request(whitelist_ab_folder_combo, 100, -1);
	gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(whitelist_ab_folder_combo)->entry),
			       TRUE);

	combo_items = NULL;
	for (i = 0; i < (gint) (sizeof(whitelist_ab_folder_text) / sizeof(gchar *)); i++) {
		combo_items = g_list_append(combo_items,
					    (gpointer) _(whitelist_ab_folder_text[i]));
	}
	gtk_combo_set_popdown_strings(GTK_COMBO(whitelist_ab_folder_combo), combo_items);
	g_list_free(combo_items);

	gtk_box_pack_start (GTK_BOX (hbox_whitelist), whitelist_ab_folder_combo, TRUE, TRUE, 0);

	whitelist_ab_select_btn = gtk_button_new_with_label(_(" Select... "));
	gtk_widget_show (whitelist_ab_select_btn);
	gtk_box_pack_start (GTK_BOX (hbox_whitelist), whitelist_ab_select_btn, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(tooltips, whitelist_ab_select_btn,
			_("Click this button to select a book or folder in the addressbook"),
			NULL);

	hbox_bogopath = gtk_hbox_new(FALSE, 8);
	gtk_widget_show(hbox_bogopath);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox_bogopath, FALSE, FALSE, 0);

	bogopath_label = gtk_label_new(_("Bogofilter call"));
	gtk_widget_show(bogopath_label);
	gtk_box_pack_start(GTK_BOX(hbox_bogopath), bogopath_label, FALSE, FALSE, 0);

	bogopath_entry = gtk_entry_new();
	gtk_widget_show(bogopath_entry);
	gtk_box_pack_start(GTK_BOX(hbox_bogopath), bogopath_entry, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(tooltips, bogopath_entry,
			_("Path to bogofilter executable"),
			NULL);

	SET_TOGGLE_SENSITIVITY(save_spam_checkbtn, save_spam_folder_entry);
	SET_TOGGLE_SENSITIVITY(save_spam_checkbtn, save_spam_folder_select);
	SET_TOGGLE_SENSITIVITY(save_spam_checkbtn, insert_header_chkbtn);
	SET_TOGGLE_SENSITIVITY(save_unsure_checkbtn, save_unsure_folder_entry);
	SET_TOGGLE_SENSITIVITY(save_unsure_checkbtn, save_unsure_folder_select);
	SET_TOGGLE_SENSITIVITY(save_unsure_checkbtn, insert_header_chkbtn);
	SET_TOGGLE_SENSITIVITY(whitelist_ab_chkbtn, whitelist_ab_folder_combo);
	SET_TOGGLE_SENSITIVITY(whitelist_ab_chkbtn, whitelist_ab_select_btn);

	config = bogofilter_get_config();

	g_signal_connect(G_OBJECT(save_spam_folder_select), "clicked",
			G_CALLBACK(foldersel_cb), save_spam_folder_entry);
	g_signal_connect(G_OBJECT(save_unsure_folder_select), "clicked",
			G_CALLBACK(foldersel_cb), save_unsure_folder_entry);
	g_signal_connect(G_OBJECT (whitelist_ab_select_btn), "clicked",
			 G_CALLBACK(bogofilter_whitelist_ab_select_cb), page);

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(max_size_spinbtn), (float) config->max_size);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(process_emails_checkbtn), config->process_emails);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(save_spam_checkbtn), config->receive_spam);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(save_unsure_checkbtn), config->save_unsure);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(insert_header_chkbtn), config->insert_header);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(whitelist_ab_chkbtn), config->whitelist_ab);
	if (config->whitelist_ab_folder != NULL)
		gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(whitelist_ab_folder_combo)->entry),
				config->whitelist_ab_folder);
	if (config->save_folder != NULL)
		gtk_entry_set_text(GTK_ENTRY(save_spam_folder_entry), config->save_folder);
	if (config->save_unsure_folder != NULL)
		gtk_entry_set_text(GTK_ENTRY(save_unsure_folder_entry), config->save_unsure_folder);
	if (config->bogopath != NULL)
		gtk_entry_set_text(GTK_ENTRY(bogopath_entry), config->bogopath);

	page->max_size = max_size_spinbtn;
	page->process_emails = process_emails_checkbtn;

	page->receive_spam = save_spam_checkbtn;
	page->save_folder = save_spam_folder_entry;
	page->save_folder_select = save_spam_folder_select;

	page->save_unsure = save_unsure_checkbtn;
	page->save_unsure_folder = save_unsure_folder_entry;
	page->save_unsure_folder_select = save_unsure_folder_select;

	page->insert_header = insert_header_chkbtn;
	page->whitelist_ab = whitelist_ab_chkbtn;
	page->whitelist_ab_folder_combo = whitelist_ab_folder_combo;
	page->bogopath = bogopath_entry;

	page->page.widget = vbox1;
}

static void bogofilter_destroy_widget_func(PrefsPage *_page)
{
	debug_print("Destroying Bogofilter widget\n");
}

static void bogofilter_save_func(PrefsPage *_page)
{
	struct BogofilterPage *page = (struct BogofilterPage *) _page;
	BogofilterConfig *config;

	debug_print("Saving Bogofilter Page\n");

	config = bogofilter_get_config();

	/* process_emails */
	config->process_emails = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->process_emails));

	/* receive_spam */
	config->receive_spam = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->receive_spam));

	/* save_folder */
	g_free(config->save_folder);
	config->save_folder = gtk_editable_get_chars(GTK_EDITABLE(page->save_folder), 0, -1);

	/* save_unsure */
	config->save_unsure = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->save_unsure));

	/* save_unsure_folder */
	g_free(config->save_unsure_folder);
	config->save_unsure_folder = gtk_editable_get_chars(GTK_EDITABLE(page->save_unsure_folder), 0, -1);

	/* insert_header */
	config->insert_header = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->insert_header));

	/* whitelist_ab */
	config->whitelist_ab = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->whitelist_ab));
	g_free(config->whitelist_ab_folder);
	config->whitelist_ab_folder = gtk_editable_get_chars(
				GTK_EDITABLE(GTK_COMBO(page->whitelist_ab_folder_combo)->entry), 0, -1);

	/* bogopath */
	g_free(config->bogopath);
	config->bogopath = gtk_editable_get_chars(GTK_EDITABLE(page->bogopath), 0, -1);

	/* max_size */
	config->max_size = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(page->max_size));

	if (config->process_emails) {
		bogofilter_register_hook();
	} else {
		bogofilter_unregister_hook();
	}

	procmsg_register_spam_learner(bogofilter_learn);
	procmsg_spam_set_folder(config->save_folder);

	bogofilter_save_config();
}

typedef struct _BogoCbData {
	gchar *message;
	gint total;
	gint done;
} BogoCbData;

static gboolean gtk_message_callback(gpointer data)
{
	BogoCbData *cbdata = (BogoCbData *)data;

	if (cbdata->message)
		statusbar_print_all(cbdata->message);
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
	BogoCbData *cbdata = g_new0(BogoCbData, 1);
	if (message)
		cbdata->message = g_strdup(message);
	cbdata->total = total;
	cbdata->done = done;
	if (thread_safe)
		g_timeout_add(0, gtk_message_callback, cbdata);
	else
		gtk_message_callback(cbdata);
}

static struct BogofilterPage bogofilter_page;

gint bogofilter_gtk_init(void)
{
	static gchar *path[3];

	path[0] = _("Plugins");
	path[1] = _("Bogofilter");
	path[2] = NULL;

	bogofilter_page.page.path = path;
	bogofilter_page.page.create_widget = bogofilter_create_widget_func;
	bogofilter_page.page.destroy_widget = bogofilter_destroy_widget_func;
	bogofilter_page.page.save_page = bogofilter_save_func;
	bogofilter_page.page.weight = 35.0;

	prefs_gtk_register_page((PrefsPage *) &bogofilter_page);
	bogofilter_set_message_callback(gtk_safe_message_callback);

	debug_print("Bogofilter GTK plugin loaded\n");
	return 0;	
}

void bogofilter_gtk_done(void)
{
        prefs_gtk_unregister_page((PrefsPage *) &bogofilter_page);
}
