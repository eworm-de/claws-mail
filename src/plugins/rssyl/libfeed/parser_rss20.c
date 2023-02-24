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

#include <glib.h>
#include <expat.h>
#include <string.h>

#include <procheader.h>

#include "feed.h"
#include "feeditem.h"
#include "feeditemenclosure.h"
#include "date.h"
#include "parser.h"
#include "common/utils.h"

void feed_parser_rss20_start(void *data, const gchar *el, const gchar **attr)
{
	FeedParserCtx *ctx = (FeedParserCtx *)data;
	FeedItemEnclosure *enclosure = NULL;
	gchar *url, *type, *size_s;
	gulong size = -1;

	/* ------------------- */
	if( ctx->depth == 2 ) {
		if( !strcmp(el, "item") ) {		/* Start of new item */

			if( ctx->curitem != NULL )
				feed_item_free(ctx->curitem);

			ctx->curitem = feed_item_new(ctx->feed);

		} else ctx->location = 0;
	/* ------------------- */
	} else if( ctx->depth == 3 ) {
		if( !strcmp(el, "enclosure") ) {	/* Media enclosure */

			url = feed_parser_get_attribute_value(attr, "url");
			type = feed_parser_get_attribute_value(attr, "type");
			size_s = feed_parser_get_attribute_value(attr, "length");
			if( size_s != NULL )
				size = (gulong)atol(size_s);

			if( url != NULL && type != NULL && size > 0 ) {
				if( (enclosure = feed_item_enclosure_new(url, type, size)) )
					feed_item_set_enclosure(ctx->curitem, enclosure);
			}

		} else if( !strcmp(el, "guid") ) { /* Unique ID */
			type = feed_parser_get_attribute_value(attr, "isPermaLink");
			if( type != NULL && !strcmp(type, "false") )
				feed_item_set_id_permalink(ctx->curitem, TRUE);
		}
	} else ctx->location = 0;

	ctx->depth++;

}

void feed_parser_rss20_end(void *data, const gchar *el)
{
	FeedParserCtx *ctx = (FeedParserCtx *)data;
	Feed *feed = ctx->feed;
	gchar *text = NULL;

	if( ctx->str != NULL )
		text = g_strstrip(g_strdup(ctx->str->str));
	else
		text = "";

	ctx->depth--;

	switch( ctx->depth ) {

	/* ------------------- */
		case 0:

			if( !strcmp(el, "rss") ) {
				/* we finished parsing the feed */
				ctx->feed->items = g_slist_reverse(ctx->feed->items);
			}

			break;

	/* ------------------- */
		case 1:

			break;	/* nothing to do at this depth */

	/* ------------------- */
		case 2:

			/* decide if we just received </item>, so we can
			 * add a complete item to feed */
			if( !strcmp(el, "item") ) {

				/* append the complete feed item, if it is valid
				 * "All elements of an item are optional, however at least one
				 * of title or description must be present." */
				if( ctx->curitem->title != NULL || ctx->curitem->summary != NULL ) {
					ctx->feed->items = 
						g_slist_prepend(ctx->feed->items, (gpointer)ctx->curitem);
				}
				
				/* since it's in the linked list, lose this pointer */
				ctx->curitem = NULL;

			} else if( !strcmp(el, "title") ) {	/* so it wasn't end of item */
				FILL(feed->title)
			} else if( !strcmp(el, "description" ) ) {
				FILL(feed->description)
			} else if( !strcmp(el, "dc:language") ) {
				FILL(feed->language)
			} else if( !strcmp(el, "author") ) {
				FILL(feed->author)
			} else if( !strcmp(el, "admin:generatorAgent") ) {
				FILL(feed->generator)
			} else if( !strcmp(el, "dc:date") ) {
				feed->date = procheader_date_parse(NULL, text, 0);
			} else if( !strcmp(el, "pubDate") ) {
				feed->date = procheader_date_parse(NULL, text, 0);
			}

			break;

	/* ------------------- */
		case 3:

			if( ctx->curitem == NULL ) {
				break;
			}

			/* decide which field did we just get */
			if( !strcmp(el, "title") ) {
				FILL(ctx->curitem->title)
			} else if( !strcmp(el, "author") ) {
				FILL(ctx->curitem->author)
			} else if( !strcmp(el, "description") ) { 
				FILL(ctx->curitem->summary)
			} else if( !strcmp(el, "content:encoded") ) {
				FILL(ctx->curitem->text)
			} else if( !strcmp(el, "link") ) {
				FILL(ctx->curitem->url)
			} else if( !strcmp(el, "guid") ) {
				FILL(ctx->curitem->id)
			} else if( !strcmp(el, "wfw:commentRSS") || !strcmp(el, "wfw:commentRss") ) {
				FILL(ctx->curitem->comments_url)
			} else if( !strcmp(el, "dc:date") ) {
				ctx->curitem->date_modified = procheader_date_parse(NULL, text, 0);
			} else if( !strcmp(el, "pubDate") ) {
				ctx->curitem->date_published = procheader_date_parse(NULL, text, 0);
			} else if( !strcmp(el, "dc:creator")) {
				FILL(ctx->curitem->author)
			}

			break;

	}

	if( ctx->str != NULL ) {
		g_free(text);
		g_string_free(ctx->str, TRUE);
		ctx->str = NULL;
	}
}
