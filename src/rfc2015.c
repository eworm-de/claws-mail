/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2001 Werner Koch (dd9jn)
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
#ifdef USE_GPGME

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

static char *create_boundary (void);

static void dump_mimeinfo (const char *text, MimeInfo *x)
{
    g_message ("** MimeInfo[%s] %p  level=%d",
               text, x, x? x->level:0 );
    if (!x)
        return;

    g_message ("**      enc=`%s' enc_type=%d mime_type=%d",
               x->encoding, x->encoding_type, x->mime_type );
    g_message ("**      cont_type=`%s' cs=`%s' name=`%s' bnd=`%s'",
               x->content_type, x->charset, x->name, x->boundary );
    g_message ("**      cont_disp=`%s' fname=`%s' fpos=%ld size=%u, lvl=%d",
               x->content_disposition, x->filename, x->fpos, x->size,
               x->level );
    dump_mimeinfo (".main", x->main );
    dump_mimeinfo (".sub", x->sub );
    dump_mimeinfo (".next", x->next );
    g_message ("** MimeInfo[.parent] %p", x ); 
    dump_mimeinfo (".children", x->children );
    dump_mimeinfo (".plaintext", x->plaintext );
}

static void dump_part ( MimeInfo *mimeinfo, FILE *fp )
{
    unsigned int size = mimeinfo->size;
    int c;

    if (fseek (fp, mimeinfo->fpos, SEEK_SET)) {
        g_warning ("dump_part: fseek error");
        return;
    }

    g_message ("** --- begin dump_part ----");
    while (size-- && (c = getc (fp)) != EOF) 
        putc (c, stderr);
    if (ferror (fp))
        g_warning ("dump_part: read error");
    g_message ("** --- end dump_part ----");
}

void
rfc2015_disable_all (void)
{
    /* FIXME: set a flag, so that we don't bother the user with failed
     * gpgme messages */
}


void
rfc2015_secure_remove (const char *fname)
{
    if (!fname)
        return;
    /* fixme: overwrite the file first */
    remove (fname);
}


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

static void check_signature (MimeInfo *mimeinfo, MimeInfo *partinfo, FILE *fp)
{
    GpgmeCtx ctx = NULL;
    GpgmeError err;
    GpgmeData sig = NULL, text = NULL;
    GpgmeSigStat status = GPGME_SIG_STAT_NONE;
    GpgmegtkSigStatus statuswindow;
    const char *result = NULL;

    statuswindow = gpgmegtk_sig_status_create ();

    err = gpgme_new (&ctx);
    if (err) {
	g_warning ("gpgme_new failed: %s", gpgme_strerror (err));
	goto leave;
    }

    /* don't include the last character (LF). It does not belong to the
     * signed text */
    err = gpgme_data_new_from_filepart (&text, NULL, fp,
					mimeinfo->children->fpos,
					mimeinfo->children->size ?
					(mimeinfo->children->size - 1) : 0 );
    if (!err)
	err = gpgme_data_new_from_filepart (&sig, NULL, fp,
					    partinfo->fpos, partinfo->size);
    if (err) {
        g_message ("gpgme_data_new_from_filepart failed: %s",
		   gpgme_strerror (err));
        goto leave;
    }

    err = gpgme_op_verify (ctx, sig, text, &status);
    if (err) 
        g_message ("gpgme_op_verify failed: %s", gpgme_strerror (err));

    /* FIXME: check what the heck this sig_status_full stuff is.
     * it should better go into sigstatus.c */
    g_free (partinfo->sigstatus_full);
    partinfo->sigstatus_full = sig_status_full (ctx);

leave:
    result = gpgmegtk_sig_status_to_string(status);
    debug_print("verification status: %s\n", result);
    gpgmegtk_sig_status_update(statuswindow,ctx);

    g_assert (!err); /* FIXME: Hey: this may indeed happen */
    g_free (partinfo->sigstatus);
    partinfo->sigstatus = g_strdup (result);

    gpgme_data_release (sig);
    gpgme_data_release (text);
    gpgme_release (ctx);
    gpgmegtk_sig_status_destroy(statuswindow);
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
    g_message ("%% requesting passphrase for `%s': ", desc );
    pass = gpgmegtk_passphrase_mbox (desc);
    if (!pass) {
        g_message ("%% cancel passphrase entry");
        gpgme_cancel (ctx);
    }
    else
        g_message ("%% sending passphrase");

    return pass;
}

