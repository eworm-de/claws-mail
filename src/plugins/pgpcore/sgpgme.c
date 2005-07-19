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

#include <time.h>
#include <gtk/gtk.h>
#include <gpgme.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <stdio.h>
#include <errno.h>

#include "sgpgme.h"
#include "privacy.h"
#include "prefs_common.h"
#include "utils.h"
#include "alertpanel.h"
#include "passphrase.h"
#include "prefs_gpg.h"
#include "select-keys.h"

static void sgpgme_disable_all(void)
{
    /* FIXME: set a flag, so that we don't bother the user with failed
     * gpgme messages */
}

gpgme_verify_result_t sgpgme_verify_signature(gpgme_ctx_t ctx, gpgme_data_t sig, 
					gpgme_data_t plain, gpgme_data_t dummy)
{
	gpgme_verify_result_t status = NULL;
	gpgme_error_t err;

	if ((err = gpgme_op_verify(ctx, sig, plain, dummy)) != GPG_ERR_NO_ERROR) {
		debug_print("op_verify err %s\n", gpgme_strerror(err));
		return NULL;
	}
	status = gpgme_op_verify_result(ctx);

	return status;
}

SignatureStatus sgpgme_sigstat_gpgme_to_privacy(gpgme_ctx_t ctx, gpgme_verify_result_t status)
{
	unsigned long validity = 0;
	gpgme_signature_t sig = NULL;
	
	if (status == NULL)
		return SIGNATURE_UNCHECKED;

	sig = status->signatures;

	if (sig == NULL)
		return SIGNATURE_UNCHECKED;

	validity = sig->validity;

	switch (gpg_err_code(sig->status)) {
	case GPG_ERR_NO_ERROR:
		if ((validity != GPGME_VALIDITY_MARGINAL) &&
		    (validity != GPGME_VALIDITY_FULL) &&
		    (validity != GPGME_VALIDITY_ULTIMATE))
			return SIGNATURE_WARN;
		return SIGNATURE_OK;
	case GPG_ERR_SIG_EXPIRED:
	case GPG_ERR_KEY_EXPIRED:
		return SIGNATURE_WARN;
	case GPG_ERR_BAD_SIGNATURE:
		return SIGNATURE_INVALID;
	case GPG_ERR_NO_PUBKEY:
		return SIGNATURE_CHECK_FAILED;
	default:
		return SIGNATURE_CHECK_FAILED;
	}
	return SIGNATURE_CHECK_FAILED;
}

static const gchar *get_validity_str(unsigned long validity)
{
	switch (gpg_err_code(validity)) {
	case GPGME_VALIDITY_UNKNOWN:
		return _("Unknown");
	case GPGME_VALIDITY_UNDEFINED:
		return _("Undefined");
	case GPGME_VALIDITY_NEVER:
		return _("Never");
	case GPGME_VALIDITY_MARGINAL:
		return _("Marginal");
	case GPGME_VALIDITY_FULL:
		return _("Full");
	case GPGME_VALIDITY_ULTIMATE:
		return _("Ultimate");
	default:
		return _("Error");
	}
}

gchar *sgpgme_sigstat_info_short(gpgme_ctx_t ctx, gpgme_verify_result_t status)
{
	gpgme_signature_t sig = NULL;
	gpgme_user_id_t user = NULL;
	gchar *uname = NULL;
	gpgme_key_t key;

	if (status == NULL) {
		return g_strdup(_("The signature has not been checked."));
	}
	sig = status->signatures;
	if (sig == NULL) {
		return g_strdup(_("The signature has not been checked."));
	}

	gpgme_get_key(ctx, sig->fpr, &key, 0);
	if (key)
		uname = key->uids->uid;
	else
		uname = "<?>";
	switch (gpg_err_code(sig->status)) {
	case GPG_ERR_NO_ERROR:
	{
		return g_strdup_printf(_("Good signature from %s (Trust: %s)."),
			uname, get_validity_str(sig->validity));
	}
	case GPG_ERR_SIG_EXPIRED:
		return g_strdup_printf(_("Expired signature from %s."), uname);
	case GPG_ERR_KEY_EXPIRED:
		return g_strdup_printf(_("Expired key from %s."), uname);
	case GPG_ERR_BAD_SIGNATURE:
		return g_strdup_printf(_("Bad signature from %s."), uname);
	case GPG_ERR_NO_PUBKEY:
		return g_strdup(_("No key available to verify this signature."));
	default:
		return g_strdup(_("The signature has not been checked."));
	}
	return g_strdup(_("Error"));
}

