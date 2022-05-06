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

#include "config.h"

#include "w32_reg.h"
#include "utils.h"

gboolean reg_set_value(HKEY root,
	const gchar *subkey,
	const gchar *value,
	DWORD type,
	const BYTE *data,
	DWORD data_size)
{
	DWORD ret;
	HKEY key;
	gchar *tmp;

	ret = RegCreateKeyEx(root, subkey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &key, NULL);
	if (ret != ERROR_SUCCESS) {
		tmp = g_win32_error_message(ret);
		debug_print("RegCreateKeyEx %p \"%s\" had error: \"%s\"\n", root, subkey, tmp);
		g_free(tmp);
		return FALSE;
	}

	ret = RegSetValueEx(key, value, 0, type, data, data_size);
	if (ret != ERROR_SUCCESS) {
		tmp = g_win32_error_message(ret);
		debug_print("RegSetValueEx %p \"%s\" had error: \"%s\"\n", root, subkey, tmp);
		g_free(tmp);
	}

	RegCloseKey(key);
	return (ret == ERROR_SUCCESS);
}

gboolean write_w32_registry_string(HKEY root,
	const gchar *subkey,
	const gchar *value,
	const gchar *data)
{
	return reg_set_value(root, subkey, value, REG_SZ, (BYTE *)data, strlen(data) + 1);
}

gboolean write_w32_registry_dword(HKEY root,
	const gchar *subkey,
	const gchar *value,
	DWORD data)
{
	return reg_set_value(root, subkey, value, REG_DWORD, (BYTE *)&data, sizeof(DWORD));
}

gchar *read_w32_registry_string(HKEY root, const gchar *subkey, const gchar *value)
{
	HKEY hkey;
	DWORD ret;
	DWORD type;
	BYTE *data;
	DWORD data_size;
	gchar *tmp;

	if (subkey == NULL)
		return NULL;

	ret = RegOpenKeyEx(root, subkey, 0, KEY_READ, &hkey);
	if (ret != ERROR_SUCCESS) {
		tmp = g_win32_error_message(ret);
		debug_print("RegOpenKeyEx %p \"%s\" had error: \"%s\"\n", root, subkey, tmp);
		g_free(tmp);
		return NULL;
	}

	// Get the needed buffer size
	ret = RegQueryValueEx(hkey, value, 0, &type, NULL, &data_size);
	if (ret != ERROR_SUCCESS) {
		tmp = g_win32_error_message(ret);
		debug_print("RegQueryValueEx %p \"%s\" \"%s\" had error: \"%s\" when getting buffer size\n",
			root, subkey, value, tmp);
		RegCloseKey(hkey);
		g_free(tmp);
		return NULL;
	} else if (type != REG_SZ) {
		debug_print("RegQueryValueEx %p \"%s\" \"%s\" returned type %lu instead of REG_SZ\n",
			root, subkey, value, type);
		RegCloseKey(hkey);
		return NULL;
	} else if (data_size == 0) {
		debug_print("RegQueryValueEx %p \"%s\" \"%s\" returned data size 0\n",
			root, subkey, value);
		RegCloseKey(hkey);
		return NULL;
	}

	// The raw value is not necessarily NUL-terminated
	data = g_malloc(data_size + 1);

	ret = RegQueryValueEx(hkey, value, 0, NULL, data, &data_size);
	if (ret == ERROR_SUCCESS) {
		data[data_size] = '\0';
	} else {
		tmp = g_win32_error_message(ret);
		debug_print("RegQueryValueEx %p \"%s\" \"%s\" had error: \"%s\"\n",
			root, subkey, value, tmp);
		RegCloseKey(hkey);
		g_free(data);
		g_free(tmp);
		return NULL;
	}

	RegCloseKey(hkey);
	return (gchar *)data;
}
