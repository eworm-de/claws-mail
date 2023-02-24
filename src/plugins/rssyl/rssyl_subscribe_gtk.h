/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2005-2023 the Claws Mail Team and Andrej Kacian <andrej@kacian.sk>
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

#ifndef __RSSYL_SUBSCRIBE_GTK_H
#define __RSSYL_SUBSCRIBE_GTK_H

#include "libfeed/feed.h"

struct _RSubCtx {
	Feed *feed;
	gboolean edit_properties;
	gchar *official_title;
};

typedef struct _RSubCtx RSubCtx;

void rssyl_subscribe_dialog(RSubCtx *ctx);

#endif /* __RSSYL_SUBSCRIBE_GTK_H */
