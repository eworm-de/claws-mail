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
#include <pthread.h>

/* Claws Mail includes */
#include <log.h>
#include <mainwindow.h>
#include <statusbar.h>
#include <main.h>

/* Local includes */
#include "libfeed/feed.h"
#include "rssyl.h"
#include "rssyl_feed.h"
#include "rssyl_parse_feed.h"
#include "rssyl_update_feed.h"
#include "parse822.h"

static void rssyl_update_reference_func(gpointer data, gpointer user_data)
{
	FeedItem *item = (FeedItem *)data;
	gchar *parent_id = (gchar *)user_data;

	g_return_if_fail(item != NULL);
	g_return_if_fail(user_data != NULL);

	feed_item_set_parent_id(item, parent_id);
}

void rssyl_update_comments(RFolderItem *ritem)
{
	FolderItem *item = &ritem->item;
	FeedItem *fi = NULL;
	RFetchCtx *fetchctx = NULL;
	RFeedCtx *feedctx = NULL;
	GDir *dp;
	const gchar *d;
	GError *error = NULL;
	gint num;
	gchar *path, *msg, *fname;
	MainWindow *mainwin = mainwindow_get_mainwindow();

	g_return_if_fail(ritem != NULL);

	if( ritem->fetch_comments == FALSE )
		return;

	path = folder_item_get_path(item);
	g_return_if_fail(path != NULL);

	debug_print("RSSyl: starting to parse comments, path is '%s'\n", path);

	if( (dp = g_dir_open(path, 0, &error)) == NULL ) {
		debug_print("g_dir_open on \"%s\" failed with error %d (%s)\n",
				path, error->code, error->message);
		g_error_free(error);
		g_free(path);
		return;
	}

	ritem->fetching_comments = TRUE;

	while( (d = g_dir_read_name(dp)) != NULL ) {
		if (claws_is_exiting()) {
			g_dir_close(dp);
			g_free(path);
			debug_print("RSSyl: bailing out, app is exiting\n");
			return;
		}

		if( (num = to_number(d)) > 0) {
			fname = g_strdup_printf("%s%c%s", path, G_DIR_SEPARATOR, d);
			if (!g_file_test(fname, G_FILE_TEST_IS_REGULAR)) {
				g_free(fname);
				continue;
			}

			debug_print("RSSyl: starting to parse '%s'\n", d);

			if( (fi = rssyl_parse_folder_item_file(fname)) != NULL ) {
				feedctx = (RFeedCtx *)fi->data;
				if( feed_item_get_comments_url(fi) && feed_item_get_id(fi) &&
						(ritem->fetch_comments_max_age == -1 ||
						 time(NULL) - feed_item_get_date_modified(fi) <= ritem->fetch_comments_max_age*86400)) {
					msg = g_strdup_printf(_("Updating comments for '%s'..."),
							feed_item_get_title(fi));
					debug_print("RSSyl: updating comments for '%s' (%s)\n",
							feed_item_get_title(fi), feed_item_get_comments_url(fi));
					STATUSBAR_PUSH(mainwin, msg);

					fetchctx = rssyl_prep_fetchctx_from_url(feed_item_get_comments_url(fi));
					if (fetchctx != NULL) {
						feed_set_ssl_verify_peer(fetchctx->feed, ritem->ssl_verify_peer);

						rssyl_fetch_feed(fetchctx, 0);
					
						if( fetchctx->success && feed_n_items(fetchctx->feed) > 0 ) {
							g_free(fetchctx->feed->title);
							fetchctx->feed->title = g_strdup(ritem->official_title);

							feed_foreach_item(fetchctx->feed, rssyl_update_reference_func,
									feed_item_get_id(fi));

							if( !rssyl_parse_feed(ritem, fetchctx->feed) ) {
								debug_print("RSSyl: Error processing comments feed\n");
								log_error(LOG_PROTOCOL, RSSYL_LOG_ERROR_PROC, fetchctx->feed->url);
							}
						}
					}
					STATUSBAR_POP(mainwin);
				}
			}

			if (feedctx != NULL) {
				g_free(feedctx->path);
			}
			feed_item_free(fi);
			g_free(fname);
		}
	}

	g_dir_close(dp);
	g_free(path);

	ritem->fetching_comments = FALSE;

	debug_print("RSSyl: rssyl_update_comments() is done\n");
}
