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

#define NN_SUCCESS	0
#define NN_SOCKET	2
#define NN_AUTHFAIL	3
#define NN_PROTOCOL	4
#define NN_SYNTAX	5
#define NN_IOERR	6
#define NN_ERROR	7

#define NNTPBUFSIZE	8192

gint nntp_open(const gchar *server, gushort port, gchar *buf);
gint nntp_group(gint sock, const gchar *group,
		gint *num, gint *first, gint *last);
gint nntp_get_article(gint sock, const gchar *cmd, gint num, gchar **msgid);
gint nntp_article(gint sock, gint num, gchar **msgid);
gint nntp_body(gint sock, gint num, gchar **msgid);
gint nntp_head(gint sock, gint num, gchar **msgid);
gint nntp_stat(gint sock, gint num, gchar **msgid);
gint nntp_next(gint sock, gint *num, gchar **msgid);
gint nntp_xover(gint sock, gint first, gint last);
gint nntp_post(gint sock, FILE *fp);
gint nntp_newgroups(gint sock);
gint nntp_newnews(gint sock);
gint nntp_mode(gint sock, gboolean stream);
gint nntp_ok(gint sock, gchar *argbuf);
gint nntp_list(gint sock);

#endif /* __NNTP_H__ */
