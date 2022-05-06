/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 *
 * Copyright (c) 2007-2008 Juha Kautto (juha at xfce.org)
 * Copyright (c) 2008-2022 the Claws Mail Team and  Colin Leroy (colin@colino.net)
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

#ifdef USE_PTHREAD
#include <pthread.h>
#endif
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
#include "common-views.h"
#include "vcal_meeting_gtk.h"

#define MAX_DAYS 40
struct _day_win
{
    GtkAccelGroup *accel_group;

    GtkWidget *Window;
    GtkWidget *Vbox;

    GtkWidget *Menubar;
    GtkWidget *File_menu;
    GtkWidget *File_menu_new;
    GtkWidget *File_menu_close;
    GtkWidget *View_menu;
    GtkWidget *View_menu_refresh;
    GtkWidget *Go_menu;
    GtkWidget *Go_menu_today;
    GtkWidget *Go_menu_prev;
    GtkWidget *Go_menu_next;

    GtkWidget *Toolbar;
    GtkWidget *Create_toolbutton;
    GtkWidget *Previous_toolbutton;
    GtkWidget *Today_toolbutton;
    GtkWidget *Next_toolbutton;
    GtkWidget *Refresh_toolbutton;
    GtkWidget *Close_toolbutton;

    GtkWidget *StartDate_button;
    GtkRequisition StartDate_button_req;
    GtkWidget *day_spin;

    GtkWidget *day_view_vbox;
    GtkWidget *scroll_win_h;
    GtkWidget *dtable_h; /* header of day table */
    GtkWidget *scroll_win;
    GtkWidget *dtable;   /* day table */
    GtkRequisition hour_req;

    GtkWidget *header[MAX_DAYS];
    GtkWidget *element[24][MAX_DAYS];
    GtkWidget *line[24][MAX_DAYS];

    guint upd_timer;
    gdouble scroll_pos; /* remember the scroll position */

    GdkColor bg1, bg2, line_color, bg_today, fg_sunday;
    GList    *apptw_list; /* keep track of appointments being updated */
    struct tm startdate;
    FolderItem *item;
    gulong selsig;
    GtkWidget *view_menu;
    GtkWidget *event_menu;
    GtkActionGroup *event_group;
    GtkUIManager *ui_manager;
};

static gchar *get_locale_date(struct tm *tmdate)
{
	gchar *d = g_malloc(100);
	strftime(d, 99, "%x", tmdate);
	return d;
}

static void set_scroll_position(day_win *dw)
{
    GtkAdjustment *v_adj;

    v_adj = gtk_scrolled_window_get_vadjustment(
            GTK_SCROLLED_WINDOW(dw->scroll_win));
    if (dw->scroll_pos > 0) /* we have old value */
        gtk_adjustment_set_value(v_adj, dw->scroll_pos);
    else if (dw->scroll_pos < 0)
        /* default: let's try to start roughly from line 8 = 8 o'clock */
        gtk_adjustment_set_value(v_adj,
						gtk_adjustment_get_upper(v_adj) / 3);
}

static gboolean scroll_position_timer(gpointer user_data)
{
    set_scroll_position((day_win *)user_data);
    return(FALSE); /* only once */
}

static void get_scroll_position(day_win *dw)
{
    GtkAdjustment *v_adj;

    v_adj = gtk_scrolled_window_get_vadjustment(
            GTK_SCROLLED_WINDOW(dw->scroll_win));
    dw->scroll_pos = gtk_adjustment_get_value(v_adj);
}

void dw_close_window(day_win *dw)
{
    vcal_view_set_summary_page(dw->Vbox, dw->selsig);

    g_free(dw);
    dw = NULL;
}

static char *orage_tm_date_to_i18_date(struct tm *tm_date)
{
    static char i18_date[32];
    struct tm t;
    t.tm_mday = tm_date->tm_mday;
    t.tm_mon = tm_date->tm_mon - 1;
    t.tm_year = tm_date->tm_year - 1900;
    t.tm_sec = 0;
    t.tm_min = 0;
    t.tm_hour = 0;
    t.tm_wday = 0;
    t.tm_yday = 0;

    if (strftime(i18_date, 32, "%x", &t) == 0)
        g_error("Orage: orage_tm_date_to_i18_date too long string in strftime");
    return(i18_date);
}

