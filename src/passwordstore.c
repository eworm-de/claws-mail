/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2016 The Claws Mail Team
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#include "claws-features.h"
#endif

#ifdef PASSWORD_CRYPTO_GNUTLS
# include <gnutls/gnutls.h>
# include <gnutls/crypto.h>
#endif

#include <glib.h>
#include <glib/gi18n.h>

#include "common/defs.h"
#include "common/utils.h"
#include "passwordstore.h"
#include "password.h"
#include "prefs_common.h"
#include "prefs_gtk.h"
#include "prefs_migration.h"
#include "file-utils.h"

static GSList *_password_store;

/* Finds password block of given type and name in the pwdstore
 * and returns a pointer to it, if it exists.
 * If link parameter is non-null, it is set to the linked list
 * element containing this block. */
static PasswordBlock *_get_block(PasswordBlockType block_type,
		const gchar *block_name, GSList **link)
{
	GSList *item;
	PasswordBlock *block;

	g_return_val_if_fail(block_type < NUM_PWS_TYPES, NULL);
	g_return_val_if_fail(block_name != NULL, NULL);

	for (item = _password_store; item != NULL; item = item->next) {
		block = (PasswordBlock *)item->data;
		if (block->block_type == block_type &&
				!strcmp(block->block_name, block_name)) {
			if (link != NULL)
				*link = item;
			return block;
		}
	}

	return NULL;
}

static gboolean _hash_equal_func(gconstpointer a, gconstpointer b)
{
	if (g_strcmp0((const gchar *)a, (const gchar *)b) == 0)
		return TRUE;
	return FALSE;
}

/* Creates a new, empty block and adds it to the pwdstore. */
static PasswordBlock *_new_block(PasswordBlockType block_type,
		const gchar *block_name)
{
	PasswordBlock *block;

	g_return_val_if_fail(block_type < NUM_PWS_TYPES, NULL);
	g_return_val_if_fail(block_name != NULL, NULL);

	/* First check to see if the block doesn't already exist. */
	if (_get_block(block_type, block_name, NULL)) {
		debug_print("Block (%d/%s) already exists.\n",
				block_type, block_name);
		return NULL;
	}

	/* Let's create an empty block, and add it to pwdstore. */
	block = g_new0(PasswordBlock, 1);
	block->block_type = block_type;
	block->block_name = g_strdup(block_name);
	block->entries = g_hash_table_new_full(g_str_hash,
			(GEqualFunc)_hash_equal_func,
			g_free, g_free);
	debug_print("Created password block (%d/%s)\n",
			block_type, block_name);

	_password_store = g_slist_append(_password_store, block);

	return block;
}

static void _delete_block(PasswordBlock *block)
{
	g_return_if_fail(block != NULL);

	if (block->block_name != NULL)
		g_free(block->block_name);

	if (block->entries != NULL)
		g_hash_table_destroy(block->entries);

	g_free(block);
}

/*************************************************************/

/* Stores a password. */
gboolean passwd_store_set(PasswordBlockType block_type,
		const gchar *block_name,
		const gchar *password_id,
		const gchar *password,
		gboolean encrypted)
{
	const gchar *p;
	PasswordBlock *block;
	gchar *encrypted_password;

	g_return_val_if_fail(block_type < NUM_PWS_TYPES, FALSE);
	g_return_val_if_fail(block_name != NULL, FALSE);
	g_return_val_if_fail(password_id != NULL, FALSE);

	/* Empty password string equals null password for us. */
	if (password == NULL || strlen(password) == 0)
		p = NULL;
	else
		p = password;

	/* find correct block (create if needed) */
	if ((block = _get_block(block_type, block_name, NULL)) == NULL) {
		/* If caller wants to delete a password, and even its block
		 * doesn't exist, we're done. */
		if (p == NULL)
			return TRUE;

		if ((block = _new_block(block_type, block_name)) == NULL) {
			debug_print("Could not create password block (%d/%s)\n",
					block_type, block_name);
			return FALSE;
		}
	}

	if (p == NULL) {
		/* NULL password was passed to us, so delete the entry with
		 * corresponding id, if it exists */
		if (g_hash_table_lookup(block->entries, password_id) != NULL) {
			debug_print("Deleting password for '%s' in block (%d/%s)\n",
					password_id, block_type, block_name);
			g_hash_table_remove(block->entries, password_id);
		}
	} else {
		debug_print("Setting password for '%s' in block (%d/%s)%s\n",
				password_id, block_type, block_name,
				(encrypted ? ", already encrypted" : ""));
		if (!encrypted) {
			/* encrypt password before saving it */
			if ((encrypted_password =
						password_encrypt(p, NULL)) == NULL) {
				debug_print("Could not encrypt password '%s' for block (%d/%s).\n",
						password_id, block_type, block_name);
				return FALSE;
			}
		} else {
			/* password is already in encrypted form already */
			encrypted_password = g_strdup(p);
		}

		/* add encrypted password to the block */
		g_hash_table_insert(block->entries,
				g_strdup(password_id),
				encrypted_password);
	}

	return TRUE;
}

