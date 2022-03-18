/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2021 Colin Leroy and the Claws Mail team
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
#include <errno.h>
#include <gpgme.h>
#include <file-utils.h>

#include "utils.h"
#include "privacy.h"
#include "procmime.h"
#include "pgpinline.h"
#include <plugins/pgpcore/sgpgme.h>
#include <plugins/pgpcore/prefs_gpg.h>
#include <plugins/pgpcore/passphrase.h>
#include <plugins/pgpcore/pgp_utils.h>
#include "quoted-printable.h"
#include "codeconv.h"
#include "plugin.h"

extern struct GPGConfig prefs_gpg;

typedef struct _PrivacyDataPGP PrivacyDataPGP;

struct _PrivacyDataPGP
{
	PrivacyData	data;
	
	gboolean	done_sigtest;
	gboolean	is_signed;
};

typedef struct _PGPInlineTaskData
{
	gchar *rawtext;
	gchar *charset;
} PGPInlineTaskData;

static PrivacySystem pgpinline_system;

static PrivacyDataPGP *pgpinline_new_privacydata()
{
	PrivacyDataPGP *data;

	data = g_new0(PrivacyDataPGP, 1);
	data->data.system = &pgpinline_system;
	
	return data;
}

static void pgpinline_free_privacydata(PrivacyData *data)
{
	g_free(data);
}

static gboolean pgpinline_is_signed(MimeInfo *mimeinfo)
{
	PrivacyDataPGP *data = NULL;
	const gchar *sig_indicator = "-----BEGIN PGP SIGNED MESSAGE-----";
	gchar *textdata, *sigpos;
	
	cm_return_val_if_fail(mimeinfo != NULL, FALSE);
	
	if (procmime_mimeinfo_parent(mimeinfo) == NULL)
		return FALSE; /* not parent */
	
	if (mimeinfo->type != MIMETYPE_TEXT &&
		(mimeinfo->type != MIMETYPE_APPLICATION ||
		 g_ascii_strcasecmp(mimeinfo->subtype, "pgp")))
		return FALSE;

	/* Seal the deal. This has to be text/plain through and through. */
	if (mimeinfo->type == MIMETYPE_APPLICATION)
	{
		mimeinfo->type = MIMETYPE_TEXT;
		g_free(mimeinfo->subtype);
		mimeinfo->subtype = g_strdup("plain");
	}

	if (mimeinfo->privacy != NULL) {
		data = (PrivacyDataPGP *) mimeinfo->privacy;
		if (data->done_sigtest)
			return data->is_signed;
	}
	
	textdata = procmime_get_part_as_string(mimeinfo, TRUE);
	if (!textdata)
		return FALSE;
	
	if ((sigpos = strstr(textdata, sig_indicator)) == NULL) {
		g_free(textdata);
		return FALSE;
	}

	if (!(sigpos == textdata) && !(sigpos[-1] == '\n')) {
		g_free(textdata);
		return FALSE;
	}

	g_free(textdata);

	if (data == NULL) {
		data = pgpinline_new_privacydata();
		mimeinfo->privacy = (PrivacyData *) data;
	}
	if (data != NULL) {
		data->done_sigtest = TRUE;
		data->is_signed = TRUE;
	}

	return TRUE;
}

static void pgpinline_free_task_data(gpointer data)
{
	PGPInlineTaskData *task_data = (PGPInlineTaskData *)data;

	g_free(task_data->rawtext);
	g_free(task_data->charset);
	g_free(task_data);
}

static gchar *get_sig_data(gchar *rawtext, gchar *charset)
{
	gchar *conv;

	conv = conv_codeset_strdup(rawtext, CS_UTF_8, charset);
	if (!conv)
		conv = conv_codeset_strdup(rawtext, CS_UTF_8, conv_get_locale_charset_str_no_utf8());

	if (!conv) {
		g_warning("can't convert charset to anything sane");
		conv = conv_codeset_strdup(rawtext, CS_UTF_8, CS_US_ASCII);
	}

	return conv;
}

