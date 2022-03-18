/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 *
 * Copyright (c) 2007-2008 Juha Kautto (juha at xfce.org)
 * Copyright (c) 2008-2021 the Claws Mail team and Colin Leroy
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
struct _month_win
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

    GtkWidget *month_view_vbox;
    GtkWidget *scroll_win_h;
    GtkWidget *dtable_h; /* header of day table */
    GtkWidget *scroll_win;
    GtkWidget *dtable;   /* day table */
    GtkRequisition hour_req;

    GtkWidget *header[MAX_DAYS];
    GtkWidget *element[6][MAX_DAYS];
    GtkWidget *line[6][MAX_DAYS];

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

gchar *dayname[7] = {
    	N_("Monday"),
    	N_("Tuesday"),
    	N_("Wednesday"),
    	N_("Thursday"),
    	N_("Friday"),
    	N_("Saturday"),
    	N_("Sunday")
	};
gchar *monthname[12] = {
    	N_("January"),
    	N_("February"),
    	N_("March"),
    	N_("April"),
    	N_("May"),
    	N_("June"),
    	N_("July"),
    	N_("August"),
    	N_("September"),
    	N_("October"),
    	N_("November"),
    	N_("December")
	};

static gchar *get_locale_date(struct tm *tmdate)
{
	gchar *d = g_malloc(100);
	strftime(d, 99, "%x", tmdate);
	return d;
}

void mw_close_window(month_win *mw)
{
    vcal_view_set_summary_page(mw->Vbox, mw->selsig);
    

    g_free(mw);
    mw = NULL;
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

static void changeSelectedDate(month_win *mw, gint month)
{
    gint curmon = mw->startdate.tm_mon;
    if (month > 0) {
     do { /* go to first of next month */
      orage_move_day(&(mw->startdate), 1);
     } while (curmon == mw->startdate.tm_mon);
    } else {
     do { /* go to last day of last month */
      orage_move_day(&(mw->startdate), -1);
     } while (curmon == mw->startdate.tm_mon);
     do { /* go to first of last month */
      orage_move_day(&(mw->startdate), -1);
     } while (mw->startdate.tm_mday > 1);
    }
}

static gint on_Previous_clicked(GtkWidget *button, GdkEventButton *event,
				    month_win *mw)
{
    changeSelectedDate(mw, -1);
    refresh_month_win(mw);
    return TRUE;
}

static gint on_Next_clicked(GtkWidget *button, GdkEventButton *event,
				    month_win *mw)
{
    changeSelectedDate(mw, +1);
    refresh_month_win(mw);
    return TRUE;
}

static void mw_summary_selected(GtkCMCTree *ctree, GtkCMCTreeNode *row,
			     gint column, month_win *mw)
{
	MsgInfo *msginfo = gtk_cmctree_node_get_row_data(ctree, row);
	
	if (msginfo && msginfo->msgid) {
		VCalEvent *event = vcal_manager_load_event(msginfo->msgid);
		if (event) {
			struct tm tm_start;
			time_t t_start = icaltime_as_timet(icaltime_from_string(event->dtstart));
			gboolean changed = FALSE;

#ifdef G_OS_WIN32
			if (t_start < 0)
				t_start = 1;
#endif
			localtime_r(&t_start, &tm_start);
			while (tm_start.tm_year < mw->startdate.tm_year) {
				changeSelectedDate(mw, -1);
				changed = TRUE;
			}
			while (tm_start.tm_year > mw->startdate.tm_year) {
				changeSelectedDate(mw, +1);
				changed = TRUE;
			}
			while (tm_start.tm_mon < mw->startdate.tm_mon) {
				changeSelectedDate(mw, -1);
				changed = TRUE;
			}
			while (tm_start.tm_mon > mw->startdate.tm_mon) {
				changeSelectedDate(mw, +1);
				changed = TRUE;
			}
			if (changed)
				refresh_month_win(mw);
		}
		vcal_manager_free_event(event);
	}
}

static void month_view_new_meeting_cb(month_win *mw, gpointer data_i, gpointer data_s)
{
    int offset = GPOINTER_TO_INT(data_i);
    struct tm tm_date = mw->startdate;

    while (offset > tm_date.tm_mday) {
	    orage_move_day(&tm_date, 1);
    }
    while (offset < tm_date.tm_mday) {
	    orage_move_day(&tm_date, -1);
    }
    tm_date.tm_hour = 0;
    vcal_meeting_create_with_start(NULL, &tm_date);
}

static void month_view_edit_meeting_cb(month_win *mw, gpointer data_i, gpointer data_s)
{
	const gchar *uid = (gchar *)data_s;
        vcal_view_select_event (uid, mw->item, TRUE,
		    	    G_CALLBACK(mw_summary_selected), mw);
}

static void month_view_cancel_meeting_cb(month_win *mw, gpointer data_i, gpointer data_s)
{
	const gchar *uid = (gchar *)data_s;
	vcalendar_cancel_meeting(mw->item, uid);
}

static void month_view_today_cb(month_win *mw, gpointer data_i, gpointer data_s)
{
    time_t now = time(NULL);
    struct tm tm_today;
    localtime_r(&now, &tm_today);

    while (tm_today.tm_mday != 1)
    	orage_move_day(&tm_today, -1);
    
    mw->startdate = tm_today;
    refresh_month_win(mw);
}

static void header_button_clicked_cb(GtkWidget *button
        , GdkEventButton *event, gpointer *user_data)
{
    month_win *mw = (month_win *)user_data;
    int offset = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "day"));

    if (event->button == 1 && event->type == GDK_2BUTTON_PRESS) {
	    month_view_new_meeting_cb(mw, GINT_TO_POINTER(offset), NULL);
    }
    if (event->button == 3) {
		g_object_set_data(G_OBJECT(mw->Vbox), "menu_win",
			  mw);
		g_object_set_data(G_OBJECT(mw->Vbox), "menu_data_i",
			  GINT_TO_POINTER(offset));
		g_object_set_data(G_OBJECT(mw->Vbox), "menu_data_s",
			  NULL);
		g_object_set_data(G_OBJECT(mw->Vbox), "new_meeting_cb",
			  month_view_new_meeting_cb);
		g_object_set_data(G_OBJECT(mw->Vbox), "go_today_cb",
			  month_view_today_cb);
		gtk_menu_popup_at_pointer(GTK_MENU(mw->view_menu), NULL);
    }
}

