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

#ifndef __SOCKET_H__
#define __SOCKET_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>

#if USE_THREADS
#  include <pthread.h>
#endif

typedef struct _SockInfo	SockInfo;

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
	gint sock;
	gchar *hostname;
	gushort port;
	ConnectionState state;
	gpointer data;
#if USE_THREADS
	pthread_t connect_thr;
	pthread_mutex_t mutex;
#endif
};

gint sock_set_nonblocking_mode		(gint sock, gboolean nonblock);
gboolean sock_is_nonblocking_mode	(gint sock);

SockInfo *sock_connect_nb		(const gchar *hostname, gushort port);
SockInfo *sock_connect			(const gchar *hostname, gushort port);

gint sock_connect_unix	(const gchar *path);
gint sock_open_unix	(const gchar *path);
gint sock_accept	(gint sock);

#if USE_THREADS
SockInfo *sock_connect_with_thread	(const gchar *hostname, gushort port);
#endif

void sock_sockinfo_free	(SockInfo *sockinfo);

gint sock_printf	(gint sock, const gchar *format, ...)
			 G_GNUC_PRINTF(2, 3);
gint sock_write		(gint sock, const gchar *buf, gint len);
gint sock_read		(gint sock, gchar *buf, gint len);
gint sock_puts		(gint sock, const gchar *buf);
gint sock_close		(gint sock);

#endif /* __SOCKET_H__ */
