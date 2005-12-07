/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2002 Hiroyuki Yamamoto
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <string.h>

#if HAVE_LOCALE_H
#  include <locale.h>
#endif


#include "prefs_common.h"
#include "manual.h"
#include "utils.h"

static gchar *get_language()
{
	gchar *language;
	gchar *c;

	language = g_strdup(setlocale(LC_MESSAGES, NULL));
	if((c = strchr(language, ',')) != NULL)
		*c = '\0';
	if((c = strchr(language, '_')) != NULL)
		*c = '\0';

	return language;
}

static gchar *get_local_path_with_locale(gchar *rootpath)
{
	gchar *lang_str, *dir;

	lang_str = get_language();
	dir = g_strconcat(rootpath, G_DIR_SEPARATOR_S, 
			  lang_str, NULL);
	g_free(lang_str);
	if(!is_dir_exist(dir)) {
		g_free(dir);
		dir = g_strconcat(rootpath, G_DIR_SEPARATOR_S,
				  "en", NULL);
		if(!is_dir_exist(dir)) {
			g_free(dir);
			dir = NULL;
		}
	}

	return dir;
}

gboolean manual_available(ManualType type)
{
	gboolean ret = FALSE;
    	gchar *dir = NULL;
	
	switch (type) {
		case MANUAL_MANUAL_LOCAL:
			dir = get_local_path_with_locale(MANUALDIR);
			if (dir != NULL) {
				g_free(dir);
				ret = TRUE;
			}
			break;
		default:
			ret = FALSE;
	}

	return ret;
}

void manual_open(ManualType type)
{
	gchar *uri = NULL;
	gchar *dir;
	gchar *lang_str;

	switch (type) {
		case MANUAL_MANUAL_LOCAL:
			dir = get_local_path_with_locale(MANUALDIR);
			if (dir != NULL) {
				uri = g_strconcat("file://", dir, G_DIR_SEPARATOR_S, MANUAL_HTML_INDEX, NULL);
				g_free(dir);
			}
			break;
		case MANUAL_FAQ_CLAWS:
			uri = g_strconcat(FAQ_URI, NULL);
			break;

		default:
			break;
	}
	open_uri(uri, prefs_common.uri_cmd);
	g_free(uri);
}
