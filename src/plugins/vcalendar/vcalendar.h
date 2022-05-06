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

#ifndef __VCALENDAR_H__
#define __VCALENDAR_H__
#include <libical/ical.h>
#include "vcal_manager.h"
#include "prefs_account.h"
#include "procmime.h"
#include "folder.h"

#define PLUGIN_NAME "vCalendar"

typedef struct _VCalViewer VCalViewer;

void vcalendar_init(void);
void vcalendar_done(void);
void vcalviewer_display_event (VCalViewer *vcalviewer, VCalEvent *event);
gchar *vcalviewer_get_uid_from_mimeinfo(MimeInfo *mimeinfo);
void vcalviewer_reload(FolderItem *item);
void vcalendar_cancel_meeting(FolderItem *item, const gchar *uid);

#define vcal_passwd_set(id, pwd) \
	passwd_store_set(PWS_PLUGIN, PLUGIN_NAME, id, pwd, FALSE)
#define vcal_passwd_get(id) \
	passwd_store_get(PWS_PLUGIN, PLUGIN_NAME, id)

#endif