static void changeSelectedDate(day_win *dw, gint day)
{
    if (day > 0) {
     do {
      orage_move_day(&(dw->startdate), 1);
     } while (--day > 0);
    } else {
     do {
      orage_move_day(&(dw->startdate), -1);
     } while (++day < 0);
    }
}

static gint on_Previous_clicked(GtkWidget *button, GdkEventButton *event,
				    day_win *dw)
{
    int days = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(dw->day_spin));
    changeSelectedDate(dw, -days);
    refresh_day_win(dw);
    return TRUE;
}

static gint on_Next_clicked(GtkWidget *button, GdkEventButton *event,
				    day_win *dw)
{
    int days = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(dw->day_spin));
    changeSelectedDate(dw, +days);
    refresh_day_win(dw);
    return TRUE;
}

static void dw_summary_selected(GtkCMCTree *ctree, GtkCMCTreeNode *row,
			     gint column, day_win *dw)
{
	MsgInfo *msginfo = gtk_cmctree_node_get_row_data(ctree, row);
	int days = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(dw->day_spin));
	
	if (msginfo && msginfo->msgid) {
		VCalEvent *event = vcal_manager_load_event(msginfo->msgid);
		if (event) {
			time_t t_first = mktime(&dw->startdate);
			time_t t_start = icaltime_as_timet(icaltime_from_string(event->dtstart));
			struct tm tm_start;
			gboolean changed = FALSE;
			GtkAdjustment *v_adj;

#ifdef G_OS_WIN32
			if (t_start < 0)
				t_start = 1;
#endif
			localtime_r(&t_start, &tm_start);
			tm_start.tm_hour = tm_start.tm_min = tm_start.tm_sec = 0;
			t_start = mktime(&tm_start);

			while (t_start < t_first) {
				changeSelectedDate(dw, -days);
				t_first = mktime(&dw->startdate);
				changed = TRUE;
			}
			while (t_start > t_first + (days-1)*24*60*60) {
				changeSelectedDate(dw, +days);
				t_first = mktime(&dw->startdate);
				changed = TRUE;
			}
			
			t_start = icaltime_as_timet(icaltime_from_string(event->dtstart));
#ifdef G_OS_WIN32
			if (t_start < 0)
				t_start = 1;
#endif
			localtime_r(&t_start, &tm_start);
			if (changed) {
				debug_print("changed from %s\n", event->summary);
				v_adj = gtk_scrolled_window_get_vadjustment(
				    GTK_SCROLLED_WINDOW(dw->scroll_win)); 
#ifdef G_OS_WIN32
				if (t_start < 0)
					t_start = 1;
#endif
				localtime_r(&t_start, &tm_start);
				if (tm_start.tm_hour > 2)
					gtk_adjustment_set_value(v_adj, 
						((gtk_adjustment_get_upper(v_adj) - gtk_adjustment_get_page_size(v_adj))/((gdouble)24/(gdouble)(tm_start.tm_hour-2))));
				else
					gtk_adjustment_set_value(v_adj, 0);
				refresh_day_win(dw);
			}
		}
		vcal_manager_free_event(event);
	}
}

static void day_view_new_meeting_cb(day_win *dw, gpointer data_i, gpointer data_s)
{
    int offset = GPOINTER_TO_INT(data_i);
    struct tm tm_date = dw->startdate;
    int offset_h = offset % 1000;
    int offset_d = (offset-offset_h) / 1000;
    guint monthdays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};    
    int mon = tm_date.tm_mon;
    
    if (((tm_date.tm_year%4) == 0) && (((tm_date.tm_year%100) != 0) 
            || ((tm_date.tm_year%400) == 0)))
        monthdays[1] = 29;

    if (offset_d > (int)monthdays[mon]) {
    	while (tm_date.tm_mday > 1)
		orage_move_day(&tm_date, 1);
	offset_d -= monthdays[mon];
    }

    while (offset_d > tm_date.tm_mday) {
	    orage_move_day(&tm_date, 1);
    }
    while (offset_d < tm_date.tm_mday) {
	    orage_move_day(&tm_date, -1);
    }
    tm_date.tm_hour = offset_h;
    vcal_meeting_create_with_start(NULL, &tm_date);
}

