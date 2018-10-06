/*
 * Claws Mail -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2018 Colin Leroy and the Claws Mail team
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
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#include <stdio.h>
#include <unistd.h>

#include "timing.h"

gboolean prefs_common_get_flush_metadata(void);

int safe_fclose(FILE *fp)
{
	int r;
	START_TIMING("");

	if (fflush(fp) != 0) {
		return EOF;
	}
	if (prefs_common_get_flush_metadata() && fsync(fileno(fp)) != 0) {
		return EOF;
	}

	r = fclose(fp);
	END_TIMING();

	return r;
}
