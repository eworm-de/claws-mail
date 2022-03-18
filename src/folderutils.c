/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2004-2012 Hiroyuki Yamamoto & The Claws Mail Team
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

#include "config.h"

#include <glib.h>

#include "utils.h"
#include "prefs_common.h"
#include "folderutils.h"
#include "prefs_account.h"
#include "account.h"
#include "mainwindow.h"
#include "summaryview.h"

static gboolean folderutils_mark_all_read_node_func(GNode *node, gpointer data);


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
				duplist = g_slist_prepend(duplist, msginfo);
			} else {
				duplist = g_slist_prepend(duplist, msginfo_dup);
				g_hash_table_insert(table, msginfo->msgid, msginfo);
			}
		}
	}

	if (duplist) {
		switch (mode) {
		case DELETE_DUPLICATES_REMOVE: {
			FolderItem *trash = NULL;
			gboolean in_trash = FALSE;
			PrefsAccount *ac;

			if (NULL != (ac = account_find_from_item(item))) {
				trash = account_get_special_folder(ac, F_TRASH);
				in_trash = (trash != NULL && trash == item);
			}
			if (trash == NULL)
				trash = item->folder->trash;

			if (folder_has_parent_of_type(item, F_TRASH) || trash == NULL || in_trash)
				folder_item_remove_msgs(item, duplist);
			else 			
				folder_item_move_msgs(trash, duplist);
			break;
		}
		case DELETE_DUPLICATES_SETFLAG:
			for (cur = duplist; cur != NULL; cur = g_slist_next(cur)) {
				MsgInfo *msginfo = (MsgInfo *) cur->data;

				procmsg_msginfo_set_to_folder(msginfo, NULL);
				procmsg_msginfo_unset_flags(msginfo, MSG_MARKED, 
					MSG_MOVE | MSG_COPY | MSG_MOVE_DONE);
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

		procmsg_msginfo_free(&msginfo);
	}
	g_slist_free(msglist);

	folder_item_update_thaw();

	debug_print("done.\n");

	return dups;
}

void folderutils_mark_all_read(FolderItem *item, gboolean mark_as_read)
{
	MsgInfoList *msglist, *cur;
	MainWindow *mainwin = mainwindow_get_mainwindow();
	int i = 0, m = 0;
	gchar *msg = mark_as_read?"read":"unread";
	void(*summary_mark_func)(SummaryView*, gboolean) =
			mark_as_read?summary_mark_all_read:summary_mark_all_unread;

	debug_print("marking all %s in item %s\n", msg, (item==NULL)?"NULL":item->name);
	cm_return_if_fail(item != NULL);

	folder_item_update_freeze();
	if (mainwin && mainwin->summaryview &&
	    mainwin->summaryview->folder_item == item) {
		debug_print("folder opened, using summary\n");
		summary_mark_func(mainwin->summaryview, FALSE);
	} else {
		msglist = folder_item_get_msg_list(item);
		debug_print("got msglist %p\n", msglist);
		if (msglist == NULL) {
			folder_item_update_thaw();
			return;
		}
		folder_item_set_batch(item, TRUE);
		for (cur = msglist; cur != NULL; cur = g_slist_next(cur)) {
			MsgInfo *msginfo = cur->data;

			if (mark_as_read) {
				if (msginfo->flags.perm_flags & (MSG_NEW | MSG_UNREAD)) {
					procmsg_msginfo_unset_flags(msginfo, MSG_NEW | MSG_UNREAD, 0);
					m++;
				}
			} else {
				if (!(msginfo->flags.perm_flags & MSG_UNREAD)) {
					procmsg_msginfo_set_flags(msginfo, MSG_UNREAD, 0);
					m++;
				}
			}
			i++;
			procmsg_msginfo_free(&msginfo);
		}
		folder_item_set_batch(item, FALSE);
		folder_item_close(item);
		debug_print("marked %d messages out of %d as %s\n", m, i, msg);
		g_slist_free(msglist);
	}
	folder_item_update_thaw();
}

static gboolean folderutils_mark_all_read_node_func(GNode *node, gpointer data)
{
	if (node) {
		FolderItem *sub_item = (FolderItem *) node->data;
		if (prefs_common.run_processingrules_before_mark_all)
			folderview_run_processing(sub_item);
		folderutils_mark_all_read(sub_item, (gboolean) GPOINTER_TO_INT(data));
	}
	return(FALSE);
}

void folderutils_mark_all_read_recursive(FolderItem *item, gboolean mark_as_read)
{
	cm_return_if_fail(item != NULL);

	cm_return_if_fail(item->folder != NULL);
	cm_return_if_fail(item->folder->node != NULL);

	g_node_traverse(item->node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
		folderutils_mark_all_read_node_func,
		GINT_TO_POINTER(mark_as_read));
}