static void on_button_press_event_cb(GtkWidget *widget
        , GdkEventButton *event, gpointer *user_data)
{
    month_win *mw = (month_win *)user_data;
    gchar *uid = g_object_get_data(G_OBJECT(widget), "UID");
    gpointer offset = g_object_get_data(G_OBJECT(widget), "offset");

    if (event->button == 1) {
	if (uid)
            vcal_view_select_event (uid, mw->item, (event->type==GDK_2BUTTON_PRESS),
		    	    G_CALLBACK(mw_summary_selected), mw);
	else if (event->type == GDK_2BUTTON_PRESS) {
	    month_view_new_meeting_cb(mw, GINT_TO_POINTER(offset), NULL);
	}
    }
    if (event->button == 3) {
	    g_object_set_data(G_OBJECT(mw->Vbox), "menu_win",
		      mw);
	    g_object_set_data(G_OBJECT(mw->Vbox), "menu_data_i",
		      offset);
	    g_object_set_data(G_OBJECT(mw->Vbox), "menu_data_s",
		      uid);
	    g_object_set_data(G_OBJECT(mw->Vbox), "new_meeting_cb",
		      month_view_new_meeting_cb);
	    g_object_set_data(G_OBJECT(mw->Vbox), "edit_meeting_cb",
		      month_view_edit_meeting_cb);
	    g_object_set_data(G_OBJECT(mw->Vbox), "cancel_meeting_cb",
		      month_view_cancel_meeting_cb);
	    g_object_set_data(G_OBJECT(mw->Vbox), "go_today_cb",
		      month_view_today_cb);
	    gtk_menu_popup_at_pointer(GTK_MENU(mw->event_menu), NULL);
    }
}

