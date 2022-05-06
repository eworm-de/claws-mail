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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#ifdef USE_GPGME

#include "defs.h"
#include <glib.h>
#include <gpgme.h>
#include <ctype.h>
#include <glib/gi18n.h>

#include "utils.h"
#include "privacy.h"
#include "procmime.h"

#include "smime.h"
#include <plugins/pgpcore/sgpgme.h>
#include <plugins/pgpcore/prefs_gpg.h>
#include <plugins/pgpcore/pgp_utils.h>
#include <plugins/pgpcore/passphrase.h>

#include "alertpanel.h"
#include "prefs_common.h"
#include "plugin.h"
#include "file-utils.h"

typedef struct _PrivacyDataPGP PrivacyDataPGP;

struct _PrivacyDataPGP
{
	PrivacyData	data;
	
	gboolean	done_sigtest;
	gboolean	is_signed;
	gboolean	inserted_mimeinfo;
};

typedef struct _PKCS7MimeTaskData {
	gchar *textstr;
	EncodingType encoding;
	gboolean create_mimeinfo;
} PKCS7MimeTaskData;

static PrivacySystem smime_system;

static PrivacyDataPGP *smime_new_privacydata()
{
	PrivacyDataPGP *data;

	data = g_new0(PrivacyDataPGP, 1);
	data->data.system = &smime_system;
	
	return data;
}

static void smime_free_privacydata(PrivacyData *data)
{
	g_free(data);
}

static gint check_pkcs7_mime_sig(MimeInfo *, GCancellable *, GAsyncReadyCallback, gpointer);

static gboolean smime_is_signed(MimeInfo *mimeinfo)
{
	MimeInfo *parent;
	MimeInfo *signature;
	const gchar *protocol, *tmpstr;
	PrivacyDataPGP *data = NULL;
	
	cm_return_val_if_fail(mimeinfo != NULL, FALSE);
	if (mimeinfo->privacy != NULL) {
		data = (PrivacyDataPGP *) mimeinfo->privacy;
		if (data->done_sigtest)
			return data->is_signed;
	}
	
	if (!g_ascii_strcasecmp(mimeinfo->subtype, "pkcs7-mime") ||
	    !g_ascii_strcasecmp(mimeinfo->subtype, "x-pkcs7-mime")) {
		tmpstr = procmime_mimeinfo_get_parameter(mimeinfo, "smime-type");
		if (tmpstr && !g_ascii_strcasecmp(tmpstr, "signed-data")) {
			if (data == NULL) {
				data = smime_new_privacydata();
				if (!data)
					return FALSE;
				mimeinfo->privacy = (PrivacyData *) data;
			}

			data->done_sigtest = TRUE;
			data->is_signed = TRUE;
			check_pkcs7_mime_sig(mimeinfo, NULL, NULL, NULL);
			return TRUE;
		}
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
	    (g_ascii_strcasecmp(protocol, "application/pkcs7-signature") &&
	     g_ascii_strcasecmp(protocol, "application/x-pkcs7-signature")))
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
	    (g_ascii_strcasecmp(signature->subtype, "pkcs7-signature") &&
	     g_ascii_strcasecmp(signature->subtype, "x-pkcs7-signature")))
		return FALSE;

	if (data == NULL) {
		data = smime_new_privacydata();
		if (!data)
			return FALSE;
		mimeinfo->privacy = (PrivacyData *) data;
	}
	
	data->done_sigtest = TRUE;
	data->is_signed = TRUE;

	return TRUE;
}

