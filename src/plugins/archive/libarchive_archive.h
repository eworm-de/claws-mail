/* vim: set textwidth=80 tabstop=4: */

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

#ifndef __LIBARCHIVE_ARCHIVE_H__
#define __LIBARCHIVE_ARCHIVE_H__

#include <glib.h>
#include "folder.h"

#include <archive.h>

typedef enum _COMPRESS_METHOD COMPRESS_METHOD;
enum _COMPRESS_METHOD {
		GZIP,
		BZIP2,
        COMPRESS,
#if ARCHIVE_VERSION_NUMBER >= 2006990
		LZMA,
		XZ,
#endif
#if ARCHIVE_VERSION_NUMBER >= 3000000
		LZIP,
#endif
#if ARCHIVE_VERSION_NUMBER >= 3001000
		LRZIP,
		LZOP,
		GRZIP,
#endif
#if ARCHIVE_VERSION_NUMBER >= 3001900
		LZ4,
#endif
        NO_COMPRESS
};

typedef enum _ARCHIVE_FORMAT ARCHIVE_FORMAT;
enum _ARCHIVE_FORMAT {
		NO_FORMAT,
		TAR,
		SHAR,
		PAX,
		CPIO
};

typedef struct _MsgTrash MsgTrash;
struct _MsgTrash {
    FolderItem* item;
    /* List of MsgInfos* */
    GSList* msgs;
};

MsgTrash* new_msg_trash(FolderItem* item);
void archive_free_archived_files();
void archive_add_msg_mark(MsgTrash* trash, MsgInfo* msg);
void archive_add_file(gchar* path);
GSList* archive_get_file_list();
void archive_free_file_list(gboolean md5, gboolean rename);
const gchar* archive_create(const char* archive_name, GSList* files,
				COMPRESS_METHOD method, ARCHIVE_FORMAT format);
gboolean before_date(time_t msg_mtime, const gchar* before);
void archiver_set_tooltip(GtkWidget* widget, gchar* text);

#ifdef _TEST
void archive_set_permissions(int perm);
const gchar* archive_extract(const char* archive_name, int flags);
void archive_scan_folder(const char* dir);
#endif

#endif
