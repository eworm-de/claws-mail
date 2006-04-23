/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2006 Hiroyuki Yamamoto and the Sylpheed-Claws team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtktextview.h>
#include <gtk/gtkstyle.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkseparatormenuitem.h>

#include "logwindow.h"
#include "utils.h"
#include "gtkutils.h"
#include "log.h"
#include "hooks.h"
#include "prefs_common.h"

static void hide_cb				(GtkWidget	*widget,
						 LogWindow	*logwin);
static gboolean key_pressed			(GtkWidget	*widget,
						 GdkEventKey	*event,
						 LogWindow	*logwin);
static void size_allocate_cb	(GtkWidget *widget,
					 GtkAllocation *allocation);
static gboolean log_window_append		(gpointer 	 source,
						 gpointer   	 data);
static void log_window_clip			(GtkWidget 	*text,
						 guint		 glip_length);
static void log_window_clear			(GtkWidget	*widget,
						 LogWindow	*logwin);
static void log_window_popup_menu_extend	(GtkTextView	*textview,
						 GtkMenu	*menu,
						 LogWindow	*logwin);
					 
/*!
 *\brief	Save Gtk object size to prefs dataset
 */
static void size_allocate_cb(GtkWidget *widget,
					 GtkAllocation *allocation)
{
	g_return_if_fail(allocation != NULL);

	prefs_common.logwin_width = allocation->width;
	prefs_common.logwin_height = allocation->height;
}

LogWindow *log_window_create(void)
{
	LogWindow *logwin;
	GtkWidget *window;
	GtkWidget *scrolledwin;
	GtkWidget *text;
	GtkTextBuffer *buffer;
	GtkTextIter iter;
	static GdkGeometry geometry;

	debug_print("Creating log window...\n");

	logwin = g_new0(LogWindow, 1);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), _("Protocol log"));
	gtk_window_set_resizable(GTK_WINDOW(window), TRUE);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(gtk_widget_hide_on_delete), NULL);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(key_pressed), logwin);
	g_signal_connect(G_OBJECT(window), "hide",
			 G_CALLBACK(hide_cb), logwin);
	gtk_widget_realize(window);

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwin),
					    GTK_SHADOW_IN);
	gtk_container_add(GTK_CONTAINER(window), scrolledwin);
	gtk_widget_show(scrolledwin);

	text = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(text), FALSE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text), GTK_WRAP_WORD);
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
	gtk_text_buffer_get_start_iter(buffer, &iter);
	gtk_text_buffer_create_mark(buffer, "end", &iter, FALSE);
	g_signal_connect(G_OBJECT(text), "populate-popup",
			 G_CALLBACK(log_window_popup_menu_extend), logwin);
	gtk_container_add(GTK_CONTAINER(scrolledwin), text);
	gtk_widget_show(text);

	g_signal_connect(G_OBJECT(window), "size_allocate",
			 G_CALLBACK(size_allocate_cb), NULL);

	if (!geometry.min_height) {
		geometry.min_width = 520;
		geometry.min_height = 400;
	}

	gtk_window_set_geometry_hints(GTK_WINDOW(window), NULL, &geometry,
				      GDK_HINT_MIN_SIZE);
	gtk_widget_set_size_request(window, prefs_common.logwin_width,
				    prefs_common.logwin_height);

	logwin->window = window;
	logwin->scrolledwin = scrolledwin;
	logwin->text = text;
	logwin->hook_id = hooks_register_hook(LOG_APPEND_TEXT_HOOKLIST, log_window_append, logwin);

	return logwin;
}

void log_window_init(LogWindow *logwin)
{
	GtkTextBuffer *buffer;
	GdkColormap *colormap;
	GdkColor color[5] =
		{{0, 0, 0xafff, 0}, {0, 0xefff, 0, 0}, {0, 0xefff, 0, 0},
		  {0, 0, 0, 0}, {0, 0, 0, 0xefff}};
	gboolean success[5];
	gint i;

	logwin->msg_color   = color[0];
	logwin->warn_color  = color[1];
	logwin->error_color = color[2];
	logwin->in_color    = color[3];
	logwin->out_color   = color[4];

	colormap = gdk_drawable_get_colormap(logwin->window->window);
	gdk_colormap_alloc_colors(colormap, color, 5, FALSE, TRUE, success);

	for (i = 0; i < 5; i++) {
		if (success[i] == FALSE) {
			GtkStyle *style;

			g_warning("LogWindow: color allocation failed\n");
			style = gtk_widget_get_style(logwin->window);
			logwin->msg_color = logwin->warn_color =
			logwin->error_color = logwin->in_color =
			logwin->out_color = style->black;
			break;
		}
	}

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(logwin->text));
	gtk_text_buffer_create_tag(buffer, "message",
				   "foreground-gdk", &logwin->msg_color,
				   NULL);
	gtk_text_buffer_create_tag(buffer, "warn",
				   "foreground-gdk", &logwin->warn_color,
				   NULL);
	gtk_text_buffer_create_tag(buffer, "error",
				   "foreground-gdk", &logwin->error_color,
				   NULL);
	gtk_text_buffer_create_tag(buffer, "input",
				   "foreground-gdk", &logwin->in_color,
				   NULL);
	gtk_text_buffer_create_tag(buffer, "output",
				   "foreground-gdk", &logwin->out_color,
				   NULL);
}

