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

#include <glib.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>

#include "manage_window.h"
#include "utils.h"

GtkWidget *focus_window;

gint manage_window_focus_in(GtkWidget *widget, GdkEventFocus *event,
			    gpointer data)
{
	debug_print("Focus in event: window: %08x\n", (guint)widget);

	focus_window = widget;

	return TRUE;
}

gint manage_window_focus_out(GtkWidget *widget, GdkEventFocus *event,
			     gpointer data)
{
	debug_print("Focused window: %08x\n", (guint)focus_window);
	debug_print("Focus out event: window: %08x\n", (guint)widget);

	if (focus_window == widget)
		focus_window = NULL;

	return TRUE;
}

void manage_window_set_transient(GtkWindow *window)
{
	debug_print("window = %08x, focus_window = %08x\n",
		    (guint)window, (guint)focus_window);

	if (window && focus_window)
		gtk_window_set_transient_for(window, GTK_WINDOW(focus_window));
}
