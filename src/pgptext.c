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
#include <fcntl.h>	/* FIXME */

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


struct passphrase_cb_info_s {
    GpgmeCtx c;
    int did_it;
};

static const gchar *
sig_status_to_string (GpgmeSigStat status)
{
    const gchar *result;

    switch (status) {
      case GPGME_SIG_STAT_NONE:
        result = _("Oops: Signature not verified");
        break;
      case GPGME_SIG_STAT_NOSIG:
        result = _("No signature found");
        break;
      case GPGME_SIG_STAT_GOOD:
        result = _("Good signature");
        break;
      case GPGME_SIG_STAT_BAD:
        result = _("BAD signature");
        break;
      case GPGME_SIG_STAT_NOKEY:
        result = _("No public key to verify the signature");
        break;
      case GPGME_SIG_STAT_ERROR:
        result = _("Error verifying the signature");
        break;
      case GPGME_SIG_STAT_DIFF:
        result = _("Different results for signatures");
        break;
      default:
	result = _("Error: Unknown status");
	break;
    }

    return result;
}

static const gchar *
sig_status_with_name (GpgmeSigStat status)
{
    const gchar *result;

    switch (status) {
      case GPGME_SIG_STAT_NONE:
        result = _("Oops: Signature not verified");
        break;
      case GPGME_SIG_STAT_NOSIG:
        result = _("No signature found");
        break;
      case GPGME_SIG_STAT_GOOD:
        result = _("Good signature from \"%s\"");
        break;
      case GPGME_SIG_STAT_BAD:
        result = _("BAD signature  from \"%s\"");
        break;
      case GPGME_SIG_STAT_NOKEY:
        result = _("No public key to verify the signature");
        break;
      case GPGME_SIG_STAT_ERROR:
        result = _("Error verifying the signature");
        break;
      case GPGME_SIG_STAT_DIFF:
        result = _("Different results for signatures");
        break;
      default:
	result = _("Error: Unknown status");
	break;
    }

    return result;
}

static void
sig_status_for_key(GString *str, GpgmeCtx ctx, GpgmeSigStat status, 
		   GpgmeKey key, const gchar *fpr)
{
	gint idx = 0;
	const char *uid;

	uid = gpgme_key_get_string_attr (key, GPGME_ATTR_USERID, NULL, idx);
	if (uid == NULL) {
		g_string_sprintfa (str, "%s\n",
				   sig_status_to_string (status));
		if ((fpr != NULL) && (*fpr != '\0'))
			g_string_sprintfa (str, "Key fingerprint: %s\n", fpr);
		g_string_append (str, _("Cannot find user ID for this key."));
		return;
	}
	g_string_sprintfa (str, sig_status_with_name (status), uid);
	g_string_append (str, "\n");

	while (1) {
		uid = gpgme_key_get_string_attr (key, GPGME_ATTR_USERID,
						 NULL, ++idx);
		if (uid == NULL)
			break;
		g_string_sprintfa (str, _("                aka \"%s\"\n"),
				   uid);
	}
}

static gchar *
sig_status_full (GpgmeCtx ctx)
{
	GString *str;
	gint sig_idx = 0;
	GpgmeError err;
	GpgmeSigStat status;
	GpgmeKey key;
	const char *fpr;
	time_t created;
	struct tm *ctime_val;
	char ctime_str[80];
	gchar *retval;

	str = g_string_new ("");

	fpr = gpgme_get_sig_status (ctx, sig_idx, &status, &created);
	while (fpr != NULL) {
		if (created != 0) {
			ctime_val = localtime (&created);
			strftime (ctime_str, sizeof (ctime_str), "%c", 
				  ctime_val);
			g_string_sprintfa (str,
					   _("Signature made %s\n"),
					   ctime_str);
		}
		err = gpgme_get_sig_key (ctx, sig_idx, &key);
		if (err != 0) {
			g_string_sprintfa (str, "%s\n",
					   sig_status_to_string (status));
			if ((fpr != NULL) && (*fpr != '\0'))
				g_string_sprintfa (str,
						   _("Key fingerprint: %s\n"),
						   fpr);
		} else {
			sig_status_for_key (str, ctx, status, key, fpr);
			gpgme_key_unref (key);
		}
		g_string_append (str, "\n\n");

		fpr = gpgme_get_sig_status (ctx, ++sig_idx, &status, &created);
	}

	retval = str->str;
	g_string_free (str, FALSE);
	return retval;
}

