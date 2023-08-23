/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2016-2023 The Claws Mail Team
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

#ifdef PASSWORD_CRYPTO_GNUTLS
# include <gnutls/gnutls.h>
# include <gnutls/crypto.h>
#endif

#include <glib.h>
#include <glib/gi18n.h>

#if defined G_OS_UNIX
#include <fcntl.h>
#include <unistd.h>
#elif defined G_OS_WIN32
#include <windows.h>
#include <wincrypt.h>
#endif

#include "common/passcrypt.h"
#include "common/plugin.h"
#include "common/pkcs5_pbkdf2.h"
#include "common/timing.h"
#include "common/utils.h"
#include "account.h"
#include "alertpanel.h"
#include "inputdialog.h"
#include "password.h"
#include "passwordstore.h"
#include "prefs_common.h"

#ifndef PASSWORD_CRYPTO_OLD
static gchar *_primary_passphrase = NULL;

/* Length of stored key derivation, before base64. */
#define KD_LENGTH 64

/* Length of randomly generated and saved salt, used for key derivation.
 * Also before base64. */
#define KD_SALT_LENGTH 64

static void _generate_salt()
{
	guchar salt[KD_SALT_LENGTH];

	if (prefs_common_get_prefs()->primary_passphrase_salt != NULL) {
		g_free(prefs_common_get_prefs()->primary_passphrase_salt);
	}

	if (!get_random_bytes(salt, KD_SALT_LENGTH)) {
		debug_print("Could not get random bytes for kd salt.\n");
		return;
	}

	prefs_common_get_prefs()->primary_passphrase_salt =
		g_base64_encode(salt, KD_SALT_LENGTH);
}

#undef KD_SALT_LENGTH

static guchar *_make_key_deriv(const gchar *passphrase, guint rounds,
		guint length)
{
	guchar *kd, *salt;
	gchar *saltpref = prefs_common_get_prefs()->primary_passphrase_salt;
	gsize saltlen;
	gint ret;

	/* Grab our salt, generating and saving a new random one if needed. */
	if (saltpref == NULL || strlen(saltpref) == 0) {
		_generate_salt();
		saltpref = prefs_common_get_prefs()->primary_passphrase_salt;
	}
	salt = g_base64_decode(saltpref, &saltlen);
	kd = g_malloc0(length);

	START_TIMING("PBKDF2");
	ret = pkcs5_pbkdf2(passphrase, strlen(passphrase), salt, saltlen,
			kd, length, rounds);
	END_TIMING();

	g_free(salt);

	if (ret == 0) {
		return kd;
	}

	g_free(kd);
	return NULL;
}

const gchar *primary_passphrase()
{
	gchar *input;
	gboolean end = FALSE;

	if (!prefs_common_get_prefs()->use_primary_passphrase) {
		return PASSCRYPT_KEY;
	}

	if (_primary_passphrase != NULL) {
		debug_print("Primary passphrase is in memory, offering it.\n");
		return _primary_passphrase;
	}

	while (!end) {
		input = input_dialog_with_invisible(_("Input primary passphrase"),
				_("Input primary passphrase"), NULL);

		if (input == NULL) {
			debug_print("Cancel pressed at primary passphrase dialog.\n");
			break;
		}

		if (primary_passphrase_is_correct(input)) {
			debug_print("Entered primary passphrase seems to be correct, remembering it.\n");
			_primary_passphrase = input;
			end = TRUE;
		} else {
			alertpanel_error(_("Incorrect primary passphrase."));
		}
	}

	return _primary_passphrase;
}

gboolean primary_passphrase_is_set()
{
	if (prefs_common_get_prefs()->primary_passphrase == NULL
			|| strlen(prefs_common_get_prefs()->primary_passphrase) == 0)
		return FALSE;

	return TRUE;
}

gboolean primary_passphrase_is_correct(const gchar *input)
{
	guchar *kd, *input_kd;
	gchar **tokens;
	gchar *stored_kd = prefs_common_get_prefs()->primary_passphrase;
	gsize kd_len;
	guint rounds = 0;
	gint ret;

	g_return_val_if_fail(stored_kd != NULL && strlen(stored_kd) > 0, FALSE);
	g_return_val_if_fail(input != NULL, FALSE);

	tokens = g_strsplit_set(stored_kd, "{}", 3);
	if (tokens[0] == NULL ||
			strlen(tokens[0]) != 0 || /* nothing before { */
			tokens[1] == NULL ||
			strncmp(tokens[1], "PBKDF2-HMAC-SHA1,", 17) || /* correct tag */
			strlen(tokens[1]) <= 17 || /* something after , */
			(rounds = atoi(tokens[1] + 17)) <= 0 || /* valid rounds # */
			tokens[2] == NULL ||
			strlen(tokens[2]) == 0) { /* string continues after } */
		debug_print("Mangled primary_passphrase format in config, can not use it.\n");
		g_strfreev(tokens);
		return FALSE;
	}

	stored_kd = tokens[2];
	kd = g_base64_decode(stored_kd, &kd_len); /* should be 64 */
	g_strfreev(tokens);

	if (kd_len != KD_LENGTH) {
		debug_print("primary_passphrase is %"G_GSIZE_FORMAT" bytes long, should be %d.\n",
				kd_len, KD_LENGTH);
		g_free(kd);
		return FALSE;
	}

	input_kd = _make_key_deriv(input, rounds, KD_LENGTH);
	ret = memcmp(kd, input_kd, kd_len);

	g_free(input_kd);
	g_free(kd);

	if (ret == 0)
		return TRUE;

	return FALSE;
}

