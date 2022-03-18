/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2016 Colin Leroy and the Claws Mail team
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
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#ifdef USE_GNUTLS
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <gnutls/pkcs12.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <stdio.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <errno.h>
#ifdef G_OS_WIN32
#  include <winsock2.h>
#else
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <netdb.h>
#endif /* G_OS_WIN32 */
#include "ssl_certificate.h"
#include "utils.h"
#include "log.h"
#include "socket.h"
#include "hooks.h"
#include "defs.h"
#include "file-utils.h"

static GHashTable *warned_expired = NULL;

gboolean prefs_common_unsafe_ssl_certs(void);

static gchar *get_certificate_path(const gchar *host, const gchar *port, const gchar *fp)
{
	gchar *ret;
	gchar *filename;

	if (fp != NULL && prefs_common_unsafe_ssl_certs()) {
		filename = g_strconcat(host, ".", port, ".", fp, ".cert", NULL);
	} else {
		filename = g_strconcat(host, ".", port, ".cert", NULL);
	}
	subst_for_filename(filename);

	ret = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
		  "certs", G_DIR_SEPARATOR_S,
		  filename, NULL);

	g_free(filename);
	return ret;
}

static gchar *get_certificate_chain_path(const gchar *host, const gchar *port, const gchar *fp)
{
	gchar *tmp = get_certificate_path(host, port, fp);
	gchar *result = g_strconcat(tmp, ".chain", NULL);

	g_free(tmp);

	return result;
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
static gnutls_x509_crt_t x509_crt_copy(gnutls_x509_crt_t src)
{
    int ret;
    size_t size;
    gnutls_datum_t tmp;
    gnutls_x509_crt_t dest;
    size = 0;
    
    if (gnutls_x509_crt_init(&dest) != 0) {
	g_warning("couldn't gnutls_x509_crt_init");
        return NULL;
    }

    if (gnutls_x509_crt_export(src, GNUTLS_X509_FMT_DER, NULL, &size) 
        != GNUTLS_E_SHORT_MEMORY_BUFFER) {
	g_warning("couldn't gnutls_x509_crt_export to get size");
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
		g_warning("couldn't gnutls_x509_crt_import for real (%d %s)", ret, gnutls_strerror(ret));
		gnutls_x509_crt_deinit(dest);
		dest = NULL;
	}
    } else {
	g_warning("couldn't gnutls_x509_crt_export for real (%d %s)", ret, gnutls_strerror(ret));
        gnutls_x509_crt_deinit(dest);
        dest = NULL;
    }

    free(tmp.data);
    return dest;
}
#endif

static SSLCertificate *ssl_certificate_new(gnutls_x509_crt_t x509_cert, const gchar *host, gushort port)
{
	SSLCertificate *cert = g_new0(SSLCertificate, 1);
	size_t n;
	unsigned char md[128];	

	if (host == NULL || x509_cert == NULL) {
		ssl_certificate_destroy(cert);
		return NULL;
	}
	cert->x509_cert = x509_crt_copy(x509_cert);
	cert->status = (guint)-1;
	cert->host = g_strdup(host);
	cert->port = port;
	
	/* fingerprint */
	n = sizeof(md);
	gnutls_x509_crt_get_fingerprint(cert->x509_cert, GNUTLS_DIG_MD5, md, &n);
	cert->fingerprint = readable_fingerprint(md, (int)n);
	return cert;
}

static void gnutls_export_X509_fp(FILE *fp, gnutls_x509_crt_t x509_cert, gnutls_x509_crt_fmt_t format)
{
	char output[10*1024];
	size_t cert_size = 10*1024;
	int r;
	
	if ((r = gnutls_x509_crt_export(x509_cert, format, output, &cert_size)) < 0) {
		g_warning("couldn't export cert %s (%"G_GSIZE_FORMAT")", gnutls_strerror(r), cert_size);
		return;
	}
	debug_print("writing %" G_GSIZE_FORMAT " bytes\n",cert_size);
	if (claws_fwrite(&output, 1, cert_size, fp) < cert_size) {
		g_warning("failed to write cert: %d %s", errno, g_strerror(errno));
	}
}

