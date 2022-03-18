/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2007 Colin Leroy <colin@colino.net> and 
 * the Claws Mail team
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef VCALPREFS_H
#define VCALPREFS_H

#include <glib.h>

typedef struct _VcalendarPrefs VcalendarPrefs;

struct _VcalendarPrefs
{
	gboolean	 alert_enable;
	gint		 alert_delay;
	gboolean	 export_enable;
	gboolean	 export_freebusy_enable;
	gboolean	 export_subs;
	gchar 		*export_path;	
	gchar 		*export_freebusy_path;	
	gchar 		*export_command;	
	gchar		*export_user;
	gchar		*export_pass;
	gchar 		*export_freebusy_command;
	gchar		*freebusy_get_url;	
	gchar		*export_freebusy_user;
	gchar		*export_freebusy_pass;
	gboolean	 orage_registered;
	gboolean	 ssl_verify_peer;
	gboolean	 calendar_server;
};

extern VcalendarPrefs vcalprefs;

void vcal_prefs_init	(void);
void vcal_prefs_done	(void);
void vcal_prefs_save	(void);

#endif
