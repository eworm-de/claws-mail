/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2001 Hiroyuki Yamamoto
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

#include <stddef.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "intl.h"
#include "mainwindow.h"
#include "alertpanel.h"
#include "manage_window.h"
#include "utils.h"
#include "gtkutils.h"
#include "inc.h"
#include "logwindow.h"
#include "pixmaps/stock_dialog-info.xpm"
#include "pixmaps/stock_dialog-question.xpm"
#include "pixmaps/stock_dialog-error.xpm"
#include "pixmaps/stock_dialog-warning.xpm"

#define TITLE_FONT	"-*-helvetica-medium-r-normal--17-*-*-*-*-*-*-*," \
			"-*-*-medium-r-normal--16-*-*-*-*-*-*-*,*"
#define MESSAGE_FONT	"-*-helvetica-medium-r-normal--14-*-*-*-*-*-*-*," \
			"-*-*-medium-r-normal--14-*-*-*-*-*-*-*,*"
#define ALERT_PANEL_WIDTH	380
#define TITLE_HEIGHT		72
#define MESSAGE_HEIGHT		62

static gboolean alertpanel_is_open = FALSE;
static AlertValue value;

static GtkWidget *dialog;

static void alertpanel_show		(void);
static void alertpanel_create		(const gchar	*title,
					 const gchar	*message,
					 const gchar	*button1_label,
					 const gchar	*button2_label,
					 const gchar	*button3_label,
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
static void alertpanel_close		(GtkWidget		*widget,
					 GdkEventAny		*event,
					 gpointer		 data);

AlertValue alertpanel(const gchar *title,
		      const gchar *message,
		      const gchar *button1_label,
		      const gchar *button2_label,
		      const gchar *button3_label)
{
	return alertpanel_with_type(title, message, button1_label, 
				    button2_label, button3_label, 
				    NULL, ALERT_QUESTION);
}

AlertValue alertpanel_with_widget(const gchar *title,
				  const gchar *message,
				  const gchar *button1_label,
				  const gchar *button2_label,
				  const gchar *button3_label,
				  GtkWidget *widget)
{
	return alertpanel_with_type(title, message, button1_label, 
				    button2_label, button3_label, 
				    widget, ALERT_QUESTION);
}

AlertValue alertpanel_with_type(const gchar *title,
				const gchar *message,
				const gchar *button1_label,
				const gchar *button2_label,
				const gchar *button3_label,
				GtkWidget   *widget,
				gint	     alert_type)
{
 	if (alertpanel_is_open)
 		return -1;
 	else
 		alertpanel_is_open = TRUE;
	
	alertpanel_create(title, message, button1_label, button2_label,
			  button3_label, FALSE, widget, alert_type);
 	alertpanel_show();
 
 	debug_print("return value = %d\n", value);
	return value;
}

static void alertpanel_message(const gchar *title, const gchar *message, gint type)
{
	if (alertpanel_is_open)
		return;
	else
		alertpanel_is_open = TRUE;

	alertpanel_create(title, message, NULL, NULL, NULL, FALSE, NULL, type);
	alertpanel_show();
}

AlertValue alertpanel_message_with_disable(const gchar *title,
					   const gchar *message,
					   const gchar	*button1_label,
					   const gchar	*button2_label,
					   const gchar	*button3_label,
					   gint alert_type)
{
	if (alertpanel_is_open)
		return 0;
	else
		alertpanel_is_open = TRUE;

	alertpanel_create(title, message, button1_label, button2_label, 
			  button3_label, TRUE, NULL, alert_type);
	alertpanel_show();

	return value;
}

void alertpanel_notice(const gchar *format, ...)
{
	va_list args;
	gchar buf[256];

	va_start(args, format);
	g_vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);
	strretchomp(buf);

	alertpanel_message(_("Notice"), buf, ALERT_NOTICE);
}

void alertpanel_warning(const gchar *format, ...)
{
	va_list args;
	gchar buf[256];

	va_start(args, format);
	g_vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);
	strretchomp(buf);

	alertpanel_message(_("Warning"), buf, ALERT_WARNING);
}

void alertpanel_error(const gchar *format, ...)
{
	va_list args;
	gchar buf[256];

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
	gchar buf[256];

	va_start(args, format);
	g_vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);
	strretchomp(buf);

	mainwin = mainwindow_get_mainwindow();
	
	if (mainwin && mainwin->logwin) {
		val = alertpanel_with_type(_("Error"), buf, _("OK"), _("View log"), NULL, NULL, ALERT_ERROR);
		if (val == G_ALERTALTERNATE)
			log_window_show(mainwin->logwin);
	} else
		alertpanel_error(buf);
}

static void alertpanel_show(void)
{
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	manage_window_set_transient(GTK_WINDOW(dialog));
	value = G_ALERTWAIT;

	if (gdk_pointer_is_grabbed())
		gdk_pointer_ungrab(GDK_CURRENT_TIME);
	inc_lock();
	while ((value & G_ALERT_VALUE_MASK) == G_ALERTWAIT)
		gtk_main_iteration();

	gtk_widget_destroy(dialog);
	GTK_EVENTS_FLUSH();

	alertpanel_is_open = FALSE;
	inc_unlock();
}

