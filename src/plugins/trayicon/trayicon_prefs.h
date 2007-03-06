/*
 * Claws Mail -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2007 the Claws Mail Team
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
 * along with this program; if not, write to the Free Software Foundation, 
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef TRAYICONPREFS_H
#define TRAYICONPREFS_H

#include <glib.h>

typedef struct _TrayIconPrefs	TrayIconPrefs;

struct _TrayIconPrefs
{
	gboolean hide_at_startup;	/**< hide main-window at startup */
	gboolean close_to_tray;	/**< hide main-window when [X] is clicked */
	gboolean hide_when_iconified;	/**< hide main-window when it got iconified */
};

extern TrayIconPrefs trayicon_prefs;

void trayicon_prefs_init(void);
void trayicon_prefs_done(void);

#endif
