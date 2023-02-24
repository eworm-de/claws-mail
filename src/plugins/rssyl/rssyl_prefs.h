/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2005-2023 the Claws Mail Team and Andrej Kacian <andrej@kacian.sk>
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

#ifndef __RSSYL_PREFS
#define __RSSYL_PREFS

#define PREFS_BLOCK_NAME	"rssyl"

#define PREF_DEFAULT_REFRESH	"180"

typedef struct _RPrefs RPrefs;

struct _RPrefs {
	gboolean refresh_enabled;
	gint refresh;
	gboolean refresh_on_startup;
	gchar *cookies_path;
	gboolean ssl_verify_peer;
};

typedef struct _RPrefsPage RPrefsPage;

struct _RPrefsPage {
	PrefsPage page;
	GtkWidget *refresh_enabled;
	GtkWidget *refresh;
	GtkWidget *refresh_on_startup;
	GtkWidget *cookies_path;
	GtkWidget *ssl_verify_peer;
};

void rssyl_prefs_init(void);
void rssyl_prefs_done(void);
RPrefs *rssyl_prefs_get(void);

#endif /* __RSSYL_PREFS */
