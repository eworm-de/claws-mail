/*
 * Claws Mail -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2016 the Claws Mail team
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
#include "config.h"
#include "claws-features.h"
#endif

#ifdef ENABLE_NLS
#include <glib/gi18n.h>
#else
#define _(a) (a)
#define N_(a) (a)
#endif

#include "defs.h"
#include "account.h"
#include "prefs_account.h"
#include "prefs_common.h"
#include "alertpanel.h"

static void _update_config(gint version)
{
	GList *cur;
	PrefsAccount *ac_prefs;

	debug_print("Updating config version %d to %d.\n", version, version + 1);

	switch (version) {
		case 0:

			/* Removing A_APOP and A_RPOP from RecvProtocol enum,
			 * protocol numbers above A_POP3 need to be adjusted.
			 *
			 * In config_version=0:
			 * A_POP3 is 0,
			 * A_APOP is 1,
			 * A_RPOP is 2,
			 * A_IMAP and the rest are from 3 up.
			 * We can't use the macros, since they may change in the
			 * future. Numbers do not change. :) */
			for (cur = account_get_list(); cur != NULL; cur = cur->next) {
				ac_prefs = (PrefsAccount *)cur->data;
				if (ac_prefs->protocol == 1) {
					ac_prefs->protocol = 0;
				} else if (ac_prefs->protocol > 2) {
					/* A_IMAP and above gets bumped down by 2. */
					ac_prefs->protocol -= 2;
				}
			}

			break;

		case 1:

			/* The autochk_interval preference is now
			 * interpreted as seconds instead of minutes */
			prefs_common.autochk_itv *= 60;

			break;

		default:
			break;
	}
}

int prefs_update_config_version()
{
	gint ver = prefs_common_get_prefs()->config_version;

	if (ver > CLAWS_CONFIG_VERSION) {
		gchar *msg;
		gchar *markup;
		AlertValue av;

		markup = g_strdup_printf(
			"<a href=\"%s\"><span underline=\"none\">",
			CONFIG_VERSIONS_URI);
		msg = g_strdup_printf(
			_("Your Claws Mail configuration is from a newer "
			  "version than the version which you are currently "
			  "using.\n\n"
			  "This is not recommended.\n\n"
			  "For further information see the %sClaws Mail "
			  "website%s.\n\n"
			  "Do you want to exit now?"),
			  markup, "</span></a>");
		g_free(markup);
		av = alertpanel_full(_("Configuration warning"), msg,
					GTK_STOCK_NO, GTK_STOCK_YES, NULL,
					FALSE, NULL,
					ALERT_ERROR, G_ALERTALTERNATE);
		g_free(msg);

		if (av != G_ALERTDEFAULT)
			return -1; /* abort startup */

		return 0; /* hic sunt dracones */
	}

	debug_print("Starting config update at config_version %d.\n", ver);
	if (ver == CLAWS_CONFIG_VERSION) {
		debug_print("No update necessary, already at latest config_version.\n");
		return 0; /* nothing to do */
	}

	while (ver < CLAWS_CONFIG_VERSION) {
		_update_config(ver++);
		prefs_common_get_prefs()->config_version = ver;
	}

	debug_print("Config update done.\n");
	return 1; /* update done */
}
