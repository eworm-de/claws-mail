/*
 * Claws Mail -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2007 Colin Leroy <colin@colino.net> 
 * and the Claws Mail team
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

#include <openssl/ssl.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "ssl_certificate.h"
#include "utils.h"
#include "log.h"
#include "socket.h"
#include "hooks.h"

static GHashTable *warned_expired = NULL;

gboolean prefs_common_unsafe_ssl_certs(void);

static gchar *get_certificate_path(const gchar *host, const gchar *port, const gchar *fp)
{
	if (fp != NULL && prefs_common_unsafe_ssl_certs())
		return g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, 
			  "certs", G_DIR_SEPARATOR_S,
			  host, ".", port, ".", fp, ".cert", NULL);
	else 
		return g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, 
			  "certs", G_DIR_SEPARATOR_S,
			  host, ".", port, ".cert", NULL);
}

static SSLCertificate *ssl_certificate_new_lookup(X509 *x509_cert, gchar *host, gushort port, gboolean lookup);

/* from Courier */
time_t asn1toTime(ASN1_TIME *asn1Time)
{
	struct tm tm;
	int offset;

	if (asn1Time == NULL || asn1Time->length < 13)
		return 0;

	memset(&tm, 0, sizeof(tm));

#define N2(n)	((asn1Time->data[n]-'0')*10 + asn1Time->data[(n)+1]-'0')

#define CPY(f,n) (tm.f=N2(n))

	CPY(tm_year,0);

	if(tm.tm_year < 50)
		tm.tm_year += 100; /* Sux */

	CPY(tm_mon, 2);
	--tm.tm_mon;
	CPY(tm_mday, 4);
	CPY(tm_hour, 6);
	CPY(tm_min, 8);
	CPY(tm_sec, 10);

	offset=0;

	if (asn1Time->data[12] != 'Z')
	{
		if (asn1Time->length < 17)
			return 0;

		offset=N2(13)*3600+N2(15)*60;

		if (asn1Time->data[12] == '-')
			offset= -offset;
	}

#undef N2
#undef CPY

	return mktime(&tm)-offset;
}

static char * get_fqdn(char *host)
{
	struct hostent *hp;

	if (host == NULL || strlen(host) == 0)
		return g_strdup("");

	hp = my_gethostbyname(host);
	if (hp == NULL)
		return g_strdup(host); /*caller should free*/
	else 
		return g_strdup(hp->h_name);
}

char * readable_fingerprint(unsigned char *src, int len) 
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

static SSLCertificate *ssl_certificate_new_lookup(X509 *x509_cert, gchar *host, gushort port, gboolean lookup)
{
	SSLCertificate *cert = g_new0(SSLCertificate, 1);
	unsigned int n;
	unsigned char md[EVP_MAX_MD_SIZE];	
	
	if (host == NULL || x509_cert == NULL) {
		ssl_certificate_destroy(cert);
		return NULL;
	}
	cert->x509_cert = X509_dup(x509_cert);
	if (lookup)
		cert->host = get_fqdn(host);
	else
		cert->host = g_strdup(host);
	cert->port = port;
	
	/* fingerprint */
	X509_digest(cert->x509_cert, EVP_md5(), md, &n);
	cert->fingerprint = readable_fingerprint(md, (int)n);

	return cert;
}

static void ssl_certificate_save (SSLCertificate *cert)
{
	gchar *file, *port;
	FILE *fp;

	file = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, 
			  "certs", G_DIR_SEPARATOR_S, NULL);
	
	if (!is_dir_exist(file))
		make_dir_hier(file);
	g_free(file);

	port = g_strdup_printf("%d", cert->port);
	file = get_certificate_path(cert->host, port, cert->fingerprint);

	g_free(port);
	fp = g_fopen(file, "wb");
	if (fp == NULL) {
		g_free(file);
		debug_print("Can't save certificate !\n");
		return;
	}
	i2d_X509_fp(fp, cert->x509_cert);
	g_free(file);
	fclose(fp);

}

