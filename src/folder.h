/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2003 Hiroyuki Yamamoto
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
typedef struct _FolderClass	FolderClass;

typedef struct _FolderItem	FolderItem;
typedef struct _FolderUpdateData	FolderUpdateData;
typedef struct _FolderItemUpdateData	FolderItemUpdateData;
typedef struct _PersistPrefs		PersistPrefs;

#define FOLDER(obj)		((Folder *)obj)
#define FOLDER_CLASS(obj)	(FOLDER(obj)->klass)
#define FOLDER_TYPE(obj)	(FOLDER(obj)->klass->type)

#define FOLDER_IS_LOCAL(obj)	(FOLDER_TYPE(obj) == F_MH      || \
				 FOLDER_TYPE(obj) == F_MBOX    || \
				 FOLDER_TYPE(obj) == F_MAILDIR)

#define FOLDER_ITEM(obj)	((FolderItem *)obj)

#define FOLDER_UPDATE_HOOKLIST "folder_update"
#define FOLDER_ITEM_UPDATE_HOOKLIST "folder_item_update"

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
	SORT_BY_STATUS,
	SORT_BY_MIME,
	SORT_BY_TO,
	SORT_BY_LOCKED
} FolderSortKey;

typedef enum
{
	SORT_ASCENDING,
	SORT_DESCENDING
} FolderSortType;

typedef enum
{
	F_MOVE_OK = 0,
	F_MOVE_FAILED_DEST_IS_PARENT = -1,
	F_MOVE_FAILED_DEST_IS_CHILD = -2,
	F_MOVE_FAILED_DEST_OUTSIDE_MAILBOX = -3,
	F_MOVE_FAILED = -4,
} FolderItemMoveResult;

typedef enum
{
	FOLDER_NEW_FOLDER 		= 1 << 0,
	FOLDER_DESTROY_FOLDER 		= 1 << 1,
	FOLDER_TREE_CHANGED 		= 1 << 2,
	FOLDER_NEW_FOLDERITEM 		= 1 << 3,
	FOLDER_REMOVE_FOLDERITEM 	= 1 << 4,
} FolderUpdateFlags;

typedef enum
{
	F_ITEM_UPDATE_MSGCNT = 1 << 0,
	F_ITEM_UPDATE_CONTENT = 1 << 1,
} FolderItemUpdateFlags;

typedef void (*FolderUIFunc)		(Folder		*folder,
					 FolderItem	*item,
					 gpointer	 data);
typedef void (*FolderDestroyNotify)	(Folder		*folder,
					 FolderItem	*item,
					 gpointer	 data);
typedef void (*FolderItemFunc)		(FolderItem	*item,
					 gpointer	 data);


#include "folder_item_prefs.h"

#include "procmsg.h"
#include "msgcache.h"
#include "xml.h"
#include "prefs_account.h"

struct _Folder
{
	FolderClass *klass;

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
};

struct _FolderClass
{
	/**
	 * A numeric identifier for the FolderClass. Will be removed in the future
	 */
	FolderType  type;
	/**
	 * A string identifier for the FolderClass. Currently used in folderlist.xml.
	 * Should be lowercase.
	 */
	gchar 	   *idstr;
	/**
	 * A string for the User Interface that identifies the FolderClass to the
	 * user. Can be upper and lowercase unlike the idstr.
	 */
	gchar	   *uistr;

	/* virtual functions */

	/* Folder funtions */
	/**
	 * Create a new Folder of this FolderClass
	 *
	 * \param name Name of the new Folder
	 * \param path Path of the new Folder
	 * \return The new Folder, or NULL when creating the Folder failed
	 */
	Folder 		*(*new_folder)		(const gchar	*name,
						 const gchar	*path);
	/**
	 * Destroy a Folder of this FolderClass, frees all resources allocated by
         * the Folder
	 *
	 * \param folder The folder that should be destroyed.
	 */
	void     	(*destroy_folder)	(Folder		*folder);
	/**
	 * Set the Folder's internal settings from an XMLTag. Also sets the
	 * parameters of the root-FolderItem of the Folder. If NULL
	 * the default function of the basic FolderClass is used, so it
	 * must not be NULL if one of the parent FolderClasses has a set_xml
	 * function. In that case the parent FolderClass' set_xml function
	 * can be used or it has to be called with the Folder and XMLTag by
	 * the implementation.
	 *
	 * \param folder The Folder which's settings should be updated
	 * \param tag The XMLTag containing the settings attributes
	 */
	void		 (*set_xml)		(Folder		*folder,
						 XMLTag		*tag);
	/**
	 * Get an \c XMLTag for the settings of the Folder and the root-FolderItem
	 * of the Folder. If \c NULL the default implementation for the basic
	 * FolderClass will be used, so it must not be NULL if one of the
	 * parent \c FolderClasses has it's own implementation for \c get_xml.
	 * In that case the parent FolderClass' \c get_xml function can be
	 * used or the \c XMLTag has to be fetched from the parent's \c get_xml
	 * function and then the \c FolderClass specific attributes can be
	 * added to it.
	 *
	 * \param Folder The \c Folder which's settings should be set in the
	 *               \c XMLTag's attributes
	 * \return XMLTag An \c XMLTag with attibutes containing the \c Folder's
	 *                settings.
	 */
	XMLTag		*(*get_xml)		(Folder		*folder);
	gint     	(*scan_tree)		(Folder		*folder);

