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

#ifndef __IMAP_H__
#define __IMAP_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <time.h>

#if USE_SSL
#  include "ssl.h"
#endif
#include "folder.h"
#include "session.h"
#include "procmsg.h"

typedef struct _IMAPFolder	IMAPFolder;
typedef struct _IMAPSession	IMAPSession;
typedef struct _IMAPNameSpace	IMAPNameSpace;

#include "prefs_account.h"

#define IMAP_FOLDER(obj)	((IMAPFolder *)obj)
#define IMAP_SESSION(obj)	((IMAPSession *)obj)

struct _IMAPFolder
{
	RemoteFolder rfolder;

	/* list of IMAPNameSpace */
	GList *ns_personal;
	GList *ns_others;
	GList *ns_shared;
};

struct _IMAPSession
{
	Session session;

	gchar *mbox;
	time_t last_access_time;
};

struct _IMAPNameSpace
{
	gchar *name;
	gchar separator;
};

#define IMAP_SUCCESS	0
#define IMAP_SOCKET	2
#define IMAP_AUTHFAIL	3
#define IMAP_PROTOCOL	4
#define IMAP_SYNTAX	5
#define IMAP_IOERR	6
#define IMAP_ERROR	7

#define IMAPBUFSIZE	8192

typedef enum
{
	IMAP_FLAG_SEEN		= 1 << 0,
	IMAP_FLAG_ANSWERED	= 1 << 1,
	IMAP_FLAG_FLAGGED	= 1 << 2,
	IMAP_FLAG_DELETED	= 1 << 3,
	IMAP_FLAG_DRAFT		= 1 << 4
} IMAPFlags;

#define IMAP_IS_SEEN(flags)	((flags & IMAP_FLAG_SEEN) != 0)
#define IMAP_IS_ANSWERED(flags)	((flags & IMAP_FLAG_ANSWERED) != 0)
#define IMAP_IS_FLAGGED(flags)	((flags & IMAP_FLAG_FLAGGED) != 0)
#define IMAP_IS_DELETED(flags)	((flags & IMAP_FLAG_DELETED) != 0)
#define IMAP_IS_DRAFT(flags)	((flags & IMAP_FLAG_DRAFT) != 0)

Folder	*imap_folder_new		(const gchar	*name,
					 const gchar	*path);
void	 imap_folder_destroy		(IMAPFolder	*folder);

Session *imap_session_new		(const PrefsAccount *account);
void imap_session_destroy		(IMAPSession	*session);
void imap_session_destroy_all		(void);

GSList *imap_get_msg_list		(Folder		*folder,
					 FolderItem	*item,
					 gboolean	 use_cache);
gchar *imap_fetch_msg			(Folder		*folder,
					 FolderItem	*item,
					 gint		 uid);
gint imap_add_msg			(Folder		*folder,
					 FolderItem	*dest,
					 const gchar	*file,
					 gboolean	 remove_source);

gint imap_move_msg			(Folder		*folder,
					 FolderItem	*dest,
					 MsgInfo	*msginfo);
gint imap_move_msgs_with_dest		(Folder		*folder,
					 FolderItem	*dest,
					 GSList		*msglist);
gint imap_copy_msg			(Folder		*folder,
					 FolderItem	*dest,
					 MsgInfo	*msginfo);
gint imap_copy_msgs_with_dest		(Folder		*folder,
					 FolderItem	*dest,
					 GSList		*msglist);

gint imap_remove_msg			(Folder		*folder,
					 FolderItem	*item,
					 gint		 uid);
gint imap_remove_all_msg		(Folder		*folder,
					 FolderItem	*item);
gboolean imap_is_msg_changed		(Folder		*folder,
					 FolderItem	*item,
					 MsgInfo	*msginfo);

gint imap_scan_folder			(Folder		*folder,
					 FolderItem	*item);
void imap_scan_tree			(Folder		*folder);

gint imap_create_tree			(Folder		*folder);

FolderItem *imap_create_folder		(Folder		*folder,
					 FolderItem	*parent,
					 const gchar	*name);
gint imap_rename_folder			(Folder		*folder,
					 FolderItem	*item,
					 const gchar	*name);
gint imap_remove_folder			(Folder		*folder,
					 FolderItem	*item);

gint imap_msg_set_perm_flags		(MsgInfo	*msginfo,
					 MsgPermFlags	 flags);
gint imap_msg_unset_perm_flags		(MsgInfo	*msginfo,
					 MsgPermFlags	 flags);

#endif /* __IMAP_H__ */