/*
 * Copy a gpgme data object to a temporary file and
 * return this filename 
 */
static char *
copy_gpgmedata_to_temp (GpgmeData data, guint *length)
{
    static int id;
    char *tmp;
    FILE *fp;
    char buf[100];
    size_t nread;
    GpgmeError err;
    
    tmp = g_strdup_printf("%s%cgpgtmp.%08x",
                          get_mime_tmp_dir(), G_DIR_SEPARATOR, ++id );

    if ((fp = fopen(tmp, "w")) == NULL) {
        FILE_OP_ERROR(tmp, "fopen");
        g_free(tmp);
        return NULL;
    }

    err = gpgme_data_rewind ( data );
    if (err)
        g_message ("** gpgme_data_rewind failed: %s", gpgme_strerror (err));

    while (!(err = gpgme_data_read (data, buf, 100, &nread))) {
        fwrite ( buf, nread, 1, fp );
    }

    if (err != GPGME_EOF)
        g_warning ("** gpgme_data_read failed: %s", gpgme_strerror (err));

    fclose (fp);
    *length = nread;

    return tmp;
}

static GpgmeData
pgp_decrypt (MimeInfo *partinfo, FILE *fp)
{
    GpgmeCtx ctx = NULL;
    GpgmeError err;
    GpgmeData cipher = NULL, plain = NULL;
    struct passphrase_cb_info_s info;

    memset (&info, 0, sizeof info);

    err = gpgme_new (&ctx);
    if (err) {
        g_message ("gpgme_new failed: %s", gpgme_strerror (err));
        goto leave;
    }

    err = gpgme_data_new_from_filepart (&cipher, NULL, fp,
					partinfo->fpos, partinfo->size);
    if (err) {
        g_message ("gpgme_data_new_from_filepart failed: %s",
                   gpgme_strerror (err));
        goto leave;
    }

    err = gpgme_data_new (&plain);
    if (err) {
        g_message ("gpgme_new failed: %s", gpgme_strerror (err));
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
        g_warning ("** decryption failed: %s", gpgme_strerror (err));
        gpgme_data_release (plain);
        plain = NULL;
    }
    else
        g_message ("** decryption succeeded");

    gpgme_release (ctx);
    return plain;
}

MimeInfo * rfc2015_find_signature (MimeInfo *mimeinfo)
{
    MimeInfo *partinfo;
    int n = 0;

    if (!mimeinfo)
        return NULL;
    if (g_strcasecmp (mimeinfo->content_type, "multipart/signed"))
        return NULL;

    g_message ("** multipart/signed encountered");

    /* check that we have at least 2 parts of the correct type */
    for (partinfo = mimeinfo->children;
         partinfo != NULL; partinfo = partinfo->next) {
        if (++n > 1  && !g_strcasecmp (partinfo->content_type,
				       "application/pgp-signature"))
            break;
    }

    return partinfo;
}

gboolean rfc2015_has_signature (MimeInfo *mimeinfo)
{
    return rfc2015_find_signature (mimeinfo) != NULL;
}

void rfc2015_check_signature (MimeInfo *mimeinfo, FILE *fp)
{
    MimeInfo *partinfo;

    partinfo = rfc2015_find_signature (mimeinfo);
    if (!partinfo)
        return;

#if 0
    g_message ("** yep, it is a pgp signature");
    dump_mimeinfo ("gpg-signature", partinfo );
    dump_part (partinfo, fp );
    dump_mimeinfo ("signed text", mimeinfo->children );
    dump_part (mimeinfo->children, fp);
#endif

    check_signature (mimeinfo, partinfo, fp);
}

int rfc2015_is_encrypted (MimeInfo *mimeinfo)
{
    if (!mimeinfo)
        return 0;
    if (strcasecmp (mimeinfo->content_type, "multipart/encrypted"))
        return 0;
    /* fixme: we should schek the protocol parameter */
    return 1;
}

