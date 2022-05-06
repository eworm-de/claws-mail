/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2016 Claws Mail team
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
 *
 */

#ifndef __PASSWORDSTORE_H
#define __PASSWORDSTORE_H

#include <glib.h>

typedef enum {
	PWS_CORE,
	PWS_ACCOUNT,
	PWS_PLUGIN,
	NUM_PWS_TYPES
} PasswordBlockType;

typedef struct _PasswordBlock {
	PasswordBlockType block_type;
	gchar *block_name;
	GHashTable *entries;
} PasswordBlock;

/* Saves a password into a chosen password block.
 * The block is created if it doesn't exist.
 * block_type - from PasswordBlockType
 * block_name - name of the block
 * password_id - ID of the saved password
 * password - the actual password string
 * encrypted - TRUE if the password string is in an already encrypted form
 *             (useful mostly for migration from accountrc)
 *
 * Function will make a copy of the strings where necessary and doesn't
 * take ownership of any of the passed arguments. */
gboolean passwd_store_set(PasswordBlockType block_type,
		const gchar *block_name,
		const gchar *password_id,
		const gchar *password,
		gboolean encrypted);

/* Retrieves a password. Returned string should be freed by the caller. */
gchar *passwd_store_get(PasswordBlockType block_type,
		const gchar *block_name,
		const gchar *password_id);

/* Returns TRUE if such password exists in the password store,
 * false otherwise. No decryption happens. */
gboolean passwd_store_has_password(PasswordBlockType block_type,
		const gchar *block_name,
		const gchar *password_id);

gboolean passwd_store_delete_block(PasswordBlockType block_type,
		const gchar *block_name);

/* Reencrypts all stored passwords using new_mpwd as an encryption
 * password. */
void passwd_store_reencrypt_all(const gchar *old_mpwd,
		const gchar *new_mpwd);

/* Writes/reads password store to/from file. */
void passwd_store_write_config(void);
int passwd_store_read_config(void);

/* Convenience wrappers for handling account passwords.
 * (This is to save some boilerplate code converting account_id to
 * a string and freeing the string afterwards.) */
gboolean passwd_store_set_account(gint account_id,
		const gchar *password_id,
		const gchar *password,
		gboolean encrypted);
gchar *passwd_store_get_account(gint account_id,
		const gchar *password_id);
gboolean passwd_store_has_password_account(gint account_id,
		const gchar *password_id);

/* Macros for standard, predefined password IDs. */
#define PWS_ACCOUNT_RECV      "recv"
#define PWS_ACCOUNT_SEND      "send"
#define PWS_ACCOUNT_RECV_CERT "recv_cert"
#define PWS_ACCOUNT_SEND_CERT "send_cert"
#define PWS_ACCOUNT_PROXY_PASS "proxy_pass"
#define PWS_ACCOUNT_OAUTH2_AUTH "oauth2_auth"
#define PWS_ACCOUNT_OAUTH2_REFRESH "oauth2_refresh"
#define PWS_ACCOUNT_OAUTH2_EXPIRY "oauth2_access_expiry"
#define PWS_CORE_PROXY "proxy"
#define PWS_CORE_PROXY_PASS "proxy_pass"

#endif /* __PASSWORDSTORE_H */