static void add_row(month_win *mw, VCalEvent *event, gint days)
{
    gint row, start_row, end_row, first_row, last_row;
    gint col, start_col, end_col, first_col, last_col;
    gint width, start_width, end_width;
    gchar *text, *tip, *start_date, *end_date;
    GtkWidget *ev = NULL, *lab = NULL, *hb;
    time_t t_start, t_end;
    struct tm tm_first, tm_start, tm_end;
    guint monthdays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int weekoffset = -1;
    gboolean pack = TRUE, update_tip = FALSE;
    time_t now = time(NULL);
    struct tm tm_today;
    gboolean start_prev_mon = FALSE;

    localtime_r(&now, &tm_today);

    tm_today.tm_year += 1900;
    tm_today.tm_mon++;

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
    tm_first = mw->startdate;
   
    tm_first.tm_year += 1900;

    if (((tm_first.tm_year%4) == 0) 
    && (((tm_first.tm_year%100) != 0) || ((tm_first.tm_year%400) == 0)))
        ++monthdays[1]; /* leap year, february has 29 days */


    tm_first.tm_mon += 1;
    tm_start.tm_year += 1900;
    tm_start.tm_mon += 1;
    tm_end.tm_year += 1900;
    tm_end.tm_mon += 1;

    start_col = orage_days_between(&tm_first, &tm_start)+1;
    end_col   = orage_days_between(&tm_first, &tm_end)+1;

    if (start_col < 0)
        start_prev_mon = TRUE;

    if (end_col < 1) {
    	return;
    }
    if (start_col > 0 && start_col > monthdays[tm_first.tm_mon-1]) {
        return;
    }
    else {
        GDate *edate = g_date_new_dmy(tm_end.tm_mday, tm_end.tm_mon, tm_end.tm_year);
        GDate *fdate = g_date_new_dmy(1, tm_first.tm_mon, tm_first.tm_year);
        GDate *sdate;
        if (start_col >= 1) {
          sdate = g_date_new_dmy(tm_start.tm_mday, tm_start.tm_mon, tm_start.tm_year);
        } else {
          sdate = g_date_new_dmy(1, tm_first.tm_mon, tm_first.tm_year);
        } /* endif */

	col = start_col = (int)g_date_get_weekday(sdate);
	end_col = (int)g_date_get_weekday(edate);
	weekoffset = (int)g_date_get_monday_week_of_year(fdate);
	row = start_row = (int)g_date_get_monday_week_of_year(sdate) - weekoffset;
	end_row = (int)g_date_get_monday_week_of_year(edate) - weekoffset;
	g_date_free(fdate);
	g_date_free(sdate);
	g_date_free(edate);
    }

    if (end_col > 7)
       end_col = 7;

    /* then add the appointment */
    text = g_strdup(event->summary?event->summary : _("Unknown"));

    if (mw->element[row][col] == NULL) {
        hb = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
        mw->element[row][col] = hb;
    }
    else {
        GList *children = NULL;
        hb = mw->element[row][col];
        /* FIXME: set some real bar here to make it visible that we
         * have more than 1 appointment here
         */
	children = gtk_container_get_children(GTK_CONTAINER(hb));
	if (g_list_length(children) > 2) {
		pack = FALSE;
		update_tip = TRUE;
	} else if (g_list_length(children) > 1) {
		pack = FALSE;
		update_tip = FALSE;
	}
	g_list_free(children);
    }
    if (pack || !update_tip) {
       ev = gtk_event_box_new();
       lab = gtk_label_new(text);
       gtk_label_set_xalign(GTK_LABEL(lab), 0.0);
       gtk_label_set_yalign(GTK_LABEL(lab), 0.0);
       gtk_label_set_ellipsize(GTK_LABEL(lab), PANGO_ELLIPSIZE_END);
       if ((row % 2) == 1)
           gtk_widget_modify_bg(ev, GTK_STATE_NORMAL, &mw->bg1);
       else
           gtk_widget_modify_bg(ev, GTK_STATE_NORMAL, &mw->bg2);
    } else if (!pack && update_tip) {
       GList *children = gtk_container_get_children(GTK_CONTAINER(hb));
       ev = GTK_WIDGET(g_list_last(children)->data);
       g_list_free(children);
    }

    if (ev && tm_start.tm_mday == tm_today.tm_mday && tm_start.tm_mon == tm_today.tm_mon 
        && tm_start.tm_year == tm_today.tm_year)
	gtk_widget_modify_bg(ev, GTK_STATE_NORMAL, &mw->bg_today);

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
    if (pack) {
        gtk_container_add(GTK_CONTAINER(ev), lab);
	CLAWS_SET_TIP(ev, tip);
        gtk_box_pack_start(GTK_BOX(hb), ev, TRUE, TRUE, 0);
    } else if (!update_tip) {
        gtk_label_set_label(GTK_LABEL(lab), "...");
        gtk_container_add(GTK_CONTAINER(ev), lab);
	CLAWS_SET_TIP(ev, tip);
        gtk_box_pack_start(GTK_BOX(hb), ev, TRUE, TRUE, 0);
    } else {
	gchar *old = gtk_widget_get_tooltip_text(ev);
	gchar *new = g_strdup_printf("%s\n\n%s", old?old:"", tip);
	g_free(old);

	CLAWS_SET_TIP(ev, new);
	g_free(new);
    }
    g_object_set_data_full(G_OBJECT(ev), "UID", g_strdup(event->uid), g_free);
    g_object_set_data(G_OBJECT(ev), "offset", GINT_TO_POINTER(tm_start.tm_mday));
    g_signal_connect((gpointer)ev, "button-press-event"
            , G_CALLBACK(on_button_press_event_cb), mw);
    g_free(tip);
    g_free(text);

    /* and finally draw the line to show how long the appointment is,
     * but only if it is Busy type event (=availability != 0) 
     * and it is not whole day event */
    if (TRUE) {
        width = mw->StartDate_button_req.width;
        /*
         * same_date = !strncmp(start_ical_time, end_ical_time, 8);
         * */
        if (start_row < 0)
            first_row = 0;
        else
            first_row = start_row;
        if (end_row > 5)
            last_row = 5;
        else
            last_row = end_row;
        for (row = first_row; row <= last_row; row++) {
            if (row == start_row)
                first_col = start_col;
            else
                first_col = 0;
            if (row == end_row)
                last_col   = end_col;
            else
                last_col   = 7;
            for (col = first_col; col <= last_col; col++) {
                if (row == start_row && col == start_col && !start_prev_mon)
                    start_width = ((tm_start.tm_hour*60)+(tm_start.tm_min))*width/(24*60);
                else
                    start_width = 0;
                if (row == last_row && col == last_col)
                    end_width = ((tm_end.tm_hour*60)+(tm_end.tm_min))*width/(24*60);
                else
                    end_width = width;

                mw->line[row][col] = build_line(start_width, 0
                        , end_width-start_width, 2, mw->line[row][col]
			, &mw->line_color);
            }
        }
    }
}

