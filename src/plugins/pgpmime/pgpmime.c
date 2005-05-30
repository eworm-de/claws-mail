/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2003 Hiroyuki Yamamoto & the Sylpheed-Claws team
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
#include <gpgme.h>
#include <ctype.h>

#include "utils.h"
#include "privacy.h"
#include "procmime.h"
#include "pgpmime.h"
#include "sgpgme.h"
#include "prefs_common.h"
#include "prefs_gpg.h"
#include "passphrase.h"

typedef struct _PrivacyDataPGP PrivacyDataPGP;

struct _PrivacyDataPGP
{
	PrivacyData	data;
	
	gboolean	done_sigtest;
	gboolean	is_signed;
	GpgmeSigStat	sigstatus;
	GpgmeCtx 	ctx;
};

static PrivacySystem pgpmime_system;

static gint pgpmime_check_signature(MimeInfo *mimeinfo);

static PrivacyDataPGP *pgpmime_new_privacydata()
{
	PrivacyDataPGP *data;

	data = g_new0(PrivacyDataPGP, 1);
	data->data.system = &pgpmime_system;
	data->done_sigtest = FALSE;
	data->is_signed = FALSE;
	data->sigstatus = GPGME_SIG_STAT_NONE;
	gpgme_new(&data->ctx);
	
	return data;
}

static void pgpmime_free_privacydata(PrivacyData *_data)
{
	PrivacyDataPGP *data = (PrivacyDataPGP *) _data;
	gpgme_release(data->ctx);
	g_free(data);
}

static gboolean pgpmime_is_signed(MimeInfo *mimeinfo)
{
	MimeInfo *parent;
	MimeInfo *signature;
	const gchar *protocol;
	PrivacyDataPGP *data = NULL;
	
	g_return_val_if_fail(mimeinfo != NULL, FALSE);
	if (mimeinfo->privacy != NULL) {
		data = (PrivacyDataPGP *) mimeinfo->privacy;
		if (data->done_sigtest)
			return data->is_signed;
	}
	
	/* check parent */
	parent = procmime_mimeinfo_parent(mimeinfo);
	if (parent == NULL)
		return FALSE;
	if ((parent->type != MIMETYPE_MULTIPART) ||
	    g_strcasecmp(parent->subtype, "signed"))
		return FALSE;
	protocol = procmime_mimeinfo_get_parameter(parent, "protocol");
	if ((protocol == NULL) || g_strcasecmp(protocol, "application/pgp-signature"))
		return FALSE;

	/* check if mimeinfo is the first child */
	if (parent->node->children->data != mimeinfo)
		return FALSE;

	/* check signature */
	signature = parent->node->children->next != NULL ? 
	    (MimeInfo *) parent->node->children->next->data : NULL;
	if (signature == NULL)
		return FALSE;
	if ((signature->type != MIMETYPE_APPLICATION) ||
	    g_strcasecmp(signature->subtype, "pgp-signature"))
		return FALSE;

	if (data == NULL) {
		data = pgpmime_new_privacydata();
		mimeinfo->privacy = (PrivacyData *) data;
	}
	data->done_sigtest = TRUE;
	data->is_signed = TRUE;

	return TRUE;
}

static gchar *get_canonical_content(FILE *fp, const gchar *boundary)
{
	gchar *ret;
	GString *textbuffer;
	guint boundary_len;
	gchar buf[BUFFSIZE];

	boundary_len = strlen(boundary);
	while (fgets(buf, sizeof(buf), fp) != NULL)
		if (IS_BOUNDARY(buf, boundary, boundary_len))
			break;

	textbuffer = g_string_new("");
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		gchar *buf2;

		if (IS_BOUNDARY(buf, boundary, boundary_len))
			break;
		
		buf2 = canonicalize_str(buf);
		g_string_append(textbuffer, buf2);
		g_free(buf2);
	}
	g_string_truncate(textbuffer, textbuffer->len - 2);
		
	ret = textbuffer->str;
	g_string_free(textbuffer, FALSE);

	return ret;
}