static void pgpinline_check_sig_task(GTask *task,
	gpointer source_object,
	gpointer g_task_data,
	GCancellable *cancellable)
{
	PGPInlineTaskData *task_data = (PGPInlineTaskData *)g_task_data;
	GQuark domain;
	gpgme_ctx_t ctx;
	gpgme_error_t err;
	gpgme_data_t sigdata = NULL;
	gpgme_data_t plain = NULL;
	gpgme_verify_result_t gpgme_res;
	gboolean return_err = TRUE;
	gboolean cancelled = FALSE;
	SigCheckTaskResult *task_result = NULL;
	gchar *textstr;
	char err_str[GPGERR_BUFSIZE] = "";

	domain = g_quark_from_static_string("claws_pgpinline");

	err = gpgme_new(&ctx);
	if (err != GPG_ERR_NO_ERROR) {
		gpgme_strerror_r(err, err_str, GPGERR_BUFSIZE);
		g_warning("couldn't initialize GPG context: %s", err_str);
		goto out;
	}

	gpgme_set_textmode(ctx, 1);
	gpgme_set_armor(ctx, 1);

	textstr = get_sig_data(task_data->rawtext, task_data->charset);
	if (!textstr) {
		err = GPG_ERR_GENERAL;
		g_snprintf(err_str, GPGERR_BUFSIZE, "Couldn't convert text data to any sane charset.");
		goto out_ctx;
	}

	err = gpgme_data_new_from_mem(&sigdata, textstr, strlen(textstr), 1);
	if (err != GPG_ERR_NO_ERROR) {
		gpgme_strerror_r(err, err_str, GPGERR_BUFSIZE);
		g_warning("gpgme_data_new_from_mem failed: %s", err_str);
		goto out_textstr;
	}

	err = gpgme_data_new(&plain);
	if (err != GPG_ERR_NO_ERROR) {
		gpgme_strerror_r(err, err_str, GPGERR_BUFSIZE);
		g_warning("gpgme_data_new failed: %s", err_str);
		goto out_sigdata;
	}

	if (g_task_return_error_if_cancelled(task)) {
		debug_print("task was cancelled, aborting task:%p\n", task);
		cancelled = TRUE;
		goto out_sigdata;
	}

	err = gpgme_op_verify(ctx, sigdata, NULL, plain);
	if (err != GPG_ERR_NO_ERROR) {
		gpgme_strerror_r(err, err_str, GPGERR_BUFSIZE);
		g_warning("gpgme_op_verify failed: %s\n", err_str);
		goto out_plain;
	}

	if (g_task_return_error_if_cancelled(task)) {
		debug_print("task was cancelled, aborting task:%p\n", task);
		cancelled = TRUE;
		goto out_sigdata;
	}

	gpgme_res = gpgme_op_verify_result(ctx);
	if (gpgme_res && gpgme_res->signatures == NULL) {
		err = GPG_ERR_SYSTEM_ERROR;
		g_warning("no signature found");
		g_snprintf(err_str, GPGERR_BUFSIZE, "No signature found");
		goto out_plain;
	}

	task_result = g_new0(SigCheckTaskResult, 1);
	task_result->sig_data = g_new0(SignatureData, 1);

	task_result->sig_data->status = sgpgme_sigstat_gpgme_to_privacy(ctx, gpgme_res);
	task_result->sig_data->info_short = sgpgme_sigstat_info_short(ctx, gpgme_res);
	task_result->sig_data->info_full = sgpgme_sigstat_info_full(ctx, gpgme_res);

	return_err = FALSE;

out_plain:
	gpgme_data_release(plain);
out_sigdata:
	gpgme_data_release(sigdata);
out_textstr:
	g_free(textstr);
out_ctx:
	gpgme_release(ctx);
out:
	if (cancelled)
		return;

	if (return_err)
		g_task_return_new_error(task, domain, err, "%s", err_str);
	else
		g_task_return_pointer(task, task_result, privacy_free_sig_check_task_result);
}

static gint pgpinline_check_sig_async(MimeInfo *mimeinfo,
	GCancellable *cancellable,
	GAsyncReadyCallback callback,
	gpointer user_data)
{
	GTask *task;
	PGPInlineTaskData *task_data;
	gchar *rawtext;
	const gchar *charset;

	if (procmime_mimeinfo_parent(mimeinfo) == NULL) {
		g_warning("Checking signature on incorrect part");
		return -1;
	}

	if (mimeinfo->type != MIMETYPE_TEXT) {
		g_warning("Checking signature on a non-text part");
		return -1;
	}

	rawtext = procmime_get_part_as_string(mimeinfo, TRUE);
	if (rawtext == NULL) {
		g_warning("Failed to get part as string");
		return -1;
	}

	charset = procmime_mimeinfo_get_parameter(mimeinfo, "charset");

	task_data = g_new0(PGPInlineTaskData, 1);
	task_data->rawtext = rawtext;
	task_data->charset = g_strdup(charset);

	task = g_task_new(NULL, cancellable, callback, user_data);
	mimeinfo->last_sig_check_task = task;

	g_task_set_task_data(task, task_data, pgpinline_free_task_data);
	debug_print("creating check sig async task:%p task_data:%p\n", task, task_data);
	g_task_set_return_on_cancel(task, TRUE);
	g_task_run_in_thread(task, pgpinline_check_sig_task);
	g_object_unref(task);

	return 0;
}

