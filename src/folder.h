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

#ifndef __FOLDER_H__
#define __FOLDER_H__

#include <glib.h>
#include <time.h>

typedef struct _Folder		Folder;
typedef struct _LocalFolder	LocalFolder;
typedef struct _RemoteFolder	RemoteFolder;
typedef struct _MHFolder	MHFolder;
typedef struct _MboxFolder	MboxFolder;
typedef struct _MaildirFolder	MaildirFolder;
typedef struct _IMAPFolder	IMAPFolder;
typedef struct _NewsFolder	NewsFolder;
typedef struct _FolderItem	FolderItem;

#include "prefs_account.h"
#include "session.h"
#include "procmsg.h"
#include "prefs_folder_item.h"

#define FOLDER(obj)		((Folder *)obj)
#define FOLDER_TYPE(obj)	(FOLDER(obj)->type)

#define LOCAL_FOLDER(obj)	((LocalFolder *)obj)
#define REMOTE_FOLDER(obj)	((RemoteFolder *)obj)

#define FOLDER_IS_LOCAL(obj)	(FOLDER_TYPE(obj) == F_MH      || \
				 FOLDER_TYPE(obj) == F_MBOX    || \
				 FOLDER_TYPE(obj) == F_MAILDIR)

#define MH_FOLDER(obj)		((MHFolder *)obj)
#define MBOX_FOLDER(obj)	((MboxFolder *)obj)
#define MAILDIR_FOLDER(obj)	((MaildirFolder *)obj)
#define IMAP_FOLDER(obj)	((IMAPFolder *)obj)
#define NEWS_FOLDER(obj)	((NewsFolder *)obj)

#define FOLDER_ITEM(obj)	((FolderItem *)obj)

typedef enum
{
	F_MH,
	F_MBOX,
	F_MAILDIR,
	F_IMAP,
	F_NEWS,
	F_UNKNOWN
} FolderType;

typedef enum
{
	F_NORMAL,
	F_INBOX,
	F_OUTBOX,
	F_DRAFT,
	F_QUEUE,
	F_TRASH
} SpecialFolderItemType;

typedef void (*FolderUIFunc)		(Folder		*folder,
					 FolderItem	*item,
					 gpointer	 data);
typedef void (*FolderDestroyNotify)	(Folder		*folder,
					 FolderItem	*item,
					 gpointer	 data);

struct _Folder
{
	FolderType type;

	gchar *name;
	PrefsAccount *account;

	FolderItem *inbox;
	FolderItem *outbox;
	FolderItem *draft;
	FolderItem *queue;
	FolderItem *trash;

	FolderUIFunc ui_func;
	gpointer     ui_func_data;

	GNode *node;

	gpointer data;

	/* virtual functions */
	GSList * (*get_msg_list)	(Folder		*folder,
					 FolderItem	*item,
					 gboolean	 use_cache);
	gchar *  (*fetch_msg)		(Folder		*folder,
					 FolderItem	*item,
					 gint		 num);
	gint     (*add_msg)		(Folder		*folder,
					 FolderItem	*dest,
					 const gchar	*file,
					 gboolean	 remove_source);
	gint     (*move_msg)		(Folder		*folder,
					 FolderItem	*dest,
					 MsgInfo	*msginfo);
	gint     (*move_msgs_with_dest)	(Folder		*folder,
					 FolderItem	*dest,
					 GSList		*msglist);
	gint     (*copy_msg)		(Folder		*folder,
					 FolderItem	*dest,
					 MsgInfo	*msginfo);
	gint     (*copy_msgs_with_dest)	(Folder		*folder,
					 FolderItem	*dest,
					 GSList		*msglist);
	gint     (*remove_msg)		(Folder		*folder,
					 FolderItem	*item,
					 gint		 num);
	gint     (*remove_all_msg)	(Folder		*folder,
					 FolderItem	*item);
	gboolean (*is_msg_changed)	(Folder		*folder,
					 FolderItem	*item,
					 MsgInfo	*msginfo);
	void     (*scan)		(Folder		*folder,
					 FolderItem	*item);
	void     (*scan_tree)		(Folder		*folder);

	gint     (*create_tree)		(Folder		*folder);
	FolderItem * (*create_folder)	(Folder		*folder,
					 FolderItem	*parent,
					 const gchar	*name);
	gint     (*rename_folder)	(Folder		*folder,
					 FolderItem	*item,
					 const gchar	*name);
	gint     (*remove_folder)	(Folder		*folder,
					 FolderItem	*item);
	void     (*update_mark)		(Folder		*folder,
					 FolderItem	*item);
	void     (*change_flags)	(Folder		*folder,
					 FolderItem	*item,
					 MsgInfo        *info);
	void     (*finished_copy)       (Folder * folder, FolderItem * item);
	void     (*finished_remove)       (Folder * folder, FolderItem * item);
};

