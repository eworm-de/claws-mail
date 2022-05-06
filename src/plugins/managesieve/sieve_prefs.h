/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2004-2015 the Claws Mail team
 * Copyright (C) 2014-2015 Charles Lehner
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

#ifndef SIEVE_PREFS_H
#define SIEVE_PREFS_H

typedef struct SieveConfig SieveConfig;
typedef struct SieveAccountConfig SieveAccountConfig;

#include "prefs_account.h"
#include "managesieve.h"

struct SieveConfig
{
	gint		manager_win_width;
	gint		manager_win_height;
};

struct SieveAccountConfig
{
	gboolean	enable;
	gboolean	use_host;
	gchar 		*host;
	gboolean	use_port;
	gushort		port;
	SieveAuth	auth;
	SieveAuthType	auth_type;
	SieveTLSType	tls_type;
	gchar		*userid;
};

extern SieveConfig sieve_config;

void sieve_prefs_init(void);
void sieve_prefs_done(void);
struct SieveAccountConfig *sieve_prefs_account_get_config(
		PrefsAccount *account);
void sieve_prefs_account_set_config(
		PrefsAccount *account, SieveAccountConfig *config);
void sieve_prefs_account_free_config(SieveAccountConfig *config);

#endif /* SIEVE_PREFS_H */