static void day_view_edit_meeting_cb(day_win *dw, gpointer data_i, gpointer data_s)
{
	const gchar *uid = (gchar *)data_s;
        vcal_view_select_event (uid, dw->item, TRUE,
		    	    G_CALLBACK(dw_summary_selected), dw);
}

static void day_view_cancel_meeting_cb(day_win *dw, gpointer data_i, gpointer data_s)
{
	const gchar *uid = (gchar *)data_s;
	vcalendar_cancel_meeting(dw->item, uid);
}

static void day_view_today_cb(day_win *dw, gpointer data_i, gpointer data_s)
{
    time_t now = time(NULL);
    struct tm tm_today;
    localtime_r(&now, &tm_today);

    while (tm_today.tm_wday != 1)
    	orage_move_day(&tm_today, -1);
    
    dw->startdate = tm_today;
    refresh_day_win(dw);
}

static void header_button_clicked_cb(GtkWidget *button, day_win *dw)
{
    int offset = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "offset"));

    day_view_new_meeting_cb(dw, GINT_TO_POINTER(offset), NULL);
}

static void on_button_press_event_cb(GtkWidget *widget
        , GdkEventButton *event, gpointer *user_data)
{
    day_win *dw = (day_win *)user_data;
    gchar *uid = g_object_get_data(G_OBJECT(widget), "UID");
    gpointer offset = g_object_get_data(G_OBJECT(widget), "offset");
 
    if (event->button == 1) {
	if (uid)
            vcal_view_select_event (uid, dw->item, (event->type==GDK_2BUTTON_PRESS),
		    	    G_CALLBACK(dw_summary_selected), dw);
    }
    if (event->button == 3) {
	    g_object_set_data(G_OBJECT(dw->Vbox), "menu_win",
		      dw);
	    g_object_set_data(G_OBJECT(dw->Vbox), "menu_data_i",
		      offset);
	    g_object_set_data(G_OBJECT(dw->Vbox), "menu_data_s",
		      uid);
	    g_object_set_data(G_OBJECT(dw->Vbox), "new_meeting_cb",
		      day_view_new_meeting_cb);
	    g_object_set_data(G_OBJECT(dw->Vbox), "edit_meeting_cb",
		      day_view_edit_meeting_cb);
	    g_object_set_data(G_OBJECT(dw->Vbox), "cancel_meeting_cb",
		      day_view_cancel_meeting_cb);
	    g_object_set_data(G_OBJECT(dw->Vbox), "go_today_cb",
		      day_view_today_cb);
	    if (uid)
		    gtk_menu_popup_at_pointer(GTK_MENU(dw->event_menu), NULL);
	    else
		    gtk_menu_popup_at_pointer(GTK_MENU(dw->view_menu), NULL);
    }
}

