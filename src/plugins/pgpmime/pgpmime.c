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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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
#include <plugins/pgpcore/sgpgme.h>
#include <plugins/pgpcore/prefs_gpg.h>
#include <plugins/pgpcore/passphrase.h>

#include "prefs_common.h"

typedef struct _PrivacyDataPGP PrivacyDataPGP;

struct _PrivacyDataPGP
{
	PrivacyData	data;
	
	gboolean	done_sigtest;
	gboolean	is_signed;
	gpgme_verify_result_t	sigstatus;
	gpgme_ctx_t 	ctx;
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
	data->sigstatus = NULL;
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
	    g_ascii_strcasecmp(parent->subtype, "signed"))
		return FALSE;
	protocol = procmime_mimeinfo_get_parameter(parent, "protocol");
	if ((protocol == NULL) || g_ascii_strcasecmp(protocol, "application/pgp-signature"))
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
	    g_ascii_strcasecmp(signature->subtype, "pgp-signature"))
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
	gpgme_data_t sigdata = NULL, textdata = NULL;
	gpgme_error_t err;
	g_return_val_if_fail(mimeinfo != NULL, -1);
	g_return_val_if_fail(mimeinfo->privacy != NULL, -1);
	data = (PrivacyDataPGP *) mimeinfo->privacy;
	gpgme_new(&data->ctx);
	
	debug_print("Checking PGP/MIME signature\n");
	parent = procmime_mimeinfo_parent(mimeinfo);

	fp = g_fopen(parent->data.filename, "rb");
	g_return_val_if_fail(fp != NULL, SIGNATURE_INVALID);
	
	boundary = g_hash_table_lookup(parent->typeparameters, "boundary");
	if (!boundary)
		return 0;

	textstr = get_canonical_content(fp, boundary);

	err = gpgme_data_new_from_mem(&textdata, textstr, strlen(textstr), 0);
	if (err) {
		debug_print ("gpgme_data_new_from_mem failed: %s\n",
                   gpgme_strerror (err));
	}
	signature = (MimeInfo *) mimeinfo->node->next->data;
	sigdata = sgpgme_data_from_mimeinfo(signature);

	data->sigstatus =
		sgpgme_verify_signature	(data->ctx, sigdata, textdata, NULL);

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

	if (data->sigstatus == NULL && 
	    prefs_gpg_get_config()->auto_check_signatures)
		pgpmime_check_signature(mimeinfo);
	
	return sgpgme_sigstat_gpgme_to_privacy(data->ctx, data->sigstatus);
}

static gchar *pgpmime_get_sig_info_short(MimeInfo *mimeinfo)
{
	PrivacyDataPGP *data = (PrivacyDataPGP *) mimeinfo->privacy;
	
	g_return_val_if_fail(data != NULL, g_strdup("Error"));

	if (data->sigstatus == NULL && 
	    prefs_gpg_get_config()->auto_check_signatures)
		pgpmime_check_signature(mimeinfo);
	
	return sgpgme_sigstat_info_short(data->ctx, data->sigstatus);
}