char* ssl_certificate_to_string(SSLCertificate *cert)
{
	char *ret, buf[100];
	char *issuer_commonname, *issuer_location, *issuer_organization;
	char *subject_commonname, *subject_location, *subject_organization;
	char *fingerprint, *sig_status;
	unsigned int n;
	unsigned char md[EVP_MAX_MD_SIZE];	
	
	/* issuer */	
	if (X509_NAME_get_text_by_NID(X509_get_issuer_name(cert->x509_cert), 
				       NID_commonName, buf, 100) >= 0)
		issuer_commonname = g_strdup(buf);
	else
		issuer_commonname = g_strdup(_("<not in certificate>"));
	if (X509_NAME_get_text_by_NID(X509_get_issuer_name(cert->x509_cert), 
				       NID_localityName, buf, 100) >= 0) {
		issuer_location = g_strdup(buf);
		if (X509_NAME_get_text_by_NID(X509_get_issuer_name(cert->x509_cert), 
				       NID_countryName, buf, 100) >= 0)
			issuer_location = g_strconcat(issuer_location,", ",buf, NULL);
	} else if (X509_NAME_get_text_by_NID(X509_get_issuer_name(cert->x509_cert), 
				       NID_countryName, buf, 100) >= 0)
		issuer_location = g_strdup(buf);
	else
		issuer_location = g_strdup(_("<not in certificate>"));

	if (X509_NAME_get_text_by_NID(X509_get_issuer_name(cert->x509_cert), 
				       NID_organizationName, buf, 100) >= 0)
		issuer_organization = g_strdup(buf);
	else 
		issuer_organization = g_strdup(_("<not in certificate>"));
	 
	/* subject */	
	if (X509_NAME_get_text_by_NID(X509_get_subject_name(cert->x509_cert), 
				       NID_commonName, buf, 100) >= 0)
		subject_commonname = g_strdup(buf);
	else
		subject_commonname = g_strdup(_("<not in certificate>"));
	if (X509_NAME_get_text_by_NID(X509_get_subject_name(cert->x509_cert), 
				       NID_localityName, buf, 100) >= 0) {
		subject_location = g_strdup(buf);
		if (X509_NAME_get_text_by_NID(X509_get_subject_name(cert->x509_cert), 
				       NID_countryName, buf, 100) >= 0)
			subject_location = g_strconcat(subject_location,", ",buf, NULL);
	} else if (X509_NAME_get_text_by_NID(X509_get_subject_name(cert->x509_cert), 
				       NID_countryName, buf, 100) >= 0)
		subject_location = g_strdup(buf);
	else
		subject_location = g_strdup(_("<not in certificate>"));

	if (X509_NAME_get_text_by_NID(X509_get_subject_name(cert->x509_cert), 
				       NID_organizationName, buf, 100) >= 0)
		subject_organization = g_strdup(buf);
	else 
		subject_organization = g_strdup(_("<not in certificate>"));
	 
	/* fingerprint */
	X509_digest(cert->x509_cert, EVP_md5(), md, &n);
	fingerprint = readable_fingerprint(md, (int)n);

	/* signature */
	sig_status = ssl_certificate_check_signer(cert->x509_cert);

	ret = g_strdup_printf(_("  Owner: %s (%s) in %s\n  Signed by: %s (%s) in %s\n  Fingerprint: %s\n  Signature status: %s"),
				subject_commonname, subject_organization, subject_location, 
				issuer_commonname, issuer_organization, issuer_location, 
				fingerprint,
				(sig_status==NULL ? "correct":sig_status));

	g_free(issuer_commonname);
	g_free(issuer_location);
	g_free(issuer_organization);
	g_free(subject_commonname);
	g_free(subject_location);
	g_free(subject_organization);
	g_free(fingerprint);
	g_free(sig_status);
	return ret;
}
	
void ssl_certificate_destroy(SSLCertificate *cert) 
{
	if (cert == NULL)
		return;

	if (cert->x509_cert)
		X509_free(cert->x509_cert);
	g_free(cert->host);
	g_free(cert->fingerprint);
	g_free(cert);
	cert = NULL;
}

void ssl_certificate_delete_from_disk(SSLCertificate *cert)
{
	gchar *buf;
	gchar *file;
	buf = g_strdup_printf("%d", cert->port);
	file = get_certificate_path(cert->host, buf, cert->fingerprint);
	g_unlink (file);
	g_free(file);
	g_free(buf);
}

SSLCertificate *ssl_certificate_find (gchar *host, gushort port, const gchar *fingerprint)
{
	return ssl_certificate_find_lookup (host, port, fingerprint, TRUE);
}

