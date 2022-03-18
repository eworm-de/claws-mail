/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2013 Colin Leroy <colin@colino.net> and 
 * the Claws Mail team
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
 * 
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#ifdef USE_GPGME

#include <glib.h>
#include <string.h> /* for strlen */
#include <stdio.h> /* for strstr */

/* It's only a valid armor header if it's at the
 * beginning of the buffer or a new line, and if
 * there is only whitespace after it on the rest
 * of the line. */
gchar *pgp_locate_armor_header(const gchar *haystack, const gchar *needle)
{
	gchar *txt, *x, *i;
	gint ok;

	g_return_val_if_fail(haystack != NULL, NULL);
	g_return_val_if_fail(needle != NULL, NULL);

	/* Start at the beginning */
	txt = (gchar *)haystack;
	while (*txt != '\0') {

		/* Find next occurrence */
		x = strstr(txt, needle);

		if (x == NULL)
			break;

		/* Make sure that what we found is at the beginning of line */
		if (x != haystack && *(x - 1) != '\n') {
			txt = x + 1;
			continue;
		}

		/* Now look at what's between end of needle and end of that line.
		 * If there is anything else than whitespace, stop looking. */
		i = x + strlen(needle);
		ok = 1;
		while (*i != '\0' && *i != '\r' && *i != '\n') {
			if (!g_ascii_isspace(*i)) {
				ok = 0;
				break;
			}
			i++;
		}

		if (ok)
			return x;

		/* We are at the end of haystack */
		if (*i == '\0')
			return NULL;

		txt = i + 1;
	}

	return NULL;
}

#endif /* USE_GPGME */