static int
name_cmp(const char *a, const char *b)
{
    for( ; *a && *b; a++, b++) {
        if(*a != *b
           && toupper(*(unsigned char *)a) != toupper(*(unsigned char *)b))
            return 1;
    }

    return *a != *b;
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
        if(!name_cmp (names[i], p))
            break;
    }
    *p2 = c;

    return names[i] != NULL;
}


void rfc2015_decrypt_message (MsgInfo *msginfo, MimeInfo *mimeinfo, FILE *fp)
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

    g_return_if_fail (mimeinfo->mime_type == MIME_MULTIPART);

    g_message ("** multipart/encrypted encountered");

    /* skip headers */
    if (fseek(fp, mimeinfo->fpos, SEEK_SET) < 0)
        perror("fseek");
    while (fgets(buf, sizeof(buf), fp) != NULL)
        if (buf[0] == '\r' || buf[0] == '\n') break;
    
    procmime_scan_multipart_message(mimeinfo, fp);

    /* check that we have the 2 parts */
    n = found = 0;
    for (partinfo = mimeinfo->children; partinfo; partinfo = partinfo->next) {
        if (++n == 1 && !strcmp (partinfo->content_type,
				 "application/pgp-encrypted")) {
            /* Fixme: check that the version is 1 */
            ver_okay = 1;
        }
        else if (n == 2 && !strcmp (partinfo->content_type,
				    "application/octet-stream")) {
            if (partinfo->next)
                g_warning ("** oops: pgp_encrypted with more than 2 parts");
            break;
        }
    }

    if (!ver_okay || !partinfo) {
        msginfo->decryption_failed = 1;
        /* fixme: remove the stuff, that the above procmime_scan_multiparts() 
         * has appended to mimeino */
        return;
    }

    g_message ("** yep, it is pgp encrypted");

    plain = pgp_decrypt (partinfo, fp);
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
        g_message ("** gpgme_data_rewind failed: %s", gpgme_strerror (err));

    while (!(err = gpgme_data_read (plain, buf, sizeof(buf), &nread))) {
        fwrite (buf, nread, 1, dstfp);
    }

    if (err != GPGME_EOF) {
        g_warning ("** gpgme_data_read failed: %s", gpgme_strerror (err));
    }

    fclose (dstfp);

    msginfo->plaintext_file = fname;
    msginfo->decryption_failed = 0;
}


/* 
 * plain contains an entire mime object.
 * Encrypt it and return an GpgmeData object with the encrypted version of
 * the file or NULL in case of error.
 */
static GpgmeData
pgp_encrypt ( GpgmeData plain, GpgmeRecipients rset )
{
    GpgmeCtx ctx = NULL;
    GpgmeError err;
    GpgmeData cipher = NULL;

    err = gpgme_new (&ctx);
    if (!err)
	err = gpgme_data_new (&cipher);
    if (!err) {
        gpgme_set_armor (ctx, 1);
	err = gpgme_op_encrypt (ctx, rset, plain, cipher);
    }

    if (err) {
        g_warning ("** encryption failed: %s", gpgme_strerror (err));
        gpgme_data_release (cipher);
        cipher = NULL;
    }
    else {
        g_message ("** encryption succeeded");
    }

    gpgme_release (ctx);
    return cipher;
}


/*
 * Encrypt the file by extracting all recipients and finding the
 * encryption keys for all of them.  The file content is then replaced
 * by the encrypted one.  */
