/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2009-2018 Ricardo Mones and the Claws Mail Team
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __ADDRESS_KEEPER_PREFS__
#define __ADDRESS_KEEPER_PREFS__

#include <glib.h>

typedef struct _AddressKeeperPrefs AddressKeeperPrefs;

struct _AddressKeeperPrefs
{
	gchar		*addressbook_folder;
	gboolean	keep_to_addrs;
	gboolean	keep_cc_addrs;
	gboolean	keep_bcc_addrs;
	gchar		*block_matching_addrs;
};

extern AddressKeeperPrefs addkeeperprefs;
void address_keeper_prefs_init(void);
void address_keeper_prefs_done(void);

#endif
