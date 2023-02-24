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

#ifndef __FEEDITEMENCLOSURE_H
#define __FEEDITEMENCLOSURE_H

struct _FeedItemEnclosure {
	gchar *url;
	gchar *type;
	gulong size;
};

typedef struct _FeedItemEnclosure FeedItemEnclosure;

FeedItemEnclosure *feed_item_enclosure_new(gchar *url, gchar *type, gulong size);
void feed_item_enclosure_free(FeedItemEnclosure *enclosure);

gchar *feed_item_enclosure_get_url(FeedItemEnclosure *enclosure);
void feed_item_enclosure_set_url(FeedItemEnclosure *enclosure, gchar *url);

gchar *feed_item_enclosure_get_type(FeedItemEnclosure *enclosure);
void feed_item_enclosure_set_type(FeedItemEnclosure *enclosure, gchar *type);

gulong feed_item_enclosure_get_size(FeedItemEnclosure *enclosure);
void feed_item_enclosure_set_size(FeedItemEnclosure *enclosure, gulong size);

FeedItemEnclosure *feed_item_enclosure_copy(FeedItemEnclosure *enclosure);

#endif /* __FEEDITEMENCLOSURE_H */
