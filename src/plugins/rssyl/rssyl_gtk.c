/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2004 Hiroyuki Yamamoto
 * This file (C) 2005 Andrej Kacian <andrej@kacian.sk>
 *
 * - GUI handling functions
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>

#include <gtk/gtk.h>

#include "gtk/menu.h"
#include "mainwindow.h"
#include "inputdialog.h"
#include "folderview.h"
#include "alertpanel.h"
#include "summaryview.h"

#include "main.h"
#include "gtkutils.h"

#include "feed.h"
#include "feedprops.h"
#include "rssyl.h"
#include "rssyl_cb_gtk.h"
#include "rssyl_cb_menu.h"
#include "rssyl_gtk.h"
#include "rssyl_prefs.h"

static char *rssyl_popup_menu_labels[] =
{
	N_("_Refresh feed"),
	N_("Refresh _all feeds"),
	N_("Subscribe _new feed..."),
	N_("_Unsubscribe feed..."),
	N_("Feed pr_operties..."),
	N_("Import feed list..."),
	N_("Rena_me..."),
	N_("_Create new folder..."),
	N_("_Delete folder..."),
	N_("Remove folder _tree..."),
	NULL
};

static GtkActionEntry rssyl_popup_entries[] = 
{
	{"FolderViewPopup/RefreshFeed",		NULL, NULL, NULL, NULL, G_CALLBACK(rssyl_refresh_cb) },
	{"FolderViewPopup/RefreshAllFeeds",	NULL, NULL, NULL, NULL, G_CALLBACK(rssyl_refresh_all_cb) },

	{"FolderViewPopup/NewFeed",		NULL, NULL, NULL, NULL, G_CALLBACK(rssyl_new_feed_cb) },
	{"FolderViewPopup/RemoveFeed",		NULL, NULL, NULL, NULL, G_CALLBACK(rssyl_remove_feed_cb) },
	{"FolderViewPopup/FeedProperties",	NULL, NULL, NULL, NULL, G_CALLBACK(rssyl_prop_cb) },
	{"FolderViewPopup/ImportFeedlist",	NULL, NULL, NULL, NULL, G_CALLBACK(rssyl_import_feed_list_cb) },

	{"FolderViewPopup/RenameFolder",	NULL, NULL, NULL, NULL, G_CALLBACK(rssyl_rename_cb) },

	{"FolderViewPopup/NewFolder",		NULL, NULL, NULL, NULL, G_CALLBACK(rssyl_new_folder_cb) },
	{"FolderViewPopup/RemoveFolder",	NULL, NULL, NULL, NULL, G_CALLBACK(rssyl_remove_folder_cb) },

	{"FolderViewPopup/RemoveMailbox",	NULL, NULL, NULL, NULL, G_CALLBACK(rssyl_remove_rss_cb) },

};

static void rssyl_add_menuitems(GtkUIManager *ui_manager, FolderItem *item)
{
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "RefreshFeed", "FolderViewPopup/RefreshFeed", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "RefreshAllFeeds", "FolderViewPopup/RefreshAllFeeds", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "SeparatorRSS1", "FolderViewPopup/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "NewFeed", "FolderViewPopup/NewFeed", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "RemoveFeed", "FolderViewPopup/RemoveFeed", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "FeedProperties", "FolderViewPopup/FeedProperties", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "ImportFeedlist", "FolderViewPopup/ImportFeedlist", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "SeparatorRSS2", "FolderViewPopup/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "RenameFolder", "FolderViewPopup/RenameFolder", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "SeparatorRSS3", "FolderViewPopup/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "NewFolder", "FolderViewPopup/NewFolder", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "RemoveFolder", "FolderViewPopup/RemoveFolder", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "SeparatorRSS4", "FolderViewPopup/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "RemoveMailbox", "FolderViewPopup/RemoveMailbox", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "SeparatorRSS5", "FolderViewPopup/---", GTK_UI_MANAGER_SEPARATOR)
}

