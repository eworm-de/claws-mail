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
#include "feeditem.h"
#include "feeditemenclosure.h"
#include "parser.h"

/* feed_item_new()
 * Initializes a new empty FeedItem struct, setting its parent feed,
 * if supplied. */
FeedItem *feed_item_new(Feed *feed)
{
	FeedItem *item = NULL;

	item = g_malloc( sizeof(FeedItem) );
	item->url = NULL;
	item->title = NULL;
	item->title_format = 0;
	item->summary = NULL;
	item->text = NULL;
	item->author = NULL;
	item->id = NULL;
	item->comments_url = NULL;
	item->parent_id = NULL;
	item->enclosure = NULL;

	item->sourcetitle = NULL;
	item->sourceid = NULL;
	item->sourcedate = -1;

	item->id_is_permalink = FALSE;
	item->xhtml_content = FALSE;

	item->date_published = -1;
	item->date_modified = -1;
	
	if( feed != NULL )
		item->feed = feed;

	item->data = NULL;

	return item;
}

void feed_item_free(FeedItem *item)
{
	if( item == NULL )
		return;

	g_free(item->url);
	g_free(item->title);
	g_free(item->summary);
	g_free(item->text);
	g_free(item->author);
	g_free(item->id);
	g_free(item->data);
	g_free(item->comments_url);
	g_free(item->parent_id);
	feed_item_enclosure_free(item->enclosure);

	g_free(item->sourcetitle);
	g_free(item->sourceid);

	g_free(item);
	item = NULL;
}

Feed *feed_item_get_feed(FeedItem *item)
{
	g_return_val_if_fail(item != NULL, NULL);
	return item->feed;
}

/* URL */
gchar *feed_item_get_url(FeedItem *item)
{
	g_return_val_if_fail(item != NULL, NULL);
	return item->url;
}

void feed_item_set_url(FeedItem *item, const gchar *url)
{
	g_return_if_fail(item != NULL);
	g_return_if_fail(url != NULL);

	g_free(item->url);
	item->url = g_strdup(url);
}

/* Title */
gchar *feed_item_get_title(FeedItem *item)
{
	g_return_val_if_fail(item != NULL, NULL);
	return item->title;
}

void feed_item_set_title(FeedItem *item, const gchar *title)
{
	g_return_if_fail(item != NULL);
	g_return_if_fail(title != NULL);

	g_free(item->title);
	item->title = g_strdup(title);
}

/* Title format */
gint feed_item_get_title_format(FeedItem *item)
{
	g_return_val_if_fail(item != NULL, -1);
	return item->title_format;
}

void feed_item_set_title_format(FeedItem *item, gint format)
{
	g_return_if_fail(item != NULL);
	g_return_if_fail(format >= 0 && format <= FEED_ITEM_TITLE_UNKNOWN);

	item->title_format = format;
}

/* Summary */
gchar *feed_item_get_summary(FeedItem *item)
{
	g_return_val_if_fail(item != NULL, NULL);
	return item->summary;
}

void feed_item_set_summary(FeedItem *item, const gchar *summary)
{
	g_return_if_fail(item != NULL);
	g_return_if_fail(summary != NULL);

	g_free(item->summary);
	item->summary = g_strdup(summary);
}

/* Text */
gchar *feed_item_get_text(FeedItem *item)
{
	g_return_val_if_fail(item != NULL, NULL);
	return item->text;
}

void feed_item_set_text(FeedItem *item, const gchar *text)
{
	g_return_if_fail(item != NULL);
	g_return_if_fail(text != NULL);

	g_free(item->text);
	item->text = g_strdup(text);
}

/* Author */
gchar *feed_item_get_author(FeedItem *item)
{
	g_return_val_if_fail(item != NULL, NULL);
	return item->author;
}

void feed_item_set_author(FeedItem *item, const gchar *author)
{
	g_return_if_fail(item != NULL);
	g_return_if_fail(author != NULL);

	g_free(item->author);
	item->author = g_strdup(author);
}

/* ID */
gchar *feed_item_get_id(FeedItem *item)
{
	g_return_val_if_fail(item != NULL, NULL);
	return item->id;
}

void feed_item_set_id(FeedItem *item, const gchar *id)
{
	g_return_if_fail(item != NULL);
	g_return_if_fail(id != NULL);

	g_free(item->id);
	item->id = g_strdup(id);
}

/* Comments URL */
gchar *feed_item_get_comments_url(FeedItem *item)
{
	g_return_val_if_fail(item != NULL, NULL);
	return item->comments_url;
}

