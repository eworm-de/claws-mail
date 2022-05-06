/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2009-2018 Ricardo Mones and the Claws Mail Team
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
	GtkWidget *block_matching_addrs;
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
	{"block_matching_addrs", "", &addkeeperprefs.block_matching_addrs,
	 P_STRING, NULL, NULL, NULL},
	{NULL, NULL, NULL, P_OTHER, NULL, NULL, NULL}
};

#ifndef USE_ALT_ADDRBOOK
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

#define SPACING 10
#define SWIN_HEIGHT 100

static void addkeeper_prefs_create_widget_func(PrefsPage * _page,
					       GtkWindow * window,
					       gpointer data)
{
	struct AddressKeeperPrefsPage *page = (struct AddressKeeperPrefsPage *) _page;
	GtkWidget *path_frame;
	GtkWidget *path_hbox;
	GtkWidget *path_vbox;
	GtkWidget *path_entry;
	GtkWidget *path_label;
	GtkWidget *path_button;
	GtkWidget *keep_frame;
	GtkWidget *keep_hbox;
	GtkWidget *keep_to_checkbox;
	GtkWidget *keep_cc_checkbox;
	GtkWidget *keep_bcc_checkbox;
	GtkWidget *blocked_frame;
	GtkWidget *blocked_vbox;
	GtkWidget *blocked_scrolledwin;
	GtkWidget *vbox;
	GtkTextBuffer *buffer;
	gchar *text;
	gchar *tr;

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, SPACING);

	path_vbox = gtkut_get_options_frame(vbox, &path_frame,
		_("Address book location"));
	gtk_container_set_border_width(GTK_CONTAINER(path_frame), SPACING);
	path_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, SPACING);
	gtk_box_pack_start(GTK_BOX(path_vbox), path_hbox, FALSE, FALSE, 0);

	path_label = gtk_label_new(_("Keep to folder"));
	gtk_box_pack_start(GTK_BOX(path_hbox), path_label, FALSE, FALSE, 0);
	gtk_widget_show(path_label);

	path_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(path_entry), addkeeperprefs.addressbook_folder);
	gtk_box_pack_start(GTK_BOX(path_hbox), path_entry, TRUE, TRUE, 0);
	gtk_widget_show(path_entry);
	CLAWS_SET_TIP(path_entry, _("Address book path where addresses are kept"));

	path_button = gtk_button_new_with_label(_("Select..."));
	gtk_box_pack_start(GTK_BOX(path_hbox), path_button, FALSE, FALSE, 0);
#ifndef USE_ALT_ADDRBOOK
	g_signal_connect(G_OBJECT (path_button), "clicked",
			 G_CALLBACK (select_addressbook_clicked_cb),
			 path_entry);
#else
	gtk_widget_set_sensitive(path_button, FALSE);