static void rssyl_set_sensitivity(GtkUIManager *ui_manager, FolderItem *item)
{
#define SET_SENS(name, sens) \
	cm_menu_set_sensitive_full(ui_manager, "Popup/"name, sens)

	RSSylFolderItem *ritem = (RSSylFolderItem *)item;
	SET_SENS("FolderViewPopup/RefreshFeed", folder_item_parent(item) != NULL && ritem->url);
	SET_SENS("FolderViewPopup/RefreshAllFeeds", folder_item_parent(item) == NULL );
	SET_SENS("FolderViewPopup/NewFeed", TRUE);
	SET_SENS("FolderViewPopup/ImportFeedlist", TRUE );
	SET_SENS("FolderViewPopup/RemoveFeed", folder_item_parent(item) != NULL && ritem->url );
	SET_SENS("FolderViewPopup/FeedProperties", folder_item_parent(item) != NULL && ritem->url );
	SET_SENS("FolderViewPopup/RenameFolder", folder_item_parent(item) != NULL );
	SET_SENS("FolderViewPopup/NewFolder", TRUE );
	SET_SENS("FolderViewPopup/RemoveFolder", folder_item_parent(item) != NULL && !ritem->url );
	SET_SENS("FolderViewPopup/RemoveMailbox", folder_item_parent(item) == NULL );

#undef SET_SENS
}

static FolderViewPopup rssyl_popup =
{
	"rssyl",
	"<rssyl>",
	rssyl_popup_entries,
	G_N_ELEMENTS(rssyl_popup_entries),
	NULL, 0,
	NULL, 0, 0, NULL,
	rssyl_add_menuitems,
	rssyl_set_sensitivity
};

static void rssyl_fill_popup_menu_labels(void)
{
	gint i;

	for( i = 0; rssyl_popup_menu_labels[i] != NULL; i++ ) {
		(rssyl_popup_entries[i]).label = _(rssyl_popup_menu_labels[i]);
	}
}

static void rssyl_add_mailbox(GtkAction *action, gpointer callback_data)
{
	MainWindow *mainwin = (MainWindow *) callback_data;
	gchar *path;
	gchar *base = NULL;
	Folder *folder;

	path = input_dialog(_("Add RSS folder tree"),
			_("Enter name for a new RSS folder tree."),
			RSSYL_DEFAULT_MAILBOX);
	if( !path ) return;

	if( folder_find_from_path(path) ) {
		alertpanel_error(_("The mailbox '%s' already exists."), path);
		g_free(path);
		return;
	}

	base = g_path_get_basename(path);
	folder = folder_new(folder_get_class_from_string("rssyl"),
			base, path);
	g_free(base);

	if( folder->klass->create_tree(folder) < 0 ) {
		alertpanel_error(_("Creation of folder tree failed.\n"
				"Maybe some files already exist, or you don't have the permission "
				"to write there?"));
		folder_destroy(folder);
		return;
	}

	folder_add(folder);
	folder_scan_tree(folder, TRUE);

	folderview_set(mainwin->folderview);
}

static GtkActionEntry mainwindow_add_mailbox[] = {{
	"File/AddMailbox/RSSyl",
	NULL, N_("RSSyl..."), NULL, NULL, G_CALLBACK(rssyl_add_mailbox)
}};

static guint main_menu_id = 0;

void rssyl_gtk_init(void)
{
	MainWindow *mainwin = mainwindow_get_mainwindow();

	gtk_action_group_add_actions(mainwin->action_group, mainwindow_add_mailbox,
			1, (gpointer)mainwin);
	MENUITEM_ADDUI_ID_MANAGER(mainwin->ui_manager, "/Menu/File/AddMailbox", "RSSyl", 
			  "File/AddMailbox/RSSyl", GTK_UI_MANAGER_MENUITEM,
			  main_menu_id)


	rssyl_fill_popup_menu_labels();
	folderview_register_popup(&rssyl_popup);
}

void rssyl_gtk_done(void)
{
	MainWindow *mainwin = mainwindow_get_mainwindow();
	FolderView *folderview = NULL;
	FolderItem *fitem = NULL;

	if (mainwin == NULL || claws_is_exiting())
		return;

	folderview = mainwin->folderview;
	fitem = folderview->summaryview->folder_item;

	if( fitem && IS_RSSYL_FOLDER_ITEM(fitem) ) {
		folderview_unselect(folderview);
		summary_clear_all(folderview->summaryview);
	}

	folderview_unregister_popup(&rssyl_popup);

	MENUITEM_REMUI_MANAGER(mainwin->ui_manager,mainwin->action_group, "File/AddMailbox/RSSyl", main_menu_id);
	main_menu_id = 0;
}

