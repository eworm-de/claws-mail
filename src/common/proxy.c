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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>

#ifdef G_OS_WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <stdint.h>
#else
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <netinet/ip.h>
#endif

#include "proxy.h"
#include "socket.h"
#include "utils.h"

gint socks4_connect(SockInfo *sock, const gchar *hostname, gushort port);
gint socks5_connect(SockInfo *sock, const gchar *hostname, gushort port,
		const gchar *proxy_name, const gchar *proxy_pass);

gint proxy_connect(SockInfo *sock, const gchar *hostname, gushort port,
		   ProxyInfo *proxy_info)
{
	gint ret;

	g_return_val_if_fail(sock != NULL, -1);
	g_return_val_if_fail(hostname != NULL, -1);
	g_return_val_if_fail(proxy_info != NULL, -1);

	debug_print("proxy_connect: connect to %s:%u via %s:%u\n",
		    hostname, port,
		    proxy_info->proxy_host, proxy_info->proxy_port);

	if (proxy_info->proxy_type == PROXY_SOCKS5) {
		ret = socks5_connect(sock, hostname, port,
				      proxy_info->use_proxy_auth ? proxy_info->proxy_name : NULL,
				      proxy_info->use_proxy_auth ? proxy_info->proxy_pass : NULL);
		/* Scrub the password before returning */
		if (proxy_info->proxy_pass != NULL) {
			memset(proxy_info->proxy_pass, 0, strlen(proxy_info->proxy_pass));
			g_free(proxy_info->proxy_pass);
			proxy_info->proxy_pass = NULL;
		}
		return ret;
	} else if (proxy_info->proxy_type == PROXY_SOCKS4) {
		return socks4_connect(sock, hostname, port);
	} else {
		g_warning("proxy_connect: unknown SOCKS type: %d",
			  proxy_info->proxy_type);
	}

	return -1;
}

gint socks4_connect(SockInfo *sock, const gchar *hostname, gushort port)
{
	guchar socks_req[1024];
	struct addrinfo hints, *res, *ai;
	gboolean got_address = FALSE;
	int s;

	g_return_val_if_fail(sock != NULL, -1);
	g_return_val_if_fail(hostname != NULL, -1);

	debug_print("socks4_connect: connect to %s:%u\n", hostname, port);

	socks_req[0] = 4;
	socks_req[1] = 1;
	*((gushort *)(socks_req + 2)) = htons(port);

	/* lookup */
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET; /* SOCKS4 only supports IPv4 addresses */

	s = getaddrinfo(hostname, NULL, &hints, &res);
	if (s != 0) {
		fprintf(stderr, "getaddrinfo for '%s' failed: %s\n",
				hostname, gai_strerror(s));
		return -1;
	}

	for (ai = res; ai != NULL; ai = ai->ai_next) {
		uint32_t addr;

		if (ai->ai_family != AF_INET)
			continue;

		addr = ((struct sockaddr_in *)ai->ai_addr)->sin_addr.s_addr;
		memcpy(socks_req + 4, &addr, 4);
		got_address = TRUE;
		break;
	}

	if (res != NULL)
		freeaddrinfo(res);

	if (!got_address) {
		g_warning("socks4_connect: could not get valid IPv4 address for '%s'", hostname);
		return -1;
	}

	debug_print("got a valid IPv4 address, continuing\n");

	/* userid (empty) */
	socks_req[8] = 0;

	if (sock_write_all(sock, (gchar *)socks_req, 9) != 9) {
		g_warning("socks4_connect: SOCKS4 initial request write failed");
		return -1;
	}

	if (sock_read(sock, (gchar *)socks_req, 8) != 8) {
		g_warning("socks4_connect: SOCKS4 response read failed");
		return -1;
	}
	if (socks_req[0] != 0) {
		g_warning("socks4_connect: SOCKS4 response has invalid version");
		return -1;
	}
	if (socks_req[1] != 90) {
		g_warning("socks4_connect: SOCKS4 connection to %u.%u.%u.%u:%u failed. (%u)", socks_req[4], socks_req[5], socks_req[6], socks_req[7], ntohs(*(gushort *)(socks_req + 2)), socks_req[1]);
		return -1;
	}

	/* replace sock->hostname with endpoint */
	if (sock->hostname != hostname) {
		g_free(sock->hostname);
		sock->hostname = g_strdup(hostname);
		sock->port = port;
	}

	debug_print("socks4_connect: SOCKS4 connection to %s:%u successful.\n", hostname, port);

	return 0;
}

