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

#include "config.h"

#include <stdlib.h>
#include <glib.h>
#include <curl/curl.h>
#include <expat.h>

#include "feed.h"
#include "parser.h"

/* feed_new()
 * Initializes new Feed struct, setting its url and a default timeout. */
Feed *feed_new(gchar *url)
{
	Feed *feed = NULL;

	g_return_val_if_fail(url != NULL, NULL);

	feed = g_malloc( sizeof(Feed) );

	feed->is_valid = TRUE;
	feed->timeout = FEED_DEFAULT_TIMEOUT;
	feed->url = g_strdup(url);
	feed->auth = NULL;
	feed->title = NULL;
	feed->description = NULL;
	feed->language = NULL;
	feed->author = NULL;
	feed->generator = NULL;
	feed->link = NULL;
	feed->items = NULL;

	feed->fetcherr = NULL;
	feed->cookies_path = NULL;

	feed->ssl_verify_peer = TRUE;
	feed->cacert_file = NULL;

	return feed;
}

static void _free_items(gpointer item, gpointer nada)
{
	feed_item_free(item);
}

static void _free_auth(Feed *feed)
{
	if (feed == NULL)
		return;

	if (feed->auth != NULL) {
                if (feed->auth->username != NULL)
                        g_free(feed->auth->username);
                if (feed->auth->password != NULL)
                        g_free(feed->auth->password);
                g_free(feed->auth);
		feed->auth = NULL;
        }
}

void feed_free(Feed *feed)
{
	if( feed == NULL )
		return;	/* Return silently, without printing a glib error. */

	g_free(feed->url);
	_free_auth(feed);
	g_free(feed->title);
	g_free(feed->description);
	g_free(feed->language);
	g_free(feed->author);
	g_free(feed->generator);
	g_free(feed->link);
	g_free(feed->fetcherr);
	g_free(feed->cookies_path);
	g_free(feed->cacert_file);

	if( feed->items != NULL ) {
		g_slist_foreach(feed->items, _free_items, NULL);
		g_slist_free(feed->items);
	}

	g_free(feed);
	feed = NULL;
}

void feed_free_items(Feed *feed)
{
	if( feed == NULL )
		return;

	if( feed->items != NULL ) {
		g_slist_foreach(feed->items, _free_items, NULL);
		g_slist_free(feed->items);
		feed->items = NULL;
	}
}

/* Timeout */
void feed_set_timeout(Feed *feed, guint timeout)
{
	g_return_if_fail(feed != NULL);
	feed->timeout = timeout;
}

guint feed_get_timeout(Feed *feed)
{
	g_return_val_if_fail(feed != NULL, 0);
	return feed->timeout;
}

/* URL */
void feed_set_url(Feed *feed, gchar *url)
{
	g_return_if_fail(feed != NULL);
	g_return_if_fail(url != NULL);

	if( feed->url != NULL ) {
		g_free(feed->url);
		feed->url = NULL;
	}

	feed->url = g_strdup(url);
}

gchar *feed_get_url(Feed *feed)
{
	g_return_val_if_fail(feed != NULL, NULL);
	return feed->url;
}

/* Auth */
void feed_set_auth(Feed *feed, FeedAuth *auth)
{
	g_return_if_fail(feed != NULL);
	g_return_if_fail(auth != NULL);

	_free_auth(feed);
	feed->auth = g_new0(FeedAuth, 1);
	feed->auth->type = auth->type;
	feed->auth->username = g_strdup(auth->username);
	feed->auth->password = g_strdup(auth->password);
}

FeedAuth *feed_get_auth(Feed *feed)
{
	g_return_val_if_fail(feed != NULL, NULL);
	return feed->auth;
}

/* Title */
gchar *feed_get_title(Feed *feed)
{
	g_return_val_if_fail(feed != NULL, NULL);
	return feed->title;
}

void feed_set_title(Feed *feed, gchar *new_title)
{
	g_return_if_fail(feed != NULL);
	g_return_if_fail(new_title != NULL);

	if (feed->title != NULL) {
		g_free(feed->title);
		feed->title = NULL;
	}

	feed->title = g_strdup(new_title);
}

/* Description */
gchar *feed_get_description(Feed *feed)
{
	g_return_val_if_fail(feed != NULL, NULL);
	return feed->description;
}

