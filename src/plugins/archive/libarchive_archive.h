/* vim: set textwidth=80 tabstop=4: */

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

#ifndef __LIBARCHIVE_ARCHIVE_H__
#define __LIBARCHIVE_ARCHIVE_H__

#include <glib.h>
#include "folder.h"

typedef enum _COMPRESS_METHOD COMPRESS_METHOD;
enum _COMPRESS_METHOD {
		ZIP,
		BZIP2,
#if NEW_ARCHIVE_API
                COMPRESS,
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

enum FILE_FLAGS { NO_FLAGS, ARCHIVE_EXTRACT_PERM, ARCHIVE_EXTRACT_TIME,
				ARCHIVE_EXTRACT_ACL, ARCHIVE_EXTRACT_FFLAGS };

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

#ifdef _TEST
void archive_set_permissions(int perm);
const gchar* archive_extract(const char* archive_name, int flags);
void archive_scan_folder(const char* dir);
#endif

#endif

