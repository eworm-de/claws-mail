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
#include <sys/stat.h>
#include <glib.h>
#include <pthread.h>

/* Claws Mail includes */
#include <common/claws.h>
#include <procheader.h>
#include <common/utils.h>
#include <main.h>

/* Local includes */
#include "libfeed/feed.h"
#include "libfeed/feeditem.h"
#include "libfeed/date.h"
#include "parse822.h"
#include "rssyl_feed.h"
#include "rssyl_parse_feed.h"
#include "strutils.h"

/* rssyl_parse_folder_item_file()
 *
 * Parse a RFC822-formatted feed item given by "path", and returns a
 * pointer to a newly-allocated FeedItem struct, which contains all required data.
 *
 */
FeedItem *rssyl_parse_folder_item_file(gchar *path)
{
	gchar *contents, **lines, **line, **splid, *tmp, *tmp2;
	GError *error = NULL;
	FeedItem *item;
	RFeedCtx *ctx;
	gint i = 0;
	GString *body = NULL;
	gboolean parsing_headers = TRUE, past_html_tag = FALSE, past_endhtml_tag = FALSE;
	gboolean started_author = FALSE, started_subject = FALSE;
	gboolean started_link = FALSE, started_clink = FALSE, got_original_title = FALSE;

	debug_print("RSSyl: parsing '%s'\n", path);

	if( !g_file_get_contents(path, &contents, NULL, &error) ) {
		g_warning("error: '%s'", error->message);
		g_error_free(error);
	}

	if( contents != NULL ) {
		lines = strsplit_no_copy(contents, '\n');
	} else {
		g_warning("badly formatted file found, ignoring: '%s'", path);
		return NULL;
	}

	ctx = g_new0(RFeedCtx, 1);
	ctx->path = g_strdup(path); /* store filesystem path to source file */
	ctx->last_seen = 0;

	item = feed_item_new(NULL);
	item->data = ctx;

	while( lines[i] ) {
		if( parsing_headers && lines[i] && !strlen(lines[i]) ) {
			parsing_headers = FALSE;
			debug_print("RSSyl: finished parsing headers\n");
		}

		if( parsing_headers ) {
			line = g_strsplit(lines[i], ": ", 2);
			if( line[0] && line[1] && strlen(line[0]) && lines[i][0] != ' ') {
				started_author = FALSE;
				started_subject = FALSE;
				started_link = FALSE;
				started_clink = FALSE;

				/* Author */
				if( !strcmp(line[0], "From") ) {
					feed_item_set_author(item, line[1]);
					debug_print("RSSyl: got author '%s'\n", feed_item_get_author(item));
					started_author = TRUE;
				}

				/* Date (set both FeedItem timestamps) */
				if( !strcmp(line[0], "Date") ) {
					feed_item_set_date_modified(item,
							procheader_date_parse(NULL, line[1], 0));
					feed_item_set_date_published(item,
							feed_item_get_date_modified(item));
					debug_print("RSSyl: got date \n" );
				}

				/* Title */
				if( !strcmp(line[0], "Subject") && !got_original_title ) {
					feed_item_set_title(item,line[1]);
					debug_print("RSSyl: got title '%s'\n", feed_item_get_title(item));
					started_subject = TRUE;
				}

				/* Original (including HTML) title - Atom feeds */
				if( !strcmp(line[0], "X-RSSyl-OrigTitle") ) {
					feed_item_set_title(item, line[1]);
					debug_print("RSSyl: got original title '%s'\n",
							feed_item_get_title(item));
					got_original_title = TRUE;
				}

				/* URL */
				if( !strcmp(line[0], "X-RSSyl-URL") ) {
					feed_item_set_url(item, line[1]);
					debug_print("RSSyl: got link '%s'\n", feed_item_get_url(item));
					started_link = TRUE;
				}

				/* Last-Seen timestamp */
				if( !strcmp(line[0], "X-RSSyl-Last-Seen") ) {
					ctx->last_seen = atol(line[1]);
					debug_print("RSSyl: got last_seen timestamp %"CM_TIME_FORMAT"\n", ctx->last_seen);
				}

				/* ID */
				if( !strcmp(line[0], "Message-ID") ) {
					if (line[1][0] != '<' || line[1][strlen(line[1])-1] != '>') {
						debug_print("RSSyl: malformed Message-ID, ignoring...\n");
					} else {
						/* Get the ID from within < and >. */
						tmp = line[1] + 1;
						tmp2 = g_strndup(tmp, strlen(tmp) - 1);
						feed_item_set_id(item, tmp2);
						g_free(tmp2);
					}
				}

				/* Feed comments */
				if( !strcmp(line[0], "X-RSSyl-Comments") ) {
					feed_item_set_comments_url(item, line[1]);
					debug_print("RSSyl: got clink '%s'\n", feed_item_get_comments_url(item));
					started_clink = TRUE;
				}

				/* References */
				if( !strcmp(line[0], "References") ) {
					splid = g_strsplit_set(line[1], "<>", 3);
					if( strlen(splid[1]) != 0 )
						feed_item_set_parent_id(item, line[1]);
					g_strfreev(splid);
				}

			} else if (lines[i][0] == ' ') {
				gchar *tmp = NULL;
				/* continuation line */
				if (started_author) {
					tmp = g_strdup_printf("%s %s", feed_item_get_author(item), lines[i]+1);
					feed_item_set_author(item, tmp);
					debug_print("RSSyl: updated author to '%s'\n", tmp);
					g_free(tmp);
				} else if (started_subject) {
					tmp = g_strdup_printf("%s %s", feed_item_get_title(item), lines[i]+1);
					feed_item_set_title(item, tmp);
					debug_print("RSSyl: updated title to '%s'\n", tmp);
					g_free(tmp);
				} else if (started_link) {
					tmp = g_strdup_printf("%s%s", feed_item_get_url(item), lines[i]+1);
					feed_item_set_url(item, tmp);
					debug_print("RSSyl: updated link to '%s'\n", tmp);
					g_free(tmp);
				} else if (started_clink) {
					tmp = g_strdup_printf("%s%s", feed_item_get_comments_url(item), lines[i]+1);
					feed_item_set_comments_url(item, tmp);
					debug_print("RSSyl: updated comments_link to '%s'\n", tmp);
				}
			}
			g_strfreev(line);
		} else {
			if( !strcmp(lines[i], RSSYL_TEXT_START) ) {
				debug_print("RSSyl: Leading html tag found at line %d\n", i);
				past_html_tag = TRUE;
				if (body)
				{
					g_warning("unexpected leading html tag found at line %d", i);
				    g_string_free(body, TRUE);
				}
				body = g_string_new("");
				i++;
				continue;
			}
			while( past_html_tag && !past_endhtml_tag && lines[i] ) {
				if( !strcmp(lines[i], RSSYL_TEXT_END) ) {
					debug_print("RSSyl: Trailing html tag found at line %d\n", i);
					past_endhtml_tag = TRUE;
					continue;
				}

				if (body->len > 0)
					body = g_string_append_c(body, '\n');
				body = g_string_append(body, lines[i]);

				i++;
			}

		}

		i++;
	}

	if (body != NULL ) {
		if (past_html_tag && past_endhtml_tag && body->str != NULL)
			feed_item_set_text(item, body->str);
		g_string_free(body, TRUE);
	}

	g_free(lines);
	g_free(contents);
	return item;
}