size_t gnutls_i2d_X509(gnutls_x509_crt_t x509_cert, unsigned char **output)
{
	size_t cert_size = 10*1024;
	int r;
	
	if (output == NULL)
		return 0;
	
	*output = malloc(cert_size);

	if ((r = gnutls_x509_crt_export(x509_cert, GNUTLS_X509_FMT_DER, *output, &cert_size)) < 0) {
		g_warning("couldn't export cert %s (%"G_GSIZE_FORMAT")", gnutls_strerror(r), cert_size);
		free(*output);
		*output = NULL;
		return 0;
	}
	return cert_size;
}

size_t gnutls_i2d_PrivateKey(gnutls_x509_privkey_t pkey, unsigned char **output)
{
	size_t key_size = 10*1024;
	int r;
	
	if (output == NULL)
		return 0;
	
	*output = malloc(key_size);

	if ((r = gnutls_x509_privkey_export(pkey, GNUTLS_X509_FMT_DER, *output, &key_size)) < 0) {
		g_warning("couldn't export key %s (%"G_GSIZE_FORMAT")", gnutls_strerror(r), key_size);
		free(*output);
		*output = NULL;
		return 0;
	}
	return key_size;
}

static int gnutls_import_X509_list_fp(FILE *fp, gnutls_x509_crt_fmt_t format,
				   gnutls_x509_crt_t **cert_list, gint *num_certs)
{
	gnutls_x509_crt_t *crt_list;
	unsigned int max = 512;
	unsigned int flags = 0;
	gnutls_datum_t tmp;
	struct stat s;
	int r;

	*cert_list = NULL;
	*num_certs = 0;

	if (fp == NULL)
		return -ENOENT;

	if (fstat(fileno(fp), &s) < 0) {
		perror("fstat");
		return -errno;
	}

	crt_list=(gnutls_x509_crt_t*)malloc(max*sizeof(gnutls_x509_crt_t));
	tmp.data = malloc(s.st_size);
	memset(tmp.data, 0, s.st_size);
	tmp.size = s.st_size;
	if (claws_fread (tmp.data, 1, s.st_size, fp) < s.st_size) {
		perror("claws_fread");
		free(tmp.data);
		free(crt_list);
		return -EIO;
	}

	if ((r = gnutls_x509_crt_list_import(crt_list, &max, 
			&tmp, format, flags)) < 0) {
		debug_print("cert import failed: %s\n", gnutls_strerror(r));
		free(tmp.data);
		free(crt_list);
		return r;
	}
	free(tmp.data);
	debug_print("got %d certs in crt_list! %p\n", max, &crt_list);

	*cert_list = crt_list;
	*num_certs = max;

	return r;
}

/* return one certificate, read from file */
static gnutls_x509_crt_t gnutls_import_X509_fp(FILE *fp, gnutls_x509_crt_fmt_t format)
{
	gnutls_x509_crt_t *certs = NULL;
	gnutls_x509_crt_t cert = NULL;
	int i, ncerts, r;

	if ((r = gnutls_import_X509_list_fp(fp, format, &certs, &ncerts)) < 0) {
		return NULL;
	}

	if (ncerts == 0)
		return NULL;

	for (i = 1; i < ncerts; i++)
		gnutls_x509_crt_deinit(certs[i]);

	cert = certs[0];
	free(certs);

	return cert;
}

static gnutls_x509_privkey_t gnutls_import_key_fp(FILE *fp, gnutls_x509_crt_fmt_t format)
{
	gnutls_x509_privkey_t key = NULL;
	gnutls_datum_t tmp;
	struct stat s;
	int r;
	if (fstat(fileno(fp), &s) < 0) {
		perror("fstat");
		return NULL;
	}
	tmp.data = malloc(s.st_size);
	memset(tmp.data, 0, s.st_size);
	tmp.size = s.st_size;
	if (claws_fread (tmp.data, 1, s.st_size, fp) < s.st_size) {
		perror("claws_fread");
		free(tmp.data);
		return NULL;
	}

	gnutls_x509_privkey_init(&key);
	if ((r = gnutls_x509_privkey_import(key, &tmp, format)) < 0) {
		debug_print("key import failed: %s\n", gnutls_strerror(r));
		gnutls_x509_privkey_deinit(key);
		key = NULL;
	}
	free(tmp.data);
	debug_print("got key! %p\n", key);
	return key;
}

