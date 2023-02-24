/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2006-2023 the Claws Mail Team and Andrej Kacian <andrej@kacian.sk>
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
#include <glib/gi18n.h>
#include <gtk/gtk.h>

/* Claws Mail includes */
#include <alertpanel.h>
#include <folder_item_prefs.h>
#include <log.h>
#include <mainwindow.h>
#include <matcher.h>
#include <msgcache.h>

/* Local includes */
#include "old_feeds.h"
#include "rssyl.h"
#include "rssyl_feed.h"
#include "strutils.h"
#include "file-utils.h"

struct _RUpdateFormatCtx {
	FolderItem *o_prev;
	FolderItem *o_parent;
	FolderItem *n_prev;
	FolderItem *n_parent;
	Folder *n_first;
	GSList *oldfeeds;
	GSList *oldroots;
	gboolean reached_first_new;
};

typedef struct _RUpdateFormatCtx RUpdateFormatCtx;

extern FolderClass rssyl_class;

static void rssyl_update_format_move_contents(FolderItem *olditem,
		FolderItem *newitem);
static gchar *_old_rssyl_item_get_path(Folder *folder, FolderItem *item);
static void _delete_old_roots_func(gpointer data, gpointer user_data);

static void rssyl_update_format_func(FolderItem *item, gpointer data)
{
	RFolderItem *ritem;
	RUpdateFormatCtx *ctx = (RUpdateFormatCtx *)data;
	Folder *f = NULL;
	FolderItem *new_item = NULL;
	gchar *name;
	OldRFeed *of;

	if( !IS_RSSYL_FOLDER_ITEM(item) )
		return;

	/* Do not do anything once we reached first new folder
	 * (which we created earlier in this process) */
	if( ctx->reached_first_new )
		return;

	if( item->folder == ctx->n_first ) {
		ctx->reached_first_new = TRUE;
		debug_print("RSSyl: (FORMAT) reached first new folder\n");
		return;
	}

	debug_print("RSSyl: (FORMAT) item '%s'\n", item->name);

	if( folder_item_parent(item) == NULL ) {
		/* Root rssyl folder */
		ctx->oldroots = g_slist_prepend(ctx->oldroots, item);

		/* Create its counterpart */
		name = rssyl_strreplace(folder_item_get_name(item), " (RSSyl)", "");
		debug_print("RSSyl: (FORMAT) adding new root folder '%s'\n", name);
		f = folder_new(rssyl_folder_get_class(), name, NULL);
		g_free(name);
		g_return_if_fail(f != NULL);
		folder_add(f);

		folder_write_list();

		new_item = FOLDER_ITEM(f->node->data);

		/* If user has more than one old rssyl foldertrees, keep the n_first
		 * pointer at the beginning of first one. */
		if (ctx->n_first == NULL)
			ctx->n_first = f;

		ctx->n_parent = new_item;
	} else {
		/* Non-root folder */

		if (folder_item_parent(item) == ctx->o_prev) {
			/* We went one step deeper in folder hierarchy, adjust pointers
			 * to parents */
			ctx->o_parent = ctx->o_prev;
			ctx->n_parent = ctx->n_prev;
		} else if (folder_item_parent(item) != ctx->o_parent) {
			/* We are not in same folder anymore, which can only mean we have
			 * moved up in the hierarchy. Find a correct parent */
			while (folder_item_parent(item) != ctx->o_parent) {
				ctx->o_parent = folder_item_parent(ctx->o_parent);
				ctx->n_parent = folder_item_parent(ctx->n_parent);
				if (ctx->o_parent == NULL) {
					/* This shouldn't happen, unless we are magically moved to a
					 * completely different folder structure */
					debug_print("RSSyl: MISHAP WHILE UPGRADING STORAGE FORMAT: couldn't find folder parent\n");
					alertpanel_error(_("Internal problem while upgrading storage format. This should not happen. Please report this, with debug output attached.\n"));
					return;
				}
			}
		} else {
			/* We have remained in the same subfolder, nothing to do here */
		}

		debug_print("RSSyl: (FORMAT) adding folder '%s'\n", item->name);
		new_item = folder_create_folder(ctx->n_parent, item->name);

		if (new_item == NULL) {
			debug_print("RSSyl: (FORMAT) couldn't add folder '%s', skipping it\n",
					item->name);
			return;
		}

		of = rssyl_old_feed_get_by_name(ctx->oldfeeds, item->name);
		if (of != NULL && of->url != NULL) {
			/* Folder with an actual subscribed feed */
			debug_print("RSSyl: (FORMAT) making '%s' a feed with URL '%s'\n",
					item->name, of->url);

			ritem = (RFolderItem *)new_item;
			ritem->url = g_strdup(of->url);

			rssyl_feed_start_refresh_timeout(ritem);

			ritem->official_title = g_strdup(of->official_name);
			ritem->default_refresh_interval =
				(of->default_refresh_interval != 0 ? TRUE : FALSE);
			ritem->refresh_interval = of->refresh_interval;
			ritem->keep_old = (of->expired_num > -1 ? TRUE : FALSE);
			ritem->fetch_comments =
				(of->fetch_comments != 0 ? TRUE : FALSE);
			ritem->fetch_comments_max_age = of->fetch_comments_for;
			ritem->silent_update = of->silent_update;
			ritem->ssl_verify_peer = of->ssl_verify_peer;

			folder_item_prefs_copy_prefs(item, &ritem->item);
		}

		rssyl_update_format_move_contents(item, new_item);

		/* destroy the new folder's cache so we'll re-read the migrated one */
		if (new_item->cache) {
			msgcache_destroy(new_item->cache);
			new_item->cache = NULL;
		}

		/* Store folderlist with the new folder */
		folder_item_scan(new_item);
		folder_write_list();
	}

	ctx->o_prev = item;
	ctx->n_prev = new_item;
}