static gint pgpmime_check_signature(MimeInfo *mimeinfo)
{
	PrivacyDataPGP *data;
	MimeInfo *parent, *signature;
	FILE *fp;
	gchar *boundary;
	gchar *textstr;
	GpgmeData sigdata, textdata;
	
	g_return_val_if_fail(mimeinfo != NULL, -1);
	g_return_val_if_fail(mimeinfo->privacy != NULL, -1);
	data = (PrivacyDataPGP *) mimeinfo->privacy;
	
	debug_print("Checking PGP/MIME signature\n");
	parent = procmime_mimeinfo_parent(mimeinfo);

	fp = fopen(parent->data.filename, "rb");
	g_return_val_if_fail(fp != NULL, SIGNATURE_INVALID);
	
	boundary = g_hash_table_lookup(parent->typeparameters, "boundary");
	if (!boundary)
		return 0;

	textstr = get_canonical_content(fp, boundary);

	gpgme_data_new_from_mem(&textdata, textstr, strlen(textstr), 0);
	signature = (MimeInfo *) mimeinfo->node->next->data;
	sigdata = sgpgme_data_from_mimeinfo(signature);

	data->sigstatus =
		sgpgme_verify_signature	(data->ctx, sigdata, textdata);
	
	gpgme_data_release(sigdata);
	gpgme_data_release(textdata);
	g_free(textstr);
	fclose(fp);
	
	return 0;
}

static SignatureStatus pgpmime_get_sig_status(MimeInfo *mimeinfo)
{
	PrivacyDataPGP *data = (PrivacyDataPGP *) mimeinfo->privacy;
	
	g_return_val_if_fail(data != NULL, SIGNATURE_INVALID);

	if (data->sigstatus == GPGME_SIG_STAT_NONE && 
	    prefs_gpg_get_config()->auto_check_signatures)
		pgpmime_check_signature(mimeinfo);
	
	return sgpgme_sigstat_gpgme_to_privacy(data->ctx, data->sigstatus);
}

static gchar *pgpmime_get_sig_info_short(MimeInfo *mimeinfo)
{
	PrivacyDataPGP *data = (PrivacyDataPGP *) mimeinfo->privacy;
	
	g_return_val_if_fail(data != NULL, g_strdup("Error"));

	if (data->sigstatus == GPGME_SIG_STAT_NONE && 
	    prefs_gpg_get_config()->auto_check_signatures)
		pgpmime_check_signature(mimeinfo);
	
	return sgpgme_sigstat_info_short(data->ctx, data->sigstatus);
}

static gchar *pgpmime_get_sig_info_full(MimeInfo *mimeinfo)
{
	PrivacyDataPGP *data = (PrivacyDataPGP *) mimeinfo->privacy;
	
	g_return_val_if_fail(data != NULL, g_strdup("Error"));

	if (data->sigstatus == GPGME_SIG_STAT_NONE && 
	    prefs_gpg_get_config()->auto_check_signatures)
		pgpmime_check_signature(mimeinfo);
	
	return sgpgme_sigstat_info_full(data->ctx, data->sigstatus);
}

static gboolean pgpmime_is_encrypted(MimeInfo *mimeinfo)
{
	MimeInfo *tmpinfo;
	const gchar *tmpstr;
	
	if (mimeinfo->type != MIMETYPE_MULTIPART)
		return FALSE;
	if (g_strcasecmp(mimeinfo->subtype, "encrypted"))
		return FALSE;
	tmpstr = procmime_mimeinfo_get_parameter(mimeinfo, "protocol");
	if ((tmpstr == NULL) || g_strcasecmp(tmpstr, "application/pgp-encrypted"))
		return FALSE;
	if (g_node_n_children(mimeinfo->node) != 2)
		return FALSE;
	
	tmpinfo = (MimeInfo *) g_node_nth_child(mimeinfo->node, 0)->data;
	if (tmpinfo->type != MIMETYPE_APPLICATION)
		return FALSE;
	if (g_strcasecmp(tmpinfo->subtype, "pgp-encrypted"))
		return FALSE;
	
	tmpinfo = (MimeInfo *) g_node_nth_child(mimeinfo->node, 1)->data;
	if (tmpinfo->type != MIMETYPE_APPLICATION)
		return FALSE;
	if (g_strcasecmp(tmpinfo->subtype, "octet-stream"))
		return FALSE;
	
	return TRUE;
}