static void add_row(day_win *dw, VCalEvent *event, gint days)
{
    gint row, start_row, end_row;
    gint col, start_col, end_col, first_col, last_col;
    gint height, start_height, end_height;
    gchar *text, *tip, *start_date, *end_date;
    GtkWidget *ev, *lab, *hb;
    time_t t_start, t_end;
    struct tm tm_first, tm_start, tm_end;

    /* First clarify timings */
    t_start = icaltime_as_timet(icaltime_from_string(event->dtstart));
    if (event->dtend && *event->dtend)
        t_end = icaltime_as_timet(icaltime_from_string(event->dtend));
    else 
	t_end = t_start;

#ifdef G_OS_WIN32
	if (t_start < 0)
		t_start = 1;
	if (t_end < 0)
		t_end = 1;
#endif
    localtime_r(&t_start, &tm_start);
    localtime_r(&t_end, &tm_end);
    tm_first = dw->startdate;
   
    tm_first.tm_year += 1900;
    tm_first.tm_mon += 1;
    tm_start.tm_year += 1900;
    tm_start.tm_mon += 1;
    tm_end.tm_year += 1900;
    tm_end.tm_mon += 1;

    start_col = orage_days_between(&tm_first, &tm_start)+1;
    end_col   = orage_days_between(&tm_first, &tm_end)+1;

    if (end_col < 1)
    	return;
    if (start_col > days)
        return;

    else {
        col = start_col;
        row = tm_start.tm_hour;
    }

    /* then add the appointment */
    text = g_strdup(event->summary?event->summary : _("Unknown"));
    ev = gtk_event_box_new();
    lab = gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(lab), 0.0);
    gtk_label_set_yalign(GTK_LABEL(lab), 0.5);
    gtk_label_set_ellipsize(GTK_LABEL(lab), PANGO_ELLIPSIZE_END);
    gtk_container_add(GTK_CONTAINER(ev), lab);

    if ((row % 2) == 1)
        gtk_widget_modify_bg(ev, GTK_STATE_NORMAL, &dw->bg1);
    else
        gtk_widget_modify_bg(ev, GTK_STATE_NORMAL, &dw->bg2);
    if (dw->element[row][col] == NULL) {
        hb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
        dw->element[row][col] = hb;
    }
    else {
        hb = dw->element[row][col];
        /* FIXME: set some real bar here to make it visible that we
         * have more than 1 appointment here
         */
    }
    if (orage_days_between(&tm_start, &tm_end) == 0)
        tip = g_strdup_printf("%s\n%02d:%02d-%02d:%02d\n%s"
                , text, tm_start.tm_hour, tm_start.tm_min
                , tm_end.tm_hour, tm_end.tm_min, event->description);
    else {
/* we took the date in unnormalized format, so we need to do that now */
        start_date = g_strdup(orage_tm_date_to_i18_date(&tm_start));
        end_date = g_strdup(orage_tm_date_to_i18_date(&tm_end));
        tip = g_strdup_printf("%s\n%s %02d:%02d - %s %02d:%02d\n%s"
                , text
                , start_date, tm_start.tm_hour, tm_start.tm_min
                , end_date, tm_end.tm_hour, tm_end.tm_min
                , event->description);
        g_free(start_date);
        g_free(end_date);
    }
    CLAWS_SET_TIP(ev, tip);
    /*
    gtk_box_pack_start(GTK_BOX(hb2), ev, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hb), hb2, TRUE, TRUE, 0);
    */
    gtk_box_pack_start(GTK_BOX(hb), ev, TRUE, TRUE, 0);
    g_object_set_data_full(G_OBJECT(ev), "UID", g_strdup(event->uid), g_free);
    g_object_set_data(G_OBJECT(ev), "offset", GINT_TO_POINTER(tm_start.tm_mday*1000 + tm_start.tm_hour));
    g_signal_connect((gpointer)ev, "button-press-event"
            , G_CALLBACK(on_button_press_event_cb), dw);
    g_free(tip);
    g_free(text);

    /* and finally draw the line to show how long the appointment is,
     * but only if it is Busy type event (=availability != 0) 
     * and it is not whole day event */
    if (TRUE) {
        height = dw->StartDate_button_req.height;
        /*
         * same_date = !strncmp(start_ical_time, end_ical_time, 8);
         * */
        if (start_col < 1)
            first_col = 1;
        else
            first_col = start_col;
        if (end_col > days)
            last_col = days;
        else
            last_col = end_col;
        for (col = first_col; col <= last_col; col++) {
            if (col == start_col)
                start_row = tm_start.tm_hour;
            else
                start_row = 0;
            if (col == end_col)
                end_row   = tm_end.tm_hour;
            else
                end_row   = 23;
            for (row = start_row; row <= end_row; row++) {
                if (row == tm_start.tm_hour && col == start_col)
                    start_height = tm_start.tm_min*height/60;
                else
                    start_height = 0;
                if (row == tm_end.tm_hour && col == end_col)
                    end_height = tm_end.tm_min*height/60;
                else
                    end_height = height;
                dw->line[row][col] = build_line(1, start_height
                        , 2, end_height-start_height, dw->line[row][col]
			, &dw->line_color);
            }
        }
    }
}

static void app_rows(day_win *dw, FolderItem *item)
{
   GSList *events = vcal_get_events_list(item);
   GSList *cur = NULL;
   int days = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(dw->day_spin));
   for (cur = events; cur ; cur = cur->next) {
   	VCalEvent *event = (VCalEvent *) (cur->data);
	add_row(dw, event, days);
	vcal_manager_free_event(event);
   }
   g_slist_free(events);
}

static void app_data(day_win *dw, FolderItem *item)
{
    app_rows(dw, item);
}


