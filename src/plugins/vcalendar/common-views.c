/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 *
 * Copyright (c) 2007-2008 Juha Kautto (juha at xfce.org)
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#include <stddef.h>
#include <glib.h>
#include <glib/gi18n.h>
#include "defs.h"

#include <string.h>
#include <time.h>

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
#include "menu.h"


static void view_new_meeting_cb			(GtkAction	*action,
						 gpointer	 user_data);
static void view_edit_meeting_cb		(GtkAction	*action,
						 gpointer	 user_data);
static void view_cancel_meeting_cb		(GtkAction	*action,
						 gpointer	 user_data);
static void view_go_today_cb			(GtkAction	*action,
						 gpointer	 user_data);

static GtkActionEntry view_event_popup_entries[] = 
{
	{"VcalViewEvent",		NULL, "VcalViewEvent", NULL, NULL, NULL },
	{"VcalViewEvent/EditMeeting",	NULL, N_("_Edit this meeting..."), NULL, NULL, G_CALLBACK(view_edit_meeting_cb) },
	{"VcalViewEvent/CancelMeeting",	NULL, N_("_Cancel this meeting..."), NULL, NULL, G_CALLBACK(view_cancel_meeting_cb) },
	{"VcalViewEvent/---",		NULL, "---", NULL, NULL, NULL },
	{"VcalViewEvent/CreateMeeting",	NULL, N_("_Create new meeting..."), NULL, NULL, G_CALLBACK(view_new_meeting_cb) },

	{"VcalViewEvent/GoToday",		NULL, N_("_Go to today"), NULL, NULL, G_CALLBACK(view_go_today_cb) },
};

GtkWidget *build_line(gint start_x, gint start_y
        , gint width, gint height, GtkWidget *hour_line, GdkColor *line_color)
{
    GtkWidget *new_hour_line;
		cairo_t *cr;
		cairo_surface_t *pic1;
    gboolean first = FALSE;

		debug_print("build_line [%d,%d] %dx%d %s\n",
				start_x, start_y, width, height, hour_line ? "widget" : "no widget");
    /*
     * GdkPixbuf *scaled;
    scaled = gdk_pixbuf_scale_simple (pix, w, h, GDK_INTERP_BILINEAR);
    */
    
    pic1 = cairo_image_surface_create(CAIRO_FORMAT_A1, width, height);
    cr = cairo_create(pic1);

    if (hour_line == NULL) {
        first = TRUE;
    } else {
        gdk_cairo_set_source_pixbuf(cr, gtk_image_get_pixbuf(GTK_IMAGE(hour_line)), 0, 0);
    }

    if (first) {
        cairo_set_source_rgb(cr,
            (double)line_color->red/255,
            (double)line_color->green/255,
            (double)line_color->blue/255);
        cairo_rectangle(cr, start_x, start_y, width, height);
    }
    else {
        cairo_rectangle(cr, start_x, start_y, width, height);
    }

    cairo_fill(cr);
		cairo_destroy(cr);

    new_hour_line = gtk_image_new_from_surface(pic1);
    cairo_surface_destroy(pic1);
    return(new_hour_line);
}

/* move one day forward or backward */
void orage_move_day(struct tm *t, int day)
{
    guint monthdays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    t->tm_year += 1900;
    if (((t->tm_year%4) == 0) 
    && (((t->tm_year%100) != 0) || ((t->tm_year%400) == 0)))
        ++monthdays[1]; /* leap year, february has 29 days */

    t->tm_mday += day; /* may be negative */
    if (t->tm_mday == 0) { /*  we went to previous month */
        if (--t->tm_mon == -1) { /* previous year */
            --t->tm_year;
            t->tm_mon = 11;
        }
        t->tm_mday = monthdays[t->tm_mon];
    }
    else if (t->tm_mday > (monthdays[t->tm_mon])) { /* next month */
        if (++t->tm_mon == 12) {
            ++t->tm_year;
            t->tm_mon = 0;
        }
        t->tm_mday = 1;
    }
    t->tm_year -= 1900;
    t->tm_wday += day; 
    if (t->tm_wday < 0)
        t->tm_wday = 6;
    t->tm_wday %=7;
}

