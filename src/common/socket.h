/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2007 Hiroyuki Yamamoto and the Claws Mail team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __SOCKET_H__
#define __SOCKET_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef _XOPEN_SOURCE
#define _BSD_SOURCE
#endif

#include <glib.h>
#if HAVE_NETDB_H
#  include <netdb.h>
#endif

typedef struct _SockInfo	SockInfo;

#if USE_OPENSSL
#  include "ssl.h"
#endif

typedef enum
{
	CONN_READY,
	CONN_LOOKUPSUCCESS,
	CONN_ESTABLISHED,
	CONN_LOOKUPFAILED,
	CONN_FAILED,
	CONN_DISCONNECTED
} ConnectionState;

typedef gint (*SockConnectFunc)		(SockInfo	*sock,
					 gpointer	 data);
typedef gboolean (*SockFunc)		(SockInfo	*sock,
					 GIOCondition	 condition,
					 gpointer	 data);

struct _SockInfo
{
	gint sock;
#if USE_OPENSSL
	SSL *ssl;
	guint g_source;
#endif
	GIOChannel *sock_ch;

	gchar *hostname;
	gushort port;
	ConnectionState state;
	gpointer data;

	SockFunc callback;
	GIOCondition condition;
	gchar *canonical_name;
};

void refresh_resolvers			(void);
gint sock_init				(void);
gint sock_cleanup			(void);

gint sock_set_io_timeout		(guint sec);

gint sock_set_nonblocking_mode		(SockInfo *sock, gboolean nonblock);
gboolean sock_is_nonblocking_mode	(SockInfo *sock);

guint sock_add_watch			(SockInfo *sock, GIOCondition condition,
					 SockFunc func, gpointer data);

struct hostent *my_gethostbyname	(const gchar *hostname);

SockInfo *sock_connect			(const gchar *hostname, gushort port);
SockInfo *sock_connect_cmd		(const gchar *hostname, const gchar *tunnelcmd);
gint sock_connect_async			(const gchar *hostname, gushort port,
					 SockConnectFunc func, gpointer data);
gint sock_connect_async_cancel		(gint id);

/* Basic I/O functions */
gint sock_read		(SockInfo *sock, gchar *buf, gint len);
gint sock_write		(SockInfo *sock, const gchar *buf, gint len);
gint sock_write_all	(SockInfo *sock, const gchar *buf, gint len);
gint sock_gets		(SockInfo *sock, gchar *buf, gint len);
gint sock_close		(SockInfo *sock);

/* Functions to directly work on FD.  They are needed for pipes */
gint fd_connect_unix	(const gchar *path);
gint fd_open_unix	(const gchar *path);
gint fd_accept		(gint sock);

gint fd_write		(gint sock, const gchar *buf, gint len);
gint fd_write_all	(gint sock, const gchar *buf, gint len);
gint fd_gets		(gint sock, gchar *buf, gint len);
gint fd_close		(gint sock);

#endif /* __SOCKET_H__ */
