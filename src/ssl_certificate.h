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

#ifndef __SSL_CERTIFICATE_H__
#define __SSL_CERTIFICATE_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#if USE_SSL

#include <openssl/ssl.h>
#include <openssl/objects.h>
#include <glib.h>

typedef struct _SSLCertificate SSLCertificate;

struct _SSLCertificate
{
	X509 *x509_cert;
	gchar *host;
	gushort port;
};

gboolean ssl_certificate_check (X509 *x509_cert, gchar *host, gushort port);
SSLCertificate *ssl_certificate_find (gchar *host, gushort port);
char* ssl_certificate_to_string(SSLCertificate *cert);

#endif /* USE_SSL */
#endif /* SSL_CERTIFICATE_H */
