/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2005-2023 the Claws Mail Team and Andrej Kacian <andrej@kacian.sk>
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
#endif

/* Global includes */
#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

/* Claws Mail includes */
#include <folderview.h>
#include <alertpanel.h>
#include <gtk/inputdialog.h>
#include <prefs_common.h>
#include <folder_item_prefs.h>
#include <filesel.h>
#include <inc.h>

/* Local includes */
#include "libfeed/parser_opml.h"
#include "rssyl_gtk.h"
#include "rssyl_feed.h"
#include "rssyl_feed_props.h"
#include "rssyl_update_feed.h"
#include "rssyl_subscribe.h"
#include "opml_import.h"

void rssyl_new_feed_cb(GtkAction *action,
		gpointer data)
{
	FolderView *folderview = (FolderView*)data;
	FolderItem *item;
	gchar *url;
	gchar *clip_text = NULL, *str = NULL;

	debug_print("RSSyl: new_feed_cb\n");

	g_return_if_fail(folderview->selected != NULL);

	item = folderview_get_selected_item(folderview);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);

	clip_text = gtk_clipboard_wait_for_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD));

	if (clip_text) {
		str = clip_text;
#if GLIB_CHECK_VERSION(2,66,0)
		GError *error = NULL;
		GUri *uri = NULL;

		/* skip any leading white-space */
		while (str && *str && g_ascii_isspace(*str))
			str++;
		uri = g_uri_parse(str, G_URI_FLAGS_PARSE_RELAXED, &error);
		if (error) {
			g_warning("could not parse clipboard text for URI: '%s'", error->message);
			g_error_free(error);
		}
		if (uri) {
			gchar* newstr = g_uri_to_string(uri);

			debug_print("URI: '%s' -> '%s'\n", str, newstr ? newstr : "N/A");
			if (newstr)
				g_free(newstr);
			g_uri_unref(uri);
		} else {
#else
		if (!is_uri_string(str)) {
#endif
			/* if no URL, ignore clipboard text */
			str = NULL;
		}
	}

	url = input_dialog(_("Subscribe feed"),
			_("Input the URL of the news feed you wish to subscribe:"),
			str ? str : "");

	if (clip_text)
		g_free(clip_text);

	if( url == NULL )	/* User cancelled */
		return;

	rssyl_subscribe(item, url, RSSYL_SHOW_ERRORS | RSSYL_SHOW_RENAME_DIALOG);

	g_free(url);
}

void rssyl_new_folder_cb(GtkAction *action,
		gpointer data)
{
	FolderView *folderview = (FolderView*)data;
	FolderItem *item;
	FolderItem *new_item;
	gchar *new_folder, *p, *tmp;
	gint i = 1;

	if (!folderview->selected) return;

	item = folderview_get_selected_item(folderview);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);

	new_folder = input_dialog(_("New folder"),
				  _("Input the name of new folder:"),
				  _("NewFolder"));
	if (!new_folder) return;

	p = strchr(new_folder, G_DIR_SEPARATOR);
	if (p) {
		alertpanel_error(_("'%c' can't be used in folder name."),
				 G_DIR_SEPARATOR);
		g_free(new_folder);
		return;
	}

	if (!folder_local_name_ok(new_folder)) {
		g_free(new_folder);
		return;
	}

	/* Find an unused name for new folder */
	/* TODO: Perhaps stop after X attempts? */
	tmp = g_strdup(new_folder);
	while (folder_find_child_item_by_name(item, tmp)) {
		debug_print("RSSyl: Folder '%s' already exists, trying another name\n",
				new_folder);
		g_free(tmp);
		tmp = g_strdup_printf("%s__%d", new_folder, ++i);
	}

	g_free(new_folder);
	new_folder = tmp;

	new_item = folder_create_folder(item, new_folder);
	if (!new_item) {
		alertpanel_error(_("Can't create the folder '%s'."), new_folder);
		g_free(new_folder);
		return;
	}

	g_free(new_folder);

	folder_write_list();
}

void rssyl_remove_folder_cb(GtkAction *action,
			     gpointer data)
{
	FolderView *folderview = (FolderView*)data;
	FolderItem *item, *opened;
	gchar *message, *name;
	AlertValue avalue;
	gchar *old_id;

	item = folderview_get_selected_item(folderview);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->path != NULL);
	g_return_if_fail(item->folder != NULL);
	opened = folderview_get_opened_item(folderview);

	name = trim_string(item->name, 32);
	AUTORELEASE_STR(name, {g_free(name); return;});
	message = g_strdup_printf
		(_("All folders and messages under '%s' will be permanently deleted.\n"
		   "Recovery will not be possible.\n\n"
		   "Do you really want to delete?"), name);
	avalue = alertpanel_full(_("Delete folder"), message,
				 NULL, _("_Cancel"),  "edit-delete", _("_Delete"),
				 NULL, NULL, ALERTFOCUS_FIRST, FALSE,
				 NULL, ALERT_WARNING);
	g_free(message);
	if (avalue != G_ALERTALTERNATE) return;

	old_id = folder_item_get_identifier(item);

	if (item == opened ||
			folder_is_child_of(item, opened)) {
		summary_clear_all(folderview->summaryview);
		folderview_close_opened(folderview, TRUE);
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

void rssyl_rename_cb(GtkAction *action,
			     gpointer *data)
{
	FolderItem *item;
	gchar *new_folder;
	gchar *message;
	FolderView *folderview = (FolderView*)data;
	item = folderview_get_selected_item(folderview);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->path != NULL);
	g_return_if_fail(item->folder != NULL);

	message = g_strdup_printf(_("Input new name for '%s':"), item->name);
	new_folder = input_dialog(_("Rename folder"), message, item->name);
	g_free(message);
	if (!new_folder) return;

	if (strchr(new_folder, G_DIR_SEPARATOR) != NULL) {
		alertpanel_error(_("'%c' can't be included in folder name."),
				 G_DIR_SEPARATOR);
		g_free(new_folder);
		return;
	}

	if (!folder_local_name_ok(new_folder)) {
		g_free(new_folder);
		return;
	}

	if (folder_find_child_item_by_name(folder_item_parent(item), new_folder)) {
		alertpanel_error(_("The folder '%s' already exists."), new_folder);
		g_free(new_folder);
		return;
	}

	if (folder_item_rename(item, new_folder) < 0) {
		alertpanel_error(_("The folder could not be renamed.\n"
				   "The new folder name is not allowed."));
		g_free(new_folder);
		return;
	}
	g_free(new_folder);

	folder_item_prefs_save_config(item);
	prefs_matcher_write_config();
	folder_write_list();
}