static MimeInfo *pgpmime_decrypt(MimeInfo *mimeinfo)
{
	MimeInfo *encinfo, *decinfo, *parseinfo;
	GpgmeData cipher, plain;
	static gint id = 0;
	FILE *dstfp;
	gint nread;
	gchar *fname;
	gchar buf[BUFFSIZE];
	GpgmeSigStat sigstat = 0;
	PrivacyDataPGP *data = NULL;
	GpgmeCtx ctx;
	
	if (gpgme_new(&ctx) != GPGME_No_Error)
		return NULL;

	
	g_return_val_if_fail(pgpmime_is_encrypted(mimeinfo), NULL);
	
	encinfo = (MimeInfo *) g_node_nth_child(mimeinfo->node, 1)->data;

	cipher = sgpgme_data_from_mimeinfo(encinfo);
	plain = sgpgme_decrypt_verify(cipher, &sigstat, ctx);

	gpgme_data_release(cipher);
	if (plain == NULL) {
		gpgme_release(ctx);
		return NULL;
	}

    	fname = g_strdup_printf("%s%cplaintext.%08x",
		get_mime_tmp_dir(), G_DIR_SEPARATOR, ++id);

    	if ((dstfp = fopen(fname, "wb")) == NULL) {
        	FILE_OP_ERROR(fname, "fopen");
        	g_free(fname);
        	gpgme_data_release(plain);
		gpgme_release(ctx);
		return NULL;
    	}

	fprintf(dstfp, "MIME-Version: 1.0\n");
	gpgme_data_rewind (plain);
	while (gpgme_data_read(plain, buf, sizeof(buf), &nread) == GPGME_No_Error) {
      		fwrite (buf, nread, 1, dstfp);
	}
	fclose(dstfp);
	
	gpgme_data_release(plain);

	parseinfo = procmime_scan_file(fname);
	g_free(fname);
	if (parseinfo == NULL) {
		gpgme_release(ctx);
		return NULL;
	}
	decinfo = g_node_first_child(parseinfo->node) != NULL ?
		g_node_first_child(parseinfo->node)->data : NULL;
	if (decinfo == NULL) {
		gpgme_release(ctx);
		return NULL;
	}

	g_node_unlink(decinfo->node);
	procmime_mimeinfo_free_all(parseinfo);

	decinfo->tmp = TRUE;

	if (sigstat != GPGME_SIG_STAT_NONE) {
		if (decinfo->privacy != NULL) {
			data = (PrivacyDataPGP *) decinfo->privacy;
		} else {
			data = pgpmime_new_privacydata();
			decinfo->privacy = (PrivacyData *) data;	
		}
		data->done_sigtest = TRUE;
		data->is_signed = TRUE;
		data->sigstatus = sigstat;
		if (data->ctx)
			gpgme_release(data->ctx);
		data->ctx = ctx;
	} else
		gpgme_release(ctx);

	return decinfo;
}

/*
 * Find TAG in XML and return a pointer into xml set just behind the
 * closing angle.  Return NULL if not found. 
 */
static const char *
find_xml_tag (const char *xml, const char *tag)
{
    int taglen = strlen (tag);
    const char *s = xml;
 
    while ( (s = strchr (s, '<')) ) {
        s++;
        if (!strncmp (s, tag, taglen)) {
            const char *s2 = s + taglen;
            if (*s2 == '>' || isspace (*(const unsigned char*)s2) ) {
                /* found */
                while (*s2 && *s2 != '>') /* skip attributes */
                    s2++;
                /* fixme: do need to handle angles inside attribute vallues? */
                return *s2? (s2+1):NULL;
            }
        }
        while (*s && *s != '>') /* skip to end of tag */
            s++;
    }
    return NULL;
}


/*
 * Extract the micalg from an GnupgOperationInfo XML container.
 */
static char *
extract_micalg (char *xml)
{
    const char *s;

    s = find_xml_tag (xml, "GnupgOperationInfo");
    if (s) {
        const char *s_end = find_xml_tag (s, "/GnupgOperationInfo");
        s = find_xml_tag (s, "signature");
        if (s && s_end && s < s_end) {
            const char *s_end2 = find_xml_tag (s, "/signature");
            if (s_end2 && s_end2 < s_end) {
                s = find_xml_tag (s, "micalg");
                if (s && s < s_end2) {
                    s_end = strchr (s, '<');
                    if (s_end) {
                        char *p = g_malloc (s_end - s + 1);
                        memcpy (p, s, s_end - s);
                        p[s_end-s] = 0;
                        return p;
                    }
                }
            }
        }
    }
    return NULL;
}

