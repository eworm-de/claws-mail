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

#include "sgpgme.h"
#include "privacy.h"
#include "prefs_common.h"
#include "utils.h"
#include "alertpanel.h"
#include "passphrase.h"
#include "intl.h"
#include "prefs_gpg.h"
#include "select-keys.h"

static void idle_function_for_gpgme(void)
{
	while (gtk_events_pending())
		gtk_main_iteration();
}

static void sgpgme_disable_all(void)
{
    /* FIXME: set a flag, so that we don't bother the user with failed
     * gpgme messages */
}

GpgmeSigStat sgpgme_verify_signature(GpgmeCtx ctx, GpgmeData sig, 
					GpgmeData plain)
{
	GpgmeSigStat status;

	if (gpgme_op_verify(ctx, sig, plain, &status) != GPGME_No_Error)
		return GPGME_SIG_STAT_ERROR;

	return status;
}

SignatureStatus sgpgme_sigstat_gpgme_to_privacy(GpgmeCtx ctx, GpgmeSigStat status)
{
	unsigned long validity = 0;
	
	validity = gpgme_get_sig_ulong_attr(ctx, 0,
		GPGME_ATTR_VALIDITY, 0);

	switch (status) {
	case GPGME_SIG_STAT_GOOD:
		if ((validity != GPGME_VALIDITY_MARGINAL) &&
		    (validity != GPGME_VALIDITY_FULL) &&
		    (validity != GPGME_VALIDITY_ULTIMATE))
			return SIGNATURE_WARN;
		return SIGNATURE_OK;
	case GPGME_SIG_STAT_GOOD_EXP:
	case GPGME_SIG_STAT_GOOD_EXPKEY:
	case GPGME_SIG_STAT_DIFF:
		return SIGNATURE_WARN;
	case GPGME_SIG_STAT_BAD:
		return SIGNATURE_INVALID;
	case GPGME_SIG_STAT_NOKEY:
	case GPGME_SIG_STAT_NOSIG:
	case GPGME_SIG_STAT_ERROR:
		return SIGNATURE_CHECK_FAILED;
	case GPGME_SIG_STAT_NONE:
		return SIGNATURE_UNCHECKED;
	}
	return SIGNATURE_CHECK_FAILED;
}

