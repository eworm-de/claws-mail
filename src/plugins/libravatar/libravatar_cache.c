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

#include <sys/stat.h>

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
			g_warning("cannot create root directory '%s'", rootdir);
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

static void cache_stat_item(gpointer filename, gpointer data)
{
	struct stat		s;
	const gchar		*fname = (const gchar *) filename;
	AvatarCacheStats	*stats = (AvatarCacheStats *) data;

	if (0 == g_stat(fname, &s)) {
		if (S_ISDIR(s.st_mode) != 0) {
			stats->dirs++;
		}
		else if (S_ISREG(s.st_mode) != 0) {
			stats->files++;
			stats->bytes += s.st_size;
		}
		else {
			stats->others++;
		}
	}
	else {
		g_warning("cannot stat %s\n", fname);
		stats->errors++;
	}
}

static void cache_items_deep_first(const gchar *dir, GSList **items, guint *failed)
{
	struct dirent	*d;
	DIR		*dp;

	cm_return_if_fail(dir != NULL);

	if ((dp = opendir(dir)) == NULL) {
		g_warning("cannot open directory %s\n", dir);
		(*failed)++;
		return;
	}
	while ((d = readdir(dp)) != NULL) {
		if (strcmp(d->d_name, ".") == 0 || strcmp(d->d_name, "..") == 0) {
			continue;
		}
		else {
			const gchar *fname = g_strconcat(dir, G_DIR_SEPARATOR_S,
						   d->d_name, NULL);
			if (is_dir_exist(fname))
				cache_items_deep_first(fname, items, failed);
			*items = g_slist_append(*items, (gpointer) fname);
		}
	}
	closedir(dp);
}

AvatarCacheStats *libravatar_cache_stats()
{
	gchar *rootdir;
	AvatarCacheStats *stats;
	GSList *items = NULL;
	guint errors = 0;

	stats = g_new0(AvatarCacheStats, 1);
	cm_return_val_if_fail(stats != NULL, NULL);

	rootdir = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
				LIBRAVATAR_CACHE_DIR, G_DIR_SEPARATOR_S,
				NULL);
	cache_items_deep_first(rootdir, &items, &errors);
	stats->errors += errors;
	g_slist_foreach(items, (GFunc) cache_stat_item, (gpointer) stats);
	slist_free_strings_full(items);
	g_free(rootdir);

	return stats;
}