static gboolean pgpinline_is_encrypted(MimeInfo *mimeinfo)
{
	const gchar *begin_indicator = "-----BEGIN PGP MESSAGE-----";
	const gchar *end_indicator = "-----END PGP MESSAGE-----";
	gchar *textdata;
	
	cm_return_val_if_fail(mimeinfo != NULL, FALSE);
	
	if (procmime_mimeinfo_parent(mimeinfo) == NULL)
		return FALSE; /* not parent */
	
	if (mimeinfo->type != MIMETYPE_TEXT &&
		(mimeinfo->type != MIMETYPE_APPLICATION ||
		 g_ascii_strcasecmp(mimeinfo->subtype, "pgp")))
		return FALSE;
	
	/* Seal the deal. This has to be text/plain through and through. */
	if (mimeinfo->type == MIMETYPE_APPLICATION)
	{
		mimeinfo->type = MIMETYPE_TEXT;
		g_free(mimeinfo->subtype);
		mimeinfo->subtype = g_strdup("plain");
	}

	textdata = procmime_get_part_as_string(mimeinfo, TRUE);
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

static MimeInfo *pgpinline_decrypt(MimeInfo *mimeinfo)
{
	MimeInfo *decinfo, *parseinfo;
	gpgme_data_t cipher, plain;
	FILE *dstfp;
	gchar *fname;
	gchar *textdata = NULL;
	static gint id = 0;
	const gchar *src_codeset = NULL;
	gpgme_verify_result_t sigstat = 0;
	PrivacyDataPGP *data = NULL;
	gpgme_ctx_t ctx;
	gchar *chars;
	size_t len;
	const gchar *begin_indicator = "-----BEGIN PGP MESSAGE-----";
	const gchar *end_indicator = "-----END PGP MESSAGE-----";
	gchar *pos;
	SignatureData *sig_data = NULL;
	
	if (gpgme_new(&ctx) != GPG_ERR_NO_ERROR)
		return NULL;

	gpgme_set_textmode(ctx, 1);
	gpgme_set_armor(ctx, 1);

	cm_return_val_if_fail(mimeinfo != NULL, NULL);
	cm_return_val_if_fail(pgpinline_is_encrypted(mimeinfo), NULL);
	
	if (procmime_mimeinfo_parent(mimeinfo) == NULL ||
	    mimeinfo->type != MIMETYPE_TEXT) {
		gpgme_release(ctx);
		privacy_set_error(_("Couldn't parse mime part."));
		return NULL;
	}

	textdata = procmime_get_part_as_string(mimeinfo, TRUE);
	if (!textdata) {
		gpgme_release(ctx);
		privacy_set_error(_("Couldn't get text data."));
		return NULL;
	}

	debug_print("decrypting '%s'\n", textdata);
	gpgme_data_new_from_mem(&cipher, textdata, (size_t)strlen(textdata), 1);

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
		g_free(textdata);
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
		g_free(textdata);
		g_free(fname);
		gpgme_data_release(plain);
		return NULL;
	}

	src_codeset = procmime_mimeinfo_get_parameter(mimeinfo, "charset");
	if (src_codeset == NULL)
		src_codeset = CS_ISO_8859_1;
		
	if (fprintf(dstfp, "MIME-Version: 1.0\r\n"
			"Content-Type: text/plain; charset=%s\r\n"
			"Content-Transfer-Encoding: 8bit\r\n"
			"\r\n",
			src_codeset) < 0) {
		FILE_OP_ERROR(fname, "fprintf");
		privacy_set_error(_("Couldn't write to decrypted file %s"), fname);
		goto FILE_ERROR;
	}

	/* Store any part before encrypted text */
	pos = pgp_locate_armor_header(textdata, begin_indicator);
	if (pos != NULL && (pos - textdata) > 0) {
		if (claws_fwrite(textdata, 1, pos - textdata, dstfp) < pos - textdata) {
			FILE_OP_ERROR(fname, "claws_fwrite");
			privacy_set_error(_("Couldn't write to decrypted file %s"), fname);
			goto FILE_ERROR;
		}
	}

	if (claws_fwrite(_("\n--- Start of PGP/Inline encrypted data ---\n"), 1,
		strlen(_("\n--- Start of PGP/Inline encrypted data ---\n")), 
		dstfp) < strlen(_("\n--- Start of PGP/Inline encrypted data ---\n"))) {
        	FILE_OP_ERROR(fname, "claws_fwrite");
		privacy_set_error(_("Couldn't write to decrypted file %s"), fname);
		goto FILE_ERROR;
	}
	chars = sgpgme_data_release_and_get_mem(plain, &len);
	if (len > 0) {
		if (claws_fwrite(chars, 1, len, dstfp) < len) {
        		FILE_OP_ERROR(fname, "claws_fwrite");
			g_free(chars);
			privacy_set_error(_("Couldn't write to decrypted file %s"), fname);
			goto FILE_ERROR;
		}
	}
	g_free(chars);
	/* Store any part after encrypted text */
	if (claws_fwrite(_("--- End of PGP/Inline encrypted data ---\n"), 1,
		strlen(_("--- End of PGP/Inline encrypted data ---\n")), 
		dstfp) < strlen(_("--- End of PGP/Inline encrypted data ---\n"))) {
        		FILE_OP_ERROR(fname, "claws_fwrite");
			privacy_set_error(_("Couldn't write to decrypted file %s"), fname);
			goto FILE_ERROR;
	}
	if (pos != NULL) {
	    pos = pgp_locate_armor_header(pos, end_indicator);
	    if (pos != NULL && *pos != '\0') {
		pos += strlen(end_indicator);
		if (claws_fwrite(pos, 1, strlen(pos), dstfp) < strlen(pos)) {
        		FILE_OP_ERROR(fname, "claws_fwrite");
			privacy_set_error(_("Couldn't write to decrypted file %s"), fname);
			goto FILE_ERROR;
		}
	    }
	}

	g_free(textdata);

	if (claws_safe_fclose(dstfp) == EOF) {
        	FILE_OP_ERROR(fname, "claws_fclose");
		privacy_set_error(_("Couldn't close decrypted file %s"), fname);
        	g_free(fname);
        	gpgme_data_release(plain);
		if (sig_data)
			privacy_free_signature_data(sig_data);
		return NULL;
	}
	
	parseinfo = procmime_scan_file(fname);
	g_free(fname);
	
	if (parseinfo == NULL) {
		privacy_set_error(_("Couldn't scan decrypted file."));
		if (sig_data)
			privacy_free_signature_data(sig_data);
		return NULL;
	}
	decinfo = g_node_first_child(parseinfo->node) != NULL ?
		g_node_first_child(parseinfo->node)->data : NULL;
		
	if (decinfo == NULL) {
		privacy_set_error(_("Couldn't scan decrypted file parts."));
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
			data = pgpinline_new_privacydata();
			decinfo->privacy = (PrivacyData *) data;	
		}
		if (data != NULL) {
			data->done_sigtest = TRUE;
			data->is_signed = TRUE;
			decinfo->sig_data = sig_data;
		}
	}

	return decinfo;

