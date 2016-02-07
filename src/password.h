/*
 * Claws Mail -- a GTK+ based, lightweight, and fast e-mail client
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

#ifndef __PASSWORD_H
#define __PASSWORD_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>

#ifndef PASSWORD_CRYPTO_OLD
const gboolean master_password_is_set();
const gboolean master_password_is_correct(const gchar *input);
void master_password_forget();
void master_password_change(const gchar *newp);
#endif

/* Wrapper around the old, DES-CBC-broken implementation which
 * returns a newly allocated string for the encrypt/decrypt result.
 * This is for compatibility with with the rest of password-related
 * functions.*/
gchar *password_encrypt_old(const gchar *password);
gchar *password_decrypt_old(const gchar *password);

#ifdef PASSWORD_CRYPTO_GNUTLS
/* GNUTLS implementation */
gchar *password_encrypt_gnutls(const gchar *password,
		const gchar *encryption_password);
gchar *password_decrypt_gnutls(const gchar *password,
		const gchar *decryption_password);
#define password_encrypt_real(n, m) password_encrypt_gnutls(n, m)
#define password_decrypt_real(n, m) password_decrypt_gnutls(n, m)
#endif

/* Wrapper function that will apply best encryption available,
 * and return a string ready to be saved as-is in preferences. */
gchar *password_encrypt(const gchar *password,
		const gchar *encryption_password);

/* This is a wrapper function that looks at the whole string from
 * prefs (e.g. including the leading '!' for old implementation),
 * and tries to do the smart thing. */
gchar *password_decrypt(const gchar *password,
		const gchar *decryption_password);

#endif /* __PASSWORD_H */