gboolean pgpmime_sign(MimeInfo *mimeinfo, PrefsAccount *account)
{
	MimeInfo *msgcontent, *sigmultipart, *newinfo;
	gchar *textstr, *opinfo, *micalg;
	FILE *fp;
	gchar *boundary, *sigcontent;
	GpgmeCtx ctx;
	GpgmeData gpgtext, gpgsig;
	size_t len;
	struct passphrase_cb_info_s info;

	memset (&info, 0, sizeof info);

	/* remove content node from message */
	msgcontent = (MimeInfo *) mimeinfo->node->children->data;
	g_node_unlink(msgcontent->node);

	/* create temporary multipart for content */
	sigmultipart = procmime_mimeinfo_new();
	sigmultipart->type = MIMETYPE_MULTIPART;
	sigmultipart->subtype = g_strdup("signed");
	boundary = generate_mime_boundary("Signature");
	g_hash_table_insert(sigmultipart->typeparameters, g_strdup("boundary"),
                            g_strdup(boundary));
	g_hash_table_insert(sigmultipart->typeparameters, g_strdup("protocol"),
                            g_strdup("application/pgp-signature"));
	g_node_append(sigmultipart->node, msgcontent->node);
	g_node_append(mimeinfo->node, sigmultipart->node);

	/* write message content to temporary file */
	fp = my_tmpfile();
	procmime_write_mimeinfo(sigmultipart, fp);
	rewind(fp);

	/* read temporary file into memory */
	textstr = get_canonical_content(fp, boundary);

	fclose(fp);

	gpgme_data_new_from_mem(&gpgtext, textstr, strlen(textstr), 0);
	gpgme_data_new(&gpgsig);
	gpgme_new(&ctx);
	gpgme_set_textmode(ctx, 1);
	gpgme_set_armor(ctx, 1);

	if (!sgpgme_setup_signers(ctx, account)) {
		gpgme_release(ctx);
		return FALSE;
	}

	if (!getenv("GPG_AGENT_INFO")) {
    		info.c = ctx;
    		gpgme_set_passphrase_cb (ctx, gpgmegtk_passphrase_cb, &info);
	}

	if (gpgme_op_sign(ctx, gpgtext, gpgsig, GPGME_SIG_MODE_DETACH) != GPGME_No_Error) {
		gpgme_release(ctx);
		return FALSE;
	}
	opinfo = gpgme_get_op_info(ctx, 0);
	micalg = extract_micalg(opinfo);
	g_free(opinfo);

	gpgme_release(ctx);
	sigcontent = gpgme_data_release_and_get_mem(gpgsig, &len);
	gpgme_data_release(gpgtext);
	g_free(textstr);

	/* add signature */
	g_hash_table_insert(sigmultipart->typeparameters, g_strdup("micalg"),
                            micalg);

	newinfo = procmime_mimeinfo_new();
	newinfo->type = MIMETYPE_APPLICATION;
	newinfo->subtype = g_strdup("pgp-signature");
	newinfo->content = MIMECONTENT_MEM;
	newinfo->data.mem = g_malloc(len + 1);
	g_memmove(newinfo->data.mem, sigcontent, len);
	newinfo->data.mem[len] = '\0';
	g_node_append(sigmultipart->node, newinfo->node);

	g_free(sigcontent);

	return TRUE;
}

gchar *pgpmime_get_encrypt_data(GSList *recp_names)
{
	return sgpgme_get_encrypt_data(recp_names);
}

