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

#ifndef __FOLDER_H__
#define __FOLDER_H__

#include <glib.h>
#include <time.h>

typedef struct _Folder		Folder;
typedef struct _LocalFolder	LocalFolder;
typedef struct _RemoteFolder	RemoteFolder;
#if 0
typedef struct _MaildirFolder	MaildirFolder;
#endif
typedef struct _FolderItem	FolderItem;

#include "prefs_folder_item.h"

#include "prefs_account.h"
#include "session.h"
#include "procmsg.h"
#include "msgcache.h"

#define FOLDER(obj)		((Folder *)obj)
#define FOLDER_TYPE(obj)	(FOLDER(obj)->type)

#define LOCAL_FOLDER(obj)	((LocalFolder *)obj)
#define REMOTE_FOLDER(obj)	((RemoteFolder *)obj)

#define FOLDER_IS_LOCAL(obj)	(FOLDER_TYPE(obj) == F_MH      || \
				 FOLDER_TYPE(obj) == F_MBOX    || \
				 FOLDER_TYPE(obj) == F_MAILDIR)

#if 0
#define MAILDIR_FOLDER(obj)	((MaildirFolder *)obj)
#endif

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

typedef enum
{
	SORT_BY_NONE,
	SORT_BY_NUMBER,
	SORT_BY_SIZE,
	SORT_BY_DATE,
	SORT_BY_FROM,
	SORT_BY_SUBJECT,
	SORT_BY_SCORE,
	SORT_BY_LABEL,
	SORT_BY_MARK,
	SORT_BY_UNREAD,
	SORT_BY_MIME,
	SORT_BY_LOCKED
} FolderSortKey;

typedef enum
{
	SORT_ASCENDING,
	SORT_DESCENDING
} FolderSortType;

typedef void (*FolderUIFunc)		(Folder		*folder,
					 FolderItem	*item,
					 gpointer	 data);
typedef void (*FolderDestroyNotify)	(Folder		*folder,
					 FolderItem	*item,
					 gpointer	 data);
typedef void (*FolderItemFunc)		(FolderItem	*item,
					 gpointer	 data);