void rssyl_refresh_feed_cb(GtkAction *action,
		gpointer data)
{
	FolderView *folderview = (FolderView*)data;
	FolderItem *item = NULL;
	RFolderItem *ritem = NULL;

	item = folderview_get_selected_item(folderview);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);

	ritem = (RFolderItem *)item;

	/* Offline check */
	if( prefs_common_get_prefs()->work_offline &&
			!inc_offline_should_override(TRUE,
					ngettext("Claws Mail needs network access in order "
					"to update the feed.",
					"Claws Mail needs network access in order "
					"to update feeds.", 1))) {
		return;
	}

	/* Update feed, displaying errors if any. */
	rssyl_update_feed(ritem, RSSYL_SHOW_ERRORS);
}

void rssyl_prop_cb(GtkAction *action, gpointer data)
{
	FolderView *folderview = (FolderView*)data;
	FolderItem *item;
	RFolderItem *ritem;

	item = folderview_get_selected_item(folderview);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);

	debug_print("RSSyl: rssyl_prop_cb() for '%s'\n", item->name);

	ritem = (RFolderItem *)item;

	rssyl_gtk_prop(ritem);
}

void rssyl_update_all_cb( GtkAction *action, gpointer data)
{
	FolderItem *item;
	FolderView *folderview = (FolderView*)data;

	item = folderview_get_selected_item(folderview);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);

	debug_print("RSSyl: rssyl_update_all_cb(): clicked on '%s'\n", item->name);

	if( item->folder->klass != rssyl_folder_get_class() ) {
		debug_print("RSSyl: this is not a RSSyl folder, returning\n");
		return;
	}

	/* Offline check */
	if( prefs_common_get_prefs()->work_offline &&
			!inc_offline_should_override(TRUE,
					ngettext("Claws Mail needs network access in order "
					"to update the feed.",
					"Claws Mail needs network access in order "
					"to update feeds.", 1))) {
		return;
	}

	rssyl_update_recursively(item);
}

void rssyl_remove_mailbox_cb(GtkAction *action, gpointer data)
{
	FolderView *folderview = (FolderView *)data;
	FolderItem *item = NULL;
	gchar *n, *message;
	AlertValue avalue;

	item = folderview_get_selected_item(folderview);

	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);

	if( folder_item_parent(item) )
		return;

	n = trim_string(item->folder->name, 32);
	message = g_strdup_printf(_("Really remove the feed tree `%s' ?\n"), n);
	avalue = alertpanel_full(_("Remove feed tree"), message,
				 NULL, _("_Cancel"), "list-remove", _("_Remove"),
				 NULL, NULL, ALERTFOCUS_FIRST, FALSE,
				 NULL, ALERT_WARNING);
	g_free(message);
	g_free(n);

	if( avalue != G_ALERTALTERNATE )
		return;

	folderview_unselect(folderview);
	summary_clear_all(folderview->summaryview);

	n = folder_item_get_path(item);
	if( remove_dir_recursive(n) < 0 ) {
		g_warning("can't remove directory '%s'", n);
		g_free(n);
		return;
	}

	g_free(n);
	folder_destroy(item->folder);
}

void rssyl_import_feed_list_cb(GtkAction *action, gpointer data)
{
	FolderView *folderview = (FolderView *)data;
	FolderItem *item = NULL;
	gchar *path = NULL;
	OPMLImportCtx *ctx = NULL;

	debug_print("RSSyl: import_feed_list_cb\n");

	/* Ask user for a file to import */
	path = filesel_select_file_open_with_filter(
			_("Select an OPML file"), NULL, "*.opml");
	if (!is_file_exist(path)) {
		g_free(path);
		return;
	}

	/* Find the destination folder for the import */
	g_return_if_fail(folderview->selected != NULL);
	item = folderview_get_selected_item(folderview);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);

	ctx = g_malloc( sizeof(OPMLImportCtx) );
	ctx->failures = 0;
	ctx->depth = rssyl_folder_depth(item) + 1;
	ctx->current = NULL;
	ctx->current = g_slist_append(ctx->current, item);

	/* Start the whole shebang - call libfeed's OPML parser with correct
	 * user function */
	opml_process(path, rssyl_opml_import_func, (gpointer)ctx);

	g_free(ctx);
}
