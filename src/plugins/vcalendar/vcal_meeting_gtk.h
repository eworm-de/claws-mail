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

#ifndef __VCAL_MEETING_GTK_H__
#define __VCAL_MEETING_GTK_H__

#include <glib.h>
#include <gtk/gtk.h>
#include "vcal_manager.h"

typedef struct _VCalMeeting VCalMeeting;
typedef struct _VCalAttendee VCalAttendee;

VCalMeeting *vcal_meeting_create(VCalEvent *event);
VCalMeeting *vcal_meeting_create_hidden(VCalEvent *event);
VCalMeeting *vcal_meeting_create_with_start(VCalEvent *event, struct tm *sdate);
gboolean vcal_meeting_send(VCalMeeting *meet);
gboolean vcal_meeting_alert_check(gpointer data);
gboolean vcal_meeting_export_calendar(const gchar *path, const gchar *user, const gchar *pass,gboolean automatic);
gboolean vcal_meeting_export_freebusy(const gchar *path, const gchar *user, const gchar *pass);
gboolean attendee_available(VCalAttendee *attendee, const gchar *dtstart, const gchar *dtend, const gchar *contents);
#endif
