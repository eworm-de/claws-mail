/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2006-2025 the Claws Mail Team and Andrej Kacian <andrej@kacian.sk>
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
#include "libfeed/date.h"
#include "rssyl.h"
#include "rssyl_deleted.h"
#include "rssyl_feed.h"
#include "rssyl_parse_feed.h"
#include "rssyl_prefs.h"
#include "rssyl_update_comments.h"

extern const gchar *plugin_version(void);

/* rssyl_get_user_agent
   returns a ptr to a newly allocated string containing either the feed's specific user agent string
   (if set and if trimmed string is non-empty),
   or the custom user agent string set in global preferences (if prefs tells so and if trimmed string
   is not empty), otherwise the default user agent string.
   caller has to free the allocated string from pointer after use.
*/
gchar *rssyl_get_user_agent(RFolderItem *ritem)
{
	gchar *user_agent = NULL; 

	/* check if feed properties has a user agent enabled and set */
	/* ritem may be NULL when call comes from an initial fetch from url (when subscribing) */
	if (ritem != NULL && ritem->use_default_user_agent == FALSE) {
		gchar *specific = g_strstrip(g_strdup(ritem->specific_user_agent));

		if (strlen(specific) > 0)
			user_agent = specific;
		else
			debug_print("RSSyl: feed-specific User-Agent is empty, trying custom value from global prefs\n");
	}

	/* fallback to user agent value from preferences (if enabled and set) */
	if (user_agent == NULL) {
		if (rssyl_prefs.use_custom_user_agent) {
			gchar *custom = g_strstrip(g_strdup(rssyl_prefs.custom_user_agent));

			if (strlen(custom) > 0)
				user_agent = custom;
			else
				debug_print("RSSyl: custom User-Agent is empty, assuming default value\n");
		}
	}

	/* fallback to default User-Agent */
	if (user_agent == NULL)
		user_agent = g_strdup_printf(RSSYL_DEFAULT_UA);

	debug_print("RSSyl: User-Agent is %s\n", user_agent);
	return user_agent;
}

/* rssyl_fetch_feed_thr() */
static void *rssyl_fetch_feed_thr(void *arg)
{
	RFetchCtx *ctx = (RFetchCtx *)arg;
	gchar *user_agent = rssyl_get_user_agent(ctx->ritem);

	/* Fetch and parse the feed. */
	ctx->response_code = feed_update(ctx->feed, user_agent);
	g_free(user_agent);

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
	} else if( ctx->response_code >= 400 && ctx->response_code <= 599 ) {
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
	} else if (ctx->response_code != 304) {
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
	ctx->ritem = ritem;

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
	feed_set_etag(ctx->feed, ritem->etag);
	feed_set_last_modified(ctx->feed, ritem->last_modified);

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
	ctx->ritem = NULL;

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
	gchar *user_agent = NULL;

	g_return_val_if_fail(ritem != NULL, FALSE);
	g_return_val_if_fail(ritem->url != NULL, FALSE);
	
	user_agent = rssyl_get_user_agent(ritem);

	debug_print("RSSyl: starting to update '%s' (%s)\n",
			ritem->item.name, ritem->url);

	log_print(LOG_PROTOCOL, RSSYL_LOG_UPDATING, ritem->url, user_agent);
	g_free(user_agent);

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
		ritem->retry_after = ctx->feed->retry_after;
		if (ritem->retry_after) {
			gchar *tmp = createRFC822Date(&ritem->retry_after);
			log_print(LOG_PROTOCOL, RSSYL_LOG_RETRY_AFTER, ritem->url, tmp);
			g_free(tmp);
		}
		feed_free(ctx->feed);
		g_free(ctx->error);
		g_free(ctx);
		STATUSBAR_POP(mainwin);
		return FALSE;
	}
	/* Successful reply means we don't have to throttle auto-updates */
	ritem->retry_after = 0;

	g_free(ritem->etag);
	gchar *etag = feed_get_etag(ctx->feed);
	ritem->etag = etag ? g_strdup(etag) : NULL;
	g_free(ritem->last_modified);
	gchar *last_modified = feed_get_last_modified(ctx->feed);
	ritem->last_modified = last_modified ? g_strdup(last_modified) : NULL;

	/* Not modified, no content, nothing to parse */
	if (ctx->response_code == 304) {
		STATUSBAR_POP(mainwin);
		log_print(LOG_PROTOCOL, RSSYL_LOG_NOT_MODIFIED, ritem->url);
		goto cleanup;
	}

	rssyl_deleted_update(ritem);

	debug_print("RSSyl: Starting to parse feed\n");
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
	
	debug_print("RSSyl: Feed parsed\n");

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

cleanup:
	/* Clean up. */
	success = ctx->success;
	feed_free(ctx->feed);
	g_free(ctx->error);
	g_free(ctx);

	return success;
}

static gboolean rssyl_update_recursively_func(GNode *node, gpointer data, gboolean manual_refresh)
{
	FolderItem *item;
	RFolderItem *ritem;

	g_return_val_if_fail(node->data != NULL, FALSE);

	item = FOLDER_ITEM(node->data);
	ritem = (RFolderItem *)item;

	if( ritem->url != NULL ) {
		debug_print("RSSyl: %s refresh'\n", manual_refresh ? "Manual" : "Automated");
		if((manual_refresh == FALSE) &&
			rssyl_prefs_get()->refresh_all_skips &&
			(ritem->default_refresh_interval == FALSE) &&
			(ritem->refresh_interval == 0)) {
			debug_print("RSSyl: Skipping feed '%s'\n", item->name);
		} else {
			debug_print("RSSyl: Updating feed '%s'\n", item->name);
			rssyl_update_feed(ritem, 0);
		}
	} else
		debug_print("RSSyl: Updating in folder '%s'\n", item->name);

	return FALSE;
}

static gboolean rssyl_update_recursively_automated_func(GNode *node, gpointer data)
{
	return rssyl_update_recursively_func(node, data, FALSE);
}    

static gboolean rssyl_update_recursively_manually_func(GNode *node, gpointer data)
{
	return rssyl_update_recursively_func(node, data, TRUE);
}    

void rssyl_update_recursively(FolderItem *item, gboolean manual_refresh)
{
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);
	g_return_if_fail(item->folder->klass == rssyl_folder_get_class());

	debug_print("RSSyl: Recursively updating '%s'\n", item->name);

	g_node_traverse(item->node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
			manual_refresh ? rssyl_update_recursively_manually_func : rssyl_update_recursively_automated_func,
			NULL);
}

static void rssyl_update_all_func(FolderItem *item, gpointer data, gboolean manual_refresh)
{
	/* Only try to refresh our feed folders */
	g_return_if_fail(IS_RSSYL_FOLDER_ITEM(item));
	g_return_if_fail(folder_item_parent(item) == NULL);

	rssyl_update_recursively(item, manual_refresh);
}

static void rssyl_update_all_automated_func(FolderItem *item, gpointer data)
{
    rssyl_update_all_func(item, data, FALSE);
}

static void rssyl_update_all_manually_func(FolderItem *item, gpointer data)
{
    rssyl_update_all_func(item, data, TRUE);
}

void rssyl_update_all_feeds(gboolean manual_refresh)
{
	if (prefs_common_get_prefs()->work_offline &&
			!inc_offline_should_override(TRUE,
				_("Claws Mail needs network access in order to update your feeds.")) ) {
		return;
	}

	folder_func_to_all_folders(
			(FolderItemFunc)(manual_refresh? rssyl_update_all_manually_func : rssyl_update_all_automated_func),
			NULL);
}