	gint     	(*create_tree)		(Folder		*folder);

	/* FolderItem functions */
	FolderItem	*(*item_new)		(Folder		*folder);
	void	 	 (*item_destroy)	(Folder		*folder,
						 FolderItem	*item);
	void		 (*item_set_xml)	(Folder		*folder,
						 FolderItem	*item,
						 XMLTag		*tag);
	XMLTag		*(*item_get_xml)	(Folder		*folder,
						 FolderItem	*item);
	gchar		*(*item_get_path)	(Folder		*folder,
						 FolderItem	*item);
	FolderItem 	*(*create_folder)	(Folder		*folder,
						 FolderItem	*parent,
						 const gchar	*name);
	gint     	 (*rename_folder)	(Folder		*folder,
						 FolderItem	*item,
						 const gchar	*name);
	gint     	 (*remove_folder)	(Folder		*folder,
						 FolderItem	*item);
	gint		 (*close)		(Folder		*folder,
						 FolderItem	*item);
	gint	 	 (*get_num_list)	(Folder		*folder,
						 FolderItem	*item,
						 GSList	       **list,
						 gboolean	*old_uids_valid);
	void     	(*update_mark)		(Folder		*folder,
						 FolderItem	*item);
	void    	(*finished_copy)        (Folder 	*folder,
						 FolderItem 	*item);
	void    	(*finished_remove)      (Folder 	*folder,
						 FolderItem 	*item);
	gboolean	(*scan_required)	(Folder 	*folder,
						 FolderItem 	*item);

	/* Message functions */
	MsgInfo 	*(*get_msginfo)		(Folder		*folder,
						 FolderItem	*item,
						 gint		 num);
	GSList  	*(*get_msginfos)	(Folder		*folder,
						 FolderItem	*item,
						 MsgNumberList	*msgnum_list);
	gchar 		*(*fetch_msg)		(Folder		*folder,
						 FolderItem	*item,
						 gint		 num);
	gint     	(*add_msg)		(Folder		*folder,
						 FolderItem	*dest,
						 const gchar	*file,
						 MsgFlags	*flags);
	gint     	(*add_msgs)             (Folder         *folder,
                                    		 FolderItem     *dest,
                                    		 GSList         *file_list,
                                    		 GRelation	*relation);
	/**
	 * Copy a message to a FolderItem
	 *
	 * \param folder The \c Folder of the destination FolderItem
	 * \param dest The destination \c FolderItem for the message
	 * \param msginfo The message that should be copied
	 * \return The message number the copied message got, 0 if it is
	 *         unknown because message numbers are assigned by an external
	 *         system and not available after copying or a negative number
	 *         if an error occuried
	 */
	gint    	(*copy_msg)		(Folder		*folder,
						 FolderItem	*dest,
						 MsgInfo	*msginfo);
	/**
	 * Copy multiple messages to a \c FolderItem. If \c NULL the folder system
	 * will use \c copy_msg to copy messages one by one.
	 *
	 * \param folder The \c Folder of the destination FolderItem
	 * \param dest The destination \c FolderItem for the message
	 * \param msglist A list of \c MsgInfos which should be copied to dest
	 * \param relation Insert tuples of (MsgInfo, new message number) to
	 *                 provide feedback for the folder system which new
	 *                 message number a \c MsgInfo has got in dest. Insert
	 *                 0 for the message number of unknown.
	 * \return 0 on success, a negative number otherwise
	 */
	gint    	(*copy_msgs)		(Folder		*folder,
						 FolderItem	*dest,
						 MsgInfoList	*msglist,
                                    		 GRelation	*relation);
	gint    	(*remove_msg)		(Folder		*folder,
						 FolderItem	*item,
						 gint		 num);
	gint    	(*remove_all_msg)	(Folder		*folder,
						 FolderItem	*item);
	gboolean	(*is_msg_changed)	(Folder		*folder,
						 FolderItem	*item,
						 MsgInfo	*msginfo);
	/**
	 * Update a message's flags in the folder data. If NULL only the
	 * internal flag management will be used. The function has to set
	 * \c msginfo->flags.perm_flags. It does not have to set the flags
	 * that it got as \c newflags. If a flag can not be set in this
	 * \c FolderClass the function can refuse to set it. Flags that are not
	 * supported by the \c FolderClass should not be refused. They will be
	 * managed by the internal cache in this case.
	 *
	 * \param folder The \c Folder of the message
	 * \param item The \c FolderItem of the message
	 * \param msginfo The \c MsgInfo for the message which's flags should be
	 *                updated
	 * \param newflags The flags the message should get
	 */
	void    	(*change_flags)		(Folder		*folder,
						 FolderItem	*item,
						 MsgInfo        *msginfo,
						 MsgPermFlags	 newflags);
};

