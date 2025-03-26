/*
 * Copyright (C) 2012 Andrej Kacian <andrej@kacian.sk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* Expat parser for old feeds.xml */

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <expat.h>

#include <alertpanel.h>
#include <common/utils.h>

#include "libfeed/parser.h"
#include "old_feeds.h"
#include "rssyl.h"

struct _oldrssyl_ctx {
	GSList *oldfeeds;
};

static void _elparse_start_oldrssyl(void *data, const gchar *el,
		const gchar **attr)
{
	struct _oldrssyl_ctx *ctx = data;
	OldRFeed *of;
	gchar *tmp;

#define GETVAL_STR(name) (g_strdup(feed_parser_get_attribute_value(attr, name)))
#define GETVAL_INT(name) \
	((tmp = feed_parser_get_attribute_value(attr, name)) == NULL ? 0 : \
		(gint)atoi(tmp))

	if (!strcmp(el, "feed")) {
		of = g_new0(OldRFeed, 1);

		of->name = GETVAL_STR("name");
		of->official_name = GETVAL_STR("official_name");
		of->url = GETVAL_STR("url");
		of->default_refresh_interval = GETVAL_INT("default_refresh_interval");
		of->refresh_interval = GETVAL_INT("refresh_interval");
		of->expired_num = GETVAL_INT("expired_num");
		of->fetch_comments = GETVAL_INT("fetch_comments");
		of->fetch_comments_for = GETVAL_INT("fetch_comments_for");
		of->silent_update = GETVAL_INT("silent_update");
		of->ssl_verify_peer = GETVAL_INT("ssl_verify_peer");

		debug_print("RSSyl: old feeds.xml: Adding '%s' (%s).\n", of->name,
				of->url);

		ctx->oldfeeds = g_slist_prepend(ctx->oldfeeds, of);
	}

	return;
}

static void _elparse_end_oldrssyl(void *data, const gchar *el)
{
	return;
}

GSList *rssyl_old_feed_metadata_parse(gchar *filepath)
{
	XML_Parser parser;
	GSList *oldfeeds = NULL;
	gchar *contents = NULL;
	gsize length;
	GError *error = NULL;
	struct _oldrssyl_ctx *ctx;

	debug_print("RSSyl: Starting to parse old feeds.xml\n");

	/* Read contents of the file into memory */
	if (!g_file_get_contents(filepath, &contents, &length, &error)) {
		alertpanel_error(_("Couldn't read contents of old feeds.xml file:\n%s"),
				error->message);
		debug_print("RSSyl: Couldn't read contents of feeds.xml\n");
		g_error_free(error);
		return NULL;
	}

	/* Set up expat parser */
	parser = XML_ParserCreate(NULL);

	ctx = g_new0(struct _oldrssyl_ctx, 1);
	ctx->oldfeeds = NULL;
	XML_SetUserData(parser, ctx);
	XML_SetElementHandler(parser,
			_elparse_start_oldrssyl,
			_elparse_end_oldrssyl);

	/* Parse the XML, our output ending up in oldfeeds */
	XML_Parse(parser, contents, length, 1);

	/* And clean up */
	XML_ParserFree(parser);
	g_free(contents);
	oldfeeds = ctx->oldfeeds;
	g_free(ctx);

	debug_print("RSSyl: old feeds.xml: added %d items in total\n",
			g_slist_length(oldfeeds));

	return oldfeeds;
}

static void _free_old_feed_entry(gpointer d, gpointer user_data)
{
	OldRFeed *of = (OldRFeed *)d;

	if (of == NULL)
		return;

	g_free(of->name);
	g_free(of->official_name);
	g_free(of->url);
	g_free(of);
}

void rssyl_old_feed_metadata_free(GSList *oldfeeds)
{
	if (oldfeeds != NULL) {
		debug_print("RSSyl: releasing parsed contents of old feeds.xml\n");
		g_slist_foreach(oldfeeds, _free_old_feed_entry, NULL);
		g_slist_free(oldfeeds);
		oldfeeds = NULL;
	}
}

static gint _old_feed_find_by_url(gconstpointer a, gconstpointer b)
{
	OldRFeed *of = (OldRFeed *)a;
	gchar *name = (gchar *)b;

	if (of == NULL || of->name == NULL || of->url == NULL || name == NULL)
		return 1;

	return strcmp(of->name, name);
}

OldRFeed *rssyl_old_feed_get_by_name(GSList *oldfeeds, gchar *name)
{
	GSList *needle;

	g_return_val_if_fail(oldfeeds != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);

	if ((needle = g_slist_find_custom(oldfeeds, name, _old_feed_find_by_url))
			!= NULL)
		return (OldRFeed *)needle->data;
	
	return NULL;
}
