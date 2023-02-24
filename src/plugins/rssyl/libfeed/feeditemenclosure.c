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

#include "feeditemenclosure.h"

FeedItemEnclosure *feed_item_enclosure_new(gchar *url, gchar *type, gulong size)
{
	FeedItemEnclosure *enclosure = NULL;

	g_return_val_if_fail(url != NULL, NULL);
	g_return_val_if_fail(type != NULL, NULL);
	g_return_val_if_fail(size > 0, NULL);

	enclosure = g_malloc( sizeof(FeedItemEnclosure) );
	enclosure->url = g_strdup(url);
	enclosure->type = g_strdup(type);
	enclosure->size = size;

	return enclosure;
}

void feed_item_enclosure_free(FeedItemEnclosure *enclosure)
{
	if( enclosure == NULL )
		return;

	g_free(enclosure->url);
	g_free(enclosure->type);

	g_free(enclosure);
	enclosure = NULL;
}

/* URL */
gchar *feed_item_enclosure_get_url(FeedItemEnclosure *enclosure)
{
	g_return_val_if_fail(enclosure != NULL, NULL);
	return enclosure->url;
}

void feed_item_enclosure_set_url(FeedItemEnclosure *enclosure, gchar *url)
{
	g_return_if_fail(enclosure != NULL);
	g_return_if_fail(url != NULL);

	g_free(enclosure->url);
	enclosure->url = g_strdup(url);
}

/* Type */
gchar *feed_item_enclosure_get_type(FeedItemEnclosure *enclosure)
{
	g_return_val_if_fail(enclosure != NULL, NULL);
	return enclosure->type;
}

void feed_item_enclosure_set_type(FeedItemEnclosure *enclosure, gchar *type)
{
	g_return_if_fail(enclosure != NULL);
	g_return_if_fail(type != NULL);

	g_free(enclosure->type);
	enclosure->type = g_strdup(type);
}

/* Size */
gulong feed_item_enclosure_get_size(FeedItemEnclosure *enclosure)
{
	g_return_val_if_fail(enclosure != NULL, -1);
	return enclosure->size;
}

void feed_item_enclosure_set_size(FeedItemEnclosure *enclosure, gulong size)
{
	g_return_if_fail(enclosure != NULL);
	g_return_if_fail(size > 0);

	enclosure->size = size;
}

FeedItemEnclosure *feed_item_enclosure_copy(FeedItemEnclosure *enclosure)
{
	if (enclosure == NULL)
		return NULL;

	return feed_item_enclosure_new(enclosure->url, enclosure->type,
			enclosure->size);
}