static void dump_part ( MimeInfo *mimeinfo, FILE *fp )
{
    unsigned int size = mimeinfo->size;
    int c;

    if (fseek (fp, mimeinfo->fpos, SEEK_SET)) {
        g_warning ("dump_part: fseek error");
        return;
    }

    debug_print ("** --- begin dump_part ----");
    while (size-- && (c = getc (fp)) != EOF)
        putc (c, stderr);
    if (ferror (fp))
        g_warning ("dump_part: read error");
    debug_print ("** --- end dump_part ----");
}

static void pgptext_fine_check_signature (MimeInfo *mimeinfo, FILE *fp)
{
    GpgmeCtx ctx = NULL;
    GpgmeError err;
    GpgmeData text = NULL;
    GpgmeSigStat status = GPGME_SIG_STAT_NONE;
    GpgmegtkSigStatus statuswindow = NULL;
    const char *result = NULL;

	/* As this is the most simple solution, I prefer it. :-) */
	statuswindow = gpgmegtk_sig_status_create ();

    err = gpgme_new (&ctx);
    if (err) {
	g_warning ("gpgme_new failed: %s", gpgme_strerror (err));
	goto leave;
    }

    err = gpgme_data_new_from_filepart (&text, NULL, fp,
					mimeinfo->fpos,	mimeinfo->size);

    if (err) {
        debug_print ("gpgme_data_new_from_filepart failed: %s",
		   gpgme_strerror (err));
        goto leave;
    }

	/* Just pass the text to gpgme_op_verify to enable plain text stuff. */
    err = gpgme_op_verify (ctx, text, NULL, &status);
    if (err)
        debug_print ("gpgme_op_verify failed: %s", gpgme_strerror (err));

    /* FIXME: check what the heck this sig_status_full stuff is.
     * it should better go into sigstatus.c */
    g_free (mimeinfo->sigstatus_full);
    mimeinfo->sigstatus_full = sig_status_full (ctx);

leave:
    result = gpgmegtk_sig_status_to_string(status);
    debug_print("verification status: %s\n", result);
    gpgmegtk_sig_status_update (statuswindow,ctx);

    g_assert (!err); /* FIXME: Hey: this may indeed happen */
    g_free (mimeinfo->sigstatus);
    mimeinfo->sigstatus = g_strdup (result);

    gpgme_data_release (text);
    gpgme_release (ctx);
    gpgmegtk_sig_status_destroy (statuswindow);
}

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

static const char *
passphrase_cb (void *opaque, const char *desc, void *r_hd)
{
    struct passphrase_cb_info_s *info = opaque;
    GpgmeCtx ctx = info ? info->c : NULL;
    const char *pass;

    if (!desc) {
        /* FIXME: cleanup by looking at *r_hd */
        return NULL;
    }

    gpgmegtk_set_passphrase_grab (prefs_common.passphrase_grab);
    debug_print ("requesting passphrase for `%s': ", desc);
    pass = gpgmegtk_passphrase_mbox (desc);
    if (!pass) {
        debug_print ("cancel passphrase entry\n");
        gpgme_cancel (ctx);
    }
    else
        debug_print ("sending passphrase\n");

    return pass;
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
        gpgme_set_passphrase_cb (ctx, passphrase_cb, &info);
    } 

    err = gpgme_op_decrypt (ctx, cipher, plain);

