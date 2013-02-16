/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2004 Hiroyuki Yamamoto
 * This file (C) 2005 Andrej Kacian <andrej@kacian.sk>
 *
 * - callback handler functions for folderview rssyl context menu items
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

#include "folderview.h"
#include "alertpanel.h"
#include "gtk/inputdialog.h"
#include "prefs_common.h"
#include "filesel.h"
#include "inc.h"
#include "folder_item_prefs.h"

#include "feed.h"
#include "feedprops.h"
#include "opml.h"
#include "rssyl.h"
#include "rssyl_gtk.h"

void rssyl_new_feed_cb(GtkAction *action, gpointer data)
{
	FolderView *folderview = (FolderView *)data;
	GtkCMCTree *ctree = GTK_CMCTREE(folderview->ctree);
	FolderItem *item;
	gchar *new_feed;

	debug_print("RSSyl: new_feed_cb\n");

	g_return_if_fail(folderview->selected != NULL);

	item = gtk_cmctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);

	new_feed = input_dialog(_("Subscribe feed"),
					 _("Input the URL of the news feed you wish to subscribe:"),
					 "");
	g_return_if_fail(new_feed != NULL);

	rssyl_subscribe_new_feed(item, new_feed, TRUE);

	g_free(new_feed);
}

void rssyl_new_folder_cb(GtkAction *action, gpointer data)
{
	FolderView *folderview = (FolderView *)data;
	GtkCMCTree *ctree = GTK_CMCTREE(folderview->ctree);
	FolderItem *item;
	FolderItem *new_item;
	gchar *new_folder;
	gchar *name;
	gchar *p;
	RSSylFolderItem *ritem = NULL;

	if (!folderview->selected) return;

	item = gtk_cmctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);

	new_folder = input_dialog(_("New folder"),
				  _("Input the name of new folder:"),
				  _("NewFolder"));
	if (!new_folder) return;
	AUTORELEASE_STR(new_folder, {g_free(new_folder); return;});

	p = strchr(new_folder, G_DIR_SEPARATOR);
	if (p) {
		alertpanel_error(_("'%c' can't be included in folder name."),
				 G_DIR_SEPARATOR);
		return;
	}

	name = trim_string(new_folder, 32);
	AUTORELEASE_STR(name, {g_free(name); return;});

	/* find whether the directory already exists */
	if (folder_find_child_item_by_name(item, new_folder)) {
		alertpanel_error(_("The folder '%s' already exists."), name);
		return;
	}

	new_item = folder_create_folder(item, new_folder);
	if (!new_item) {
		alertpanel_error(_("Can't create the folder '%s'."), name);
		return;
	}

	ritem = (RSSylFolderItem *)new_item;
	ritem->url = NULL;

	folder_write_list();
}

void rssyl_remove_rss_cb(GtkAction *action, gpointer data)
{
	FolderView *folderview = (FolderView *)data;
	FolderItem *item;
	gchar *name, *message;
	AlertValue aval;

	debug_print("RSSyl: remove_rss_cb\n");

	item = folderview_get_selected_item(folderview);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);
	g_return_if_fail( !folder_item_parent(item) );

	name = trim_string(item->folder->name, 32);
	message = g_strdup_printf(_("Really remove the folder tree '%s' ?\n"), name);
	aval = alertpanel_full(_("Remove folder tree"), message,
			 GTK_STOCK_CANCEL, _("_Remove"), NULL, FALSE, NULL,
			  ALERT_WARNING, G_ALERTDEFAULT);
	g_free(message);
	g_free(name);

	if (aval != G_ALERTALTERNATE)
		return;

	folderview_unselect(folderview);
	summary_clear_all(folderview->summaryview);

	folder_destroy(item->folder);
}

void rssyl_remove_feed_cb(GtkAction *action, gpointer data)
{
	FolderView *folderview = (FolderView *)data;
	GtkCMCTree *ctree = GTK_CMCTREE(folderview->ctree);
	FolderItem *item;
	gint response;
	GtkWidget *dialog, *rmcache_widget = NULL;
	gboolean rmcache = FALSE;
	debug_print("RSSyl: remove_feed_cb\n");

	item = folderview_get_selected_item(folderview);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->path != NULL);
	g_return_if_fail(item->folder != NULL);

	dialog = rssyl_feed_removal_dialog(item->name, &rmcache_widget);

	g_return_if_fail(dialog != NULL);

	gtk_widget_show_all(dialog);

	response = gtk_dialog_run(GTK_DIALOG(dialog));

	if( response != GTK_RESPONSE_YES ) {
		debug_print("'No' clicked, returning\n");
		gtk_widget_destroy(dialog);
		return;
	}

	g_return_if_fail(rmcache_widget != NULL);

	rmcache = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(rmcache_widget) );

	gtk_widget_destroy(dialog);

	if( folderview->opened == folderview->selected ||
			gtk_cmctree_is_ancestor(ctree,
					folderview->selected,
					folderview->opened) ) {
		summary_clear_all(folderview->summaryview);
		folderview->opened = NULL;
	}

	rssyl_remove_feed_props((RSSylFolderItem *)item);

	if( rmcache == TRUE )
		rssyl_remove_feed_cache(item);

	if( item->folder->klass->remove_folder(item->folder, item) < 0 ) {
		gchar *tmp;
		tmp = g_markup_printf_escaped(_("Can't remove feed '%s'."), item->name);
		alertpanel_error("%s", tmp);
		g_free(tmp);
		if( folderview->opened == folderview->selected )
			summary_show(folderview->summaryview,
					folderview->summaryview->folder_item);
		return;
	}

	folder_write_list();
}

