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

#include <gtk/gtk.h>
#include <gpgme.h>

#include "sgpgme.h"
#include "privacy.h"
#include "prefs_common.h"
#include "utils.h"
#include "alertpanel.h"
#include "passphrase.h"
#include "intl.h"

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

SignatureStatus sgpgme_sigstat_gpgme_to_privacy(GpgmeSigStat status)
{
	switch (status) {
	case GPGME_SIG_STAT_GOOD:
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

void sgpgme_init()
{
	if (gpgme_engine_check_version(GPGME_PROTOCOL_OpenPGP) != 
			GPGME_No_Error) {  /* Also does some gpgme init */
		sgpgme_disable_all();
		debug_print("gpgme_engine_version:\n%s\n",
			    gpgme_get_engine_info());

		if (prefs_common.gpg_warning) {
			AlertValue val;

			val = alertpanel_message_with_disable
				(_("Warning"),
				 _("GnuPG is not installed properly, or needs to be upgraded.\n"
				   "OpenPGP support disabled."));
			if (val & G_ALERTDISABLE)
				prefs_common.gpg_warning = FALSE;
		}
	}

	gpgme_register_idle(idle_function_for_gpgme);
}

void sgpgme_done()
{
        gpgmegtk_free_passphrase();
}

#endif /* USE_GPGME */
