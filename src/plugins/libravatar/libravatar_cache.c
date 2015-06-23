/*
 * Claws Mail -- a GTK+ based, lightweight, and fast e-mail client
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

#include "libravatar_cache.h"
#include "utils.h"

gchar *libravatar_cache_init(const char *dirs[], gint start, gint end)
{
	gchar *subdir, *rootdir;
	int i;

	rootdir = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
				LIBRAVATAR_CACHE_DIR, G_DIR_SEPARATOR_S,
				NULL);
	if (!is_dir_exist(rootdir)) {
		if (make_dir(rootdir) < 0) {
			g_warning("cannot create root directory '%s'", subdir);
			g_free(rootdir);
			return NULL;
		}
	}
	for (i = start; i <= end; ++i) {
		subdir = g_strconcat(rootdir, dirs[i], NULL);
		if (!is_dir_exist(subdir)) {
			if (make_dir(subdir) < 0) {
				g_warning("cannot create directory '%s'", subdir);
				g_free(subdir);
				g_free(rootdir);
				return NULL;
			}
		}
		g_free(subdir);
	}

	return rootdir;
}
