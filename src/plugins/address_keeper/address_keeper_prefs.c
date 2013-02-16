/*
 * Claws Mail -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2009 Hiroyuki Yamamoto and the Claws Mail Team
 * Copyright (C) 2009-2010 Ricardo Mones
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
#include "claws-features.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>

#include "address_keeper.h"

#include <gtk/gtk.h>

#include "defs.h"
#include "address_keeper_prefs.h"
#include "prefs_common.h"
#include "prefs_gtk.h"

#define PREFS_BLOCK_NAME "AddressKeeper"

AddressKeeperPrefs addkeeperprefs;

struct AddressKeeperPrefsPage
{
	PrefsPage page;
	
	GtkWidget *addressbook_folder;
	GtkWidget *keep_to_addrs_check;
	GtkWidget *keep_cc_addrs_check;
	GtkWidget *keep_bcc_addrs_check;
};

struct AddressKeeperPrefsPage addkeeperprefs_page;

static PrefParam param[] = {
	{"addressbook_folder", "", &addkeeperprefs.addressbook_folder,
         P_STRING, NULL, NULL, NULL},
	{"keep_to_addrs", "TRUE", &addkeeperprefs.keep_to_addrs,
         P_BOOL, NULL, NULL, NULL},
	{"keep_cc_addrs", "TRUE", &addkeeperprefs.keep_cc_addrs,
         P_BOOL, NULL, NULL, NULL},
	{"keep_bcc_addrs", "FALSE", &addkeeperprefs.keep_bcc_addrs,
         P_BOOL, NULL, NULL, NULL},
	{NULL, NULL, NULL, P_OTHER, NULL, NULL, NULL}
};

#ifndef USE_NEW_ADDRBOOK
static void select_addressbook_clicked_cb(GtkWidget *widget, gpointer data) {
	const gchar *folderpath = NULL;
	gchar *new_path = NULL;

	folderpath = gtk_entry_get_text(GTK_ENTRY(data));
	new_path = addressbook_folder_selection(folderpath);
	if (new_path) {
		gtk_entry_set_text(GTK_ENTRY(data), new_path);
		g_free(new_path);
	}
}
#endif

static void addkeeper_prefs_create_widget_func(PrefsPage * _page,
					       GtkWindow * window,
					       gpointer data)
{
	struct AddressKeeperPrefsPage *page = (struct AddressKeeperPrefsPage *) _page;
	GtkWidget *entry;
	GtkWidget *label;
	GtkWidget *button;
	GtkWidget *keep_to_checkbox;
	GtkWidget *keep_cc_checkbox;
	GtkWidget *keep_bcc_checkbox;
	GtkWidget *hbox;
	GtkWidget *vbox;

	vbox = gtk_vbox_new(FALSE, 6);
	hbox = gtk_hbox_new(FALSE, 6);

	label = gtk_label_new(_("Keep to folder"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(entry), addkeeperprefs.addressbook_folder);
	gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
	gtk_widget_show(entry);
	CLAWS_SET_TIP(entry, _("Address book path where addresses are kept"));

	button = gtk_button_new_with_label(_("Select..."));
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
#ifndef USE_NEW_ADDRBOOK
	g_signal_connect(G_OBJECT (button), "clicked",
			 G_CALLBACK (select_addressbook_clicked_cb),
			 entry);
#else
	gtk_widget_set_sensitive(button, FALSE);
#endif
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 6);
	gtk_widget_show(button);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	page->addressbook_folder = entry;

	keep_to_checkbox = gtk_check_button_new_with_label(_("Keep 'To' addresses"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(keep_to_checkbox), addkeeperprefs.keep_to_addrs);
	gtk_container_set_border_width(GTK_CONTAINER(keep_to_checkbox), 6);
	gtk_box_pack_start(GTK_BOX(vbox), keep_to_checkbox, FALSE, FALSE, 0);
	gtk_widget_show(keep_to_checkbox);
	CLAWS_SET_TIP(keep_to_checkbox, _("Keep addresses which appear in 'To' headers"));
	gtk_widget_show(keep_to_checkbox);

	page->keep_to_addrs_check = keep_to_checkbox;

	keep_cc_checkbox = gtk_check_button_new_with_label(_("Keep 'Cc' addresses"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(keep_cc_checkbox), addkeeperprefs.keep_cc_addrs);
	gtk_container_set_border_width(GTK_CONTAINER(keep_cc_checkbox), 6);
	gtk_box_pack_start(GTK_BOX(vbox), keep_cc_checkbox, FALSE, FALSE, 0);
	gtk_widget_show(keep_cc_checkbox);
	CLAWS_SET_TIP(keep_cc_checkbox, _("Keep addresses which appear in 'Cc' headers"));
	gtk_widget_show(keep_cc_checkbox);

	page->keep_cc_addrs_check = keep_cc_checkbox;

	keep_bcc_checkbox = gtk_check_button_new_with_label(_("Keep 'Bcc' addresses"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(keep_bcc_checkbox), addkeeperprefs.keep_bcc_addrs);
	gtk_container_set_border_width(GTK_CONTAINER(keep_bcc_checkbox), 6);
	gtk_box_pack_start(GTK_BOX(vbox), keep_bcc_checkbox, FALSE, FALSE, 0);
	gtk_widget_show(keep_bcc_checkbox);
	CLAWS_SET_TIP(keep_bcc_checkbox, _("Keep addresses which appear in 'Bcc' headers"));
	gtk_widget_show(keep_bcc_checkbox);

	page->keep_bcc_addrs_check = keep_bcc_checkbox;
	
	gtk_widget_show_all(vbox);

	page->page.widget = vbox;
}

static void addkeeper_prefs_destroy_widget_func(PrefsPage *_page)
{
}

static void addkeeper_save_config(void)
{
	PrefFile *pfile;
	gchar *rcpath;

	debug_print("Saving AddressKeeper Page\n");

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMON_RC, NULL);
	pfile = prefs_write_open(rcpath);
	g_free(rcpath);
	if (!pfile || (prefs_set_block_label(pfile, PREFS_BLOCK_NAME) < 0))
		return;

	if (prefs_write_param(param, pfile->fp) < 0) {
		g_warning("Failed to write AddressKeeper configuration to file\n");
		prefs_file_close_revert(pfile);
		return;
	}
        if (fprintf(pfile->fp, "\n") < 0) {
		FILE_OP_ERROR(rcpath, "fprintf");
		prefs_file_close_revert(pfile);
	} else
	        prefs_file_close(pfile);
}


static void addkeeper_prefs_save_func(PrefsPage * _page)
{
	struct AddressKeeperPrefsPage *page = (struct AddressKeeperPrefsPage *) _page;
	const gchar *text;
	text = gtk_entry_get_text(GTK_ENTRY(page->addressbook_folder));
	addkeeperprefs.addressbook_folder = g_strdup(text);
	addkeeperprefs.keep_to_addrs = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->keep_to_addrs_check));
	addkeeperprefs.keep_cc_addrs = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->keep_cc_addrs_check));
	addkeeperprefs.keep_bcc_addrs = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->keep_bcc_addrs_check));
	addkeeper_save_config();
}

void address_keeper_prefs_init(void)
{
	static gchar *path[3];
	gchar *rcpath;
	
	path[0] = _("Plugins");
	path[1] = _("Address Keeper");
	path[2] = NULL;

	prefs_set_default(param);
	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMON_RC, NULL);
	prefs_read_config(param, PREFS_BLOCK_NAME, rcpath, NULL);
	g_free(rcpath);

	addkeeperprefs_page.page.path = path;
	addkeeperprefs_page.page.create_widget = addkeeper_prefs_create_widget_func;
	addkeeperprefs_page.page.destroy_widget = addkeeper_prefs_destroy_widget_func;
	addkeeperprefs_page.page.save_page = addkeeper_prefs_save_func;

	prefs_gtk_register_page((PrefsPage *) &addkeeperprefs_page);
}

void address_keeper_prefs_done(void)
{
	prefs_gtk_unregister_page((PrefsPage *) &addkeeperprefs_page);
}
