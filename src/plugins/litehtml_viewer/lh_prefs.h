/*
 * Claws Mail -- A GTK based, lightweight, and fast e-mail client
 * Copyright(C) 2019 the Claws Mail Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write tothe Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef LH_PREFS_H
#define LH_PREFS_H

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _LHPrefs LHPrefs;

struct _LHPrefs
{
	gboolean enable_remote_content;
	gint image_cache_size;
	gchar *default_font;
};

LHPrefs *lh_prefs_get(void);
void lh_prefs_init(void);
void lh_prefs_done(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LH_PREFS_H */