gboolean primary_passphrase_is_entered()
{
	return (_primary_passphrase == NULL) ? FALSE : TRUE;
}

void primary_passphrase_forget()
{
	/* If primary passphrase is currently in memory (entered by user),
	 * get rid of it. User will have to enter the new one again. */
	if (_primary_passphrase != NULL) {
		memset(_primary_passphrase, 0, strlen(_primary_passphrase));
		g_free(_primary_passphrase);
		_primary_passphrase = NULL;
	}
}

void primary_passphrase_change(const gchar *oldp, const gchar *newp)
{
	guchar *kd;
	gchar *base64_kd;
	guint rounds = prefs_common_get_prefs()->primary_passphrase_pbkdf2_rounds;

	g_return_if_fail(rounds > 0);

	if (oldp == NULL) {
		/* If oldp is NULL, make sure the user has to enter the
		 * current primary passphrase before being able to change it. */
		primary_passphrase_forget();
		oldp = primary_passphrase();
	}
	g_return_if_fail(oldp != NULL);

	/* Update primary passphrase hash in prefs */
	if (prefs_common_get_prefs()->primary_passphrase != NULL)
		g_free(prefs_common_get_prefs()->primary_passphrase);

	if (newp != NULL) {
		debug_print("Storing key derivation of new primary passphrase\n");
		kd = _make_key_deriv(newp, rounds, KD_LENGTH);
		base64_kd = g_base64_encode(kd, 64);
		prefs_common_get_prefs()->primary_passphrase =
			g_strdup_printf("{PBKDF2-HMAC-SHA1,%d}%s", rounds, base64_kd);
		g_free(kd);
		g_free(base64_kd);
	} else {
		debug_print("Setting primary_passphrase to NULL\n");
		prefs_common_get_prefs()->primary_passphrase = NULL;
	}

	/* Now go over all accounts, reencrypting their passwords using
	 * the new primary passphrase. */

	if (oldp == NULL)
		oldp = PASSCRYPT_KEY;
	if (newp == NULL)
		newp = PASSCRYPT_KEY;

	debug_print("Reencrypting all account passwords...\n");
	passwd_store_reencrypt_all(oldp, newp);

	primary_passphrase_forget();
}
#endif

#ifdef PASSWORD_CRYPTO_OLD
gchar *password_encrypt_old(const gchar *password)
{
	if (!password || strlen(password) == 0) {
		return NULL;
	}

	gchar *encrypted = g_strdup(password);
	gchar *encoded, *result;
	gsize len = strlen(password);

	passcrypt_encrypt(encrypted, len);
	encoded = g_base64_encode(encrypted, len);
	g_free(encrypted);
	result = g_strconcat("!", encoded, NULL);
	g_free(encoded);

	return result;
}
#endif

/* Decryption is still needed for supporting migration of old
 * configurations to newer encryption mechanisms. */
gchar *password_decrypt_old(const gchar *password)
{
	if (!password || strlen(password) == 0) {
		return NULL;
	}

	if (*password != '!' || strlen(password) < 2) {
		return NULL;
	}

	gsize len;
	gchar *decrypted = g_base64_decode(password + 1, &len);

	passcrypt_decrypt(decrypted, len);
	return decrypted;
}

#ifdef PASSWORD_CRYPTO_GNUTLS
#define BUFSIZE 128

/* Since we can't count on having GnuTLS new enough to have
 * gnutls_cipher_get_iv_size(), we hardcode the IV length for now. */
#define IVLEN 16