struct _FolderItem
{
	SpecialFolderItemType stype;

	gchar *name;
	gchar *path;

	time_t mtime;

	gint new_msgs;
	gint unread_msgs;
	gint total_msgs;
	gint unreadmarked_msgs;

	gint last_num;

	MsgCache *cache;

	/* special flags */
	guint no_sub         : 1; /* no child allowed?    */
	guint no_select      : 1; /* not selectable?      */
	guint collapsed      : 1; /* collapsed item       */
	guint thread_collapsed      : 1; /* collapsed item       */
	guint threaded       : 1; /* threaded folder view */
	guint hide_read_msgs : 1; /* hide read messages   */
	guint ret_rcpt       : 1; /* return receipt       */

	gint op_count;
	guint opened         : 1; /* opened by summary view */
	FolderItemUpdateFlags update_flags; /* folderview for this folder should be updated */

	FolderSortKey sort_key;
	FolderSortType sort_type;

	GNode *node;

	Folder *folder;

	PrefsAccount *account;

	gboolean apply_sub;
	
	GSList *mark_queue;

	gpointer data;
 
 #ifdef WIN32
 	int n_child;
 #endif

	FolderItemPrefs * prefs;
};

struct _PersistPrefs
{
	FolderSortKey	sort_key;
	FolderSortType	sort_type;
	guint		collapsed	: 1;
	guint		thread_collapsed	: 1;
	guint		threaded	: 1;
	guint		hide_read_msgs	: 1; /* CLAWS */
	guint		ret_rcpt	: 1; /* CLAWS */
};

struct _FolderUpdateData
{
	Folder			*folder;
	FolderUpdateFlags	 update_flags;
	FolderItem		*item;
};

struct _FolderItemUpdateData
{
	FolderItem		*item;
	FolderItemUpdateFlags	 update_flags;
};

void	    folder_system_init		(void);
void	    folder_register_class	(FolderClass	*klass);
void	    folder_unregister_class	(FolderClass	*klass);
Folder     *folder_new			(FolderClass	*type,
					 const gchar	*name,
					 const gchar	*path);
void 	    folder_init			(Folder		*folder,
					 const gchar	*name);

void        folder_destroy		(Folder		*folder);

void 	    folder_set_xml		(Folder		 *folder,
					 XMLTag		 *tag);
XMLTag 	   *folder_get_xml		(Folder		 *folder);

FolderItem *folder_item_new		(Folder		*folder,
				 	 const gchar	*name,
				 	 const gchar	*path);
void        folder_item_append		(FolderItem	*parent,
				 	 FolderItem	*item);
void        folder_item_remove		(FolderItem	*item);
void        folder_item_remove_children	(FolderItem	*item);
void        folder_item_destroy		(FolderItem	*item);
FolderItem *folder_item_parent		(FolderItem	*item);

void 	    folder_item_set_xml		(Folder		 *folder,
					 FolderItem	 *item,
					 XMLTag		 *tag);
XMLTag 	   *folder_item_get_xml		(Folder		 *folder,
					 FolderItem	 *item);

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
void   folder_count_total_msgs	(guint		*new_msgs,
				 guint		*unread_msgs,
				 guint		*unreadmarked_msgs,
				 guint		*total_msgs);
