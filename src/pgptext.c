/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2001 Jens Jahnke <jan0sch@gmx.net>
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

#if USE_GPGME

#include "defs.h"

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <ctype.h>

#include <gpgme.h>

#include "intl.h"
#include "procmime.h"
#include "procheader.h"
#include "base64.h"
#include "uuencode.h"
#include "unmime.h"
#include "codeconv.h"
#include "utils.h"
#include "prefs_common.h"
#include "passphrase.h"
#include "select-keys.h"
#include "sigstatus.h"
#include "rfc2015.h"

#include "pgptext.h"

#define DIM(v)     (sizeof(v)/sizeof((v)[0]))

static char *content_names[] = {
    "Content-Type",
    "Content-Disposition",
    "Content-Transfer-Encoding",
    NULL
};

static char *mime_version_name[] = {
    "Mime-Version",
    NULL
};

/* stolen from rfc2015.c */
static int
gpg_name_cmp(const char *a, const char *b)
{
    for( ; *a && *b; a++, b++) {
        if(*a != *b
           && toupper(*(unsigned char *)a) != toupper(*(unsigned char *)b))
            return 1;
    }

    return *a != *b;
}

static GpgmeData
pgptext_decrypt (MimeInfo *partinfo, FILE *fp)
{
    GpgmeCtx ctx = NULL;
    GpgmeError err;
    GpgmeData cipher = NULL, plain = NULL;
    struct passphrase_cb_info_s info;

    memset (&info, 0, sizeof info);

    err = gpgme_new (&ctx);
    if (err) {
        debug_print ("gpgme_new failed: %s\n", gpgme_strerror (err));
        goto leave;
    }

    err = gpgme_data_new_from_filepart (&cipher, NULL, fp,
					partinfo->fpos, partinfo->size);
    if (err) {
        debug_print ("gpgme_data_new_from_filepart failed: %s\n",
                     gpgme_strerror (err));
        goto leave;
    }

    err = gpgme_data_new (&plain);
    if (err) {
        debug_print ("gpgme_new failed: %s\n", gpgme_strerror (err));
        goto leave;
    }

    if (!getenv("GPG_AGENT_INFO")) {
        info.c = ctx;
        gpgme_set_passphrase_cb (ctx, gpgmegtk_passphrase_cb, &info);
    } 

    err = gpgme_op_decrypt (ctx, cipher, plain);

leave:
    gpgme_data_release (cipher);
    if (err) {
        gpgmegtk_free_passphrase();
        debug_print ("decryption failed: %s\n", gpgme_strerror (err));
        gpgme_data_release (plain);
        plain = NULL;
    }
    else
        debug_print ("decryption succeeded\n");

    gpgme_release (ctx);
    return plain;
}

static int
headerp(char *p, char **names)
{
    int i, c;
    char *p2;

    p2 = strchr(p, ':');
    if(!p2 || p == p2) {
        return 0;
    }
    if(p2[-1] == ' ' || p2[-1] == '\t') {
        return 0;
    }

    if(!names[0])
        return 1;  

    c = *p2;
    *p2 = 0;
    for(i = 0 ; names[i] != NULL; i++) {
        if(!gpg_name_cmp (names[i], p))
            break;
    }
    *p2 = c;

    return names[i] != NULL;
}

MimeInfo * pgptext_find_signature (MimeInfo *mimeinfo)
{
}

gboolean pgptext_has_signature (MimeInfo *mimeinfo)
{
	/*
	 * check for the following strings:
	 *   -----BEGIN PGP SIGNED MESSAGE-----
	 *   ----- ????
	 *   -----BEGIN PGP SIGNATURE-----
	 *   -----END PGP SIGNATURE-----
	 */
	
	return 0;
}

void pgptext_check_signature (MimeInfo *mimeinfo, FILE *fp)
{
}

