/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2012 Colin Leroy <colin@colino.net> 
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#ifdef USE_GNUTLS
#ifdef HAVE_LIBETPAN
#include <libetpan/libetpan.h>
#include <libetpan/libetpan_version.h>
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <errno.h>

#include "etpan-ssl.h"
#include "ssl_certificate.h"
#include "utils.h"
#include "log.h"
#include "prefs_account.h"

gboolean etpan_certificate_check(mailstream *stream, const char *host, gint port,
				 gboolean accept_if_valid)
{
#if (!defined LIBETPAN_API_CURRENT || LIBETPAN_API_CURRENT < 18)
	unsigned char *cert_der = NULL;
	int len;
	gnutls_x509_crt_t cert = NULL;
	gnutls_datum_t tmp;

	if (stream == NULL)
		return FALSE;

	len = (int)mailstream_ssl_get_certificate(stream, &cert_der);

	if (cert_der == NULL || len < 0) {
		g_warning("no cert presented");
		return FALSE;
	}

	tmp.data = malloc(len);
	memcpy(tmp.data, cert_der, len);
	tmp.size = len;
	gnutls_x509_crt_init(&cert);

	free(cert_der);

	if (gnutls_x509_crt_import(cert, &tmp, GNUTLS_X509_FMT_DER) < 0) {
		free(tmp.data);
		g_warning("IMAP: can't get cert");
		return FALSE;
	} else if (ssl_certificate_check(cert, (guint)-1, host, port, accept_if_valid) == TRUE) {
		free(tmp.data);
		gnutls_x509_crt_deinit(cert);
		return TRUE;
	} else {
		free(tmp.data);
		gnutls_x509_crt_deinit(cert);
		return FALSE;
	}
#else
	carray *certs_der = NULL;
	gint chain_len = 0, i;
	gnutls_x509_crt_t *certs = NULL;
	gboolean result;

	if (stream == NULL)
		return FALSE;

	certs_der = mailstream_get_certificate_chain(stream);
	if (!certs_der) {
		g_warning("could not get certs");
		return FALSE;
	}
	chain_len = carray_count(certs_der);

	certs = malloc(sizeof(gnutls_x509_crt_t) * chain_len);
	if  (certs == NULL) {
		g_warning("could not allocate certs");
		return FALSE;
	}

	result = TRUE;
	for (i = 0; i < chain_len; i++) {
		MMAPString *cert_str = carray_get(certs_der, i);
		gnutls_datum_t tmp;

		tmp.data = malloc(cert_str->len);
		memcpy(tmp.data, cert_str->str, cert_str->len);
		tmp.size = cert_str->len;

		mmap_string_free(cert_str);

		gnutls_x509_crt_init(&certs[i]);
		if (gnutls_x509_crt_import(certs[i], &tmp, GNUTLS_X509_FMT_DER) < 0)
			result = FALSE;

		free(tmp.data);
	}

	carray_free(certs_der);

	if (result == TRUE)
		result = ssl_certificate_check_chain(certs, chain_len, host, port,
						     accept_if_valid);

	for (i = 0; i < chain_len; i++)
		gnutls_x509_crt_deinit(certs[i]);
	free(certs);

	return result;
#endif
}

void etpan_connect_ssl_context_cb(struct mailstream_ssl_context * ssl_context, void * data)
{
	PrefsAccount *account = (PrefsAccount *)data;
	const gchar *cert_path = NULL;
	const gchar *password = NULL;
	gnutls_x509_crt_t x509 = NULL;
	gnutls_x509_privkey_t pkey = NULL;

	if (account->in_ssl_client_cert_file && *account->in_ssl_client_cert_file)
		cert_path = account->in_ssl_client_cert_file;
	if (account->in_ssl_client_cert_pass && *account->in_ssl_client_cert_pass)
		password = account->in_ssl_client_cert_pass;

	if (mailstream_ssl_set_client_certificate_data(ssl_context, NULL, 0) < 0 ||
	    mailstream_ssl_set_client_private_key_data(ssl_context, NULL, 0) < 0)
		debug_print("Impossible to set the client certificate.\n");
	x509 = ssl_certificate_get_x509_from_pem_file(cert_path);
	pkey = ssl_certificate_get_pkey_from_pem_file(cert_path);
	if (!(x509 && pkey)) {
		/* try pkcs12 format */
		ssl_certificate_get_x509_and_pkey_from_p12_file(cert_path, password, &x509, &pkey);
	}
	if (x509 && pkey) {
		unsigned char *x509_der = NULL, *pkey_der = NULL;
		size_t x509_len, pkey_len;

		x509_len = (size_t)gnutls_i2d_X509(x509, &x509_der);
		pkey_len = (size_t)gnutls_i2d_PrivateKey(pkey, &pkey_der);
		if (x509_len > 0 && pkey_len > 0) {
			if (mailstream_ssl_set_client_certificate_data(ssl_context, x509_der, x509_len) < 0 ||
			    mailstream_ssl_set_client_private_key_data(ssl_context, pkey_der, pkey_len) < 0) 
				log_error(LOG_PROTOCOL, _("Impossible to set the client certificate.\n"));
			g_free(x509_der);
			g_free(pkey_der);
		}
		gnutls_x509_crt_deinit(x509);
		gnutls_x509_privkey_deinit(pkey);
	}

#if (defined LIBETPAN_API_CURRENT && LIBETPAN_API_CURRENT >= 23)
	/* If we have a host name, rather than a numerical IP address, tell
	 * gnutls to send it in the Server Name Identification extension field,
	 * to give the server a chance to select the correct certificate in the
	 * virtual hosting case where multiple domain names are hosted on the
	 * same IP address. */
	if (account->use_tls_sni &&
			!is_numeric_host_address(account->recv_server)) {
		int r;

		r = mailstream_ssl_set_server_name(ssl_context, account->recv_server);
		debug_print("Set libetpan SSL mail stream server name indication to %s, status = %d\n",
			    account->recv_server, r);
	}
#endif /* LIBETPAN_API_CURRENT >= 23 */

}

#endif /* USE_GNUTLS */
#endif /* HAVE_LIBETPAN */