static gchar *get_canonical_content(FILE *fp, const gchar *boundary)
{
	GString *textbuffer;
	guint boundary_len = 0;
	gchar buf[BUFFSIZE];

	if (boundary) {
		boundary_len = strlen(boundary);
		while (claws_fgets(buf, sizeof(buf), fp) != NULL)
			if (IS_BOUNDARY(buf, boundary, boundary_len))
				break;
	}
	
	textbuffer = g_string_new("");
	while (claws_fgets(buf, sizeof(buf), fp) != NULL) {
		gchar *buf2;

		if (boundary && IS_BOUNDARY(buf, boundary, boundary_len))
			break;
		
		buf2 = canonicalize_str(buf);
		g_string_append(textbuffer, buf2);
		g_free(buf2);
	}
	g_string_truncate(textbuffer, textbuffer->len - 2);
		
	return g_string_free(textbuffer, FALSE);
}

static void free_pkcs7_mime_task_data(gpointer data)
{
	PKCS7MimeTaskData *task_data = (PKCS7MimeTaskData *)data;

	g_free(task_data->textstr);
	g_free(task_data);
}

static gboolean create_mimeinfo_for_plaintext(const GString *verified, MimeInfo **created)
{
	gchar *tmpfile;
	MimeInfo *newinfo = NULL;
	MimeInfo *decinfo = NULL;

	tmpfile = get_tmp_file();

	str_write_to_file(verified->str, tmpfile, TRUE);
	newinfo = procmime_scan_file(tmpfile);
	g_free(tmpfile);
	decinfo = g_node_first_child(newinfo->node) != NULL ?
		g_node_first_child(newinfo->node)->data : NULL;

	if (decinfo == NULL)
		return FALSE;

	g_node_unlink(decinfo->node);
	procmime_mimeinfo_free_all(&newinfo);
	decinfo->tmp = TRUE;

	*created = decinfo;
	return TRUE;
}