static const gchar *get_validity_str(unsigned long validity)
{
	switch (validity) {
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

gchar *sgpgme_sigstat_info_short(GpgmeCtx ctx, GpgmeSigStat status)
{
	switch (status) {
	case GPGME_SIG_STAT_GOOD:
	{
		GpgmeKey key;
		unsigned long validity = 0;
	
        	if (gpgme_get_sig_key(ctx, 0, &key) != GPGME_No_Error)
                	return g_strdup(_("Error"));

		validity = gpgme_get_sig_ulong_attr(ctx, 0,
			GPGME_ATTR_VALIDITY, 0);
		
		return g_strdup_printf(_("Valid signature by %s (Trust: %s)"),
			gpgme_key_get_string_attr(key, GPGME_ATTR_NAME, NULL, 0),
			get_validity_str(validity));
	}
	case GPGME_SIG_STAT_GOOD_EXP:
		return g_strdup(_("The signature has expired"));
	case GPGME_SIG_STAT_GOOD_EXPKEY:
		return g_strdup(_("The key that was used to sign this part has expired"));
	case GPGME_SIG_STAT_DIFF:
		return g_strdup(_("Not all signatures are valid"));
	case GPGME_SIG_STAT_BAD:
		return g_strdup(_("This signature is invalid"));
	case GPGME_SIG_STAT_NOKEY:
		return g_strdup(_("You have no key to verify this signature"));
	case GPGME_SIG_STAT_NOSIG:
		return g_strdup(_("No signature found"));
	case GPGME_SIG_STAT_ERROR:
		return g_strdup(_("An error occured"));
	case GPGME_SIG_STAT_NONE:
		return g_strdup(_("The signature has not been checked"));
	}
	return g_strdup(_("Error"));
}

gchar *sgpgme_sigstat_info_full(GpgmeCtx ctx, GpgmeSigStat status)
{
	gint i = 0;
	gchar *ret;
	GString *siginfo;
	GpgmeKey key;
	
	siginfo = g_string_sized_new(64);
	while (gpgme_get_sig_key(ctx, i, &key) != GPGME_EOF) {
		time_t sigtime, expiretime;
		GpgmeSigStat sigstatus;
		gchar timestr[64];
		const gchar *keytype, *keyid, *uid;
		
		sigtime = gpgme_get_sig_ulong_attr(ctx, i, GPGME_ATTR_CREATED, 0);
		strftime(timestr, 64, "%c", gmtime(&sigtime));
		keytype = gpgme_key_get_string_attr(key, GPGME_ATTR_ALGO, NULL, 0);
		keyid = gpgme_key_get_string_attr(key, GPGME_ATTR_KEYID, NULL, 0);
		g_string_sprintfa(siginfo,
			_("Signature made %s using %s key ID %s\n"),
			timestr, keytype, keyid);
		
		sigstatus = gpgme_get_sig_ulong_attr(ctx, i, GPGME_ATTR_SIG_STATUS, 0);	
		uid = gpgme_key_get_string_attr(key, GPGME_ATTR_USERID, NULL, 0);
		switch (sigstatus) {
		case GPGME_SIG_STAT_GOOD:
		case GPGME_SIG_STAT_GOOD_EXPKEY:
			g_string_sprintfa(siginfo,
				_("Good signature from \"%s\"\n"),
				uid);
			break;
		case GPGME_SIG_STAT_GOOD_EXP:
			g_string_sprintfa(siginfo,
				_("Expired signature from \"%s\"\n"),
				uid);
			break;
		case GPGME_SIG_STAT_BAD:
			g_string_sprintfa(siginfo,
				_("BAD signature from \"%s\"\n"),
				uid);
			break;
		default:
			break;
		}
		if (sigstatus != GPGME_SIG_STAT_BAD) {
			gint j = 1;
			
			while ((uid = gpgme_key_get_string_attr(key, GPGME_ATTR_USERID, NULL, j)) != 0) {
				g_string_sprintfa(siginfo,
					_("                aka \"%s\"\n"),
					uid);
				j++;
			}
			g_string_sprintfa(siginfo,
				_("Primary key fingerprint: %s\n"), 
				gpgme_key_get_string_attr(key, GPGME_ATTR_FPR, NULL, 0));
		}

		
		expiretime = gpgme_get_sig_ulong_attr(ctx, i, GPGME_ATTR_EXPIRE, 0);
		if (expiretime > 0) {
			const gchar *format;

			strftime(timestr, 64, "%c", gmtime(&expiretime));
			if (time(NULL) < expiretime)
				format = _("Signature expires %s\n");
			else
				format = _("Signature expired %s\n");
			g_string_sprintfa(siginfo, format, timestr);
		}
		
		g_string_append(siginfo, "\n");
		i++;
	}

	ret = siginfo->str;
	g_string_free(siginfo, FALSE);
	return ret;
}

GpgmeData sgpgme_data_from_mimeinfo(MimeInfo *mimeinfo)
{
	GpgmeData data;
	
	gpgme_data_new_from_filepart(&data,
		mimeinfo->data.filename,
		NULL,
		mimeinfo->offset,
		mimeinfo->length);
	
	return data;
}

GpgmeData sgpgme_decrypt_verify(GpgmeData cipher, GpgmeSigStat *status, GpgmeCtx ctx)
{
	struct passphrase_cb_info_s info;
	GpgmeData plain;
	GpgmeError err;

	memset (&info, 0, sizeof info);
	
	if (gpgme_data_new(&plain) != GPGME_No_Error) {
		gpgme_release(ctx);
		return NULL;
	}
	
    	if (!getenv("GPG_AGENT_INFO")) {
        	info.c = ctx;
        	gpgme_set_passphrase_cb (ctx, gpgmegtk_passphrase_cb, &info);
    	}

	err = gpgme_op_decrypt_verify(ctx, cipher, plain, status);

	if (err != GPGME_No_Error) {
		gpgmegtk_free_passphrase();
		gpgme_data_release(plain);
		return NULL;
	}

	return plain;
}

gchar *sgpgme_get_encrypt_data(GSList *recp_names)
{

	GpgmeRecipients recp;
	GString *encdata;
	void *iter;
	const gchar *recipient;
	gchar *data;

	recp = gpgmegtk_recipient_selection(recp_names);
	if (recp == NULL)
		return NULL;

	if (gpgme_recipients_enum_open(recp, &iter) != GPGME_No_Error) {
		gpgme_recipients_release(recp);
		return NULL;
	}

	encdata = g_string_sized_new(64);
	while ((recipient = gpgme_recipients_enum_read(recp, &iter)) != NULL) {
		if (encdata->len > 0)
			g_string_append_c(encdata, ' ');
		g_string_append(encdata, recipient);
	}

	gpgme_recipients_release(recp);

	data = encdata->str;
	g_string_free(encdata, FALSE);

	return data;
}

gboolean sgpgme_setup_signers(GpgmeCtx ctx, PrefsAccount *account)
{
	GPGAccountConfig *config;

	gpgme_signers_clear(ctx);

	config = prefs_gpg_account_get_config(account);

	if (config->sign_key != SIGN_KEY_DEFAULT) {
		gchar *keyid;
		GpgmeKey key;

		if (config->sign_key == SIGN_KEY_BY_FROM)
			keyid = account->address;
		else if (config->sign_key == SIGN_KEY_CUSTOM)
			keyid = config->sign_key_id;
		else
			return FALSE;

		gpgme_op_keylist_start(ctx, keyid, 1);
		while (!gpgme_op_keylist_next(ctx, &key)) {
			debug_print("adding key: %s\n", 
				gpgme_key_get_string_attr(key, GPGME_ATTR_KEYID, NULL, 0));
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
	if (gpgme_engine_check_version(GPGME_PROTOCOL_OpenPGP) != 
			GPGME_No_Error) {  /* Also does some gpgme init */
		sgpgme_disable_all();
		debug_print("gpgme_engine_version:\n%s\n",
			    gpgme_get_engine_info());

		if (prefs_gpg_get_config()->gpg_warning) {
			AlertValue val;

			val = alertpanel_message_with_disable
				(_("Warning"),
				 _("GnuPG is not installed properly, or needs "
				   "to be upgraded.\n"
				   "OpenPGP support disabled."), 
				NULL, NULL, NULL, ALERT_WARNING);
			if (val & G_ALERTDISABLE)
				prefs_gpg_get_config()->gpg_warning = FALSE;
		}
	}

	gpgme_register_idle(idle_function_for_gpgme);
}

void sgpgme_done()
{
        gpgmegtk_free_passphrase();
	gpgme_register_idle(NULL);
}

#endif /* USE_GPGME */
