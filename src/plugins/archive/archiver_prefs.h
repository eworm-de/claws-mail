/*
 * Claws Mail -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2008 Michael Rasmussen and the Claws Mail Team
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

#ifndef __ARCHIVERPREFS__
#define __ARCHIVERPREFS__

#include <glib.h>

typedef struct _ArchiverPrefs	ArchiverPrefs;

typedef enum {
	COMPRESSION_ZIP,
	COMPRESSION_BZIP,
#if NEW_ARCHIVE_API
        COMPRESSION_COMPRESS,
#endif
        COMPRESSION_NONE
} CompressionType;

typedef enum {
	FORMAT_TAR,
	FORMAT_SHAR,
	FORMAT_CPIO,
	FORMAT_PAX
} ArchiveFormat;

struct _ArchiverPrefs
{
	gchar		*save_folder;
	CompressionType	compression;
	ArchiveFormat	format;
	gint		recursive;
	gint		md5sum;
	gint		rename;
        gint            unlink;
};

extern ArchiverPrefs archiver_prefs;
void archiver_prefs_init(void);
void archiver_prefs_done(void);

#endif