static void app_rows(month_win *mw, FolderItem *item)
{
   GSList *events = vcal_get_events_list(item);
   GSList *cur = NULL;
   int days = 7;
   for (cur = events; cur ; cur = cur->next) {
   	VCalEvent *event = (VCalEvent *) (cur->data);
	add_row(mw, event, days);
	vcal_manager_free_event(event);
   }
   g_slist_free(events);
}

static void app_data(month_win *mw, FolderItem *item)
{
    app_rows(mw, item);
}


static void fill_days(month_win *mw, gint days, FolderItem *item)
{
    gint day, col, height, width;
    GtkWidget *ev, *vb, *hb;
    guint monthdays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    struct tm t = mw->startdate;
    int weekoffset = -1;
    time_t now = time(NULL);
    struct tm tm_today;
    
    localtime_r(&now, &tm_today);

    t.tm_year += 1900;
    t.tm_mon++;
    tm_today.tm_year += 1900;
    tm_today.tm_mon++;
    
    if (((t.tm_year%4) == 0) 
    && (((t.tm_year%100) != 0) || ((t.tm_year%400) == 0)))
        ++monthdays[1]; /* leap year, february has 29 days */

    height = mw->StartDate_button_req.height;
    width = mw->StartDate_button_req.width;

    /* first clear the structure */
    for (col = 1; col <  days+1; col++) {
        mw->header[col] = NULL;
    }
    for (day = 0; day < MAX_DAYS; day++)
    	for (col = 0; col < 6; col++)
		mw->line[col][day] = NULL;
    for (day = 1; day <= monthdays[t.tm_mon-1]; day++) {
	GDate *date = g_date_new_dmy(day, t.tm_mon, t.tm_year);
	int dcol = (int)g_date_get_weekday(date);
	int row = (int)g_date_get_monday_week_of_year(date);
	if (weekoffset == -1) {
		weekoffset = row;
		row = 0;
	} else {
		row = row - weekoffset;
	}
        mw->element[row][dcol] = NULL;
        mw->line[row][dcol] = build_line(0, 0, width, 3, NULL
			, &mw->line_color);
	g_date_free(date);
    }

    app_data(mw, item);

    /* check rows */
    for (day = 1; day <= monthdays[t.tm_mon-1]; day++) {
	GDate *date = g_date_new_dmy(day, t.tm_mon, t.tm_year);
	int col = (int)g_date_get_weekday(date);
	int row = (int)g_date_get_monday_week_of_year(date);
	gchar *label;
	gchar *tmp;
	GtkWidget *name;

	if (weekoffset == -1) {
		weekoffset = row;
		row = 0;
	} else {
		row = row - weekoffset;
	}
        vb = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_set_size_request(vb, width, height);
	gtk_widget_set_hexpand(vb, TRUE);
	gtk_widget_set_vexpand(vb, TRUE);
	    if (g_date_get_day(date) == 1)
    	        label = g_strdup_printf("%d %s", g_date_get_day(date),
				_(monthname[g_date_get_month(date)-1]));
	    else
	        label = g_strdup_printf("%d", g_date_get_day(date));
	    tmp = g_strdup_printf("%s %d %s %d",
	    			_(dayname[col-1]),
				g_date_get_day(date),
				_(monthname[g_date_get_month(date)-1]),
				g_date_get_year(date));

            ev = gtk_event_box_new();
	    g_object_set_data(G_OBJECT(ev), "day", GINT_TO_POINTER((int)g_date_get_day(date)));
	    g_signal_connect((gpointer)ev, "button-press-event"
        	    , G_CALLBACK(header_button_clicked_cb), mw);
            name = gtk_label_new(label);
	    gtk_label_set_xalign(GTK_LABEL(name), 0.0);
	    gtk_label_set_yalign(GTK_LABEL(name), 0.0);

	    CLAWS_SET_TIP(ev, tmp);
            gtk_container_add(GTK_CONTAINER(ev), name);
	    g_free(tmp);
            g_free(label);

            if ((row % 2) == 1)
                gtk_widget_modify_bg(ev, GTK_STATE_NORMAL, &mw->bg1);
            else
                gtk_widget_modify_bg(ev, GTK_STATE_NORMAL, &mw->bg2);
	    if (col == 7)
	    	gtk_widget_modify_fg(name, GTK_STATE_NORMAL, &mw->fg_sunday);
	    if (day == tm_today.tm_mday && t.tm_mon == tm_today.tm_mon && t.tm_year == tm_today.tm_year)
	    	gtk_widget_modify_bg(ev, GTK_STATE_NORMAL, &mw->bg_today);

            hb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
            gtk_box_pack_start(GTK_BOX(hb), ev, TRUE, TRUE, 1);
            gtk_box_pack_start(GTK_BOX(vb), hb, TRUE, TRUE, 0);
            if (mw->element[row][col]) {
		gtk_box_pack_start(GTK_BOX(vb), mw->element[row][col], TRUE, TRUE, 0);
            }
            gtk_box_pack_start(GTK_BOX(vb), mw->line[row][col]
                    , FALSE, FALSE, 0);

	gtk_grid_attach(GTK_GRID(mw->dtable), vb, col, row, 1, 1);
	g_date_free(date);
    }
}

