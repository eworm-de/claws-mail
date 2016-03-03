/*
 * Claws Mail -- a GTK+ based, lightweight, and fast e-mail client
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

#if defined G_OS_UNIX
#include <fcntl.h>
#include <unistd.h>
#elif defined G_OS_WIN32
#include <windows.h>
#include <wincrypt.h>
#endif

#include "common/passcrypt.h"
#include "common/plugin.h"
#include "common/utils.h"
#include "account.h"
#include "alertpanel.h"
#include "inputdialog.h"
#include "password.h"
#include "passwordstore.h"
#include "prefs_common.h"

#ifndef PASSWORD_CRYPTO_OLD
static gchar *_master_passphrase = NULL;

static const gchar *master_passphrase()
{
	gchar *input;
	gboolean end = FALSE;

	if (!prefs_common_get_prefs()->use_master_passphrase) {
		return PASSCRYPT_KEY;
	}

	if (_master_passphrase != NULL) {
		debug_print("Master passphrase is in memory, offering it.\n");
		return _master_passphrase;
	}

	while (!end) {
		input = input_dialog_with_invisible(_("Input master passphrase"),
				_("Input master passphrase"), NULL);

		if (input == NULL) {
			debug_print("Cancel pressed at master passphrase dialog.\n");
			break;
		}

		if (master_passphrase_is_correct(input)) {
			debug_print("Entered master passphrase seems to be correct, remembering it.\n");
			_master_passphrase = input;
			end = TRUE;
		} else {
			alertpanel_error(_("Incorrect master passphrase."));
		}
	}

	return _master_passphrase;
}

const gboolean master_passphrase_is_set()
{
	if (prefs_common_get_prefs()->master_passphrase_hash == NULL
			|| strlen(prefs_common_get_prefs()->master_passphrase_hash) == 0)
		return FALSE;

	return TRUE;
}

const gboolean master_passphrase_is_correct(const gchar *input)
{
	gchar *hash;
	gchar *stored_hash = prefs_common_get_prefs()->master_passphrase_hash;
	const GChecksumType hashtype = G_CHECKSUM_SHA512;
	const gssize hashlen = g_checksum_type_get_length(hashtype);
	gssize stored_len;

	g_return_val_if_fail(input != NULL, FALSE);

	if (stored_hash == NULL)
		return FALSE;

	debug_print("|stored_hash|%s|\n", stored_hash);
	stored_len = strlen(stored_hash);
	g_return_val_if_fail(stored_len == 2*hashlen, FALSE);

	hash = g_compute_checksum_for_string(hashtype, input, -1);

	if (!strncasecmp(hash, stored_hash, stored_len)) {
		g_free(hash);
		return TRUE;
	}
	g_free(hash);

	return FALSE;
}

gboolean master_passphrase_is_entered()
{
	return (_master_passphrase == NULL) ? FALSE : TRUE;
}

void master_passphrase_forget()
{
	/* If master passphrase is currently in memory (entered by user),
	 * get rid of it. User will have to enter the new one again. */
	if (_master_passphrase != NULL) {
		memset(_master_passphrase, 0, strlen(_master_passphrase));
		g_free(_master_passphrase);
	}
	_master_passphrase = NULL;
}

void master_passphrase_change(const gchar *oldp, const gchar *newp)
{
	if (oldp == NULL) {
		/* If oldp is NULL, make sure the user has to enter the
		 * current master passphrase before being able to change it. */
		master_passphrase_forget();
		oldp = master_passphrase();
	}
	g_return_if_fail(oldp != NULL);

	/* Update master passphrase hash in prefs */
	if (prefs_common_get_prefs()->master_passphrase_hash != NULL)
		g_free(prefs_common_get_prefs()->master_passphrase_hash);

	if (newp != NULL) {
		debug_print("Storing hash of new master passphrase\n");
		prefs_common_get_prefs()->master_passphrase_hash =
			g_compute_checksum_for_string(G_CHECKSUM_SHA512, newp, -1);
	} else {
		debug_print("Setting master_passphrase_hash to NULL\n");
		prefs_common_get_prefs()->master_passphrase_hash = NULL;
	}

	/* Now go over all accounts, reencrypting their passwords using
	 * the new master passphrase. */

	if (oldp == NULL)
		oldp = PASSCRYPT_KEY;
	if (newp == NULL)
		newp = PASSCRYPT_KEY;

	debug_print("Reencrypting all account passwords...\n");
	passwd_store_reencrypt_all(oldp, newp);

	/* Now reencrypt all plugins passwords fields 
	 * FIXME: Unloaded plugins won't be able to update their stored passwords
	 */
	plugins_master_passphrase_change(oldp, newp);

	master_passphrase_forget();
}
#endif

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