static void alertpanel_create(const gchar *title,
			      const gchar *message,
			      const gchar *button1_label,
			      const gchar *button2_label,
			      const gchar *button3_label,
			      gboolean	   can_disable,
			      GtkWidget   *custom_widget,
			      gint	   alert_type)
{
	static GdkFont *titlefont;
	GtkStyle *style;
	GtkWidget *label;
	GtkWidget *w_hbox;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *disable_chkbtn;
	GtkWidget *confirm_area;
	GtkWidget *button1;
	GtkWidget *button2;
	GtkWidget *button3;
	GtkWidget *icon;
	GdkPixmap *iconpix;
	GdkBitmap *mask;
	const gchar *label2;
	const gchar *label3;
	void *icon_desc[] = {	stock_dialog_info_xpm,
				stock_dialog_question_xpm,
				stock_dialog_warning_xpm,
				stock_dialog_error_xpm };

	debug_print("Creating alert panel dialog...\n");

	dialog = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(dialog), title);
	gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, FALSE, FALSE);
/*	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE); */
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);

	gtk_container_set_border_width
		(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area), 5);
	gtk_window_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	gtk_signal_connect(GTK_OBJECT(dialog), "delete_event",
			   GTK_SIGNAL_FUNC(alertpanel_deleted),
			   (gpointer)G_ALERTOTHER);
	gtk_signal_connect(GTK_OBJECT(dialog), "key_press_event",
			   GTK_SIGNAL_FUNC(alertpanel_close),
			   (gpointer)G_ALERTOTHER);
	gtk_widget_realize(dialog);

	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 14);
	hbox = gtk_hbox_new (FALSE, 12);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox,
			    FALSE, FALSE, 0);

	/* for title label */
	w_hbox = gtk_hbox_new(FALSE, 0);
	
	if (alert_type < 0 || alert_type > 3)
		alert_type = 0;
	iconpix = gdk_pixmap_create_from_xpm_d(dialog->window, &mask,
			NULL, icon_desc[alert_type]);
	icon = gtk_pixmap_new(iconpix, mask);
	gtk_misc_set_alignment (GTK_MISC (icon), 0.5, 0.0);
	gtk_box_pack_start (GTK_BOX (hbox), icon, FALSE, FALSE, 0);
	
	vbox = gtk_vbox_new (FALSE, 12);
	gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);
	gtk_widget_show (vbox);
	
	label = gtk_label_new(title);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);
	
	style = gtk_style_copy(gtk_widget_get_style(label));
	if (!titlefont)
		titlefont = gtkut_font_load(TITLE_FONT);
	if (titlefont)
		style->font = titlefont;
	gtk_widget_set_style(label, style);
	
	label = gtk_label_new(message);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);
		
	/* Claws: custom widget */
	if (custom_widget) {
		gtk_box_pack_start(GTK_BOX(vbox), custom_widget, FALSE,
				   FALSE, 0);
	}
	
	if (can_disable) {
		disable_chkbtn = gtk_check_button_new_with_label
			(_("Show this message next time"));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(disable_chkbtn),
					     TRUE);
		gtk_box_pack_start(GTK_BOX(vbox), disable_chkbtn,
				   FALSE, FALSE, 0);
		gtk_signal_connect(GTK_OBJECT(disable_chkbtn), "toggled",
				 GTK_SIGNAL_FUNC(alertpanel_button_toggled),
				 GUINT_TO_POINTER(G_ALERTDISABLE));
	} 

	/* for button(s) */
	if (!button1_label)
		button1_label = _("OK");
	label2 = button2_label;
	label3 = button3_label;
	if (label2 && *label2 == '+') label2++;
	if (label3 && *label3 == '+') label3++;

	gtkut_button_set_create(&confirm_area,
				&button1, button1_label,
				button2_label ? &button2 : NULL, label2,
				button3_label ? &button3 : NULL, label3);

	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->action_area), confirm_area, FALSE, FALSE, 12);
	gtk_widget_grab_default(button1);
	gtk_widget_grab_focus(button1);
	if (button2_label && *button2_label == '+') {
		gtk_widget_grab_default(button2);
		gtk_widget_grab_focus(button2);
	}
	if (button3_label && *button3_label == '+') {
		gtk_widget_grab_default(button3);
		gtk_widget_grab_focus(button3);
	}

	gtk_signal_connect(GTK_OBJECT(button1), "clicked",
			 GTK_SIGNAL_FUNC(alertpanel_button_clicked),
			 GUINT_TO_POINTER(G_ALERTDEFAULT));
	if (button2_label)
		gtk_signal_connect(GTK_OBJECT(button2), "clicked",
				 GTK_SIGNAL_FUNC(alertpanel_button_clicked),
				 GUINT_TO_POINTER(G_ALERTALTERNATE));
	if (button3_label)
		gtk_signal_connect(GTK_OBJECT(button3), "clicked",
				 GTK_SIGNAL_FUNC(alertpanel_button_clicked),
				 GUINT_TO_POINTER(G_ALERTOTHER));

	gtk_widget_show_all(dialog);
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

static void alertpanel_close(GtkWidget *widget, GdkEventAny *event,
			     gpointer data)
{
	if (event->type == GDK_KEY_PRESS)
		if (((GdkEventKey *)event)->keyval != GDK_Escape)
			return;

	value = (value & ~G_ALERT_VALUE_MASK) | (AlertValue)data;
}