void rssyl_remove_folder_cb(GtkAction *action, gpointer data)
{
	FolderView *folderview = (FolderView *)data;
	GtkCMCTree *ctree = GTK_CMCTREE(folderview->ctree);
	FolderItem *item;
	gchar *message, *name;
	AlertValue avalue;
	gchar *old_path;
	gchar *old_id;

	item = folderview_get_selected_item(folderview);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->path != NULL);
	g_return_if_fail(item->folder != NULL);

	name = trim_string(item->name, 32);
	AUTORELEASE_STR(name, {g_free(name); return;});
	message = g_strdup_printf
		(_("All folders and messages under '%s' will be permanently deleted. "
		   "Recovery will not be possible.\n\n"
		   "Do you really want to delete?"), name);
	avalue = alertpanel_full(_("Delete folder"), message,
				 GTK_STOCK_CANCEL, GTK_STOCK_DELETE, NULL, FALSE,
				 NULL, ALERT_WARNING, G_ALERTDEFAULT);
	g_free(message);
	if (avalue != G_ALERTALTERNATE) return;

	Xstrdup_a(old_path, item->path, return);
	old_id = folder_item_get_identifier(item);

	if (folderview->opened == folderview->selected ||
	    gtk_cmctree_is_ancestor(ctree,
				  folderview->selected,
				  folderview->opened)) {
		summary_clear_all(folderview->summaryview);
		folderview->opened = NULL;
	}

	if (item->folder->klass->remove_folder(item->folder, item) < 0) {
		folder_item_scan(item);
		alertpanel_error(_("Can't remove the folder '%s'."), name);
		g_free(old_id);
		return;
	}

	folder_write_list();

	prefs_filtering_delete_path(old_id);
	g_free(old_id);

}

void rssyl_refresh_cb(GtkAction *action, gpointer data)
{
	FolderView *folderview = (FolderView *)data;
	FolderItem *item;
	RSSylFolderItem *ritem;

	item = folderview_get_selected_item(folderview);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);

	ritem = (RSSylFolderItem *)item;

	if (prefs_common_get_prefs()->work_offline && 
	   !inc_offline_should_override(TRUE,
			ngettext(
			   "Claws Mail needs network access in order "
			   "to update the feed.",
			   "Claws Mail needs network access in order "
			   "to update the feeds.", 1))) {
			return;
	}

	main_window_cursor_wait(mainwindow_get_mainwindow());
	rssyl_update_feed(ritem);
	main_window_cursor_normal(mainwindow_get_mainwindow());
}

void rssyl_refresh_all_cb(GtkAction *action, gpointer data)
{
	main_window_cursor_wait(mainwindow_get_mainwindow());
	rssyl_refresh_all_feeds();
	main_window_cursor_normal(mainwindow_get_mainwindow());
}

void rssyl_prop_cb(GtkAction *action, gpointer data)
{
	FolderView *folderview = (FolderView *)data;
	FolderItem *item;
	RSSylFolderItem *ritem;

	item = folderview_get_selected_item(folderview);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);

	debug_print("RSSyl: rssyl_prop_cb() for '%s'\n", item->name);

	ritem = (RSSylFolderItem *)item;
	rssyl_get_feed_props(ritem);
	rssyl_gtk_prop(ritem);
}

void rssyl_rename_cb(GtkAction *action, gpointer data)
{
	FolderView *folderview = (FolderView *)data;
	FolderItem *item;
	gchar *new_folder;
	gchar *name;
	gchar *message;

	item = folderview_get_selected_item(folderview);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->path != NULL);
	g_return_if_fail(item->folder != NULL);

	name = trim_string(item->name, 32);
	message = g_strdup_printf(_("Input new name for '%s':"), name);
	new_folder = input_dialog(_("Rename folder"), message, item->name);
	g_free(message);
	g_free(name);
	if (!new_folder) return;
	AUTORELEASE_STR(new_folder, {g_free(new_folder); return;});

	if (strchr(new_folder, G_DIR_SEPARATOR) != NULL) {
		alertpanel_error(_("'%c' can't be included in folder name."),
				 G_DIR_SEPARATOR);
		return;
	}

	if (folder_find_child_item_by_name(folder_item_parent(item), new_folder)) {
		name = trim_string(new_folder, 32);
		alertpanel_error(_("The folder '%s' already exists."), name);
		g_free(name);
		return;
	}

	if (folder_item_rename(item, new_folder) < 0) {
		alertpanel_error(_("The folder could not be renamed.\n"
				   "The new folder name is not allowed."));
		return;
	}

	folder_item_prefs_save_config_recursive(item);
	folder_write_list();
}

void rssyl_import_feed_list_cb(GtkAction *action, gpointer data)
{
	FolderView *folderview = (FolderView *)data;
	debug_print("RSSyl: rssyl_import_feed_cb\n");

	FolderItem *item;
	gchar *opmlfile = NULL;

	item = folderview_get_selected_item(folderview);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);

	opmlfile = filesel_select_file_open_with_filter(
			_("Select a .opml file"), NULL, "*.opml");
	
	if (!is_file_exist(opmlfile)) {
		g_free(opmlfile);
		return;
	}

	rssyl_opml_import(opmlfile, item);
}
