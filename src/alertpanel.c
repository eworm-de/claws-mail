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

#include <stddef.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "hooks.h"
#include "mainwindow.h"
#include "alertpanel.h"
#include "manage_window.h"
#include "utils.h"
#include "gtkutils.h"
#include "inc.h"
#include "logwindow.h"
#include "prefs_common.h"

#define ALERT_PANEL_BUFSIZE	1024

static AlertValue value;
static gboolean alertpanel_is_open = FALSE;
static GtkWidget *window;

static void alertpanel_show		(void);
static void alertpanel_create		(const gchar	*title,
					 const gchar	*message,
					 const gchar	*stock_icon1,
					 const gchar	*button1_label,
					 const gchar	*stock_icon2,
					 const gchar	*button2_label,
					 const gchar	*stock_icon3,
					 const gchar	*button3_label,
					 AlertFocus    focus,
					 gboolean	 can_disable,
					 GtkWidget	*custom_widget,
					 gint		 alert_type);

static void alertpanel_button_toggled	(GtkToggleButton	*button,
					 gpointer		 data);
static void alertpanel_button_clicked	(GtkWidget		*widget,
					 gpointer		 data);
static gint alertpanel_deleted		(GtkWidget		*widget,
					 GdkEventAny		*event,
					 gpointer		 data);
static gboolean alertpanel_close	(GtkWidget		*widget,
					 GdkEventAny		*event,
					 gpointer		 data);

AlertValue alertpanel_with_widget(const gchar *title,
				  const gchar *message,
				  const gchar *stock_icon1,
				  const gchar *button1_label,
				  const gchar *stock_icon2,
				  const gchar *button2_label,
				  const gchar *stock_icon3,
				  const gchar *button3_label,
					AlertFocus   focus,
				  gboolean     can_disable,
				  GtkWidget   *widget)
{
	return alertpanel_full(title, message, NULL, button1_label,
			       NULL, button2_label, NULL, button3_label,
			       focus, can_disable, widget, ALERT_QUESTION);
}

AlertValue alertpanel_full(const gchar *title, const gchar *message,
			   const gchar *stock_icon1,
			   const gchar *button1_label,
			   const gchar *stock_icon2,
			   const gchar *button2_label,
			   const gchar *stock_icon3,
			   const gchar *button3_label,
			   AlertFocus   focus,
			   gboolean     can_disable,
			   GtkWidget   *widget,
			   AlertType    alert_type)
{
	if (alertpanel_is_open)
		return -1;
	else {
		alertpanel_is_open = TRUE;
		hooks_invoke(ALERTPANEL_OPENED_HOOKLIST, &alertpanel_is_open);
	}
	alertpanel_create(title, message, stock_icon1, button1_label, stock_icon2,
			  button2_label, stock_icon3, button3_label, focus,
			  can_disable, widget, alert_type);
	alertpanel_show();

	debug_print("return value = %d\n", value);
	return value;
}

AlertValue alertpanel(const gchar *title,
		      const gchar *message,
		      const gchar *stock_icon1,
		      const gchar *button1_label,
		      const gchar *stock_icon2,
		      const gchar *button2_label,
		      const gchar *stock_icon3,
		      const gchar *button3_label,
		      AlertFocus   focus)
{
	return alertpanel_full(title, message, stock_icon1, button1_label, stock_icon2, button2_label,
			       stock_icon3, button3_label, focus, FALSE, NULL, ALERT_QUESTION);
}

static void alertpanel_message(const gchar *title, const gchar *message, gint type)
{
	if (alertpanel_is_open)
		return;
	else {
		alertpanel_is_open = TRUE;
		hooks_invoke(ALERTPANEL_OPENED_HOOKLIST, &alertpanel_is_open);
	}

	alertpanel_create(title, message, NULL, _("_Close"), NULL, NULL, NULL, NULL,
			  ALERTFOCUS_FIRST, FALSE, NULL, type);
	alertpanel_show();
}

void alertpanel_notice(const gchar *format, ...)
{
	va_list args;
	gchar buf[ALERT_PANEL_BUFSIZE];

	va_start(args, format);
	g_vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);
	strretchomp(buf);

	alertpanel_message(_("Notice"), buf, ALERT_NOTICE);
}

void alertpanel_warning(const gchar *format, ...)
{
	va_list args;
	gchar buf[ALERT_PANEL_BUFSIZE];

	va_start(args, format);
	g_vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);
	strretchomp(buf);

	alertpanel_message(_("Warning"), buf, ALERT_WARNING);
}

