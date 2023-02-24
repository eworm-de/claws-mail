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
#include <glib.h>
#include <glib/gi18n.h>

/* Claws Mail includes */
#include <log.h>
#include <codeconv.h>
#include <main.h>
#include <procmsg.h>

/* Local includes */
#include "libfeed/feed.h"
#include "libfeed/feeditem.h"
#include "libfeed/date.h"
#include "parse822.h"
#include "rssyl.h"
#include "rssyl_add_item.h"
#include "rssyl_deleted.h"
#include "rssyl_feed.h"
#include "rssyl_parse_feed.h"
#include "rssyl_prefs.h"
#include "strutils.h"

static void rssyl_foreach_parse_func(gpointer data, gpointer user_data)
{
	FeedItem *feed_item = (FeedItem *)data;
	RFolderItem *ritem = (RFolderItem *)user_data;

	rssyl_add_item(ritem, feed_item);
}

struct _RSSylExpireItemsCtx {
	gboolean exists;
	FeedItem *item;
	GSList *expired_ids;
};

typedef struct _RSSylExpireItemsCtx RSSylExpireItemsCtx;

static void expire_items_func(gpointer data, gpointer user_data)
{
	RSSylExpireItemsCtx *ctx = (RSSylExpireItemsCtx *)user_data;
	FeedItem *item = (FeedItem *)data;
	gchar *id = NULL, *id2 = NULL;

	if( (id = feed_item_get_id(item)) == NULL )
		id = feed_item_get_url(item);

	if( id == NULL )
		return;

	if( (id2 = feed_item_get_id(ctx->item)) == NULL )
		id2 = feed_item_get_url(ctx->item);

	if( id2 == NULL )
		return;

	/* Simply check ID, as we should have up-to-date items right now. */
	if( !strcmp(id, id2) )
		ctx->exists = TRUE;
}

static void rssyl_expire_items(RFolderItem *ritem, Feed *feed)
{
	FeedItem *item = NULL;
	GSList *i = NULL;
	RSSylExpireItemsCtx *ctx = NULL;
	RFeedCtx *fctx;

	debug_print("RSSyl: rssyl_expire_items()\n");

	g_return_if_fail(ritem != NULL);
	g_return_if_fail(ritem->items != NULL);
	g_return_if_fail(feed != NULL);

	ctx = g_malloc( sizeof(RSSylExpireItemsCtx) );
	ctx->expired_ids = NULL;

	/* Check each locally stored item, if it is still in the upstream
	 * feed - xnay it if not. */
	for( i = ritem->items; i != NULL; i = i->next ) {
		item = (FeedItem *)i->data;

		/* Comments will be expired later, once we know which parent items
		 * have been expired. */
		if (feed_item_get_parent_id(item) != NULL)
			continue;

		/* Find matching item in the fresh feed. */
		ctx->exists = FALSE;
		ctx->item = item;
		feed_foreach_item(feed, expire_items_func, ctx);

		if( !ctx->exists ) {
			/* No match, add item ids to the list and get rid of it. */
			debug_print("RSSyl: expiring '%s'\n", feed_item_get_id(item));
			ctx->expired_ids = g_slist_prepend(ctx->expired_ids,
					g_strdup(feed_item_get_id(item)));
			fctx = (RFeedCtx *)item->data;
			if (g_remove(fctx->path) != 0) {
				debug_print("RSSyl: couldn't delete expiring item file '%s'\n",
						fctx->path);
			}
		}
	}

	/* Now do one more pass over folder contents, and expire comments
	 * whose parents are gone. */
	for( i = ritem->items; i != NULL; i = i->next ) {
		item = (FeedItem *)i->data;

		/* Handle comments expiration. */
		if (feed_item_get_parent_id(item) != NULL) {
			/* If its parent's id is on list of expired ids, this comment
			 * can go as well. */
			if (g_slist_find_custom(ctx->expired_ids,
					feed_item_get_parent_id(item), (GCompareFunc)g_strcmp0)) {
				debug_print("RSSyl: expiring comment '%s'\n", feed_item_get_id(item));
				fctx = (RFeedCtx *)item->data;
				if (g_remove(fctx->path) != 0) {
					debug_print("RSSyl: couldn't delete expiring comment file '%s'\n",
							fctx->path);
				}
			}
		}
	}

	debug_print("RSSyl: expired %d items\n", g_slist_length(ctx->expired_ids));

	slist_free_strings_full(ctx->expired_ids);
	g_free(ctx);
}

/* -------------------------------------------------------------------------
 * rssyl_parse_feed() */

gboolean rssyl_parse_feed(RFolderItem *ritem, Feed *feed)
{
	gchar *tmp = NULL, *tmp2 = NULL;
	gint i = 1;

	g_return_val_if_fail(ritem != NULL, FALSE);
	g_return_val_if_fail(feed != NULL, FALSE);
	g_return_val_if_fail(feed->title != NULL, FALSE);

	debug_print("RSSyl: parse_feed\n");

	/* Set the last_update timestamp here, so it is the same for all items */
	ritem->last_update = time(NULL);

	/* If the upstream feed changed its title, change name of our folder
	 * accordingly even if user has renamed it before. This makes sure that
	 * user will be aware of the upstream title change. */
	if( !ritem->ignore_title_rename &&
			(ritem->official_title == NULL ||
			strcmp(feed->title, ritem->official_title)) ) {
		g_free(ritem->official_title);
		ritem->official_title = g_strdup(feed->title);

		tmp = rssyl_format_string(feed->title, TRUE, TRUE);

		tmp2 = g_strdup(tmp);
		while (folder_item_rename(&ritem->item, tmp2) != 0 && i < 20) {
			g_free(tmp2);
			tmp2 = g_strdup_printf("%s__%d", tmp, ++i);
			debug_print("RSSyl: couldn't rename, trying '%s'\n", tmp2);
		}
		/* TODO: handle case when i reaches 20 */
	
		g_free(tmp);
		g_free(tmp2);

		/* FIXME: update name in properties */
		/* FIXME: store feed properties */
	}

	folder_item_update_freeze();

	/* Read contents of folder, so we can check for duplicates/updates */
	rssyl_folder_read_existing(ritem);

	if( claws_is_exiting() ) {
		debug_print("RSSyl: Claws Mail is exiting, bailing out\n");
		log_print(LOG_PROTOCOL, RSSYL_LOG_ABORTED_EXITING, ritem->url);
		folder_item_update_thaw();
		return TRUE;
	}

	/* Parse each item in the feed, adding or updating existing items if
	 * necessary */
	if( feed_n_items(feed) > 0 )
		feed_foreach_item(feed, rssyl_foreach_parse_func, (gpointer)ritem);

	if( !ritem->keep_old && !ritem->fetching_comments ) {
		rssyl_folder_read_existing(ritem);
		rssyl_expire_items(ritem, feed);
	}

	folder_item_scan(&ritem->item);
	folder_item_update_thaw();

	if( !ritem->fetching_comments )
		log_print(LOG_PROTOCOL, RSSYL_LOG_UPDATED, ritem->url);

	return TRUE;
}