/* Retrieves a password. */
gchar *passwd_store_get(PasswordBlockType block_type,
		const gchar *block_name,
		const gchar *password_id)
{
	PasswordBlock *block;
	gchar *encrypted_password, *password;

	g_return_val_if_fail(block_type < NUM_PWS_TYPES, NULL);
	g_return_val_if_fail(block_name != NULL, NULL);
	g_return_val_if_fail(password_id != NULL, NULL);

	debug_print("Getting password '%s' from block (%d/%s)\n",
			password_id, block_type, block_name);

	/* find correct block */
	if ((block = _get_block(block_type, block_name, NULL)) == NULL) {
		debug_print("Block (%d/%s) not found.\n", block_type, block_name);
		return NULL;
	}

	/* grab pointer to encrypted password */
	if ((encrypted_password =
				g_hash_table_lookup(block->entries, password_id)) == NULL) {
		debug_print("Password '%s' in block (%d/%s) not found.\n",
				password_id, block_type, block_name);
		return NULL;
	}

	/* decrypt password */
	if ((password =
				password_decrypt(encrypted_password, NULL)) == NULL) {
		debug_print("Could not decrypt password '%s' for block (%d/%s).\n",
				password_id, block_type, block_name);
		return NULL;
	}

	/* return decrypted password */
	return password;
}

/* Checks if a password exists in the password store.
 * No decryption happens. */
gboolean passwd_store_has_password(PasswordBlockType block_type,
		const gchar *block_name,
		const gchar *password_id)
{
	PasswordBlock *block;

	g_return_val_if_fail(block_type < NUM_PWS_TYPES, FALSE);
	g_return_val_if_fail(block_name != NULL, FALSE);
	g_return_val_if_fail(password_id != NULL, FALSE);

	/* find correct block */
	if ((block = _get_block(block_type, block_name, NULL)) == NULL) {
		debug_print("Block (%d/%s) not found.\n", block_type, block_name);
		return FALSE;
	}

	/* do we have specified password in this block? */
	if (g_hash_table_lookup(block->entries, password_id) != NULL) {
		return TRUE; /* yes */
	}

	return FALSE; /* no */
}


gboolean passwd_store_delete_block(PasswordBlockType block_type,
		const gchar *block_name)
{
	PasswordBlock *block;
	GSList *link = NULL;

	g_return_val_if_fail(block_type < NUM_PWS_TYPES, FALSE);
	g_return_val_if_fail(block_name != NULL, FALSE);

	debug_print("Deleting block (%d/%s)\n", block_type, block_name);

	/* find correct block */
	if ((block = _get_block(block_type, block_name, &link)) == NULL) {
		debug_print("Block (%d/%s) not found.\n", block_type, block_name);
		return FALSE;
	}

	/* free the block data and remove it from the list */
	_delete_block(block);
	_password_store = g_slist_delete_link(_password_store, link);
	return TRUE;
}

gboolean passwd_store_set_account(gint account_id,
		const gchar *password_id,
		const gchar *password,
		gboolean encrypted)
{
	gchar *uid = g_strdup_printf("%d", account_id);
	gboolean ret = passwd_store_set(PWS_ACCOUNT, uid,
			password_id, password, encrypted);
	g_free(uid);
	return ret;
}

gchar *passwd_store_get_account(gint account_id,
		const gchar *password_id)
{
	gchar *uid = g_strdup_printf("%d", account_id);
	gchar *ret = passwd_store_get(PWS_ACCOUNT, uid, password_id);
	g_free(uid);
	return ret;
}

