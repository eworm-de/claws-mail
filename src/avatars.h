/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2014 Ricardo Mones and the Claws Mail team
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __AVATARS_H__
#define __AVATARS_H__

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "proctypes.h"

#define AVATAR_IMAGE_RENDER_HOOKLIST "avatar_image_render"

typedef struct _AvatarRender	AvatarRender;

struct _AvatarRender
{
	MsgInfo *full_msginfo;
	GtkWidget *image;
	gint type;
};

AvatarRender *avatars_avatarrender_new		(MsgInfo *msginfo);
void avatars_avatarrender_free			(AvatarRender *avrender);

gboolean avatars_internal_rendering_hook	(gpointer source,
						 gpointer data);

void avatars_init				(void);
void avatars_done				(void);

#endif
