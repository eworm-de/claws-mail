/*
 * Claws Mail -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2012 Hiroyuki Yamamoto and the Claws Mail Team
 * Copyright (C) 2006-2012 Ricardo Mones
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

#ifndef __ATTACHWARNER_PREFS__
#define __ATTACHWARNER_PREFS__

#include <glib.h>

typedef struct _AttachWarnerPrefs AttachWarnerPrefs;

struct _AttachWarnerPrefs
{
	gchar		 *match_strings;
	gboolean	 skip_quotes;
	gboolean	 skip_forwards_and_redirections;
	gboolean	 skip_signature;
};

extern AttachWarnerPrefs attwarnerprefs;
void attachwarner_prefs_init(void);
void attachwarner_prefs_done(void);

#endif