gboolean passwd_store_has_password_account(gint account_id,
		const gchar *password_id)
{
	gchar *uid = g_strdup_printf("%d", account_id);
	gboolean ret = passwd_store_has_password(PWS_ACCOUNT, uid, password_id);
	g_free(uid);
	return ret;
}

/* Reencrypts all stored passwords. */
void passwd_store_reencrypt_all(const gchar *old_mpwd,
		const gchar *new_mpwd)
{
	PasswordBlock *block;
	GSList *item;
	GList *keys, *eitem;
	gchar *encrypted_password, *decrypted_password, *key;

	g_return_if_fail(old_mpwd != NULL);
	g_return_if_fail(new_mpwd != NULL);

	for (item = _password_store; item != NULL; item = item->next) {
		block = (PasswordBlock *)item->data;
		if (block == NULL)
			continue; /* Just in case. */

		debug_print("Reencrypting passwords in block (%d/%s).\n",
				block->block_type, block->block_name);

		if (block->entries == NULL || g_hash_table_size(block->entries) == 0)
			continue;

		keys = g_hash_table_get_keys(block->entries);
		for (eitem = keys; eitem != NULL; eitem = eitem->next) {
			key = (gchar *)eitem->data;
			if ((encrypted_password =
						g_hash_table_lookup(block->entries, key)) == NULL)
				continue;

			if ((decrypted_password =
						password_decrypt(encrypted_password, old_mpwd)) == NULL) {
				debug_print("Reencrypt: couldn't decrypt password for '%s'.\n", key);
				continue;
			}

			encrypted_password = password_encrypt(decrypted_password, new_mpwd);
			memset(decrypted_password, 0, strlen(decrypted_password));
			g_free(decrypted_password);
			if (encrypted_password == NULL) {
				debug_print("Reencrypt: couldn't encrypt password for '%s'.\n", key);
				continue;
			}

			g_hash_table_insert(block->entries, g_strdup(key), encrypted_password);
		}

		g_list_free(keys);
	}

	debug_print("Reencrypting done.\n");
}

static gint _write_to_file(FILE *fp)
{
	PasswordBlock *block;
	GSList *item;
	GList *keys, *eitem;
	gchar *typestr, *line, *key, *pwd;

	/* Write out the config_version */
	line = g_strdup_printf("[config_version:%d]\n", CLAWS_CONFIG_VERSION);
	if (claws_fputs(line, fp) == EOF) {
		FILE_OP_ERROR("password store, config_version", "claws_fputs");
		g_free(line);
		return -1;
	}
	g_free(line);

	/* Add a newline if needed */
	if (_password_store != NULL && claws_fputs("\n", fp) == EOF) {
		FILE_OP_ERROR("password store", "claws_fputs");
		return -1;
	}

	/* Write out each password store block */
	for (item = _password_store; item != NULL; item = item->next) {
		block = (PasswordBlock*)item->data;
		if (block == NULL)
			continue; /* Just in case. */

		/* Do not save empty blocks. */
		if (block->entries == NULL || g_hash_table_size(block->entries) == 0)
			continue;

		/* Prepare the section header string and write it out. */
		typestr = NULL;
		if (block->block_type == PWS_CORE) {
			typestr = "core";
		} else if (block->block_type == PWS_ACCOUNT) {
			typestr = "account";
		} else if (block->block_type == PWS_PLUGIN) {
			typestr = "plugin";
		}
		line = g_strdup_printf("[%s:%s]\n", typestr, block->block_name);

		if (claws_fputs(line, fp) == EOF) {
			FILE_OP_ERROR("password store", "claws_fputs");
			g_free(line);
			return -1;
		}
		g_free(line);

		/* Now go through all passwords in the block and write each out. */
		keys = g_hash_table_get_keys(block->entries);
		for (eitem = keys; eitem != NULL; eitem = eitem->next) {
			key = (gchar *)eitem->data;
			if ((pwd = g_hash_table_lookup(block->entries, key)) == NULL)
				continue;

			line = g_strdup_printf("%s %s\n", key, pwd);
			if (claws_fputs(line, fp) == EOF) {
				FILE_OP_ERROR("password store", "claws_fputs");
				g_free(line);
				return -1;
			}
			g_free(line);
		}
		g_list_free(keys);

		/* Add a separating new line if there is another block remaining */
		if (item->next != NULL && claws_fputs("\n", fp) == EOF) {
			FILE_OP_ERROR("password store", "claws_fputs");
			return -1;
		}

	}

	return 1;
}

