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

#ifndef __POP_H__
#define __POP_H__

#include <glib.h>

#include "socket.h"

typedef struct _Pop3MsgInfo	Pop3MsgInfo;

typedef enum {
	POP3_GREETING_RECV,
	POP3_GETAUTH_USER_SEND,
	POP3_GETAUTH_USER_RECV,
	POP3_GETAUTH_PASS_SEND,
	POP3_GETAUTH_PASS_RECV,
	POP3_GETAUTH_APOP_SEND,
	POP3_GETAUTH_APOP_RECV,
	POP3_GETRANGE_STAT_SEND,
	POP3_GETRANGE_STAT_RECV,
	POP3_GETRANGE_LAST_SEND,
	POP3_GETRANGE_LAST_RECV,
	POP3_GETRANGE_UIDL_SEND,
	POP3_GETRANGE_UIDL_RECV,
	POP3_GETSIZE_LIST_SEND,
	POP3_GETSIZE_LIST_RECV,
	POP3_TOP_SEND,                   
	POP3_TOP_RECV,                    
	POP3_RETR_SEND,
	POP3_RETR_RECV,
	POP3_DELETE_SEND,
	POP3_DELETE_RECV,
	POP3_LOGOUT_SEND,
	POP3_LOGOUT_RECV,

	N_POP3_PHASE
} Pop3Phase;

struct _Pop3MsgInfo
{
	gint size;
	gchar *uidl;
	guint received : 1;
	guint deleted  : 1;
};

#define POPBUFSIZE	512
#define IDLEN		128

/* exit code values */
#define		PS_SUCCESS	0	/* successful receipt of messages */
#define		PS_NOMAIL	1	/* no mail available */
#define		PS_SOCKET	2	/* socket I/O woes */
#define		PS_AUTHFAIL	3	/* user authorization failed */
#define		PS_PROTOCOL	4	/* protocol violation */
#define		PS_SYNTAX	5	/* command-line syntax error */
#define		PS_IOERR	6	/* bad permissions on rc file */
#define		PS_ERROR	7	/* protocol error */
#define		PS_EXCLUDE	8	/* client-side exclusion error */
#define		PS_LOCKBUSY	9	/* server responded lock busy */
#define		PS_SMTP		10	/* SMTP error */
#define		PS_DNS		11	/* fatal DNS error */
#define		PS_BSMTP	12	/* output batch could not be opened */
#define		PS_MAXFETCH	13	/* poll ended by fetch limit */
/* leave space for more codes */
#define		PS_UNDEFINED	23	/* something I hadn't thought of */
#define		PS_TRANSIENT	24	/* transient failure (internal use) */
#define		PS_REFUSED	25	/* mail refused (internal use) */
#define		PS_RETAINED	26	/* message retained (internal use) */
#define		PS_TRUNCATED	27	/* headers incomplete (internal use) */

gint pop3_greeting_recv		(SockInfo *sock, gpointer data);
gint pop3_getauth_user_send	(SockInfo *sock, gpointer data);
gint pop3_getauth_user_recv	(SockInfo *sock, gpointer data);
gint pop3_getauth_pass_send	(SockInfo *sock, gpointer data);
gint pop3_getauth_pass_recv	(SockInfo *sock, gpointer data);
gint pop3_getauth_apop_send	(SockInfo *sock, gpointer data);
gint pop3_getauth_apop_recv	(SockInfo *sock, gpointer data);
gint pop3_getrange_stat_send	(SockInfo *sock, gpointer data);
gint pop3_getrange_stat_recv	(SockInfo *sock, gpointer data);
gint pop3_getrange_last_send	(SockInfo *sock, gpointer data);
gint pop3_getrange_last_recv	(SockInfo *sock, gpointer data);
gint pop3_getrange_uidl_send	(SockInfo *sock, gpointer data);
gint pop3_getrange_uidl_recv	(SockInfo *sock, gpointer data);
gint pop3_getsize_list_send	(SockInfo *sock, gpointer data);
gint pop3_getsize_list_recv	(SockInfo *sock, gpointer data);
gint pop3_top_send              (SockInfo *sock, gpointer data);
gint pop3_top_recv              (SockInfo *sock, gpointer data);
gint pop3_retr_send		(SockInfo *sock, gpointer data);
gint pop3_retr_recv		(SockInfo *sock, gpointer data);
gint pop3_delete_send		(SockInfo *sock, gpointer data);
gint pop3_delete_recv		(SockInfo *sock, gpointer data);
gint pop3_logout_send		(SockInfo *sock, gpointer data);
gint pop3_logout_recv		(SockInfo *sock, gpointer data);

#endif /* __POP_H__ */