void alertpanel_error(const gchar *format, ...)
{
	va_list args;
	gchar buf[ALERT_PANEL_BUFSIZE];

	va_start(args, format);
	g_vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);
	strretchomp(buf);

	alertpanel_message(_("Error"), buf, ALERT_ERROR);
}

/*!
 *\brief	display an error with a View Log button
 *
 */
void alertpanel_error_log(const gchar *format, ...)
{
	va_list args;
	int val;
	MainWindow *mainwin;
	gchar buf[ALERT_PANEL_BUFSIZE];

	va_start(args, format);
	g_vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);
	strretchomp(buf);

	mainwin = mainwindow_get_mainwindow();
	
	if (mainwin && mainwin->logwin) {
		mainwindow_clear_error(mainwin);
		val = alertpanel_full(_("Error"), buf, NULL, _("_Close"), NULL,
				      _("_View log"), NULL, NULL, ALERTFOCUS_FIRST,
				      FALSE, NULL, ALERT_ERROR);
		if (val == G_ALERTALTERNATE)
			log_window_show(mainwin->logwin);
	} else
		alertpanel_error("%s", buf);
}

static void alertpanel_show(void)
{
	GdkDisplay *display;
	GdkSeat    *seat;
	GdkDevice  *device;
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	manage_window_set_transient(GTK_WINDOW(window));
	gtk_widget_show_all(window);
	value = G_ALERTWAIT;
	
	display = gdk_display_get_default();
	seat = gdk_display_get_default_seat(display);
	device = gdk_seat_get_pointer(seat);

	if (gdk_display_device_is_grabbed(display, device))
		gdk_seat_ungrab(seat);
	inc_lock();
	while ((value & G_ALERT_VALUE_MASK) == G_ALERTWAIT)
		gtk_main_iteration();

	gtk_widget_destroy(window);
	GTK_EVENTS_FLUSH();

	alertpanel_is_open = FALSE;
	hooks_invoke(ALERTPANEL_OPENED_HOOKLIST, &alertpanel_is_open);

	inc_unlock();
}

