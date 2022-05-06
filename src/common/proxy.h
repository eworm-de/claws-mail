/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2018 the Claws Mail team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __PROXY_H__
#define __PROXY_H__

#include <glib.h>

#include "socket.h"

typedef struct _ProxyInfo ProxyInfo;

typedef enum {
	PROXY_SOCKS4,
	PROXY_SOCKS5
} ProxyType;

struct _ProxyInfo
{
	ProxyType proxy_type;
	gchar *proxy_host;
	gushort proxy_port;

	gboolean use_proxy_auth;
	gchar *proxy_name;
	gchar *proxy_pass;
};

/* As a side effect, this function will zero out and free
 * string pointed to by proxy_info->proxy_pass after the password
 * is no longer needed. */
gint proxy_connect(SockInfo *sock, const gchar *hostname, gushort port,
		   ProxyInfo *proxy_info);

#endif /* __PROXY_H__ */
