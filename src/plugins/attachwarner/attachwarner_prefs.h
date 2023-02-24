/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2006-2023 the Claws Mail Team and Ricardo Mones
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

#ifndef __ATTACHWARNER_PREFS__
#define __ATTACHWARNER_PREFS__

#include <glib.h>

typedef struct _AttachWarnerPrefs AttachWarnerPrefs;

struct _AttachWarnerPrefs
{
	gchar *		match_strings;
	gboolean	skip_quotes;
	gboolean	skip_forwards_and_redirections;
	gboolean	skip_signature;
	gboolean	case_sensitive;
};

extern AttachWarnerPrefs attwarnerprefs;
void attachwarner_prefs_init(void);
void attachwarner_prefs_done(void);

#endif