gint socks5_connect(SockInfo *sock, const gchar *hostname, gushort port,
		    const gchar *proxy_name, const gchar *proxy_pass)
{
	guchar socks_req[1024];
	size_t len;
	size_t size;

	g_return_val_if_fail(sock != NULL, -1);
	g_return_val_if_fail(hostname != NULL, -1);

	debug_print("socks5_connect: connect to %s:%u\n", hostname, port);

	len = strlen(hostname);
	if (len > 255) {
		g_warning("socks5_connect: hostname too long");
		return -1;
	}

	socks_req[0] = 5;
	socks_req[1] = proxy_name ? 2 : 1;
	socks_req[2] = 0;
	socks_req[3] = 2;

	if (sock_write_all(sock, (gchar *)socks_req, 2 + socks_req[1]) != 2 + socks_req[1]) {
		g_warning("socks5_connect: SOCKS5 initial request write failed");
		return -1;
	}

	if (sock_read(sock, (gchar *)socks_req, 2) != 2) {
		g_warning("socks5_connect: SOCKS5 response read failed");
		return -1;
	}
	if (socks_req[0] != 5) {
		g_warning("socks5_connect: SOCKS5 response has invalid version");
		return -1;
	}
	if (socks_req[1] == 2) {
		/* auth */
		size_t userlen, passlen;
		gint reqlen;

		if (proxy_name && proxy_pass) {
			debug_print("socks5_connect: auth using username '%s'\n", proxy_name);
			userlen = strlen(proxy_name);
			passlen = strlen(proxy_pass);
		} else
			userlen = passlen = 0;

		socks_req[0] = 1;
		socks_req[1] = (guchar)userlen;
		if (proxy_name && userlen > 0)
			memcpy(socks_req + 2, proxy_name, userlen);
		socks_req[2 + userlen] = (guchar)passlen;
		if (proxy_pass && passlen > 0)
			memcpy(socks_req + 2 + userlen + 1, proxy_pass, passlen);

		reqlen = 2 + userlen + 1 + passlen;
		if (sock_write_all(sock, (gchar *)socks_req, reqlen) != reqlen) {
			memset(socks_req, 0, reqlen);
			g_warning("socks5_connect: SOCKS5 auth write failed");
			return -1;
		}
		memset(socks_req, 0, reqlen);
		if (sock_read(sock, (gchar *)socks_req, 2) != 2) {
			g_warning("socks5_connect: SOCKS5 auth response read failed");
			return -1;
		}
		if (socks_req[1] != 0) {
			g_warning("socks5_connect: SOCKS5 authentication failed: user: %s (%u %u)", proxy_name ? proxy_name : "(none)", socks_req[0], socks_req[1]);
			return -1;
		}
	} else if (socks_req[1] != 0) {
		g_warning("socks5_connect: SOCKS5 reply (%u) error", socks_req[1]);
		return -1;
	}

	socks_req[0] = 5;
	socks_req[1] = 1;
	socks_req[2] = 0;

	socks_req[3] = 3;
	socks_req[4] = (guchar)len;
	memcpy(socks_req + 5, hostname, len);
	*((gushort *)(socks_req + 5 + len)) = htons(port);

	if (sock_write_all(sock, (gchar *)socks_req, 5 + len + 2) != 5 + len + 2) {
		g_warning("socks5_connect: SOCKS5 connect request write failed");
		return -1;
	}

	if (sock_read(sock, (gchar *)socks_req, 10) != 10) {
		g_warning("socks5_connect: SOCKS5 connect request response read failed");
		return -1;
	}
	if (socks_req[0] != 5) {
		g_warning("socks5_connect: SOCKS5 response has invalid version");
		return -1;
	}
	if (socks_req[1] != 0) {
		g_warning("socks5_connect: SOCKS5 connection to %s:%u failed. (%u)",
				hostname, port, socks_req[1]);
		return -1;
	}

	size = 10;
	if (socks_req[3] == 3)
		size = 5 + socks_req[4] + 2;
	else if (socks_req[3] == 4)
		size = 4 + 16 + 2;
	if (size > 10) {
		size -= 10;
		if (sock_read(sock, (gchar *)socks_req + 10, size) != size) {
			g_warning("socks5_connect: SOCKS5 connect request response read failed");
			return -1;
		}
	}

	/* replace sock->hostname with endpoint */
	if (sock->hostname != hostname) {
		g_free(sock->hostname);
		sock->hostname = g_strdup(hostname);
		sock->port = port;
	}

	debug_print("socks5_connect: SOCKS5 connection to %s:%u successful.\n", hostname, port);

	return 0;
}
