/*
 * Claws Mail -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2007 Colin Leroy <colin@colino.net> 
 * and the Claws Mail team
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

#ifndef __SSL_CERTIFICATE_H__
#define __SSL_CERTIFICATE_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#if (defined(USE_OPENSSL) || defined (USE_GNUTLS))
#if USE_OPENSSL
#include <openssl/ssl.h>
#include <openssl/objects.h>
#else
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#endif

#include <glib.h>

#define SSLCERT_ASK_HOOKLIST "sslcert_ask"

typedef struct _SSLCertificate SSLCertificate;

struct _SSLCertificate
{
#if USE_OPENSSL
	X509 *x509_cert;
#else
	gnutls_x509_crt x509_cert;
#endif
	gchar *host;
	gushort port;
	gchar *fingerprint;
#if USE_GNUTLS
	guint status;
#endif
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
#if USE_OPENSSL
gboolean ssl_certificate_check (X509 *x509_cert, gchar *fqdn, gchar *host, gushort port);
#else
gboolean ssl_certificate_check (gnutls_x509_crt x509_cert, guint status, gchar *fqdn, gchar *host, gushort port);
#endif
void ssl_certificate_destroy(SSLCertificate *cert);
void ssl_certificate_delete_from_disk(SSLCertificate *cert);
char * readable_fingerprint(unsigned char *src, int len);
#if USE_OPENSSL
char *ssl_certificate_check_signer (X509 *cert);
time_t asn1toTime(ASN1_TIME *asn1Time);
#else
char *ssl_certificate_check_signer (gnutls_x509_crt cert, guint status);
#endif

#endif /* USE_OPENSSL */
#endif /* SSL_CERTIFICATE_H */