void log_window_show(LogWindow *logwin)
{
	GtkTextView *text = GTK_TEXT_VIEW(logwin->text);
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(text);
	GtkTextMark *mark;

	mark = gtk_text_buffer_get_mark(buffer, "end");
	gtk_text_view_scroll_mark_onscreen(text, mark);

	gtk_widget_show(logwin->window);
}

void log_window_set_clipping(LogWindow *logwin, gboolean clip, guint clip_length)
{
	g_return_if_fail(logwin != NULL);

	logwin->clip = clip;
	logwin->clip_length = clip_length;
}

static gboolean log_window_append(gpointer source, gpointer data)
{
	LogText *logtext = (LogText *) source;
	LogWindow *logwindow = (LogWindow *) data;
	GtkTextView *text;
	GtkTextBuffer *buffer;
	GtkTextIter iter;
	gchar *head = NULL;
	const gchar *tag;

	g_return_val_if_fail(logtext != NULL, TRUE);
	g_return_val_if_fail(logtext->text != NULL, TRUE);
	g_return_val_if_fail(logwindow != NULL, FALSE);

	if (logwindow->clip && !logwindow->clip_length)
		return FALSE;

	text = GTK_TEXT_VIEW(logwindow->text);
	buffer = gtk_text_view_get_buffer(text);
	gtk_text_buffer_get_iter_at_offset(buffer, &iter, -1);

	switch (logtext->type) {
	case LOG_MSG:
		tag = "message";
		head = "* ";
		break;
	case LOG_WARN:
		tag = "warn";
		head = "** ";
		break;
	case LOG_ERROR:
		tag = "error";
		head = "*** ";
		break;
	default:
		tag = NULL;
		break;
	}
  
	if (tag == NULL) {
		if (strstr(logtext->text, "] POP3>")
		||  strstr(logtext->text, "] IMAP4>")
		||  strstr(logtext->text, "] NNTP>"))
			tag = "output";
		if (strstr(logtext->text, "] POP3<")
		||  strstr(logtext->text, "] IMAP4<")
		||  strstr(logtext->text, "] NNTP<"))
			tag = "input";
	}

	if (head)
		gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, head, -1,
							 tag, NULL);
	gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, logtext->text, -1,
						 tag, NULL);

	gtk_text_buffer_get_start_iter(buffer, &iter);

	if (logwindow->clip)
	       log_window_clip (GTK_WIDGET (text), logwindow->clip_length);

	gtk_text_buffer_get_iter_at_offset(buffer, &iter, -1);
	gtk_text_view_scroll_to_iter(text, &iter, 0, TRUE, 0, 0);
	gtk_text_buffer_place_cursor(buffer, &iter);

	return FALSE;
}

static void hide_cb(GtkWidget *widget, LogWindow *logwin)
{
}

static gboolean key_pressed(GtkWidget *widget, GdkEventKey *event,
			LogWindow *logwin)
{
	if (event && event->keyval == GDK_Escape)
		gtk_widget_hide(logwin->window);
	else if (event && event->keyval == GDK_Delete) 
		log_window_clear(NULL, logwin);

	return FALSE;
}

static void log_window_clip(GtkWidget *textw, guint clip_length)
{
        guint length;
	guint point;
	GtkTextView *textview = GTK_TEXT_VIEW(textw);
	GtkTextBuffer *textbuf = gtk_text_view_get_buffer(textview);
	GtkTextIter start_iter, end_iter;
	
	length = gtk_text_buffer_get_line_count(textbuf);
	/* debug_print("Log window length: %u\n", length); */
	
	if (length > clip_length) {
	        /* find the end of the first line after the cut off
		 * point */
       	        point = length - clip_length;
		gtk_text_buffer_get_iter_at_line(textbuf, &end_iter, point);
		if (!gtk_text_iter_forward_to_line_end(&end_iter))
			return;
		gtk_text_buffer_get_start_iter(textbuf, &start_iter);
		gtk_text_buffer_delete(textbuf, &start_iter, &end_iter);
	}
}

static void log_window_clear(GtkWidget *widget, LogWindow *logwin)
{
	GtkTextView *textview = GTK_TEXT_VIEW(logwin->text);
	GtkTextBuffer *textbuf = gtk_text_view_get_buffer(textview);
	GtkTextIter start_iter, end_iter;
	
	gtk_text_buffer_get_start_iter(textbuf, &start_iter);
	gtk_text_buffer_get_end_iter(textbuf, &end_iter);
	gtk_text_buffer_delete(textbuf, &start_iter, &end_iter);
}

static void log_window_popup_menu_extend(GtkTextView *textview,
   			GtkMenu *menu, LogWindow *logwin)
{
	GtkWidget *menuitem;
	
	g_return_if_fail(menu != NULL);
	g_return_if_fail(GTK_IS_MENU_SHELL(menu));

	menuitem = gtk_separator_menu_item_new();
	gtk_menu_shell_prepend(GTK_MENU_SHELL(menu), menuitem);
	gtk_widget_show(menuitem);
	
	menuitem = gtk_menu_item_new_with_mnemonic(_("Clear _Log"));
	g_signal_connect(G_OBJECT(menuitem), "activate",
			 G_CALLBACK(log_window_clear), logwin);
	gtk_menu_shell_prepend(GTK_MENU_SHELL(menu), menuitem);
	gtk_widget_show(menuitem);
}

