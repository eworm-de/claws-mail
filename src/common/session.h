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

#ifndef __SESSION_H__
#define __SESSION_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>

#include <time.h>
#include <unistd.h>

#include "socket.h"

typedef struct _Session	Session;

#define SESSION(obj)	((Session *)obj)

typedef enum {
	SESSION_IMAP,
	SESSION_NEWS,
	SESSION_SMTP,
	SESSION_POP3
} SessionType;

typedef enum {
	SESSION_READY,
	SESSION_SEND,
	SESSION_RECV,
	SESSION_ERROR,
	SESSION_DISCONNECTED
} SessionState;

typedef enum
{
	SESSION_MSG_NORMAL,
	SESSION_MSG_SEND_DATA,
	SESSION_MSG_RECV_DATA,
	SESSION_MSG_CONTROL,
	SESSION_MSG_ERROR,
	SESSION_MSG_UNKNOWN
} SessionMsgType;

typedef gint (*RecvMsgNotify)			(Session	*session,
						 const gchar	*msg,
						 gpointer	 user_data);
typedef gint (*RecvDataProgressiveNotify)	(Session	*session,
						 guint		 cur_len,
						 guint		 total_len,
						 gpointer	 user_data);
typedef gint (*RecvDataNotify)			(Session	*session,
						 guint		 len,
						 gpointer	 user_data);
typedef gint (*SendDataProgressiveNotify)	(Session	*session,
						 guint		 cur_len,
						 guint		 total_len,
						 gpointer	 user_data);
typedef gint (*SendDataNotify)			(Session	*session,
						 guint		 len,
						 gpointer	 user_data);

struct _Session
{
	SessionType type;

	SockInfo *sock;

	gchar *server;
	gushort port;

#if USE_OPENSSL
	SSLType ssl_type;
#endif

	SessionState state;

	time_t last_access_time;

	pid_t child_pid;

	/* pipe I/O */
	GIOChannel *read_ch;
	GIOChannel *write_ch;

	gint read_tag;

	gpointer data;

	/* virtual methods to parse server responses */
	gint (*recv_msg)		(Session	*session,
					 const gchar	*msg);

	gint (*recv_data_finished)	(Session	*session,
					 guchar		*data,
					 guint		 len);
	gint (*send_data_finished)	(Session	*session,
					 guint		 len);

	void (*destroy)			(Session	*session);

	/* notification functions */
	RecvMsgNotify			recv_msg_notify;
	RecvDataProgressiveNotify	recv_data_progressive_notify;
	RecvDataNotify			recv_data_notify;
	SendDataProgressiveNotify	send_data_progressive_notify;
	SendDataNotify			send_data_notify;

	gpointer recv_msg_notify_data;
	gpointer recv_data_progressive_notify_data;
	gpointer recv_data_notify_data;
	gpointer send_data_progressive_notify_data;
	gpointer send_data_notify_data;
};

void session_init	(Session	*session);
gint session_connect	(Session	*session,
			 const gchar	*server,
			 gushort	 port);
gint session_disconnect	(Session	*session);
void session_destroy	(Session	*session);

void session_set_recv_message_notify	(Session	*session,
					 RecvMsgNotify	 notify_func,
					 gpointer	 data);
void session_set_recv_data_progressive_notify
					(Session	*session,
					 RecvDataProgressiveNotify notify_func,
					 gpointer	 data);
void session_set_recv_data_notify	(Session	*session,
					 RecvDataNotify	 notify_func,
					 gpointer	 data);
void session_set_send_data_progressive_notify
					(Session	*session,
					 SendDataProgressiveNotify notify_func,
					 gpointer	 data);
void session_set_send_data_notify	(Session	*session,
					 SendDataNotify	 notify_func,
					 gpointer	 data);

gint session_send_msg	(Session	*session,
			 SessionMsgType	 type,
			 const gchar	*msg);
gint session_send_data	(Session	*session,
			 const guchar	*data,
			 guint		 size);
gint session_recv_data	(Session	*session,
			 guint		 size,
			 gboolean	 unescape_dot);

#if USE_OPENSSL
gint session_start_tls	(Session	*session);
#endif

#endif /* __SESSION_H__ */