void passwd_store_write_config(void)
{
	gchar *rcpath;
	PrefFile *pfile;

	debug_print("Writing password store...\n");

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
			PASSWORD_STORE_RC, NULL);

	if ((pfile = prefs_write_open(rcpath)) == NULL) {
		g_warning("failed to open password store file for writing");
		g_free(rcpath);
		return;
	}

	g_free(rcpath);

	if (_write_to_file(pfile->fp) < 0) {
		g_warning("failed to write password store to file");
		prefs_file_close_revert(pfile);
	} else if (prefs_file_close(pfile) < 0) {
		g_warning("failed to properly close password store file after writing");
	}
}

int passwd_store_read_config(void)
{
	gchar *rcpath, *contents, **lines, **line, *typestr, *name;
	GError *error = NULL;
	guint i = 0;
	PasswordBlock *block = NULL;
	PasswordBlockType type;
	gboolean reading_config_version = FALSE;
	gint config_version = -1;

	/* TODO: passwd_store_clear(); */

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
			PASSWORD_STORE_RC, NULL);

	debug_print("Reading password store from file '%s'\n", rcpath);

	if (!g_file_test(rcpath, G_FILE_TEST_EXISTS)) {
		debug_print("File does not exist, looks like a new configuration.\n");
		g_free(rcpath);
		return 0;
	}

	if (!g_file_get_contents(rcpath, &contents, NULL, &error)) {
		g_warning("couldn't read password store from file: %s", error->message);
		g_error_free(error);
		g_free(rcpath);
		return -1;
	}
	g_free(rcpath);

	lines = g_strsplit(contents, "\n", -1);

	g_free(contents);

	while (lines[i] != NULL) {
		if (*lines[i] == '[') {
			/* Beginning of a new block */
			line = g_strsplit_set(lines[i], "[:]", -1);
			if (line[0] != NULL && strlen(line[0]) == 0
					&& line[1] != NULL && strlen(line[1]) > 0
					&& line[2] != NULL && strlen(line[2]) > 0
					&& line[3] != NULL && strlen(line[3]) == 0) {
				typestr = line[1];
				name = line[2];
				if (!strcmp(typestr, "core")) {
					type = PWS_CORE;
				} else if (!strcmp(typestr, "account")) {
					type = PWS_ACCOUNT;
				} else if (!strcmp(typestr, "plugin")) {
					type = PWS_PLUGIN;
				} else if (!strcmp(typestr, "config_version")) {
					reading_config_version = TRUE;
					config_version = atoi(name);
				} else {
					debug_print("Unknown password block type: '%s'\n", typestr);
					g_strfreev(line);
					i++; continue;
				}

				if (reading_config_version) {
					if (config_version < 0) {
						debug_print("config_version:%d looks invalid, ignoring it\n",
								config_version);
						config_version = -1; /* set to default value if missing */
						g_strfreev(line);
						i++; continue;
					}
					debug_print("config_version in file is %d\n", config_version);
					reading_config_version = FALSE;
				} else {
					if ((block = _new_block(type, name)) == NULL) {
						debug_print("Duplicate password block, ignoring: (%d/%s)\n",
								type, name);
						g_strfreev(line);
						i++; continue;
					}
				}
			}
			g_strfreev(line);
		} else if (strlen(lines[i]) > 0 && block != NULL) {
			/* If we have started a password block, test for a
			 * "password_id = password" line. */
			line = g_strsplit(lines[i], " ", -1);
			if (line[0] != NULL && strlen(line[0]) > 0
					&& line[1] != NULL && strlen(line[1]) > 0
					&& line[2] == NULL) {
				debug_print("Adding password '%s'\n", line[0]);
				g_hash_table_insert(block->entries,
						g_strdup(line[0]), g_strdup(line[1]));
			}
			g_strfreev(line);
		}
		i++;
	}
	g_strfreev(lines);

	if (prefs_update_config_version_password_store(config_version) < 0) {
		debug_print("Password store configuration file version upgrade failed\n");
		return -2;
	}

	return g_slist_length(_password_store);
}