int
rfc2015_encrypt (const char *file, GSList *recp_list)
{
    FILE *fp = NULL;
    char buf[BUFFSIZE];
    int i, clineidx, saved_last;
    char *clines[3] = {NULL};
    GpgmeError err;
    GpgmeData header = NULL;
    GpgmeData plain = NULL;
    GpgmeData cipher = NULL;
    GpgmeRecipients rset = NULL;
    size_t nread;
    int mime_version_seen = 0;
    char *boundary = create_boundary ();

    /* Create the list of recipients */
    rset = gpgmegtk_recipient_selection (recp_list);
    if (!rset) {
        g_warning ("error creating recipient list" );
        goto failure;
    }

    /* Open the source file */
    if ((fp = fopen(file, "rb")) == NULL) {
        FILE_OP_ERROR(file, "fopen");
        goto failure;
    }

    err = gpgme_data_new (&header);
    if (!err)
	err = gpgme_data_new (&plain);
    if (err) {
        g_message ("gpgme_data_new failed: %s", gpgme_strerror (err));
        goto failure;
    }

    /* get the content header lines from the source */
    clineidx=0;
    saved_last = 0;
    while (!err && fgets(buf, sizeof(buf), fp)) {
        /* fixme: check for overlong lines */
        if (headerp (buf, content_names)) {
            if (clineidx >= DIM (clines)) {
                g_message ("rfc2015_encrypt: too many content lines");
                goto failure;
            }
            clines[clineidx++] = g_strdup (buf);
            saved_last = 1;
            continue;
        }
        if (saved_last) {
            saved_last = 0;
            if (*buf == ' ' || *buf == '\t') {
                char *last = clines[clineidx-1];
                clines[clineidx-1] = g_strconcat (last, buf, NULL);
                g_free (last);
                continue;
            }
        }

        if (headerp (buf, mime_version_name)) 
            mime_version_seen = 1;

        if (buf[0] == '\r' || buf[0] == '\n')
            break;
        err = gpgme_data_write (header, buf, strlen (buf));
    }
    if (ferror (fp)) {
        FILE_OP_ERROR (file, "fgets");
        goto failure;
    }

    /* write them to the temp data and add the rest of the message */
    for (i = 0; !err && i < clineidx; i++) {
        g_message ("%% %s:%d: cline=`%s'", __FILE__ ,__LINE__, clines[i]);
        err = gpgme_data_write (plain, clines[i], strlen (clines[i]));
    }
    if (!err)
        err = gpgme_data_write (plain, "\r\n", 2);
    while (!err && fgets(buf, sizeof(buf), fp)) {
        err = gpgme_data_write (plain, buf, strlen (buf));
    }
    if (ferror (fp)) {
        FILE_OP_ERROR (file, "fgets");
        goto failure;
    }
    if (err) {
        g_warning ("** gpgme_data_write failed: %s", gpgme_strerror (err));
        goto failure;
    }

    cipher = pgp_encrypt (plain, rset);
    gpgme_data_release (plain); plain = NULL;
    gpgme_recipients_release (rset); rset = NULL;
    if (!cipher)
        goto failure;

    /* we have the encrypted message available in cipher and now we
     * are going to rewrite the source file. To be sure that file has
     * been truncated we use an approach which should work everywhere:
     * close the file and then reopen it for writing. It is important
     * that this works, otherwise it may happen that parts of the
     * plaintext are still in the file (The encrypted stuff is, due to
     * compression, usually shorter than the plaintext). 
     * 
     * Yes, there is a race condition here, but everyone, who is so
     * stupid to store the temp file with the plaintext in a public
     * directory has to live with this anyway. */
    if (fclose (fp)) {
        FILE_OP_ERROR(file, "fclose");
        goto failure;
    }
    if ((fp = fopen(file, "wb")) == NULL) {
        FILE_OP_ERROR(file, "fopen");
        goto failure;
    }

    /* Write the header, append new content lines, part 1 and part 2 header */
    err = gpgme_data_rewind (header);
    if (err) {
        g_message ("gpgme_data_rewind failed: %s", gpgme_strerror (err));
        goto failure;
    }
    while (!(err = gpgme_data_read (header, buf, BUFFSIZE, &nread))) {
        fwrite (buf, nread, 1, fp);
    }
    if (err != GPGME_EOF) {
        g_warning ("** gpgme_data_read failed: %s", gpgme_strerror (err));
        goto failure;
    }
    if (ferror (fp)) {
        FILE_OP_ERROR (file, "fwrite");
        goto failure;
    }
    gpgme_data_release (header); header = NULL;
    
    if (!mime_version_seen) 
        fputs ("MIME-Version: 1\r\n", fp);

    fprintf (fp,
	     "Content-Type: multipart/encrypted;"
	     " protocol=\"application/pgp-encrypted\";\r\n"
	     " boundary=\"%s\"\r\n"
	     "\r\n"
	     "--%s\r\n"
	     "Content-Type: application/pgp-encrypted\r\n"
	     "\r\n"
	     "Version: 1\r\n"
	     "\r\n"
	     "--%s\r\n"
	     "Content-Type: application/octet-stream\r\n"
	     "\r\n",
	     boundary, boundary, boundary);

    /* append the encrypted stuff */
    err = gpgme_data_rewind (cipher);
    if (err) {
        g_warning ("** gpgme_data_rewind on cipher failed: %s",
                   gpgme_strerror (err));
        goto failure;
    }

    while (!(err = gpgme_data_read (cipher, buf, BUFFSIZE, &nread))) {
        fwrite (buf, nread, 1, fp);
    }
    if (err != GPGME_EOF) {
        g_warning ("** gpgme_data_read failed: %s", gpgme_strerror (err));
        goto failure;
    }

    /* and the final boundary */
    fprintf (fp,
	     "\r\n"
	     "--%s--\r\n"
	     "\r\n",
	     boundary);
    fflush (fp);
    if (ferror (fp)) {
        FILE_OP_ERROR (file, "fwrite");
        goto failure;
    }
    fclose (fp);
    gpgme_data_release (cipher);
    return 0;

failure:
    if (fp) 
        fclose (fp);
    gpgme_data_release (header);
    gpgme_data_release (plain);
    gpgme_data_release (cipher);
    gpgme_recipients_release (rset);
    g_free (boundary);
    return -1; /* error */
}