gint orage_days_between(struct tm *t1, struct tm *t2)
{
    GDate *g_t1, *g_t2;
    gint dd;
    g_t1 = g_date_new_dmy(t1->tm_mday, t1->tm_mon, t1->tm_year);
    g_t2 = g_date_new_dmy(t2->tm_mday, t2->tm_mon, t2->tm_year);
    dd = g_date_days_between(g_t1, g_t2);
    g_date_free(g_t1);
    g_date_free(g_t2);
    return(dd);
}

gint vcal_view_set_calendar_page(GtkWidget *to_show, GCallback cb, gpointer data)
{
    SummaryView *summaryview = NULL;
    gint selsig = -1;
    if (mainwindow_get_mainwindow()) {
	summaryview = mainwindow_get_mainwindow()->summaryview;
	gtk_container_add(GTK_CONTAINER(summaryview->mainwidget_book),
		to_show);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(summaryview->mainwidget_book),
		gtk_notebook_page_num(GTK_NOTEBOOK(summaryview->mainwidget_book), 
		to_show));
	main_window_set_menu_sensitive(mainwindow_get_mainwindow());
	toolbar_main_set_sensitive(mainwindow_get_mainwindow());
	selsig = g_signal_connect(G_OBJECT(summaryview->ctree), "tree_select_row",
			 G_CALLBACK(cb), data);
    }
    return selsig;
}

void vcal_view_set_summary_page(GtkWidget *to_remove, guint selsig)
{
    SummaryView *summaryview = NULL;
    if (mainwindow_get_mainwindow()) {
	summaryview = mainwindow_get_mainwindow()->summaryview;
        if (selsig)
        	g_signal_handler_disconnect(G_OBJECT(summaryview->ctree), selsig);
	gtk_container_remove(GTK_CONTAINER(summaryview->mainwidget_book),
		to_remove);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(summaryview->mainwidget_book),
		gtk_notebook_page_num(GTK_NOTEBOOK(summaryview->mainwidget_book), 
		summaryview->scrolledwin));
	main_window_set_menu_sensitive(mainwindow_get_mainwindow());
	toolbar_main_set_sensitive(mainwindow_get_mainwindow());
    }
	
}

void vcal_view_select_event (const gchar *uid, FolderItem *item, gboolean edit,
		    	    GCallback block_cb, gpointer block_data)
{
	if (edit) {
    		VCalEvent *event = NULL;
        	event = vcal_manager_load_event(uid);
		if (event) {
			vcal_meeting_create(event);
			vcal_manager_free_event(event);
        	}
	} else {
        	SummaryView *summaryview = NULL;
		if (mainwindow_get_mainwindow()) {
		   MsgInfo *info = folder_item_get_msginfo_by_msgid(item, uid);
		   if (info) {
			   summaryview = mainwindow_get_mainwindow()->summaryview;
			   g_signal_handlers_block_by_func(G_OBJECT(summaryview->ctree),
					       G_CALLBACK(block_cb), block_data);
			   summary_select_by_msgnum(summaryview, info->msgnum, TRUE);
			   procmsg_msginfo_free(&info);
			   g_signal_handlers_unblock_by_func(G_OBJECT(summaryview->ctree),
					       G_CALLBACK(block_cb), block_data);
		   }
		}
	}
}

