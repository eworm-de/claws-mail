/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2002 Hiroyuki Yamamoto
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

#include "defs.h"

#include <glib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtktable.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkhbbox.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkctree.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "intl.h"
#include "main.h"
#include "message_search.h"
#include "messageview.h"
#include "utils.h"
#include "gtkutils.h"
#include "manage_window.h"
#include "alertpanel.h"

static GtkWidget *window;
static GtkWidget *body_entry;
static GtkWidget *case_checkbtn;
static GtkWidget *backward_checkbtn;
static GtkWidget *search_btn;
static GtkWidget *clear_btn;
static GtkWidget *close_btn;

static void message_search_create(MessageView *summaryview);
static void message_search_execute(GtkButton *button, gpointer data);
static void message_search_clear(GtkButton *button, gpointer data);
static void body_activated(void);
static void key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data);

void message_search(MessageView *messageview)
{
	if (!window)
		message_search_create(messageview);
	else
		gtk_widget_hide(window);

	gtk_widget_grab_focus(search_btn);
	gtk_widget_grab_focus(body_entry);
	gtk_widget_show(window);
}

static void message_search_create(MessageView *messageview)
{
	GtkWidget *vbox1;
	GtkWidget *hbox1;
	GtkWidget *body_label;
	GtkWidget *checkbtn_hbox;
	GtkWidget *confirm_area;

	window = gtk_window_new (GTK_WINDOW_DIALOG);
	gtk_window_set_title (GTK_WINDOW (window),
			      _("Find in current message"));
	gtk_widget_set_usize (window, 450, -1);
	gtk_window_set_policy(GTK_WINDOW(window), FALSE, TRUE, TRUE);
	gtk_container_set_border_width (GTK_CONTAINER (window), 8);
	gtk_signal_connect(GTK_OBJECT(window), "delete_event",
			   GTK_SIGNAL_FUNC(gtk_widget_hide_on_delete), NULL);
	gtk_signal_connect(GTK_OBJECT(window), "key_press_event",
			   GTK_SIGNAL_FUNC(key_pressed), NULL);
	MANAGE_WINDOW_SIGNALS_CONNECT(window);

	vbox1 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (window), vbox1);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox1, TRUE, TRUE, 0);

	body_label = gtk_label_new (_("Find text:"));
	gtk_widget_show (body_label);
	gtk_box_pack_start (GTK_BOX (hbox1), body_label, FALSE, FALSE, 0);

	body_entry = gtk_entry_new ();
	gtk_widget_show (body_entry);
	gtk_box_pack_start (GTK_BOX (hbox1), body_entry, TRUE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(body_entry), "activate",
			   GTK_SIGNAL_FUNC(body_activated), messageview);

	checkbtn_hbox = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (checkbtn_hbox);
	gtk_box_pack_start (GTK_BOX (vbox1), checkbtn_hbox, TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (checkbtn_hbox), 8);

	case_checkbtn = gtk_check_button_new_with_label (_("Case sensitive"));
	gtk_widget_show (case_checkbtn);
	gtk_box_pack_start (GTK_BOX (checkbtn_hbox), case_checkbtn,
			    FALSE, FALSE, 0);

	backward_checkbtn =
		gtk_check_button_new_with_label (_("Backward search"));
	gtk_widget_show (backward_checkbtn);
	gtk_box_pack_start (GTK_BOX (checkbtn_hbox), backward_checkbtn,
			    FALSE, FALSE, 0);

	gtkut_button_set_create(&confirm_area,
				&search_btn, _("Search"),
				&clear_btn,  _("Clear"),
				&close_btn,  _("Close"));
	gtk_widget_show (confirm_area);
	gtk_box_pack_start (GTK_BOX (vbox1), confirm_area, FALSE, FALSE, 0);
	gtk_widget_grab_default(search_btn);

	gtk_signal_connect(GTK_OBJECT(search_btn), "clicked",
			   GTK_SIGNAL_FUNC(message_search_execute),
			   messageview);
	gtk_signal_connect(GTK_OBJECT(clear_btn), "clicked",
			   GTK_SIGNAL_FUNC(message_search_clear),
			   messageview);
	gtk_signal_connect_object(GTK_OBJECT(close_btn), "clicked",
				  GTK_SIGNAL_FUNC(gtk_widget_hide),
				  GTK_OBJECT(window));
}

static void message_search_execute(GtkButton *button, gpointer data)
{
	MessageView *messageview = data;
	gboolean case_sens;
	gboolean backward;
	gboolean all_searched = FALSE;
	gchar *body_str;

	body_str = gtk_entry_get_text(GTK_ENTRY(body_entry));
	if (*body_str == '\0') return;

	case_sens = gtk_toggle_button_get_active
		(GTK_TOGGLE_BUTTON(case_checkbtn));
	backward = gtk_toggle_button_get_active
		(GTK_TOGGLE_BUTTON(backward_checkbtn));

	for (;;) {
		gchar *str;
		AlertValue val;

		if (backward) {
			if (messageview_search_string_backward
				(messageview, body_str, case_sens) == TRUE)
				break;
		} else {
			if (messageview_search_string
				(messageview, body_str, case_sens) == TRUE)
				break;
		}

		if (all_searched) {
			alertpanel_message
				(_("Search failed"),
				 _("Search string not found."));
			break;
		}

		all_searched = TRUE;

		if (backward)
			str = _("Beginning of message reached; "
				"continue from end?");
		else
			str = _("End of message reached; "
				"continue from beginning?");

		val = alertpanel(_("Search finished"), str,
				 _("Yes"), _("No"), NULL);
		if (G_ALERTDEFAULT == val) {
			manage_window_focus_in(window, NULL, NULL);
			messageview_set_position(messageview,
						 backward ? -1 : 0);
		} else
			break;
	}
}

static void message_search_clear(GtkButton *button, gpointer data)
{
	gtk_editable_delete_text(GTK_EDITABLE(body_entry),    0, -1);
}

static void body_activated(void)
{
	gtk_button_clicked(GTK_BUTTON(search_btn));
}

static void key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		gtk_widget_hide(window);
}
