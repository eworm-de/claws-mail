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

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "intl.h"

static GtkWidget *quote_desc_win;

static void quote_fmt_quote_description_create		(void);
static void quote_fmt_quote_description_key_pressed	(GtkWidget *widget,
							 GdkEventKey *event,
							 gpointer data);

static gchar *symbol_table[][2] =
{
	{"%d",		N_("Date")}, /* date */
	{"%f",		N_("From")}, /* from */
	{"%N",		N_("Full Name of Sender")}, /* full name */
	{"%F",		N_("First Name of Sender")}, /* first name */
	{"%L",		N_("Last Name of Sender")}, /* last name */
	{"%I",		N_("Initials of Sender")}, /* initial of sender */
	{"%s",		N_("Subject")}, /* subject */ 
	{"%t",		N_("To")}, /* to */ 
	{"%c",		N_("Cc")}, /* cc */ 
	{"%n",		N_("Newsgroups")}, /* newsgroups */ 
	{"%r",		N_("References")}, /* references */ 
	{"%i",		N_("Message-ID")}, /* message-id */ 
	{"%M",		N_("Message body")}, /* message */ 
	{"%Q",		N_("Quoted message body")}, /* quoted message */ 
	{"%m",		N_("Message body without signature")}, /* message with no signature */ 
	{"%q",		N_("Quoted message body without signature")}, /* quoted message with no signature */ 
	{"",		NULL},
	{"?x{expr}",	N_("Insert expr if x is set\nx is one of the characters above after %")},
	{"",		NULL},
	{"\\%", 	N_("Literal %")}, /* % */ 
	{"\\\\",	N_("Literal backslash")}, /* \ */ 
	{"\\?",		N_("Literal question mark")}, /* ? */ 
	{"\\|",		N_("Literal pipe")}, /* | */
	{"\\{",		N_("Literal opening curly brace")},
	{"\\}",		N_("Literal closing curly brace")},
	{"",		NULL},
	{"|f{file}",	N_("Insert File")}, /* insert file */ 
	{"|p{command}", N_("Insert program output")}, /* insert program output */ 
	{NULL,NULL},
};

void quote_fmt_quote_description(void)
{
	if (!quote_desc_win)
		quote_fmt_quote_description_create();

	manage_window_set_transient(GTK_WINDOW(quote_desc_win));
	gtk_widget_show(quote_desc_win);
	gtk_main();
	gtk_widget_hide(quote_desc_win);
}

static void quote_fmt_quote_description_create(void)
{
	GtkWidget *table;
	GtkWidget *hbbox;
	GtkWidget *ok_btn;
	int i;

	quote_desc_win = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_window_set_title(GTK_WINDOW(quote_desc_win),
			     _("Description of symbols"));
	gtk_container_set_border_width(GTK_CONTAINER(quote_desc_win), 8);
	gtk_window_set_position(GTK_WINDOW(quote_desc_win), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(quote_desc_win), TRUE);
	gtk_window_set_policy(GTK_WINDOW(quote_desc_win), FALSE, FALSE, FALSE);

	table = gtk_table_new(sizeof(symbol_table), 2, FALSE);
	gtk_container_add(GTK_CONTAINER(quote_desc_win), table);
	gtk_table_set_col_spacings(GTK_TABLE(table), 10);

	for(i = 0; symbol_table[i][0] != NULL; i++) {
		if(symbol_table[i][0][0] != '\0') {
			GtkWidget *label;

			label = gtk_label_new(symbol_table[i][0]);
			gtk_misc_set_alignment (GTK_MISC(label), 0, 0);
			gtk_table_attach(GTK_TABLE(table), label,
					 0, 1, i, i+1,
					 GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
					 0, 0);
			label = gtk_label_new(gettext(symbol_table[i][1]));
			gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
			gtk_misc_set_alignment (GTK_MISC(label), 0, 0);
			gtk_table_attach(GTK_TABLE(table), label,
					 1, 2, i, i+1,
					 GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
					 0, 0);
		} else {
			GtkWidget *separator;
			
			separator = gtk_hseparator_new();
			gtk_table_attach(GTK_TABLE(table), separator,
					 0, 2, i, i+1,
					 GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
					 0, 4);
		}
	}

	gtkut_button_set_create(&hbbox, &ok_btn, _("OK"),
				NULL, NULL, NULL, NULL);
	gtk_table_attach_defaults(GTK_TABLE(table), hbbox,
				  1, 2, i, i+1);

	gtk_widget_grab_default(ok_btn);
	gtk_signal_connect(GTK_OBJECT(ok_btn), "clicked",
			   GTK_SIGNAL_FUNC(gtk_main_quit), NULL);
	gtk_signal_connect
		(GTK_OBJECT(quote_desc_win), "key_press_event",
		 GTK_SIGNAL_FUNC(quote_fmt_quote_description_key_pressed),
		 NULL);
	gtk_signal_connect(GTK_OBJECT(quote_desc_win), "delete_event",
			   GTK_SIGNAL_FUNC(gtk_main_quit), NULL);

	gtk_widget_show_all(table);
}

static void quote_fmt_quote_description_key_pressed(GtkWidget *widget,
						GdkEventKey *event,
						gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		gtk_main_quit();
}

