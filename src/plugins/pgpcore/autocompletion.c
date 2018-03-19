/*
 * PGP/Core keyring autocompletion
 *
 * Copyright (C) 2014 Christian Hesse <mail@eworm.de> and the Claws Mail team
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n.h>

#include <gpgme.h>

#include "autocompletion.h"
#include "addr_compl.h"
#include "prefs_gpg.h"
#include "hooks.h"
#include "utils.h"

static gulong autocompletion_hook_id = HOOK_NONE;

static gboolean pgp_autocompletion_hook(gpointer source, gpointer data)
{
	gpgme_ctx_t ctx;
	gpgme_key_t key;
	gpgme_error_t err;
	gpgme_user_id_t uid;
	address_entry *ae;
	GList *addr_list = NULL;
	gint i;

	/* just return if autocompletion is disabled */
	if (!prefs_gpg_get_config()->autocompletion)
		return EXIT_SUCCESS;

	/* initialize */
	gpgme_check_version(NULL);

	if ((err = gpgme_new(&ctx)) == 0) {
		err = gpgme_op_keylist_start(ctx, NULL, 0);

		/* walk the available keys */
		while (err == 0) {
			if ((err = gpgme_op_keylist_next(ctx, &key)) != 0)
				break;

			/* skip keys that are revoked, expired, ... */
			if ((key->revoked || key->expired || key->disabled || key->invalid) == FALSE) {
				uid = key->uids;
				i = 0;

				/* walk all user ids within a key */
				while (uid != NULL) {
					if (uid->email != NULL && *uid->email != 0) {
						ae = g_new0(address_entry, 1);

						ae->address = g_strdup(uid->email);
						addr_compl_add_address1(ae->address, ae);

						if (uid->name != NULL && *uid->name != 0) {
							ae->name = g_strdup(uid->name);
							addr_compl_add_address1(ae->name, ae);
						} else
							ae->name = NULL;

						ae->grp_emails = NULL;

						addr_list = g_list_prepend(addr_list, ae);

						debug_print("%s <%s>\n", uid->name, uid->email);
					}

					if (prefs_gpg_get_config()->autocompletion_limit > 0 &&
							prefs_gpg_get_config()->autocompletion_limit == i)
						break;

					uid = uid->next;
					i++;
				}
			}
			gpgme_key_unref(key);
		}
		gpgme_release(ctx);
	}

	if (gpg_err_code(err) != GPG_ERR_EOF) {
		debug_print("can not list keys: %s\n", gpgme_strerror(err));
		return EXIT_FAILURE;
	}
	*((GList **)source) = addr_list;

	return EXIT_SUCCESS;
}

gboolean autocompletion_done(void)
{
	if (autocompletion_hook_id != HOOK_NONE)
	{
		hooks_unregister_hook(ADDDRESS_COMPLETION_BUILD_ADDRESS_LIST_HOOKLIST, autocompletion_hook_id);

		debug_print("PGP address autocompletion hook unregistered\n");
	}

	return TRUE;
}

gint autocompletion_init(gchar ** error)
{
	if ((autocompletion_hook_id = hooks_register_hook(ADDDRESS_COMPLETION_BUILD_ADDRESS_LIST_HOOKLIST, pgp_autocompletion_hook, NULL)) == HOOK_NONE)
	{
		*error = g_strdup(_("Failed to register PGP address autocompletion hook"));
		return -1;
	}
	debug_print("PGP address autocompletion hook registered\n");

	return EXIT_SUCCESS;
}

