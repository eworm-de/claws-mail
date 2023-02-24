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
#include <common/claws.h>
#include <mainwindow.h>
#include <statusbar.h>
#include <alertpanel.h>
#include <log.h>
#include <prefs_common.h>
#include <inc.h>
#include <main.h>

/* Local includes */
#include "libfeed/feed.h"
#include "rssyl.h"
#include "rssyl_deleted.h"
#include "rssyl_feed.h"
#include "rssyl_parse_feed.h"
#include "rssyl_prefs.h"
#include "rssyl_update_comments.h"

/* rssyl_fetch_feed_thr() */

static void *rssyl_fetch_feed_thr(void *arg)
{
	RFetchCtx *ctx = (RFetchCtx *)arg;

	/* Fetch and parse the feed. */
	ctx->response_code = feed_update(ctx->feed, -1);

	/* Signal main thread that we're done here. */
	ctx->ready = TRUE;

	return NULL;
}

/* rssyl_fetch_feed() */
void rssyl_fetch_feed(RFetchCtx *ctx, RSSylVerboseFlags verbose)
{
#ifdef USE_PTHREAD
	pthread_t pt;
#endif

	g_return_if_fail(ctx != NULL);

#ifdef USE_PTHREAD
	if( pthread_create(&pt, NULL, rssyl_fetch_feed_thr,
				(void *)ctx) != 0 ) {
		/* Bummer, couldn't create thread. Continue non-threaded. */
		rssyl_fetch_feed_thr(ctx);
	} else {
		/* Thread created, let's wait until it finishes. */
		debug_print("RSSyl: waiting for thread to finish (timeout: %ds)\n",
				feed_get_timeout(ctx->feed));
		while( !ctx->ready ) {
			claws_do_idle();
		}

		debug_print("RSSyl: thread finished\n");
		pthread_join(pt, NULL);
	}
#else
	debug_print("RSSyl: no pthreads available, running non-threaded fetch\n");
	rssyl_fetch_feed_thr(ctx);
#endif

	debug_print("RSSyl: got response_code %d\n", ctx->response_code);

	if( ctx->response_code == FEED_ERR_INIT ) {
		debug_print("RSSyl: libfeed reports init error from libcurl\n");
		ctx->error = g_strdup("Internal error");
	} else if( ctx->response_code == FEED_ERR_FETCH ) {
		debug_print("RSSyl: libfeed reports some other error from libcurl\n");
		ctx->error = g_strdup(ctx->feed->fetcherr);
	} else if( ctx->response_code == FEED_ERR_UNAUTH ) {
		debug_print("RSSyl: URL authorization type is unknown\n");
		ctx->error = g_strdup("Unknown value for URL authorization type");
	} else if( ctx->response_code >= 400 && ctx->response_code < 500 ) {
		switch( ctx->response_code ) {
			case 401:
				ctx->error = g_strdup(_("401 (Authorisation required)"));
				break;
			case 403:
				ctx->error = g_strdup(_("403 (Forbidden)"));
				break;
			case 404:
				ctx->error = g_strdup(_("404 (Not found)"));
				break;
			default:
				ctx->error = g_strdup_printf(_("Error %d"), ctx->response_code);
				break;
		}
	}

	/* Here we handle "imperfect" conditions. If requested, we also
	 * display error dialogs for user. We always log the error. */
	if( ctx->error != NULL ) {
		/* libcurl wasn't happy */
		debug_print("RSSyl: Error: %s\n", ctx->error);
		if( verbose & RSSYL_SHOW_ERRORS) {
			gchar *msg = g_markup_printf_escaped(
					(const char *) C_("First parameter is URL, second is error text",
						"Error fetching feed at\n<b>%s</b>:\n\n%s"),
					feed_get_url(ctx->feed), ctx->error);
			alertpanel_error("%s", msg);
			g_free(msg);
		}

		log_error(LOG_PROTOCOL, RSSYL_LOG_ERROR_FETCH, ctx->feed->url, ctx->error);

		ctx->success = FALSE;
	} else {
		if( ctx->feed == NULL || ctx->response_code == FEED_ERR_NOFEED) {
			if( verbose & RSSYL_SHOW_ERRORS) {
				gchar *msg = g_markup_printf_escaped(
						(const char *) _("No valid feed found at\n<b>%s</b>"),
						feed_get_url(ctx->feed));
				alertpanel_error("%s", msg);
				g_free(msg);
			}

			log_error(LOG_PROTOCOL, RSSYL_LOG_ERROR_NOFEED,
					feed_get_url(ctx->feed));

			ctx->success = FALSE;
		} else if (feed_get_title(ctx->feed) == NULL) {
			/* We shouldn't do this, since a title is mandatory. */
			feed_set_title(ctx->feed, _("Untitled feed"));
			log_print(LOG_PROTOCOL,
					_("RSSyl: Possibly invalid feed without title at %s.\n"),
					feed_get_url(ctx->feed));
		}
	}
}