static void fill_days(day_win *dw, gint days, FolderItem *item, gint first_col_day)
{
    gint row, col, height, width;
    GtkWidget *ev, *hb;

    height = dw->StartDate_button_req.height;
    width = dw->StartDate_button_req.width;

    /* first clear the structure */
    for (col = 1; col <  days+1; col++) {
        dw->header[col] = NULL;
        for (row = 0; row < 24; row++) {
            dw->element[row][col] = NULL;
    /* gdk_draw_rectangle(, , , left_x, top_y, width, height); */
            dw->line[row][col] = build_line(0, 0, 3, height, NULL
			, &dw->line_color);
        }
    }

    app_data(dw, item);

    for (col = 1; col < days+1; col++) {
        hb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        /* check if we have full day events and put them to header */
        if (dw->header[col]) {
            gtk_box_pack_start(GTK_BOX(hb), dw->header[col], TRUE, TRUE, 0);
            gtk_widget_set_size_request(hb, width, -1);
        }
        else {
            ev = gtk_event_box_new();
            gtk_widget_modify_bg(ev, GTK_STATE_NORMAL, &dw->bg2);
            gtk_box_pack_start(GTK_BOX(hb), ev, TRUE, TRUE, 0);
        }
        gtk_widget_set_hexpand(hb, TRUE);
	gtk_grid_attach(GTK_GRID(dw->dtable_h), hb, col, 1, 1, 1);

        /* check rows */
        for (row = 0; row < 24; row++) {
            hb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
            if (row == 0) {
                gtk_widget_set_size_request(hb, width, -1);
		gtk_widget_set_hexpand(hb, TRUE);
	    }
            if (dw->element[row][col]) {
                gtk_box_pack_start(GTK_BOX(hb), dw->line[row][col]
                        , FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(hb), dw->element[row][col]
                        , TRUE, TRUE, 0);
                gtk_widget_set_size_request(hb, width, -1);
		gtk_widget_set_hexpand(hb, TRUE);
            }
            else {
                ev = gtk_event_box_new();
		g_object_set_data(G_OBJECT(ev), "offset", GINT_TO_POINTER(first_col_day*1000 + row));
		g_signal_connect((gpointer)ev, "button-press-event"
        		, G_CALLBACK(on_button_press_event_cb), dw);
                /*
                name = gtk_label_new(" ");
                gtk_container_add(GTK_CONTAINER(ev), name);
                */
                if ((row % 2) == 1)
                    gtk_widget_modify_bg(ev, GTK_STATE_NORMAL, &dw->bg1);
                else
                    gtk_widget_modify_bg(ev, GTK_STATE_NORMAL, &dw->bg2);
                gtk_box_pack_start(GTK_BOX(hb), dw->line[row][col]
                        , FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(hb), ev, TRUE, TRUE, 0);
            }
            gtk_grid_attach(GTK_GRID(dw->dtable), hb, col, row, 1, 1);
       }
	first_col_day++;
    }
}

static void build_day_view_header(day_win *dw, char *start_date)
{
    GtkWidget *hbox, *label, *space_label;
    SummaryView *summaryview = NULL;
    int avail_w = 0, avail_d = 0;

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

    label = gtk_label_new(_("Start"));
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 10);

    /* start date button */
    dw->StartDate_button = gtk_button_new();
    gtk_box_pack_start(GTK_BOX(hbox), dw->StartDate_button, FALSE, FALSE, 0);

    space_label = gtk_label_new("  ");
    gtk_box_pack_start(GTK_BOX(hbox), space_label, FALSE, FALSE, 0);

    space_label = gtk_label_new("     ");
    gtk_box_pack_start(GTK_BOX(hbox), space_label, FALSE, FALSE, 0);

    label = gtk_label_new(_("Show"));
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 10);

    /* show days spin = how many days to show */
    dw->day_spin = gtk_spin_button_new_with_range(1, MAX_DAYS, 1);
    gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(dw->day_spin), TRUE);
    gtk_widget_set_size_request(dw->day_spin, 40, -1);
    gtk_box_pack_start(GTK_BOX(hbox), dw->day_spin, FALSE, FALSE, 0);
    label = gtk_label_new(_("days"));
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);

    space_label = gtk_label_new("   ");
    gtk_box_pack_start(GTK_BOX(hbox), space_label, FALSE, FALSE, 0);

    /* sizes */
    gtk_button_set_label(GTK_BUTTON(dw->StartDate_button)
            , (const gchar *)start_date);
    gtk_widget_get_preferred_size(dw->StartDate_button, &dw->StartDate_button_req, &dw->StartDate_button_req);
    dw->StartDate_button_req.width += dw->StartDate_button_req.width/10;
    label = gtk_label_new("00");
    gtk_widget_get_preferred_size(label, &dw->hour_req, &dw->hour_req);

    if (mainwindow_get_mainwindow()) {
        GtkAllocation allocation;
	summaryview = mainwindow_get_mainwindow()->summaryview;
	gtk_widget_get_allocation(summaryview->mainwidget_book,
			&allocation);
	
	avail_w = allocation.width - 20 - 2*(dw->hour_req.width);
	avail_d = (dw->StartDate_button_req.width > 0)? avail_w / dw->StartDate_button_req.width : avail_w;
    }
    if (avail_d >= 7) {
    	avail_d = 7;
	gtk_widget_set_size_request(dw->StartDate_button, avail_w / avail_d, -1);
	gtk_widget_get_preferred_size(dw->StartDate_button, &dw->StartDate_button_req, &dw->StartDate_button_req);
    }
  
    /* initial values */
    if (avail_d)
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(dw->day_spin), avail_d);

}