void rssyl_update_format()
{
	RUpdateFormatCtx *ctx = NULL;
	GSList *oldfeeds;
	gchar *old_feeds_xml = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
			RSSYL_DIR, G_DIR_SEPARATOR_S, "feeds.xml", NULL);

	if (!g_file_test(old_feeds_xml,
				G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
		g_free(old_feeds_xml);
		return;
	}

	debug_print("RSSyl: Old format found, updating.\n");

	oldfeeds = rssyl_old_feed_metadata_parse(old_feeds_xml);

	/* We find all rssyl root folders and perform magic on each */
	ctx = g_new0(RUpdateFormatCtx, 1);
	ctx->o_prev = NULL;
	ctx->o_parent = NULL;
	ctx->n_prev = NULL;
	ctx->n_parent = NULL;
	ctx->n_first = NULL;
	ctx->oldfeeds = oldfeeds;
	ctx->oldroots = NULL;
	ctx->reached_first_new = FALSE;

	folder_item_update_freeze();

	/* Go through all RSSyl folders, making new copies */
	folder_func_to_all_folders((FolderItemFunc)rssyl_update_format_func, ctx);

	g_slist_foreach(ctx->oldroots, _delete_old_roots_func, NULL);
	g_slist_free(ctx->oldroots);

	prefs_matcher_write_config();
	folder_write_list();

	folder_item_update_thaw();

	g_free(ctx);

	if (g_remove(old_feeds_xml) != 0) {
		debug_print("RSSyl: Couldn't delete '%s'\n", old_feeds_xml);
	}
	g_free(old_feeds_xml);
}

static void _delete_old_roots_func(gpointer data, gpointer user_data)
{
	FolderItem *item = (FolderItem *)data;

	folder_destroy(item->folder);
}

/* Copy each item in a feed to the new directory */
static void rssyl_update_format_move_contents(FolderItem *olditem,
		FolderItem *newitem)
{
	gchar *oldpath, *newpath, *fname, *fpath, *nfpath;
	GDir *d = NULL;
	GError *error = NULL;

	oldpath = _old_rssyl_item_get_path(NULL, olditem);
	newpath = folder_item_get_path(newitem);

	if ((d = g_dir_open(oldpath, 0, &error)) == NULL) {
		debug_print("RSSyl: (FORMAT) couldn't open dir '%s': %s\n", oldpath,
				error->message);
		g_free(oldpath);
		g_free(newpath);
		g_error_free(error);
		return;
	}

	debug_print("RSSyl: (FORMAT) moving contents of '%s' to '%s'\n",
			oldpath, newpath);

	while ((fname = (gchar *)g_dir_read_name(d)) != NULL) {
		gboolean migrate_file = to_number(fname) > 0 || strstr(fname, ".claws_") == fname;
		fpath = g_strconcat(oldpath, G_DIR_SEPARATOR_S, fname, NULL);
		if (migrate_file && g_file_test(fpath, G_FILE_TEST_IS_REGULAR)) {
			nfpath = g_strconcat(newpath, G_DIR_SEPARATOR_S, fname, NULL);
			move_file(fpath, nfpath, FALSE);
			g_free(nfpath);
		}
		if (g_remove(fpath) != 0) {
			debug_print("RSSyl: (FORMAT) couldn't delete '%s'\n", fpath);
		}
		g_free(fpath);
	}

	g_dir_close(d);
	g_rmdir(oldpath);

	g_free(oldpath);
	g_free(newpath);
}

static gchar *_old_rssyl_item_get_path(Folder *folder, FolderItem *item)
{
	gchar *result, *tmp;

	if (folder_item_parent(item) == NULL)
		return g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, RSSYL_DIR, NULL);

	tmp = rssyl_strreplace(item->name, "/", "\\");
	result = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, RSSYL_DIR,
			G_DIR_SEPARATOR_S, tmp, NULL);
	g_free(tmp);
	return result;
}