gboolean pgpmime_encrypt(MimeInfo *mimeinfo, const gchar *encrypt_data)
{
	MimeInfo *msgcontent, *encmultipart, *newinfo;
	FILE *fp;
	gchar *boundary, *enccontent;
	size_t len;
	gchar *textstr;
	GpgmeData gpgtext, gpgenc;
	gchar **recipients, **nextrecp;
	GpgmeRecipients recp;
	GpgmeCtx ctx;

	/* build GpgmeRecipients from encrypt_data */
	recipients = g_strsplit(encrypt_data, " ", 0);
	gpgme_recipients_new(&recp);
	for (nextrecp = recipients; *nextrecp != NULL; nextrecp++) {
		gpgme_recipients_add_name_with_validity(recp, *nextrecp,
							GPGME_VALIDITY_FULL);
	}
	g_strfreev(recipients);

	debug_print("Encrypting message content\n");

	/* remove content node from message */
	msgcontent = (MimeInfo *) mimeinfo->node->children->data;
	g_node_unlink(msgcontent->node);

	/* create temporary multipart for content */
	encmultipart = procmime_mimeinfo_new();
	encmultipart->type = MIMETYPE_MULTIPART;
	encmultipart->subtype = g_strdup("encrypted");
	boundary = generate_mime_boundary("Encrypt");
	g_hash_table_insert(encmultipart->typeparameters, g_strdup("boundary"),
                            g_strdup(boundary));
	g_hash_table_insert(encmultipart->typeparameters, g_strdup("protocol"),
                            g_strdup("application/pgp-encrypted"));
	g_node_append(encmultipart->node, msgcontent->node);

	/* write message content to temporary file */
	fp = my_tmpfile();
	procmime_write_mimeinfo(encmultipart, fp);
	rewind(fp);

	/* read temporary file into memory */
	textstr = get_canonical_content(fp, boundary);

	fclose(fp);

	/* encrypt data */
	gpgme_data_new_from_mem(&gpgtext, textstr, strlen(textstr), 0);
	gpgme_data_new(&gpgenc);
	gpgme_new(&ctx);
	gpgme_set_armor(ctx, 1);

	gpgme_op_encrypt(ctx, recp, gpgtext, gpgenc);

	gpgme_release(ctx);
	enccontent = gpgme_data_release_and_get_mem(gpgenc, &len);
	gpgme_recipients_release(recp);
	gpgme_data_release(gpgtext);
	g_free(textstr);

	/* create encrypted multipart */
	g_node_unlink(msgcontent->node);
	procmime_mimeinfo_free_all(msgcontent);
	g_node_append(mimeinfo->node, encmultipart->node);

	newinfo = procmime_mimeinfo_new();
	newinfo->type = MIMETYPE_APPLICATION;
	newinfo->subtype = g_strdup("pgp-encrypted");
	newinfo->content = MIMECONTENT_MEM;
	newinfo->data.mem = g_strdup("Version: 1\n");
	g_node_append(encmultipart->node, newinfo->node);

	newinfo = procmime_mimeinfo_new();
	newinfo->type = MIMETYPE_APPLICATION;
	newinfo->subtype = g_strdup("octet-stream");
	newinfo->content = MIMECONTENT_MEM;
	newinfo->data.mem = g_malloc(len + 1);
	g_memmove(newinfo->data.mem, enccontent, len);
	newinfo->data.mem[len] = '\0';
	g_node_append(encmultipart->node, newinfo->node);

	g_free(enccontent);

	return TRUE;
}

static PrivacySystem pgpmime_system = {
	"pgpmime",			/* id */
	"PGP MIME",			/* name */

	pgpmime_free_privacydata,	/* free_privacydata */

	pgpmime_is_signed,		/* is_signed(MimeInfo *) */
	pgpmime_check_signature,	/* check_signature(MimeInfo *) */
	pgpmime_get_sig_status,		/* get_sig_status(MimeInfo *) */
	pgpmime_get_sig_info_short,	/* get_sig_info_short(MimeInfo *) */
	pgpmime_get_sig_info_full,	/* get_sig_info_full(MimeInfo *) */

	pgpmime_is_encrypted,		/* is_encrypted(MimeInfo *) */
	pgpmime_decrypt,		/* decrypt(MimeInfo *) */

	TRUE,
	pgpmime_sign,

	TRUE,
	pgpmime_get_encrypt_data,
	pgpmime_encrypt,
};

void pgpmime_init()
{
	privacy_register_system(&pgpmime_system);
}

void pgpmime_done()
{
	privacy_unregister_system(&pgpmime_system);
}

#endif /* USE_GPGME */
