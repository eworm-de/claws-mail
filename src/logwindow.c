/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999,2000 Hiroyuki Yamamoto
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtktext.h>
#include <gtk/gtkstyle.h>

#include "intl.h"
#include "logwindow.h"
#include "utils.h"
#include "gtkutils.h"
#include "prefs_common.h"

static LogWindow *logwindow;

static void key_pressed(GtkWidget *widget, GdkEventKey *event,
			LogWindow *logwin);
void log_window_clear(GtkWidget *text);

LogWindow *log_window_create(void)
{
	LogWindow *logwin;
	GtkWidget *window;
	GtkWidget *scrolledwin;
	GtkWidget *text;

	debug_print(_("Creating log window...\n"));
	logwin = g_new0(LogWindow, 1);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), _("Protocol log"));
	gtk_window_set_wmclass(GTK_WINDOW(window), "log_window", "Sylpheed");
	gtk_window_set_policy(GTK_WINDOW(window), TRUE, TRUE, FALSE);
	gtk_widget_set_usize(window, 520, 400);
	gtk_signal_connect(GTK_OBJECT(window), "delete_event",
			   GTK_SIGNAL_FUNC(gtk_widget_hide_on_delete), NULL);
	gtk_signal_connect(GTK_OBJECT(window), "key_press_event",
			   GTK_SIGNAL_FUNC(key_pressed), logwin);
	gtk_widget_realize(window);

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_container_add(GTK_CONTAINER(window), scrolledwin);
	gtk_widget_show(scrolledwin);

	text = gtk_text_new(gtk_scrolled_window_get_hadjustment
			    (GTK_SCROLLED_WINDOW(scrolledwin)),
			    gtk_scrolled_window_get_vadjustment
			    (GTK_SCROLLED_WINDOW(scrolledwin)));
	gtk_container_add(GTK_CONTAINER(scrolledwin), text);
	gtk_widget_show(text);

	logwin->window = window;
	logwin->scrolledwin = scrolledwin;
	logwin->text = text;

	logwindow = logwin;

	return logwin;
}

void log_window_init(LogWindow *logwin)
{
	GdkColormap *colormap;
	GdkColor color[3] =
		{{0, 0, 0xafff, 0}, {0, 0xefff, 0, 0}, {0, 0xefff, 0, 0}};
	gboolean success[3];
	gint i;

	gtkut_widget_disable_theme_engine(logwin->text);

	logwin->msg_color   = color[0];
	logwin->warn_color  = color[1];
	logwin->error_color = color[2];

	colormap = gdk_window_get_colormap(logwin->window->window);
	gdk_colormap_alloc_colors(colormap, color, 3, FALSE, TRUE, success);

	for (i = 0; i < 3; i++) {
		if (success[i] == FALSE) {
			GtkStyle *style;

			g_warning("LogWindow: color allocation failed\n");
			style = gtk_widget_get_style(logwin->window);
			logwin->msg_color = logwin->warn_color =
			logwin->error_color = style->black;
			break;
		}
	}
}

void log_window_show(LogWindow *logwin)
{
	gtk_widget_hide(logwin->window);
	gtk_widget_show(logwin->window);
}

void log_window_append(const gchar *str, LogType type)
{
	GtkText *text;
	GdkColor *color = NULL;
	gchar *head = NULL;

	g_return_if_fail(logwindow != NULL);

	text = GTK_TEXT(logwindow->text);

	switch (type) {
	case LOG_WARN:
		color = &logwindow->warn_color;
		head = "*** ";
		break;
	case LOG_ERROR:
		color = &logwindow->error_color;
		head = "*** ";
		break;
	case LOG_MSG:
		color = &logwindow->msg_color;
		break;
	default:
		break;
	}

	if (head) gtk_text_insert(text, NULL, color, NULL, head, -1);
	gtk_text_insert(text, NULL, color, NULL, str, -1);
	if (prefs_common.cliplog)
	       log_window_clear (GTK_WIDGET (text));
}

static void key_pressed(GtkWidget *widget, GdkEventKey *event,
			LogWindow *logwin)
{
	if (event && event->keyval == GDK_Escape)
		gtk_widget_hide(logwin->window);
}

void log_window_clear(GtkWidget *text)
{
        guint length;
	guint point;
	gchar *str;
	
	length = gtk_text_get_length (GTK_TEXT (text));
	debug_print(_("Log window length: %u"), length);
	
	if (length > prefs_common.loglength) {
	        /* find the end of the first line after the cut off
		 * point */
       	        point = length - prefs_common.loglength;
	        gtk_text_set_point (GTK_TEXT (text), point);
		str = gtk_editable_get_chars (GTK_EDITABLE (text),
					      point, point + 1);
		gtk_text_freeze (GTK_TEXT (text));
		while(str && *str != '\n')
		       str = gtk_editable_get_chars (GTK_EDITABLE (text),
						     ++point, point + 2);
		
		/* erase the text */
		gtk_text_set_point (GTK_TEXT (text), 0);
		if (!gtk_text_forward_delete (GTK_TEXT (text), point + 1))
		        debug_print (_("Error clearing log"));
		gtk_text_thaw (GTK_TEXT (text));
		gtk_text_set_point (GTK_TEXT (text),
				    gtk_text_get_length (GTK_TEXT (text)));
	}
	
}


