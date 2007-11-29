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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#if (defined(USE_OPENSSL) || defined (USE_GNUTLS))
#if USE_OPENSSL
#include <openssl/ssl.h>
#else
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#endif
#include <glib.h>
#include <glib/gi18n.h>

#ifdef G_OS_WIN32
#include "winsock2.h"
#endif
#include "ssl_certificate.h"
#include "utils.h"
#include "log.h"
#include "socket.h"
#include "hooks.h"
#include "defs.h"

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

#if USE_OPENSSL
static SSLCertificate *ssl_certificate_new_lookup(X509 *x509_cert, gchar *host, gushort port, gboolean lookup);
#else
static SSLCertificate *ssl_certificate_new_lookup(gnutls_x509_crt x509_cert, gchar *host, gushort port, gboolean lookup);
#endif
#if USE_OPENSSL
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
#endif

static char * get_fqdn(char *host)
{
#ifdef INET6
        gint gai_err;
        struct addrinfo hints, *res;
#else
	struct hostent *hp;
#endif

	if (host == NULL || strlen(host) == 0)
		return g_strdup("");
#ifdef INET6
        memset(&hints, 0, sizeof(hints));
        hints.ai_flags = AI_CANONNAME;
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        gai_err = getaddrinfo(host, NULL, &hints, &res);
        if (gai_err != 0) {
                g_warning("getaddrinfo for %s failed: %s\n",
                          host, gai_strerror(gai_err));
		return g_strdup(host);
        }
	if (res != NULL) {
		if (res->ai_canonname && strlen(res->ai_canonname)) {
			gchar *fqdn = g_strdup(res->ai_canonname);
			freeaddrinfo(res);
			return fqdn;
		} else {
			freeaddrinfo(res);
			return g_strdup(host);
		}
	} else {
		return g_strdup(host);
	}
#else
	hp = my_gethostbyname(host);
	if (hp == NULL)
		return g_strdup(host); /*caller should free*/
	else 
		return g_strdup(hp->h_name);
#endif
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

#if USE_GNUTLS
static gnutls_x509_crt x509_crt_copy(gnutls_x509_crt src)
{
    int ret;
    size_t size;
    gnutls_datum tmp;
    gnutls_x509_crt dest;
    
    if (gnutls_x509_crt_init(&dest) != 0) {
    	g_warning("couldn't gnutls_x509_crt_init\n");
        return NULL;
    }

    if (gnutls_x509_crt_export(src, GNUTLS_X509_FMT_DER, NULL, &size) 
        != GNUTLS_E_SHORT_MEMORY_BUFFER) {
    	g_warning("couldn't gnutls_x509_crt_export to get size\n");
        gnutls_x509_crt_deinit(dest);
        return NULL;
    }

    tmp.data = malloc(size);
    memset(tmp.data, 0, size);
    ret = gnutls_x509_crt_export(src, GNUTLS_X509_FMT_DER, tmp.data, &size);
    if (ret == 0) {
        tmp.size = size;
        ret = gnutls_x509_crt_import(dest, &tmp, GNUTLS_X509_FMT_DER);
	if (ret) {
		g_warning("couldn't gnutls_x509_crt_import for real (%d %s)\n", ret, gnutls_strerror(ret));
		gnutls_x509_crt_deinit(dest);
		dest = NULL;
	}
    } else {
    	g_warning("couldn't gnutls_x509_crt_export for real (%d %s)\n", ret, gnutls_strerror(ret));
        gnutls_x509_crt_deinit(dest);
        dest = NULL;
    }

    free(tmp.data);
    return dest;
}
#endif

#if USE_OPENSSL
static SSLCertificate *ssl_certificate_new_lookup(X509 *x509_cert, gchar *host, gushort port, gboolean lookup)
#else
static SSLCertificate *ssl_certificate_new_lookup(gnutls_x509_crt x509_cert, gchar *host, gushort port, gboolean lookup)
#endif
{
	SSLCertificate *cert = g_new0(SSLCertificate, 1);
	unsigned int n;
	unsigned char md[128];	

	if (host == NULL || x509_cert == NULL) {
		ssl_certificate_destroy(cert);
		return NULL;
	}
#if USE_OPENSSL
	cert->x509_cert = X509_dup(x509_cert);
#else
	cert->x509_cert = x509_crt_copy(x509_cert);
	cert->status = (guint)-1;
#endif
	if (lookup)
		cert->host = get_fqdn(host);
	else
		cert->host = g_strdup(host);
	cert->port = port;
	
	/* fingerprint */
#if USE_OPENSSL
	X509_digest(cert->x509_cert, EVP_md5(), md, &n);
	cert->fingerprint = readable_fingerprint(md, (int)n);
#else
	gnutls_x509_crt_get_fingerprint(cert->x509_cert, GNUTLS_DIG_MD5, md, &n);
	cert->fingerprint = readable_fingerprint(md, (int)n);
#endif
	return cert;
}

#ifdef USE_GNUTLS
static void i2d_X509_fp(FILE *fp, gnutls_x509_crt x509_cert)
{
	char output[10*1024];
	size_t cert_size = 10*1024;
	int r;
	
	if ((r = gnutls_x509_crt_export(x509_cert, GNUTLS_X509_FMT_DER, output, &cert_size)) < 0) {
		g_warning("couldn't export cert %s (%d)\n", gnutls_strerror(r), cert_size);
		return;
	}
	debug_print("writing %zd bytes\n",cert_size);
	if (fwrite(&output, 1, cert_size, fp) < cert_size) {
		g_warning("failed to write cert\n");
	}
}
static gnutls_x509_crt d2i_X509_fp(FILE *fp, int unused)
{
	gnutls_x509_crt cert = NULL;
	gnutls_datum tmp;
	struct stat s;
	int r;
	if (fstat(fileno(fp), &s) < 0) {
		perror("fstat");
		return NULL;
	}
	tmp.data = malloc(s.st_size);
	memset(tmp.data, 0, s.st_size);
	tmp.size = s.st_size;
	if (fread (tmp.data, 1, s.st_size, fp) < s.st_size) {
		perror("fread");
		return NULL;
	}

	gnutls_x509_crt_init(&cert);
	if ((r = gnutls_x509_crt_import(cert, &tmp, GNUTLS_X509_FMT_DER)) < 0) {
		g_warning("import failed: %s\n", gnutls_strerror(r));
		gnutls_x509_crt_deinit(cert);
		cert = NULL;
	}
	debug_print("got cert! %p\n", cert);
	return cert;
}
#endif

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

void ssl_certificate_destroy(SSLCertificate *cert) 
{
	if (cert == NULL)
		return;

	if (cert->x509_cert)
#if USE_OPENSSL
		X509_free(cert->x509_cert);
#else
		gnutls_x509_crt_deinit(cert->x509_cert);
#endif
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
#if USE_OPENSSL
	X509 *tmp_x509;
#else
	gnutls_x509_crt tmp_x509;
#endif
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
		debug_print("didn't get %s\n", file);
		g_free(file);
		file = get_certificate_path(fqdn_host, buf, NULL);
		fp = g_fopen(file, "rb");

		if (fp) {
			debug_print("got %s\n", file);
			must_rename = (fingerprint != NULL);
		}
	} else {
		debug_print("got %s first try\n", file);
	}
	if (fp == NULL) {
		g_free(file);
		g_free(fqdn_host);
		g_free(buf);
		return NULL;
	}
	
	if ((tmp_x509 = d2i_X509_fp(fp, 0)) != NULL) {
		cert = ssl_certificate_new_lookup(tmp_x509, fqdn_host, port, lookup);
		debug_print("got cert %p\n", cert);
#if USE_OPENSSL
		X509_free(tmp_x509);
#else
		gnutls_x509_crt_deinit(tmp_x509);
#endif
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
#ifdef USE_OPENSSL
 	if (cert_a == NULL || cert_b == NULL)
 		return FALSE;
	else if (!X509_cmp(cert_a->x509_cert, cert_b->x509_cert))
		return TRUE;
	else
		return FALSE;
#else
	char *output_a;
	char *output_b;
	size_t cert_size_a = 0, cert_size_b = 0;
	int r;

	if (cert_a == NULL || cert_b == NULL)
		return FALSE;

	if ((r = gnutls_x509_crt_export(cert_a->x509_cert, GNUTLS_X509_FMT_DER, NULL, &cert_size_a)) 
	    != GNUTLS_E_SHORT_MEMORY_BUFFER) {
		g_warning("couldn't gnutls_x509_crt_export to get size a %s\n", gnutls_strerror(r));
		return FALSE;
	}

	if ((r = gnutls_x509_crt_export(cert_b->x509_cert, GNUTLS_X509_FMT_DER, NULL, &cert_size_b))
	    != GNUTLS_E_SHORT_MEMORY_BUFFER) {
		g_warning("couldn't gnutls_x509_crt_export to get size b %s\n", gnutls_strerror(r));
		return FALSE;
	}

	output_a = malloc(cert_size_a);
	output_b = malloc(cert_size_b);
	if ((r = gnutls_x509_crt_export(cert_a->x509_cert, GNUTLS_X509_FMT_DER, output_a, &cert_size_a)) < 0) {
		g_warning("couldn't gnutls_x509_crt_export a %s\n", gnutls_strerror(r));
		return FALSE;
	}
	if ((r = gnutls_x509_crt_export(cert_b->x509_cert, GNUTLS_X509_FMT_DER, output_b, &cert_size_b)) < 0) {
		g_warning("couldn't gnutls_x509_crt_export b %s\n", gnutls_strerror(r));
		return FALSE;
	}
	if (cert_size_a != cert_size_b) {
		g_warning("size differ %d %d\n", cert_size_a, cert_size_b);
		return FALSE;
	}
	if (memcmp(output_a, output_b, cert_size_a)) {
		g_warning("contents differ\n");
		return FALSE;
	}
	
	return TRUE;
#endif
}

#if USE_OPENSSL
char *ssl_certificate_check_signer (X509 *cert) 
{
	X509_STORE_CTX store_ctx;
	X509_STORE *store;
	char *err_msg = NULL;

	store = X509_STORE_new();
	if (store == NULL) {
		g_print("Can't create X509_STORE\n");
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
#else
char *ssl_certificate_check_signer (gnutls_x509_crt cert, guint status) 
{
	if (status == (guint)-1)
		return g_strdup(_("Uncheckable"));

	if (status & GNUTLS_CERT_INVALID) {
		if (gnutls_x509_crt_check_issuer(cert, cert))
			return g_strdup(_("Self-signed certificate"));
	}
	if (status & GNUTLS_CERT_REVOKED)
		return g_strdup(_("Revoked certificate"));
	if (status & GNUTLS_CERT_SIGNER_NOT_FOUND)
		return g_strdup(_("No certificate issuer found"));
	if (status & GNUTLS_CERT_SIGNER_NOT_CA)
		return g_strdup(_("Certificate issuer is not a CA"));


	return NULL;
}
#endif

#if USE_OPENSSL
gboolean ssl_certificate_check (X509 *x509_cert, gchar *fqdn, gchar *host, gushort port)
#else
gboolean ssl_certificate_check (gnutls_x509_crt x509_cert, guint status, gchar *fqdn, gchar *host, gushort port)
#endif
{
	SSLCertificate *current_cert = NULL;
	SSLCertificate *known_cert;
	SSLCertHookData cert_hook_data;
	gchar *fqdn_host = NULL;	
	gchar *fingerprint;
	unsigned int n;
	unsigned char md[128];	

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

#if USE_GNUTLS
	current_cert->status = status;
#endif
	/* fingerprint */
#if USE_OPENSSL
	X509_digest(x509_cert, EVP_md5(), md, &n);
	fingerprint = readable_fingerprint(md, (int)n);
#else
	n = 128;
	gnutls_x509_crt_get_fingerprint(x509_cert, GNUTLS_DIG_MD5, md, &n);
	fingerprint = readable_fingerprint(md, (int)n);
#endif

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
#if USE_OPENSSL
	} else if (asn1toTime(X509_get_notAfter(current_cert->x509_cert)) < time(NULL)) {
#else
	} else if (gnutls_x509_crt_get_expiration_time(current_cert->x509_cert) < time(NULL)) {
#endif
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
