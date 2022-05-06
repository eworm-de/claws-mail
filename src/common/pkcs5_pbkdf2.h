/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2016 The Claws Mail Team
 *
 * pkcs5_pbkdf2.h - Password-Based Key Derivation Function 2
 *
 * according to the definition of MD5 in RFC 1321 from April 1992.
 * NOTE: This is *not* the same file as the one from glibc
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 3, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __PKCS_PBKDF2_H
#define __PKCS_PBKDF2_H

/* The output will be placed into memory pointed to by key parameter.
 * Memory needs to be pre-allocated with key_len bytes. */
gint pkcs5_pbkdf2(const gchar *pass, size_t pass_len, const guchar *salt,
    size_t salt_len, guchar *key, size_t key_len, guint rounds);

#endif /* __PKCS_PBKDF2_H */
