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

#ifndef __NNTP_H__
#define __NNTP_H__

#include "socket.h"

typedef struct _NNTPSockInfo	NNTPSockInfo;

struct _NNTPSockInfo
{
	SockInfo *sock;

	gchar *userid;
	gchar *passwd;
	gboolean auth_failed;
};

#define NN_SUCCESS	0
#define NN_SOCKET	2
#define NN_AUTHFAIL	3
#define NN_PROTOCOL	4
#define NN_SYNTAX	5
#define NN_IOERR	6
#define NN_ERROR	7
#define NN_AUTHREQ	8
#define NN_AUTHCONT	9

#define NNTPBUFSIZE	8192

#if USE_SSL
NNTPSockInfo *nntp_open		(const gchar	*server,
				 gushort	 port,
				 gchar		*buf,
				 gboolean use_ssl);
#else
NNTPSockInfo *nntp_open		(const gchar	*server,
				 gushort	 port,
				 gchar		*buf);
#endif

#if USE_SSL
NNTPSockInfo *nntp_open_auth	(const gchar	*server,
				 gushort	 port,
				 gchar		*buf,
				 const gchar	*userid,
				 const gchar	*passwd,
				 gboolean use_ssl);
#else
NNTPSockInfo *nntp_open_auth	(const gchar	*server,
				 gushort	 port,
				 gchar		*buf,
				 const gchar	*userid,
				 const gchar	*passwd);
#endif
void nntp_close			(NNTPSockInfo	*sock);
gint nntp_group			(NNTPSockInfo	*sock,
				 const gchar	*group,
				 gint		*num,
				 gint		*first,
				 gint		*last);
gint nntp_get_article		(NNTPSockInfo	*sock,
				 const gchar	*cmd,
				 gint		 num,
				 gchar	       **msgid);
gint nntp_article		(NNTPSockInfo	*sock,
				 gint		 num,
				 gchar	       **msgid);
gint nntp_body			(NNTPSockInfo	*sock,
				 gint		 num,
				 gchar	       **msgid);
gint nntp_head			(NNTPSockInfo	*sock,
				 gint		 num,
				 gchar	       **msgid);
gint nntp_stat			(NNTPSockInfo	*sock,
				 gint		 num,
				 gchar	       **msgid);
gint nntp_next			(NNTPSockInfo	*sock,
				 gint		*num,
				 gchar	       **msgid);
gint nntp_xover			(NNTPSockInfo	*sock,
				 gint		 first,
				 gint		 last);
gint nntp_xhdr			(NNTPSockInfo	*sock,
				 const gchar	*header,
				 gint		 first,
				 gint		 last);
gint nntp_list			(NNTPSockInfo	*sock);
gint nntp_post			(NNTPSockInfo	*sock,
				 FILE		*fp);
gint nntp_newgroups		(NNTPSockInfo	*sock);
gint nntp_newnews		(NNTPSockInfo	*sock);
gint nntp_mode			(NNTPSockInfo	*sock,
				 gboolean	 stream);
gint nntp_ok			(NNTPSockInfo	*sock,
				 gchar		*argbuf);

#endif /* __NNTP_H__ */