FILE_ERROR:
	if (sig_data)
		privacy_free_signature_data(sig_data);
	g_free(textdata);
	claws_fclose(dstfp);
	g_free(fname);
	gpgme_data_release(plain);
	return NULL;
}

static gboolean pgpinline_sign(MimeInfo *mimeinfo, PrefsAccount *account, const gchar *from_addr)
{
	MimeInfo *msgcontent;
	gchar *textstr, *tmp;
	FILE *fp;
	gchar *sigcontent;
	gpgme_ctx_t ctx;
	gpgme_data_t gpgtext, gpgsig;
	size_t len;
	gpgme_error_t err;
	struct passphrase_cb_info_s info;
	gpgme_sign_result_t result = NULL;

	memset (&info, 0, sizeof info);

	/* get content node from message */
	msgcontent = (MimeInfo *) mimeinfo->node->children->data;
	if (msgcontent->type == MIMETYPE_MULTIPART) {
		if (!msgcontent->node->children) {
			debug_print("msgcontent->node->children NULL, bailing\n");
			privacy_set_error(_("Malformed message"));
			return FALSE;
		}
		msgcontent = (MimeInfo *) msgcontent->node->children->data;
	}
	/* get rid of quoted-printable or anything */
	procmime_decode_content(msgcontent);

	fp = my_tmpfile();
	if (fp == NULL) {
		perror("my_tmpfile");
		privacy_set_error(_("Couldn't create temporary file, %s"), g_strerror(errno));
		return FALSE;
	}
	procmime_write_mimeinfo(msgcontent, fp);
	rewind(fp);

	/* read temporary file into memory */
	textstr = file_read_stream_to_str_no_recode(fp);
	
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

	if (!sgpgme_setup_signers(ctx, account, from_addr)) {
		gpgme_release(ctx);
		return FALSE;
	}

	prefs_gpg_enable_agent(prefs_gpg_get_config()->use_gpg_agent);
	if (!g_getenv("GPG_AGENT_INFO") || !prefs_gpg_get_config()->use_gpg_agent) {
    		info.c = ctx;
    		gpgme_set_passphrase_cb (ctx, gpgmegtk_passphrase_cb, &info);
	}

	err = gpgme_op_sign(ctx, gpgtext, gpgsig, GPGME_SIG_MODE_CLEAR);
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
	
	if (sigcontent == NULL || len <= 0) {
		g_warning("sgpgme_data_release_and_get_mem failed");
		privacy_set_error(_("Data signing failed, no contents."));
		gpgme_data_release(gpgtext);
		g_free(textstr);
		g_free(sigcontent);
		gpgme_release(ctx);
		return FALSE;
	}

	tmp = g_malloc(len+1);
	memmove(tmp, sigcontent, len+1);
	tmp[len] = '\0';
	gpgme_data_release(gpgtext);
	g_free(textstr);
	g_free(sigcontent);

	if (msgcontent->content == MIMECONTENT_FILE &&
	    msgcontent->data.filename != NULL) {
	    	if (msgcontent->tmp == TRUE)
			claws_unlink(msgcontent->data.filename);
		g_free(msgcontent->data.filename);
	}
	msgcontent->data.mem = g_strdup(tmp);
	msgcontent->content = MIMECONTENT_MEM;
	g_free(tmp);

	/* avoid all sorts of clear-signing problems with non ascii
	 * chars
	 */
	procmime_encode_content(msgcontent, ENC_BASE64);
	gpgme_release(ctx);

	return TRUE;
}