int
set_signers (GpgmeCtx ctx, PrefsAccount *ac)
{
    GSList *key_list = NULL;
    GpgmeCtx list_ctx = NULL;
    const char *keyid = NULL;
    GSList *p;
    GpgmeError err;
    GpgmeKey key;

    if (ac == NULL)
	return 0;

    switch (ac->sign_key) {
    case SIGN_KEY_DEFAULT:
	return 0;		/* nothing to do */

    case SIGN_KEY_BY_FROM:
	keyid = ac->address;
	break;

    case SIGN_KEY_CUSTOM:
	keyid = ac->sign_key_id;
	break;

    default:
	g_assert_not_reached ();
    }

    err = gpgme_new (&list_ctx);
    if (err)
	goto leave;
    err = gpgme_op_keylist_start (list_ctx, keyid, 1);
    if (err)
	goto leave;
    while ( !(err = gpgme_op_keylist_next (list_ctx, &key)) ) {
	key_list = g_slist_append (key_list, key);
    }
    if (err != GPGME_EOF)
	goto leave;
    if (key_list == NULL) {
	g_warning ("no keys found for keyid \"%s\"", keyid);
    }
    gpgme_signers_clear (ctx);
    for (p = key_list; p != NULL; p = p->next) {
	err = gpgme_signers_add (ctx, (GpgmeKey) p->data);
	if (err)
	    goto leave;
    }

leave:
    if (err)
        g_message ("** set_signers failed: %s", gpgme_strerror (err));
    for (p = key_list; p != NULL; p = p->next)
	gpgme_key_unref ((GpgmeKey) p->data);
    g_slist_free (key_list);
    if (list_ctx)
	gpgme_release (list_ctx);
    return err;
}

/* 
 * plain contains an entire mime object.  Sign it and return an
 * GpgmeData object with the signature of it or NULL in case of error.
 */
static GpgmeData
pgp_sign (GpgmeData plain, PrefsAccount *ac)
{
    GpgmeCtx ctx = NULL;
    GpgmeError err;
    GpgmeData sig = NULL;
    struct passphrase_cb_info_s info;

    memset (&info, 0, sizeof info);

    err = gpgme_new (&ctx);
    if (err)
        goto leave;
    err = gpgme_data_new (&sig);
    if (err)
        goto leave;

    if (!getenv("GPG_AGENT_INFO")) {
        info.c = ctx;
        gpgme_set_passphrase_cb (ctx, passphrase_cb, &info);
    }
    gpgme_set_textmode (ctx, 1);
    gpgme_set_armor (ctx, 1);
    err = set_signers (ctx, ac);
    if (err)
	goto leave;
    err = gpgme_op_sign (ctx, plain, sig, GPGME_SIG_MODE_DETACH);

leave:
    if (err) {
        g_message ("** signing failed: %s", gpgme_strerror (err));
        gpgme_data_release (sig);
        sig = NULL;
    }
    else {
        g_message ("** signing succeeded");
    }

    gpgme_release (ctx);
    return sig;
}