static void build_day_view_colours(day_win *dw)
{
    GtkStyle *def_style, *cur_style;
    GtkWidget *ctree = NULL;
    def_style = gtk_widget_get_default_style();
    
    if (mainwindow_get_mainwindow()) {
        ctree = mainwindow_get_mainwindow()->summaryview->ctree;
    }
    if (ctree) {
        cur_style = gtk_widget_get_style(ctree);
        dw->bg1 = cur_style->bg[GTK_STATE_NORMAL];
        dw->bg2 = cur_style->bg[GTK_STATE_NORMAL];
    } else {
        dw->bg1 = def_style->bg[GTK_STATE_NORMAL];
        dw->bg2 = def_style->bg[GTK_STATE_NORMAL];
    }
    dw->bg1.red +=  (dw->bg1.red < 63000 ? 2000 : -2000);
    dw->bg1.green += (dw->bg1.green < 63000 ? 2000 : -2000);
    dw->bg1.blue += (dw->bg1.blue < 63000 ? 2000 : -2000);

    dw->bg2.red +=  (dw->bg2.red > 1000 ? -1000 : 1000);
    dw->bg2.green += (dw->bg2.green > 1000 ? -1000 : 1000);
    dw->bg2.blue += (dw->bg2.blue > 1000 ? -1000 : 1000);

    if (!gdk_color_parse("white", &dw->line_color)) {
        g_warning("color parse failed: white");
        dw->line_color.red =  239 * (65535/255);
        dw->line_color.green = 235 * (65535/255);
        dw->line_color.blue = 230 * (65535/255);
    }

    if (!gdk_color_parse("blue", &dw->fg_sunday)) {
        g_warning("color parse failed: blue");
        dw->fg_sunday.red = 10 * (65535/255);
        dw->fg_sunday.green = 10 * (65535/255);
        dw->fg_sunday.blue = 255 * (65535/255);
    }

    if (!gdk_color_parse("gold", &dw->bg_today)) {
        g_warning("color parse failed: gold");
        dw->bg_today.red = 255 * (65535/255);
        dw->bg_today.green = 215 * (65535/255);
        dw->bg_today.blue = 115 * (65535/255);
    }

    if (ctree) {
        cur_style = gtk_widget_get_style(ctree);
        dw->fg_sunday.red = (dw->fg_sunday.red + cur_style->fg[GTK_STATE_SELECTED].red)/2;
        dw->fg_sunday.green = (dw->fg_sunday.green + cur_style->fg[GTK_STATE_SELECTED].red)/2;
        dw->fg_sunday.blue = (3*dw->fg_sunday.blue + cur_style->fg[GTK_STATE_SELECTED].red)/4;
        dw->bg_today.red = (3*dw->bg_today.red + cur_style->bg[GTK_STATE_NORMAL].red)/4;
        dw->bg_today.green = (3*dw->bg_today.green + cur_style->bg[GTK_STATE_NORMAL].red)/4;
        dw->bg_today.blue = (3*dw->bg_today.blue + cur_style->bg[GTK_STATE_NORMAL].red)/4;
    }
}

