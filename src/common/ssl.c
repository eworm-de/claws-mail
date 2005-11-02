/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2002 Hiroyuki Yamamoto
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#if USE_OPENSSL

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>

#include "sylpheed.h"
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
	SSL *ssl;
	gboolean done;
} thread_data;
#endif


static SSL_CTX *ssl_ctx;

void ssl_init(void)
{
	SSL_METHOD *meth;

	/* Global system initialization*/
	SSL_library_init();
	SSL_load_error_strings();

#ifdef HAVE_LIBETPAN
	mailstream_ssl_init_not_required();
#endif	

	/* Create our context*/
	meth = SSLv23_client_method();
	ssl_ctx = SSL_CTX_new(meth);

	/* Set default certificate paths */
	SSL_CTX_set_default_verify_paths(ssl_ctx);
	
#if (OPENSSL_VERSION_NUMBER < 0x0090600fL)
	SSL_CTX_set_verify_depth(ssl_ctx,1);
#endif
}

void ssl_done(void)
{
	if (!ssl_ctx)
		return;
	
	SSL_CTX_free(ssl_ctx);
}

#ifdef USE_PTHREAD
void *SSL_connect_thread(void *data)
{
	thread_data *td = (thread_data *)data;
	int result = SSL_connect(td->ssl);
	td->done = TRUE; /* let the caller thread join() */
	return GINT_TO_POINTER(result);
}
#endif

gint SSL_connect_nb(SSL *ssl)
{
#if (defined USE_PTHREAD && defined __GLIBC__ && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 3)))
	thread_data *td = g_new0(thread_data, 1);
	pthread_t pt;
	void *res = NULL;
	
	td->ssl  = ssl;
	td->done = FALSE;
	
	/* try to create a thread to initialize the SSL connection,
	 * fallback to blocking method in case of problem 
	 */
	if (pthread_create(&pt, PTHREAD_CREATE_JOINABLE, 
			SSL_connect_thread, td) != 0)
		return SSL_connect(ssl);
	
	debug_print("waiting for SSL_connect thread...\n");
	while(!td->done) {
		/* don't let the interface freeze while waiting */
		sylpheed_do_idle();
	}

	/* get the thread's return value and clean its resources */
	pthread_join(pt, &res);
	g_free(td);

	debug_print("SSL_connect thread returned %d\n", 
			GPOINTER_TO_INT(res));
	
	return GPOINTER_TO_INT(res);
#else
	return SSL_connect(ssl);
#endif
}

gboolean ssl_init_socket(SockInfo *sockinfo)
{
	return ssl_init_socket_with_method(sockinfo, SSL_METHOD_SSLv23);
}

gboolean ssl_init_socket_with_method(SockInfo *sockinfo, SSLMethod method)
{
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


	if (!ssl_certificate_check(server_cert, sockinfo->hostname, sockinfo->port)) {
		X509_free(server_cert);
		SSL_free(ssl);
		return FALSE;
	}


	X509_free(server_cert);
	sockinfo->ssl = ssl;

	return TRUE;
}

void ssl_done_socket(SockInfo *sockinfo)
{
	if (sockinfo->ssl) {
		SSL_free(sockinfo->ssl);
	}
}

#endif /* USE_OPENSSL */