gchar *sgpgme_sigstat_info_full(gpgme_ctx_t ctx, gpgme_verify_result_t status)
{
	gint i = 0;
	gchar *ret;
	GString *siginfo;
	gpgme_signature_t sig = status->signatures;
	
	siginfo = g_string_sized_new(64);
	while (sig) {
		gpgme_user_id_t user = NULL;
		gpgme_key_t key;

		const gchar *keytype, *keyid, *uid;
		
		gpgme_get_key(ctx, sig->fpr, &key, 0);
		user = key->uids;

		keytype = gpgme_pubkey_algo_name(key->subkeys->pubkey_algo);
		keyid = key->subkeys->keyid;
		g_string_append_printf(siginfo,
			_("Signature made using %s key ID %s\n"),
			keytype, keyid);
		
		uid = user->uid;
		switch (gpg_err_code(sig->status)) {
		case GPG_ERR_NO_ERROR:
		case GPG_ERR_KEY_EXPIRED:
			g_string_append_printf(siginfo,
				_("Good signature from \"%s\"\n"),
				uid);
			break;
		case GPG_ERR_SIG_EXPIRED:
			g_string_append_printf(siginfo,
				_("Expired signature from \"%s\"\n"),
				uid);
			break;
		case GPG_ERR_BAD_SIGNATURE:
			g_string_append_printf(siginfo,
				_("BAD signature from \"%s\"\n"),
				uid);
			break;
		default:
			break;
		}
		if (sig->status != GPG_ERR_BAD_SIGNATURE) {
			gint j = 1;
			user = user->next;
			while (user != NULL) {
				g_string_append_printf(siginfo,
					_("                aka \"%s\"\n"),
					user->uid);
				j++;
				user = user->next;
			}
			g_string_append_printf(siginfo,
				_("Primary key fingerprint: %s\n"), 
				sig->fpr);
		}
		
		g_string_append(siginfo, "\n");
		i++;
		sig = sig->next;
	}

	ret = siginfo->str;
	g_string_free(siginfo, FALSE);
	return ret;
}

gpgme_data_t sgpgme_data_from_mimeinfo(MimeInfo *mimeinfo)
{
	gpgme_data_t data = NULL;
	gpgme_error_t err;
	FILE *fp = fopen(mimeinfo->data.filename, "rb");
	gchar *tmp_file = NULL;

	if (!fp) 
		return NULL;

	tmp_file = get_tmp_file();
	copy_file_part(fp, mimeinfo->offset, mimeinfo->length, tmp_file);
	fclose(fp);
	fp = fopen(tmp_file, "rb");
	debug_print("tmp file %s\n", tmp_file);
	if (!fp) 
		return NULL;
	
	err = gpgme_data_new_from_file(&data, tmp_file, 1);
	unlink(tmp_file);
	g_free(tmp_file);

	debug_print("data %p (%d %d)\n", data, mimeinfo->offset, mimeinfo->length);
	if (err) {
		debug_print ("gpgme_data_new_from_file failed: %s\n",
                   gpgme_strerror (err));
		return NULL;
	}
	return data;
}

