/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2018 Michael Rasmussen and the Claws Mail Team
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
#include <archive.h>

typedef struct _ArchiverPrefs	ArchiverPrefs;

typedef enum {
	COMPRESSION_GZIP,
	COMPRESSION_BZIP,
    COMPRESSION_COMPRESS,
#if ARCHIVE_VERSION_NUMBER >= 2006990
	COMPRESSION_LZMA,
	COMPRESSION_XZ,
#endif
#if ARCHIVE_VERSION_NUMBER >= 3000000
	COMPRESSION_LZIP,
#endif
#if ARCHIVE_VERSION_NUMBER >= 3001000
	COMPRESSION_LRZIP,
	COMPRESSION_LZOP,
	COMPRESSION_GRZIP,
#endif
#if ARCHIVE_VERSION_NUMBER >= 3001900
	COMPRESSION_LZ4,
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
    gint        unlink;
};

extern ArchiverPrefs archiver_prefs;
void archiver_prefs_init(void);
void archiver_prefs_done(void);

#endif

