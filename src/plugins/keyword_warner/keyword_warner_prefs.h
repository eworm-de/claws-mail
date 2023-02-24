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

#ifndef __KEYWORD_WARNER_PREFS__
#define __KEYWORD_WARNER_PREFS__

#include <glib.h>

typedef struct _KeywordWarnerPrefs KeywordWarnerPrefs;

struct _KeywordWarnerPrefs
{
	gchar *		match_strings;
	gboolean	skip_quotes;
	gboolean	skip_forwards_and_redirections;
	gboolean	skip_signature;
	gboolean	case_sensitive;
};

extern KeywordWarnerPrefs kwarnerprefs;
void keyword_warner_prefs_init(void);
void keyword_warner_prefs_done(void);

#endif
