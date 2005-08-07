/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2004 Hiroyuki Yamamoto & The Sylpheed-Claws Team
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

#include <glib.h>

#include "utils.h"
#include "prefs_common.h"
#include "folderutils.h"

gint folderutils_delete_duplicates(FolderItem *item,
				   DeleteDuplicatesMode mode)
{
	GHashTable *table;
	GSList *msglist, *cur, *duplist = NULL;
	guint dups;
	
	if (item->folder->klass->remove_msg == NULL)
		return -1;
	
	debug_print("Deleting duplicated messages...\n");

	msglist = folder_item_get_msg_list(item);
	if (msglist == NULL)
		return 0;
	table = g_hash_table_new(g_str_hash, g_str_equal);

	folder_item_update_freeze();
	for (cur = msglist; cur != NULL; cur = g_slist_next(cur)) {
		MsgInfo *msginfo = (MsgInfo *) cur->data;
		MsgInfo *msginfo_dup = NULL;

		if (!msginfo || !msginfo->msgid || !*msginfo->msgid)
			continue;
		
		msginfo_dup = g_hash_table_lookup(table, msginfo->msgid);
		if (msginfo_dup == NULL)
			g_hash_table_insert(table, msginfo->msgid, msginfo);
		else {
			if ((MSG_IS_UNREAD(msginfo->flags) && !MSG_IS_UNREAD(msginfo_dup->flags)) || 
			    (MSG_IS_UNREAD(msginfo->flags) == MSG_IS_UNREAD(msginfo_dup->flags))) {
				duplist = g_slist_append(duplist, msginfo);
			} else {
				duplist = g_slist_append(duplist, msginfo_dup);
				g_hash_table_insert(table, msginfo->msgid, msginfo);
			}
		}
	}

	if (duplist) {
		switch (mode) {
		case DELETE_DUPLICATES_REMOVE: {
			FolderItem *trash = item->folder->trash;

			if (folder_has_parent_of_type(item, F_TRASH) || trash == NULL)
				folder_item_remove_msgs(item, duplist);
			else 			
				folder_item_move_msgs(trash, duplist);
			break;
		}
		case DELETE_DUPLICATES_SETFLAG:
			for (cur = duplist; cur != NULL; cur = g_slist_next(cur)) {
				MsgInfo *msginfo = (MsgInfo *) cur->data;

				procmsg_msginfo_set_to_folder(msginfo, NULL);
				procmsg_msginfo_unset_flags(msginfo, MSG_MARKED, MSG_MOVE | MSG_COPY);
				procmsg_msginfo_set_flags(msginfo, MSG_DELETED, 0);
			}
			break;
		default:
			break;
		}
	}
	dups = g_slist_length(duplist);
	g_slist_free(duplist);

	g_hash_table_destroy(table);

	for (cur = msglist; cur != NULL; cur = g_slist_next(cur)) {
		MsgInfo *msginfo = (MsgInfo *) cur->data;

		procmsg_msginfo_free(msginfo);
	}
	g_slist_free(msglist);

	folder_item_update_thaw();

	debug_print("done.\n");

	return dups;	
}

void folderutils_mark_all_read(FolderItem *item)
{
	MsgInfoList *msglist, *cur;

	g_return_if_fail(item != NULL);

	msglist = folder_item_get_msg_list(item);
	if (msglist == NULL)
		return;

	folder_item_update_freeze();
	folder_item_set_batch(item, TRUE);
	for (cur = msglist; cur != NULL; cur = g_slist_next(cur)) {
		MsgInfo *msginfo = cur->data;

		if (msginfo->flags.perm_flags & (MSG_NEW | MSG_UNREAD))
			procmsg_msginfo_unset_flags(msginfo, MSG_NEW | MSG_UNREAD, 0);
		procmsg_msginfo_free(msginfo);
	}
	folder_item_set_batch(item, FALSE);
	folder_item_update_thaw();

	g_slist_free(msglist);
}