static void build_month_view_header(month_win *mw, char *start_date)
{
    GtkWidget *hbox, *label, *space_label;

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

    label = gtk_label_new(_("Start"));
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 10);

    /* start date button */
    mw->StartDate_button = gtk_button_new();
    gtk_box_pack_start(GTK_BOX(hbox), mw->StartDate_button, FALSE, FALSE, 0);

    space_label = gtk_label_new("  ");
    gtk_box_pack_start(GTK_BOX(hbox), space_label, FALSE, FALSE, 0);

    space_label = gtk_label_new("     ");
    gtk_box_pack_start(GTK_BOX(hbox), space_label, FALSE, FALSE, 0);

    label = gtk_label_new(_("Show"));
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 10);

    /* show days spin = how many days to show */
    mw->day_spin = gtk_spin_button_new_with_range(1, MAX_DAYS, 1);
    gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(mw->day_spin), TRUE);
    gtk_widget_set_size_request(mw->day_spin, 40, -1);
    gtk_box_pack_start(GTK_BOX(hbox), mw->day_spin, FALSE, FALSE, 0);
    label = gtk_label_new(_("days"));
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);

    space_label = gtk_label_new("   ");
    gtk_box_pack_start(GTK_BOX(hbox), space_label, FALSE, FALSE, 0);

    /* sizes */
    gtk_button_set_label(GTK_BUTTON(mw->StartDate_button)
            , (const gchar *)start_date);
    gtk_widget_get_preferred_size(mw->StartDate_button, NULL, &mw->StartDate_button_req);
    mw->StartDate_button_req.width += mw->StartDate_button_req.width/10;
    label = gtk_label_new("00");
    gtk_widget_get_preferred_size(label, NULL, &mw->hour_req);
}