static gchar *pgpmime_get_sig_info_full(MimeInfo *mimeinfo)
{
	PrivacyDataPGP *data = (PrivacyDataPGP *) mimeinfo->privacy;
	
	g_return_val_if_fail(data != NULL, g_strdup("Error"));

	if (data->sigstatus == NULL && 
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
	if (g_ascii_strcasecmp(mimeinfo->subtype, "encrypted"))
		return FALSE;
	tmpstr = procmime_mimeinfo_get_parameter(mimeinfo, "protocol");
	if ((tmpstr == NULL) || g_ascii_strcasecmp(tmpstr, "application/pgp-encrypted"))
		return FALSE;
	if (g_node_n_children(mimeinfo->node) != 2)
		return FALSE;
	
	tmpinfo = (MimeInfo *) g_node_nth_child(mimeinfo->node, 0)->data;
	if (tmpinfo->type != MIMETYPE_APPLICATION)
		return FALSE;
	if (g_ascii_strcasecmp(tmpinfo->subtype, "pgp-encrypted"))
		return FALSE;
	
	tmpinfo = (MimeInfo *) g_node_nth_child(mimeinfo->node, 1)->data;
	if (tmpinfo->type != MIMETYPE_APPLICATION)
		return FALSE;
	if (g_ascii_strcasecmp(tmpinfo->subtype, "octet-stream"))
		return FALSE;
	
	return TRUE;
}

static MimeInfo *pgpmime_decrypt(MimeInfo *mimeinfo)
{
	MimeInfo *encinfo, *decinfo, *parseinfo;
	gpgme_data_t cipher = NULL, plain = NULL;
	static gint id = 0;
	FILE *dstfp;
	gchar *fname;
	gpgme_verify_result_t sigstat = NULL;
	PrivacyDataPGP *data = NULL;
	gpgme_ctx_t ctx;
	gchar *chars;
	size_t len;

	if (gpgme_new(&ctx) != GPG_ERR_NO_ERROR)
		return NULL;

	
	g_return_val_if_fail(pgpmime_is_encrypted(mimeinfo), NULL);
	
	encinfo = (MimeInfo *) g_node_nth_child(mimeinfo->node, 1)->data;

	cipher = sgpgme_data_from_mimeinfo(encinfo);
	plain = sgpgme_decrypt_verify(cipher, &sigstat, ctx);

	gpgme_data_release(cipher);
	if (plain == NULL) {
		debug_print("plain is null!\n");
		gpgme_release(ctx);
		return NULL;
	}

    	fname = g_strdup_printf("%s%cplaintext.%08x",
		get_mime_tmp_dir(), G_DIR_SEPARATOR, ++id);

    	if ((dstfp = g_fopen(fname, "wb")) == NULL) {
        	FILE_OP_ERROR(fname, "fopen");
        	g_free(fname);
        	gpgme_data_release(plain);
		gpgme_release(ctx);
		debug_print("can't open!\n");
		return NULL;
    	}

	fprintf(dstfp, "MIME-Version: 1.0\n");

	chars = gpgme_data_release_and_get_mem(plain, &len);
	if (len > 0)
		fwrite(chars, len, 1, dstfp);
	fclose(dstfp);

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

	if (sigstat != NULL && sigstat->signatures != NULL) {
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

gboolean pgpmime_sign(MimeInfo *mimeinfo, PrefsAccount *account)
{
	MimeInfo *msgcontent, *sigmultipart, *newinfo;
	gchar *textstr, *micalg;
	FILE *fp;
	gchar *boundary, *sigcontent;
	gpgme_ctx_t ctx;
	gpgme_data_t gpgtext, gpgsig;
	size_t len;
	struct passphrase_cb_info_s info;
	gpgme_sign_result_t result = NULL;

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
	gpgme_signers_clear (ctx);

	if (!sgpgme_setup_signers(ctx, account)) {
		gpgme_release(ctx);
		return FALSE;
	}

	if (!getenv("GPG_AGENT_INFO")) {
    		info.c = ctx;
    		gpgme_set_passphrase_cb (ctx, gpgmegtk_passphrase_cb, &info);
	}

	if (gpgme_op_sign(ctx, gpgtext, gpgsig, GPGME_SIG_MODE_DETACH) != GPG_ERR_NO_ERROR) {
		gpgme_release(ctx);
		return FALSE;
	}
	result = gpgme_op_sign_result(ctx);
	if (result && result->signatures) {
	    if (gpgme_get_protocol(ctx) == GPGME_PROTOCOL_OpenPGP) {
		micalg = g_strdup_printf("PGP-%s", gpgme_hash_algo_name(
			    result->signatures->hash_algo));
	    } else {
		micalg = g_strdup(gpgme_hash_algo_name(
			    result->signatures->hash_algo));
	    }
	} else {
	    /* can't get result (maybe no signing key?) */
	    return FALSE;
	}

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
	gpgme_data_t gpgtext = NULL, gpgenc = NULL;
	gpgme_ctx_t ctx = NULL;
	gpgme_key_t *kset = NULL;
	gchar **fprs = g_strsplit(encrypt_data, " ", -1);
	gint i = 0;
	while (fprs[i] && strlen(fprs[i])) {
		i++;
	}
	
	kset = g_malloc(sizeof(gpgme_key_t)*(i+1));
	memset(kset, 0, sizeof(gpgme_key_t)*(i+1));
	gpgme_new(&ctx);
	i = 0;
	while (fprs[i] && strlen(fprs[i])) {
		gpgme_key_t key;
		gpgme_error_t err;
		err = gpgme_get_key(ctx, fprs[i], &key, 0);
		if (err) {
			debug_print("can't add key '%s'[%d] (%s)\n", fprs[i],i, gpgme_strerror(err));
			break;
		}
		debug_print("found %s at %d\n", fprs[i], i);
		kset[i] = key;
		i++;
	}
	
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
	gpgme_set_armor(ctx, 1);
	gpgme_data_rewind(gpgtext);
	
	gpgme_op_encrypt(ctx, kset, GPGME_ENCRYPT_ALWAYS_TRUST, gpgtext, gpgenc);

	gpgme_release(ctx);
	enccontent = gpgme_data_release_and_get_mem(gpgenc, &len);
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
