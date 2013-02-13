/* Copyright (c) 2007-2008 Juha Kautto (juha at xfce.org)
 * Copyright (c) 2008 Colin Leroy (colin@colino.net)
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

#ifndef __COMMON_VIEWS_H__
#define __COMMON_VIEWS_H__

#include <string.h>
#include <time.h>

#include <glib.h>
#include <glib/gprintf.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "summaryview.h"
#include "vcalendar.h"
#include "vcal_folder.h"
#include "vcal_prefs.h"
#include "vcal_manager.h"
#include "vcal_meeting_gtk.h"

GtkWidget *build_line(gint start_x, gint start_y
        , gint width, gint height, GtkWidget *hour_line, GdkColor *line_color);
void orage_move_day(struct tm *t, int day);
gint orage_days_between(struct tm *t1, struct tm *t2);
gint vcal_view_set_calendar_page(GtkWidget *to_show, GCallback cb, gpointer data);
void vcal_view_set_summary_page(GtkWidget *to_remove, guint selsig);
void vcal_view_select_event (const gchar *uid, FolderItem *item, gboolean edit,
		    	    GCallback block_cb, gpointer block_data);
void vcal_view_create_popup_menus(gpointer data, 
				GtkWidget **view_menu,
				GtkWidget **event_menu, GtkActionGroup **event_group,
				GtkUIManager **ui_manager);

#endif
