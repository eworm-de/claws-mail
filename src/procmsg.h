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

#ifndef __PROCMSG_H__
#define __PROCMSG_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <string.h>

typedef struct _MsgInfo		MsgInfo;
typedef struct _MsgFlags	MsgFlags;

#include "folder.h"
#include "procmime.h"

typedef enum
{
	MSG_NEW		= 1 << 0,
	MSG_UNREAD	= 1 << 1,
	MSG_MARKED	= 1 << 2,
	MSG_DELETED	= 1 << 3,
	MSG_REPLIED	= 1 << 4,
	MSG_FORWARDED	= 1 << 5,
	
	MSG_REALLY_DELETED = 1 << 6,		/* mbox stuff */

#define MSG_CLABEL_SBIT	(7)		/* start bit of color label */
#define MAKE_MSG_CLABEL(h, m, l)	(((h) << (MSG_CLABEL_SBIT + 2)) | \
					 ((m) << (MSG_CLABEL_SBIT + 1)) | \
					 ((l) << (MSG_CLABEL_SBIT + 0)))

	MSG_CLABEL_NONE = MAKE_MSG_CLABEL(0, 0, 0),
	MSG_CLABEL_1    = MAKE_MSG_CLABEL(0, 0, 1),
	MSG_CLABEL_2    = MAKE_MSG_CLABEL(0, 1, 0),
	MSG_CLABEL_3    = MAKE_MSG_CLABEL(0, 1, 1),
	MSG_CLABEL_4    = MAKE_MSG_CLABEL(1, 0, 0),
	MSG_CLABEL_5    = MAKE_MSG_CLABEL(1, 0, 1),
	MSG_CLABEL_6    = MAKE_MSG_CLABEL(1, 1, 0),
	MSG_CLABEL_7    = MAKE_MSG_CLABEL(1, 1, 1),

#define MSG_CLABEL_ORANGE	MSG_CLABEL_1
#define MSG_CLABEL_RED		MSG_CLABEL_2
#define MSG_CLABEL_PINK		MSG_CLABEL_3
#define MSG_CLABEL_SKYBLUE	MSG_CLABEL_4
#define MSG_CLABEL_BLUE		MSG_CLABEL_5
#define MSG_CLABEL_GREEN	MSG_CLABEL_6
#define MSG_CLABEL_BROWN	MSG_CLABEL_7

	MSG_IGNORE_THREAD   = 1 << 10,   /* ignore threads */
	MSG_LOCKED	    = 1 << 11,   /* msg is locked  */
	MSG_RETRCPT_PENDING = 1 << 12,	 /* return receipt pending */

	/* RESERVED */
	MSG_RESERVED_CLAWS  = 1 << 30,	/* for sylpheed-claws */
	MSG_RESERVED_MAIN   = 1 << 31	/* for sylpheed-main  */
} MsgPermFlags;

#define MSG_CLABEL_FLAG_MASK	(MSG_CLABEL_7)

typedef enum
{
	MSG_MOVE	= 1 << 0,
	MSG_COPY	= 1 << 1,

	MSG_QUEUED	= 1 << 16,
	MSG_DRAFT	= 1 << 17,
	MSG_ENCRYPTED   = 1 << 18,
	MSG_IMAP	= 1 << 19,
	MSG_NEWS	= 1 << 20,
	MSG_SIGNED	= 1 << 21,

	MSG_MIME	= 1 << 29,

	MSG_CACHED	= 1 << 31
} MsgTmpFlags;

#define MSG_CACHED_FLAG_MASK	(MSG_MIME | MSG_ENCRYPTED | MSG_SIGNED)

#define MSG_SET_FLAGS(msg, flags)	{ (msg) |= (flags); }
#define MSG_UNSET_FLAGS(msg, flags)	{ (msg) &= ~(flags); }
#define MSG_SET_PERM_FLAGS(msg, flags) \
	MSG_SET_FLAGS((msg).perm_flags, flags)
#define MSG_SET_TMP_FLAGS(msg, flags) \
	MSG_SET_FLAGS((msg).tmp_flags, flags)
#define MSG_UNSET_PERM_FLAGS(msg, flags) \
	MSG_UNSET_FLAGS((msg).perm_flags, flags)
#define MSG_UNSET_TMP_FLAGS(msg, flags) \
	MSG_UNSET_FLAGS((msg).tmp_flags, flags)

#define MSG_IS_NEW(msg)			(((msg).perm_flags & MSG_NEW) != 0)
#define MSG_IS_UNREAD(msg)		(((msg).perm_flags & MSG_UNREAD) != 0)
#define MSG_IS_MARKED(msg)		(((msg).perm_flags & MSG_MARKED) != 0)
#define MSG_IS_DELETED(msg)		(((msg).perm_flags & MSG_DELETED) != 0)
#define MSG_IS_REPLIED(msg)		(((msg).perm_flags & MSG_REPLIED) != 0)
#define MSG_IS_LOCKED(msg)		(((msg).perm_flags & MSG_LOCKED) != 0)
#define MSG_IS_FORWARDED(msg)		(((msg).perm_flags & MSG_FORWARDED) != 0)