leave:
    gpgme_data_release (cipher);
    if (err) {
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

gboolean pgptext_has_signature (MsgInfo *msginfo, MimeInfo *mimeinfo)
{
	FILE *fp;
	gchar *file = NULL, *tmpchk, *tmpln, *tmpln_1;
	gchar buf[BUFFSIZE];
	gboolean has_begin_pgp_signed_msg = FALSE;
	gboolean has_begin_pgp_sig = FALSE;
	gboolean has_end_pgp_sig = FALSE;
	gchar *check_begin_pgp_signed_msg = "-----BEGIN PGP SIGNED MESSAGE-----\n";
	gchar *check_begin_pgp_signed_msg_1 = "-----BEGIN PGP SIGNED MESSAGE-----\r\n";
	gchar *check_begin_pgp_sig = "-----BEGIN PGP SIGNATURE-----\n";
	gchar *check_begin_pgp_sig_1 = "-----BEGIN PGP SIGNATURE-----\r\n";
	gchar *check_end_pgp_sig = "-----END PGP SIGNATURE-----\n";
	gchar *check_end_pgp_sig_1 = "-----END PGP SIGNATURE-----\r\n";

	if ((!mimeinfo) || (!msginfo))
		return FALSE;
	
	file = g_strdup_printf("%s", procmsg_get_message_file_path(msginfo));
	
	if (mimeinfo->mime_type != MIME_TEXT) {
		if ((fp = fopen(file, "r")) == NULL) {
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
		/* now check for a pgptext signed message
		 * the strlen check catches quoted signatures */
		while (fgets(buf, sizeof(buf), fp) != NULL) {
			tmpchk = g_strnfill(sizeof(buf), '\n');
			memmove(tmpchk, &buf, sizeof(buf));
			
			tmpln = strstr(tmpchk, check_begin_pgp_signed_msg);
			tmpln_1 = strstr(tmpchk, check_begin_pgp_signed_msg_1);
			if (((tmpln != NULL) || (tmpln_1 != NULL)) && ((strlen(tmpchk) ==
			      strlen(tmpln)) || (strlen(tmpchk) == strlen(tmpln_1))) )
				has_begin_pgp_signed_msg = TRUE;
			
			tmpln = strstr(tmpchk, check_begin_pgp_sig);
			tmpln_1 = strstr(tmpchk, check_begin_pgp_sig_1);
			if (((tmpln != NULL) || (tmpln_1 != NULL)) && ((strlen(tmpchk) ==
			      strlen(tmpln)) || (strlen(tmpchk) == strlen(tmpln_1))) )
				has_begin_pgp_sig = TRUE;
			
			tmpln = strstr(tmpchk, check_end_pgp_sig);
			tmpln_1 = strstr(tmpchk, check_end_pgp_sig_1);
			if (((tmpln != NULL) || (tmpln_1 != NULL)) && ((strlen(tmpchk) ==
			      strlen(tmpln)) || (strlen(tmpchk) == strlen(tmpln_1))) )
				has_end_pgp_sig = TRUE;
			
			g_free(tmpchk);
		}
		fclose(fp);
	} else {
		if ((fp = fopen(file, "r")) == NULL) {
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

			if (strstr(tmpchk, check_begin_pgp_signed_msg) != NULL ||
					strstr(tmpchk, check_begin_pgp_signed_msg_1) != NULL)
				has_begin_pgp_signed_msg = TRUE;
			if (strstr(tmpchk, check_begin_pgp_sig) != NULL ||
					strstr(tmpchk, check_begin_pgp_sig_1) != NULL)
				has_begin_pgp_sig = TRUE;
			if (strstr(tmpchk, check_end_pgp_sig) != NULL ||
					strstr(tmpchk, check_end_pgp_sig_1) != NULL)
				has_end_pgp_sig = TRUE;

			g_free(tmpchk);
		}
		fclose(fp);
	}
	
	g_free(file);
	
	/* do we have a proper message? */
	if (has_begin_pgp_signed_msg && has_begin_pgp_sig && has_end_pgp_sig) {
		debug_print ("** pgptext signed message encountered\n");
		return TRUE;
	} else
		return FALSE;
}

void pgptext_check_signature (MimeInfo *mimeinfo, FILE *fp)
{
	gchar *file, *tmpchk;
	gchar buf[BUFFSIZE];
	gboolean has_begin_pgp_signed_msg = FALSE;
	gboolean has_begin_pgp_sig = FALSE;
	gboolean has_end_pgp_sig = FALSE;
	gchar *check_begin_pgp_signed_msg = "-----BEGIN PGP SIGNED MESSAGE-----\n";
	gchar *check_begin_pgp_signed_msg_1 = "-----BEGIN PGP SIGNED
MESSAGE-----\r\n";
	gchar *check_begin_pgp_sig = "-----BEGIN PGP SIGNATURE-----\n";
	gchar *check_begin_pgp_sig_1 = "-----BEGIN PGP SIGNATURE-----\r\n";
	gchar *check_end_pgp_sig = "-----END PGP SIGNATURE-----\n";
	gchar *check_end_pgp_sig_1 = "-----END PGP SIGNATURE-----\r\n";

    if (!mimeinfo)
        return;
	
	/* now we have to set fpos and size correctly */
	/* skip headers */
	/* FIXME: we should check for the correct mime type
	 * f.e. mime/text, application/pgp and so on...*/
/*	if (mimeinfo->mime_type == MIME_TEXT) {*/
		if (fseek(fp, mimeinfo->fpos, SEEK_SET) < 0)
		perror("fseek");
		while (fgets(buf, sizeof(buf), fp) != NULL) {
			mimeinfo->fpos = mimeinfo->fpos + strlen(buf);
			if (buf[0] == '\r' || buf[0] == '\n') break;
		}
/*	}*/

	/* now check for fpos and size of the pgptext signed message */
	mimeinfo->size = 0;	/* init */
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		tmpchk = g_strnfill(sizeof(buf), '\n');
		memmove(tmpchk, &buf, sizeof(buf));

		if (has_begin_pgp_signed_msg)
			/* get the size */
			mimeinfo->size = mimeinfo->size + strlen(tmpchk);

		if (strstr(tmpchk, check_begin_pgp_signed_msg) != NULL ||
				strstr(tmpchk, check_begin_pgp_signed_msg_1) != NULL)
			has_begin_pgp_signed_msg = TRUE;
		else if (!has_begin_pgp_signed_msg)
			/* set the beginning of the pgptext signed message */
			mimeinfo->fpos = mimeinfo->fpos + strlen(tmpchk);

		if (strstr(tmpchk, check_begin_pgp_sig) != NULL ||
				strstr(tmpchk, check_begin_pgp_sig_1) != NULL)
			has_begin_pgp_sig = TRUE;

		if (strstr(tmpchk, check_end_pgp_sig) != NULL ||
				strstr(tmpchk, check_end_pgp_sig_1) != NULL) {
			has_end_pgp_sig = TRUE;
			/* FIXME: Find out why the hell there are always 6[+1] 
			 * chars less in our counter!*/
			mimeinfo->size = mimeinfo->size + strlen(tmpchk) + 7;
			break;
		}

		g_free(tmpchk);
	}
	
#if 0
	debug_print ("** pgptext sig check...");
	debug_print ("\tmimeinfo->fpos: %lu\tmimeinfo->size: %lu\n",
			mimeinfo->fpos, mimeinfo->size);
	dump_part (mimeinfo, fp);
#endif

	pgptext_fine_check_signature (mimeinfo, fp);
}

int pgptext_is_encrypted (MimeInfo *mimeinfo, MsgInfo *msginfo)
{
	FILE *fp;
	gchar *file, *tmpchk;
	gchar buf[BUFFSIZE];
	gboolean has_begin_pgp_msg = FALSE;
	gboolean has_end_pgp_msg = FALSE;
	gchar *check_begin_pgp_msg = "-----BEGIN PGP MESSAGE-----\n";
	gchar *check_begin_pgp_msg_1 = "-----BEGIN PGP MESSAGE-----\r\n";
	gchar *check_end_pgp_msg = "-----END PGP MESSAGE-----\n";
	gchar *check_end_pgp_msg_1 = "-----END PGP MESSAGE-----\r\n";
	
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
		if ((fp = fopen(file, "r")) == NULL) {
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
			
			if (strstr(tmpchk, check_begin_pgp_msg) != NULL || 
			    strstr(tmpchk, check_begin_pgp_msg_1) != NULL)
				has_begin_pgp_msg = TRUE;
			if (strstr(tmpchk, check_end_pgp_msg) != NULL || 
			    strstr(tmpchk, check_end_pgp_msg_1) != NULL)
				has_end_pgp_msg = TRUE;

			g_free(tmpchk);
		}
		fclose(fp);
	} else {
		if ((fp = fopen(file, "r")) == NULL) {
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
			
			if (strstr(tmpchk, check_begin_pgp_msg) != NULL || 
			    strstr(tmpchk, check_begin_pgp_msg_1) != NULL)
				has_begin_pgp_msg = TRUE;
			if (strstr(tmpchk, check_end_pgp_msg) != NULL || 
			    strstr(tmpchk, check_end_pgp_msg_1) != NULL)
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

    if ((dstfp = fopen(fname, "w")) == NULL) {
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

/* int pgptext_encrypt (const char *file, GSList *recp_list)
{
}

int pgptext_sign (const char *file, PrefsAccount *ac)
{
} */

#endif	/* USE_GPGME */

