/*
 * Claws Mail -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2015 Hiroyuki Yamamoto and the Claws Mail Team
 * Copyright (C) 2014-2015 Ricardo Mones
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

gchar	*libravatar_cache_init		(const char *dirs[],
					 gint start,
					 gint end);

#endif