gchar *password_encrypt_gnutls(const gchar *password,
		const gchar *encryption_passphrase)
{
	gnutls_cipher_algorithm_t algo = GNUTLS_CIPHER_AES_256_CBC;
	gnutls_cipher_hd_t handle;
	gnutls_datum_t key, iv;
	int keylen, blocklen, ret, len, i;
	unsigned char *buf, *encbuf, *base, *output;
	guint rounds = prefs_common_get_prefs()->primary_passphrase_pbkdf2_rounds;

	g_return_val_if_fail(password != NULL, NULL);
	g_return_val_if_fail(encryption_passphrase != NULL, NULL);

/*	ivlen = gnutls_cipher_get_iv_size(algo);*/
	keylen = gnutls_cipher_get_key_size(algo);
	blocklen = gnutls_cipher_get_block_size(algo);
/*	digestlen = gnutls_hash_get_len(digest); */

	/* Take the passphrase and compute a key derivation of suitable
	 * length to be used as encryption key for our block cipher. */
	key.data = _make_key_deriv(encryption_passphrase, rounds, keylen);
	key.size = keylen;

	/* Prepare random IV for cipher */
	iv.data = malloc(IVLEN);
	iv.size = IVLEN;
	if (!get_random_bytes(iv.data, IVLEN)) {
		g_free(key.data);
		g_free(iv.data);
		return NULL;
	}

	/* Initialize the encryption */
	ret = gnutls_cipher_init(&handle, algo, &key, &iv);
	if (ret < 0) {
		g_free(key.data);
		g_free(iv.data);
		return NULL;
	}

	/* Find out how big buffer (in multiples of BUFSIZE)
	 * we need to store the password. */
	i = 1;
	len = strlen(password);
	while(len >= i * BUFSIZE)
		i++;
	len = i * BUFSIZE;

	/* Fill buf with one block of random data, our password, pad the
	 * rest with zero bytes. */
	buf = malloc(len + blocklen);
	memset(buf, 0, len + blocklen);
	if (!get_random_bytes(buf, blocklen)) {
		g_free(buf);
		g_free(key.data);
		g_free(iv.data);
		gnutls_cipher_deinit(handle);
		return NULL;
	}

	memcpy(buf + blocklen, password, strlen(password));

	/* Encrypt into encbuf */
	encbuf = malloc(len + blocklen);
	memset(encbuf, 0, len + blocklen);
	ret = gnutls_cipher_encrypt2(handle, buf, len + blocklen,
			encbuf, len + blocklen);
	if (ret < 0) {
		g_free(key.data);
		g_free(iv.data);
		g_free(buf);
		g_free(encbuf);
		gnutls_cipher_deinit(handle);
		return NULL;
	}

	/* Cleanup */
	gnutls_cipher_deinit(handle);
	g_free(key.data);
	g_free(iv.data);
	g_free(buf);

	/* And finally prepare the resulting string:
	 * "{algorithm,rounds}base64encodedciphertext" */
	base = g_base64_encode(encbuf, len + blocklen);
	g_free(encbuf);
	output = g_strdup_printf("{%s,%d}%s",
			gnutls_cipher_get_name(algo), rounds, base);
	g_free(base);

	return output;
}

