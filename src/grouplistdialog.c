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

#include "defs.h"

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
#include <gtk/gtkclist.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkhbbox.h>

#include "intl.h"
#include "grouplistdialog.h"
#include "manage_window.h"
#include "gtkutils.h"
#include "utils.h"
#include "news.h"
#include "folder.h"
#include "alertpanel.h"
#include "recv.h"
#include "socket.h"

#define GROUPLIST_DIALOG_WIDTH	420
#define GROUPLIST_DIALOG_HEIGHT	400

static gboolean ack;
static gboolean locked;

static GtkWidget *dialog;
static GtkWidget *entry;
static GtkWidget *clist;
static GtkWidget *status_label;
static GtkWidget *ok_button;
static GSList *group_list;
static Folder *news_folder;

static void grouplist_dialog_create	(void);
static void grouplist_dialog_set_list	(void);
static void grouplist_clear		(void);
static void grouplist_recv_func		(SockInfo	*sock,
					 gint		 count,
					 gint		 read_bytes,
					 gpointer	 data);

static void ok_clicked		(GtkWidget	*widget,
				 gpointer	 data);
static void cancel_clicked	(GtkWidget	*widget,
				 gpointer	 data);
static void refresh_clicked	(GtkWidget	*widget,
				 gpointer	 data);
static void key_pressed		(GtkWidget	*widget,
				 GdkEventKey	*event,
				 gpointer	 data);
static void clist_selected	(GtkCList	*clist,
				 gint		 row,
				 gint		 column,
				 GdkEventButton	*event,
				 gpointer	 user_data);
static void entry_activated	(GtkEditable	*editable);

gchar *grouplist_dialog(Folder *folder)
{
	gchar *str;

	if (dialog && GTK_WIDGET_VISIBLE(dialog)) return NULL;

	if (!dialog)
		grouplist_dialog_create();

	news_folder = folder;

	gtk_widget_show(dialog);
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	manage_window_set_transient(GTK_WINDOW(dialog));
	GTK_EVENTS_FLUSH();

	grouplist_dialog_set_list();

	gtk_main();

	manage_window_focus_out(dialog, NULL, NULL);
	gtk_widget_hide(dialog);

	if (ack) {
		str = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
		if (str && *str == '\0') {
			g_free(str);
			str = NULL;
		}
	} else
		str = NULL;

	grouplist_clear();
	GTK_EVENTS_FLUSH();

	debug_print("return string = %s\n", str ? str : "(none)");
	return str;
}

static void grouplist_clear(void)
{
	gtk_clist_clear(GTK_CLIST(clist));
	gtk_entry_set_text(GTK_ENTRY(entry), "");
	slist_free_strings(group_list);
	g_slist_free(group_list);
}

static void grouplist_dialog_create(void)
{
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *msg_label;
	GtkWidget *confirm_area;
	GtkWidget *cancel_button;	
	GtkWidget *refresh_button;	
	GtkWidget *scrolledwin;

	dialog = gtk_dialog_new();
	gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, TRUE, FALSE);
	gtk_widget_set_usize(dialog,
			     GROUPLIST_DIALOG_WIDTH, GROUPLIST_DIALOG_HEIGHT);
	gtk_container_set_border_width
		(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area), 5);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	gtk_window_set_title(GTK_WINDOW(dialog), _("Subscribe to newsgroup"));
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

	msg_label = gtk_label_new(_("Input subscribing newsgroup:"));
	gtk_box_pack_start(GTK_BOX(hbox), msg_label, FALSE, FALSE, 0);

	entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(vbox), entry, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(entry), "activate",
			   GTK_SIGNAL_FUNC(entry_activated), NULL);

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_box_pack_start(GTK_BOX (vbox), scrolledwin, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW (scrolledwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);

	clist = gtk_clist_new(1);
	gtk_container_add(GTK_CONTAINER(scrolledwin), clist);
	gtk_clist_set_column_width(GTK_CLIST(clist), 0, 80);
	gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_BROWSE);
	GTK_WIDGET_UNSET_FLAGS(GTK_CLIST(clist)->column[0].button,
			       GTK_CAN_FOCUS);
	gtk_signal_connect(GTK_OBJECT(clist), "select_row",
			   GTK_SIGNAL_FUNC(clist_selected), NULL);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	status_label = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(hbox), status_label, FALSE, FALSE, 0);

	gtkut_button_set_create(&confirm_area,
				&ok_button,      _("OK"),
				&cancel_button,  _("Cancel"),
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
	GSList *cur;
	gint row;

	if (locked) return;
	locked = TRUE;

	recv_set_ui_func(grouplist_recv_func, NULL);
	group_list = news_get_group_list(news_folder);
	recv_set_ui_func(NULL, NULL);
	if (group_list == NULL) {
		alertpanel_error(_("Can't retrieve newsgroup list."));
		locked = FALSE;
		return;
	}

	gtk_clist_freeze(GTK_CLIST(clist));
	for (cur = group_list; cur != NULL ; cur = cur->next) {
		row = gtk_clist_append(GTK_CLIST(clist),
				       (gchar **)&(cur->data));
		gtk_clist_set_row_data(GTK_CLIST(clist), row, cur->data);
	}
	gtk_clist_thaw(GTK_CLIST(clist));

	gtk_widget_grab_focus(ok_button);
	gtk_widget_grab_focus(clist);

	gtk_label_set_text(GTK_LABEL(status_label), _("Done."));

	locked = FALSE;
}

static void grouplist_recv_func(SockInfo *sock, gint count, gint read_bytes,
				gpointer data)
{
	gchar buf[BUFFSIZE];

	g_snprintf(buf, sizeof(buf),
		   _("%d newsgroups received (%s read)"),
		   count, to_human_readable(read_bytes));
	gtk_label_set_text(GTK_LABEL(status_label), buf);
	GTK_EVENTS_FLUSH();
}

static void ok_clicked(GtkWidget *widget, gpointer data)
{
	ack = TRUE;
	if (gtk_main_level() > 1)
		gtk_main_quit();
}

static void cancel_clicked(GtkWidget *widget, gpointer data)
{
	ack = FALSE;
	if (gtk_main_level() > 1)
		gtk_main_quit();
}

static void refresh_clicked(GtkWidget *widget, gpointer data)
{ 
	if (locked) return;

	grouplist_clear();
	news_remove_group_list(news_folder);
	grouplist_dialog_set_list();
}

static void key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		cancel_clicked(NULL, NULL);
}

static void clist_selected(GtkCList *clist, gint row, gint column,
			   GdkEventButton *event, gpointer user_data)
{
	gchar *group;

	group = (gchar *)gtk_clist_get_row_data(GTK_CLIST(clist), row);
	gtk_entry_set_text(GTK_ENTRY(entry), group ? group : "");
}

static void entry_activated(GtkEditable *editable)
{
	ok_clicked(NULL, NULL);
}
