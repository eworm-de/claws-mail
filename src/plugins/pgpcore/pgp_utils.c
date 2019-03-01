/*
 * Claws Mail -- a GTK+ based, lightweight, and fast e-mail client
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

#include "pgp_utils.h"
#include "codeconv.h"
#include "file-utils.h"

gchar *pgp_locate_armor_header(gchar *textdata, const gchar *armor_header)
{
	gchar *pos;

	pos = strstr(textdata, armor_header);
	/*
	 * It's only a valid armor header if it's at the
	 * beginning of the buffer or a new line.
	 */
	if (pos != NULL && (pos == textdata || *(pos-1) == '\n'))
	{
	      return pos;
	}
	return NULL;
}
#endif /* USE_GPGME */