static void check_pkcs7_mime_sig_task(GTask *task,
	gpointer source_object,
	gpointer _task_data,
	GCancellable *cancellable)
{
	PKCS7MimeTaskData *task_data = (PKCS7MimeTaskData *)_task_data;
	GQuark domain;
	gpgme_ctx_t ctx;
	gpgme_error_t err;
	gpgme_data_t sigdata = NULL;
	gpgme_data_t plain;
	gpgme_verify_result_t gpgme_res;
	size_t len;
	gboolean return_err = TRUE;
	gboolean cancelled = FALSE;
	SigCheckTaskResult *task_result = NULL;
	MimeInfo *created = NULL;
	GString *verified;
	gchar *tmp;
	char err_str[GPGERR_BUFSIZE] = "";

	domain = g_quark_from_static_string("claws_smime");

	err = gpgme_new(&ctx);
	if (err != GPG_ERR_NO_ERROR) {
		gpgme_strerror_r(err, err_str, GPGERR_BUFSIZE);
		g_warning("couldn't initialize GPG context: %s", err_str);
		goto out;
	}

	err = gpgme_set_protocol(ctx, GPGME_PROTOCOL_CMS);
	if (err != GPG_ERR_NO_ERROR) {
		gpgme_strerror_r(err, err_str, GPGERR_BUFSIZE);
		g_warning("couldn't set GPG protocol: %s", err_str);
		goto out_ctx;
	}

	err = gpgme_data_new_from_mem(&sigdata,
		task_data->textstr,
		task_data->textstr ? strlen(task_data->textstr) : 0,
		0);
	if (err != GPG_ERR_NO_ERROR) {
		gpgme_strerror_r(err, err_str, GPGERR_BUFSIZE);
		g_warning("gpgme_data_new_from_mem failed: %s", err_str);
		goto out_ctx;
	}

	if (task_data->encoding == ENC_BASE64) {
		err = gpgme_data_set_encoding (sigdata, GPGME_DATA_ENCODING_BASE64);
		if (err != GPG_ERR_NO_ERROR) {
			gpgme_strerror_r(err, err_str, GPGERR_BUFSIZE);
			g_warning("gpgme_data_set_encoding failed: %s\n", err_str);
			goto out_sigdata;
		}
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

	if (task_data->create_mimeinfo) {
		tmp = gpgme_data_release_and_get_mem(plain, &len);
		if (!tmp) {
			debug_print("S/MIME signed message had no plaintext\n");
			goto out_sigdata;
		}

		verified = g_string_new_len(tmp, len);
		gpgme_free(tmp);

		if (!create_mimeinfo_for_plaintext(verified, &created)) {
			g_warning("Failed to create new mimeinfo from plaintext");
			g_string_free(verified, TRUE);
			goto out_sigdata;
		}

		g_string_free(verified, TRUE);
	} else {
		gpgme_data_release(plain);
	}

	task_result = g_new0(SigCheckTaskResult, 1);
	task_result->sig_data = g_new0(SignatureData, 1);

	task_result->sig_data->status = sgpgme_sigstat_gpgme_to_privacy(ctx, gpgme_res);
	task_result->sig_data->info_short = sgpgme_sigstat_info_short(ctx, gpgme_res);
	task_result->sig_data->info_full = sgpgme_sigstat_info_full(ctx, gpgme_res);

	task_result->newinfo = created;
	return_err = FALSE;

	goto out_sigdata;

out_plain:
	gpgme_data_release(plain);
out_sigdata:
	gpgme_data_release(sigdata);
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

/* Check PKCS7-MIME signed-data type signature either synchronously or asynchronously.
 * Check it asynchronously if the caller provides a callback.
 * If the caller does not provide a callback, and we have not already done so, create
 * and insert a new MimeInfo for the plaintext data returned by the sig verification.
 */
static gint check_pkcs7_mime_sig(MimeInfo *mimeinfo,
	GCancellable *_cancellable,
	GAsyncReadyCallback callback,
	gpointer user_data)
{
	MimeInfo *parent;
	const gchar *tmp;
	EncodingType real_enc;
	gchar *textstr = NULL;
	PrivacyDataPGP *privacy_data;
	GCancellable *cancellable;
	GTask *task;
	PKCS7MimeTaskData *task_data;
	SigCheckTaskResult *task_result;
	GError *error = NULL;
	gboolean unref_cancellable = FALSE;

	debug_print("Checking pkcs7-mime signature\n");

	parent = procmime_mimeinfo_parent(mimeinfo);
	tmp = g_hash_table_lookup(parent->typeparameters, "boundary");
	if (tmp) {
		g_warning("Unexpected S/MIME message format subtype:%s boundary:%s",
			mimeinfo->subtype, tmp);
		return -1;
	}

	if (g_ascii_strcasecmp(mimeinfo->subtype, "pkcs7-mime") &&
		g_ascii_strcasecmp(mimeinfo->subtype, "x-pkcs7-mime"))
	{
		g_warning("Unexpected S/MIME subtype:%s", mimeinfo->subtype);
		return -1;
	}

	tmp = procmime_mimeinfo_get_parameter(mimeinfo, "smime-type");
	if (!tmp || g_ascii_strcasecmp(tmp, "signed-data")) {
		g_warning("Unexpected S/MIME smime-type parameter:%s", tmp);
		return -1;
	}

	real_enc = mimeinfo->encoding_type;
	mimeinfo->encoding_type = ENC_BINARY;
	textstr = procmime_get_part_as_string(mimeinfo, TRUE);
	mimeinfo->encoding_type = real_enc;
	if (!textstr) {
		g_warning("Failed to get PKCS7-Mime signature data");
		return -1;
	}

	privacy_data = (PrivacyDataPGP *)mimeinfo->privacy;

	task_data = g_new0(PKCS7MimeTaskData, 1);
	task_data->textstr = textstr;
	task_data->encoding = mimeinfo->encoding_type;

	if (!callback && !privacy_data->inserted_mimeinfo)
		task_data->create_mimeinfo = TRUE;

	if (_cancellable != NULL) {
		cancellable = _cancellable;
	} else {
		cancellable = g_cancellable_new();
		unref_cancellable = TRUE;
	}

	task = g_task_new(NULL, cancellable, callback, user_data);
	mimeinfo->last_sig_check_task = task;

	g_task_set_task_data(task, task_data, free_pkcs7_mime_task_data);
	g_task_set_return_on_cancel(task, TRUE);

	if (callback) {
		debug_print("creating check sig async task:%p task_data:%p\n", task, task_data);
		g_task_run_in_thread(task, check_pkcs7_mime_sig_task);
		g_object_unref(task);
		return 0;
	}

	debug_print("creating check sig sync task:%p task_data:%p\n", task, task_data);
	g_task_run_in_thread_sync(task, check_pkcs7_mime_sig_task);
	mimeinfo->last_sig_check_task = NULL;

	task_result = g_task_propagate_pointer(task, &error);
	if (unref_cancellable)
		g_object_unref(cancellable);

	if (mimeinfo->sig_data) {
		privacy_free_signature_data(mimeinfo->sig_data);
		mimeinfo->sig_data = NULL;
	}

	if (task_result == NULL) {
		debug_print("sig check task propagated NULL task: %p GError: domain: %s code: %d message: \"%s\"\n",
			task, g_quark_to_string(error->domain), error->code, error->message);
		g_object_unref(task);
		g_error_free(error);
		return -1;
	}
	g_object_unref(task);

	mimeinfo->sig_data = task_result->sig_data;

	if (task_result->newinfo) {
		if (parent->type == MIMETYPE_MESSAGE && !strcmp(parent->subtype, "rfc822")) {
			if (parent->content == MIMECONTENT_MEM) {
				gint newlen = (gint)(strstr(parent->data.mem, "\n\n") - parent->data.mem);
				if (newlen > 0)
				parent->length = newlen;
			}
		}

		g_node_prepend(parent->node, task_result->newinfo->node);
		privacy_data->inserted_mimeinfo = TRUE;
	}

	/* Only free the task result struct, not the SigData and MimeInfo */
	g_free(task_result);

	return 1;
}

static gint smime_check_sig_async(MimeInfo *mimeinfo,
	GCancellable *cancellable,
	GAsyncReadyCallback callback,
	gpointer user_data)
{
	MimeInfo *parent;
	gchar *boundary;

	/* Detached signature with a boundary */
	if (g_ascii_strcasecmp(mimeinfo->subtype, "pkcs7-mime") &&
		g_ascii_strcasecmp(mimeinfo->subtype, "x-pkcs7-mime"))
	{
		parent = procmime_mimeinfo_parent(mimeinfo);
		boundary = g_hash_table_lookup(parent->typeparameters, "boundary");

		if (boundary == NULL) {
			g_warning("Unexpected S/MIME format subtype:%s without a boundary",
				mimeinfo->subtype);
			return -1;
		}

		return cm_check_detached_sig_async(mimeinfo,
			cancellable,
			callback,
			user_data,
			GPGME_PROTOCOL_CMS,
			get_canonical_content);

	/* Opaque pkcs7-mime blob with smime-type=signed-data */
	} else {
		return check_pkcs7_mime_sig(mimeinfo, cancellable, callback, user_data);
	}
}

static gboolean smime_is_encrypted(MimeInfo *mimeinfo)
{
	const gchar *tmpstr;
	
	if (mimeinfo->type != MIMETYPE_APPLICATION)
		return FALSE;
	if (!g_ascii_strcasecmp(mimeinfo->subtype, "pkcs7-mime")) {
		tmpstr = procmime_mimeinfo_get_parameter(mimeinfo, "smime-type");
		if (tmpstr && g_ascii_strcasecmp(tmpstr, "enveloped-data"))
			return FALSE;
		else 
			return TRUE;

	} else if (!g_ascii_strcasecmp(mimeinfo->subtype, "x-pkcs7-mime")) {
		tmpstr = procmime_mimeinfo_get_parameter(mimeinfo, "smime-type");
		if (tmpstr && g_ascii_strcasecmp(tmpstr, "enveloped-data"))
			return FALSE;
		else 
			return TRUE;
	}
	return FALSE;
}

static MimeInfo *smime_decrypt(MimeInfo *mimeinfo)
{
	MimeInfo *encinfo, *decinfo, *parseinfo;
	gpgme_data_t cipher = NULL, plain = NULL;
	static gint id = 0;
	FILE *dstfp;
	gchar *fname;
	gpgme_verify_result_t sigstat = NULL;
	PrivacyDataPGP *data = NULL;
	gpgme_ctx_t ctx;
	gpgme_error_t err;
	gchar *chars;
	size_t len;
	SignatureData *sig_data = NULL;

	cm_return_val_if_fail(smime_is_encrypted(mimeinfo), NULL);
	
	if ((err = gpgme_new(&ctx)) != GPG_ERR_NO_ERROR) {
		privacy_set_error(_("Couldn't initialize GPG context, %s"), gpgme_strerror(err));
		return NULL;
	}

	err = gpgme_set_protocol(ctx, GPGME_PROTOCOL_CMS);
	if (err) {
		debug_print ("gpgme_set_protocol failed: %s\n",
                   gpgme_strerror (err));
		privacy_set_error(_("Couldn't set GPG protocol, %s"), gpgme_strerror(err));
		gpgme_release(ctx);
		return NULL;
	}
	gpgme_set_armor(ctx, TRUE);

	encinfo = mimeinfo;

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
        	g_free(fname);
        	gpgme_data_release(plain);
		debug_print("can't open!\n");
		privacy_set_error(_("Couldn't open temporary file"));
		if (sig_data)
			privacy_free_signature_data(sig_data);
		return NULL;
    	}

	if (fprintf(dstfp, "MIME-Version: 1.0\n") < 0) {
        	FILE_OP_ERROR(fname, "fprintf");
        	g_free(fname);
		claws_fclose(dstfp);
        	gpgme_data_release(plain);
		debug_print("can't close!\n");
		privacy_set_error(_("Couldn't write to temporary file"));
		if (sig_data)
			privacy_free_signature_data(sig_data);
		return NULL;
	}

	chars = sgpgme_data_release_and_get_mem(plain, &len);

	if (len > 0) {
		if (claws_fwrite(chars, 1, len, dstfp) < len) {
        		FILE_OP_ERROR(fname, "claws_fwrite");
        		claws_fclose(dstfp);
        		g_free(fname);
        		g_free(chars);
        		gpgme_data_release(plain);
			debug_print("can't write!\n");
			privacy_set_error(_("Couldn't write to temporary file"));
			if (sig_data)
				privacy_free_signature_data(sig_data);
			return NULL;
		}
	}
	if (claws_safe_fclose(dstfp) == EOF) {
        	FILE_OP_ERROR(fname, "claws_fclose");
        	g_free(fname);
       		g_free(chars);
        	gpgme_data_release(plain);
		debug_print("can't close!\n");
		privacy_set_error(_("Couldn't close temporary file"));
		if (sig_data)
			privacy_free_signature_data(sig_data);
		return NULL;
	}
	g_free(chars);

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
			data = smime_new_privacydata();
			if (!data) {
				return NULL;
			}
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

gboolean smime_sign(MimeInfo *mimeinfo, PrefsAccount *account, const gchar *from_addr)
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
	gchar *real_content = NULL;
	
	fp = my_tmpfile();
	if (fp == NULL) {
		perror("my_tmpfile");
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
		if (boundary)
			g_free(boundary);
		boundary = generate_mime_boundary("Sig");
	} while (strstr(test_msg, boundary) != NULL);
	
	g_free(test_msg);

	g_hash_table_insert(sigmultipart->typeparameters, g_strdup("boundary"),
                            g_strdup(boundary));
	g_hash_table_insert(sigmultipart->typeparameters, g_strdup("protocol"),
                            g_strdup("application/pkcs7-signature"));
	g_node_append(sigmultipart->node, msgcontent->node);
	g_node_append(mimeinfo->node, sigmultipart->node);

	/* write message content to temporary file */
	fp = my_tmpfile();
	if (fp == NULL) {
		perror("my_tmpfile");
		g_free(boundary);
		return FALSE;
	}
	procmime_write_mimeinfo(sigmultipart, fp);
	rewind(fp);

	/* read temporary file into memory */
	textstr = get_canonical_content(fp, boundary);

	g_free(boundary);

	claws_fclose(fp);

	gpgme_data_new_from_mem(&gpgtext, textstr, textstr?strlen(textstr):0, 0);
	gpgme_data_new(&gpgsig);
	gpgme_new(&ctx);
	gpgme_set_armor(ctx, TRUE);
	gpgme_signers_clear (ctx);

	err = gpgme_set_protocol(ctx, GPGME_PROTOCOL_CMS);

	if (err) {
		debug_print ("gpgme_set_protocol failed: %s\n",
                   gpgme_strerror (err));
		gpgme_data_release(gpgtext);
		gpgme_release(ctx);
		return FALSE;
	}

	if (!sgpgme_setup_signers(ctx, account, from_addr)) {
		debug_print("setup_signers failed\n");
		gpgme_data_release(gpgtext);
		gpgme_release(ctx);
		return FALSE;
	}

	info.c = ctx;
	prefs_gpg_enable_agent(TRUE);
    	gpgme_set_passphrase_cb (ctx, NULL, &info);
	
	err = gpgme_op_sign(ctx, gpgtext, gpgsig, GPGME_SIG_MODE_DETACH);
	if (err != GPG_ERR_NO_ERROR) {
		alertpanel_error("S/MIME : Cannot sign, %s (%d)", gpg_strerror(err), gpg_err_code(err));
		gpgme_data_release(gpgtext);
		gpgme_release(ctx);
		return FALSE;
	}
	result = gpgme_op_sign_result(ctx);
	if (result && result->signatures) {
	    if (gpgme_get_protocol(ctx) == GPGME_PROTOCOL_OpenPGP) {
		gchar *down_algo = g_ascii_strdown(gpgme_hash_algo_name(
			    result->signatures->hash_algo), -1);
		micalg = g_strdup_printf("pgp-%s", down_algo);
		g_free(down_algo);
	    } else {
		micalg = g_strdup(gpgme_hash_algo_name(
			    result->signatures->hash_algo));
	    }
	} else {
	    /* can't get result (maybe no signing key?) */
	    debug_print("gpgme_op_sign_result error\n");
	    return FALSE;
	}

	gpgme_release(ctx);
	sigcontent = sgpgme_data_release_and_get_mem(gpgsig, &len);
	gpgme_data_release(gpgtext);
	g_free(textstr);

	if (!sigcontent) {
		gpgme_release(ctx);
		g_free(micalg);
		return FALSE;
	}
	real_content = sigcontent+strlen("-----BEGIN SIGNED MESSAGE-----\n");
	if (!strstr(real_content, "-----END SIGNED MESSAGE-----")) {
		debug_print("missing end\n");
		gpgme_release(ctx);
		g_free(micalg);
		return FALSE;
	}
	*strstr(real_content, "-----END SIGNED MESSAGE-----") = '\0';
	/* add signature */
	g_hash_table_insert(sigmultipart->typeparameters, g_strdup("micalg"),
                            micalg);

	newinfo = procmime_mimeinfo_new();
	newinfo->type = MIMETYPE_APPLICATION;
	newinfo->subtype = g_strdup("pkcs7-signature");
	g_hash_table_insert(newinfo->typeparameters, g_strdup("name"),
			     g_strdup("smime.p7s"));
	newinfo->content = MIMECONTENT_MEM;
	newinfo->disposition = DISPOSITIONTYPE_ATTACHMENT;
	g_hash_table_insert(newinfo->dispositionparameters, g_strdup("filename"),
			    g_strdup("smime.p7s"));
	newinfo->data.mem = g_malloc(len + 1);
	newinfo->tmp = TRUE;
	memmove(newinfo->data.mem, real_content, len);
	newinfo->data.mem[len] = '\0';
	newinfo->encoding_type = ENC_BASE64;
	g_node_append(sigmultipart->node, newinfo->node);

	g_free(sigcontent);

	return TRUE;
}
gchar *smime_get_encrypt_data(GSList *recp_names)
{
	return sgpgme_get_encrypt_data(recp_names, GPGME_PROTOCOL_CMS);
}

static const gchar *smime_get_encrypt_warning(void)
{
	if (prefs_gpg_should_skip_encryption_warning(smime_system.id))
		return NULL;
	else
		return _("Please note that email headers, like Subject, "
			 "are not encrypted by the S/MIME system.");
}

static void smime_inhibit_encrypt_warning(gboolean inhibit)
{
	if (inhibit)
		prefs_gpg_add_skip_encryption_warning(smime_system.id);
	else
		prefs_gpg_remove_skip_encryption_warning(smime_system.id);
}

gboolean smime_encrypt(MimeInfo *mimeinfo, const gchar *encrypt_data)
{
	MimeInfo *msgcontent, *encmultipart;
	FILE *fp;
	gchar *enccontent;
	size_t len;
	gchar *textstr = NULL;
	gpgme_data_t gpgtext = NULL, gpgenc = NULL;
	gpgme_ctx_t ctx = NULL;
	gpgme_key_t *kset = NULL;
	gchar **fprs = g_strsplit(encrypt_data, " ", -1);
	gint i = 0;
	gpgme_error_t err;
	gchar *tmpfile = NULL;

	while (fprs[i] && strlen(fprs[i])) {
		i++;
	}

	if ((err = gpgme_new(&ctx)) != GPG_ERR_NO_ERROR) {
		debug_print ("gpgme_new failed: %s\n", gpgme_strerror(err));
		g_free(fprs);
		return FALSE;
	}

	err = gpgme_set_protocol(ctx, GPGME_PROTOCOL_CMS);

	if (err) {
		debug_print ("gpgme_set_protocol failed: %s\n",
                   gpgme_strerror (err));
		g_free(fprs);
		return FALSE;
	}

	kset = g_malloc(sizeof(gpgme_key_t)*(i+1));
	memset(kset, 0, sizeof(gpgme_key_t)*(i+1));
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
	g_free(fprs);

	debug_print("Encrypting message content\n");

	/* remove content node from message */
	msgcontent = (MimeInfo *) mimeinfo->node->children->data;
	g_node_unlink(msgcontent->node);


	/* create temporary multipart for content */
	encmultipart = procmime_mimeinfo_new();
	encmultipart->type = MIMETYPE_APPLICATION;
	encmultipart->subtype = g_strdup("x-pkcs7-mime");
	g_hash_table_insert(encmultipart->typeparameters, g_strdup("name"),
                            g_strdup("smime.p7m"));
	g_hash_table_insert(encmultipart->typeparameters,
			    g_strdup("smime-type"),
			    g_strdup("enveloped-data"));
	
	encmultipart->disposition = DISPOSITIONTYPE_ATTACHMENT;
	g_hash_table_insert(encmultipart->dispositionparameters, g_strdup("filename"),
                            g_strdup("smime.p7m"));

	g_node_append(encmultipart->node, msgcontent->node);

	/* write message content to temporary file */
	tmpfile = get_tmp_file();
	fp = claws_fopen(tmpfile, "wb");
	if (fp == NULL) {
		FILE_OP_ERROR(tmpfile, "create");
		for (gint x = 0; x < i; x++)
			gpgme_key_unref(kset[x]);
		g_free(kset);
		g_free(tmpfile);
		return FALSE;
	}
	procmime_decode_content(msgcontent);
	procmime_write_mime_header(msgcontent, fp);
	procmime_write_mimeinfo(msgcontent, fp);
	claws_safe_fclose(fp);
	canonicalize_file_replace(tmpfile);
	fp = claws_fopen(tmpfile, "rb");
	if (fp == NULL) {
		FILE_OP_ERROR(tmpfile, "open");
		for (gint x = 0; x < i; x++)
			gpgme_key_unref(kset[x]);
		g_free(kset);
		g_free(tmpfile);
		return FALSE;
	}
	g_free(tmpfile);

	/* read temporary file into memory */
	textstr = file_read_stream_to_str_no_recode(fp);

	claws_fclose(fp);

	/* encrypt data */
	gpgme_data_new_from_mem(&gpgtext, textstr, textstr?strlen(textstr):0, 0);
	gpgme_data_new(&gpgenc);
	cm_gpgme_data_rewind(gpgtext);
	
	gpgme_op_encrypt(ctx, kset, GPGME_ENCRYPT_ALWAYS_TRUST, gpgtext, gpgenc);

	gpgme_release(ctx);
	for (gint x = 0; x < i; x++)
		gpgme_key_unref(kset[x]);
	g_free(kset);
	enccontent = sgpgme_data_release_and_get_mem(gpgenc, &len);

	if (!enccontent) {
		g_warning("no enccontent");
		return FALSE;
	}

	tmpfile = get_tmp_file();
	fp = claws_fopen(tmpfile, "wb");
	if (fp) {
		if (claws_fwrite(enccontent, 1, len, fp) < len) {
			FILE_OP_ERROR(tmpfile, "claws_fwrite");
			claws_fclose(fp);
			claws_unlink(tmpfile);
		}
		if (claws_safe_fclose(fp) == EOF) {
			FILE_OP_ERROR(tmpfile, "claws_fclose");
			claws_unlink(tmpfile);
			g_free(tmpfile);
			g_free(enccontent);
			return FALSE;
		}
	} else {
		FILE_OP_ERROR(tmpfile, "create");
		g_free(tmpfile);
		g_free(enccontent);
		return FALSE;
	}
	gpgme_data_release(gpgtext);
	g_free(textstr);

	/* create encrypted multipart */
	procmime_mimeinfo_free_all(&msgcontent);
	g_node_append(mimeinfo->node, encmultipart->node);

	encmultipart->content = MIMECONTENT_FILE;
	encmultipart->data.filename = tmpfile;
	encmultipart->tmp = TRUE;
	procmime_encode_content(encmultipart, ENC_BASE64);

	g_free(enccontent);

	return TRUE;
}

static PrivacySystem smime_system = {
	"smime",			/* id */
	"S-MIME",			/* name */

	smime_free_privacydata,	/* free_privacydata */

	smime_is_signed,		/* is_signed(MimeInfo *) */
	smime_check_sig_async,

	smime_is_encrypted,		/* is_encrypted(MimeInfo *) */
	smime_decrypt,			/* decrypt(MimeInfo *) */

	TRUE,
	smime_sign,

	TRUE,
	smime_get_encrypt_data,
	smime_encrypt,
	smime_get_encrypt_warning,
	smime_inhibit_encrypt_warning,
	prefs_gpg_auto_check_signatures,
};

void smime_init()
{
	privacy_register_system(&smime_system);
}

void smime_done()
{
	privacy_unregister_system(&smime_system);
}

struct PluginFeature *plugin_provides(void)
{
	static struct PluginFeature features[] = 
		{ {PLUGIN_PRIVACY, N_("S/MIME")},
		  {PLUGIN_NOTHING, NULL}};
	return features;
}
#endif /* USE_GPGME */