SSLCertificate *ssl_certificate_find_lookup (gchar *host, gushort port, const gchar *fingerprint, gboolean lookup)
{
	gchar *file = NULL;
	gchar *buf;
	gchar *fqdn_host;
	SSLCertificate *cert = NULL;
	X509 *tmp_x509;
	FILE *fp = NULL;
	gboolean must_rename = FALSE;

	if (lookup)
		fqdn_host = get_fqdn(host);
	else
		fqdn_host = g_strdup(host);

	buf = g_strdup_printf("%d", port);
	
	if (fingerprint != NULL) {
		file = get_certificate_path(fqdn_host, buf, fingerprint);
		fp = g_fopen(file, "rb");
	}
	if (fp == NULL) {
		/* see if we have the old one */
		g_free(file);
		file = get_certificate_path(fqdn_host, buf, NULL);
		fp = g_fopen(file, "rb");

		if (fp)
			must_rename = (fingerprint != NULL);
	}
	if (fp == NULL) {
		g_free(file);
		g_free(fqdn_host);
		g_free(buf);
		return NULL;
	}
	
	if ((tmp_x509 = d2i_X509_fp(fp, 0)) != NULL) {
		cert = ssl_certificate_new_lookup(tmp_x509, fqdn_host, port, lookup);
		X509_free(tmp_x509);
	}
	fclose(fp);
	g_free(file);
	
	if (must_rename) {
		gchar *old = get_certificate_path(fqdn_host, buf, NULL);
		gchar *new = get_certificate_path(fqdn_host, buf, fingerprint);
		if (strcmp(old, new))
			move_file(old, new, TRUE);
		g_free(old);
		g_free(new);
	}
	g_free(buf);
	g_free(fqdn_host);

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

char *ssl_certificate_check_signer (X509 *cert) 
{
	X509_STORE_CTX store_ctx;
	X509_STORE *store;
	char *err_msg = NULL;

	store = X509_STORE_new();
	if (store == NULL) {
		printf("Can't create X509_STORE\n");
		return NULL;
	}
	if (!X509_STORE_set_default_paths(store)) {
		X509_STORE_free (store);
		return g_strdup(_("Couldn't load X509 default paths"));
	}
	
	X509_STORE_CTX_init (&store_ctx, store, cert, NULL);

	if(!X509_verify_cert (&store_ctx)) {
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

gboolean ssl_certificate_check (X509 *x509_cert, gchar *fqdn, gchar *host, gushort port)
{
	SSLCertificate *current_cert = NULL;
	SSLCertificate *known_cert;
	SSLCertHookData cert_hook_data;
	gchar *fqdn_host = NULL;	
	gchar *fingerprint;
	unsigned int n;
	unsigned char md[EVP_MAX_MD_SIZE];	

	if (fqdn)
		fqdn_host = g_strdup(fqdn);
	else if (host)
		fqdn_host = get_fqdn(host);
	else {
		g_warning("no host!\n");
		return FALSE;
	}
		
	current_cert = ssl_certificate_new_lookup(x509_cert, fqdn_host, port, FALSE);
	
	if (current_cert == NULL) {
		debug_print("Buggy certificate !\n");
		g_free(fqdn_host);
		return FALSE;
	}

	/* fingerprint */
	X509_digest(x509_cert, EVP_md5(), md, &n);
	fingerprint = readable_fingerprint(md, (int)n);

	known_cert = ssl_certificate_find_lookup (fqdn_host, port, fingerprint, FALSE);

	g_free(fingerprint);
	g_free(fqdn_host);

	if (known_cert == NULL) {
		cert_hook_data.cert = current_cert;
		cert_hook_data.old_cert = NULL;
		cert_hook_data.expired = FALSE;
		cert_hook_data.accept = FALSE;
		
		hooks_invoke(SSLCERT_ASK_HOOKLIST, &cert_hook_data);
		
		if (!cert_hook_data.accept) {
			ssl_certificate_destroy(current_cert);
			return FALSE;
		} else {
			ssl_certificate_save(current_cert);
			ssl_certificate_destroy(current_cert);
			return TRUE;
		}
	} else if (!ssl_certificate_compare (current_cert, known_cert)) {
		cert_hook_data.cert = current_cert;
		cert_hook_data.old_cert = known_cert;
		cert_hook_data.expired = FALSE;
		cert_hook_data.accept = FALSE;
		
		hooks_invoke(SSLCERT_ASK_HOOKLIST, &cert_hook_data);

		if (!cert_hook_data.accept) {
			ssl_certificate_destroy(current_cert);
			ssl_certificate_destroy(known_cert);
			return FALSE;
		} else {
			ssl_certificate_save(current_cert);
			ssl_certificate_destroy(current_cert);
			ssl_certificate_destroy(known_cert);
			return TRUE;
		}
	} else if (asn1toTime(X509_get_notAfter(current_cert->x509_cert)) < time(NULL)) {
		gchar *tmp = g_strdup_printf("%s:%d", current_cert->host, current_cert->port);
		
		if (warned_expired == NULL)
			warned_expired = g_hash_table_new(g_str_hash, g_str_equal);
		
		if (g_hash_table_lookup(warned_expired, tmp)) {
			g_free(tmp);
			ssl_certificate_destroy(current_cert);
			ssl_certificate_destroy(known_cert);
			return TRUE;
		}
			
		cert_hook_data.cert = current_cert;
		cert_hook_data.old_cert = NULL;
		cert_hook_data.expired = TRUE;
		cert_hook_data.accept = FALSE;
		
		hooks_invoke(SSLCERT_ASK_HOOKLIST, &cert_hook_data);

		if (!cert_hook_data.accept) {
			g_free(tmp);
			ssl_certificate_destroy(current_cert);
			ssl_certificate_destroy(known_cert);
			return FALSE;
		} else {
			g_hash_table_insert(warned_expired, tmp, GINT_TO_POINTER(1));
			ssl_certificate_destroy(current_cert);
			ssl_certificate_destroy(known_cert);
			return TRUE;
		}
	}

	ssl_certificate_destroy(current_cert);
	ssl_certificate_destroy(known_cert);
	return TRUE;
}

#endif /* USE_OPENSSL */