static void alertpanel_create(const gchar *title,
			      const gchar *message,
			      const gchar *stock_icon1,
			      const gchar *button1_label,
			      const gchar *stock_icon2,
			      const gchar *button2_label,
			      const gchar *stock_icon3,
			      const gchar *button3_label,
			      AlertFocus   focus,
			      gboolean	   can_disable,
			      GtkWidget   *custom_widget,
			      gint	   alert_type)
{
	static PangoFontDescription *font_desc;
	GtkWidget *image;
	GtkWidget *label;
	GtkWidget *hbox;
	GtkWidget *content_area;
	GtkWidget *vbox;
	GtkWidget *disable_checkbtn;
	GtkWidget *confirm_area;
	GtkWidget *button1;
	GtkWidget *button2;
	GtkWidget *button3;
	GtkWidget *focusbutton;
	const gchar *label2;
	const gchar *label3;
	gchar *tmp = title?g_markup_printf_escaped("%s", title)
			:g_strdup("");
	gchar *title_full = g_strdup_printf("<span weight=\"bold\" "
				"size=\"larger\">%s</span>",
				tmp);
	g_free(tmp);
	debug_print("Creating alert panel window...\n");

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	manage_window_set_transient(GTK_WINDOW(window));
	gtk_window_set_title(GTK_WINDOW(window), title);
	gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	gtk_window_set_default_size (GTK_WINDOW(window), 450, 200);
	
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(alertpanel_deleted),
			 (gpointer)G_ALERTCANCEL);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(alertpanel_close),
			 (gpointer)G_ALERTCANCEL);

	content_area = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_container_add (GTK_CONTAINER(window), content_area);
	/* for title icon, label and message */
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 12);

	gtk_container_add (GTK_CONTAINER(content_area), hbox);
	
	/* title icon */
	switch (alert_type) {
	case ALERT_QUESTION:
		image = gtk_image_new_from_icon_name
			("dialog-question", GTK_ICON_SIZE_DIALOG);
		break;
	case ALERT_WARNING:
		image = gtk_image_new_from_icon_name
			("dialog-warning", GTK_ICON_SIZE_DIALOG);
		break;
	case ALERT_ERROR:
		image = gtk_image_new_from_icon_name
			("dialog-error", GTK_ICON_SIZE_DIALOG);
		break;
	case ALERT_NOTICE:
	default:
		image = gtk_image_new_from_icon_name
			("dialog-information", GTK_ICON_SIZE_DIALOG);
		break;
	}
	gtk_widget_set_halign(image, GTK_ALIGN_CENTER);
	gtk_widget_set_valign(image, GTK_ALIGN_START);
	gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
	
	gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);
	gtk_widget_show (vbox);
	
	label = gtk_label_new(title_full);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
	gtk_label_set_use_markup(GTK_LABEL (label), TRUE);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	if (!font_desc) {
		gint size;

		size = pango_font_description_get_size
			(gtk_widget_get_style(label)->font_desc);
		font_desc = pango_font_description_new();
		pango_font_description_set_weight
			(font_desc, PANGO_WEIGHT_BOLD);
		pango_font_description_set_size
			(font_desc, size * PANGO_SCALE_LARGE);
	}
	if (font_desc)
		gtk_widget_override_font(label, font_desc);
	g_free(title_full);

	label = gtk_label_new(message);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_widget_set_can_focus(label, FALSE);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_label_set_line_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);
		
	/* Claws: custom widget */
	if (custom_widget) {
		gtk_box_pack_start(GTK_BOX(vbox), custom_widget, FALSE,
				   FALSE, 0);
	}

	if (can_disable) {
		hbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
		gtk_box_pack_start(GTK_BOX(vbox), hbox,
				   FALSE, FALSE, 0);

		disable_checkbtn = gtk_check_button_new_with_label
			(_("Show this message next time"));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(disable_checkbtn),
					     TRUE);
		gtk_box_pack_start(GTK_BOX(hbox), disable_checkbtn,
				   FALSE, FALSE, 12);

		g_signal_connect(G_OBJECT(disable_checkbtn), "toggled",
				 G_CALLBACK(alertpanel_button_toggled),
				 GUINT_TO_POINTER(G_ALERTDISABLE));
	}

	/* for button(s) */
	if (!button1_label)
		button1_label = _("_OK");
	label2 = button2_label;
	label3 = button3_label;

	gtkut_stock_button_set_create(&confirm_area,
				      &button1, stock_icon1, button1_label,
				      button2_label ? &button2 : NULL, stock_icon2, label2,
				      button3_label ? &button3 : NULL, stock_icon3, label3);

	gtk_box_pack_end(GTK_BOX(vbox), confirm_area, FALSE, FALSE, 0);

	/* Set focus on correct button as requested. */
	focusbutton = button1;
	switch (focus) {
		case ALERTFOCUS_SECOND:
			if (button2_label != NULL)
				focusbutton = button2;
			break;
		case ALERTFOCUS_THIRD:
			if (button3_label != NULL)
				focusbutton = button3;
			break;
		case ALERTFOCUS_FIRST:
		default:
			focusbutton = button1;
			break;
	}
	gtk_widget_grab_default(focusbutton);
	gtk_widget_grab_focus(focusbutton);

	g_signal_connect(G_OBJECT(button1), "clicked",
			 G_CALLBACK(alertpanel_button_clicked),
			 GUINT_TO_POINTER(G_ALERTDEFAULT));
	if (button2_label)
		g_signal_connect(G_OBJECT(button2), "clicked",
				 G_CALLBACK(alertpanel_button_clicked),
				 GUINT_TO_POINTER(G_ALERTALTERNATE));
	if (button3_label)
		g_signal_connect(G_OBJECT(button3), "clicked",
				 G_CALLBACK(alertpanel_button_clicked),
				 GUINT_TO_POINTER(G_ALERTOTHER));

	gtk_widget_show_all(window);
}

static void alertpanel_button_toggled(GtkToggleButton *button,
				      gpointer data)
{
	if (gtk_toggle_button_get_active(button))
		value &= ~GPOINTER_TO_UINT(data);
	else
		value |= GPOINTER_TO_UINT(data);
}

static void alertpanel_button_clicked(GtkWidget *widget, gpointer data)
{
	value = (value & ~G_ALERT_VALUE_MASK) | (AlertValue)data;
}

static gint alertpanel_deleted(GtkWidget *widget, GdkEventAny *event,
			       gpointer data)
{
	value = (value & ~G_ALERT_VALUE_MASK) | (AlertValue)data;
	return TRUE;
}

static gboolean alertpanel_close(GtkWidget *widget, GdkEventAny *event,
				 gpointer data)
{
	if (event->type == GDK_KEY_PRESS)
		if (((GdkEventKey *)event)->keyval != GDK_KEY_Escape)
			return FALSE;

	value = (value & ~G_ALERT_VALUE_MASK) | (AlertValue)data;
	return FALSE;
}
