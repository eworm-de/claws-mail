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
#include <stdio.h>

#include <procheader.h>

#include "feed.h"
#include "feeditem.h"
#include "date.h"
#include "parser.h"
#include "parser_atom10.h"

enum {
	FEED_LOC_ATOM10_NONE,
	FEED_LOC_ATOM10_ENTRY,
	FEED_LOC_ATOM10_AUTHOR,
	FEED_LOC_ATOM10_SOURCE,
	FEED_LOC_ATOM10_CONTENT
} FeedAtom10Locations;

void feed_parser_atom10_start(void *data, const gchar *el, const gchar **attr)
{
	FeedParserCtx *ctx = (FeedParserCtx *)data;
	gchar *a = NULL;

	if( ctx->depth == 1 ) {

		if( !strcmp(el, "entry") ) {
			/* Start of new feed item found.
			 * Create a new FeedItem, freeing the one we already have, if any. */
			if( ctx->curitem != NULL )
				feed_item_free(ctx->curitem);
			ctx->curitem = feed_item_new(ctx->feed);
			ctx->location = FEED_LOC_ATOM10_ENTRY;
		} else if( !strcmp(el, "author") ) {
			/* Start of author info for the feed found.
			 * Set correct location. */
			ctx->location = FEED_LOC_ATOM10_AUTHOR;
		} else if( !strcmp(el, "link") ) {
			if (!feed_parser_get_attribute_value(attr, "rel")) {
				/* Link tag for the feed */
				g_free(ctx->feed->link);
				ctx->feed->link =
					g_strdup(feed_parser_get_attribute_value(attr, "href"));
			}
		} else ctx->location = FEED_LOC_ATOM10_NONE;

	} else if( ctx->depth == 2 ) {

		/* Make sure we are in one of known locations within the XML structure.
		 * This condition should never be true on a valid Atom feed. */
		if (ctx->location != FEED_LOC_ATOM10_AUTHOR &&
				ctx->location != FEED_LOC_ATOM10_ENTRY) {
			ctx->depth++;
			return;
		}

		if( !strcmp(el, "author") ) {
			/* Start of author info for current feed item.
			 * Set correct location. */
			ctx->location = FEED_LOC_ATOM10_AUTHOR;
		} else if( !strcmp(el, "link") ) {
			/* Capture item URL, from the "url" XML attribute. */
			if (ctx->curitem && ctx->location == FEED_LOC_ATOM10_ENTRY)
				ctx->curitem->url = g_strdup(feed_parser_get_attribute_value(attr, "href"));
		} else if( !strcmp(el, "source") ) {
			ctx->location = FEED_LOC_ATOM10_SOURCE;
		} else ctx->location = FEED_LOC_ATOM10_ENTRY;

		if( !strcmp(el, "title") && ctx->curitem != NULL) {
			a = feed_parser_get_attribute_value(attr, "type");
			if( !a || !strcmp(a, "text") )
				ctx->curitem->title_format = FEED_ITEM_TITLE_TEXT;
			else if( !strcmp(a, "html") )
				ctx->curitem->title_format = FEED_ITEM_TITLE_HTML;
			else if( !strcmp(a, "xhtml") )
				ctx->curitem->title_format = FEED_ITEM_TITLE_XHTML;
			else
				ctx->curitem->title_format = FEED_ITEM_TITLE_UNKNOWN;
		} else if (!strcmp(el, "content") && ctx->curitem != NULL) {
			ctx->location = FEED_LOC_ATOM10_CONTENT;
			a = feed_parser_get_attribute_value(attr, "type");
			if (a && !strcmp(a, "xhtml")) {
				ctx->curitem->xhtml_content = TRUE;
				ctx->xhtml_str = g_string_new(NULL);
			}
		}
	} else if (ctx->depth >= 3) {
		if (ctx->location == FEED_LOC_ATOM10_CONTENT
				&& ctx->curitem != NULL
				&& ctx->curitem->xhtml_content) {
			guint i;
			GString *txt = ctx->xhtml_str;
			g_string_append_c(txt, '<');
			g_string_append(txt, el);

			for (i = 0; attr[i] != NULL && attr[i+1] != NULL; i += 2) {
				g_string_append_printf(txt, " %s='%s'", attr[i], attr[i+1]);
			}
			g_string_append_c(txt, '>');
		}
	}


	ctx->depth++;
}

