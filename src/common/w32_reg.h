/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2021 the Claws Mail team and Hiroyuki Yamamoto
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

#ifndef __W32_REG_H__
#define __W32_REG_H__

#include <windows.h>
#include <glib.h>

gboolean write_w32_registry_string(HKEY root,
	const gchar *subkey,
	const gchar *value,
	const gchar *data);

gboolean write_w32_registry_dword(HKEY root,
	const gchar *subkey,
	const gchar *value,
	DWORD data);

// Caller should deallocate the return value with g_free()
gchar *read_w32_registry_string(HKEY root,
	const gchar *subkey,
	const gchar *value);

#endif