RFetchCtx *rssyl_prep_fetchctx_from_item(RFolderItem *ritem)
{
	RFetchCtx *ctx = NULL;

	g_return_val_if_fail(ritem != NULL, NULL);

	ctx = g_new0(RFetchCtx, 1);
	ctx->feed = feed_new(ritem->url);
	ctx->error = NULL;
	ctx->success = TRUE;
	ctx->ready = FALSE;

	if (ritem->auth->type != FEED_AUTH_NONE)
		ritem->auth->password = rssyl_passwd_get(ritem);

	feed_set_timeout(ctx->feed, prefs_common_get_prefs()->io_timeout_secs);
	feed_set_cookies_path(ctx->feed, rssyl_prefs_get()->cookies_path);
	feed_set_ssl_verify_peer(ctx->feed, ritem->ssl_verify_peer);
	feed_set_auth(ctx->feed, ritem->auth);
#ifdef G_OS_WIN32
	if (!g_ascii_strncasecmp(ritem->url, "https", 5)) {
		feed_set_cacert_file(ctx->feed, claws_ssl_get_cert_file());
		debug_print("RSSyl: using cert file '%s'\n", feed_get_cacert_file(ctx->feed));
	}
#endif

	return ctx;
}

RFetchCtx *rssyl_prep_fetchctx_from_url(gchar *url)
{
	RFetchCtx *ctx = NULL;

	g_return_val_if_fail(url != NULL, NULL);

	ctx = g_new0(RFetchCtx, 1);
	ctx->feed = feed_new(url);
	ctx->error = NULL;
	ctx->success = TRUE;
	ctx->ready = FALSE;

	feed_set_timeout(ctx->feed, prefs_common_get_prefs()->io_timeout_secs);
	feed_set_cookies_path(ctx->feed, rssyl_prefs_get()->cookies_path);
	feed_set_ssl_verify_peer(ctx->feed, rssyl_prefs_get()->ssl_verify_peer);
#ifdef G_OS_WIN32
	if (!g_ascii_strncasecmp(url, "https", 5)) {
		feed_set_cacert_file(ctx->feed, claws_ssl_get_cert_file());
		debug_print("RSSyl: using cert file '%s'\n", feed_get_cacert_file(ctx->feed));
	}
#endif

	return ctx;
}

/* rssyl_update_feed() */

