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
#include <gtk/gtkclist.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkhbbox.h>

#include "intl.h"
#include "grouplist_dialog.h"
#include "manage_window.h"
#include "gtkutils.h"
#include "utils.h"
#include "news.h"
#include "folder.h"

#define GROUPLIST_DIALOG_WIDTH	420
#define GROUPLIST_DIALOG_HEIGHT	400

static gboolean ack;

static GtkWidget *dialog;
static GtkWidget *msg_label;
static GtkWidget *group_clist;
static GtkWidget *ok_button;
static gchar * group_selected;
static GSList * group_list;
static FolderItem * item;

static void grouplist_dialog_create	(void);
static void grouplist_dialog_set	(void);
static void grouplist_free		(void);

static void ok_clicked		(GtkWidget	*widget,
				 gpointer	 data);
static void cancel_clicked	(GtkWidget	*widget,
				 gpointer	 data);
static void refresh_clicked(GtkWidget *widget, gpointer data);
static void key_pressed		(GtkWidget	*widget,
				 GdkEventKey	*event,
				 gpointer	 data);
static void group_clist_select(GtkCList *clist, gint row, gint column,
			      GdkEventButton *event, gpointer user_data);

gchar *grouplist_dialog(FolderItem * i)
{
	gchar *str;

	if (dialog && GTK_WIDGET_VISIBLE(dialog)) return NULL;

	if (!dialog)
		grouplist_dialog_create();

	item = i;

	grouplist_dialog_set();
	gtk_widget_show(dialog);
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	manage_window_set_transient(GTK_WINDOW(dialog));

	gtk_main();

	manage_window_focus_out(dialog, NULL, NULL);
	gtk_widget_hide(dialog);

	if (ack) {
		if (group_selected != NULL)
			str = g_strdup(group_selected);
		else
			str = NULL;
		if (str && *str == '\0')
			str = NULL;
	} else
		str = NULL;

	grouplist_free();

	GTK_EVENTS_FLUSH();

	debug_print("return string = %s\n", str ? str : "(none)");
	return str;
}

static void grouplist_free(void)
{
	GSList * group_elt;

	for(group_elt = group_list ; group_elt != NULL ;
	    group_elt = group_elt->next)
	    g_free(group_elt->data);
	g_slist_free(group_list);
}

static void grouplist_dialog_create(void)
{
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *confirm_area;
	GtkWidget *cancel_button;	
	GtkWidget *refresh_button;	
	GtkWidget *scrolledwin;

	dialog = gtk_dialog_new();
	gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, FALSE, FALSE);
	gtk_widget_set_usize(dialog, GROUPLIST_DIALOG_WIDTH, GROUPLIST_DIALOG_HEIGHT);
	gtk_container_set_border_width
		(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area), 5);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	gtk_signal_connect(GTK_OBJECT(dialog), "delete_event",
			   GTK_SIGNAL_FUNC(cancel_clicked), NULL);
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


	scrolledwin = gtk_scrolled_window_new (NULL, NULL);
	gtk_box_pack_start (GTK_BOX (vbox), scrolledwin,
			    TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwin),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);

	group_clist = gtk_clist_new(1);
	gtk_container_add (GTK_CONTAINER (scrolledwin), group_clist);
	gtk_clist_set_column_width (GTK_CLIST (group_clist), 0, 80);
	gtk_clist_set_selection_mode (GTK_CLIST (group_clist),
				      GTK_SELECTION_BROWSE);
	GTK_WIDGET_UNSET_FLAGS (GTK_CLIST (group_clist)->column[0].button,
				GTK_CAN_FOCUS);
	gtk_signal_connect (GTK_OBJECT (group_clist), "select_row",
			    GTK_SIGNAL_FUNC (group_clist_select), NULL);

	gtkut_button_set_create(&confirm_area,
				&ok_button,     _("OK"),
				&cancel_button, _("Cancel"),
				&refresh_button, _("Refresh"));
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area),
			  confirm_area);
	gtk_widget_grab_default(ok_button);

	gtk_signal_connect(GTK_OBJECT(ok_button), "clicked",
			   GTK_SIGNAL_FUNC(ok_clicked), NULL);
	gtk_signal_connect(GTK_OBJECT(cancel_button), "clicked",
			   GTK_SIGNAL_FUNC(cancel_clicked), NULL);
	gtk_signal_connect(GTK_OBJECT(refresh_button), "clicked",
			   GTK_SIGNAL_FUNC(refresh_clicked), NULL);


	gtk_widget_show_all(GTK_DIALOG(dialog)->vbox);
}

static void grouplist_dialog_set_list(void)
{
	GSList * elt;
	gint row;

	group_selected = NULL;
	group_list = news_get_group_list(item);

	gtk_clist_clear(GTK_CLIST(group_clist));
	for(elt = group_list; elt != NULL ; elt = elt->next)
	  {
	    row = gtk_clist_append(GTK_CLIST(group_clist),
				   (void *) &(elt->data));
	    gtk_clist_set_row_data(GTK_CLIST(group_clist), row, elt->data);
	  }

	gtk_widget_grab_focus(ok_button);
	gtk_widget_grab_focus(group_clist);

	if (group_list != NULL)
	  group_selected = group_list->data;
	else
	  group_selected = NULL;
}

static void grouplist_dialog_set(void)
{
	gtk_window_set_title(GTK_WINDOW(dialog), _("Subscribe newsgroup"));
	gtk_label_set_text(GTK_LABEL(msg_label), _("Input subscribing newsgroup:"));
	grouplist_dialog_set_list();
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

static void refresh_clicked(GtkWidget *widget, gpointer data)
{ 
	grouplist_free();
	news_reset_group_list(item);
	grouplist_dialog_set_list();
}

static void key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event && event->keyval == GDK_Escape) {
		ack = FALSE;
		gtk_main_quit();
	}
}

static void group_clist_select(GtkCList *clist, gint row, gint column,
			      GdkEventButton *event, gpointer user_data)
{
  group_selected = (gchar *)
    gtk_clist_get_row_data(GTK_CLIST(group_clist), row);
}