#endif
	gtk_widget_show(path_button);
	gtk_widget_show(path_hbox);
	gtk_widget_show(path_vbox);

	page->addressbook_folder = path_entry;

	keep_hbox = gtkut_get_options_frame(vbox, &keep_frame,
		_("Fields to keep addresses from"));
	gtk_container_set_border_width(GTK_CONTAINER(keep_frame), SPACING);

	keep_to_checkbox = gtk_check_button_new_with_label(prefs_common_translated_header_name("To"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(keep_to_checkbox), addkeeperprefs.keep_to_addrs);
	gtk_box_pack_start(GTK_BOX(keep_hbox), keep_to_checkbox, FALSE, FALSE, 0);
	gtk_widget_show(keep_to_checkbox);
	tr = g_strdup(C_("address keeper: %s stands for a header name",
			"Keep addresses which appear in '%s' headers"));
	text = g_strdup_printf(tr, prefs_common_translated_header_name("To"));
	CLAWS_SET_TIP(keep_to_checkbox, text);
	g_free(tr);
	g_free(text);
	gtk_widget_show(keep_to_checkbox);

	page->keep_to_addrs_check = keep_to_checkbox;

	keep_cc_checkbox = gtk_check_button_new_with_label(prefs_common_translated_header_name("Cc"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(keep_cc_checkbox), addkeeperprefs.keep_cc_addrs);
	gtk_box_pack_start(GTK_BOX(keep_hbox), keep_cc_checkbox, FALSE, FALSE, 0);
	gtk_widget_show(keep_cc_checkbox);
	tr = g_strdup(C_("address keeper: %s stands for a header name",
			"Keep addresses which appear in '%s' headers"));
	text = g_strdup_printf(tr, prefs_common_translated_header_name("Cc"));
	CLAWS_SET_TIP(keep_cc_checkbox, text);
	g_free(text);
	g_free(tr);
	gtk_widget_show(keep_cc_checkbox);

	page->keep_cc_addrs_check = keep_cc_checkbox;

	keep_bcc_checkbox = gtk_check_button_new_with_label(prefs_common_translated_header_name("Bcc"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(keep_bcc_checkbox), addkeeperprefs.keep_bcc_addrs);
	gtk_box_pack_start(GTK_BOX(keep_hbox), keep_bcc_checkbox, FALSE, FALSE, 0);
	gtk_widget_show(keep_bcc_checkbox);
	tr = g_strdup(C_("address keeper: %s stands for a header name",
			"Keep addresses which appear in '%s' headers"));
	text = g_strdup_printf(tr, prefs_common_translated_header_name("Bcc"));
	CLAWS_SET_TIP(keep_bcc_checkbox, text);
	g_free(text);
	g_free(tr);
	gtk_widget_show(keep_bcc_checkbox);

	page->keep_bcc_addrs_check = keep_bcc_checkbox;

	blocked_vbox = gtkut_get_options_frame(vbox, &blocked_frame,
		_("Exclude addresses matching the following regular expressions (one per line)"));
	gtk_container_set_border_width(GTK_CONTAINER(blocked_frame), SPACING);

	page->block_matching_addrs = gtk_text_view_new();
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(page->block_matching_addrs));
	gtk_text_buffer_set_text(buffer, addkeeperprefs.block_matching_addrs, -1);
	
	blocked_scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(
		GTK_SCROLLED_WINDOW (blocked_scrolledwin),
		GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(
		GTK_SCROLLED_WINDOW (blocked_scrolledwin), GTK_SHADOW_IN);
	gtk_scrolled_window_set_min_content_height(
		GTK_SCROLLED_WINDOW (blocked_scrolledwin), SWIN_HEIGHT);

	gtk_container_add(GTK_CONTAINER(blocked_scrolledwin), page->block_matching_addrs);
	gtk_box_pack_start(GTK_BOX(blocked_vbox), blocked_scrolledwin, FALSE, FALSE, 0);
	
	gtk_widget_show_all(vbox);

	page->page.widget = vbox;
}

#undef SPACING
#undef SWIN_HEIGHT

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
		g_warning("failed to write Address Keeper configuration to file");
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
	GtkTextBuffer *buffer;
	GtkTextIter start, end;

	text = gtk_entry_get_text(GTK_ENTRY(page->addressbook_folder));
	addkeeperprefs.addressbook_folder = g_strdup(text);
	addkeeperprefs.keep_to_addrs = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->keep_to_addrs_check));
	addkeeperprefs.keep_cc_addrs = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->keep_cc_addrs_check));
	addkeeperprefs.keep_bcc_addrs = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->keep_bcc_addrs_check));

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(page->block_matching_addrs));
	gtk_text_buffer_get_start_iter(buffer, &start);
	gtk_text_buffer_get_end_iter(buffer, &end);
	text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
	g_free(addkeeperprefs.block_matching_addrs);
	addkeeperprefs.block_matching_addrs = g_malloc(2 * strlen(text) + 1);
	pref_get_escaped_pref(addkeeperprefs.block_matching_addrs, text);

	addkeeper_save_config();
	g_free(addkeeperprefs.block_matching_addrs);
	addkeeperprefs.block_matching_addrs = (gchar *)text;
}

void address_keeper_prefs_init(void)
{
	static gchar *path[3];
	gchar *rcpath;
	gchar *tmp;
	
	path[0] = _("Plugins");
	path[1] = _("Address Keeper");
	path[2] = NULL;

	prefs_set_default(param);
	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMON_RC, NULL);
	prefs_read_config(param, PREFS_BLOCK_NAME, rcpath, NULL);
	g_free(rcpath);

	tmp = g_malloc(strlen(addkeeperprefs.block_matching_addrs) + 1);
	pref_get_unescaped_pref(tmp, addkeeperprefs.block_matching_addrs);
	g_free(addkeeperprefs.block_matching_addrs);
	addkeeperprefs.block_matching_addrs = tmp;

	addkeeperprefs_page.page.path = path;
	addkeeperprefs_page.page.create_widget = addkeeper_prefs_create_widget_func;
	addkeeperprefs_page.page.destroy_widget = addkeeper_prefs_destroy_widget_func;
	addkeeperprefs_page.page.save_page = addkeeper_prefs_save_func;
	addkeeperprefs_page.page.weight = 40.0;

	prefs_gtk_register_page((PrefsPage *) &addkeeperprefs_page);
}

void address_keeper_prefs_done(void)
{
	prefs_gtk_unregister_page((PrefsPage *) &addkeeperprefs_page);
}