static gnutls_pkcs12_t gnutls_import_PKCS12_fp(FILE *fp, gnutls_x509_crt_fmt_t format)
{
	gnutls_pkcs12_t p12 = NULL;
	gnutls_datum_t tmp;
	struct stat s;
	int r;
	if (fstat(fileno(fp), &s) < 0) {
		log_error(LOG_PROTOCOL, _("Cannot stat P12 certificate file (%s)\n"),
				  g_strerror(errno));
		return NULL;
	}
	tmp.data = malloc(s.st_size);
	memset(tmp.data, 0, s.st_size);
	tmp.size = s.st_size;
	if (claws_fread (tmp.data, 1, s.st_size, fp) < s.st_size) {
		log_error(LOG_PROTOCOL, _("Cannot read P12 certificate file (%s)\n"),
				  g_strerror(errno));
		free(tmp.data);
		return NULL;
	}

	gnutls_pkcs12_init(&p12);

	if ((r = gnutls_pkcs12_import(p12, &tmp, format, 0)) < 0) {
		log_error(LOG_PROTOCOL, _("Cannot import P12 certificate file (%s)\n"),
				  gnutls_strerror(r));
		gnutls_pkcs12_deinit(p12);
		p12 = NULL;
	}
	free(tmp.data);
	debug_print("got p12! %p\n", p12);
	return p12;
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
	fp = claws_fopen(file, "wb");
	if (fp == NULL) {
		g_free(file);
		debug_print("Can't save certificate !\n");
		return;
	}

	gnutls_export_X509_fp(fp, cert->x509_cert, GNUTLS_X509_FMT_DER);

	g_free(file);
	claws_safe_fclose(fp);

}

void ssl_certificate_destroy(SSLCertificate *cert) 
{
	if (cert == NULL)
		return;

	if (cert->x509_cert)
		gnutls_x509_crt_deinit(cert->x509_cert);
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
	claws_unlink (file);
	g_free(file);
	file = get_certificate_chain_path(cert->host, buf, cert->fingerprint);
	claws_unlink (file);
	g_free(file);
	g_free(buf);
}

SSLCertificate *ssl_certificate_find (const gchar *host, gushort port, const gchar *fingerprint)
{
	gchar *file = NULL;
	gchar *buf;
	SSLCertificate *cert = NULL;
	gnutls_x509_crt_t tmp_x509;
	FILE *fp = NULL;
	gboolean must_rename = FALSE;

	buf = g_strdup_printf("%d", port);
	
	if (fingerprint != NULL) {
		file = get_certificate_path(host, buf, fingerprint);
		fp = claws_fopen(file, "rb");
	}
	if (fp == NULL) {
		/* see if we have the old one */
		debug_print("didn't get %s\n", file);
		g_free(file);
		file = get_certificate_path(host, buf, NULL);
		fp = claws_fopen(file, "rb");

		if (fp) {
			debug_print("got %s\n", file);
			must_rename = (fingerprint != NULL);
		}
	} else {
		debug_print("got %s first try\n", file);
	}
	if (fp == NULL) {
		g_free(file);
		g_free(buf);
		return NULL;
	}
	
	if ((tmp_x509 = gnutls_import_X509_fp(fp, GNUTLS_X509_FMT_DER)) != NULL) {
		cert = ssl_certificate_new(tmp_x509, host, port);
		debug_print("got cert %p\n", cert);
		gnutls_x509_crt_deinit(tmp_x509);
	}

	claws_fclose(fp);
	g_free(file);
	
	if (must_rename) {
		gchar *old = get_certificate_path(host, buf, NULL);
		gchar *new = get_certificate_path(host, buf, fingerprint);
		if (strcmp(old, new))
			move_file(old, new, TRUE);
		g_free(old);
		g_free(new);
	}
	g_free(buf);

	return cert;
}

