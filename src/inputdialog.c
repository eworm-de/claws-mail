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
#include <gtk/gtkmain.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkhbbox.h>

#include "intl.h"
#include "inputdialog.h"
#include "manage_window.h"
#include "gtkutils.h"
#include "utils.h"

#define INPUT_DIALOG_WIDTH	420

static gboolean ack;

static GtkWidget *dialog;
static GtkWidget *msg_label;
static GtkWidget *entry;
static GtkWidget *ok_button;

static void input_dialog_create	(void);
static void input_dialog_set	(const gchar	*title,
				 const gchar	*message,
				 const gchar	*default_string);

static void ok_clicked		(GtkWidget	*widget,
				 gpointer	 data);
static void cancel_clicked	(GtkWidget	*widget,
				 gpointer	 data);
static gint delete_event	(GtkWidget	*widget,
				 GdkEventAny	*event,
				 gpointer	 data);
static void key_pressed		(GtkWidget	*widget,
				 GdkEventKey	*event,
				 gpointer	 data);
static void entry_activated	(GtkEditable	*editable);

gchar *input_dialog(const gchar *title, const gchar *message,
		    const gchar *default_string)
{
	gchar *str;

	if (dialog && GTK_WIDGET_VISIBLE(dialog)) return NULL;

	if (!dialog)
		input_dialog_create();

	input_dialog_set(title, message, default_string);
	gtk_widget_show(dialog);
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	manage_window_set_transient(GTK_WINDOW(dialog));

	gtk_main();

	manage_window_focus_out(dialog, NULL, NULL);
	gtk_widget_hide(dialog);
	gtk_entry_set_visibility(GTK_ENTRY(entry), TRUE);

	if (ack) {
		str = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
		if (str && *str == '\0') {
			g_free(str);
			str = NULL;
		}
	} else
		str = NULL;

	GTK_EVENTS_FLUSH();

	debug_print("return string = %s\n", str ? str : "(none)");
	return str;
}

gchar *input_dialog_with_invisible(const gchar *title, const gchar *message,
				   const gchar *default_string)
{
	if (dialog && GTK_WIDGET_VISIBLE(dialog)) return NULL;

	if (!dialog)
		input_dialog_create();

	gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);

	return input_dialog(title, message, default_string);
}

static void input_dialog_create(void)
{
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *confirm_area;
	GtkWidget *cancel_button;

	dialog = gtk_dialog_new();
	gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, FALSE, FALSE);
	gtk_widget_set_usize(dialog, INPUT_DIALOG_WIDTH, -1);
	gtk_container_set_border_width
		(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area), 5);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	gtk_signal_connect(GTK_OBJECT(dialog), "delete_event",
			   GTK_SIGNAL_FUNC(delete_event), NULL);
	gtk_signal_connect(GTK_OBJECT(dialog), "key_press_event",
			   GTK_SIGNAL_FUNC(key_pressed), NULL);
	gtk_signal_connect(GTK_OBJECT(dialog), "focus_in_event",
			   GTK_SIGNAL_FUNC(manage_window_focus_in), NULL);
	gtk_signal_connect(GTK_OBJECT(dialog), "focus_out_event",
			   GTK_SIGNAL_FUNC(manage_window_focus_out), NULL);

	gtk_widget_realize(dialog);

	vbox = gtk_vbox_new(FALSE, 8);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), vbox);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	msg_label = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(hbox), msg_label, FALSE, FALSE, 0);
	gtk_label_set_justify(GTK_LABEL(msg_label), GTK_JUSTIFY_LEFT);

	entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(vbox), entry, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(entry), "activate",
			   GTK_SIGNAL_FUNC(entry_activated), NULL);

	gtkut_button_set_create(&confirm_area,
				&ok_button,     _("OK"),
				&cancel_button, _("Cancel"),
				NULL, NULL);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area),
			  confirm_area);
	gtk_widget_grab_default(ok_button);

	gtk_signal_connect(GTK_OBJECT(ok_button), "clicked",
			   GTK_SIGNAL_FUNC(ok_clicked), NULL);
	gtk_signal_connect(GTK_OBJECT(cancel_button), "clicked",
			   GTK_SIGNAL_FUNC(cancel_clicked), NULL);


	gtk_widget_show_all(GTK_DIALOG(dialog)->vbox);
}

static void input_dialog_set(const gchar *title, const gchar *message,
			     const gchar *default_string)
{
	gtk_window_set_title(GTK_WINDOW(dialog), title);
	gtk_label_set_text(GTK_LABEL(msg_label), message);
	if (default_string && *default_string)
		gtk_entry_set_text(GTK_ENTRY(entry), default_string);
	else
		gtk_entry_set_text(GTK_ENTRY(entry), "");
	gtk_entry_set_position(GTK_ENTRY(entry), 0);
	gtk_entry_select_region(GTK_ENTRY(entry), 0, -1);

	gtk_widget_grab_focus(ok_button);
	gtk_widget_grab_focus(entry);
}

static void ok_clicked(GtkWidget *widget, gpointer data)
{
	ack = TRUE;
	gtk_main_quit();
}

static void cancel_clicked(GtkWidget *widget, gpointer data)
{
	ack = FALSE;
	gtk_main_quit();
}

static gint delete_event(GtkWidget *widget, GdkEventAny *event, gpointer data)
{
	ack = FALSE;
	gtk_main_quit();

	return TRUE;
}

static void key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event && event->keyval == GDK_Escape) {
		ack = FALSE;
		gtk_main_quit();
	}
}

static void entry_activated(GtkEditable *editable)
{
	ack = TRUE;
	gtk_main_quit();
}
