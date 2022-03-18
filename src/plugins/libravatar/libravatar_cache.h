/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2014-2015 Ricardo Mones and the Claws Mail Team
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

#ifndef __LIBRAVATAR_CACHE_H
#define __LIBRAVATAR_CACHE_H

#include <glib.h>

#define LIBRAVATAR_CACHE_DIR "avatarcache"

typedef struct _AvatarCacheStats	AvatarCacheStats;
typedef struct _AvatarCleanupResult	AvatarCleanupResult;

gchar			*libravatar_cache_init		(const char *dirs[],
							 gint start,
							 gint end);
AvatarCacheStats	*libravatar_cache_stats		();
AvatarCleanupResult	*libravatar_cache_clean		();

struct _AvatarCacheStats
{
	gint bytes;
	gint files;
	gint dirs;
	gint others;
	gint errors;
};

struct _AvatarCleanupResult
{
	guint removed;
	guint e_stat;
	guint e_unlink;
};

#endif
