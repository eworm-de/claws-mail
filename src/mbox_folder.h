/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2001 Hiroyuki Yamamoto & The Sylpheed Claws Team
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

#ifndef MBOX_FOLDER_H

#define MBOX_FOLDER_H

#include <glib.h>
#include "folder.h"

/*
mailfile mailfile_init(char * filename);
char * mailfile_readmsg(mailfile f, int index);
char * mailfile_readheader(mailfile f, int index);
void mailfile_done(mailfile f);
int mailfile_count(mailfile f);
*/
typedef struct _MBOXFolder	MBOXFolder;

#define MBOX_FOLDER(obj)	((MBOXFolder *)obj)

struct _MBOXFolder
{
	LocalFolder lfolder;
};

FolderClass *mbox_get_class	();
Folder	*mbox_folder_new	(const gchar	*name,
				 const gchar	*path);
void     mbox_folder_destroy	(Folder		*folder);


GSList *mbox_get_msg_list(Folder *folder, FolderItem *item, gboolean use_cache);
gchar *mbox_fetch_msg(Folder *folder, FolderItem *item, gint num);

void mbox_scan_folder(Folder *folder, FolderItem *item);
gchar * mbox_get_virtual_path(FolderItem * item);
gint mbox_add_msg(Folder *folder, FolderItem *dest, const gchar *file,
		  gboolean remove_source);

gint mbox_remove_all_msg(Folder *folder, FolderItem *item);
gint mbox_remove_msg(Folder *folder, FolderItem *item, gint num);
void mbox_update_mark(Folder * folder, FolderItem * item);
gint mbox_move_msgs_with_dest(Folder *folder, FolderItem *dest,
			      GSList *msglist);
gint mbox_move_msg(Folder *folder, FolderItem *dest, MsgInfo *msginfo);
void mbox_change_flags(Folder * folder, FolderItem * item, MsgInfo * info);
gint mbox_copy_msg(Folder *folder, FolderItem *dest, MsgInfo *msginfo);
gint mbox_copy_msgs_with_dest(Folder *folder, FolderItem *dest, GSList *msglist);
gint mbox_create_tree(Folder *folder);
FolderItem *mbox_create_folder(Folder *folder, FolderItem *parent,
			       const gchar *name);
gint mbox_rename_folder(Folder *folder, FolderItem *item, const gchar *name);
gint mbox_remove_folder(Folder *folder, FolderItem *item);
void mbox_finished_copy(Folder *folder, FolderItem *dest);


#endif