static void build_month_view_colours(month_win *mw)
{
    GtkStyle *def_style, *cur_style;
    GtkWidget *ctree = NULL;
    def_style = gtk_widget_get_default_style();
    
    if (mainwindow_get_mainwindow()) {
        ctree = mainwindow_get_mainwindow()->summaryview->ctree;
    }
    if (ctree) {
        cur_style = gtk_widget_get_style(ctree);
        mw->bg1 = cur_style->bg[GTK_STATE_NORMAL];
        mw->bg2 = cur_style->bg[GTK_STATE_NORMAL];
    } else {
        mw->bg1 = def_style->bg[GTK_STATE_NORMAL];
        mw->bg2 = def_style->bg[GTK_STATE_NORMAL];
    }

    mw->bg1.red +=  (mw->bg1.red < 63000 ? 2000 : -2000);
    mw->bg1.green += (mw->bg1.green < 63000 ? 2000 : -2000);
    mw->bg1.blue += (mw->bg1.blue < 63000 ? 2000 : -2000);

    mw->bg2.red +=  (mw->bg2.red > 1000 ? -1000 : 1000);
    mw->bg2.green += (mw->bg2.green > 1000 ? -1000 : 1000);
    mw->bg2.blue += (mw->bg2.blue > 1000 ? -1000 : 1000);

    if (!gdk_color_parse("white", &mw->line_color)) {
        g_warning("color parse failed: white");
        mw->line_color.red =  239 * (65535/255);
        mw->line_color.green = 235 * (65535/255);
        mw->line_color.blue = 230 * (65535/255);
    }

    if (!gdk_color_parse("blue", &mw->fg_sunday)) {
        g_warning("color parse failed: blue");
        mw->fg_sunday.red = 10 * (65535/255);
        mw->fg_sunday.green = 10 * (65535/255);
        mw->fg_sunday.blue = 255 * (65535/255);
    }

    if (!gdk_color_parse("gold", &mw->bg_today)) {
        g_warning("color parse failed: gold");
        mw->bg_today.red = 255 * (65535/255);
        mw->bg_today.green = 215 * (65535/255);
        mw->bg_today.blue = 115 * (65535/255);
    }

    if (ctree) {
        cur_style = gtk_widget_get_style(ctree);
        mw->fg_sunday.red = (mw->fg_sunday.red + cur_style->fg[GTK_STATE_SELECTED].red)/2;
        mw->fg_sunday.green = (mw->fg_sunday.green + cur_style->fg[GTK_STATE_SELECTED].red)/2;
        mw->fg_sunday.blue = (3*mw->fg_sunday.blue + cur_style->fg[GTK_STATE_SELECTED].red)/4;
        mw->bg_today.red = (3*mw->bg_today.red + cur_style->bg[GTK_STATE_NORMAL].red)/4;
        mw->bg_today.green = (3*mw->bg_today.green + cur_style->bg[GTK_STATE_NORMAL].red)/4;
        mw->bg_today.blue = (3*mw->bg_today.blue + cur_style->bg[GTK_STATE_NORMAL].red)/4;
    }
}