static void rssyl_flush_folder_func(gpointer data, gpointer user_data)
{
	FeedItem *item = (FeedItem *)data;
	RFeedCtx *ctx = (RFeedCtx *)item->data;

	if( ctx != NULL && ctx->path != NULL) {
		g_free(ctx->path);
	}
	feed_item_free(item);
}

static void rssyl_folder_read_existing_real(RFolderItem *ritem)
{
	gchar *path = NULL, *fname = NULL;
	GDir *dp;
	const gchar *d;
	GError *error = NULL;
	gint num;
	FeedItem *item = NULL;
	RFeedCtx *ctx;

	g_return_if_fail(ritem != NULL);

	path = folder_item_get_path(&ritem->item);
	g_return_if_fail(path != NULL);

	debug_print("RSSyl: reading existing items from '%s'\n", path);

	/* Flush contents if any, so we can add new */
	if( g_slist_length(ritem->items) > 0 ) {
		g_slist_foreach(ritem->items, (GFunc)rssyl_flush_folder_func, NULL);
		g_slist_free(ritem->items);
	}
	ritem->items = NULL;
	ritem->last_update = 0;

	if( (dp = g_dir_open(path, 0, &error)) == NULL ) {
		debug_print("g_dir_open on \"%s\" failed with error %d (%s)\n",
				path, error->code, error->message);
		g_error_free(error);
		g_free(path);
		return;
	}

	while( (d = g_dir_read_name(dp)) != NULL ) {
		if( claws_is_exiting() ) {
			g_dir_close(dp);
			g_free(path);
			return;
		}

		if( d[0] != '.' && (num = to_number(d)) > 0 ) {
			fname = g_strdup_printf("%s%c%s", path, G_DIR_SEPARATOR, d);
			if (!g_file_test(fname, G_FILE_TEST_IS_REGULAR)) {
				debug_print("RSSyl: not a regular file: '%s', ignoring it\n", fname);
				g_free(fname);
				continue;
			}

			debug_print("RSSyl: starting to parse '%s'\n", d);
			if( (item = rssyl_parse_folder_item_file(fname)) != NULL ) {
				/* Find latest timestamp */
				ctx = (RFeedCtx *)item->data;
				if( ritem->last_update < ctx->last_seen )
					ritem->last_update = ctx->last_seen;
				debug_print("RSSyl: Appending '%s'\n", feed_item_get_title(item));
				ritem->items = g_slist_prepend(ritem->items, item);
			}
			g_free(fname);
		}
	}

	g_dir_close(dp);
	g_free(path);

	ritem->items = g_slist_reverse(ritem->items);
}

#ifdef USE_PTHREAD
static void *rssyl_read_existing_thr(void *arg)
{
	RParseCtx *ctx = (RParseCtx *)arg;

	rssyl_folder_read_existing_real(ctx->ritem);
	ctx->ready = TRUE;
	return NULL;
}
#endif

void rssyl_folder_read_existing(RFolderItem *ritem)
{
#ifdef USE_PTHREAD
	RParseCtx *ctx;
	pthread_t pt;
#endif

	g_return_if_fail(ritem != NULL);


#ifdef USE_PTHREAD
	ctx = g_new0(RParseCtx, 1);
	ctx->ritem = ritem;
	ctx->ready = FALSE;

	if( pthread_create(&pt, NULL, rssyl_read_existing_thr,
				(void *)ctx) != 0 ) {
		/* Couldn't create thread, let's continue non-threaded. */
		rssyl_folder_read_existing_real(ritem);
	} else {
		/* Thread started, wait until it is done. */
		debug_print("RSSyl: waiting for thread to finish\n");
		while( !ctx->ready ) {
			claws_do_idle();
		}

		debug_print("RSSyl: thread finished\n");
		pthread_join(pt, NULL);
	}

	g_free(ctx);
#else
	rssyl_folder_read_existing_real(ritem);
#endif
}
