/*
 * Copyright (C) 2006 Andrej Kacian <andrej@kacian.sk>
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

#include "config.h"

#include <glib.h>
#include <expat.h>
#include <string.h>

#include <procheader.h>

#include "feed.h"
#include "date.h"
#include "parser_rdf.h"

enum {
	FEED_LOC_RDF_NONE,
	FEED_LOC_RDF_CHANNEL,
	FEED_LOC_RDF_ITEM
} FeedRdfLocations;

void feed_parser_rdf_start(void *data, const gchar *el, const gchar **attr)
{
	FeedParserCtx *ctx = (FeedParserCtx *)data;

	if( ctx->depth == 1 ) {
		if( !strcmp(el, "channel") ) {
			ctx->location = FEED_LOC_RDF_CHANNEL;
		} else if( !strcmp(el, "item") ) {

			if( ctx->curitem != NULL )
				feed_item_free(ctx->curitem);

			ctx->curitem = feed_item_new(ctx->feed);
			ctx->location = FEED_LOC_RDF_ITEM;

		} else ctx->location = 0;
	}

	ctx->depth++;

}

void feed_parser_rdf_end(void *data, const gchar *el)
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

		case 0:

			if( !strcmp(el, "rdf") ) {
				/* we finished parsing the feed */
				ctx->feed->items = g_slist_reverse(ctx->feed->items);
			}

			break;

		case 1:

			/* <item></item> block just ended, so ... */
			if( !strcmp(el, "item") ) {

				/* add the complete feed item to our feed struct */
				ctx->feed->items = 
					g_slist_prepend(ctx->feed->items, (gpointer)ctx->curitem);
				
				/* since it's in the linked list, lose this pointer */
				ctx->curitem = NULL;
			}

			break;

		case 2:

			switch(ctx->location) {

				/* We're inside introductory <channel></channel> */
				case FEED_LOC_RDF_CHANNEL:
					if( !strcmp(el, "title") ) {
						FILL(feed->title)
					} else if( !strcmp(el, "description" ) ) {
						FILL(feed->description)
					} else if( !strcmp(el, "dc:language") ) {
						FILL(feed->language)
					} else if( !strcmp(el, "dc:creator") ) {
						FILL(feed->author)
					} else if( !strcmp(el, "dc:date") ) {
						feed->date = procheader_date_parse(NULL, text, 0);
					} else if( !strcmp(el, "pubDate") ) {
						feed->date = procheader_date_parse(NULL, text, 0);
					}

					break;

				/* We're inside an <item></item> */
				case FEED_LOC_RDF_ITEM:
					if( ctx->curitem == NULL ) {
						break;
					}

					/* decide which field did we just get */
					if( !strcmp(el, "title") ) {
						FILL(ctx->curitem->title)
					} else if( !strcmp(el, "dc:creator") ) {
						FILL(ctx->curitem->author)
					} else if( !strcmp(el, "description") ) {
						FILL(ctx->curitem->summary)
					} else if( !strcmp(el, "content:encoded") ) {
						FILL(ctx->curitem->text)
					} else if( !strcmp(el, "link") ) {
						FILL(ctx->curitem->url)
					} else if( !strcmp(el, "dc:date") ) {
						ctx->curitem->date_modified = procheader_date_parse(NULL, text, 0);
					} else if( !strcmp(el, "pubDate") ) {
						ctx->curitem->date_modified = procheader_date_parse(NULL, text, 0);
					}

					break;
			}

			break;

	}

	if( ctx->str != NULL ) {
		g_free(text);
		g_string_free(ctx->str, TRUE);
		ctx->str = NULL;
	}
}
