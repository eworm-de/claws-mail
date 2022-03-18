/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2021 the Claws Mail team
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

#ifdef USE_GPGME

#include "defs.h"
#include <glib.h>
#include <glib/gi18n.h>
#include <gpgme.h>
#include <ctype.h>
#include <errno.h>

#include "utils.h"
#include "privacy.h"
#include "procmime.h"
#include "plugin.h"

#include "pgpmime.h"
#include <plugins/pgpcore/sgpgme.h>
#include <plugins/pgpcore/prefs_gpg.h>
#include <plugins/pgpcore/passphrase.h>
#include <plugins/pgpcore/pgp_utils.h>

#include "prefs_common.h"
#include "file-utils.h"

typedef struct _PrivacyDataPGP PrivacyDataPGP;

struct _PrivacyDataPGP
{
	PrivacyData	data;
	
	gboolean	done_sigtest;
	gboolean	is_signed;
};

static PrivacySystem pgpmime_system;

static PrivacyDataPGP *pgpmime_new_privacydata()
{
	PrivacyDataPGP *data;

	data = g_new0(PrivacyDataPGP, 1);
	data->data.system = &pgpmime_system;
	
	return data;
}

static void pgpmime_free_privacydata(PrivacyData *data)
{
	g_free(data);
}

static gboolean pgpmime_is_signed(MimeInfo *mimeinfo)
{
	MimeInfo *parent;
	MimeInfo *signature;
	const gchar *protocol;
	PrivacyDataPGP *data = NULL;
	
	cm_return_val_if_fail(mimeinfo != NULL, FALSE);
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
	if ((protocol == NULL) || 
	    (g_ascii_strcasecmp(protocol, "application/pgp-signature")))
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
	    (g_ascii_strcasecmp(signature->subtype, "pgp-signature")))
		return FALSE;

	if (data == NULL) {
		data = pgpmime_new_privacydata();
		mimeinfo->privacy = (PrivacyData *) data;
	}
	if (data != NULL) {
		data->done_sigtest = TRUE;
		data->is_signed = TRUE;
	}

	return TRUE;
}

static gchar *get_canonical_content(FILE *fp, const gchar *boundary)
{
	GString *textbuffer;
	guint boundary_len;
	gchar buf[BUFFSIZE];

	boundary_len = strlen(boundary);
	while (claws_fgets(buf, sizeof(buf), fp) != NULL)
		if (IS_BOUNDARY(buf, boundary, boundary_len))
			break;

	textbuffer = g_string_new("");
	while (claws_fgets(buf, sizeof(buf), fp) != NULL) {
		gchar *buf2;

		if (IS_BOUNDARY(buf, boundary, boundary_len))
			break;
		
		buf2 = canonicalize_str(buf);
		g_string_append(textbuffer, buf2);
		g_free(buf2);
	}
	g_string_truncate(textbuffer, textbuffer->len - 2);
		
	return g_string_free(textbuffer, FALSE);
}

static gint pgpmime_check_sig_async(MimeInfo *mimeinfo,
	GCancellable *cancellable,
	GAsyncReadyCallback callback,
	gpointer user_data)
{
	return cm_check_detached_sig_async(mimeinfo,
		cancellable,
		callback,
		user_data,
		GPGME_PROTOCOL_OpenPGP,
		get_canonical_content);
}

