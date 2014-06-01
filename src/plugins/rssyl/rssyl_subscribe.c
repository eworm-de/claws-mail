/*
 * Claws-Mail-- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2004 Hiroyuki Yamamoto
 * This file (C) 2005 Andrej Kacian <andrej@kacian.sk>
 *
 * - DESCRIPTION HERE
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
#endif

/* Global includes */
#include <glib/gi18n.h>
#include <gtk/gtk.h>

/* Claws Mail includes */
#include <alertpanel.h>
#include <folder.h>
#include <log.h>
#include <common/utils.h>

/* Local includes */
#include "libfeed/feed.h"
#include "libfeed/feeditem.h"
#include "rssyl.h"
#include "rssyl_add_item.h"
#include "rssyl_feed.h"
#include "rssyl_update_feed.h"
#include "rssyl_subscribe_gtk.h"
#include "strutils.h"

static void rssyl_subscribe_foreach_func(gpointer data, gpointer user_data)
{
	RFolderItem *ritem = (RFolderItem *)user_data;
	FeedItem *feed_item = (FeedItem *)data;

	g_return_if_fail(ritem != NULL);
	g_return_if_fail(feed_item != NULL);

	rssyl_add_item(ritem, feed_item);
}

gboolean rssyl_subscribe(FolderItem *parent, const gchar *url,
		gboolean verbose)
{
	gchar *myurl = NULL, *tmpname = NULL, *tmpname2 = NULL;
	RFetchCtx *ctx;
	FolderItem *new_item;
	RFolderItem *ritem;
	gint i = 1;
	RSubCtx *sctx;

	g_return_val_if_fail(parent != NULL, FALSE);
	g_return_val_if_fail(url != NULL, FALSE);

	log_print(LOG_PROTOCOL, RSSYL_LOG_SUBSCRIBING, url);

	myurl = my_normalize_url(url);

	/* Fetch the feed. */
	ctx = rssyl_prep_fetchctx_from_url(myurl);
	g_free(myurl);
	g_return_val_if_fail(ctx != NULL, FALSE);

	rssyl_fetch_feed(ctx, verbose);

	debug_print("RSSyl: fetch success == %s\n",
			ctx->success ? "TRUE" : "FALSE");

	if (!ctx->success) {
		/* User notification was already handled inside rssyl_fetch_feed(),
		 * let's just return quietly. */
		feed_free(ctx->feed);
		g_free(ctx->error);
		g_free(ctx);
		return FALSE;
	}

	if (verbose) {
		sctx = g_new0(RSubCtx, 1);
		sctx->feed = ctx->feed;

		debug_print("RSSyl: Calling subscribe dialog routine...\n");
		rssyl_subscribe_dialog(sctx);

		if (sctx->feed == NULL) {
			debug_print("RSSyl: User cancelled subscribe.\n");
			g_free(sctx);
			return FALSE;
		}
	}

	/* OK, feed is succesfully fetched and correct, let's add it to CM. */

	/* Create a folder for it. */
	tmpname = rssyl_format_string(ctx->feed->title, TRUE, TRUE);
	tmpname2 = g_strdup(tmpname);
	while (folder_find_child_item_by_name(parent, tmpname2) != 0 && i < 20) {
		debug_print("RSSyl: Folder '%s' already exists, trying another name\n",
				tmpname2);
		g_free(tmpname2);
		tmpname2 = g_strdup_printf("%s__%d", tmpname, ++i);
	}
	/* TODO: handle cases where i reaches 20 */

	folder_item_update_freeze();

	new_item = folder_create_folder(parent, tmpname2);
	g_free(tmpname);
	g_free(tmpname2);

	if (!new_item) {
		if (verbose)
			alertpanel_error(_("Couldn't create folder for new feed '%s'."),
					myurl);
		feed_free(ctx->feed);
		g_free(ctx->error);
		g_free(ctx); 
		g_free(myurl);
		return FALSE;
	}

	debug_print("RSSyl: Adding '%s'\n", ctx->feed->url);

	ritem = (RFolderItem *)new_item;
	ritem->url = g_strdup(ctx->feed->url);

	if (feed_n_items(ctx->feed) > 0)
		feed_foreach_item(ctx->feed, rssyl_subscribe_foreach_func, (gpointer)ritem);

	folder_item_scan(new_item);
	folder_write_list();
	folder_item_update_thaw();

	return TRUE;
}
