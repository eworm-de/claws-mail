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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#if USE_SSL

#include <openssl/ssl.h>
#include <glib.h>
#include "ssl_certificate.h"
#include "alertpanel.h"
#include "utils.h"
#include "intl.h"

static void ssl_certificate_destroy(SSLCertificate *cert);

static char * readable_fingerprint(unsigned char *src, int len) 
{
	int i=0;
	char * ret;
	
	if (src == NULL)
		return NULL;
	ret = g_strdup("");
	while (i < len) {
		char *tmp2;
		if(i>0)
			tmp2 = g_strdup_printf("%s:%02X", ret, src[i]);
		else
			tmp2 = g_strdup_printf("%02X", src[i]);
		g_free(ret);
		ret = g_strdup(tmp2);
		g_free(tmp2);
		i++;
	}
	return ret;
}

SSLCertificate *ssl_certificate_new(X509 *x509_cert, gchar *host)
{
	SSLCertificate *cert = g_new0(SSLCertificate, 1);
	
	if (host == NULL || x509_cert == NULL) {
		ssl_certificate_destroy(cert);
		return NULL;
	}

	cert->x509_cert = X509_dup(x509_cert);
	cert->host = g_strdup(host);
	return cert;
}

static void ssl_certificate_save (SSLCertificate *cert)
{
	gchar *file;
	FILE *fp;
	file = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, 
			  "certs", G_DIR_SEPARATOR_S, NULL);
	
	if (!is_dir_exist(file))
		make_dir_hier(file);
	g_free(file);

	file = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, 
			  "certs", G_DIR_SEPARATOR_S,
			  cert->host, ".cert", NULL);

	fp = fopen(file, "w");
	if (fp == NULL) {
		g_free(file);
		alertpanel_error(_("Can't save certificate !"));
		return;
	}
	i2d_X509_fp(fp, cert->x509_cert);
	g_free(file);
	fclose(fp);

}

static char* ssl_certificate_to_string(SSLCertificate *cert)
{
	char *ret;
	int j;
	unsigned int n;
	unsigned char md[EVP_MAX_MD_SIZE];	
		
	X509_digest(cert->x509_cert, EVP_md5(), md, &n);
			
	ret = g_strdup_printf("  Issuer: %s\n  Subject: %s\n  Fingerprint: %s",
				X509_NAME_oneline(X509_get_issuer_name(cert->x509_cert), 0, 0),
				X509_NAME_oneline(X509_get_subject_name(cert->x509_cert), 0, 0),
				readable_fingerprint(md, (int)n));

	return ret;
}
	
void ssl_certificate_destroy(SSLCertificate *cert) 
{
	g_return_if_fail(cert != NULL);
	if (cert->x509_cert)
		X509_free(cert->x509_cert);
	if (cert->host)	
		g_free(cert->host);
	g_free(cert);
	cert = NULL;
}

static SSLCertificate *ssl_certificate_find (gchar *host)
{
	gchar *file;
	gchar buf[1024], *subject, *issuer, *fingerprint;
	SSLCertificate *cert = NULL;
	X509 *tmp_x509;
	FILE *fp;
	
	file = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, 
			  "certs", G_DIR_SEPARATOR_S,
			  host, ".cert", NULL);

	fp = fopen(file, "r");
	if (fp == NULL) {
		g_free(file);
		return NULL;
	}
	
	
	if ((tmp_x509 = d2i_X509_fp(fp, 0)) != NULL) {
		cert = ssl_certificate_new(tmp_x509, host);
		X509_free(tmp_x509);
	}
	fclose(fp);
	g_free(file);
	
	return cert;
}

static gboolean ssl_certificate_compare (SSLCertificate *cert_a, SSLCertificate *cert_b)
{
	if (cert_a == NULL || cert_b == NULL)
		return FALSE;
	else if (!X509_cmp(cert_a->x509_cert, cert_b->x509_cert))
		return TRUE;	
	else
		return FALSE;
}