struct _LocalFolder
{
	Folder folder;

	gchar *rootpath;
};

struct _RemoteFolder
{
	Folder folder;

	Session *session;
};

struct _MHFolder
{
	LocalFolder lfolder;
};

struct _MboxFolder
{
	LocalFolder lfolder;
};

struct _MaildirFolder
{
	LocalFolder lfolder;
};

struct _IMAPFolder
{
	RemoteFolder rfolder;

	GList *namespace;	/* list of IMAPNameSpace */
};

struct _NewsFolder
{
	RemoteFolder rfolder;

	gboolean use_auth;
};

struct _FolderItem
{
	SpecialFolderItemType stype;

	gchar *name;
	gchar *path;
	PrefsAccount *account;

	time_t mtime;

	gint new;
	gint unread;
	gint total;

	gint last_num;

	/* special flags */
	guint no_sub    : 1; /* no child allowed?    */
	guint no_select : 1; /* not selectable?      */
	guint collapsed : 1; /* collapsed item       */
	guint threaded  : 1; /* threaded folder view */
	guint ret_rcpt  : 1; /* return receipt       */

	gint op_count;

	FolderItem *parent;

	Folder *folder;

	gpointer data;

	PrefsFolderItem * prefs;
};

Folder     *folder_new		(FolderType	 type,
				 const gchar	*name,
				 const gchar	*path);
Folder     *mh_folder_new	(const gchar	*name,
				 const gchar	*path);
Folder     *mbox_folder_new	(const gchar	*name,
				 const gchar	*path);
Folder     *maildir_folder_new	(const gchar	*name,
				 const gchar	*path);
Folder     *imap_folder_new	(const gchar	*name,
				 const gchar	*path);
Folder     *news_folder_new	(const gchar	*name,
				 const gchar	*path);
FolderItem *folder_item_new	(const gchar	*name,
				 const gchar	*path);
void        folder_item_append	(FolderItem	*parent,
				 FolderItem	*item);
void        folder_item_remove	(FolderItem	*item);
void        folder_item_destroy	(FolderItem	*item);
void        folder_set_ui_func	(Folder		*folder,
				 FolderUIFunc	 func,
				 gpointer	 data);
void        folder_set_name	(Folder		*folder,
				 const gchar	*name);
void        folder_destroy	(Folder		*folder);
void        folder_tree_destroy	(Folder		*folder);

void   folder_add		(Folder		*folder);

GList *folder_get_list		(void);
gint   folder_read_list		(void);
void   folder_write_list	(void);
void   folder_update_op_count	(void);

Folder     *folder_find_from_path	(const gchar	*path);
FolderItem *folder_find_item_from_path	(const gchar	*path);
Folder     *folder_get_default_folder	(void);
FolderItem *folder_get_default_inbox	(void);
FolderItem *folder_get_default_outbox	(void);
FolderItem *folder_get_default_draft	(void);
FolderItem *folder_get_default_queue	(void);
FolderItem *folder_get_default_trash	(void);

gchar *folder_item_get_path		(FolderItem	*item);
void   folder_item_scan			(FolderItem	*item);
void   folder_item_scan_foreach		(GHashTable	*table);
gchar *folder_item_fetch_msg		(FolderItem	*item,
					 gint		 num);
gint   folder_item_add_msg		(FolderItem	*dest,
					 const gchar	*file,
					 gboolean	 remove_source);
gint   folder_item_move_msg		(FolderItem	*dest,
					 MsgInfo	*msginfo);
gint   folder_item_move_msgs_with_dest	(FolderItem	*dest,
					 GSList		*msglist);
gint   folder_item_copy_msg		(FolderItem	*dest,
					 MsgInfo	*msginfo);
gint   folder_item_copy_msgs_with_dest	(FolderItem	*dest,
					 GSList		*msglist);
gint   folder_item_remove_msg		(FolderItem	*item,
					 gint		 num);
gint   folder_item_remove_all_msg	(FolderItem	*item);
gboolean folder_item_is_msg_changed	(FolderItem	*item,
					 MsgInfo	*msginfo);
gchar *folder_item_get_cache_file	(FolderItem	*item);
gchar *folder_item_get_mark_file	(FolderItem	*item);
gchar * folder_item_get_identifier(FolderItem * item);
FolderItem * folder_find_item_from_identifier(const gchar *identifier);

#endif /* __FOLDER_H__ */
