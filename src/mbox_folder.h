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

#endif
