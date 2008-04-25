/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2007 Hiroyuki Yamamoto and the Claws Mail team
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
#endif

#if (defined(USE_OPENSSL) || defined (USE_GNUTLS))
#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>

#include "claws.h"
#include "utils.h"
#include "ssl.h"
#include "ssl_certificate.h"

#ifdef HAVE_LIBETPAN
#include <libetpan/mailstream_ssl.h>
#endif

#ifdef USE_PTHREAD
#include <pthread.h>
#endif

#ifdef USE_PTHREAD
typedef struct _thread_data {
#ifdef USE_OPENSSL
	SSL *ssl;
#else
	gnutls_session ssl;
#endif
	gboolean done;
} thread_data;
#endif


#ifdef USE_OPENSSL
static SSL_CTX *ssl_ctx;
#endif

void ssl_init(void)
{
#ifdef USE_OPENSSL
	SSL_METHOD *meth;

	/* Global system initialization*/
	SSL_library_init();
	SSL_load_error_strings();

#ifdef HAVE_LIBETPAN
	mailstream_openssl_init_not_required();
#endif	

	/* Create our context*/
	meth = SSLv23_client_method();
	ssl_ctx = SSL_CTX_new(meth);

	/* Set default certificate paths */
	SSL_CTX_set_default_verify_paths(ssl_ctx);
	
#if (OPENSSL_VERSION_NUMBER < 0x0090600fL)
	SSL_CTX_set_verify_depth(ssl_ctx,1);
#endif
#else
	gnutls_global_init();
#endif
}

void ssl_done(void)
{
#if USE_OPENSSL
	if (!ssl_ctx)
		return;
	
	SSL_CTX_free(ssl_ctx);
#else
	gnutls_global_deinit();
#endif
}

#ifdef USE_PTHREAD
static void *SSL_connect_thread(void *data)
{
	thread_data *td = (thread_data *)data;
	int result = -1;

	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

#ifdef USE_OPENSSL
	result = SSL_connect(td->ssl);
#else
	do {
		result = gnutls_handshake(td->ssl);
	} while (result == GNUTLS_E_AGAIN || result == GNUTLS_E_INTERRUPTED);
#endif
	td->done = TRUE; /* let the caller thread join() */
	return GINT_TO_POINTER(result);
}
#endif

#ifdef USE_OPENSSL
static gint SSL_connect_nb(SSL *ssl)
#else
static gint SSL_connect_nb(gnutls_session ssl)
#endif
{
#ifdef USE_GNUTLS
	int result;
#endif
#ifdef USE_PTHREAD
	thread_data *td = g_new0(thread_data, 1);
	pthread_t pt;
	pthread_attr_t pta;
	void *res = NULL;
	time_t start_time = time(NULL);
	gboolean killed = FALSE;
	
	td->ssl  = ssl;
	td->done = FALSE;
	
	/* try to create a thread to initialize the SSL connection,
	 * fallback to blocking method in case of problem 
	 */
	if (pthread_attr_init(&pta) != 0 ||
	    pthread_attr_setdetachstate(&pta, PTHREAD_CREATE_JOINABLE) != 0 ||
	    pthread_create(&pt, &pta, SSL_connect_thread, td) != 0) {
#ifdef USE_OPENSSL
		return SSL_connect(ssl);
#else
		do {
			result = gnutls_handshake(td->ssl);
		} while (result == GNUTLS_E_AGAIN || result == GNUTLS_E_INTERRUPTED);
		return result;
#endif
	}
	debug_print("waiting for SSL_connect thread...\n");
	while(!td->done) {
		/* don't let the interface freeze while waiting */
		claws_do_idle();
		if (time(NULL) - start_time > 30) {
			pthread_cancel(pt);
			td->done = TRUE;
			killed = TRUE;
		}
	}

	/* get the thread's return value and clean its resources */
	pthread_join(pt, &res);
	g_free(td);
	
	if (killed) {
		res = GINT_TO_POINTER(-1);
	}
	debug_print("SSL_connect thread returned %d\n", 
			GPOINTER_TO_INT(res));
	
	return GPOINTER_TO_INT(res);
#else
#ifdef USE_OPENSSL
	return SSL_connect(ssl);
#else
	do {
#ifdef USE_PTHRED
		result = gnutls_handshake(td->ssl);
#else
		result = gnutls_handshake(ssl);
#endif
	} while (result == GNUTLS_E_AGAIN || result == GNUTLS_E_INTERRUPTED);
#endif
#endif
}

gboolean ssl_init_socket(SockInfo *sockinfo)
{
	return ssl_init_socket_with_method(sockinfo, SSL_METHOD_SSLv23);
}

#ifdef USE_GNUTLS
static const gchar *ssl_get_cert_file(void)
{
	if (g_getenv("SSL_CERT_FILE"))
		return g_getenv("SSL_CERT_FILE");
#ifndef G_OS_WIN32
	return "/etc/ssl/certs/ca-certificates.crt";
#else
	return "put_what_s_needed_here";
#endif
}
#endif