gpgme_data_t sgpgme_decrypt_verify(gpgme_data_t cipher, gpgme_verify_result_t *status, gpgme_ctx_t ctx)
{
	struct passphrase_cb_info_s info;
	gpgme_data_t plain;
	gpgme_error_t err;

	memset (&info, 0, sizeof info);
	
	if (gpgme_data_new(&plain) != GPG_ERR_NO_ERROR) {
		gpgme_release(ctx);
		return NULL;
	}
	
    	if (!getenv("GPG_AGENT_INFO")) {
        	info.c = ctx;
        	gpgme_set_passphrase_cb (ctx, gpgmegtk_passphrase_cb, &info);
    	}

	err = gpgme_op_decrypt_verify(ctx, cipher, plain);
	if (err != GPG_ERR_NO_ERROR) {
		debug_print("can't decrypt (%s)\n", gpgme_strerror(err));
		gpgmegtk_free_passphrase();
		gpgme_data_release(plain);
		return NULL;
	}

	err = gpgme_data_rewind(plain);
	if (err) {
		debug_print("can't seek (%d %d %s)\n", err, errno, strerror(errno));
	}

	debug_print("decrypted.\n");
	*status = gpgme_op_verify_result (ctx);

	return plain;
}

gchar *sgpgme_get_encrypt_data(GSList *recp_names)
{
	gpgme_key_t *keys = gpgmegtk_recipient_selection(recp_names);
	gchar *ret = NULL;
	int i = 0;

	if (!keys)
		return NULL;

	while (keys[i]) {
		gpgme_subkey_t skey = keys[i]->subkeys;
		gchar *fpr = skey->fpr;
		gchar *tmp = NULL;
		debug_print("adding %s\n", fpr);
		tmp = g_strconcat(ret?ret:"", fpr, " ", NULL);
		g_free(ret);
		ret = tmp;
		i++;
	}
	return ret;
}

gboolean sgpgme_setup_signers(gpgme_ctx_t ctx, PrefsAccount *account)
{
	GPGAccountConfig *config;

	gpgme_signers_clear(ctx);

	config = prefs_gpg_account_get_config(account);

	if (config->sign_key != SIGN_KEY_DEFAULT) {
		gchar *keyid;
		gpgme_key_t key;

		if (config->sign_key == SIGN_KEY_BY_FROM)
			keyid = account->address;
		else if (config->sign_key == SIGN_KEY_CUSTOM)
			keyid = config->sign_key_id;
		else
			return FALSE;

		gpgme_op_keylist_start(ctx, keyid, 1);
		while (!gpgme_op_keylist_next(ctx, &key)) {
			gpgme_signers_add(ctx, key);
			gpgme_key_release(key);
		}
		gpgme_op_keylist_end(ctx);
	}

	prefs_gpg_account_free_config(config);

	return TRUE;
}

void sgpgme_init()
{
	gpgme_engine_info_t engineInfo;
	if (gpgme_check_version("0.4.5")) {
		gpgme_set_locale(NULL, LC_CTYPE, setlocale(LC_CTYPE, NULL));
		gpgme_set_locale(NULL, LC_MESSAGES, setlocale(LC_MESSAGES, NULL));
		if (!gpgme_get_engine_info(&engineInfo)) {
			while (engineInfo) {
				debug_print("GpgME Protocol: %s\n      Version: %s\n",
					gpgme_get_protocol_name(engineInfo->protocol),
					engineInfo->version);
				engineInfo = engineInfo->next;
			}
		}
	} else {
		sgpgme_disable_all();

		if (prefs_gpg_get_config()->gpg_warning) {
			AlertValue val;

			val = alertpanel_full
				(_("Warning"),
				 _("GnuPG is not installed properly, or needs "
				 "to be upgraded.\n"
				 "OpenPGP support disabled."),
				 GTK_STOCK_CLOSE, NULL, NULL, TRUE, NULL,
				 ALERT_WARNING, G_ALERTDEFAULT);
			if (val & G_ALERTDISABLE)
				prefs_gpg_get_config()->gpg_warning = FALSE;
		}
	}
}

void sgpgme_done()
{
        gpgmegtk_free_passphrase();
}

#endif /* USE_GPGME */