static gchar *pgpinline_get_encrypt_data(GSList *recp_names)
{
	return sgpgme_get_encrypt_data(recp_names, GPGME_PROTOCOL_OpenPGP);
}

static const gchar *pgpinline_get_encrypt_warning(void)
{
	if (prefs_gpg_should_skip_encryption_warning(pgpinline_system.id))
		return NULL;
	else
		return _("Please note that attachments are not encrypted by "
		 "the PGP/Inline system, nor are email headers, like Subject.");
}

static void pgpinline_inhibit_encrypt_warning(gboolean inhibit)
{
	if (inhibit)
		prefs_gpg_add_skip_encryption_warning(pgpinline_system.id);
	else
		prefs_gpg_remove_skip_encryption_warning(pgpinline_system.id);
}

static gboolean pgpinline_encrypt(MimeInfo *mimeinfo, const gchar *encrypt_data)
{
	MimeInfo *msgcontent;
	FILE *fp;
	gchar *enccontent;
	size_t len;
	gchar *textstr, *tmp;
	gpgme_data_t gpgtext, gpgenc;
	gpgme_ctx_t ctx;
	gpgme_key_t *kset = NULL;
	gchar **fprs = g_strsplit(encrypt_data, " ", -1);
	gpgme_error_t err;
	gint i = 0;

	while (fprs[i] && strlen(fprs[i])) {
		i++;
	}
	
	kset = g_malloc(sizeof(gpgme_key_t)*(i+1));
	memset(kset, 0, sizeof(gpgme_key_t)*(i+1));
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
			return FALSE;
		}
		debug_print("found %s at %d\n", fprs[i], i);
		kset[i] = key;
		i++;
	}

	debug_print("Encrypting message content\n");

	/* get content node from message */
	msgcontent = (MimeInfo *) mimeinfo->node->children->data;
	if (msgcontent->type == MIMETYPE_MULTIPART) {
		if (!msgcontent->node->children) {
			debug_print("msgcontent->node->children NULL, bailing\n");
			privacy_set_error(_("Malformed message"));
			for (gint x = 0; x < i; x++)
				gpgme_key_unref(kset[x]);
			g_free(kset);
			g_free(fprs);
			return FALSE;
		}
		msgcontent = (MimeInfo *) msgcontent->node->children->data;
	}
	/* get rid of quoted-printable or anything */
	procmime_decode_content(msgcontent);

	fp = my_tmpfile();
	if (fp == NULL) {
		privacy_set_error(_("Couldn't create temporary file, %s"), g_strerror(errno));
		perror("my_tmpfile");
		for (gint x = 0; x < i; x++)
			gpgme_key_unref(kset[x]);
		g_free(kset);
		g_free(fprs);
		return FALSE;
	}
	procmime_write_mimeinfo(msgcontent, fp);
	rewind(fp);

	/* read temporary file into memory */
	textstr = file_read_stream_to_str_no_recode(fp);
	
	claws_fclose(fp);

	/* encrypt data */
	gpgme_data_new_from_mem(&gpgtext, textstr, (size_t)strlen(textstr), 0);
	gpgme_data_new(&gpgenc);
	if ((err = gpgme_new(&ctx)) != GPG_ERR_NO_ERROR) {
		debug_print(("Couldn't initialize GPG context, %s\n"), gpgme_strerror(err));
		privacy_set_error(_("Couldn't initialize GPG context, %s"), gpgme_strerror(err));
		for (gint x = 0; x < i; x++)
			gpgme_key_unref(kset[x]);
		g_free(kset);
		g_free(fprs);
		return FALSE;
	}
	gpgme_set_armor(ctx, 1);

	err = gpgme_op_encrypt(ctx, kset, GPGME_ENCRYPT_ALWAYS_TRUST, gpgtext, gpgenc);

	enccontent = sgpgme_data_release_and_get_mem(gpgenc, &len);
	for (gint x = 0; x < i; x++)
		gpgme_key_unref(kset[x]);
	g_free(kset);

	if (enccontent == NULL || len <= 0) {
		g_warning("sgpgme_data_release_and_get_mem failed");
		privacy_set_error(_("Encryption failed, %s"), gpgme_strerror(err));
		gpgme_data_release(gpgtext);
		g_free(textstr);
		gpgme_release(ctx);
		g_free(enccontent);
		g_free(fprs);
		return FALSE;
	}

	tmp = g_malloc(len+1);
	memmove(tmp, enccontent, len+1);
	tmp[len] = '\0';
	g_free(enccontent);

	gpgme_data_release(gpgtext);
	g_free(textstr);

	if (msgcontent->content == MIMECONTENT_FILE &&
	    msgcontent->data.filename != NULL) {
	    	if (msgcontent->tmp == TRUE)
			claws_unlink(msgcontent->data.filename);
		g_free(msgcontent->data.filename);
	}
	msgcontent->data.mem = g_strdup(tmp);
	msgcontent->content = MIMECONTENT_MEM;
	g_free(tmp);
	gpgme_release(ctx);

	g_free(fprs);

	return TRUE;
}

static PrivacySystem pgpinline_system = {
	"pgpinline",			/* id */
	"PGP Inline",			/* name */

	pgpinline_free_privacydata,	/* free_privacydata */

	pgpinline_is_signed,		/* is_signed(MimeInfo *) */
	pgpinline_check_sig_async,

	pgpinline_is_encrypted,		/* is_encrypted(MimeInfo *) */
	pgpinline_decrypt,		/* decrypt(MimeInfo *) */

	TRUE,
	pgpinline_sign,

	TRUE,
	pgpinline_get_encrypt_data,
	pgpinline_encrypt,
	pgpinline_get_encrypt_warning,
	pgpinline_inhibit_encrypt_warning,
	prefs_gpg_auto_check_signatures,
};

void pgpinline_init()
{
	privacy_register_system(&pgpinline_system);
}

void pgpinline_done()
{
	privacy_unregister_system(&pgpinline_system);
}

struct PluginFeature *plugin_provides(void)
{
	static struct PluginFeature features[] = 
		{ {PLUGIN_PRIVACY, N_("PGP/Inline")},
		  {PLUGIN_NOTHING, NULL}};
	return features;
}
#endif /* USE_GPGME */
