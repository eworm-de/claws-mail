/*
 * Claws Mail -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2016 the Claws Mail team
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

#include "prefs_common.h"

static void _update_config(gint version)
{
	switch (version) {
		case 0:
		default:
			break;
	}
}

void prefs_update_config_version()
{
	gint ver = prefs_common_get_prefs()->config_version;

	debug_print("Starting config update at config_version %d.\n", ver);
	if (ver == CLAWS_CONFIG_VERSION) {
		debug_print("No update necessary, already at latest config_version.\n");
		return;
	}

	while (ver < CLAWS_CONFIG_VERSION) {
		_update_config(ver++);
		prefs_common_get_prefs()->config_version = ver;
	}

	debug_print("Config update done.\n");
}