/* Language */
gchar *feed_get_language(Feed *feed)
{
	g_return_val_if_fail(feed != NULL, NULL);
	return feed->language;
}

/* Author */
gchar *feed_get_author(Feed *feed)
{
	g_return_val_if_fail(feed != NULL, NULL);
	return feed->author;
}

/* Generator */
gchar *feed_get_generator(Feed *feed)
{
	g_return_val_if_fail(feed != NULL, NULL);
	return feed->generator;
}

/* Fetch error (if not NULL, supplied by libcurl) */
gchar *feed_get_fetcherror(Feed *feed)
{
	g_return_val_if_fail(feed != NULL, NULL);
	return feed->fetcherr;
}

/* Returns number of items currently in the feed. */
gint feed_n_items(Feed *feed)
{
	g_return_val_if_fail(feed != NULL, -1);

	if( feed->items == NULL )	/* No items here. */
		return 0;

	return g_slist_length(feed->items);
}

/* Returns nth item from feed. */
FeedItem *feed_nth_item(Feed *feed, guint n)
{
	g_return_val_if_fail(feed != NULL, NULL);

	return g_slist_nth_data(feed->items, n);
}

/* feed_update()
 * Takes initialized feed with url set, fetches the feed from this url,
 * updates rest of Feed struct members and returns HTTP response code
 * we got from url's server. */
