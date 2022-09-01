/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2022 the Claws Mail team and Hiroyuki Yamamoto
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "defs.h"
#include "manage_window.h"
#include "description_window.h"
#include "gtkutils.h"
#include "prefs_gtk.h"

static void description_create				(DescriptionWindow *dwindow);
static gboolean description_window_key_pressed		(GtkWidget *widget,
						 	 GdkEventKey *event,
							 gpointer data);
static gboolean description_window_focus_in_event	(GtkWidget *widget,
							 GdkEventFocus *event,
							 gpointer data);
static gboolean description_window_focus_out_event	(GtkWidget *widget,
							 GdkEventFocus *event,
							 gpointer data);
static void description_window_destroy	 		(GtkWidget *parent,
							 gpointer data);

void description_window_create(DescriptionWindow *dwindow)
{
	if (!dwindow->window) {
		description_create(dwindow);
	
		gtk_window_set_transient_for(GTK_WINDOW(dwindow->window), GTK_WINDOW(dwindow->parent));
		dwindow->parent_modal = gtk_window_get_modal(GTK_WINDOW(dwindow->parent));
#ifndef G_OS_WIN32
		gtk_window_set_modal(GTK_WINDOW(dwindow->parent), TRUE);
#else
		gtk_window_set_modal(GTK_WINDOW(dwindow->window), TRUE);
#endif
		gtk_window_set_destroy_with_parent(GTK_WINDOW(dwindow->window), TRUE);
		gtk_widget_show(dwindow->window);

	} else g_print("window exists\n");
}

static void description_create(DescriptionWindow * dwindow)
{
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *vbox;
	GtkWidget *table;
	GtkWidget *hbbox;
	GtkWidget *close_btn;
	GtkWidget *scrolledwin;
	int i;
	int sz;
	int line;
	int j;
	int *max_width = g_new0(int, dwindow->columns), width=0;
	GtkRequisition req;
	
	dwindow->window = gtkut_window_new(GTK_WINDOW_TOPLEVEL, "description_window");
	
	gtk_window_set_title(GTK_WINDOW(dwindow->window),
			     gettext(dwindow->title));
	gtk_container_set_border_width(GTK_CONTAINER(dwindow->window), 8);
	gtk_window_set_resizable(GTK_WINDOW(dwindow->window), TRUE);
	gtk_window_set_type_hint(GTK_WINDOW(dwindow->window), GDK_WINDOW_TYPE_HINT_DIALOG);

	/* Check number of lines to be show */
	sz = 0;
	for (i = 0; dwindow->symbol_table[i] != NULL; i = i + dwindow->columns) {
		sz++;
	}
	
	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	
	table = gtk_grid_new();
	gtk_container_add(GTK_CONTAINER(scrolledwin), table);
	gtk_container_set_border_width(GTK_CONTAINER(table), 4);

	line = 0;
	for(i = 0; dwindow->symbol_table[i] != NULL; i = i + dwindow->columns) {
		if(dwindow->symbol_table[i][0] != '\0') {
			GtkWidget *label;

			for (j = 0; j < dwindow->columns; j++) {
				gint col = j;
				gint colend = j+1;
				/* Expand using next NULL columns */
				while ((colend < dwindow->columns) && 
				       (dwindow->symbol_table[i+colend] == NULL)) {
				       colend++;
				       j++;
				}
				label = gtk_label_new(gettext(dwindow->symbol_table[i+col]));
				gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
				gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
				gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
				gtk_label_set_xalign(GTK_LABEL(label), 0.0);
				gtk_label_set_yalign(GTK_LABEL(label), 0.0);
				gtk_widget_set_margin_end(GTK_WIDGET(label), 12);
				gtk_grid_attach(GTK_GRID(table), label, col, line, 1, 1);

				gtk_widget_get_preferred_size(label, &req, NULL);
				if(req.width > max_width[j])
					max_width[j] = req.width;
			}
		} else {
			GtkWidget *separator;
			
			separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
			gtk_grid_attach(GTK_GRID(table), separator, 0, line, 1, 1);
		}
		line++;
	}

	for(j=0; j<dwindow->columns; j++)
		width += max_width[j];

	g_free(max_width);
	width += 100;
	
	gtkut_stock_button_set_create(&hbbox, &close_btn, "window-close", _("_Close"),
				      NULL, NULL, NULL, NULL, NULL, NULL);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, VSPACING_NARROW);
	gtk_container_add(GTK_CONTAINER(dwindow->window), vbox);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new(gettext(dwindow->description));
	gtk_widget_set_size_request(GTK_WIDGET(label), width-2, -1);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(scrolledwin),
			   TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(hbbox),
			   FALSE, FALSE, 3);
			   
	gtk_widget_grab_default(close_btn);

	g_signal_connect(G_OBJECT(close_btn), "clicked",
			 G_CALLBACK(description_window_destroy), dwindow);
	g_signal_connect(G_OBJECT(dwindow->window), "key_press_event",
		 	 G_CALLBACK(description_window_key_pressed), dwindow);
	g_signal_connect(G_OBJECT(dwindow->window), "focus_in_event",
			 G_CALLBACK(description_window_focus_in_event), NULL);
	g_signal_connect(G_OBJECT(dwindow->window), "focus_out_event",
			 G_CALLBACK(description_window_focus_out_event), NULL);
	g_signal_connect(G_OBJECT(dwindow->window), "destroy",
			 G_CALLBACK(description_window_destroy), dwindow);
	
	if(dwindow->parent)
		g_signal_connect(G_OBJECT(dwindow->parent), "hide",
			G_CALLBACK(description_window_destroy), dwindow);

	gtk_widget_show_all(vbox);
	gtk_widget_set_size_request(dwindow->window,
                               (width < 400) ? 400 : width, 450);	
}

static gboolean description_window_key_pressed(GtkWidget *widget,
				 	       GdkEventKey *event,
					       gpointer data)
{
	if (event && event->keyval == GDK_KEY_Escape)
		description_window_destroy(widget, data);
	return FALSE;
}

static gboolean description_window_focus_in_event (GtkWidget *widget,
						   GdkEventFocus *event,
						   gpointer data)
{
	if (gtk_grab_get_current() != widget)
		gtk_grab_add(GTK_WIDGET(widget));

	return FALSE;
}

static gboolean description_window_focus_out_event (GtkWidget *widget,
						   GdkEventFocus *event,
						   gpointer data)
{
	gtk_grab_remove(GTK_WIDGET(widget));
		
	return FALSE;
}

static void description_window_destroy (GtkWidget *widget, gpointer data)
{
	DescriptionWindow *dwindow = (DescriptionWindow *) data;

	if(dwindow->window) {
		gtk_widget_hide(dwindow->window);
		gtk_widget_destroy(dwindow->window);
		dwindow->window = NULL;
	}
	
	if(dwindow->parent) {
		if (GTK_IS_WINDOW(dwindow->parent))
			gtk_window_set_modal(GTK_WINDOW(dwindow->parent), dwindow->parent_modal);
		g_signal_handlers_disconnect_by_func(G_OBJECT(dwindow->parent), 
					description_window_destroy, dwindow->parent);
	}
}
