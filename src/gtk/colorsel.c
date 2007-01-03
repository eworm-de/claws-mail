/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2003-2007 Hiroyuki Yamamoto & The Claws Mail Team
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

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "colorsel.h"
#include "manage_window.h"

static void quote_colors_set_dialog_ok(GtkWidget *widget, gpointer data)
{
	*((gint *) data) = 0;
	gtk_main_quit();
}

static void quote_colors_set_dialog_cancel(GtkWidget *widget, gpointer data)
{
	*((gint *) data) = 1;
	gtk_main_quit();
}

static gboolean quote_colors_set_dialog_key_pressed(GtkWidget *widget,
						GdkEventKey *event,
						gpointer data)
{
	if (event && event->keyval == GDK_Escape) {
		*((gint *) data) = 1;
		gtk_main_quit();
		return TRUE;
	} else if (event && event->keyval == GDK_Return) {
		*((gint *) data) = 0;
		gtk_main_quit();
		return FALSE;
	}
	return FALSE;
}

gint colorsel_select_color_rgb(gchar *title, gint rgbvalue)
{
	gdouble color[4] = {0.0, 0.0, 0.0, 0.0};
	GtkColorSelectionDialog *color_dialog;
	gint result;

	color_dialog = GTK_COLOR_SELECTION_DIALOG(gtk_color_selection_dialog_new(title));
	gtk_window_set_modal(GTK_WINDOW(color_dialog), TRUE);
	gtk_window_set_resizable(GTK_WINDOW(color_dialog), FALSE);
	manage_window_set_transient(GTK_WINDOW(color_dialog));

	g_signal_connect(G_OBJECT(GTK_COLOR_SELECTION_DIALOG(color_dialog)->ok_button),
			 "clicked", 
			 G_CALLBACK(quote_colors_set_dialog_ok), &result);
	g_signal_connect(G_OBJECT(GTK_COLOR_SELECTION_DIALOG(color_dialog)->cancel_button),
			 "clicked", 
			 G_CALLBACK(quote_colors_set_dialog_cancel), &result);
	g_signal_connect(G_OBJECT(color_dialog), "key_press_event",
			 G_CALLBACK(quote_colors_set_dialog_key_pressed), &result);

	/* preselect the previous color in the color selection dialog */
	color[0] = (gdouble) ((rgbvalue & 0xff0000) >> 16) / 255.0;
	color[1] = (gdouble) ((rgbvalue & 0x00ff00) >>  8) / 255.0;
	color[2] = (gdouble)  (rgbvalue & 0x0000ff)        / 255.0;
	gtk_color_selection_set_color
		(GTK_COLOR_SELECTION(color_dialog->colorsel), color);

	gtk_widget_show(GTK_WIDGET(color_dialog));
	gtk_main();

	if (result == 0) {
		gint red, green, blue, rgbvalue_new;

		gtk_color_selection_get_color(GTK_COLOR_SELECTION(color_dialog->colorsel), color);

		red          = (gint) (color[0] * 255.0);
		green        = (gint) (color[1] * 255.0);
		blue         = (gint) (color[2] * 255.0);
		rgbvalue_new = (gint) ((red * 0x10000) | (green * 0x100) | blue);
		
		rgbvalue = rgbvalue_new;
	}

	gtk_widget_destroy(GTK_WIDGET(color_dialog));

	return rgbvalue;
}