static char *ssl_certificate_check_signer (X509 *cert) 
{
	X509_STORE_CTX store_ctx;
	X509_STORE *store;
	int ok = 0;
	char *cert_file = NULL;
	char *err_msg = NULL;

	store = X509_STORE_new();
	if (store == NULL) {
		printf("Can't create X509_STORE\n");
		return FALSE;
	}
	if (X509_STORE_set_default_paths(store)) 
		ok++;
	if (X509_STORE_load_locations(store, cert_file, NULL))
		ok++;

	if (ok == 0) {
		X509_STORE_free (store);
		return g_strdup(_("Can't load X509 default paths"));
	}
	
	X509_STORE_CTX_init (&store_ctx, store, cert, NULL);
	ok = X509_verify_cert (&store_ctx);
	
	if (ok == 0) {
		err_msg = g_strdup(X509_verify_cert_error_string(
					X509_STORE_CTX_get_error(&store_ctx)));
		debug_print("Can't check signer: %s\n", err_msg);
		X509_STORE_CTX_cleanup (&store_ctx);
		X509_STORE_free (store);
		return err_msg;
			
	}
	X509_STORE_CTX_cleanup (&store_ctx);
	X509_STORE_free (store);
	return NULL;
}

gboolean ssl_certificate_check (X509 *x509_cert, gchar *host)
{
	SSLCertificate *current_cert = ssl_certificate_new(x509_cert, host);
	SSLCertificate *known_cert;

	if (current_cert == NULL) {
		debug_print("Buggy certificate !\n");
		return FALSE;
	}

	known_cert = ssl_certificate_find (host);

	if (known_cert == NULL) {
		gint val;
		gchar *err_msg, *cur_cert_str;
		gchar *sig_status = NULL;
		
		cur_cert_str = ssl_certificate_to_string(current_cert);
		
		sig_status = ssl_certificate_check_signer(x509_cert);

		err_msg = g_strdup_printf(_("The SSL certificate presented by %s is unknown.\nPresented certificate is:\n%s\n\n%s%s"),
					  host,
					  cur_cert_str,
					  (sig_status == NULL)?"The presented certificate signature is correct.":"The presented certificate signature is not correct: ",
					  (sig_status == NULL)?"":sig_status);
		g_free (cur_cert_str);
		if (sig_status)
			g_free (sig_status);
 
		val = alertpanel(_("Warning"),
			       err_msg,
			       _("Accept and save"), _("Cancel connection"), NULL);
		g_free(err_msg);

		switch (val) {
			case G_ALERTALTERNATE:
				ssl_certificate_destroy(current_cert);
				return FALSE;
			default:
				ssl_certificate_save(current_cert);
				ssl_certificate_destroy(current_cert);
				return TRUE;
		}
	}
	else if (!ssl_certificate_compare (current_cert, known_cert)) {
		gint val;
		gchar *err_msg, *known_cert_str, *cur_cert_str, *sig_status;
		
		sig_status = ssl_certificate_check_signer(x509_cert);

		known_cert_str = ssl_certificate_to_string(known_cert);
		cur_cert_str = ssl_certificate_to_string(current_cert);
		err_msg = g_strdup_printf(_("The SSL certificate presented by %s differs from the known one.\nKnown certificate is:\n%s\nPresented certificate is:\n%s\n\n%s%s"),
					  host,
					  known_cert_str,
					  cur_cert_str,
					  (sig_status == NULL)?"The presented certificate signature is correct.":"The presented certificate signature is not correct: ",
					  (sig_status == NULL)?"":sig_status);
		g_free (cur_cert_str);
		g_free (known_cert_str);
		if (sig_status)
			g_free (sig_status);

		val = alertpanel(_("Warning"),
			       err_msg,
			       _("Accept and save"), _("Cancel connection"), NULL);
		g_free(err_msg);

		switch (val) {
			case G_ALERTALTERNATE:
				ssl_certificate_destroy(current_cert);
				ssl_certificate_destroy(known_cert);
				return FALSE;
			default:
				ssl_certificate_save(current_cert);
				ssl_certificate_destroy(current_cert);
				ssl_certificate_destroy(known_cert);
				return TRUE;
		}
	}

	ssl_certificate_destroy(current_cert);
	ssl_certificate_destroy(known_cert);
	return TRUE;
}

#endif /* USE_SSL */