static gboolean pgpmime_is_encrypted(MimeInfo *mimeinfo)
{
	MimeInfo *tmpinfo;
	const gchar *tmpstr;
	const gchar *begin_indicator = "-----BEGIN PGP MESSAGE-----";
	const gchar *end_indicator = "-----END PGP MESSAGE-----";
	gchar *textdata;

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
	
	textdata = procmime_get_part_as_string(tmpinfo, TRUE);
	if (!textdata)
		return FALSE;
	
	if (!pgp_locate_armor_header(textdata, begin_indicator)) {
		g_free(textdata);
		return FALSE;
	}
	if (!pgp_locate_armor_header(textdata, end_indicator)) {
		g_free(textdata);
		return FALSE;
	}

	g_free(textdata);

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
	gpgme_error_t err;
	SignatureData *sig_data = NULL;

	if ((err = gpgme_new(&ctx)) != GPG_ERR_NO_ERROR) {
		debug_print(("Couldn't initialize GPG context, %s\n"), gpgme_strerror(err));
		privacy_set_error(_("Couldn't initialize GPG context, %s"), gpgme_strerror(err));
		return NULL;
	}
	
	cm_return_val_if_fail(pgpmime_is_encrypted(mimeinfo), NULL);
	
	encinfo = (MimeInfo *) g_node_nth_child(mimeinfo->node, 1)->data;

	cipher = sgpgme_data_from_mimeinfo(encinfo);
	plain = sgpgme_decrypt_verify(cipher, &sigstat, ctx);

	if (sigstat != NULL && sigstat->signatures != NULL) {
		sig_data = g_new0(SignatureData, 1);
		sig_data->status = sgpgme_sigstat_gpgme_to_privacy(ctx, sigstat);
		sig_data->info_short = sgpgme_sigstat_info_short(ctx, sigstat);
		sig_data->info_full = sgpgme_sigstat_info_full(ctx, sigstat);
	}

	gpgme_release(ctx);
	gpgme_data_release(cipher);
	if (plain == NULL) {
		debug_print("plain is null!\n");
		if (sig_data)
			privacy_free_signature_data(sig_data);
		return NULL;
	}

	fname = g_strdup_printf("%s%cplaintext.%08x",
		get_mime_tmp_dir(), G_DIR_SEPARATOR, ++id);

	if ((dstfp = claws_fopen(fname, "wb")) == NULL) {
		FILE_OP_ERROR(fname, "claws_fopen");
		privacy_set_error(_("Couldn't open decrypted file %s"), fname);
		if (sig_data)
			privacy_free_signature_data(sig_data);
		g_free(fname);
		gpgme_data_release(plain);
		debug_print("can't open!\n");
		return NULL;
	}

	if (fprintf(dstfp, "MIME-Version: 1.0\n") < 0) {
		FILE_OP_ERROR(fname, "fprintf");
		claws_fclose(dstfp);
		privacy_set_error(_("Couldn't write to decrypted file %s"), fname);
		if (sig_data)
			privacy_free_signature_data(sig_data);
		g_free(fname);
		gpgme_data_release(plain);
		debug_print("can't open!\n");
		return NULL;
	}

	chars = sgpgme_data_release_and_get_mem(plain, &len);
	if (len > 0) {
		if (claws_fwrite(chars, 1, len, dstfp) < len) {
			FILE_OP_ERROR(fname, "claws_fwrite");
			g_free(chars);
			claws_fclose(dstfp);
			privacy_set_error(_("Couldn't write to decrypted file %s"), fname);
			if (sig_data)
				privacy_free_signature_data(sig_data);
			g_free(fname);
			gpgme_data_release(plain);
			debug_print("can't open!\n");
			return NULL;
		}
	}
	g_free(chars);

	if (claws_safe_fclose(dstfp) == EOF) {
		FILE_OP_ERROR(fname, "claws_fclose");
		privacy_set_error(_("Couldn't close decrypted file %s"), fname);
		if (sig_data)
			privacy_free_signature_data(sig_data);
		g_free(fname);
		gpgme_data_release(plain);
		debug_print("can't open!\n");
		return NULL;
	}

	parseinfo = procmime_scan_file(fname);
	g_free(fname);
	if (parseinfo == NULL) {
		privacy_set_error(_("Couldn't parse decrypted file."));
		if (sig_data)
			privacy_free_signature_data(sig_data);
		return NULL;
	}
	decinfo = g_node_first_child(parseinfo->node) != NULL ?
		g_node_first_child(parseinfo->node)->data : NULL;
	if (decinfo == NULL) {
		privacy_set_error(_("Couldn't parse decrypted file parts."));
		if (sig_data)
			privacy_free_signature_data(sig_data);
		return NULL;
	}

	g_node_unlink(decinfo->node);
	procmime_mimeinfo_free_all(&parseinfo);

	decinfo->tmp = TRUE;

	if (sig_data != NULL) {
		if (decinfo->privacy != NULL) {
			data = (PrivacyDataPGP *) decinfo->privacy;
		} else {
			data = pgpmime_new_privacydata();
			decinfo->privacy = (PrivacyData *) data;
		}

		if (data != NULL) {
			data->done_sigtest = TRUE;
			data->is_signed = TRUE;
			decinfo->sig_data = sig_data;
		}
	}

	return decinfo;
}

