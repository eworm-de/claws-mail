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

#ifndef PREFS_FOLDER_ITEM_H

#define PREFS_FOLDER_ITEM_H

#include "folder.h"
#include <glib.h>
#include <sys/types.h>

struct _PrefsFolderItem {
	gchar * directory;

	gboolean sort_by_number;
	gboolean sort_by_size;
	gboolean sort_by_date;
	gboolean sort_by_from;
	gboolean sort_by_subject;
	gboolean sort_by_score;

	gboolean sort_descending;

	gboolean enable_thread;

	gint kill_score;
	gint important_score;

	GSList * scoring;
	GSList * processing;

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
	gboolean save_copy_to_folder;
	guint color;
};

typedef struct _PrefsFolderItem PrefsFolderItem;

/* CLAWS: due a messed up circular header references using 
 * void *. but this is *REALLY* a folderview */ 
void prefs_folder_item_create (void *folderview, FolderItem *item); 

void prefs_folder_item_read_config(FolderItem * item);
void prefs_folder_item_save_config(FolderItem * item);
void prefs_folder_item_set_config(FolderItem * item,
				  int sort_type, gint sort_mode);
PrefsFolderItem * prefs_folder_item_new(void);
void prefs_folder_item_free(PrefsFolderItem * prefs);
gint prefs_folder_item_get_sort_type(FolderItem * item);
gint prefs_folder_item_get_sort_mode(FolderItem * item);
void prefs_folder_item_copy_prefs(FolderItem * src, FolderItem * dest);

#endif
