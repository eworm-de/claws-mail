/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2005 Andrej Kacian <andrej@kacian.sk>
 *
 * - a strreplace function (something like sed's s/foo/bar/g)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <stdlib.h>
#include <ctype.h>

#include "common/utils.h"

gchar *rssyl_strreplace(const gchar *source, gchar *pattern,
			gchar *replacement)
{
	gchar *new, *w_new;
	const gchar *c;
	guint count = 0, final_length;
	size_t len_pattern, len_replacement;

/*
	debug_print("RSSyl: ======= strreplace: '%s': '%s'->'%s'\n", source, pattern,
			replacement);
*/

	if( source == NULL || pattern == NULL ) {
		debug_print("RSSyl: source or pattern is NULL!!!\n");
		return NULL;
	}

	if( !g_utf8_validate(source, -1, NULL) ) {
		debug_print("RSSyl: source is not an UTF-8 encoded text\n");
		return NULL;
	}

	if( !g_utf8_validate(pattern, -1, NULL) ) {
		debug_print("RSSyl: pattern is not an UTF-8 encoded text\n");
		return NULL;
	}

	len_pattern = strlen(pattern);
	len_replacement = strlen(replacement);

	c = source;
	while( ( c = g_strstr_len(c, strlen(c), pattern) ) ) {
		count++;
		c += len_pattern;
	}

/*
	debug_print("RSSyl: ==== count = %d\n", count);
*/

	final_length = strlen(source)
		- ( count * len_pattern )
		+ ( count * len_replacement );

	new = malloc(final_length + 1);
	w_new = new;
	memset(new, '\0', final_length + 1);

	c = source;

	while( *c != '\0' ) {
		if( !memcmp(c, pattern, len_pattern) ) {
			gboolean break_after_rep = FALSE;
			int i;
			if (*(c + len_pattern) == '\0')
				break_after_rep = TRUE;
			for (i = 0; i < len_replacement; i++) {
				*w_new = replacement[i];
				w_new++;
			}
			if (break_after_rep)
				break;
			c = c + len_pattern;
		} else {
			*w_new = *c;
			w_new++;
			c++;
		}
	}
	return new;
}

gchar *rssyl_sanitize_string(const gchar *str, gboolean strip_nl)
{
	gchar *new = NULL;
	const gchar *c = str;
	gchar *new_ptr;
	if( str == NULL )
		return NULL;

	new_ptr = new = malloc(strlen(str) + 1);
	if (new == NULL)
		return NULL;
	memset(new, '\0', strlen(str) + 1);

	while( *c != '\0' ) {
		if( !g_ascii_isspace(*c) || *c == ' ' || (!strip_nl && *c == '\n') ) {
			*new_ptr = *c;
			new_ptr++;
		}
		c++;
	}

	return new;
}
