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

#ifndef __SSL_CHECK_H__
#define __SSL_CHECK_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#if USE_SSL

#include <openssl/ssl.h>
#include <glib.h>

typedef enum {
	SSL_CERTIFICATE_OK,
	SSL_CERTIFICATE_UNKNOWN,
	SSL_CERTIFICATE_CHANGED
} SSLCertificateStatus;

typedef struct _SSLCertificate SSLCertificate;

struct _SSLCertificate
{
	gchar *host;
	gchar *issuer;
	gchar *subject;
	gchar *fingerprint;
};

gboolean ssl_certificate_check (X509 *x509_cert, gchar *host, gchar *issuer, 
				gchar *subject, gchar *md);

#endif /* USE_SSL */
#endif /* SSL_CHECK_H */
