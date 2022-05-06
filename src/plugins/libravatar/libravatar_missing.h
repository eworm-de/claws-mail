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

#ifndef __LIBRAVATAR_MISSING_H
#define __LIBRAVATAR_MISSING_H

#include <glib.h>

#define LIBRAVATAR_MISSING_FILE "missing"
/* multiply cache interval time pref for missing items */
#define LIBRAVATAR_MISSING_TIME (libravatarprefs.cache_interval * 3600 * 7)

GHashTable *missing_load_from_file	(const gchar *filename);
gint missing_save_to_file		(GHashTable *table,
					 const gchar *filename);
void missing_add_md5			(GHashTable *table,
					 const gchar *md5);
gboolean is_missing_md5			(GHashTable *table,
					 const gchar *md5);

#endif
