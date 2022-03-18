/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2016-2022 the Claws Mail team
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
#include "folder_item_prefs.h"
#include "prefs_account.h"
#include "prefs_common.h"
#include "alertpanel.h"

static gint starting_config_version = 0;

gboolean _version_check(gint ver)
{
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
					NULL, _("_No"), NULL, _("_Yes"),
				        NULL, NULL, ALERTFOCUS_SECOND,
					FALSE, NULL, ALERT_ERROR);
		g_free(msg);

		if (av != G_ALERTDEFAULT)
			return FALSE; /* abort startup */

		return TRUE; /* hic sunt dracones */
	}

	return TRUE;
}

static void _update_config_common(gint version)
{
	debug_print("Updating config version %d to %d.\n", version, version + 1);

	switch (version) {
		case 1:

			/* The autochk_interval preference is now
			 * interpreted as seconds instead of minutes */
			prefs_common.autochk_itv *= 60;

			break;

		default:

			/* NOOP */

			break;
	}
}

static void _update_config_account(PrefsAccount *ac_prefs, gint version)
{
	debug_print("Account '%s': Updating config version from %d to %d.\n",
			ac_prefs->account_name, version, version + 1);

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
			if (ac_prefs->protocol == 1) {
				ac_prefs->protocol = 0;
			} else if (ac_prefs->protocol > 2) {
				/* A_IMAP and above gets bumped down by 2. */
				ac_prefs->protocol -= 2;
			}

			break;

		case 2:

			/* Introducing per-account mail check intervals, and separating
			 * recv_at_getall from autocheck function.
			 *
			 * If recv_at_getall is TRUE, the account's autocheck will be
			 * enabled, following global autocheck interval.
			 *
			 * The account's own autocheck interval will be set to the
			 * same value as the global interval, but will not be used.
			 *
			 * recv_at_getall will remain enabled, but will only be used
			 * to determine whether or not to include this account for
			 * manual 'Get all' check. */
			ac_prefs->autochk_itv = prefs_common_get_prefs()->autochk_itv;
			ac_prefs->autochk_use_custom = FALSE;
			if (ac_prefs->recv_at_getall) {
				ac_prefs->autochk_use_default = TRUE;
			} else {
				ac_prefs->autochk_use_default = FALSE;
			}

			break;

		case 3:
			/* With the introduction of OAuth2 support, the APOP option
			 * (use_apop_auth) has been swallowed into a combobox and
			 * renamed */
			if (ac_prefs->use_apop_auth == 1) {
				ac_prefs->use_pop_auth = 1;
				ac_prefs->pop_auth_type = 1;
			}
			break;
		default:

			/* NOOP */

			break;
	}

	ac_prefs->config_version = version + 1;
}

static void _update_config_password_store(gint version)
{
	debug_print("Password store: Updating config version from %d to %d.\n",
			version, version + 1);

	switch (version) {
		/* nothing here yet */

		default:

			/* NOOP */

			break;
	}
}

static void _update_config_folderlist(gint version)
{
	debug_print("Folderlist: Updating config version from %d to %d.\n",
			version, version + 1);

	switch (version) {
		/* nothing here yet */

		default:

			/* NOOP */

			break;
	}
}

static void _update_config_folder_item(FolderItem *item,
		gint version)
{
	debug_print("Updating config_version from %d to %d.\n",
			version, version + 1);

	switch (version) {
		/* nothing here yet */

		default:

			/* NOOP */

			break;
	}
	item->prefs->config_version = version + 1;
	folder_item_prefs_save_config(item);
}

int prefs_update_config_version_common()
{
	gint ver = prefs_common_get_prefs()->config_version;

	/* Store the starting version number for other components'
	 * migration functions. */
	starting_config_version = ver;

	if (!_version_check(ver))
		return -1;

	debug_print("Starting config update at config_version %d.\n", ver);
	if (ver == CLAWS_CONFIG_VERSION) {
		debug_print("No update necessary, already at latest config_version.\n");
		return 0; /* nothing to do */
	}

	while (ver < CLAWS_CONFIG_VERSION) {
		_update_config_common(ver++);
		prefs_common_get_prefs()->config_version = ver;
	}

	debug_print("Config update done.\n");
	return 1; /* update done */
}