static void fill_hour(month_win *mw, gint col, gint row, char *text)
{
    GtkWidget *name, *ev;

    ev = gtk_event_box_new();
    name = gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(name), 0.0);
    CLAWS_SET_TIP(ev, _("Week number"));
    gtk_container_add(GTK_CONTAINER(ev), name);
    gtk_widget_set_size_request(ev, mw->hour_req.width
            , mw->StartDate_button_req.height);
    if (text)
	gtk_grid_attach(GTK_GRID(mw->dtable), ev, col, row, 1, 1);
    else  /* special, needed for header table full day events */
	gtk_grid_attach(GTK_GRID(mw->dtable_h), ev, col, row, 1, 1);
}

static void build_month_view_table(month_win *mw)
{
    gint days;   /* number of days to show */
    gint i = 0;
    GtkWidget *button;
    struct tm tm_date, tm_today;
    guint monthdays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    GtkWidget *vp;
    time_t t = time(NULL);
    GtkWidget *arrow;
    int avail_w = 0, avail_d = 7;
    int avail_h = 0;
    int weekoffset = -1;
    GDate *date;
    int first_week = 0;

    if (mainwindow_get_mainwindow()) {
		GtkAllocation allocation;
		SummaryView *summaryview = mainwindow_get_mainwindow()->summaryview;
		GTK_EVENTS_FLUSH();
		gtk_widget_get_allocation(summaryview->mainwidget_book,
				&allocation);

		avail_w = allocation.width - 25 - 2*(mw->hour_req.width);
		avail_h = allocation.height - 20;
		if (avail_h < 250)
			avail_h = 250;
		/* avail_d = avail_w / mw->StartDate_button_req.width; */
	}

    gtk_widget_set_size_request(mw->StartDate_button, avail_w / avail_d, 
    			(avail_h)/6);
    gtk_widget_get_preferred_size(mw->StartDate_button, &mw->StartDate_button_req,
	    			  NULL);
   
    /* initial values */
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(mw->day_spin), avail_d);

#ifdef G_OS_WIN32
	if (t < 0)
		t = 1;
