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
#include "claws_io.h"

gboolean prefs_common_get_flush_metadata(void);

/* FIXME make static once every file I/O is done using claws_* wrappers */
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

#if HAVE_FGETS_UNLOCKED

/* Open a file and locks it once
 * so subsequent I/O is faster
 */
FILE *claws_fopen(const char *file, const char *mode)
{
	FILE *fp = fopen(file, mode);
	if (!fp)
		return NULL;
	flockfile(fp);
	return fp;
}

FILE *claws_fdopen(int fd, const char *mode)
{
	FILE *fp = fdopen(fd, mode);
	if (!fp)
		return NULL;
	flockfile(fp);
	return fp;
}

/* Unlocks and close a file pointer
 */

int claws_fclose(FILE *fp)
{
	funlockfile(fp);
	return fclose(fp);
}

/* Unlock, then safe-close a file pointer
 * Safe close is done using fflush + fsync
 * if the according preference says so.
 */
int claws_safe_fclose(FILE *fp)
{
	funlockfile(fp);
	return safe_fclose(fp);
}

#endif
