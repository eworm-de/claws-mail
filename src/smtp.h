/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999,2000 Hiroyuki Yamamoto
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

#include <glib.h>

#include "socket.h"

#define	SMTPBUFSIZE		256

#define	SM_OK			0
#define	SM_ERROR		128
#define	SM_UNRECOVERABLE	129
#define	SM_AUTHFAIL		130

#define	ESMTP_8BITMIME		0x01
#define	ESMTP_SIZE		0x02
#define	ESMTP_ETRN		0x04

gint smtp_helo(SockInfo *sock, const gchar *hostname, gboolean esmtp);
gint smtp_from(SockInfo *sock, const gchar *from, const gchar *userid,
	       const gchar *passwd, gboolean use_smtp_auth);
gint smtp_rcpt(SockInfo *sock, const gchar *to);
gint smtp_data(SockInfo *sock);
gint smtp_rset(SockInfo *sock);
gint smtp_quit(SockInfo *sock);
gint smtp_eom(SockInfo *sock);
gint smtp_ok(SockInfo *sock);

#endif /* __SMTP_H__ */