/*
 * Sign the file and replace its content with the signed one.
 */
int
rfc2015_sign (const char *file, PrefsAccount *ac)
{
    FILE *fp = NULL;
    char buf[BUFFSIZE];
    int i, clineidx, saved_last;
    char *clines[3] = {NULL};
    GpgmeError err;
    GpgmeData header = NULL;
    GpgmeData plain = NULL;
    GpgmeData sigdata = NULL;
    size_t nread;
    int mime_version_seen = 0;
    char *boundary = create_boundary ();

    /* Open the source file */
    if ((fp = fopen(file, "rb")) == NULL) {
        FILE_OP_ERROR(file, "fopen");
        goto failure;
    }

    err = gpgme_data_new (&header);
    if (!err)
        err = gpgme_data_new (&plain);
    if (err) {
        g_message ("gpgme_data_new failed: %s", gpgme_strerror (err));
        goto failure;
    }

    /* get the content header lines from the source */
    clineidx = 0;
    saved_last = 0;
    while (!err && fgets(buf, sizeof(buf), fp)) {
        /* fixme: check for overlong lines */
        if (headerp (buf, content_names)) {
            if (clineidx >= DIM (clines)) {
                g_message ("rfc2015_sign: too many content lines");
                goto failure;
            }
            clines[clineidx++] = g_strdup (buf);
            saved_last = 1;
            continue;
        }
        if (saved_last) {
            saved_last = 0;
            if (*buf == ' ' || *buf == '\t') {
                char *last = clines[clineidx - 1];
                clines[clineidx - 1] = g_strconcat (last, buf, NULL);
                g_free (last);
                continue;
            }
        }

        if (headerp (buf, mime_version_name)) 
            mime_version_seen = 1;

        if (buf[0] == '\r' || buf[0] == '\n')
            break;
        err = gpgme_data_write (header, buf, strlen (buf));
    }
    if (ferror (fp)) {
        FILE_OP_ERROR (file, "fgets");
        goto failure;
    }

    /* write them to the temp data and add the rest of the message */
    for (i = 0; !err && i < clineidx; i++) {
        err = gpgme_data_write (plain, clines[i], strlen (clines[i]));
    }
    if (!err)
        err = gpgme_data_write (plain, "\r\n", 2 );
    while (!err && fgets(buf, sizeof(buf), fp)) {
        err = gpgme_data_write (plain, buf, strlen (buf));
    }
    if (ferror (fp)) {
        FILE_OP_ERROR (file, "fgets");
        goto failure;
    }
    if (err) {
        g_message ("** gpgme_data_write failed: %s", gpgme_strerror (err));
        goto failure;
    }

    sigdata = pgp_sign (plain, ac);
    if (!sigdata) 
        goto failure;

    /* we have the signed message available in sigdata and now we are
     * going to rewrite the original file. To be sure that file has
     * been truncated we use an approach which should work everywhere:
     * close the file and then reopen it for writing. */
    if (fclose (fp)) {
        FILE_OP_ERROR(file, "fclose");
        goto failure;
    }
    if ((fp = fopen(file, "wb")) == NULL) {
        FILE_OP_ERROR(file, "fopen");
        goto failure;
    }

    /* Write the rfc822 header and add new content lines */
    err = gpgme_data_rewind (header);
    if (err)
        g_message ("** gpgme_data_rewind failed: %s", gpgme_strerror (err));
    while (!(err = gpgme_data_read (header, buf, BUFFSIZE, &nread))) {
        fwrite (buf, nread, 1, fp);
    }
    if (err != GPGME_EOF) {
        g_message ("** gpgme_data_read failed: %s", gpgme_strerror (err));
        goto failure;
    }
    if (ferror (fp)) {
        FILE_OP_ERROR (file, "fwrite");
        goto failure;
    }
    gpgme_data_release (header);
    header = NULL;

    if (!mime_version_seen) 
        fputs ("MIME-Version: 1\r\n", fp);
    fprintf (fp, "Content-Type: multipart/signed; "
		 "protocol=\"application/pgp-signature\";\r\n"
		 " boundary=\"%s\"\r\n", boundary );

    /* Part 1: signed material */
    fprintf (fp, "\r\n--%s\r\n", boundary);
    err = gpgme_data_rewind (plain);
    if (err) {
        g_message ("** gpgme_data_rewind on plain failed: %s",
                   gpgme_strerror (err));
        goto failure;
    }
    while (!(err = gpgme_data_read (plain, buf, BUFFSIZE, &nread))) {
        fwrite (buf, nread, 1, fp);   
    }
    if (err != GPGME_EOF) {
        g_message ("** gpgme_data_read failed: %s", gpgme_strerror (err));
        goto failure;
    }

    /* Part 2: signature */
    fprintf (fp, "\r\n--%s\r\n", boundary);
    fputs ("Content-Type: application/pgp-signature\r\n"
	   "\r\n", fp);

    err = gpgme_data_rewind (sigdata);
    if (err) {
        g_message ("** gpgme_data_rewind on sigdata failed: %s",
                   gpgme_strerror (err));
        goto failure;
    }

    while (!(err = gpgme_data_read (sigdata, buf, BUFFSIZE, &nread))) {
        fwrite (buf, nread, 1, fp);
    }
    if (err != GPGME_EOF) {
        g_message ("** gpgme_data_read failed: %s", gpgme_strerror (err));
        goto failure;
    }

    /* Final boundary */
    fprintf (fp, "\r\n--%s--\r\n\r\n", boundary);
    fflush (fp);
    if (ferror (fp)) {
        FILE_OP_ERROR (file, "fwrite");
        goto failure;
    }
    fclose (fp);
    gpgme_data_release (header);
    gpgme_data_release (plain);
    gpgme_data_release (sigdata);
    g_free (boundary);
    return 0;

failure:
    if (fp) 
        fclose (fp);
    gpgme_data_release (header);
    gpgme_data_release (plain);
    gpgme_data_release (sigdata);
    g_free (boundary);
    return -1; /* error */
}


