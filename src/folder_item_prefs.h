/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2001 Hiroyuki Yamamoto
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

#ifndef FOLDER_ITEM_PREFS_H
#define FOLDER_ITEM_PREFS_H

#include <glib.h>
#include <sys/types.h>

typedef struct _FolderItemPrefs FolderItemPrefs;

#include "folder.h"

struct _FolderItemPrefs {
	gchar * directory;

	gboolean sort_by_number;
	gboolean sort_by_size;
	gboolean sort_by_date;
	gboolean sort_by_from;
	gboolean sort_by_subject;
	gboolean sort_by_score;

	gboolean sort_descending;

	gboolean enable_thread;

        int enable_processing;
	GSList * processing;

	int newmailcheck;
	int offlinesync;

	gboolean request_return_receipt;
	gboolean enable_default_to;
	gchar *default_to;
	gboolean enable_default_reply_to;
	gchar *default_reply_to;
	gboolean enable_simplify_subject;
	gchar *simplify_subject_regexp;
	gboolean enable_folder_chmod;
	gint folder_chmod;
	gboolean enable_default_account;
	gint default_account;
#if USE_ASPELL
	gboolean enable_default_dictionary;
	gchar *default_dictionary;
#endif
	gboolean save_copy_to_folder;
	guint color;
};

void folder_item_prefs_read_config(FolderItem * item);
void folder_item_prefs_save_config(FolderItem * item);
void folder_item_prefs_set_config(FolderItem * item,
				  int sort_type, gint sort_mode);
FolderItemPrefs *folder_item_prefs_new(void);
void folder_item_prefs_free(FolderItemPrefs * prefs);
gint folder_item_prefs_get_sort_type(FolderItem * item);
gint folder_item_prefs_get_sort_mode(FolderItem * item);
void folder_item_prefs_copy_prefs(FolderItem * src, FolderItem * dest);

#endif /* FOLDER_ITEM_PREFS_H */