int pgptext_is_encrypted (MimeInfo *mimeinfo, MsgInfo *msginfo)
{
	FILE *fp;
	gchar *file, *tmpchk;
	gchar buf[BUFFSIZE];
	gboolean has_begin_pgp_msg = FALSE;
	gboolean has_end_pgp_msg = FALSE;
	gchar *check_begin_pgp_msg = "-----BEGIN PGP MESSAGE-----\n";
	gchar *check_end_pgp_msg = "-----END PGP MESSAGE-----\n";
	
	g_return_if_fail(msginfo != NULL);
	
	if (!mimeinfo)
		return 0;
	
	if ((fp = procmsg_open_message(msginfo)) == NULL) return;
	mimeinfo = procmime_scan_mime_header(fp);
	fclose(fp);
	if (!mimeinfo) return;

	file = procmsg_get_message_file_path(msginfo);
	g_return_if_fail(file != NULL);

	if (mimeinfo->mime_type != MIME_TEXT) {
		if ((fp = fopen(file, "rb")) == NULL) {
			FILE_OP_ERROR(file, "fopen");
			return;
		}
		/* skip headers */
		if (mimeinfo->mime_type == MIME_MULTIPART) {
			if (fseek(fp, mimeinfo->fpos, SEEK_SET) < 0)
			perror("fseek");
			while (fgets(buf, sizeof(buf), fp) != NULL)
				if (buf[0] == '\r' || buf[0] == '\n') break;
		}
		/* now check for a pgptext encrypted message */
		while (fgets(buf, sizeof(buf), fp) != NULL) {
			tmpchk = g_strnfill(sizeof(buf), '\n');
			memmove(tmpchk, &buf, sizeof(buf));
			
			if (strstr(tmpchk, check_begin_pgp_msg) != NULL)
				has_begin_pgp_msg = TRUE;
			if (strstr(tmpchk, check_end_pgp_msg) != NULL)
				has_end_pgp_msg = TRUE;
			
			g_free(tmpchk);
		}
		fclose(fp);
	} else {
		if ((fp = fopen(file, "rb")) == NULL) {
			FILE_OP_ERROR(file, "fopen");
			return;
		}
		/* skip headers */
		if (fseek(fp, mimeinfo->fpos, SEEK_SET) < 0)
		perror("fseek");
		while (fgets(buf, sizeof(buf), fp) != NULL)
			if (buf[0] == '\r' || buf[0] == '\n') break;
		
		/* now check for a pgptext encrypted message */
		while (fgets(buf, sizeof(buf), fp) != NULL) {
			tmpchk = g_strnfill(sizeof(buf), '\n');
			memmove(tmpchk, &buf, sizeof(buf));
			
			if (strstr(tmpchk, check_begin_pgp_msg) != NULL)
				has_begin_pgp_msg = TRUE;
			if (strstr(tmpchk, check_end_pgp_msg) != NULL)
				has_end_pgp_msg = TRUE;
			
			g_free(tmpchk);
		}
		fclose(fp);
	}
	
	g_free(file);	
	
	/* do we have a proper message? */
	if (has_begin_pgp_msg && has_end_pgp_msg)
		return 1;
	else
		return 0;
}

void pgptext_decrypt_message (MsgInfo *msginfo, MimeInfo *mimeinfo, FILE *fp)
{
    static int id;
    MimeInfo *partinfo;
    int n, found;
    int ver_okay=0;
    char *fname;
    GpgmeData plain;
    FILE *dstfp;
    size_t nread;
    char buf[BUFFSIZE];
    GpgmeError err;

    g_return_if_fail (mimeinfo->mime_type == MIME_TEXT);

    debug_print ("text/plain with pgptext encountered\n");

    partinfo = procmime_scan_message(msginfo);
		
    /* skip headers */
    if (fseek(fp, partinfo->fpos, SEEK_SET) < 0)
        perror("fseek");
    while (fgets(buf, sizeof(buf), fp) != NULL) {
				partinfo->fpos = partinfo->fpos + strlen(buf);
        if (buf[0] == '\r' || buf[0] == '\n') break;
		}
    /* get size */
    while (fgets(buf, sizeof(buf), fp) != NULL)
				partinfo->size = partinfo->size + strlen(buf);
		
    plain = pgptext_decrypt (partinfo, fp);
    if (!plain) {
        msginfo->decryption_failed = 1;
        return;
    }
    
    fname = g_strdup_printf("%s%cplaintext.%08x",
			    get_mime_tmp_dir(), G_DIR_SEPARATOR, ++id);

    if ((dstfp = fopen(fname, "wb")) == NULL) {
        FILE_OP_ERROR(fname, "fopen");
        g_free(fname);
        msginfo->decryption_failed = 1;
        return;
    }

    /* write the orginal header to the new file */
    if (fseek(fp, mimeinfo->fpos, SEEK_SET) < 0)
        perror("fseek");

    while (fgets(buf, sizeof(buf), fp)) {
        if (headerp (buf, content_names))
            continue;
        if (buf[0] == '\r' || buf[0] == '\n')
            break;
        fputs (buf, dstfp);
    }
		
    err = gpgme_data_rewind (plain);
    if (err)
        debug_print ("gpgme_data_rewind failed: %s\n", gpgme_strerror (err));

		/* insert blank line to avoid some trouble... */
		fputs ("\n", dstfp);
		
    while (!(err = gpgme_data_read (plain, buf, sizeof(buf), &nread))) {
        fwrite (buf, nread, 1, dstfp);
    }

    if (err != GPGME_EOF) {
        debug_print ("gpgme_data_read failed: %s\n", gpgme_strerror (err));
    }

    fclose (dstfp);

    msginfo->plaintext_file = fname;
    msginfo->decryption_failed = 0;

}

int pgptext_encrypt (const char *file, GSList *recp_list)
{
}

int pgptext_sign (const char *file, PrefsAccount *ac)
{
}

#endif	/* USE_GPGME */

