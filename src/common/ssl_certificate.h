/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2007 Hiroyuki Yamamoto
 * This file Copyright (C) 2002-2005 Colin Leroy <colin@colino.net>
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

#ifndef __SSL_CERTIFICATE_H__
#define __SSL_CERTIFICATE_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#if USE_OPENSSL

#include <openssl/ssl.h>
#include <openssl/objects.h>
#include <glib.h>

#define SSLCERT_ASK_HOOKLIST "sslcert_ask"

typedef struct _SSLCertificate SSLCertificate;

struct _SSLCertificate
{
	X509 *x509_cert;
	gchar *host;
	gushort port;
	gchar *fingerprint;
};

typedef struct _SSLCertHookData SSLCertHookData;

struct _SSLCertHookData
{
	SSLCertificate *cert;
	SSLCertificate *old_cert;
	gboolean expired;
	gboolean accept;
};

SSLCertificate *ssl_certificate_find (gchar *host, gushort port, const gchar *fingerprint);
SSLCertificate *ssl_certificate_find_lookup (gchar *host, gushort port, const gchar *fingerprint, gboolean lookup);
gboolean ssl_certificate_check (X509 *x509_cert, gchar *fqdn, gchar *host, gushort port);
char* ssl_certificate_to_string(SSLCertificate *cert);
void ssl_certificate_destroy(SSLCertificate *cert);
void ssl_certificate_delete_from_disk(SSLCertificate *cert);
char * readable_fingerprint(unsigned char *src, int len);
char *ssl_certificate_check_signer (X509 *cert);
time_t asn1toTime(ASN1_TIME *asn1Time);

#endif /* USE_OPENSSL */
#endif /* SSL_CERTIFICATE_H */