static void fill_hour(day_win *dw, gint col, gint row, char *text)
{
    GtkWidget *name, *ev;

    ev = gtk_event_box_new();
    name = gtk_label_new(text);
    gtk_container_add(GTK_CONTAINER(ev), name);
    if ((row % 2) == 1)
        gtk_widget_modify_bg(ev, GTK_STATE_NORMAL, &dw->bg1);
    else
        gtk_widget_modify_bg(ev, GTK_STATE_NORMAL, &dw->bg2);
    gtk_widget_set_size_request(ev, dw->hour_req.width
            , dw->StartDate_button_req.height);
    if (text)
	gtk_grid_attach(GTK_GRID(dw->dtable), ev, col, row, 1, 1);
    else  /* special, needed for header table full day events */
	gtk_grid_attach(GTK_GRID(dw->dtable_h), ev, col, row, 1, 1);
}

static void build_day_view_table(day_win *dw)
{
    gint days;   /* number of days to show */
    gint i = 0;
    GtkWidget *label, *button;
    char text[5+1], *date, *today;
    struct tm tm_date, tm_today;
    guint monthdays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    GtkWidget *vp;
    time_t t = time(NULL);
    GtkWidget *arrow;
    gchar *tip;
    gint first_col_day = -1;

#ifdef G_OS_WIN32
	if (t < 0)
		t = 1;
#endif
    localtime_r(&t, &tm_today);
    days = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(dw->day_spin));
    today = get_locale_date(&tm_today);
    /****** header of day table = days columns ******/
    dw->scroll_win_h = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(dw->scroll_win_h)
            , GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
    gtk_box_pack_start(GTK_BOX(dw->Vbox), dw->scroll_win_h
            , TRUE, TRUE, 0);
    dw->day_view_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(dw->scroll_win_h), dw->day_view_vbox);
    /* row 1= day header buttons 
     * row 2= full day events after the buttons */
    dw->dtable_h = gtk_grid_new();
    gtk_box_pack_start(GTK_BOX(dw->day_view_vbox), dw->dtable_h
            , FALSE, FALSE, 0);

    tm_date = dw->startdate;

    if (((tm_date.tm_year%4) == 0) && (((tm_date.tm_year%100) != 0) 
            || ((tm_date.tm_year%400) == 0)))
        monthdays[1] = 29;


    i=0;
    dw->Previous_toolbutton = gtk_event_box_new();
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(dw->Previous_toolbutton), FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(dw->Previous_toolbutton), 0);
    arrow = gtk_image_new_from_icon_name("pan-start-symbolic", GTK_ICON_SIZE_MENU);
    gtk_container_add(GTK_CONTAINER(dw->Previous_toolbutton), arrow);
    gtk_grid_attach(GTK_GRID(dw->dtable_h), dw->Previous_toolbutton, i, 0, 1, 1);
    gtk_widget_show_all(dw->Previous_toolbutton);
    g_signal_connect((gpointer)dw->Previous_toolbutton, "button_release_event"
            , G_CALLBACK(on_Previous_clicked), dw);
    tip = g_strdup_printf("Back %d days", days);
    CLAWS_SET_TIP(dw->Previous_toolbutton, tip);
    g_free(tip);
    for (i = 1; i < days+1; i++) {
        tip = g_malloc(100);
	strftime(tip, 99, "%A %d %B %Y", &tm_date);
	if (first_col_day == -1)
		first_col_day = tm_date.tm_mday;
        date = get_locale_date(&tm_date);
        button = gtk_button_new();
        gtk_button_set_label(GTK_BUTTON(button), date);
        if (strcmp(today, date) == 0) {
            gtk_widget_modify_bg(button, GTK_STATE_NORMAL, &dw->bg_today);
        }

        if (tm_date.tm_wday % 7 == 0) {
            label = gtk_bin_get_child(GTK_BIN(button));
            gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &dw->fg_sunday);
        }
        CLAWS_SET_TIP(button, tip);
	g_free(tip);
        gtk_widget_set_size_request(button, dw->StartDate_button_req.width, -1);
        g_signal_connect((gpointer)button, "clicked"
                , G_CALLBACK(header_button_clicked_cb), dw);
        g_object_set_data(G_OBJECT(button), "offset", GINT_TO_POINTER(tm_date.tm_mday*1000));
	gtk_widget_set_hexpand(button, TRUE);

	gtk_grid_attach(GTK_GRID(dw->dtable_h), button, i, 0, 1, 1);

        if (++tm_date.tm_mday == (int)(monthdays[tm_date.tm_mon]+1)) {
            if (++tm_date.tm_mon == 12) {
                ++tm_date.tm_year;
                tm_date.tm_mon = 0;
            }
            tm_date.tm_mday = 1;
        }
	++tm_date.tm_wday; tm_date.tm_wday %=7;
	g_free(date);
    }

    dw->Next_toolbutton = gtk_event_box_new();
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(dw->Next_toolbutton), FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(dw->Next_toolbutton), 0);
    arrow = gtk_image_new_from_icon_name("pan-end-symbolic", GTK_ICON_SIZE_MENU);
    gtk_container_add(GTK_CONTAINER(dw->Next_toolbutton), arrow);
    gtk_grid_attach(GTK_GRID(dw->dtable_h), dw->Next_toolbutton, i, 0, 1, 1);
    gtk_widget_show_all(dw->Next_toolbutton);
    g_signal_connect((gpointer)dw->Next_toolbutton, "button_release_event"
            , G_CALLBACK(on_Next_clicked), dw);
    tip = g_strdup_printf("Forward %d days", days);
    CLAWS_SET_TIP(dw->Next_toolbutton, tip);
    g_free(tip);
    g_free(today);

    /****** body of day table ******/
    dw->scroll_win = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(dw->scroll_win)
            , GTK_SHADOW_NONE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(dw->scroll_win)
            , GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
    gtk_scrolled_window_set_placement(GTK_SCROLLED_WINDOW(dw->scroll_win)
            , GTK_CORNER_TOP_LEFT);
    gtk_box_pack_start(GTK_BOX(dw->day_view_vbox), dw->scroll_win
            , TRUE, TRUE, 0);
    vp = gtk_viewport_new(NULL, NULL);
    gtk_viewport_set_shadow_type(GTK_VIEWPORT(vp), GTK_SHADOW_NONE);
    gtk_container_add(GTK_CONTAINER(dw->scroll_win), vp);
    dw->dtable = gtk_grid_new();
    gtk_container_add(GTK_CONTAINER(vp), dw->dtable);

    gtk_widget_show_all(dw->dtable_h);
    /* hours column = hour rows */
    for (i = 0; i < 24; i++) {
        g_sprintf(text, "%02d", i);
        /* ev is needed for background colour */
        fill_hour(dw, 0, i, text);
        fill_hour(dw, days+1, i, text);
    }
    fill_days(dw, days, dw->item, first_col_day);
}