static gboolean ssl_certificate_compare (SSLCertificate *cert_a, SSLCertificate *cert_b)
{
	char *output_a;
	char *output_b;
	size_t cert_size_a = 0, cert_size_b = 0;
	int r;

	if (cert_a == NULL || cert_b == NULL)
		return FALSE;

	if ((r = gnutls_x509_crt_export(cert_a->x509_cert, GNUTLS_X509_FMT_DER, NULL, &cert_size_a)) 
	    != GNUTLS_E_SHORT_MEMORY_BUFFER) {
		g_warning("couldn't gnutls_x509_crt_export to get size a %s", gnutls_strerror(r));
		return FALSE;
	}

	if ((r = gnutls_x509_crt_export(cert_b->x509_cert, GNUTLS_X509_FMT_DER, NULL, &cert_size_b))
	    != GNUTLS_E_SHORT_MEMORY_BUFFER) {
		g_warning("couldn't gnutls_x509_crt_export to get size b %s", gnutls_strerror(r));
		return FALSE;
	}

	output_a = g_malloc(cert_size_a);
	output_b = g_malloc(cert_size_b);
	if ((r = gnutls_x509_crt_export(cert_a->x509_cert, GNUTLS_X509_FMT_DER, output_a, &cert_size_a)) < 0) {
		g_warning("couldn't gnutls_x509_crt_export a %s", gnutls_strerror(r));
		g_free(output_a);
		g_free(output_b);
		return FALSE;
	}
	if ((r = gnutls_x509_crt_export(cert_b->x509_cert, GNUTLS_X509_FMT_DER, output_b, &cert_size_b)) < 0) {
		g_warning("couldn't gnutls_x509_crt_export b %s", gnutls_strerror(r));
		g_free(output_a);
		g_free(output_b);
		return FALSE;
	}
	if (cert_size_a != cert_size_b) {
		g_warning("size differ %"G_GSIZE_FORMAT" %"G_GSIZE_FORMAT, cert_size_a, cert_size_b);
		g_free(output_a);
		g_free(output_b);
		return FALSE;
	}
	if (memcmp(output_a, output_b, cert_size_a)) {
		g_warning("contents differ");
		g_free(output_a);
		g_free(output_b);
		return FALSE;
	}
	g_free(output_a);
	g_free(output_b);
	
	return TRUE;
}

static guint check_cert(SSLCertificate *cert)
{
	gnutls_x509_crt_t *ca_list = NULL;
	gnutls_x509_crt_t *chain = NULL;
	unsigned int max_ca = 512, max_certs;
	unsigned int flags = 0;
	int r, i;
	unsigned int status;
	gchar *chain_file = NULL, *buf = NULL;
	FILE *fp;

	if (claws_ssl_get_cert_file())
		fp = claws_fopen(claws_ssl_get_cert_file(), "r");
	else
		return (guint)-1;

	if (fp == NULL)
		return (guint)-1;

	if ((r = gnutls_import_X509_list_fp(fp, GNUTLS_X509_FMT_PEM, &ca_list, &max_ca)) < 0) {
		debug_print("CA import failed: %s\n", gnutls_strerror(r));
		claws_fclose(fp);
		return (guint)-1;
	}
	claws_fclose(fp);
	fp = NULL;
	
	buf = g_strdup_printf("%d", cert->port);
	chain_file = get_certificate_chain_path(cert->host, buf, cert->fingerprint);
	g_free(buf);
	if (is_file_exist(chain_file)) {
		unsigned char md[128];
		size_t n = 128;
		char *fingerprint;

		fp = claws_fopen(chain_file, "r");
		if (fp == NULL) {
			debug_print("claws_fopen %s failed: %s\n", chain_file, g_strerror(errno));
			g_free(chain_file);
			return (guint)-1;
		}
		if ((r = gnutls_import_X509_list_fp(fp, GNUTLS_X509_FMT_PEM, &chain, &max_certs)) < 0) {
			debug_print("chain import failed: %s\n", gnutls_strerror(r));
			claws_fclose(fp);
			g_free(chain_file);
			return (guint)-1;
		}
		g_free(chain_file);
		claws_fclose(fp);
		fp = NULL;

		gnutls_x509_crt_get_fingerprint(chain[0], GNUTLS_DIG_MD5, md, &n);
		fingerprint = readable_fingerprint(md, n);
		if (!fingerprint || strcmp(fingerprint, cert->fingerprint)) {
			debug_print("saved chain fingerprint does not match current : %s / %s\n",
				cert->fingerprint, fingerprint);
				
			return (guint)-1;
		}
		g_free(fingerprint);

		r = gnutls_x509_crt_list_verify (chain,
				     max_certs,
				     ca_list, max_ca,
				     NULL, 0,
				     GNUTLS_VERIFY_ALLOW_X509_V1_CA_CRT,
				     &status);
		if (r < 0)
			debug_print("chain check failed: %s\n", gnutls_strerror(r));

		for (i = 0; i < max_certs; i++)
			gnutls_x509_crt_deinit(chain[i]);
		free(chain);

	} else {
		r = gnutls_x509_crt_verify(cert->x509_cert, ca_list, max_ca, flags, &status);
		if (r < 0)
			debug_print("cert check failed: %s\n", gnutls_strerror(r));
	}

	for (i = 0; i < max_ca; i++)
		gnutls_x509_crt_deinit(ca_list[i]);
	free(ca_list);

	if (r < 0)
		return (guint)-1;
	else
		return status;

}

