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

static char * convert_fingerprint(char *src) 
{
	int i=0;
	char * ret;
	
	if (src == NULL)
		return NULL;
	ret = g_strdup("");
	while (src[i] != '\0') {
		char *tmp2;
		if(i>0)
			tmp2 = g_strdup_printf("%s:%02X", ret, src[i]);
		else
			tmp2 = g_strdup_printf("%02X", src[i]);
		ret = g_strdup(tmp2);
		g_free(tmp2);
		i++;
	}
	return ret;
}

SSLCertificate *ssl_certificate_new(gchar *host, gchar *issuer, gchar *subject, gchar *md)
{
	SSLCertificate *cert = g_new0(SSLCertificate, 1);
	
	if (host == NULL || issuer == NULL || subject == NULL || md == NULL) {
		ssl_certificate_destroy(cert);
		return NULL;
	}

	cert->host = g_strdup(host);
	cert->issuer = g_strdup(issuer);
	cert->subject = g_strdup(subject);
	cert->fingerprint = g_strdup(md);
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
	fputs("issuer=", fp);
	fputs(cert->issuer, fp);
	fputs("\nsubject=", fp);
	fputs(cert->subject, fp);
	fputs("\nfingerprint=", fp);
	fputs(cert->fingerprint, fp);
	fputs("\n", fp);
	fclose(fp);
}

static char* ssl_certificate_to_string(SSLCertificate *cert)
{
	char *ret;
	ret = g_strdup_printf("  Issuer: %s\n  Subject: %s\n  Fingerprint: %s",
				cert->issuer,
				cert->subject,
				cert->fingerprint);
	return ret;
}
	
void ssl_certificate_destroy(SSLCertificate *cert) 
{
	g_return_if_fail(cert != NULL);
	if (cert->host)	
		g_free(cert->host);
	if (cert->issuer)
		g_free(cert->issuer);
	if (cert->subject)
		g_free(cert->subject);
	if (cert->fingerprint)
		g_free(cert->fingerprint);
	g_free(cert);
	cert = NULL;
}

static SSLCertificate *ssl_certificate_find (gchar *host)
{
	gchar *file;
	gchar buf[1024], *subject, *issuer, *fingerprint;
	SSLCertificate *cert;
	FILE *fp;
	
	file = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, 
			  "certs", G_DIR_SEPARATOR_S,
			  host, ".cert", NULL);

	fp = fopen(file, "r");
	if (fp == NULL) {
		g_free(file);
		return NULL;
	}
	
	while( fgets( buf, sizeof( buf ), fp ) != NULL ) {
		if (!strncmp(buf, "subject=", 8)) {
			subject = g_strdup((char *)buf +8);
			g_strdelimit(subject, "\r\n", '\0');
		}
		else if (!strncmp(buf, "issuer=", 7)) {
			issuer = g_strdup((char *)buf +7);
			g_strdelimit(issuer, "\r\n", '\0');
		}
		else if (!strncmp(buf, "fingerprint=", 12)) {
			fingerprint = g_strdup((char *)buf +12);
			g_strdelimit(fingerprint, "\r\n", '\0');
		}
	}
	fclose (fp);
	if (subject && issuer && fingerprint) {
		cert = ssl_certificate_new(host, issuer, subject, fingerprint);
	}
	
	g_free(file);
	
	if (subject)
		g_free(subject);
	if (issuer)
		g_free(issuer);
	if (fingerprint)
		g_free(fingerprint);

	return cert;
}

static gboolean ssl_certificate_compare (SSLCertificate *cert_a, SSLCertificate *cert_b)
{
	if (!strcmp(cert_a->issuer, cert_b->issuer)
	&&  !strcmp(cert_a->subject, cert_b->subject)
	&&  !strcmp(cert_a->fingerprint, cert_b->fingerprint))
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

gboolean ssl_certificate_check (X509 *x509_cert, gchar *host, gchar *issuer, 
				gchar *subject, gchar *md) 
{
	char *readable_md = convert_fingerprint(md);
	SSLCertificate *current_cert = ssl_certificate_new(host, issuer, subject, readable_md);
	SSLCertificate *known_cert;

	if (current_cert == NULL) {
		debug_print("Buggy certificate !\n");
		debug_print("host: %s\n", host);
		debug_print("issuer: %s\n", issuer);
		debug_print("subject: %s\n", subject);
		debug_print("md: %s\n", readable_md);
		if (readable_md)
			g_free(readable_md);
		return FALSE;
	}

	if (readable_md)
		g_free(readable_md);

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