void refresh_day_win(day_win *dw)
{
    get_scroll_position(dw);
    gtk_widget_destroy(dw->scroll_win_h);
    build_day_view_table(dw);
    gtk_widget_show_all(dw->scroll_win_h);
    /* I was not able to get this work without the timer. Ugly yes, but
     * it works and does not hurt - much */
    g_timeout_add(100, (GSourceFunc)scroll_position_timer, (gpointer)dw);
}

day_win *create_day_win(FolderItem *item, struct tm tmdate)
{
    day_win *dw;
    char *start_date = get_locale_date(&tmdate);

    /* initialisation + main window + base vbox */
    dw = g_new0(day_win, 1);
    dw->scroll_pos = -1; /* not set */

    dw->accel_group = gtk_accel_group_new();
    
    while (tmdate.tm_wday != 1)
    	orage_move_day(&tmdate, -1);
    
    dw->startdate = tmdate;
    dw->startdate.tm_hour = 0;
    dw->startdate.tm_min = 0;
    dw->startdate.tm_sec = 0;
    dw->Vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_name(GTK_WIDGET(dw->Vbox), "vcal_day_win");

    dw->item = item;
    build_day_view_colours(dw);
    build_day_view_header(dw, start_date);
    build_day_view_table(dw);
    gtk_widget_show_all(dw->Vbox);
    dw->selsig = vcal_view_set_calendar_page(dw->Vbox, 
		    G_CALLBACK(dw_summary_selected), dw);
    vcal_view_create_popup_menus(dw->Vbox, &dw->view_menu, 
		    		 &dw->event_menu, &dw->event_group,
		    		 &dw->ui_manager);

    g_timeout_add(100, (GSourceFunc)scroll_position_timer, (gpointer)dw);

    return(dw);
}
