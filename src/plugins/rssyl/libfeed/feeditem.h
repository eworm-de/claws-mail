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

#ifndef __FEEDITEM_H
#define __FEEDITEM_H

#include "feed.h"
#include "feeditemenclosure.h"

struct _FeedItem {
	gchar *url;
	gchar *title;
	gint title_format;
	gchar *summary;
	gchar *text;
	gchar *author;
	gchar *id;
	gchar *comments_url;
	gchar *parent_id;

	gchar *sourceid;
	gchar *sourcetitle;
	time_t sourcedate;

	gboolean id_is_permalink;
	gboolean xhtml_content;

	FeedItemEnclosure *enclosure;

	time_t date_published;
	time_t date_modified;

	Feed *feed;		/* feed we belong to */

	gpointer data;		/* application-specific data */
};

#define FEED_ITEM_TITLE_TEXT 0
#define FEED_ITEM_TITLE_HTML 1
#define FEED_ITEM_TITLE_XHTML 2
#define FEED_ITEM_TITLE_UNKNOWN 3

FeedItem *feed_item_new(Feed *feed);
void feed_item_free(FeedItem *item);

Feed *feed_item_get_feed(FeedItem *item);

gchar *feed_item_get_url(FeedItem *item);
void feed_item_set_url(FeedItem *item, const gchar *url);

gchar *feed_item_get_title(FeedItem *item);
void feed_item_set_title(FeedItem *item, const gchar *title);

gint feed_item_get_title_format(FeedItem *item);
void feed_item_set_title_format(FeedItem *item, gint format);

gchar *feed_item_get_summary(FeedItem *item);
void feed_item_set_summary(FeedItem *item, const gchar *summary);

gchar *feed_item_get_text(FeedItem *item);
void feed_item_set_text(FeedItem *item, const gchar *text);

gchar *feed_item_get_author(FeedItem *item);
void feed_item_set_author(FeedItem *item, const gchar *author);

gchar *feed_item_get_id(FeedItem *item);
void feed_item_set_id(FeedItem *item, const gchar *id);

gchar *feed_item_get_comments_url(FeedItem *item);
void feed_item_set_comments_url(FeedItem *item, const gchar *url);

gchar *feed_item_get_parent_id(FeedItem *item);
void feed_item_set_parent_id(FeedItem *item, const gchar *id);

FeedItemEnclosure *feed_item_get_enclosure(FeedItem *item);
void feed_item_set_enclosure(FeedItem *item, FeedItemEnclosure *enclosure);

gchar *feed_item_get_sourcetitle(FeedItem *item);
void feed_item_set_sourcetitle(FeedItem *item, const gchar *sourcetitle);

gchar *feed_item_get_sourceid(FeedItem *item);
void feed_item_set_sourceid(FeedItem *item, const gchar *sourceid);

time_t feed_item_get_sourcedate(FeedItem *item);
void feed_item_set_sourcedate(FeedItem *item, time_t date);

time_t feed_item_get_date_published(FeedItem *item);
void feed_item_set_date_published(FeedItem *item, time_t date);

time_t feed_item_get_date_modified(FeedItem *item);
void feed_item_set_date_modified(FeedItem *item, time_t date);

FeedItem *feed_item_copy(FeedItem *item);

gboolean feed_item_id_is_permalink(FeedItem *item);
void feed_item_set_id_permalink(FeedItem *item, gboolean permalink);

#endif /* __FEEDITEM_H */