int prefs_update_config_version_accounts()
{
	GList *cur;
	PrefsAccount *ac_prefs;

	for (cur = account_get_list(); cur != NULL; cur = cur->next) {
		ac_prefs = (PrefsAccount *)cur->data;

		if (ac_prefs->config_version == -1) {
			/* There was no config_version stored in the config, let's
			 * assume config_version same as what clawsrc started at
			 * this session, to avoid breaking the configuration by
			 * "upgrading" it unnecessarily. */
			debug_print("Account '%s': config_version not saved, using one from clawsrc: %d\n", ac_prefs->account_name, starting_config_version);
			ac_prefs->config_version = starting_config_version;
		}

		gint ver = ac_prefs->config_version;

		debug_print("Account '%s': Starting config update at config_version %d.\n", ac_prefs->account_name, ver);

		if (!_version_check(ver))
			return -1;

		if (ver == CLAWS_CONFIG_VERSION) {
			debug_print("Account '%s': No update necessary, already at latest config_version.\n", ac_prefs->account_name);
			continue;
		}

		while (ver < CLAWS_CONFIG_VERSION) {
			_update_config_account(ac_prefs, ver++);
		}
	}

	debug_print("Accounts config update done.\n");
	return 1;
}

int prefs_update_config_version_password_store(gint from_version)
{
	gint ver = from_version;

	if (ver == -1) {
		/* There was no config_version stored in the config, let's assume
		 * config_version same as what clawsrc started at this session,
		 * to avoid breaking the configuration by "upgrading" it unnecessarily. */
		debug_print("Password store: config_version not saved, using one from clawsrc: %d\n", starting_config_version);
		ver = starting_config_version;
	}

	debug_print("Starting config update at config_version %d.\n", ver);

	if (!_version_check(ver))
		return -1;

	if (ver == CLAWS_CONFIG_VERSION) {
		debug_print("No update necessary, already at latest config_version.\n");
		return 0; /* nothing to do */
	}

	while (ver < CLAWS_CONFIG_VERSION) {
		_update_config_password_store(ver++);
	}

	debug_print("Passwordstore config update done.\n");
	return 1;
}

int prefs_update_config_version_folderlist(gint from_version)
{
	gint ver = from_version;

	if (ver == -1) {
		/* There was no config_version stored in the config, let's assume
		 * config_version same as what clawsrc started at this session,
		 * to avoid breaking the configuration by "upgrading" it unnecessarily. */
		debug_print("Folderlist: config_version not saved, using one from clawsrc: %d\n", starting_config_version);
		ver = starting_config_version;
	}

	debug_print("Starting config_update at config_version %d,\n", ver);

	if (!_version_check(ver))
		return -1;

	if (ver == CLAWS_CONFIG_VERSION) {
		debug_print("No update necessary, already at latest config_version.\n");
		return 0; /* nothing to do */
	}

	while (ver < CLAWS_CONFIG_VERSION) {
		_update_config_folderlist(ver++);
	}

	debug_print("Folderlist config update done.\n");
	return 1;
}

int prefs_update_config_version_folder_item(FolderItem *item)
{
	gint ver;
	gchar *id;

	cm_return_val_if_fail(item != NULL, 0);

	id = folder_item_get_identifier(item);

	if (item->prefs->config_version == -1) {
		/* There was no config_version stored in the config, let's assume
		 * config_version same as what clawsrc started at this session,
		 * to avoid breaking the configuration by "upgrading" it unnecessarily. */
		debug_print("Folder item '%s': config_version not saved, using one from clawsrc: %d\n", id, starting_config_version);
		item->prefs->config_version = starting_config_version;
	}

	ver = item->prefs->config_version;

	if (!_version_check(ver)) {
		g_free(id);
		return -1;
	}

	if (ver == CLAWS_CONFIG_VERSION) {
		debug_print("Folder item '%s': No update necessary, already at latest config_version %d.\n", id, ver);
		g_free(id);
		return 0; /* nothing to do */
	}

	debug_print("Folder item '%s': starting config_update at version %d.\n",
			id, ver);
	g_free(id);

	while (ver < CLAWS_CONFIG_VERSION) {
		_update_config_folder_item(item, ver++);
	}

	debug_print("Folder item config update done.\n");
	return 0;
}