gboolean pgpmime_sign(MimeInfo *mimeinfo, PrefsAccount *account, const gchar *from_addr)
{
	MimeInfo *msgcontent, *sigmultipart, *newinfo;
	gchar *textstr, *micalg = NULL;
	FILE *fp;
	gchar *boundary = NULL;
	gchar *sigcontent;
	gpgme_ctx_t ctx;
	gpgme_data_t gpgtext, gpgsig;
	gpgme_error_t err;
	size_t len;
	struct passphrase_cb_info_s info;
	gpgme_sign_result_t result = NULL;
	gchar *test_msg;
	
	fp = my_tmpfile();
	if (fp == NULL) {
		perror("my_tmpfile");
		privacy_set_error(_("Couldn't create temporary file: %s"), g_strerror(errno));
		return FALSE;
	}
	procmime_write_mimeinfo(mimeinfo, fp);
	rewind(fp);

	/* read temporary file into memory */
	test_msg = file_read_stream_to_str(fp);
	claws_fclose(fp);
	
	memset (&info, 0, sizeof info);

	/* remove content node from message */
	msgcontent = (MimeInfo *) mimeinfo->node->children->data;
	g_node_unlink(msgcontent->node);

	/* create temporary multipart for content */
	sigmultipart = procmime_mimeinfo_new();
	sigmultipart->type = MIMETYPE_MULTIPART;
	sigmultipart->subtype = g_strdup("signed");
	
	do {
		g_free(boundary);
		boundary = generate_mime_boundary("Sig");
	} while (strstr(test_msg, boundary) != NULL);
	
	g_free(test_msg);

	g_hash_table_insert(sigmultipart->typeparameters, g_strdup("boundary"),
                            g_strdup(boundary));
	g_hash_table_insert(sigmultipart->typeparameters, g_strdup("protocol"),
                            g_strdup("application/pgp-signature"));
	g_node_append(sigmultipart->node, msgcontent->node);
	g_node_append(mimeinfo->node, sigmultipart->node);

	/* write message content to temporary file */
	fp = my_tmpfile();
	if (fp == NULL) {
		perror("my_tmpfile");
		privacy_set_error(_("Couldn't create temporary file: %s"), g_strerror(errno));
		g_free(boundary);
		return FALSE;
	}
	procmime_write_mimeinfo(sigmultipart, fp);
	rewind(fp);

	/* read temporary file into memory */
	textstr = get_canonical_content(fp, boundary);

	g_free(boundary);
	claws_fclose(fp);

	gpgme_data_new_from_mem(&gpgtext, textstr, (size_t)strlen(textstr), 0);
	gpgme_data_new(&gpgsig);
	if ((err = gpgme_new(&ctx)) != GPG_ERR_NO_ERROR) {
		debug_print(("Couldn't initialize GPG context, %s\n"), gpgme_strerror(err));
		privacy_set_error(_("Couldn't initialize GPG context, %s"), gpgme_strerror(err));
		return FALSE;
	}
	gpgme_set_textmode(ctx, 1);
	gpgme_set_armor(ctx, 1);
	gpgme_signers_clear (ctx);

	if (!sgpgme_setup_signers(ctx, account, from_addr)) {
		gpgme_release(ctx);
		return FALSE;
	}

	prefs_gpg_enable_agent(prefs_gpg_get_config()->use_gpg_agent);
	if (g_getenv("GPG_AGENT_INFO") && prefs_gpg_get_config()->use_gpg_agent) {
		debug_print("GPG_AGENT_INFO environment defined, running without passphrase callback\n");
	} else {
   		info.c = ctx;
    		gpgme_set_passphrase_cb (ctx, gpgmegtk_passphrase_cb, &info);
	}

	err = gpgme_op_sign(ctx, gpgtext, gpgsig, GPGME_SIG_MODE_DETACH);
	if (err != GPG_ERR_NO_ERROR) {
		if (err == GPG_ERR_CANCELED) {
			/* ignore cancelled signing */
			privacy_reset_error();
			debug_print("gpgme_op_sign cancelled\n");
		} else {
			privacy_set_error(_("Data signing failed, %s"), gpgme_strerror(err));
			debug_print("gpgme_op_sign error : %x\n", err);
		}
		gpgme_release(ctx);
		return FALSE;
	}
	result = gpgme_op_sign_result(ctx);
	if (result && result->signatures) {
		gpgme_new_signature_t sig = result->signatures;
		if (gpgme_get_protocol(ctx) == GPGME_PROTOCOL_OpenPGP) {
			gchar *down_algo = g_ascii_strdown(gpgme_hash_algo_name(
				result->signatures->hash_algo), -1);
			micalg = g_strdup_printf("pgp-%s", down_algo);
			g_free(down_algo);
		} else {
			micalg = g_strdup(gpgme_hash_algo_name(
				result->signatures->hash_algo));
		}
		while (sig) {
			debug_print("valid signature: %s\n", sig->fpr);
			sig = sig->next;
		}
	} else if (result && result->invalid_signers) {
		gpgme_invalid_key_t invalid = result->invalid_signers;
		while (invalid) {
			g_warning("invalid signer: %s (%s)", invalid->fpr, 
				gpgme_strerror(invalid->reason));
			privacy_set_error(_("Data signing failed due to invalid signer: %s"), 
				gpgme_strerror(invalid->reason));
			invalid = invalid->next;
		}
		gpgme_release(ctx);
		return FALSE;
	} else {
		/* can't get result (maybe no signing key?) */
		debug_print("gpgme_op_sign_result error\n");
		privacy_set_error(_("Data signing failed, no results."));
		gpgme_release(ctx);
		return FALSE;
	}

	sigcontent = sgpgme_data_release_and_get_mem(gpgsig, &len);
	gpgme_data_release(gpgtext);
	g_free(textstr);

	if (sigcontent == NULL || len <= 0) {
		g_warning("sgpgme_data_release_and_get_mem failed");
		privacy_set_error(_("Data signing failed, no contents."));
		g_free(micalg);
		g_free(sigcontent);
		gpgme_release(ctx);
		return FALSE;
	}

	/* add signature */
	g_hash_table_insert(sigmultipart->typeparameters, g_strdup("micalg"),
                            micalg);

	newinfo = procmime_mimeinfo_new();
	newinfo->type = MIMETYPE_APPLICATION;
	newinfo->subtype = g_strdup("pgp-signature");
	newinfo->description = g_strdup(_("OpenPGP digital signature"));
	newinfo->content = MIMECONTENT_MEM;
	newinfo->data.mem = g_malloc(len + 1);
	memmove(newinfo->data.mem, sigcontent, len);
	newinfo->data.mem[len] = '\0';
	newinfo->tmp = TRUE;
	g_node_append(sigmultipart->node, newinfo->node);

	g_free(sigcontent);
	gpgme_release(ctx);

	return TRUE;
}
gchar *pgpmime_get_encrypt_data(GSList *recp_names)
{
	return sgpgme_get_encrypt_data(recp_names, GPGME_PROTOCOL_OpenPGP);
}

