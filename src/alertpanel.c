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

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "intl.h"
#include "alertpanel.h"
#include "manage_window.h"
#include "utils.h"
#include "gtkutils.h"

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
					 gboolean	 can_disable);
static void alertpanel_button_toggled	(GtkToggleButton *button,
					 gpointer	 data);
static void alertpanel_button_clicked	(GtkWidget	*widget,
					 gpointer	 data);
static void alertpanel_close		(GtkWidget	*widget,
					 GdkEventAny	*event,
					 gpointer	 data);

AlertValue alertpanel(const gchar *title,
		      const gchar *message,
		      const gchar *button1_label,
		      const gchar *button2_label,
		      const gchar *button3_label)
{
	if (alertpanel_is_open)
		return -1;
	else
		alertpanel_is_open = TRUE;

	alertpanel_create(title, message, button1_label, button2_label,
			  button3_label, FALSE);
	alertpanel_show();

	debug_print("return value = %d\n", value);
	return value;
}

void alertpanel_message(const gchar *title, const gchar *message)
{
	if (alertpanel_is_open)
		return;
	else
		alertpanel_is_open = TRUE;

	alertpanel_create(title, message, NULL, NULL, NULL, FALSE);
	alertpanel_show();
}

AlertValue alertpanel_message_with_disable(const gchar	*title,
					   const gchar	*message)
{
	if (alertpanel_is_open)
		return;
	else
		alertpanel_is_open = TRUE;

	alertpanel_create(title, message, NULL, NULL, NULL, TRUE);
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

	alertpanel_message(_("Notice"), buf);
}

void alertpanel_warning(const gchar *format, ...)
{
	va_list args;
	gchar buf[256];

	va_start(args, format);
	g_vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);
	strretchomp(buf);

	alertpanel_message(_("Warning"), buf);
}

void alertpanel_error(const gchar *format, ...)
{
	va_list args;
	gchar buf[256];

	va_start(args, format);
	g_vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);
	strretchomp(buf);

	alertpanel_message(_("Error"), buf);
}

static void alertpanel_show(void)
{
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	manage_window_set_transient(GTK_WINDOW(dialog));
	value = G_ALERTWAIT;

	while ((value & G_ALERT_VALUE_MASK) == G_ALERTWAIT)
		gtk_main_iteration();

	gtk_widget_destroy(dialog);
	GTK_EVENTS_FLUSH();

	alertpanel_is_open = FALSE;
}

static void alertpanel_create(const gchar *title,
			      const gchar *message,
			      const gchar *button1_label,
			      const gchar *button2_label,
			      const gchar *button3_label,
			      gboolean	   can_disable)
{
	static GdkFont *titlefont;
	GtkStyle *style;
	GtkWidget *label;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *confirm_area;
	GtkWidget *button1;
	GtkWidget *button2;
	GtkWidget *button3;
	const gchar *label2;
	const gchar *label3;
	GtkWidget *hbox2;
	GtkWidget *disable_chkbtn;
	GtkWidget *box_for_buttons;

	debug_print(_("Creating alert panel dialog...\n"));

	dialog = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(dialog), title);
	gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, FALSE, FALSE);
	gtk_container_set_border_width
		(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area), 5);
	gtk_window_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	gtk_signal_connect(GTK_OBJECT(dialog), "delete_event",
			   GTK_SIGNAL_FUNC(alertpanel_close),
			   (gpointer)G_ALERTOTHER);
	gtk_signal_connect(GTK_OBJECT(dialog), "key_press_event",
			   GTK_SIGNAL_FUNC(alertpanel_close),
			   (gpointer)G_ALERTOTHER);
	gtk_widget_realize(dialog);

	/* for title label */
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
			   hbox, TRUE, TRUE, 16);

	/* title label */
	//pixmapwid = create_pixmapwid(dialog, GNUstep_xpm);
	//gtk_box_pack_start(GTK_BOX(hbox), pixmapwid, FALSE, FALSE, 16);
	label = gtk_label_new(title);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 16);
	style = gtk_style_copy(gtk_widget_get_style(label));
	if (!titlefont)
		titlefont = gdk_fontset_load(TITLE_FONT);
	if (titlefont)
		style->font = titlefont;
	gtk_widget_set_style(label, style);

	/* for message and button(s) */
	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area),
			  vbox);

	/* for message label */
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 20);

	/* message label */
	label = gtk_label_new(message);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 32);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);

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

	if (can_disable) {
		hbox2 = gtk_hbox_new(FALSE, 8);
		gtk_box_pack_end(GTK_BOX(vbox), hbox2, FALSE, FALSE, 0);
		disable_chkbtn = gtk_check_button_new_with_label
			(_("Show this message next time"));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(disable_chkbtn),
					     TRUE);
		gtk_signal_connect(GTK_OBJECT(disable_chkbtn), "toggled",
				   GTK_SIGNAL_FUNC(alertpanel_button_toggled),
				   GUINT_TO_POINTER(G_ALERTDISABLE));
		gtk_box_pack_start(GTK_BOX(hbox2), disable_chkbtn,
				   TRUE, TRUE, 0);
		box_for_buttons = hbox2;
	} else
		box_for_buttons = vbox;

	gtk_box_pack_end(GTK_BOX(box_for_buttons), confirm_area,
			 FALSE, FALSE, 0);
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
			   (gpointer)G_ALERTDEFAULT);
	if (button2_label)
		gtk_signal_connect(GTK_OBJECT(button2), "clicked",
				   GTK_SIGNAL_FUNC(alertpanel_button_clicked),
				   (gpointer)G_ALERTALTERNATE);
	if (button3_label)
		gtk_signal_connect(GTK_OBJECT(button3), "clicked",
				   GTK_SIGNAL_FUNC(alertpanel_button_clicked),
				   (gpointer)G_ALERTOTHER);

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

static void alertpanel_close(GtkWidget *widget, GdkEventAny *event,
			     gpointer data)
{
	if (event->type == GDK_KEY_PRESS)
		if (((GdkEventKey *)event)->keyval != GDK_Escape)
			return;

	value = (value & ~G_ALERT_VALUE_MASK) | (AlertValue)data;
}
