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

#ifndef __VCAL_FOLDER_H__
#define __VCAL_FOLDER_H__

#include <glib.h>
#include "folder.h"

extern gboolean manual_update;
typedef struct _day_win day_win;
typedef struct _month_win month_win;
FolderClass *vcal_folder_get_class();
void vcal_folder_gtk_init(void);
void vcal_folder_gtk_done(void);
GSList *vcal_folder_get_waiting_events(void);
GSList *vcal_folder_get_webcal_events(void);
GSList * vcal_folder_get_webcal_events_for_folder(FolderItem *item);
void vcal_folder_export(Folder *folder);

gboolean vcal_curl_put(gchar *url, FILE *fp, gint filesize, const gchar *user, const gchar *pass);
gchar *vcal_curl_read(const char *url, const gchar *label, gboolean verbose, 
	void (*callback)(const gchar *url, gchar *data, gboolean verbose, gchar
		*error));
gchar* get_item_event_list_for_date(FolderItem *item, EventTime date);
void vcal_folder_block_export(gboolean block);
void vcal_folder_refresh_cal(FolderItem *item);
GSList *vcal_get_events_list(FolderItem *item);

day_win *create_day_win(FolderItem *item, struct tm tmdate);
void refresh_day_win(day_win *dw);
void dw_close_window(day_win *dw);

month_win *create_month_win(FolderItem *item, struct tm tmdate);
void refresh_month_win(month_win *dw);
void mw_close_window(month_win *dw);

VCalEvent *vcal_get_event_from_ical(const gchar *ical, const gchar *charset);

void vcal_folder_free_data(void);
#endif
