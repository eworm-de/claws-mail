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

#ifndef __NNTP_H__
#define __NNTP_H__

#include "socket.h"

#define NN_SUCCESS	0
#define NN_SOCKET	2
#define NN_AUTHFAIL	3
#define NN_PROTOCOL	4
#define NN_SYNTAX	5
#define NN_IOERR	6
#define NN_ERROR	7
#define NN_AUTHREQ	8
#define NN_AUTHCONT 9

#define NNTPBUFSIZE	8192

SockInfo *nntp_open(const gchar *server, gushort port, gchar *buf);
gint nntp_group(SockInfo *sock, const gchar *group,
		gint *num, gint *first, gint *last);
gint nntp_get_article(SockInfo *sock, const gchar *cmd, gint num, gchar **msgid);
gint nntp_article(SockInfo *sock, gint num, gchar **msgid);
gint nntp_body(SockInfo *sock, gint num, gchar **msgid);
gint nntp_head(SockInfo *sock, gint num, gchar **msgid);
gint nntp_stat(SockInfo *sock, gint num, gchar **msgid);
gint nntp_next(SockInfo *sock, gint *num, gchar **msgid);
gint nntp_xover(SockInfo *sock, gint first, gint last);
gint nntp_post(SockInfo *sock, FILE *fp);
gint nntp_newgroups(SockInfo *sock);
gint nntp_newnews(SockInfo *sock);
gint nntp_mode(SockInfo *sock, gboolean stream);
gint nntp_ok(SockInfo *sock, gchar *argbuf);
gint nntp_authinfo_user(SockInfo *sock, const gchar *user);
gint nntp_authinfo_pass(SockInfo *sock, const gchar *pass);
gint nntp_list(SockInfo *sock);

#endif /* __NNTP_H__ */