#endif
    localtime_r(&t, &tm_today);
    days = 7;
    /****** header of day table = days columns ******/
    mw->scroll_win_h = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(mw->scroll_win_h)
            , GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
    gtk_box_pack_start(GTK_BOX(mw->Vbox), mw->scroll_win_h
            , TRUE, TRUE, 0);
    mw->month_view_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(mw->scroll_win_h), mw->month_view_vbox);
    /* row 1= day header buttons 
     * row 2= full day events after the buttons */
    mw->dtable_h = gtk_grid_new();
    gtk_box_pack_start(GTK_BOX(mw->month_view_vbox), mw->dtable_h
            , FALSE, FALSE, 0);

    tm_date = mw->startdate;

    if (((tm_date.tm_year%4) == 0) && (((tm_date.tm_year%100) != 0) 
            || ((tm_date.tm_year%400) == 0)))
        ++monthdays[1];


    i=0;
    mw->Previous_toolbutton = gtk_event_box_new();
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(mw->Previous_toolbutton), FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(mw->Previous_toolbutton), 0);
    arrow = gtk_image_new_from_icon_name("pan-start-symbolic", GTK_ICON_SIZE_MENU);
    gtk_container_add(GTK_CONTAINER(mw->Previous_toolbutton), arrow);
    gtk_grid_attach(GTK_GRID(mw->dtable_h), mw->Previous_toolbutton, i, 0, 1, 1);
    gtk_widget_show_all(mw->Previous_toolbutton);
    g_signal_connect((gpointer)mw->Previous_toolbutton, "button_release_event"
            , G_CALLBACK(on_Previous_clicked), mw);
    CLAWS_SET_TIP(mw->Previous_toolbutton, _("Previous month"));
    for (i = 1; i < days+1; i++) {
        button = gtk_label_new(_(dayname[i-1]));

        gtk_widget_set_size_request(button, mw->StartDate_button_req.width, -1);
        g_object_set_data(G_OBJECT(button), "offset", GINT_TO_POINTER(i-1));
	gtk_widget_set_hexpand(button, TRUE);
	gtk_grid_attach(GTK_GRID(mw->dtable_h), button, i, 0, 1, 1);
    }

    mw->Next_toolbutton = gtk_event_box_new();
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(mw->Next_toolbutton), FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(mw->Next_toolbutton), 0);
    arrow = gtk_image_new_from_icon_name("pan-end-symbolic", GTK_ICON_SIZE_MENU);
    gtk_container_add(GTK_CONTAINER(mw->Next_toolbutton), arrow);
    gtk_grid_attach(GTK_GRID(mw->dtable_h), mw->Next_toolbutton, i, 0, 1, 1);
    gtk_widget_show_all(mw->Next_toolbutton);
    g_signal_connect((gpointer)mw->Next_toolbutton, "button_release_event"
            , G_CALLBACK(on_Next_clicked), mw);
    CLAWS_SET_TIP(mw->Next_toolbutton, _("Next month"));

    /****** body of day table ******/
    mw->scroll_win = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(mw->scroll_win)
            , GTK_SHADOW_NONE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(mw->scroll_win)
            , GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_placement(GTK_SCROLLED_WINDOW(mw->scroll_win)
            , GTK_CORNER_TOP_LEFT);
    gtk_box_pack_start(GTK_BOX(mw->month_view_vbox), mw->scroll_win
            , TRUE, TRUE, 0);
    vp = gtk_viewport_new(NULL, NULL);
    gtk_viewport_set_shadow_type(GTK_VIEWPORT(vp), GTK_SHADOW_IN);
    gtk_container_add(GTK_CONTAINER(mw->scroll_win), vp);
    mw->dtable = gtk_grid_new();
    gtk_container_add(GTK_CONTAINER(vp), mw->dtable);

    gtk_widget_show_all(mw->dtable_h);

    date = g_date_new_dmy(1, 1, tm_date.tm_year+1900);
    first_week = g_date_get_monday_week_of_year(date);

    if (first_week == 0)
    	first_week = 1;
    else
        first_week = 0;
    g_date_free(date);

    /* hours column = hour rows */
    for (i = 0; i <= 6; i++) {
	int day;
	for (day = 1; day <= monthdays[tm_date.tm_mon]; day++) {
	    date = g_date_new_dmy(day, tm_date.tm_mon+1, tm_date.tm_year+1900);
	    int row = (int)g_date_get_monday_week_of_year(date);
	    if (weekoffset == -1) {
		weekoffset = row;
	    }
	    if (row - weekoffset == i) {
	        gchar *wn = g_strdup_printf("%d", row+first_week > 53?1:row+first_week);
                fill_hour(mw, 0, i, wn);
                fill_hour(mw, days+1, i, "");
		g_free(wn);
		g_date_free(date);
		break;
	    }
	    g_date_free(date);
	}
    }
    fill_days(mw, days, mw->item);
}

void refresh_month_win(month_win *mw)
{
    gtk_widget_destroy(mw->scroll_win_h);
    build_month_view_table(mw);
    gtk_widget_show_all(mw->scroll_win_h);
}

month_win *create_month_win(FolderItem *item, struct tm tmdate)
{
    month_win *mw;
    char *start_date = get_locale_date(&tmdate);

    /* initialisation + main window + base vbox */
    mw = g_new0(month_win, 1);
    mw->scroll_pos = -1; /* not set */

    mw->accel_group = gtk_accel_group_new();

    while (tmdate.tm_mday != 1)
    	orage_move_day(&tmdate, -1);

    mw->startdate = tmdate;

    mw->Vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_name(GTK_WIDGET(mw->Vbox), "vcal_month_win");

    mw->item = item;
    build_month_view_colours(mw);
    build_month_view_header(mw, start_date);
    build_month_view_table(mw);
    gtk_widget_show_all(mw->Vbox);
    mw->selsig = vcal_view_set_calendar_page(mw->Vbox, 
		    G_CALLBACK(mw_summary_selected), mw);

    vcal_view_create_popup_menus(mw->Vbox, &mw->view_menu,
		    		 &mw->event_menu, &mw->event_group,
		    		 &mw->ui_manager);
    return(mw);
}