typedef void (*FolderItemUpdateFunc)	(FolderItem	*item,
					 gboolean	 contentchange,
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

	GHashTable *newsart;

	/* virtual functions */
/*
	GSList * (*get_msg_list)	(Folder		*folder,
					 FolderItem	*item,
					 gboolean	 use_cache);
*/
	FolderItem *(*item_new)		(Folder		*folder);
	void	 (*item_destroy)	(Folder		*folder,
					 FolderItem	*item);
	gchar *  (*fetch_msg)		(Folder		*folder,
					 FolderItem	*item,
					 gint		 num);
	MsgInfo *(*fetch_msginfo)	(Folder		*folder,
					 FolderItem	*item,
					 gint		 num);
	GSList  *(*fetch_msginfos)	(Folder		*folder,
					 FolderItem	*item,
					 GSList		*msgnum_list);
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
	gint     (*remove_msgs)		(Folder		*folder,
					 FolderItem	*item,
					 GSList		*msglist);
	gint     (*remove_all_msg)	(Folder		*folder,
					 FolderItem	*item);
	gboolean (*is_msg_changed)	(Folder		*folder,
					 FolderItem	*item,
					 MsgInfo	*msginfo);
	gint     (*scan)		(Folder		*folder);
	gint	 (*get_num_list)	(Folder		*folder,
					 FolderItem	*item,
					 GSList	       **list);
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

	void     (*destroy)		(Folder		*folder);
	void     (*update_mark)		(Folder		*folder,
					 FolderItem	*item);
	void     (*change_flags)	(Folder		*folder,
					 FolderItem	*item,
					 MsgInfo        *info);
	void     (*finished_copy)       (Folder * folder, FolderItem * item);
	void     (*finished_remove)     (Folder * folder, FolderItem * item);
	gboolean (*check_msgnum_validity) (Folder * folder, FolderItem * item);
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

#if 0
struct _MaildirFolder
{
	LocalFolder lfolder;
};
#endif

struct _FolderItem
{
	SpecialFolderItemType stype;

	gchar *name;
	gchar *path;

	time_t mtime;

	gint new;
	gint unread;
	gint total;

	gint last_num;

	MsgCache *cache;

	/* special flags */
	guint no_sub         : 1; /* no child allowed?    */
	guint no_select      : 1; /* not selectable?      */
	guint collapsed      : 1; /* collapsed item       */
	guint threaded       : 1; /* threaded folder view */
	guint hide_read_msgs : 1; /* hide read messages   */
	guint ret_rcpt       : 1; /* return receipt       */

	gint op_count;
	guint opened    : 1; /* opened by summary view */
	guint need_update    : 1; /* folderview for this folder should be updated */

	FolderSortKey sort_key;
	FolderSortType sort_type;

	FolderItem *parent;

	Folder *folder;

	PrefsAccount *account;

	gboolean apply_sub;
	
	GSList *mark_queue;

	gpointer data;

	PrefsFolderItem * prefs;
};

typedef struct {
	FolderSortKey	sort_key;
	FolderSortType	sort_type;
	guint		collapsed	: 1;
	guint		threaded	: 1;
	guint		hide_read_msgs	: 1; /* CLAWS */
	guint		ret_rcpt	: 1; /* CLAWS */
} PersistPrefs;

Folder     *folder_new			(FolderType	 type,
					 const gchar	*name,
					 const gchar	*path);
void        folder_local_folder_init	(Folder		*folder,
					 const gchar	*name,
					 const gchar	*path);
void        folder_remote_folder_init	(Folder		*folder,
					 const gchar	*name,
					 const gchar	*path);

void        folder_destroy		(Folder		*folder);
void        folder_local_folder_destroy	(LocalFolder	*lfolder);
void        folder_remote_folder_destroy(RemoteFolder	*rfolder);

FolderItem *folder_item_new	(Folder		*folder,
				 const gchar	*name,
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
void        folder_tree_destroy	(Folder		*folder);

void   folder_add		(Folder		*folder);

GList *folder_get_list		(void);
gint   folder_read_list		(void);
void   folder_write_list	(void);
void   folder_scan_tree		(Folder *folder);
FolderItem *folder_create_folder(FolderItem	*parent, const gchar *name);
void   folder_update_op_count		(void);
void   folder_func_to_all_folders	(FolderItemFunc function,
					 gpointer data);
void   folder_count_total_msgs	(guint		*new,
				 guint		*unread,
				 guint		*total);

Folder     *folder_find_from_path		(const gchar	*path);
Folder     *folder_find_from_name		(const gchar	*name,
						 FolderType	 type);
FolderItem *folder_find_item_from_path		(const gchar	*path);
gchar      *folder_get_identifier		(Folder		*folder);
gchar      *folder_item_get_identifier		(FolderItem	*item);
FolderItem *folder_find_item_from_identifier	(const gchar	*identifier);

Folder     *folder_get_default_folder	(void);
FolderItem *folder_get_default_inbox	(void);
FolderItem *folder_get_default_outbox	(void);
FolderItem *folder_get_default_draft	(void);
FolderItem *folder_get_default_queue	(void);
FolderItem *folder_get_default_trash	(void);
FolderItem *folder_get_default_processing (void);
void folder_set_missing_folders		(void);
void folder_unref_account_all		(PrefsAccount	*account);

gchar *folder_get_path			(Folder		*folder);
gchar *folder_item_get_path		(FolderItem	*item);

gint   folder_item_open			(FolderItem	*item);
void   folder_item_close		(FolderItem	*item);
gint   folder_item_scan			(FolderItem	*item);
void   folder_item_scan_foreach		(GHashTable	*table);
MsgInfo *folder_item_fetch_msginfo	(FolderItem 	*item,
					 gint		 num);
MsgInfo *folder_item_fetch_msginfo_by_id(FolderItem 	*item,
					 const gchar 	*msgid);
GSList *folder_item_get_msg_list	(FolderItem 	*item);
gchar *folder_item_fetch_msg		(FolderItem	*item,
					 gint		 num);
gint   folder_item_add_msg		(FolderItem	*dest,
					 const gchar	*file,
					 gboolean	 remove_source);
FolderItem   *folder_item_move_to	(FolderItem	*src,
					 FolderItem	*dest);
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
gint   folder_item_remove_msgs		(FolderItem	*item,
					 GSList		*msglist);
gint   folder_item_remove_all_msg	(FolderItem	*item);
gboolean folder_item_is_msg_changed	(FolderItem	*item,
					 MsgInfo	*msginfo);
gchar *folder_item_get_cache_file	(FolderItem	*item);
gchar *folder_item_get_mark_file	(FolderItem	*item);
gchar * folder_item_get_identifier(FolderItem * item);

GHashTable *folder_persist_prefs_new	(Folder *folder);
void folder_persist_prefs_free		(GHashTable *pptable);
const PersistPrefs *folder_get_persist_prefs
					(GHashTable *pptable, const char *name);

void folder_item_restore_persist_prefs	(FolderItem *item, GHashTable *pptable);
void folder_clean_cache_memory		();
void folder_item_write_cache		(FolderItem *item);
void folder_item_set_default_flags	(FolderItem *dest, MsgFlags *flags);

void folder_item_apply_processing	(FolderItem *item);

void folder_update_item			(FolderItem *item,
					 gboolean contentchange);
void folder_update_items_when_required	(gboolean contentchange);
void folder_update_item_recursive	(FolderItem *item,
					 gboolean update_summary);
gint folder_item_update_callback_register(FolderItemUpdateFunc func,
					  gpointer data);
void folder_item_update_callback_unregister(gint id);

#endif /* __FOLDER_H__ */