/***********************************************/

static RSSylFeedProp *rssyl_gtk_prop_real(RSSylFolderItem *ritem)
{
	MainWindow *mainwin = mainwindow_get_mainwindow();
	RSSylFeedProp *feedprop;
	GtkWidget *vbox, *urllabel, *urlframe, *urlalign, *table, *refresh_label,
						*expired_label, *hsep, *sep, *bbox, *cancel_button, *cancel_align,
						*cancel_hbox, *cancel_image, *cancel_label, *ok_button, *ok_align,
						*ok_hbox, *ok_image, *ok_label, *silent_update_label;
	GtkObject *refresh_adj, *expired_adj, *fetch_comments_for_adj;
	gint refresh, expired;
	gint row = 0;

	g_return_val_if_fail(ritem != NULL, NULL);

	feedprop = g_new0(RSSylFeedProp, 1);

	/* Create required widgets */

	/* Window */
	feedprop->window = gtkut_window_new(GTK_WINDOW_TOPLEVEL, "rssyl_gtk");

	/* URL entry */
	feedprop->url = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(feedprop->url), ritem->url);

	/* "Use default refresh interval" checkbutton */
	feedprop->default_refresh_interval = gtk_check_button_new_with_mnemonic(
			_("Use default refresh interval"));
	gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON(feedprop->default_refresh_interval),
			ritem->default_refresh_interval);

	if( ritem->refresh_interval >= 0 && !ritem->default_refresh_interval )
		refresh = ritem->refresh_interval;
	else
		refresh = rssyl_prefs_get()->refresh;

	/* "Keep default number of expired items" checkbutton */
	feedprop->default_expired_num = gtk_check_button_new_with_mnemonic(
			_("Keep default number of expired entries"));
	gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON(feedprop->default_expired_num),
			ritem->default_expired_num);

	feedprop->fetch_comments = gtk_check_button_new_with_mnemonic(
			_("Fetch comments if possible"));
	gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON(feedprop->fetch_comments),
			ritem->fetch_comments);

	/* Refresh interval spinbutton */
	fetch_comments_for_adj = gtk_adjustment_new(ritem->fetch_comments_for,
			-1, 100000, 1, 10, 0);
	feedprop->fetch_comments_for = gtk_spin_button_new(GTK_ADJUSTMENT(fetch_comments_for_adj),
			1, 0);

	if( ritem->default_expired_num )
		expired = rssyl_prefs_get()->expired;
	else
		expired = ritem->expired_num;

	/* Refresh interval spinbutton */
	refresh_adj = gtk_adjustment_new(refresh,
			0, 100000, 1, 10, 0);
	feedprop->refresh_interval = gtk_spin_button_new(GTK_ADJUSTMENT(refresh_adj),
			1, 0);

	/* Expired num spinbutton */
	expired_adj = gtk_adjustment_new(ritem->expired_num, -1, 100000, 1, 10, 0);
	feedprop->expired_num = gtk_spin_button_new(GTK_ADJUSTMENT(expired_adj),
			1, 0);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(feedprop->window), vbox);

	/* URL frame */
	urlframe = gtk_frame_new(NULL);
	gtk_container_set_border_width(GTK_CONTAINER(urlframe), 5);
	gtk_frame_set_label_align(GTK_FRAME(urlframe), 0.05, 0.5);
	gtk_frame_set_shadow_type(GTK_FRAME(urlframe), GTK_SHADOW_ETCHED_OUT);
	gtk_box_pack_start(GTK_BOX(vbox), urlframe, FALSE, FALSE, 0);

	/* Label for URL frame */
	urllabel = gtk_label_new(_("<b>Source URL:</b>"));
	gtk_label_set_use_markup(GTK_LABEL(urllabel), TRUE);
	gtk_misc_set_padding(GTK_MISC(urllabel), 5, 0);
	gtk_frame_set_label_widget(GTK_FRAME(urlframe), urllabel);

	/* URL entry (+ alignment in frame) */
	urlalign = gtk_alignment_new(0, 0.5, 1, 1);
	gtk_alignment_set_padding(GTK_ALIGNMENT(urlalign), 5, 5, 5, 5);
	gtk_container_add(GTK_CONTAINER(urlframe), urlalign);

	gtk_entry_set_activates_default(GTK_ENTRY(feedprop->url), TRUE);
	gtk_container_add(GTK_CONTAINER(urlalign), feedprop->url);

	/* Table for remaining properties */
	table = gtk_table_new(8, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), table, TRUE, TRUE, 0);

	/* Fetch comments - checkbutton */
	gtk_table_attach(GTK_TABLE(table), feedprop->fetch_comments,
			0, 2, row, row+1,
			(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			(GtkAttachOptions) (0), 10, 0);
	g_signal_connect(G_OBJECT(feedprop->fetch_comments), "toggled",
			G_CALLBACK(rssyl_fetch_comments_toggled_cb),
			(gpointer)feedprop->fetch_comments_for);
	row++;

	/* Fetch comments for - label */
	refresh_label = gtk_label_new(_("<b>Fetch comments on posts aged less than:</b>\n"
			"<small>(In days; set to -1 to fetch all comments)"
			"</small>"));
	gtk_label_set_use_markup(GTK_LABEL(refresh_label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(refresh_label), 0, 0.5);
	gtk_table_attach(GTK_TABLE(table), refresh_label, 0, 1, row, row+1,
			(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			(GtkAttachOptions) (0), 10, 5);

	/* Fetch comments for - spinbutton */
	gtk_widget_set_sensitive(feedprop->fetch_comments_for,
			ritem->fetch_comments);
	gtk_table_attach(GTK_TABLE(table), feedprop->fetch_comments_for, 1, 2, row, row+1,
			(GtkAttachOptions) (0),
			(GtkAttachOptions) (0), 10, 5);

	row++;
	hsep = gtk_hseparator_new();
	gtk_widget_set_size_request(hsep, -1, 10);
	gtk_table_attach(GTK_TABLE(table), hsep, 0, 2, row, row+1,
			(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			(GtkAttachOptions) (0), 10, 5);

	row++;
	/* Use default refresh interval - checkbutton */
	gtk_table_attach(GTK_TABLE(table), feedprop->default_refresh_interval,
			0, 2, row, row+1,
			(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			(GtkAttachOptions) (0), 10, 0);
	g_signal_connect(G_OBJECT(feedprop->default_refresh_interval), "toggled",
			G_CALLBACK(rssyl_default_refresh_interval_toggled_cb),
			(gpointer)feedprop->refresh_interval);
	row++;
	/* Refresh interval - label */
	refresh_label = gtk_label_new(_("<b>Refresh interval in minutes:</b>\n"
			"<small>(Set to 0 to disable automatic refreshing for this feed)"
			"</small>"));
	gtk_label_set_use_markup(GTK_LABEL(refresh_label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(refresh_label), 0, 0.5);
	gtk_table_attach(GTK_TABLE(table), refresh_label, 0, 1, row, row+1,
			(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			(GtkAttachOptions) (0), 10, 5);

	/* Refresh interval - spinbutton */
	gtk_widget_set_sensitive(feedprop->refresh_interval,
			!ritem->default_refresh_interval);
	gtk_table_attach(GTK_TABLE(table), feedprop->refresh_interval, 1, 2, row, row+1,
			(GtkAttachOptions) (0),
			(GtkAttachOptions) (0), 10, 5);
	row++;
	hsep = gtk_hseparator_new();
	gtk_widget_set_size_request(hsep, -1, 10);
	gtk_table_attach(GTK_TABLE(table), hsep, 0, 2, row, row+1,
			(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			(GtkAttachOptions) (0), 10, 5);

	row++;
	/* Use default number for expired - checkbutton */
	gtk_table_attach(GTK_TABLE(table), feedprop->default_expired_num,	0, 2, row, row+1,
			(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			(GtkAttachOptions) (0), 10, 0);
	g_signal_connect(G_OBJECT(feedprop->default_expired_num), "toggled",
			G_CALLBACK(rssyl_default_expired_num_toggled_cb),
			(gpointer)feedprop->expired_num);

	row++;
	/* Expired items - label */
	expired_label = gtk_label_new(_("<b>Number of expired entries to keep:"
			"</b>\n<small>(Set to -1 if you want to keep expired entries)"
			"</small>"));
	gtk_label_set_use_markup(GTK_LABEL(expired_label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(expired_label), 0, 0.5);
	gtk_table_attach(GTK_TABLE(table), expired_label, 0, 1, row, row+1,
			(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			(GtkAttachOptions) (0), 10, 5);

	/* Expired items - spinbutton */
	gtk_widget_set_sensitive(feedprop->expired_num,
			!ritem->default_expired_num);
	gtk_table_attach(GTK_TABLE(table), feedprop->expired_num, 1, 2, row, row+1,
			(GtkAttachOptions) (0),
			(GtkAttachOptions) (0), 10, 5);

	row++;
	hsep = gtk_hseparator_new();
	gtk_widget_set_size_request(hsep, -1, 10);
	gtk_table_attach(GTK_TABLE(table), hsep, 0, 2, row, row+1,
			(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			(GtkAttachOptions) (0), 10, 5);

	row++;
	/* Silent update - label */
	silent_update_label =
		gtk_label_new(_("<b>If an item changes, do not mark it as unread:</b>"));
	gtk_label_set_use_markup(GTK_LABEL(silent_update_label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(silent_update_label), 0, 0.5);
	gtk_table_attach(GTK_TABLE(table), silent_update_label, 0, 1, row, row+1,
			(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			(GtkAttachOptions) (0), 10, 5);

	/* Silent update - combobox */
	feedprop->silent_update = gtk_combo_box_new_text();
	gtk_combo_box_append_text(GTK_COMBO_BOX(feedprop->silent_update),
			_("Always mark as unread"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(feedprop->silent_update),
			_("If only its text changed"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(feedprop->silent_update),
			_("Never mark as unread"));
	gtk_table_attach(GTK_TABLE(table), feedprop->silent_update, 1, 2, row, row+1,
			(GtkAttachOptions) (0),
			(GtkAttachOptions) (0), 10, 5);
	gtk_combo_box_set_active(GTK_COMBO_BOX(feedprop->silent_update),
			ritem->silent_update);

	/* Separator above the button box */
	sep = gtk_hseparator_new();
	gtk_widget_set_size_request(sep, -1, 10);
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 0);

	/* Buttonbox */
	bbox = gtk_hbutton_box_new();
	gtk_container_set_border_width(GTK_CONTAINER(bbox), 5);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
	gtk_box_set_spacing(GTK_BOX(bbox), 5);
	gtk_box_pack_end(GTK_BOX(vbox), bbox, FALSE, FALSE, 0);

	/* Cancel button */
	cancel_button = gtk_button_new();
	gtk_container_add(GTK_CONTAINER(bbox), cancel_button);

	cancel_align = gtk_alignment_new(0.5, 0.5, 0, 0);
	gtk_container_add(GTK_CONTAINER(cancel_button), cancel_align);

	cancel_hbox = gtk_hbox_new(FALSE, 2);
	gtk_container_add(GTK_CONTAINER(cancel_align), cancel_hbox);

	cancel_image = gtk_image_new_from_stock(GTK_STOCK_CANCEL,
			GTK_ICON_SIZE_BUTTON);
	gtk_box_pack_start(GTK_BOX(cancel_hbox), cancel_image, FALSE, FALSE, 0);

	cancel_label = gtk_label_new_with_mnemonic(_("_Cancel"));
	gtk_box_pack_end(GTK_BOX(cancel_hbox), cancel_label, FALSE, FALSE, 0);

	g_signal_connect(G_OBJECT(cancel_button), "clicked",
			G_CALLBACK(rssyl_props_cancel_cb), ritem);

	/* OK button */
	ok_button = gtk_button_new();
	gtk_container_add(GTK_CONTAINER(bbox), ok_button);
	gtkut_widget_set_can_default(ok_button, TRUE);

	ok_align = gtk_alignment_new(0.5, 0.5, 0, 0);
	gtk_container_add(GTK_CONTAINER(ok_button), ok_align);

	ok_hbox = gtk_hbox_new(FALSE, 2);
	gtk_container_add(GTK_CONTAINER(ok_align), ok_hbox);

	ok_image = gtk_image_new_from_stock(GTK_STOCK_OK,
			GTK_ICON_SIZE_BUTTON);
	gtk_box_pack_start(GTK_BOX(ok_hbox), ok_image, FALSE, FALSE, 0);

	ok_label = gtk_label_new_with_mnemonic(_("_OK"));
	gtk_box_pack_end(GTK_BOX(ok_hbox), ok_label, FALSE, FALSE, 0);

	g_signal_connect(G_OBJECT(ok_button), "clicked",
			G_CALLBACK(rssyl_props_ok_cb), ritem);

	/* Set some misc. stuff */
	gtk_window_set_title(GTK_WINDOW(feedprop->window),
			g_strdup(_("Set feed properties")) );
	gtk_window_set_modal(GTK_WINDOW(feedprop->window), TRUE);
	gtk_window_set_transient_for(GTK_WINDOW(feedprop->window),
			GTK_WINDOW(mainwin->window) );

	/* Attach callbacks to handle Enter and Escape keys */
	g_signal_connect(G_OBJECT(feedprop->window), "key_press_event",
			G_CALLBACK(rssyl_props_key_press_cb), ritem);

	/* ...and voila! */
	gtk_widget_show_all(feedprop->window);
	gtk_widget_grab_default(ok_button);

	/* Unselect the text in URL entry */
	gtk_editable_select_region(GTK_EDITABLE(feedprop->url), 0, 0);

	ritem->feedprop = feedprop;

	return feedprop;
}

void rssyl_gtk_prop(RSSylFolderItem *ritem)
{
	g_return_if_fail(ritem != NULL);

	rssyl_gtk_prop_real(ritem);
}

void rssyl_gtk_prop_store(RSSylFolderItem *ritem)
{
	gchar *url;
	gint x, old_ri, old_ex, old_fetch_comments;
	gboolean use_default_ri = FALSE, use_default_ex = FALSE;

	g_return_if_fail(ritem != NULL);
	g_return_if_fail(ritem->feedprop != NULL);

	url = (gchar *)gtk_entry_get_text(GTK_ENTRY(ritem->feedprop->url));

	if( strlen(url) ) {
		if( ritem->url ) {
			g_free(ritem->url);
		}
		ritem->url = g_strdup(url);
	}

	use_default_ri = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(ritem->feedprop->default_refresh_interval));
	ritem->default_refresh_interval = use_default_ri;
	debug_print("store: default is %s\n", ( use_default_ri ? "ON" : "OFF" ) );

	/* Use default if checkbutton is set */
	if( use_default_ri )
		x = rssyl_prefs_get()->refresh;
	else
		x = gtk_spin_button_get_value_as_int(
				GTK_SPIN_BUTTON(ritem->feedprop->refresh_interval) );

	old_ri = ritem->refresh_interval;
	ritem->refresh_interval = x;

	/* Update refresh interval setting if it has changed and has a sane value */
	if( old_ri != x && x >= 0 ) {
		debug_print("RSSyl: GTK - refresh interval changed to %d , updating "
				"timeout\n", ritem->refresh_interval);
		/* Value of 0 means we do not want to update automatically */
		if( x > 0 )
			rssyl_start_refresh_timeout(ritem);
	}

	old_fetch_comments = ritem->fetch_comments;
	ritem->fetch_comments = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(ritem->feedprop->fetch_comments));

	ritem->fetch_comments_for = gtk_spin_button_get_value_as_int(
				GTK_SPIN_BUTTON(ritem->feedprop->fetch_comments_for));

	if (!old_fetch_comments && ritem->fetch_comments) {
		/* reset the RSSylFolderItem's mtime to be sure we get all 
		 * available comments */
		 ritem->item.mtime = 0;
	}
	
	use_default_ex = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(ritem->feedprop->default_expired_num));
	ritem->default_expired_num = use_default_ex;
	debug_print("store: default is %s\n", ( use_default_ex ? "ON" : "OFF" ) );

	/* Use default if checkbutton is set */
	if( use_default_ex )
		x = rssyl_prefs_get()->expired;
	else
		x = gtk_spin_button_get_value_as_int(
				GTK_SPIN_BUTTON(ritem->feedprop->expired_num) );

	old_ex = ritem->expired_num;
	ritem->expired_num = x;

	ritem->silent_update =
		gtk_combo_box_get_active(GTK_COMBO_BOX(ritem->feedprop->silent_update));
	if( ritem->silent_update < 0 )
		ritem->silent_update = 0;

	rssyl_store_feed_props(ritem);

	debug_print("last_count %d, x %d, old_ex %d\n", ritem->last_count, x, old_ex);

	/* update if new setting is lower, or if it was changed from -1 */
	if( ritem->last_count != 0 && x != -1 && (old_ex > x || old_ex == -1) ) {
		debug_print("RSSyl: GTK - expired_num has changed to %d, expiring\n",
				ritem->expired_num);
		rssyl_expire_items(ritem);
	}
}

GtkWidget *rssyl_feed_removal_dialog(gchar *name, GtkWidget **rmcache_widget)
{
	gchar *message;
	GtkWidget *dialog, *vbox, *hbox, *image, *vbox2, *label, *cb, *aa,
						*bno, *byes;
	MainWindow *mainwin = mainwindow_get_mainwindow();

	g_return_val_if_fail(name != NULL, NULL);

	dialog = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(dialog), _("Unsubscribe feed"));
	gtk_window_set_type_hint(GTK_WINDOW(dialog), GDK_WINDOW_TYPE_HINT_DIALOG);

	vbox = GTK_DIALOG(dialog)->vbox;

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

	/* Question icon */
	image = gtk_image_new_from_stock(GTK_STOCK_DIALOG_QUESTION,
			GTK_ICON_SIZE_DIALOG);
	gtk_misc_set_alignment(GTK_MISC(image), 0.5, 0.30);
	gtk_misc_set_padding(GTK_MISC(image), 12, 0);
	gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);

	vbox2 = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vbox2, TRUE, TRUE, 0);

	/* Dialog text label */
	label = gtk_label_new("");
	gtk_misc_set_alignment(GTK_MISC(label), 0.1, 0);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_padding(GTK_MISC(label), 0, 12);
	message = g_markup_printf_escaped("<span size='x-large'><b>%s</b></span>"
			"\n\n%s '%s' ?", _("Unsubscribe feed"),
			_("Do you really want to remove feed"), name);
	gtk_label_set_markup(GTK_LABEL(label), message);
	g_free(message);
	gtk_box_pack_start(GTK_BOX(vbox2), label, FALSE, FALSE, 0);

	/* Remove cache checkbutton */
	cb = gtk_check_button_new_with_mnemonic(_("Remove cached entries"));
	gtk_button_set_focus_on_click(GTK_BUTTON(cb), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb), TRUE);
	gtk_box_pack_start(GTK_BOX(vbox2), cb, FALSE, FALSE, 0);
	*rmcache_widget = cb;

	aa = GTK_DIALOG(dialog)->action_area;
	gtk_button_box_set_layout(GTK_BUTTON_BOX(aa), GTK_BUTTONBOX_END);

	bno = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
	gtk_dialog_add_action_widget(GTK_DIALOG(dialog), bno, GTK_RESPONSE_NO);
	gtkut_widget_set_can_default(bno, TRUE);
	
	byes = gtk_button_new_with_mnemonic(_("_Unsubscribe"));
	gtk_dialog_add_action_widget(GTK_DIALOG(dialog), byes, GTK_RESPONSE_YES);

	gtk_widget_grab_default(bno);
	gtk_window_set_transient_for(GTK_WINDOW(dialog),
			GTK_WINDOW(mainwin->window) );

	return dialog;
}
