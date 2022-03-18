/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2014-2015 Ricardo Mones and the Claws Mail Team
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
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

#ifndef __LIBRAVATAR_PREFS_H
#define __LIBRAVATAR_PREFS_H

#include <glib.h>

typedef struct _LibravatarPrefs LibravatarPrefs;

/* http://wiki.libravatar.org/api/ */
enum
{
	DEF_MODE_NONE		= 0,
	DEF_MODE_URL		= 1,
	DEF_MODE_404		= 10, /* not used, only useful in web pages */
	DEF_MODE_MM		= 11,
	DEF_MODE_IDENTICON	= 12,
	DEF_MODE_MONSTERID	= 13,
	DEF_MODE_WAVATAR	= 14,
	DEF_MODE_RETRO		= 15,
	DEF_MODE_ROBOHASH	= 16,
	DEF_MODE_PAGAN		= 17
};

struct _LibravatarPrefs
{
	gchar		*base_url; /* hidden pref */
	guint		cache_interval;
	gboolean	cache_icons;
	guint		default_mode;
	gchar		*default_mode_url;
	gboolean	allow_redirects;
#if defined USE_GNUTLS
	gboolean	allow_federated;
#endif
	guint		timeout;
	guint		max_redirects_url; /* hidden pref */
	guint		max_redirects_mm; /* hidden pref */
	guint		max_redirects;   /* hidden pref */
};

extern LibravatarPrefs libravatarprefs;
extern GHashTable *libravatarmisses;

void libravatar_prefs_init(void);
void libravatar_prefs_done(void);

#endif

