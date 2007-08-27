/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2007 Hiroyuki Yamamoto and the Claws Mail team
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
	int offlinesync_days;
	int remove_old_bodies;

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
	gboolean enable_default_alt_dictionary;
	gchar *default_alt_dictionary;
#endif
	gboolean save_copy_to_folder;
	guint color;

	gboolean compose_with_format;
	gchar *compose_subject_format;
	gchar *compose_body_format;
	gboolean reply_with_format;
	gchar *reply_quotemark;
	gchar *reply_body_format;
	gboolean forward_with_format;
	gchar *forward_quotemark;
	gchar *forward_body_format;
};

void folder_item_prefs_read_config(FolderItem * item);
void folder_item_prefs_save_config(FolderItem * item);
void folder_item_prefs_save_config_recursive(FolderItem * item);
FolderItemPrefs *folder_item_prefs_new(void);
void folder_item_prefs_free(FolderItemPrefs * prefs);
void folder_item_prefs_copy_prefs(FolderItem * src, FolderItem * dest);

#endif /* FOLDER_ITEM_PREFS_H */