void feed_item_set_comments_url(FeedItem *item, const gchar *url)
{
	g_return_if_fail(item != NULL);
	g_return_if_fail(url != NULL);

	g_free(item->comments_url);
	item->comments_url = g_strdup(url);
}

/* Comments URL */
gchar *feed_item_get_parent_id(FeedItem *item)
{
	g_return_val_if_fail(item != NULL, NULL);
	return item->parent_id;
}

void feed_item_set_parent_id(FeedItem *item, const gchar *id)
{
	g_return_if_fail(item != NULL);
	g_return_if_fail(id != NULL);

	g_free(item->parent_id);
	item->parent_id = g_strdup(id);
}

/* Media enclosure */
FeedItemEnclosure *feed_item_get_enclosure(FeedItem *item)
{
	g_return_val_if_fail(item != NULL, NULL);
	return item->enclosure;
}

void feed_item_set_enclosure(FeedItem *item, FeedItemEnclosure *enclosure)
{
	g_return_if_fail(item != NULL);
	g_return_if_fail(enclosure != NULL);
	g_return_if_fail(enclosure->url != NULL);
	g_return_if_fail(enclosure->type != NULL);

	feed_item_enclosure_free(item->enclosure);
	item->enclosure = enclosure;
}

/* Source title (Atom only) */
gchar *feed_item_get_sourcetitle(FeedItem *item)
{
	g_return_val_if_fail(item != NULL, NULL);
	return item->sourcetitle;
}

void feed_item_set_sourcetitle(FeedItem *item, const gchar *sourcetitle)
{
	g_return_if_fail(item != NULL);
	g_return_if_fail(sourcetitle != NULL);

	g_free(item->sourcetitle);
	item->sourcetitle = g_strdup(sourcetitle);
}

/* Source ID (Atom only) */
gchar *feed_item_get_sourceid(FeedItem *item)
{
	g_return_val_if_fail(item != NULL, NULL);
	return item->sourceid;
}

void feed_item_set_sourceid(FeedItem *item, const gchar *sourceid)
{
	g_return_if_fail(item != NULL);
	g_return_if_fail(sourceid != NULL);

	g_free(item->sourceid);
	item->sourceid = g_strdup(sourceid);
}

/* Source date (Atom only) */
time_t feed_item_get_sourcedate(FeedItem *item)
{
	g_return_val_if_fail(item != NULL, -1);
	return item->sourcedate;
}

void feed_item_set_sourcedate(FeedItem *item, time_t date)
{
	g_return_if_fail(item != NULL);
	item->sourcedate = date;
}

/* Date published */
time_t feed_item_get_date_published(FeedItem *item)
{
	g_return_val_if_fail(item != NULL, -1);
	return item->date_published;
}

void feed_item_set_date_published(FeedItem *item, time_t date)
{
	g_return_if_fail(item != NULL);
	item->date_published = date;
}

/* Date modified */
time_t feed_item_get_date_modified(FeedItem *item)
{
	g_return_val_if_fail(item != NULL, -1);
	return item->date_modified;
}

void feed_item_set_date_modified(FeedItem *item, time_t date)
{
	g_return_if_fail(item != NULL);
	item->date_modified = date;
}

FeedItem *feed_item_copy(FeedItem *item)
{
	FeedItem *nitem;

	g_return_val_if_fail(item != NULL, NULL);

	nitem = feed_item_new(NULL);

	nitem->url = g_strdup(item->url);
	nitem->title = g_strdup(item->title);
	nitem->summary = g_strdup(item->summary);
	nitem->text = g_strdup(item->text);
	nitem->author = g_strdup(item->author);
	nitem->id = g_strdup(item->id);
	nitem->comments_url = g_strdup(item->comments_url);
	nitem->parent_id = g_strdup(item->parent_id);

	nitem->enclosure = feed_item_enclosure_copy(item->enclosure);

	nitem->date_published = item->date_published;
	nitem->date_modified = item->date_modified;

	nitem->id_is_permalink = item->id_is_permalink;
	nitem->xhtml_content = item->xhtml_content;

	/* We have no way of knowing the size of object item->data is pointing
	 * to, so we can not reliably copy it to the new item. Caller will have
	 * to take care of that itself. */
	nitem->data = NULL;

	return nitem;
}

gboolean feed_item_id_is_permalink(FeedItem *item)
{
	g_return_val_if_fail(item != NULL, FALSE);

	return item->id_is_permalink;
}

void feed_item_set_id_permalink(FeedItem *item, gboolean permalink)
{
	g_return_if_fail(item != NULL);

	item->id_is_permalink = permalink;
}
