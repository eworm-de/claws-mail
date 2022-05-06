/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2016 Ricardo Mones and the Claws Mail Team
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
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

#ifndef __LIBRAVATAR_IMAGE_H
#define __LIBRAVATAR_IMAGE_H

#include <glib.h>

typedef struct _AvatarImageFetch	AvatarImageFetch;

GdkPixbuf *libravatar_image_fetch(AvatarImageFetch *ctx);

struct _AvatarImageFetch
{
	const gchar	*url;
	const gchar	*md5;
	gchar		*filename;
	GdkPixbuf	*pixbuf;
	gboolean	ready;
};

#endif