static const gchar *pgpmime_get_encrypt_warning(void)
{
	if (prefs_gpg_should_skip_encryption_warning(pgpmime_system.id))
		return NULL;
	else
		return _("Please note that email headers, like Subject, "
			 "are not encrypted by the PGP/Mime system.");
}

static void pgpmime_inhibit_encrypt_warning(gboolean inhibit)
{
	if (inhibit)
		prefs_gpg_add_skip_encryption_warning(pgpmime_system.id);
	else
		prefs_gpg_remove_skip_encryption_warning(pgpmime_system.id);
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
	gpgme_error_t err;
	
	while (fprs[i] && strlen(fprs[i])) {
		i++;
	}
	
	kset = g_malloc0(sizeof(gpgme_key_t)*(i+1));
	if ((err = gpgme_new(&ctx)) != GPG_ERR_NO_ERROR) {
		debug_print(("Couldn't initialize GPG context, %s\n"), gpgme_strerror(err));
		privacy_set_error(_("Couldn't initialize GPG context, %s"), gpgme_strerror(err));
		g_free(kset);
		g_free(fprs);
		return FALSE;
	}
	i = 0;
	while (fprs[i] && strlen(fprs[i])) {
		gpgme_key_t key;
		err = gpgme_get_key(ctx, fprs[i], &key, 0);
		if (err) {
			debug_print("can't add key '%s'[%d] (%s)\n", fprs[i],i, gpgme_strerror(err));
			privacy_set_error(_("Couldn't add GPG key %s, %s"), fprs[i], gpgme_strerror(err));
			for (gint x = 0; x < i; x++)
				gpgme_key_unref(kset[x]);
			g_free(kset);
			g_free(fprs);
			gpgme_release(ctx);
			return FALSE;
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
	if (fp == NULL) {
		perror("my_tmpfile");
		privacy_set_error(_("Couldn't create temporary file, %s"), g_strerror(errno));
		for (gint x = 0; x < i; x++)
			gpgme_key_unref(kset[x]);
		g_free(kset);
		g_free(boundary);
		g_free(fprs);
		gpgme_release(ctx);
		return FALSE;
	}
	procmime_write_mimeinfo(encmultipart, fp);
	rewind(fp);

	/* read temporary file into memory */
	textstr = get_canonical_content(fp, boundary);

	g_free(boundary);
	claws_fclose(fp);

	/* encrypt data */
	gpgme_data_new_from_mem(&gpgtext, textstr, (size_t)strlen(textstr), 0);
	gpgme_data_new(&gpgenc);
	gpgme_set_armor(ctx, 1);
	cm_gpgme_data_rewind(gpgtext);
	
	err = gpgme_op_encrypt(ctx, kset, GPGME_ENCRYPT_ALWAYS_TRUST, gpgtext, gpgenc);

	enccontent = sgpgme_data_release_and_get_mem(gpgenc, &len);
	gpgme_data_release(gpgtext);
	g_free(textstr);
	for (gint x = 0; x < i; x++)
		gpgme_key_unref(kset[x]);
	g_free(kset);

	if (enccontent == NULL || len <= 0) {
		g_warning("sgpgme_data_release_and_get_mem failed");
		privacy_set_error(_("Encryption failed, %s"), gpgme_strerror(err));
		gpgme_release(ctx);
		g_free(enccontent);
		g_free(fprs);
		return FALSE;
	}

	/* create encrypted multipart */
	g_node_unlink(msgcontent->node);
	procmime_mimeinfo_free_all(&msgcontent);
	g_node_append(mimeinfo->node, encmultipart->node);

	newinfo = procmime_mimeinfo_new();
	newinfo->type = MIMETYPE_APPLICATION;
	newinfo->subtype = g_strdup("pgp-encrypted");
	newinfo->content = MIMECONTENT_MEM;
	newinfo->data.mem = g_strdup("Version: 1\n");
	newinfo->tmp = TRUE;
	g_node_append(encmultipart->node, newinfo->node);

	newinfo = procmime_mimeinfo_new();
	newinfo->type = MIMETYPE_APPLICATION;
	newinfo->subtype = g_strdup("octet-stream");
	newinfo->content = MIMECONTENT_MEM;
	newinfo->data.mem = g_malloc(len + 1);
	newinfo->tmp = TRUE;
	memmove(newinfo->data.mem, enccontent, len);
	newinfo->data.mem[len] = '\0';
	g_node_append(encmultipart->node, newinfo->node);

	g_free(enccontent);
	gpgme_release(ctx);

	g_free(fprs);

	return TRUE;
}

static PrivacySystem pgpmime_system = {
	"pgpmime",			/* id */
	"PGP MIME",			/* name */

	pgpmime_free_privacydata,	/* free_privacydata */

	pgpmime_is_signed,		/* is_signed(MimeInfo *) */
	pgpmime_check_sig_async,

	pgpmime_is_encrypted,		/* is_encrypted(MimeInfo *) */
	pgpmime_decrypt,		/* decrypt(MimeInfo *) */

	TRUE,
	pgpmime_sign,

	TRUE,
	pgpmime_get_encrypt_data,
	pgpmime_encrypt,
	pgpmime_get_encrypt_warning,
	pgpmime_inhibit_encrypt_warning,
	prefs_gpg_auto_check_signatures,
};

void pgpmime_init()
{
	privacy_register_system(&pgpmime_system);
}

void pgpmime_done()
{
	privacy_unregister_system(&pgpmime_system);
}

struct PluginFeature *plugin_provides(void)
{
	static struct PluginFeature features[] = 
		{ {PLUGIN_PRIVACY, N_("PGP/Mime")},
		  {PLUGIN_NOTHING, NULL}};
	return features;
}
#endif /* USE_GPGME */