void vcal_view_create_popup_menus(gpointer data, 
				GtkWidget **view_menu,
				GtkWidget **event_menu, GtkActionGroup **event_group,
				GtkUIManager **ui_manager)
{
	*ui_manager = gtk_ui_manager_new();
	*event_group = cm_menu_create_action_group_full(*ui_manager,"VcalViewEvent", view_event_popup_entries,
			G_N_ELEMENTS(view_event_popup_entries), (gpointer)data);

	MENUITEM_ADDUI_MANAGER(*ui_manager, "/", "Menus", "Menus", GTK_UI_MANAGER_MENUBAR)
	MENUITEM_ADDUI_MANAGER(*ui_manager, "/Menus", "VcalView", "VcalViewEvent", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(*ui_manager, "/Menus/VcalView", "CreateMeeting", "VcalViewEvent/CreateMeeting", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(*ui_manager, "/Menus/VcalView", "Separator1", "VcalViewEvent/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(*ui_manager, "/Menus/VcalView", "GoToday", "VcalViewEvent/GoToday", GTK_UI_MANAGER_MENUITEM)

	MENUITEM_ADDUI_MANAGER(*ui_manager, "/Menus", "VcalViewEvent", "VcalViewEvent", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(*ui_manager, "/Menus/VcalViewEvent", "EditMeeting", "VcalViewEvent/EditMeeting", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(*ui_manager, "/Menus/VcalViewEvent", "CancelMeeting", "VcalViewEvent/CancelMeeting", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(*ui_manager, "/Menus/VcalViewEvent", "Separator1", "VcalViewEvent/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(*ui_manager, "/Menus/VcalViewEvent", "CreateMeeting", "VcalViewEvent/CreateMeeting", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(*ui_manager, "/Menus/VcalViewEvent", "Separator2", "VcalViewEvent/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(*ui_manager, "/Menus/VcalViewEvent", "GoToday", "VcalViewEvent/GoToday", GTK_UI_MANAGER_MENUITEM)

	*view_menu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(
				gtk_ui_manager_get_widget(*ui_manager, "/Menus/VcalView")) );
	*event_menu= gtk_menu_item_get_submenu(GTK_MENU_ITEM(
				gtk_ui_manager_get_widget(*ui_manager, "/Menus/VcalViewEvent")) );
}

static void view_new_meeting_cb			(GtkAction	*action,
						 gpointer	 user_data)
{
	gpointer data_i = g_object_get_data(G_OBJECT(user_data), "menu_data_i");
	gpointer data_s = g_object_get_data(G_OBJECT(user_data), "menu_data_s");
	gpointer win = g_object_get_data(G_OBJECT(user_data), "menu_win");
	void (*cb)(gpointer win, gpointer data_i, gpointer data_s) = 
		g_object_get_data(G_OBJECT(user_data), "new_meeting_cb");
	if (cb)
		cb(win, data_i, data_s);
}
static void view_edit_meeting_cb		(GtkAction	*action,
						 gpointer	 user_data)
{
	gpointer data_i = g_object_get_data(G_OBJECT(user_data), "menu_data_i");
	gpointer data_s = g_object_get_data(G_OBJECT(user_data), "menu_data_s");
	gpointer win = g_object_get_data(G_OBJECT(user_data), "menu_win");
	void (*cb)(gpointer win, gpointer data_i, gpointer data_s) = 
		g_object_get_data(G_OBJECT(user_data), "edit_meeting_cb");
	if (cb)
		cb(win, data_i, data_s);
}
static void view_cancel_meeting_cb		(GtkAction	*action,
						 gpointer	 user_data)
{
	gpointer data_i = g_object_get_data(G_OBJECT(user_data), "menu_data_i");
	gpointer data_s = g_object_get_data(G_OBJECT(user_data), "menu_data_s");
	gpointer win = g_object_get_data(G_OBJECT(user_data), "menu_win");
	void (*cb)(gpointer win, gpointer data_i, gpointer data_s) = 
		g_object_get_data(G_OBJECT(user_data), "cancel_meeting_cb");
	if (cb)
		cb(win, data_i, data_s);
}
static void view_go_today_cb			(GtkAction	*action,
						 gpointer	 user_data)
{
	gpointer data_i = g_object_get_data(G_OBJECT(user_data), "menu_data_i");
	gpointer data_s = g_object_get_data(G_OBJECT(user_data), "menu_data_s");
	gpointer win = g_object_get_data(G_OBJECT(user_data), "menu_win");
	void (*cb)(gpointer win, gpointer data_i, gpointer data_s) = 
		g_object_get_data(G_OBJECT(user_data), "go_today_cb");
	if (cb)
		cb(win, data_i, data_s);
}