#define MSG_GET_COLORLABEL(msg)		(((msg).perm_flags & MSG_CLABEL_FLAG_MASK))
#define MSG_GET_COLORLABEL_VALUE(msg)	(MSG_GET_COLORLABEL(msg) >> MSG_CLABEL_SBIT)
#define MSG_SET_COLORLABEL_VALUE(msg, val) \
	MSG_SET_PERM_FLAGS(msg, ((((guint)(val)) & 7) << MSG_CLABEL_SBIT))

#define MSG_COLORLABEL_TO_FLAGS(val) ((((guint)(val)) & 7) << MSG_CLABEL_SBIT)
#define MSG_COLORLABEL_FROM_FLAGS(val) (val >> MSG_CLABEL_SBIT)

#define MSG_IS_MOVE(msg)		(((msg).tmp_flags & MSG_MOVE) != 0)
#define MSG_IS_COPY(msg)		(((msg).tmp_flags & MSG_COPY) != 0)

#define MSG_IS_QUEUED(msg)		(((msg).tmp_flags & MSG_QUEUED) != 0)
#define MSG_IS_DRAFT(msg)		(((msg).tmp_flags & MSG_DRAFT) != 0)
#define MSG_IS_ENCRYPTED(msg)		(((msg).tmp_flags & MSG_ENCRYPTED) != 0)
#define MSG_IS_SIGNED(msg)		(((msg).tmp_flags & MSG_SIGNED) != 0)
#define MSG_IS_IMAP(msg)		(((msg).tmp_flags & MSG_IMAP) != 0)
#define MSG_IS_NEWS(msg)		(((msg).tmp_flags & MSG_NEWS) != 0)
#define MSG_IS_MIME(msg)		(((msg).tmp_flags & MSG_MIME) != 0)
#define MSG_IS_CACHED(msg)		(((msg).tmp_flags & MSG_CACHED) != 0)

/* Claws related flags */
#define MSG_IS_REALLY_DELETED(msg)	(((msg).perm_flags & MSG_REALLY_DELETED) != 0)
#define MSG_IS_IGNORE_THREAD(msg)	(((msg).perm_flags & MSG_IGNORE_THREAD) != 0)
#define MSG_IS_RETRCPT_PENDING(msg)	(((msg).perm_flags & MSG_RETRCPT_PENDING) != 0)


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

struct _MsgFlags
{
	MsgPermFlags perm_flags;
	MsgTmpFlags  tmp_flags;
};

struct _MsgInfo
{
	guint refcnt;

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
	gchar *xref;

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
void	procmsg_flush_mark_queue	(FolderItem	*item,
					 FILE		*fp);
void	procmsg_add_flags		(FolderItem	*item,
					 gint		 num,
					 MsgFlags	 flags);
void	procmsg_get_mark_sum		(const gchar	*folder,
					 gint		*new,
					 gint		*unread,
					 gint		*total,
					 gint		*min,
					 gint		*max,
					 gint		 first);
FILE   *procmsg_open_mark_file		(const gchar	*folder,
					 gboolean	 append);

GNode  *procmsg_get_thread_tree		(GSList		*mlist);

void	procmsg_move_messages		(GSList		*mlist);
void	procmsg_copy_messages		(GSList		*mlist);

gchar  *procmsg_get_message_file_path	(MsgInfo	*msginfo);
gchar  *procmsg_get_message_file	(MsgInfo	*msginfo);
FILE   *procmsg_open_message		(MsgInfo	*msginfo);
#if USE_GPGME
FILE   *procmsg_open_message_decrypted	(MsgInfo	*msginfo,
					 MimeInfo      **mimeinfo);
#endif
gboolean procmsg_msg_exist		(MsgInfo	*msginfo);

void	procmsg_empty_trash		(void);
gint	procmsg_send_queue		(FolderItem	*queue,
					 gboolean	 save_msgs);
gint	procmsg_save_to_outbox		(FolderItem	*outbox,
					 const gchar	*file,
					 gboolean	 is_queued);
void	procmsg_print_message		(MsgInfo	*msginfo,
					 const gchar	*cmdline);

MsgInfo *procmsg_msginfo_new		();
MsgInfo *procmsg_msginfo_new_ref	(MsgInfo 	*msginfo);
MsgInfo *procmsg_msginfo_copy		(MsgInfo	*msginfo);
void	 procmsg_msginfo_free		(MsgInfo	*msginfo);
guint	 procmsg_msginfo_memusage	(MsgInfo	*msginfo);

gint procmsg_cmp_msgnum_for_sort	(gconstpointer	 a,
					 gconstpointer	 b);
gint procmsg_send_message_queue		(const gchar *file);

void procmsg_msginfo_set_flags		(MsgInfo *msginfo,
					 MsgPermFlags perm_flags,
					  MsgTmpFlags tmp_flags);
void procmsg_msginfo_unset_flags	(MsgInfo *msginfo,
					 MsgPermFlags perm_flags,
					  MsgTmpFlags tmp_flags);

#endif /* __PROCMSG_H__ */
