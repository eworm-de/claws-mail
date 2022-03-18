/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2012 Hiroyuki Yamamoto and the Claws Mail team
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
 * 
 */

#ifndef __SSL_H__
#define __SSL_H__

#ifdef HAVE_CONFIG_H
#include "claws-features.h"
#endif

typedef enum {
	SSL_NONE,
	SSL_TUNNEL,
	SSL_STARTTLS
} SSLType;

#ifdef USE_GNUTLS
#include <glib.h>

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#if GNUTLS_VERSION_NUMBER >= 0x030000
#include <gnutls/abstract.h>
#endif

#include "socket.h"

void ssl_init				(void);
void ssl_done				(void);
gboolean ssl_init_socket		(SockInfo	*sockinfo);
void ssl_done_socket			(SockInfo	*sockinfo);

typedef struct _SSLClientCertHookData SSLClientCertHookData;
struct _SSLClientCertHookData
{
	const void *account;
	const gchar *cert_path;
	gchar *password;
	gboolean is_smtp;
};

const gchar *claws_ssl_get_cert_file(void);
#endif /* USE_GNUTLS */

#endif /* __SSL_H__ */
