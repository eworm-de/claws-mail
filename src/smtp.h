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

#ifndef __SMTP_H__
#define __SMTP_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>

#include "socket.h"
#if USE_SSL
#  include "ssl.h"
#endif
#include "session.h"

typedef struct _SMTPSession	SMTPSession;

#define SMTP_SESSION(obj)	((SMTPSession *)obj)

#define MSGBUFSIZE		8192

#define	SM_OK			0
#define	SM_ERROR		128
#define	SM_UNRECOVERABLE	129
#define SM_AUTHFAIL		130

#define	ESMTP_8BITMIME		0x01
#define	ESMTP_SIZE		0x02
#define	ESMTP_ETRN		0x04

typedef enum
{
	SMTPAUTH_LOGIN      = 1 << 0,
	SMTPAUTH_CRAM_MD5   = 1 << 1,
	SMTPAUTH_DIGEST_MD5 = 1 << 2
} SMTPAuthType;

struct _SMTPSession
{
	Session session;

	SMTPAuthType avail_auth_type;
	gchar *user;
	gchar *pass;
};

#if USE_SSL
Session *smtp_session_new	(const gchar	*server,
				 gushort	 port,
				 const gchar	*domain,
				 const gchar	*user,
				 const gchar	*pass,
				 SSLType	 ssl_type,
				 SMTPAuthType	 enable_auth_type);
#else
Session *smtp_session_new	(const gchar	*server,
				 gushort	 port,
				 const gchar	*domain,
				 const gchar	*user,
				 const gchar	*pass,
				 SMTPAuthType	 enable_auth_type);
#endif
void smtp_session_destroy	(SMTPSession	*session);

gint smtp_from			(SMTPSession	*session,
				 const gchar	*from);
gint smtp_auth			(SMTPSession	*session);

gint smtp_ehlo			(SockInfo	*sock,
				 const gchar	*hostname,
				 SMTPAuthType	*avail_auth_type,
				 SMTPAuthType	 enable_auth_type);

gint smtp_helo			(SockInfo	*sock,
				 const gchar	*hostname);
gint smtp_rcpt			(SockInfo	*sock,
				 const gchar	*to);
gint smtp_data			(SockInfo	*sock);
gint smtp_rset			(SockInfo	*sock);
gint smtp_quit			(SockInfo	*sock);
gint smtp_eom			(SockInfo	*sock);

#endif /* __SMTP_H__ */