static gboolean ssl_certificate_is_valid(SSLCertificate *cert, guint status)
{
	gchar *str_status = ssl_certificate_check_signer(cert, status);

	if (str_status != NULL) {
		g_free(str_status);
		return FALSE;
	}
	return ssl_certificate_check_subject_cn(cert);
}

char *ssl_certificate_check_signer (SSLCertificate *cert, guint status) 
{
	gnutls_x509_crt_t x509_cert = cert ? cert->x509_cert : NULL;

	if (!cert) 
		return g_strdup(_("Internal error"));

	if (status == (guint)-1) {
		status = check_cert(cert);
		if (status == -1)
			return g_strdup(_("Uncheckable"));
	}
	if (status & GNUTLS_CERT_INVALID) {
		if (gnutls_x509_crt_check_issuer(x509_cert, x509_cert))
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

static void ssl_certificate_save_chain(gnutls_x509_crt_t *certs, gint len, const gchar *host, gushort port)
{
	gint i;
	gchar *file = NULL;
	FILE *fp = NULL;
	
	for (i = 0; i < len; i++) {
		size_t n;
		unsigned char md[128];	
		gnutls_x509_crt_t cert = certs[i];

		if (i == 0) {
			n = sizeof(md);
			gnutls_x509_crt_get_fingerprint(cert, GNUTLS_DIG_MD5, md, &n);
			gchar *fingerprint = readable_fingerprint(md, n);
			gchar *buf = g_strdup_printf("%d", port);

			file = get_certificate_chain_path(host, buf, fingerprint);
			g_free(fingerprint);

			g_free(buf);

			fp = claws_fopen(file, "wb");
			if (fp == NULL) {
				g_free(file);
				debug_print("Can't save certificate !\n");
				return;
			}
			g_free(file);
		}

		gnutls_export_X509_fp(fp, cert, GNUTLS_X509_FMT_PEM);

	}
	if (fp)
		claws_safe_fclose(fp);
}

gboolean ssl_certificate_check (gnutls_x509_crt_t x509_cert, guint status, 
				const gchar *host, gushort port,
				gboolean accept_if_valid)
{
	SSLCertificate *current_cert = NULL;
	SSLCertificate *known_cert;
	SSLCertHookData cert_hook_data;
	gchar *fingerprint;
	size_t n;
	unsigned char md[128];
	gboolean valid = FALSE;

	current_cert = ssl_certificate_new(x509_cert, host, port);

	if (current_cert == NULL) {
		debug_print("Buggy certificate !\n");
		return FALSE;
	}

	current_cert->status = status;
	/* fingerprint */
	n = sizeof(md);
	gnutls_x509_crt_get_fingerprint(x509_cert, GNUTLS_DIG_MD5, md, &n);
	fingerprint = readable_fingerprint(md, n);

	known_cert = ssl_certificate_find(host, port, fingerprint);

	g_free(fingerprint);

	if (accept_if_valid)
		valid = ssl_certificate_is_valid(current_cert, status);
	else
		valid = FALSE; /* Force check */

	if (known_cert == NULL) {
		if (valid) {
			ssl_certificate_save(current_cert);
			ssl_certificate_destroy(current_cert);
			return TRUE;
		}

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
		if (valid) {
			ssl_certificate_save(current_cert);
			ssl_certificate_destroy(current_cert);
			ssl_certificate_destroy(known_cert);
			return TRUE;
		}

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
	} else if (gnutls_x509_crt_get_expiration_time(current_cert->x509_cert) < time(NULL)) {
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

gboolean ssl_certificate_check_chain(gnutls_x509_crt_t *certs, gint chain_len,
				     const gchar *host, gushort port,
				     gboolean accept_if_valid)
{
	int ncas = 0;
	gnutls_x509_crt_t *cas = NULL;
	gboolean result = FALSE;
	int i;
	gint status;

	if (claws_ssl_get_cert_file()) {
		FILE *fp = claws_fopen(claws_ssl_get_cert_file(), "rb");
		int r = -errno;

		if (fp) {
			r = gnutls_import_X509_list_fp(fp, GNUTLS_X509_FMT_PEM, &cas, &ncas);
			claws_fclose(fp);
		}

		if (r < 0)
			g_warning("can't read SSL_CERT_FILE '%s': %s",
				claws_ssl_get_cert_file(), 
				gnutls_strerror(r));
	} else {
		debug_print("Can't find SSL ca-certificates file\n");
	}


	gnutls_x509_crt_list_verify (certs,
                             chain_len,
                             cas, ncas,
                             NULL, 0,
                             GNUTLS_VERIFY_ALLOW_X509_V1_CA_CRT,
                             &status);

	result = ssl_certificate_check(certs[0], status, host, port,
					accept_if_valid);

	if (result == TRUE) {
		ssl_certificate_save_chain(certs, chain_len, host, port);
	}

	for (i = 0; i < ncas; i++)
		gnutls_x509_crt_deinit(cas[i]);
	free(cas);

	return result;
}

gnutls_x509_crt_t ssl_certificate_get_x509_from_pem_file(const gchar *file)
{
	gnutls_x509_crt_t x509 = NULL;
	if (!file)
		return NULL;
	
	if (is_file_exist(file)) {
		FILE *fp = claws_fopen(file, "r");
		if (fp) {
			x509 = gnutls_import_X509_fp(fp, GNUTLS_X509_FMT_PEM);
			claws_fclose(fp);
			return x509;
		} else {
			log_error(LOG_PROTOCOL, _("Cannot open certificate file %s: %s\n"),
				  file, g_strerror(errno));
		}
	} else {
		log_error(LOG_PROTOCOL, _("Certificate file %s missing (%s)\n"),
			  file, g_strerror(errno));
	}
	return NULL;
}

gnutls_x509_privkey_t ssl_certificate_get_pkey_from_pem_file(const gchar *file)
{
	gnutls_x509_privkey_t key = NULL;
	if (!file)
		return NULL;
	
	if (is_file_exist(file)) {
		FILE *fp = claws_fopen(file, "r");
		if (fp) {
			key = gnutls_import_key_fp(fp, GNUTLS_X509_FMT_PEM);
			claws_fclose(fp);
			return key;
		} else {
			log_error(LOG_PROTOCOL, _("Cannot open key file %s (%s)\n"),
			file, g_strerror(errno));
		}
	} else {
		log_error(LOG_PROTOCOL, _("Key file %s missing (%s)\n"), file,
			  g_strerror(errno));
	}
	return NULL;
}

/* From GnuTLS lib/gnutls_x509.c */
static int
parse_pkcs12 (gnutls_pkcs12_t p12,
	      const char *password,
	      gnutls_x509_privkey_t * key,
	      gnutls_x509_crt_t * cert)
{
  gnutls_pkcs12_bag_t bag = NULL;
  int index = 0;
  int ret;

  for (;;)
    {
      int elements_in_bag;
      int i;

      ret = gnutls_pkcs12_bag_init (&bag);
      if (ret < 0)
	{
	  bag = NULL;
	  goto done;
	}

      ret = gnutls_pkcs12_get_bag (p12, index, bag);
      if (ret == GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE)
	break;
      if (ret < 0)
	{
	  goto done;
	}

      ret = gnutls_pkcs12_bag_get_type (bag, 0);
      if (ret < 0)
	{
	  goto done;
	}

      if (ret == GNUTLS_BAG_ENCRYPTED)
	{
	  ret = gnutls_pkcs12_bag_decrypt (bag, password);
	  if (ret < 0)
	    {
	      goto done;
	    }
	}

      elements_in_bag = gnutls_pkcs12_bag_get_count (bag);
      if (elements_in_bag < 0)
	{
	  goto done;
	}

      for (i = 0; i < elements_in_bag; i++)
	{
	  int type;
	  gnutls_datum_t data;

	  type = gnutls_pkcs12_bag_get_type (bag, i);
	  if (type < 0)
	    {
	      goto done;
	    }

	  ret = gnutls_pkcs12_bag_get_data (bag, i, &data);
	  if (ret < 0)
	    {
	      goto done;
	    }

	  switch (type)
	    {
	    case GNUTLS_BAG_PKCS8_ENCRYPTED_KEY:
	    case GNUTLS_BAG_PKCS8_KEY:
	      ret = gnutls_x509_privkey_init (key);
	      if (ret < 0)
		{
		  goto done;
		}

	      ret = gnutls_x509_privkey_import_pkcs8
		(*key, &data, GNUTLS_X509_FMT_DER, password,
		 type == GNUTLS_BAG_PKCS8_KEY ? GNUTLS_PKCS_PLAIN : 0);
	      if (ret < 0)
		{
		  goto done;
		}
	      break;

	    case GNUTLS_BAG_CERTIFICATE:
	      ret = gnutls_x509_crt_init (cert);
	      if (ret < 0)
		{
		  goto done;
		}

	      ret =
		gnutls_x509_crt_import (*cert, &data, GNUTLS_X509_FMT_DER);
	      if (ret < 0)
		{
		  goto done;
		}
	      break;

	    case GNUTLS_BAG_ENCRYPTED:
	      /* XXX Bother to recurse one level down?  Unlikely to
	         use the same password anyway. */
	    case GNUTLS_BAG_EMPTY:
	    default:
	      break;
	    }
	}

      index++;
      gnutls_pkcs12_bag_deinit (bag);
    }

  ret = 0;

done:
  if (bag)
    gnutls_pkcs12_bag_deinit (bag);

  return ret;
}
void ssl_certificate_get_x509_and_pkey_from_p12_file(const gchar *file, const gchar *password,
			gnutls_x509_crt_t *x509, gnutls_x509_privkey_t *pkey)
{
	gnutls_pkcs12_t p12 = NULL;

	int r;

	*x509 = NULL;
	*pkey = NULL;
	if (!file)
		return;

	if (is_file_exist(file)) {
		FILE *fp = claws_fopen(file, "r");
		if (fp) {
			p12 = gnutls_import_PKCS12_fp(fp, GNUTLS_X509_FMT_DER);
			claws_fclose(fp);
			if (!p12) {
				log_error(LOG_PROTOCOL, _("Failed to read P12 certificate file %s\n"), file);
			}
		} else {
			log_error(LOG_PROTOCOL, _("Cannot open P12 certificate file %s (%s)\n"),
				  file, g_strerror(errno));
		}
	} else {
		log_error(LOG_PROTOCOL, _("P12 Certificate file %s missing (%s)\n"), file,
			  g_strerror(errno));
	}
	if (p12 != NULL) {
		if ((r = parse_pkcs12(p12, password, pkey, x509)) == 0) {
			debug_print("got p12\n");
		} else {
			log_error(LOG_PROTOCOL, "%s\n", gnutls_strerror(r));
		}
		gnutls_pkcs12_deinit(p12);
	}
}

gboolean ssl_certificate_check_subject_cn(SSLCertificate *cert)
{
	return gnutls_x509_crt_check_hostname(cert->x509_cert, cert->host) != 0;
}

gchar *ssl_certificate_get_subject_cn(SSLCertificate *cert)
{
	gchar subject_cn[BUFFSIZE];
	size_t n = BUFFSIZE;

	if(gnutls_x509_crt_get_dn_by_oid(cert->x509_cert, 
		GNUTLS_OID_X520_COMMON_NAME, 0, 0, subject_cn, &n))
		return g_strdup(_("<not in certificate>"));

	return g_strdup(subject_cn);
}

#endif /* USE_GNUTLS */