gchar *folder_get_status	(GPtrArray	*folders,
				 gboolean	 full);

Folder     *folder_find_from_path		(const gchar	*path);
Folder     *folder_find_from_name		(const gchar	*name,
						 FolderClass	*klass);
FolderItem *folder_find_item_from_path		(const gchar	*path);
FolderClass *folder_get_class_from_string	(const gchar 	*str);
FolderItem *folder_find_child_item_by_name	(FolderItem	*item,
						 const gchar	*name);
gchar      *folder_get_identifier		(Folder		*folder);
gchar      *folder_item_get_identifier		(FolderItem	*item);
FolderItem *folder_find_item_from_identifier	(const gchar	*identifier);
gchar 	   *folder_item_get_name		(FolderItem 	*item);

Folder     *folder_get_default_folder	(void);
FolderItem *folder_get_default_inbox	(void);
FolderItem *folder_get_default_outbox	(void);
FolderItem *folder_get_default_draft	(void);
FolderItem *folder_get_default_queue	(void);
FolderItem *folder_get_default_trash	(void);
FolderItem *folder_get_default_processing (void);
void folder_set_missing_folders		(void);
void folder_unref_account_all		(PrefsAccount	*account);

gchar *folder_item_get_path		(FolderItem	*item);

gint   folder_item_open			(FolderItem	*item);
gint   folder_item_close		(FolderItem	*item);
gint   folder_item_scan			(FolderItem	*item);
void   folder_item_scan_foreach		(GHashTable	*table);
MsgInfo *folder_item_get_msginfo	(FolderItem 	*item,
					 gint		 num);
MsgInfo *folder_item_get_msginfo_by_msgid(FolderItem 	*item,
					 const gchar 	*msgid);
GSList *folder_item_get_msg_list	(FolderItem 	*item);
gchar *folder_item_fetch_msg		(FolderItem	*item,
					 gint		 num);
gint   folder_item_fetch_all_msg	(FolderItem	*item);
gint   folder_item_add_msg		(FolderItem	*dest,
					 const gchar	*file,
					 MsgFlags	*flags,
					 gboolean	 remove_source);
gint   folder_item_add_msgs             (FolderItem     *dest,
                                         GSList         *file_list,
                                         gboolean        remove_source);
gint   folder_item_move_to		(FolderItem	*src,
					 FolderItem	*dest,
					 FolderItem    **new_item);
gint   folder_item_move_msg		(FolderItem	*dest,
					 MsgInfo	*msginfo);
gint   folder_item_move_msgs		(FolderItem	*dest,
					 GSList		*msglist);
gint   folder_item_copy_msg		(FolderItem	*dest,
					 MsgInfo	*msginfo);
gint   folder_item_copy_msgs		(FolderItem	*dest,
					 GSList		*msglist);
gint   folder_item_remove_msg		(FolderItem	*item,
					 gint		 num);
gint   folder_item_remove_msgs		(FolderItem	*item,
					 GSList		*msglist);
gint   folder_item_remove_all_msg	(FolderItem	*item);
void 	folder_item_change_msg_flags	(FolderItem 	*item,
					 MsgInfo 	*msginfo,
					 MsgPermFlags 	 newflags);
gboolean folder_item_is_msg_changed	(FolderItem	*item,
					 MsgInfo	*msginfo);
gchar *folder_item_get_cache_file	(FolderItem	*item);
gchar *folder_item_get_mark_file	(FolderItem	*item);
gchar * folder_item_get_identifier	(FolderItem * item);

GHashTable *folder_persist_prefs_new	(Folder *folder);
void folder_persist_prefs_free		(GHashTable *pptable);
const PersistPrefs *folder_get_persist_prefs
					(GHashTable *pptable, const char *name);

void folder_item_restore_persist_prefs	(FolderItem *item, GHashTable *pptable);
void folder_clean_cache_memory		(void);
void folder_item_write_cache		(FolderItem *item);
void folder_item_set_default_flags	(FolderItem *dest, MsgFlags *flags);

void folder_item_apply_processing	(FolderItem *item);

void folder_item_update			(FolderItem *item,
					 FolderItemUpdateFlags update_flags);
void folder_item_update_recursive	(FolderItem *item,
					 FolderItemUpdateFlags update_flags);
void folder_item_update_freeze		(void);
void folder_item_update_thaw		(void);

#endif /* __FOLDER_H__ */
