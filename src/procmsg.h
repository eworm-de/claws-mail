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

#ifndef __PROCMSG_H__
#define __PROCMSG_H__

#include <glib.h>
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <string.h>

typedef struct _MsgInfo		MsgInfo;

#include "folder.h"

typedef enum
{
	/* permanent flags (0x0000ffff) */
	MSG_NEW		= 1 << 0,
	MSG_UNREAD	= 1 << 1,
	MSG_MARKED	= 1 << 2,
	MSG_DELETED	= 1 << 3,
	MSG_REPLIED	= 1 << 4,
	MSG_FORWARDED	= 1 << 5,
	MSG_REALLY_DELETED = 1 << 6,

	/* temporary flags (0xffff0000) */
	MSG_MOVE	= 1 << 16,
	MSG_COPY	= 1 << 17,

	MSG_QUEUED	= 1 << 25,
	MSG_DRAFT	= 1 << 26,
	MSG_ENCRYPTED   = 1 << 27,
	MSG_IMAP	= 1 << 28,
	MSG_MIME	= 1 << 29,
	MSG_NEWS	= 1 << 30,
	MSG_CACHED	= 1 << 31
} MsgFlags;

#define MSG_PERMANENT_FLAG_MASK		(MSG_NEW       | \
					 MSG_UNREAD    | \
					 MSG_MARKED    | \
					 MSG_DELETED   | \
					 MSG_REPLIED   | \
					 MSG_FORWARDED | \
                                         MSG_REALLY_DELETED)
#define MSG_CACHED_FLAG_MASK		(MSG_MIME)

#define MSG_SET_FLAGS(msg, flags)	{ (msg) |= (flags); }
#define MSG_UNSET_FLAGS(msg, flags)	{ (msg) &= ~(flags); }
#define MSG_IS_NEW(msg)			((msg & MSG_NEW) != 0)
#define MSG_IS_UNREAD(msg)		((msg & MSG_UNREAD) != 0)
#define MSG_IS_MARKED(msg)		((msg & MSG_MARKED) != 0)
#define MSG_IS_DELETED(msg)		((msg & MSG_DELETED) != 0)
#define MSG_IS_REPLIED(msg)		((msg & MSG_REPLIED) != 0)
#define MSG_IS_FORWARDED(msg)		((msg & MSG_FORWARDED) != 0)

#define MSG_IS_MOVE(msg)		((msg & MSG_MOVE) != 0)
#define MSG_IS_COPY(msg)		((msg & MSG_COPY) != 0)
#define MSG_IS_REALLY_DELETED(msg)	((msg & MSG_REALLY_DELETED) != 0)

#define MSG_IS_QUEUED(msg)		((msg & MSG_QUEUED) != 0)
#define MSG_IS_DRAFT(msg)		((msg & MSG_DRAFT) != 0)
#define MSG_IS_ENCRYPTED(msg)		((msg & MSG_ENCRYPTED) != 0)
#define MSG_IS_IMAP(msg)		((msg & MSG_IMAP) != 0)
#define MSG_IS_MIME(msg)		((msg & MSG_MIME) != 0)
#define MSG_IS_NEWS(msg)		((msg & MSG_NEWS) != 0)
#define MSG_IS_CACHED(msg)		((msg & MSG_CACHED) != 0)

#define WRITE_CACHE_DATA_INT(n, fp) \
	fwrite(&n, sizeof(n), 1, fp)

#define WRITE_CACHE_DATA(data, fp) \
{ \
	gint len; \
 \
	if (data == NULL || (len = strlen(data)) == 0) { \
		len = 0; \
		WRITE_CACHE_DATA_INT(len, fp); \
	} else { \
		len = strlen(data); \
		WRITE_CACHE_DATA_INT(len, fp); \
		fwrite(data, len, 1, fp); \
	} \
}

struct _MsgInfo
{
	guint  msgnum;
	off_t  size;
	time_t mtime;
	time_t date_t;
	MsgFlags flags;

	gchar *fromname;

	gchar *date;
	gchar *from;
	gchar *to;
	gchar *cc;
	gchar *newsgroups;
	gchar *subject;
	gchar *msgid;
	gchar *inreplyto;

	FolderItem *folder;
	FolderItem *to_folder;

	gchar *xface;

	gchar *dispositionnotificationto;
	gchar *returnreceiptto;

	gchar *references;
	gchar *fromspace;

	gint score;
	gint threadscore;

	/* used only for encrypted messages */
	gchar *plaintext_file;
	guint decryption_failed : 1;
};

GHashTable *procmsg_msg_hash_table_create	(GSList		*mlist);
void procmsg_msg_hash_table_append		(GHashTable	*msg_table,
						 GSList		*mlist);
GHashTable *procmsg_to_folder_hash_table_create	(GSList		*mlist);

GSList *procmsg_read_cache		(FolderItem	*item,
					 gboolean	 scan_file);
void	procmsg_set_flags		(GSList		*mlist,
					 FolderItem	*item);
gint	procmsg_get_last_num_in_cache	(GSList		*mlist);
void	procmsg_msg_list_free		(GSList		*mlist);
void	procmsg_write_cache		(MsgInfo	*msginfo,
					 FILE		*fp);
void	procmsg_write_flags		(MsgInfo	*msginfo,
					 FILE		*fp);
void	procmsg_get_mark_sum		(const gchar	*folder,
					 gint		*new,
					 gint		*unread,
					 gint		*total);
FILE   *procmsg_open_mark_file		(const gchar	*folder,
					 gboolean	 append);

void	procmsg_move_messages		(GSList		*mlist);
void	procmsg_copy_messages		(GSList		*mlist);

gchar  *procmsg_get_message_file_path	(MsgInfo	*msginfo);
gchar  *procmsg_get_message_file	(MsgInfo	*msginfo);
FILE   *procmsg_open_message		(MsgInfo	*msginfo);
gboolean procmsg_msg_exist		(MsgInfo	*msginfo);

void	procmsg_empty_trash		(void);
gint	procmsg_send_queue		(void);
void	procmsg_print_message		(MsgInfo	*msginfo,
					 const gchar	*cmdline);

MsgInfo *procmsg_msginfo_copy		(MsgInfo	*msginfo);
void	 procmsg_msginfo_free		(MsgInfo	*msginfo);

gint procmsg_cmp_msgnum_for_sort	(gconstpointer	 a,
					 gconstpointer	 b);

#endif /* __PROCMSG_H__ */