void feed_parser_atom10_end(void *data, const gchar *el)
{
	FeedParserCtx *ctx = (FeedParserCtx *)data;
	Feed *feed = ctx->feed;
	gchar *text = NULL, *tmp;

	if( ctx->str != NULL )
		text = g_strstrip(g_strdup(ctx->str->str));
	else
		text = "";

	switch( ctx->depth ) {

		case 0:
			/* Just in case. */
			break;

		case 1:

			if( !strcmp(el, "feed") ) {
				/* We have finished parsing the feed, reverse the list
				 * so it's not upside down. */
				feed->items = g_slist_reverse(ctx->feed->items);
			}

			break;

		case 2:

			/* decide if we just received </entry>, so we can
			 * add a complete item to feed */
			if( !strcmp(el, "entry") ) {

				/* Fix up URL, if it is relative */
				if (ctx->curitem->url != NULL &&
						!strstr(ctx->curitem->url, "://") &&
						ctx->feed->link != NULL) {
					tmp = g_strconcat(ctx->feed->link,
							(ctx->curitem->url[0] == '/' ? "" : "/"),
							ctx->curitem->url, NULL);
					feed_item_set_url(ctx->curitem, tmp);
					g_free(tmp);
				}

				/* append the complete feed item */
				if( ctx->curitem->id && ctx->curitem->title
						&& ctx->curitem->date_modified ) {
					feed->items = 
						g_slist_prepend(feed->items, (gpointer)ctx->curitem);
				}
				
				/* since it's in the linked list, lose this pointer */
				ctx->curitem = NULL;

			} else if( !strcmp(el, "title") ) {	/* so it wasn't end of item */
				FILL(feed->title)
			} else if( !strcmp(el, "summary" ) ) {
				FILL(feed->description)
			} else if( !strcmp(el, "updated" ) ) {
				feed->date = procheader_date_parse(NULL, text, 0);
			}
			/* FIXME: add more later */

			break;

		case 3:

			if( ctx->curitem == NULL )
				break;

			switch(ctx->location) {

				/* We're in feed/entry */
				case FEED_LOC_ATOM10_ENTRY:
					if( !strcmp(el, "title") ) {
						FILL(ctx->curitem->title)
					} else if( !strcmp(el, "summary") ) {
						FILL(ctx->curitem->summary)
					} else if( !strcmp(el, "id") ) {
						FILL(ctx->curitem->id)
						feed_item_set_id_permalink(ctx->curitem, TRUE);
					} else if( !strcmp(el, "published") ) {
						ctx->curitem->date_published = procheader_date_parse(NULL, text, 0);
					} else if( !strcmp(el, "updated") ) {
						ctx->curitem->date_modified = procheader_date_parse(NULL, text, 0);
					}

					break;

				/* We're in feed/author or about to leave feed/entry/author */
				case FEED_LOC_ATOM10_AUTHOR:
					if( !strcmp(el, "author" ) ) {
						/* We just finished parsing <author> */
						ctx->curitem->author = g_strdup_printf("%s%s%s%s%s",
								ctx->name ? ctx->name : "",
								ctx->name && ctx->mail ? " <" : ctx->mail ? "<" : "",
								ctx->mail ? ctx->mail : "",
								ctx->mail ? ">" : "",
								!ctx->name && !ctx->mail ? "N/A" : "");
						ctx->location = FEED_LOC_ATOM10_ENTRY;
					} else if( !strcmp(el, "name") ) {
						FILL(feed->author)
					}

					break;

				case FEED_LOC_ATOM10_CONTENT:
					if( !strcmp(el, "content") ) {
						if (ctx->curitem->xhtml_content) {
							/* Just in case the <content> tag itself also has some
							 * content of its own, not just the <div> it should,
							 * let's append it to the end. */
							g_string_append(ctx->xhtml_str, text);
							ctx->curitem->text = g_string_free(ctx->xhtml_str, FALSE);
							ctx->xhtml_str = NULL;
						} else {
							FILL(ctx->curitem->text)
						}
						ctx->location = FEED_LOC_ATOM10_ENTRY;
					}

					break;
			}
			break;

		case 4:

			if( ctx->curitem == NULL )
				break;

			switch(ctx->location) {

				/* We're in feed/entry/author */
				case FEED_LOC_ATOM10_AUTHOR:
					if( !strcmp(el, "name") ) {
						FILL(ctx->name)
					} else if( !strcmp(el, "email") ) {
						FILL(ctx->mail)
					}

					break;

				/* We're in feed/entry/source */
				case FEED_LOC_ATOM10_SOURCE:
					if( !strcmp(el, "title" ) ) {
						FILL(ctx->curitem->sourcetitle)
					} else if( !strcmp(el, "id" ) ) {
						FILL(ctx->curitem->sourceid)
					} else if( !strcmp(el, "updated" ) ) {
						ctx->curitem->sourcedate = procheader_date_parse(NULL, text, 0);
					}

					break;

				case FEED_LOC_ATOM10_CONTENT:
					if (ctx->curitem->xhtml_content) {
						g_string_append(ctx->xhtml_str, text);
						g_string_append_printf(ctx->xhtml_str, "</%s>", el);
					}
					break;

				}


			break;

		default:
			if (ctx->location == FEED_LOC_ATOM10_CONTENT
					&& ctx->curitem->xhtml_content) {
				g_string_append(ctx->xhtml_str, text);
				g_string_append_printf(ctx->xhtml_str, "</%s>", el);
			}
			break;
	}

	if( ctx->str != NULL ) {
		g_free(text);
		g_string_free(ctx->str, TRUE);
		ctx->str = NULL;
	}
	ctx->str = NULL;

	ctx->depth--;
}