gchar *password_decrypt_gnutls(const gchar *password,
		const gchar *decryption_passphrase)
{
	gchar **tokens, *tmp;
	gnutls_cipher_algorithm_t algo;
	gnutls_cipher_hd_t handle;
	gnutls_datum_t key, iv;
	int keylen, blocklen, ret;
	gsize len;
	unsigned char *buf;
	guint rounds;
	size_t commapos;
	gboolean valid_utf8;

	g_return_val_if_fail(password != NULL, NULL);
	g_return_val_if_fail(decryption_passphrase != NULL, NULL);

	tokens = g_strsplit_set(password, "{}", 3);

	/* Parse the string, retrieving algorithm and encrypted data.
	 * We expect "{algorithm,rounds}base64encodedciphertext". */
	if (tokens[0] == NULL || strlen(tokens[0]) != 0 ||
			tokens[1] == NULL || strlen(tokens[1]) == 0 ||
			tokens[2] == NULL || strlen(tokens[2]) == 0) {
		debug_print("Garbled password string.\n");
		g_strfreev(tokens);
		return NULL;
	}

	commapos = strcspn(tokens[1], ",");
	if (commapos == strlen(tokens[1]) || commapos == 0) {
		debug_print("Garbled algorithm substring.\n");
		g_strfreev(tokens);
		return NULL;
	}

	buf = g_strndup(tokens[1], commapos);
	if ((algo = gnutls_cipher_get_id(buf)) == GNUTLS_CIPHER_UNKNOWN) {
		debug_print("Password string has unknown algorithm: '%s'\n", buf);
		g_free(buf);
		g_strfreev(tokens);
		return NULL;
	}
	g_free(buf);

	if ((rounds = atoi(tokens[1] + commapos + 1)) <= 0) {
		debug_print("Invalid number of rounds: %d\n", rounds);
		g_strfreev(tokens);
		return NULL;
	}

/*	ivlen = gnutls_cipher_get_iv_size(algo); */
	keylen = gnutls_cipher_get_key_size(algo);
	blocklen = gnutls_cipher_get_block_size(algo);
/*	digestlen = gnutls_hash_get_len(digest); */

	/* Take the passphrase and compute a key derivation of suitable
	 * length to be used as encryption key for our block cipher. */
	key.data = _make_key_deriv(decryption_passphrase, rounds, keylen);
	key.size = keylen;

	/* Prepare random IV for cipher */
	iv.data = malloc(IVLEN);
	iv.size = IVLEN;
	if (!get_random_bytes(iv.data, IVLEN)) {
		g_free(key.data);
		g_free(iv.data);
		g_strfreev(tokens);
		return NULL;
	}

	/* Prepare encrypted password string for decryption. */
	tmp = g_base64_decode(tokens[2], &len);
	g_strfreev(tokens);
	if (tmp == NULL || len == 0) {
		debug_print("Failed base64-decoding of stored password string\n");
		g_free(key.data);
		g_free(iv.data);
		if (tmp != NULL)
			g_free(tmp);
		return NULL;
	}
	debug_print("Encrypted password string length: %"G_GSIZE_FORMAT"\n", len);

	/* Initialize the decryption */
	ret = gnutls_cipher_init(&handle, algo, &key, &iv);
	if (ret < 0) {
		debug_print("Cipher init failed: %s\n", gnutls_strerror(ret));
		g_free(key.data);
		g_free(iv.data);
		g_free(tmp);
		return NULL;
	}

	/* Allocate the buffer to store decrypted plaintext in. */
	buf = malloc(len);
	memset(buf, 0, len);

	/* Decrypt! */
	ret = gnutls_cipher_decrypt2(handle, tmp, len,
			buf, len);
	g_free(tmp);
	if (ret < 0) {
		debug_print("Decryption failed: %s\n", gnutls_strerror(ret));
		g_free(key.data);
		g_free(iv.data);
		g_free(buf);
		gnutls_cipher_deinit(handle);
		return NULL;
	}

	/* Cleanup */
	gnutls_cipher_deinit(handle);
	g_free(key.data);
	g_free(iv.data);

	/* 'buf+blocklen' should now be pointing to the plaintext
	 * password string.
	 * (The first block contains random data from the IV.)
	 *
	 * At this point, it should be a valid UTF-8 string. Let's make sure. */

	/* First, let's assume there's just garbage and play it safe
	 * by looking for a first NULL byte within the decrypted range.
	 * (We could really use g_strchr_len() here instead, but Glib
	 * doesn't have that.) */
	if (!g_strstr_len(buf + blocklen, len - blocklen, "\0")) {
		debug_print("Could not find a NULL byte in the decrypted password.\n");
		valid_utf8 = FALSE;
	} else {
		/* There is a NULL byte, we can rely on strlen() returning
		 * a sane value, so we don't read past the end of the allocated
		 * buffer. */
		valid_utf8 = g_utf8_validate(buf + blocklen, strlen(buf + blocklen), NULL);
	}

	if (!valid_utf8)
		debug_print("Decrypted password is not a valid UTF-8 string!\n");
	cm_return_val_if_fail(valid_utf8, NULL);

	tmp = g_strndup(buf + blocklen, strlen(buf + blocklen));
	memset(buf, 0, len);
	g_free(buf);

	return tmp;
}

#undef BUFSIZE

#endif

gchar *password_encrypt(const gchar *password,
		const gchar *encryption_passphrase)
{
	if (password == NULL || strlen(password) == 0) {
		return NULL;
	}

#ifndef PASSWORD_CRYPTO_OLD
	if (encryption_passphrase == NULL)
		encryption_passphrase = primary_passphrase();

	return password_encrypt_real(password, encryption_passphrase);
#else
	return password_encrypt_old(password);
#endif
}

gchar *password_decrypt(const gchar *password,
		const gchar *decryption_passphrase)
{
	if (password == NULL || strlen(password) == 0) {
		return NULL;
	}

	/* First, check if the password was possibly decrypted using old,
	 * obsolete method */
	if (*password == '!') {
		debug_print("Trying to decrypt password using the old method...\n");
		return password_decrypt_old(password);
	}

	/* Try available crypto backend */
#ifndef PASSWORD_CRYPTO_OLD
	if (decryption_passphrase == NULL)
		decryption_passphrase = primary_passphrase();

	if (*password == '{') {
		debug_print("Trying to decrypt password...\n");
		return password_decrypt_real(password, decryption_passphrase);
	}
#endif

	/* Fallback, in case the configuration is really old and
	 * stored password in plaintext */
	debug_print("Assuming password was stored plaintext, returning it unchanged\n");
	return g_strdup(password);
}