/****************
 * Create a new boundary in a way that it is very unlikely that this
 * will occur in the following text.  It would be easy to ensure
 * uniqueness if everything is either quoted-printable or base64
 * encoded (note that conversion is allowed), but because MIME bodies
 * may be nested, it may happen that the same boundary has already
 * been used. We avoid scanning the message for conflicts and hope the
 * best.
 *
 *   boundary := 0*69<bchars> bcharsnospace
 *   bchars := bcharsnospace / " "
 *   bcharsnospace := DIGIT / ALPHA / "'" / "(" / ")" /
 *                    "+" / "_" / "," / "-" / "." /
 *                    "/" / ":" / "=" / "?"  
 */

static char *
create_boundary (void)
{
    static char tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			"abcdefghijklmnopqrstuvwxyz"
			"1234567890'()+_,./:=?";
    char buf[17];
    int i, equal;
    int pid;

    pid = getpid();

    /* We make the boundary depend on the pid, so that all running
     * processed generate different values even when they have been
     * started within the same second and srand48(time(NULL)) has been
     * used.  I can't see whether this is really an advantage but it
     * doesn't do any harm.
     */
    equal = -1;
    for(i = 0; i < sizeof(buf) - 1; i++) {
	buf[i] = tbl[(lrand48() ^ pid) % (sizeof(tbl) - 1)]; /* fill with random */
	if(buf[i] == '=' && equal == -1)
	    equal = i;
    }
    buf[i] = 0;

    /* now make sure that we do have the sequence "=." in it which cannot
     * be matched by quoted-printable or base64 encoding */
    if(equal != -1 && (equal+1) < i)
	buf[equal+1] = '.';
    else {
	buf[0] = '=';
	buf[1] = '.';
    }

    return g_strdup(buf);
}

#endif /* USE_GPGME */