gchar *password_encrypt_gnutls(const gchar *password,
		const gchar *encryption_passphrase)
{
	/* Another, slightly inferior combination is AES-128-CBC + SHA-256.
	 * Any block cipher in CBC mode with keysize N and a hash algo with
	 * digest length 2*N would do. */
	gnutls_cipher_algorithm_t algo = GNUTLS_CIPHER_AES_256_CBC;
	gnutls_digest_algorithm_t digest = GNUTLS_DIG_SHA512;
	gnutls_cipher_hd_t handle;
	gnutls_datum_t key, iv;
	int ivlen, keylen, digestlen, blocklen, ret, i;
	unsigned char hashbuf[BUFSIZE], *buf, *encbuf, *base, *output;
#if defined G_OS_UNIX
	int rnd;
#elif defined G_OS_WIN32
	HCRYPTPROV rnd;
#endif

	g_return_val_if_fail(password != NULL, NULL);
	g_return_val_if_fail(encryption_passphrase != NULL, NULL);

	ivlen = gnutls_cipher_get_iv_size(algo);
	keylen = gnutls_cipher_get_key_size(algo);
	blocklen = gnutls_cipher_get_block_size(algo);
	digestlen = gnutls_hash_get_len(digest);

	/* Prepare key for cipher - first half of hash of passkey XORed with
	 * the second. */
	memset(&hashbuf, 0, BUFSIZE);
	if ((ret = gnutls_hash_fast(digest, encryption_passphrase,
					strlen(encryption_passphrase), &hashbuf)) < 0) {
		debug_print("Hashing passkey failed: %s\n", gnutls_strerror(ret));
		return NULL;
	}
	for (i = 0; i < digestlen/2; i++) {
		hashbuf[i] = hashbuf[i] ^ hashbuf[i+digestlen/2];
	}

	key.data = malloc(keylen);
	memcpy(key.data, &hashbuf, keylen);
	key.size = keylen;

	/* Prepare our source of random data. */
#if defined G_OS_UNIX
	rnd = open("/dev/urandom", O_RDONLY);
	if (rnd == -1) {
		perror("fopen on /dev/urandom");
#elif defined G_OS_WIN32
	if (!CryptAcquireContext(&rnd, NULL, NULL, PROV_RSA_FULL, 0) &&
			!CryptAcquireContext(&rnd, NULL, NULL, PROV_RSA_FULL, CRYPT_NEWKEYSET)) {
		debug_print("Could not acquire a CSP handle.\n");
#endif
		g_free(key.data);
		g_free(iv.data);
		return NULL;
	}

	/* Prepare random IV for cipher */
	iv.data = malloc(ivlen);
	iv.size = ivlen;
#if defined G_OS_UNIX
	ret = read(rnd, iv.data, ivlen);
	if (ret != ivlen) {
		perror("read into iv");
		close(rnd);
#elif defined G_OS_WIN32
	if (!CryptGenRandom(rnd, ivlen, iv.data)) {
		debug_print("Could not read random data for IV\n");
		CryptReleaseContext(rnd, 0);
#endif
		g_free(key.data);
		g_free(iv.data);
		return NULL;
	}

	/* Initialize the encryption */
	ret = gnutls_cipher_init(&handle, algo, &key, &iv);
	if (ret < 0) {
		g_free(key.data);
		g_free(iv.data);
#if defined G_OS_UNIX
		close(rnd);
#elif defined G_OS_WIN32
		CryptReleaseContext(rnd, 0);
#endif
		return NULL;
	}

	/* Fill buf with one block of random data, our password, pad the
	 * rest with zero bytes. */
	buf = malloc(BUFSIZE + blocklen);
	memset(buf, 0, BUFSIZE);
#if defined G_OS_UNIX
	ret = read(rnd, buf, blocklen);
	if (ret != blocklen) {
		perror("read into buffer");
		close(rnd);
#elif defined G_OS_WIN32
	if (!CryptGenRandom(rnd, blocklen, buf)) {
		debug_print("Could not read random data for IV\n");
		CryptReleaseContext(rnd, 0);
#endif
		g_free(buf);
		g_free(key.data);
		g_free(iv.data);
		gnutls_cipher_deinit(handle);
		return NULL;
	}

	/* We don't need any more random data. */
#if defined G_OS_UNIX
	close(rnd);
#elif defined G_OS_WIN32
	CryptReleaseContext(rnd, 0);
#endif

	memcpy(buf + blocklen, password, strlen(password));

	/* Encrypt into encbuf */
	encbuf = malloc(BUFSIZE + blocklen);
	memset(encbuf, 0, BUFSIZE + blocklen);
	ret = gnutls_cipher_encrypt2(handle, buf, BUFSIZE + blocklen,
			encbuf, BUFSIZE + blocklen);
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
	 * "{algorithm}base64encodedciphertext" */
	base = g_base64_encode(encbuf, BUFSIZE);
	g_free(encbuf);
	output = g_strdup_printf("{%s}%s", gnutls_cipher_get_name(algo), base);
	g_free(base);

	return output;
}

gchar *password_decrypt_gnutls(const gchar *password,
		const gchar *decryption_passphrase)
{
	gchar **tokens, *tmp;
	gnutls_cipher_algorithm_t algo;
	gnutls_digest_algorithm_t digest = GNUTLS_DIG_UNKNOWN;
	gnutls_cipher_hd_t handle;
	gnutls_datum_t key, iv;
	int ivlen, keylen, digestlen, blocklen, ret, i;
	gsize len;
	unsigned char hashbuf[BUFSIZE], *buf;
#if defined G_OS_UNIX
	int rnd;
#elif defined G_OS_WIN32
	HCRYPTPROV rnd;
#endif

	g_return_val_if_fail(password != NULL, NULL);
	g_return_val_if_fail(decryption_passphrase != NULL, NULL);

	tokens = g_strsplit_set(password, "{}", 3);

	/* Parse the string, retrieving algorithm and encrypted data.
	 * We expect "{algorithm}base64encodedciphertext". */
	if (strlen(tokens[0]) != 0 ||
			(algo = gnutls_cipher_get_id(tokens[1])) == GNUTLS_CIPHER_UNKNOWN ||
			strlen(tokens[2]) == 0)
		return NULL;

	/* Our hash algo needs to have digest length twice as long as our
	 * cipher algo's key length. */
	if (algo == GNUTLS_CIPHER_AES_256_CBC) {
		debug_print("Using AES-256-CBC + SHA-512 for decryption\n");
		digest = GNUTLS_DIG_SHA512;
	} else if (algo == GNUTLS_CIPHER_AES_128_CBC) {
		debug_print("Using AES-128-CBC + SHA-256 for decryption\n");
		digest = GNUTLS_DIG_SHA256;
	}
	if (digest == GNUTLS_DIG_UNKNOWN) {
		debug_print("Password is encrypted with unsupported cipher, giving up.\n");
		g_strfreev(tokens);
		return NULL;
	}

	ivlen = gnutls_cipher_get_iv_size(algo);
	keylen = gnutls_cipher_get_key_size(algo);
	blocklen = gnutls_cipher_get_block_size(algo);
	digestlen = gnutls_hash_get_len(digest);

	/* Prepare key for cipher - first half of hash of passkey XORed with
	 * the second. AES-256 has key length 32 and length of SHA-512 hash
	 * is exactly twice that, 64. */
	memset(&hashbuf, 0, BUFSIZE);
	if ((ret = gnutls_hash_fast(digest, decryption_passphrase,
					strlen(decryption_passphrase), &hashbuf)) < 0) {
		debug_print("Hashing passkey failed: %s\n", gnutls_strerror(ret));
		g_strfreev(tokens);
		return NULL;
	}
	for (i = 0; i < digestlen/2; i++) {
		hashbuf[i] = hashbuf[i] ^ hashbuf[i+digestlen/2];
	}

	key.data = malloc(keylen);
	memcpy(key.data, &hashbuf, keylen);
	key.size = keylen;

	/* Prepare our source of random data. */
#if defined G_OS_UNIX
	rnd = open("/dev/urandom", O_RDONLY);
	if (rnd == -1) {
		perror("fopen on /dev/urandom");
#elif defined G_OS_WIN32
	if (!CryptAcquireContext(&rnd, NULL, NULL, PROV_RSA_FULL, 0) &&
			!CryptAcquireContext(&rnd, NULL, NULL, PROV_RSA_FULL, CRYPT_NEWKEYSET)) {
		debug_print("Could not acquire a CSP handle.\n");
#endif
		g_free(key.data);
		g_free(iv.data);
		g_strfreev(tokens);
		return NULL;
	}

	/* Prepare random IV for cipher */
	iv.data = malloc(ivlen);
	iv.size = ivlen;
#if defined G_OS_UNIX
	ret = read(rnd, iv.data, ivlen);
	if (ret != ivlen) {
		perror("read into iv");
		close(rnd);
#elif defined G_OS_WIN32
	if (!CryptGenRandom(rnd, ivlen, iv.data)) {
		debug_print("Could not read random data for IV\n");
		CryptReleaseContext(rnd, 0);
#endif
		g_free(key.data);
		g_free(iv.data);
		g_strfreev(tokens);
		return NULL;
	}

	/* We don't need any more random data. */
#if defined G_OS_UNIX
	close(rnd);
#elif defined G_OS_WIN32
	CryptReleaseContext(rnd, 0);
#endif

	/* Prepare encrypted password string for decryption. */
	tmp = g_base64_decode(tokens[2], &len);
	g_strfreev(tokens);

	/* Initialize the decryption */
	ret = gnutls_cipher_init(&handle, algo, &key, &iv);
	if (ret < 0) {
		debug_print("Cipher init failed: %s\n", gnutls_strerror(ret));
		g_free(key.data);
		g_free(iv.data);
		return NULL;
	}

	buf = malloc(BUFSIZE + blocklen);
	memset(buf, 0, BUFSIZE + blocklen);
	ret = gnutls_cipher_decrypt2(handle, tmp, len,
			buf, BUFSIZE + blocklen);
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

	tmp = g_strndup(buf + blocklen, MIN(strlen(buf + blocklen), BUFSIZE));
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
		encryption_passphrase = master_passphrase();

	return password_encrypt_real(password, encryption_passphrase);
#endif

	return password_encrypt_old(password);
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
		decryption_passphrase = master_passphrase();

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
