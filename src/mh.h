/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2002 Hiroyuki Yamamoto
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

#ifndef __MH_H__
#define __MH_H__

#include <glib.h>

#include "folder.h"
#include "procmsg.h"

typedef struct _MHFolder	MHFolder;

#define MH_FOLDER(obj)		((MHFolder *)obj)

struct _MHFolder
{
	LocalFolder lfolder;
};

Folder	*mh_folder_new		(const gchar	*name,
				 const gchar	*path);
void     mh_folder_destroy	(MHFolder	*folder);

GSList  *mh_get_msg_list	(Folder		*folder,
				 FolderItem	*item,
				 gboolean	 use_cache);
gchar   *mh_fetch_msg		(Folder		*folder,
				 FolderItem	*item,
				 gint		 num);
gint     mh_add_msg		(Folder		*folder,
				 FolderItem	*dest,
				 const gchar	*file,
				 gboolean	 remove_source);
gint     mh_move_msg		(Folder		*folder,
				 FolderItem	*dest,
				 MsgInfo	*msginfo);
gint     mh_move_msgs_with_dest	(Folder		*folder,
				 FolderItem	*dest,
				 GSList		*msglist);
gint     mh_copy_msg		(Folder		*folder,
				 FolderItem	*dest,
				 MsgInfo	*msginfo);
gint     mh_copy_msgs_with_dest	(Folder		*folder,
				 FolderItem	*dest,
				 GSList		*msglist);
gint     mh_remove_msg		(Folder		*folder,
				 FolderItem	*item,
				 gint		 num);
gint     mh_remove_all_msg	(Folder		*folder,
				 FolderItem	*item);
gboolean mh_is_msg_changed	(Folder		*folder,
				 FolderItem	*item,
				 MsgInfo	*msginfo);

void    mh_scan_folder		(Folder		*folder,
				 FolderItem	*item);
void    mh_scan_tree		(Folder		*folder);

gint    mh_create_tree		(Folder		*folder);
FolderItem *mh_create_folder	(Folder		*folder,
				 FolderItem	*parent,
				 const gchar	*name);
gint    mh_rename_folder	(Folder		*folder,
				 FolderItem	*item,
				 const gchar	*name);
gint    mh_remove_folder	(Folder		*folder,
				 FolderItem	*item);

#endif /* __MH_H__ */