gboolean rssyl_update_feed(RFolderItem *ritem, RSSylVerboseFlags verbose)
{
	RFetchCtx *ctx = NULL;
	MainWindow *mainwin = mainwindow_get_mainwindow();
	gchar *msg = NULL;
	gboolean success = FALSE;

	g_return_val_if_fail(ritem != NULL, FALSE);
	g_return_val_if_fail(ritem->url != NULL, FALSE);

	debug_print("RSSyl: starting to update '%s' (%s)\n",
			ritem->item.name, ritem->url);

	log_print(LOG_PROTOCOL, RSSYL_LOG_UPDATING, ritem->url);

	msg = g_strdup_printf(_("Updating feed '%s'..."), ritem->item.name);
	STATUSBAR_PUSH(mainwin, msg);
	g_free(msg);

	GTK_EVENTS_FLUSH();

	/* Prepare context for fetching the feed file */
	ctx = rssyl_prep_fetchctx_from_item(ritem);
	g_return_val_if_fail(ctx != NULL, FALSE);

	/* Fetch the feed file */
	rssyl_fetch_feed(ctx, verbose);

	if (ritem->auth != NULL && ritem->auth->password != NULL) {
		memset(ritem->auth->password, 0, strlen(ritem->auth->password));
		g_free(ritem->auth->password);
	}

	debug_print("RSSyl: fetch done; success == %s\n",
			ctx->success ? "TRUE" : "FALSE");

	if (!ctx->success) {
		feed_free(ctx->feed);
		g_free(ctx->error);
		g_free(ctx);
		STATUSBAR_POP(mainwin);
		return FALSE;
	}

	rssyl_deleted_update(ritem);

	debug_print("RSSyl: STARTING TO PARSE FEED\n");
  if( ctx->success && !(ctx->success = rssyl_parse_feed(ritem, ctx->feed)) ) {
		/* both libcurl and libfeed were happy, but we weren't */
		debug_print("RSSyl: Error processing feed\n");
		if( verbose & RSSYL_SHOW_ERRORS ) {
			gchar *msg = g_markup_printf_escaped(
					(const char *) _("Couldn't process feed at\n<b>%s</b>\n\n"
						"Please contact developers, this should not happen."),
					feed_get_url(ctx->feed));
			alertpanel_error("%s", msg);
			g_free(msg);
		}

		log_error(LOG_PROTOCOL, RSSYL_LOG_ERROR_PROC, ctx->feed->url);
	}
	
	debug_print("RSSyl: FEED PARSED\n");

	STATUSBAR_POP(mainwin);

	if( claws_is_exiting() ) {
		feed_free(ctx->feed);
		g_free(ctx->error);
		g_free(ctx);
		return FALSE;
	}

	if( ritem->fetch_comments )
		rssyl_update_comments(ritem);

	/* Prune our deleted items list of items which are no longer in
	 * upstream feed. */
	rssyl_deleted_expire(ritem, ctx->feed);
	rssyl_deleted_store(ritem);
	rssyl_deleted_free(ritem);

	/* Clean up. */
	success = ctx->success;
	feed_free(ctx->feed);
	g_free(ctx->error);
	g_free(ctx);

	return success;
}

static gboolean rssyl_update_recursively_func(GNode *node, gpointer data)
{
	FolderItem *item;
	RFolderItem *ritem;

	g_return_val_if_fail(node->data != NULL, FALSE);

	item = FOLDER_ITEM(node->data);
	ritem = (RFolderItem *)item;

	if( ritem->url != NULL ) {
		debug_print("RSSyl: Updating feed '%s'\n", item->name);
		rssyl_update_feed(ritem, 0);
	} else
		debug_print("RSSyl: Updating in folder '%s'\n", item->name);

	return FALSE;
}

void rssyl_update_recursively(FolderItem *item)
{
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);

	if( item->folder->klass != rssyl_folder_get_class() )
		return;

	debug_print("Recursively updating '%s'\n", item->name);

	g_node_traverse(item->node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
			rssyl_update_recursively_func, NULL);
}

void rssyl_update_all_func(FolderItem *item, gpointer data)
{
	/* Only try to refresh our feed folders */
	if( !IS_RSSYL_FOLDER_ITEM(item) )
		return;

	if( folder_item_parent(item) == NULL )
		rssyl_update_recursively(item);
}

void rssyl_update_all_feeds(void)
{
	if (prefs_common_get_prefs()->work_offline &&
			!inc_offline_should_override(TRUE,
				_("Claws Mail needs network access in order to update your feeds.")) ) {
		return;
	}

	folder_func_to_all_folders((FolderItemFunc)rssyl_update_all_func, NULL);
}
