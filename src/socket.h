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

#ifndef __SOCKET_H__
#define __SOCKET_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <gdk/gdk.h> /* ugly, just needed for the GdkInputCondition et al. */

typedef struct _SockInfo	SockInfo;

#if USE_SSL
#  include "ssl.h"
#endif

typedef enum
{
	CONN_READY,
	CONN_LOOKUPSUCCESS,
	CONN_ESTABLISHED,
	CONN_LOOKUPFAILED,
	CONN_FAILED
} ConnectionState;

struct _SockInfo
{
#if USE_GIO
	GIOChannel *channel;
	gchar *buf;
	gint buflen;
#else
	gint sock;
#endif
	gchar *hostname;
	gushort port;
	ConnectionState state;
	gpointer data;
#if USE_SSL
	SSL *ssl;
#endif
};

gint sock_set_nonblocking_mode		(SockInfo *sock, gboolean nonblock);
gboolean sock_is_nonblocking_mode	(SockInfo *sock);

SockInfo *sock_connect_nb		(const gchar *hostname, gushort port);
SockInfo *sock_connect			(const gchar *hostname, gushort port);
SockInfo *sock_connect_cmd		(const gchar *hostname, const gchar *tunnelcmd);

gint sock_printf	(SockInfo *sock, const gchar *format, ...)
			 G_GNUC_PRINTF(2, 3);
gint sock_read		(SockInfo *sock, gchar *buf, gint len);
gint sock_write		(SockInfo *sock, const gchar *buf, gint len);
gint sock_gets		(SockInfo *sock, gchar *buf, gint len);
gchar *sock_getline	(SockInfo *sock);
gint sock_puts		(SockInfo *sock, const gchar *buf);
gint sock_close		(SockInfo *sock);

/* wrapper functions */
gint sock_gdk_input_add	  (SockInfo		*sock,
			   GdkInputCondition	 condition,
			   GdkInputFunction	 function,
			   gpointer		 data);

/* Functions to directly work on FD.  They are needed for pipes */
gint fd_connect_unix	(const gchar *path);
gint fd_open_unix	(const gchar *path);
gint fd_accept		(gint sock);

gint fd_read		(gint sock, gchar *buf, gint len);
gint fd_write		(gint sock, const gchar *buf, gint len);
gint fd_gets		(gint sock, gchar *buf, gint len);
gchar *fd_getline	(gint sock);
gint fd_close		(gint sock);

/* Functions for SSL */
#if USE_SSL
gint ssl_read(SSL *ssl, gchar *buf, gint len);
gint ssl_write(SSL *ssl, const gchar *buf, gint len);
gint ssl_gets(SSL *ssl, gchar *buf, gint len);
gchar *ssl_getline(SSL *ssl);
#endif

#endif /* __SOCKET_H__ */