guint feed_update(Feed *feed, time_t last_update)
{
	CURL *eh = NULL;
	CURLcode res;
	FeedParserCtx *feed_ctx = NULL;
	glong response_code = 0;

	g_return_val_if_fail(feed != NULL, FEED_ERR_NOFEED);
	g_return_val_if_fail(feed->url != NULL, FEED_ERR_NOURL);

	/* Init curl before anything else. */
	eh = curl_easy_init();

	g_return_val_if_fail(eh != NULL, FEED_ERR_INIT);

	/* Curl initialized, create parser context now. */
	feed_ctx = g_malloc( sizeof(FeedParserCtx) );

	feed_ctx->parser = XML_ParserCreate(NULL);
	feed_ctx->depth = 0;
	feed_ctx->str = NULL;
	feed_ctx->xhtml_str = NULL;
	feed_ctx->feed = feed;
	feed_ctx->location = 0;
	feed_ctx->curitem = NULL;
	feed_ctx->id_is_permalink = TRUE;

	feed_ctx->name = NULL;
	feed_ctx->mail = NULL;

	/* Set initial expat handlers, which will take care of choosing
	 * correct parser later. */
	feed_parser_set_expat_handlers(feed_ctx);

	curl_easy_setopt(eh, CURLOPT_URL, feed->url);
	curl_easy_setopt(eh, CURLOPT_NOPROGRESS, 1);
#ifdef CURLOPT_MUTE
	curl_easy_setopt(eh, CURLOPT_MUTE, 1);
#endif
	curl_easy_setopt(eh, CURLOPT_WRITEFUNCTION, feed_writefunc);
	curl_easy_setopt(eh, CURLOPT_WRITEDATA, feed_ctx);
	curl_easy_setopt(eh, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(eh, CURLOPT_MAXREDIRS, 3);
	curl_easy_setopt(eh, CURLOPT_TIMEOUT, feed->timeout);
	curl_easy_setopt(eh, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(eh, CURLOPT_ENCODING, "");
	curl_easy_setopt(eh, CURLOPT_USERAGENT, "libfeed 0.1");
	curl_easy_setopt(eh, CURLOPT_NETRC, CURL_NETRC_OPTIONAL);

	/* Use HTTP's If-Modified-Since feature, if application provided
	 * the timestamp of last update. */
	if( last_update != -1 ) {
		curl_easy_setopt(eh, CURLOPT_TIMECONDITION,
				CURL_TIMECOND_IFMODSINCE);
		curl_easy_setopt(eh, CURLOPT_TIMEVALUE, (long)last_update);
	}

#if LIBCURL_VERSION_NUM >= 0x070a00
	if (feed->ssl_verify_peer == FALSE) {
		curl_easy_setopt(eh, CURLOPT_SSL_VERIFYPEER, 0);
		curl_easy_setopt(eh, CURLOPT_SSL_VERIFYHOST, 0);
	}
#endif

	if (feed->cacert_file != NULL)
		curl_easy_setopt(eh, CURLOPT_CAINFO, feed->cacert_file);

	if(feed->cookies_path != NULL)
		curl_easy_setopt(eh, CURLOPT_COOKIEFILE, feed->cookies_path);

	if (feed->auth != NULL) {
		switch (feed->auth->type) {
		case FEED_AUTH_NONE:
			break;
		case FEED_AUTH_BASIC:
			curl_easy_setopt(eh, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
			curl_easy_setopt(eh, CURLOPT_USERNAME,
					 feed->auth->username);
			curl_easy_setopt(eh, CURLOPT_PASSWORD,
					 feed->auth->password);
			break;
		default:
			response_code = FEED_ERR_UNAUTH; /* unknown auth */
			goto cleanup;
		}
	}

	res = curl_easy_perform(eh);
	XML_Parse(feed_ctx->parser, "", 0, TRUE);

	if( res != CURLE_OK ) {
		feed->fetcherr = g_strdup(curl_easy_strerror(res));
		response_code = FEED_ERR_FETCH;
	} else {
		curl_easy_getinfo(eh, CURLINFO_RESPONSE_CODE, &response_code);
	}

cleanup:
	curl_easy_cleanup(eh);

	/* Cleanup, we should be done. */
	XML_ParserFree(feed_ctx->parser);
	if (feed_ctx->name != NULL)
		g_free(feed_ctx->name);
	if (feed_ctx->mail != NULL)
		g_free(feed_ctx->mail);
	if (feed_ctx->str != NULL)
		g_string_free(feed_ctx->str, TRUE);
	if (feed_ctx->xhtml_str != NULL)
		g_string_free(feed_ctx->xhtml_str, TRUE);
	g_free(feed_ctx);

	return response_code;
}

void feed_foreach_item(Feed *feed, GFunc func, gpointer data)
{
	g_return_if_fail(feed != NULL);
	g_return_if_fail(feed->items != NULL);

	g_slist_foreach(feed->items, func, data);
}

gboolean feed_prepend_item(Feed *feed, FeedItem *item)
{
	g_return_val_if_fail(feed != NULL, FALSE);
	g_return_val_if_fail(item != NULL, FALSE);

	feed->items = g_slist_prepend(feed->items, item);
	return TRUE;
}

gboolean feed_append_item(Feed *feed, FeedItem *item)
{
	g_return_val_if_fail(feed != NULL, FALSE);
	g_return_val_if_fail(item != NULL, FALSE);

	feed->items = g_slist_append(feed->items, item);
	return TRUE;
}

gboolean feed_insert_item(Feed *feed, FeedItem *item, gint pos)
{
	g_return_val_if_fail(feed != NULL, FALSE);
	g_return_val_if_fail(item != NULL, FALSE);
	g_return_val_if_fail(pos < 0, FALSE);

	feed->items = g_slist_insert(feed->items, item, pos);
	return TRUE;
}

gchar *feed_get_cookies_path(Feed *feed)
{
	g_return_val_if_fail(feed != NULL, NULL);
	return feed->cookies_path;
}

void feed_set_cookies_path(Feed *feed, gchar *path)
{
	g_return_if_fail(feed != NULL);

	if( feed->cookies_path != NULL ) {
		g_free(feed->cookies_path);
		feed->cookies_path = NULL;
	}

	feed->cookies_path = (path != NULL ? g_strdup(path) : NULL);
}

gboolean feed_get_ssl_verify_peer(Feed *feed)
{
	g_return_val_if_fail(feed != NULL, FALSE);
	return feed->ssl_verify_peer;
}

void feed_set_ssl_verify_peer(Feed *feed, gboolean ssl_verify_peer)
{
	g_return_if_fail(feed != NULL);
	feed->ssl_verify_peer = ssl_verify_peer;
}

gchar *feed_get_cacert_file(Feed *feed)
{
	g_return_val_if_fail(feed != NULL, NULL);
	return feed->cacert_file;
}

void feed_set_cacert_file(Feed *feed, const gchar *path)
{
	g_return_if_fail(feed != NULL);

	if( feed->cacert_file != NULL ) {
		g_free(feed->cacert_file);
		feed->cacert_file = NULL;
	}

	feed->cacert_file = (path != NULL ? g_strdup(path) : NULL);
}
