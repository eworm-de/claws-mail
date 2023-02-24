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

#ifndef __FEED_H
#define __FEED_H

#include <glib.h>
#include <expat.h>

#define FEED_DEFAULT_TIMEOUT	20	/* In seconds */

/* ---------------- Structures */

typedef struct _Feed Feed;
typedef struct _FeedItem FeedItem;
typedef struct _FeedParserCtx FeedParserCtx;
typedef struct _FeedAuth FeedAuth;

typedef enum {
	FEED_AUTH_NONE,
	FEED_AUTH_BASIC
} FeedAuthType;

struct _FeedAuth {
	FeedAuthType type;
	gchar *username;
	gchar *password;
};
	
struct _Feed {
	gchar *url;
	FeedAuth *auth;
	gboolean is_valid;
	gchar *title;
	gchar *description;
	gchar *language;
	gchar *author;
	gchar *generator;
	gchar *link;
	time_t date;

	guint timeout;
	gchar *fetcherr;
	gchar *cookies_path;
	gboolean ssl_verify_peer;
	gchar *cacert_file;

	GSList *items;
};

struct _FeedParserCtx {
	XML_Parser parser;
	guint depth;
	guint location;
	GString *str;
	GString *xhtml_str;
	gchar *name;
	gchar *mail;
	gboolean id_is_permalink;

	Feed *feed;
	FeedItem *curitem;
};

typedef enum {
	FEED_ERR_NOFEED,
	FEED_ERR_NOURL,
	FEED_ERR_INIT,
	FEED_ERR_FETCH,
	FEED_ERR_UNAUTH
} FeedErrCodes;

/* ---------------- Prototypes */

Feed *feed_new(gchar *url);
void feed_free(Feed *feed);

void feed_free_items(Feed *feed);

void feed_set_timeout(Feed *feed, guint timeout);
guint feed_get_timeout(Feed *feed);

void feed_set_url(Feed *feed, gchar *url);
gchar *feed_get_url(Feed *feed);

void feed_set_auth(Feed *feed, FeedAuth *auth);
FeedAuth *feed_get_auth(Feed *feed);

gchar *feed_get_title(Feed *feed);
void feed_set_title(Feed *feed, gchar *new_title);

gchar *feed_get_description(Feed *feed);
gchar *feed_get_language(Feed *feed);
gchar *feed_get_author(Feed *feed);
gchar *feed_get_generator(Feed *feed);
gchar *feed_get_link(Feed *feed);
gchar *feed_get_fetcherror(Feed *feed);

gchar *feed_get_cookies_path(Feed *feed);
void feed_set_cookies_path(Feed *feed, gchar *path);

gboolean feed_get_ssl_verify_peer(Feed *feed);
void feed_set_ssl_verify_peer(Feed *feed, gboolean ssl_verify_peer);

gchar *feed_get_cacert_file(Feed *feed);
void feed_set_cacert_file(Feed *feed, const gchar *path);

gint feed_n_items(Feed *feed);
FeedItem *feed_nth_item(Feed *feed, guint n);

void feed_foreach_item(Feed *feed, GFunc func, gpointer data);

gboolean feed_prepend_item(Feed *feed, FeedItem *item);
gboolean feed_append_item(Feed *feed, FeedItem *item);
gboolean feed_insert_item(Feed *feed, FeedItem *item, gint pos);

guint feed_update(Feed *feed, time_t last_update);

#define FILL(n)		do { g_free(n); n = g_strdup(text); } while(0);

#include "feeditem.h"

#endif /* __FEED_H */
