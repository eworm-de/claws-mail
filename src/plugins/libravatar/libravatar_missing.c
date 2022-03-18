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

#include "config.h"

#include <stdio.h>

#include "libravatar_missing.h"
#include "libravatar_prefs.h"
#include "utils.h"
#include "file-utils.h"

/**
 * Loads the hash table of md5sum → time from the given filename.
 *
 * @param filename  Name of the hash table filename.
 *
 * @return A hash table with the entries not expired contained in
 *         the given filename or NULL if failed to load.
 */
GHashTable *missing_load_from_file(const gchar *filename)
{
	FILE *file = claws_fopen(filename, "r");
	time_t t;
	time_t seen;
	gchar md5sum[33];
	GHashTable *table = NULL;
	int r = 0, a = 0, d = 0;

	if (file == NULL) {
		if (!is_file_exist(filename)) { /* first run, return an empty table */
			return g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
		}
		g_warning("cannot open '%s' for reading", filename);
		return NULL;
	}
	t = time(NULL);
	if (t == (time_t)-1) {
		g_warning("cannot get time!");
		goto close_exit;
	}

	table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	while ((r = fscanf(file, "%32s %" CM_TIME_FORMAT "\n", md5sum, &seen)) != EOF) {
		if (t - seen <= LIBRAVATAR_MISSING_TIME) {
			time_t *value = g_malloc0(sizeof(time_t));
			*value = seen;
			g_hash_table_insert(table, g_strdup(md5sum), value);
		} else
			d++;
		a++;
	}

close_exit:
	if (claws_fclose(file) != 0)
		g_warning("error closing '%s'", filename);

	debug_print("Read %d missing avatar entries, %d obsolete entries discarded\n", a, d);
	return table;
}

/**
 * Saves a hash table item.
 *
 * @param key    Hash table key, a md5sum string.
 * @param vakue  Hash table value, a time_t.
 * @param data   User data, a pointer to the open FILE being written.
 */
static void missing_save_item(gpointer key, gpointer value, gpointer data)
{
	FILE *file = (FILE *)data;
	gchar *line = g_strdup_printf("%s %"CM_TIME_FORMAT"\n", (gchar *)key, *(time_t *)value);
	if (claws_fputs(line, file) < 0)
		g_warning("error saving missing item");
	g_free(line);
}

/**
 * Saves a hash table of md5sum → time to a given file name.
 *
 * @param table     The table to save.
 * @param filename  The name of the file where table data will be saved.
 *
 * @return 0 on success, -1 if there was some problem saving.
 */
gint missing_save_to_file(GHashTable *table, const gchar *filename)
{
	FILE *file = claws_fopen(filename, "w");

	if (file == NULL) {
		g_warning("cannot open '%s' for writing", filename);
		return -1;
	}

	g_hash_table_foreach(table, missing_save_item, (gpointer)file);
	debug_print("Saved %u missing avatar entries\n", g_hash_table_size(table));

	if (claws_safe_fclose(file) != 0) {
		g_warning("error closing '%s'", filename);
		return -1;
	}

	return 0;
}

/**
 * Adds a md5sum to a md5sum → time hash table.
 * If the md5sum is already in the table its time is updated.
 *
 * @param table  The table to use.
 * @param md5    The md5sum to add or update.
 */
void missing_add_md5(GHashTable *table, const gchar *md5)
{
	time_t t = time(NULL);

	if (t == (time_t)-1) {
		g_warning("cannot get time!");
		return;
	}

	time_t *seen = g_hash_table_lookup(table, md5);
	if (seen == NULL) {
		seen = g_malloc0(sizeof(time_t));
		*seen = t;
		g_hash_table_insert(table, g_strdup(md5), seen);
		debug_print("New md5 %s added with time %"CM_TIME_FORMAT"\n", md5, t);
	} else {
		*seen = t; /* just update */
		debug_print("Updated md5 %s with time %"CM_TIME_FORMAT"\n", md5, t);
	}
}

/**
 * Check if a md5sum is in hash table and not expired.
 *
 * @param table   The table to check against.
 * @param md5     The md5sum to check.
 *
 * @return TRUE if the md5sum is in the table and is not expired,
           FALSE otherwise.
 */
gboolean is_missing_md5(GHashTable *table, const gchar *md5)
{
	time_t t;
	time_t *seen = (time_t *)g_hash_table_lookup(table, md5);

	if (seen == NULL)
		return FALSE;

	t = time(NULL);
	if (t != (time_t)-1) {
		if (t - *seen <= LIBRAVATAR_MISSING_TIME) {
			debug_print("Found missing md5 %s\n", md5);
			return TRUE;
		}
	}
	return FALSE;
}