gboolean ssl_init_socket_with_method(SockInfo *sockinfo, SSLMethod method)
{
#ifdef USE_OPENSSL
	X509 *server_cert;
	SSL *ssl;

	ssl = SSL_new(ssl_ctx);
	if (ssl == NULL) {
		g_warning(_("Error creating ssl context\n"));
		return FALSE;
	}

	switch (method) {
	case SSL_METHOD_SSLv23:
		debug_print("Setting SSLv23 client method\n");
		SSL_set_ssl_method(ssl, SSLv23_client_method());
		break;
	case SSL_METHOD_TLSv1:
		debug_print("Setting TLSv1 client method\n");
		SSL_set_ssl_method(ssl, TLSv1_client_method());
		break;
	default:
		break;
	}

	SSL_set_fd(ssl, sockinfo->sock);
	if (SSL_connect_nb(ssl) == -1) {
		g_warning(_("SSL connect failed (%s)\n"),
			    ERR_error_string(ERR_get_error(), NULL));
		SSL_free(ssl);
		return FALSE;
	}

	/* Get the cipher */

	debug_print("SSL connection using %s\n", SSL_get_cipher(ssl));

	/* Get server's certificate (note: beware of dynamic allocation) */
	if ((server_cert = SSL_get_peer_certificate(ssl)) == NULL) {
		debug_print("server_cert is NULL ! this _should_not_ happen !\n");
		SSL_free(ssl);
		return FALSE;
	}


	if (!ssl_certificate_check(server_cert, sockinfo->canonical_name, sockinfo->hostname, sockinfo->port)) {
		X509_free(server_cert);
		SSL_free(ssl);
		return FALSE;
	}


	X509_free(server_cert);
	sockinfo->ssl = ssl;
#else
	gnutls_session session;
	int r;
	const int cipher_prio[] = { GNUTLS_CIPHER_AES_128_CBC,
		  		GNUTLS_CIPHER_3DES_CBC,
		  		GNUTLS_CIPHER_AES_256_CBC,
		  		GNUTLS_CIPHER_ARCFOUR_128, 0 };
	const int kx_prio[] = { GNUTLS_KX_DHE_RSA,
			   GNUTLS_KX_RSA, 
			   GNUTLS_KX_DHE_DSS, 0 };
	const int mac_prio[] = { GNUTLS_MAC_SHA1,
		  		GNUTLS_MAC_MD5, 0 };
	const int proto_prio[] = { GNUTLS_TLS1,
		  		  GNUTLS_SSL3, 0 };
	const gnutls_datum *raw_cert_list;
	unsigned int raw_cert_list_length;
	gnutls_x509_crt cert = NULL;
	guint status;
	gnutls_certificate_credentials_t xcred;

	if (gnutls_certificate_allocate_credentials (&xcred) != 0)
		return FALSE;

	r = gnutls_init(&session, GNUTLS_CLIENT);
	if (session == NULL || r != 0)
		return FALSE;
  
	gnutls_set_default_priority(session);
	gnutls_protocol_set_priority (session, proto_prio);
	gnutls_cipher_set_priority (session, cipher_prio);
	gnutls_kx_set_priority (session, kx_prio);
	gnutls_mac_set_priority (session, mac_prio);

	gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE, xcred);

	r = gnutls_certificate_set_x509_trust_file(xcred, ssl_get_cert_file(),  GNUTLS_X509_FMT_PEM);
	if (r < 0)
		g_warning("Can't read SSL_CERT_FILE %s: %s\n",
			ssl_get_cert_file(), 
			gnutls_strerror(r));

	gnutls_certificate_set_verify_flags (xcred, GNUTLS_VERIFY_ALLOW_X509_V1_CA_CRT);

	gnutls_transport_set_ptr(session, (gnutls_transport_ptr) 
		sockinfo->sock);

	gnutls_dh_set_prime_bits(session, 512);

	if ((r = SSL_connect_nb(session)) < 0) {
		g_warning("SSL connection failed (%s)", gnutls_strerror(r));
		gnutls_certificate_free_credentials(xcred);
		gnutls_deinit(session);
		return FALSE;
	}

	/* Get server's certificate (note: beware of dynamic allocation) */
	raw_cert_list = gnutls_certificate_get_peers(session, &raw_cert_list_length);

	if (!raw_cert_list 
	||  gnutls_certificate_type_get(session) != GNUTLS_CRT_X509
	||  (r = gnutls_x509_crt_init(&cert)) < 0
	||  (r = gnutls_x509_crt_import(cert, &raw_cert_list[0], GNUTLS_X509_FMT_DER)) < 0) {
		g_warning("cert get failure: %d %s\n", r, gnutls_strerror(r));
		gnutls_certificate_free_credentials(xcred);
		gnutls_deinit(session);
		return FALSE;
	}

	r = gnutls_certificate_verify_peers2(session, &status);

	if (!ssl_certificate_check(cert, status, sockinfo->canonical_name, sockinfo->hostname, sockinfo->port)) {
		gnutls_x509_crt_deinit(cert);
		gnutls_certificate_free_credentials(xcred);
		gnutls_deinit(session);
		return FALSE;
	}

	gnutls_x509_crt_deinit(cert);

	sockinfo->ssl = session;
	sockinfo->xcred = xcred;
#endif
	return TRUE;
}

void ssl_done_socket(SockInfo *sockinfo)
{
	if (sockinfo && sockinfo->ssl) {
#ifdef USE_OPENSSL
		SSL_free(sockinfo->ssl);
#else
		gnutls_certificate_free_credentials(sockinfo->xcred);
		gnutls_deinit(sockinfo->ssl);
#endif
		sockinfo->ssl = NULL;
	}
}

#endif /* USE_OPENSSL */
