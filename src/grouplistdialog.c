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
#include <string.h>
#include <fnmatch.h>

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

#define GROUPLIST_DIALOG_WIDTH	500
#define GROUPLIST_NAMES	        250
#define GROUPLIST_DIALOG_HEIGHT	400

static GList * subscribed = NULL;
gboolean dont_unsubscribed = FALSE;

static gboolean ack;
static gboolean locked;

static GtkWidget *dialog;
static GtkWidget *entry;
static GtkWidget *clist;
static GtkWidget *status_label;
static GtkWidget *ok_button;
static GSList *group_list = NULL;
static Folder *news_folder;

static void grouplist_dialog_create	(void);
static void grouplist_dialog_set_list	(gchar * pattern);
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
static void clist_unselected	(GtkCList	*clist,
				 gint		 row,
				 gint		 column,
				 GdkEventButton	*event,
				 gpointer	 user_data);
static void entry_activated	(GtkEditable	*editable);

GList *grouplist_dialog(Folder *folder, GList * cur_subscriptions)
{
	gchar *str;
	GList * l;

	if (dialog && GTK_WIDGET_VISIBLE(dialog)) return NULL;

	if (!dialog)
		grouplist_dialog_create();

	news_folder = folder;

	gtk_widget_show(dialog);
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	manage_window_set_transient(GTK_WINDOW(dialog));
	GTK_EVENTS_FLUSH();

	subscribed = NULL;

	for(l = cur_subscriptions ; l != NULL ; l = l->next)
	  subscribed = g_list_append(subscribed, g_strdup((gchar *) l->data));

	grouplist_dialog_set_list(NULL);

	gtk_main();

	manage_window_focus_out(dialog, NULL, NULL);
	gtk_widget_hide(dialog);

	/*
	if (ack) {
		str = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
		if (str && *str == '\0') {
			g_free(str);
			str = NULL;
		}
	} else
		str = NULL;
	*/
	if (!ack) {
	  list_free_strings(subscribed);
	  g_list_free(subscribed);

	  subscribed = NULL;
	  
	  for(l = cur_subscriptions ; l != NULL ; l = l->next)
	    subscribed = g_list_append(subscribed, g_strdup((gchar *) l->data));
	}

	GTK_EVENTS_FLUSH();

	debug_print("return string = %s\n", str ? str : "(none)");
	return subscribed;
}

static void grouplist_clear(void)
{
	dont_unsubscribed = TRUE;
	gtk_clist_clear(GTK_CLIST(clist));
	gtk_entry_set_text(GTK_ENTRY(entry), "");
	dont_unsubscribed = FALSE;
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
	gchar * col_names[3] = {
		_("name"), _("count of messages"), _("type")
	};

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

	clist = gtk_clist_new_with_titles(3, col_names);
	gtk_container_add(GTK_CONTAINER(scrolledwin), clist);
	gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_MULTIPLE);
	GTK_WIDGET_UNSET_FLAGS(GTK_CLIST(clist)->column[0].button,
			       GTK_CAN_FOCUS);
	gtk_signal_connect(GTK_OBJECT(clist), "select_row",
			   GTK_SIGNAL_FUNC(clist_selected), NULL);
	gtk_signal_connect(GTK_OBJECT(clist), "unselect_row",
			   GTK_SIGNAL_FUNC(clist_unselected), NULL);

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

static void grouplist_dialog_set_list(gchar * pattern)
{
	GSList *cur;
	gint row;

	if (pattern == NULL)
		pattern = "*";

	if (locked) return;
	locked = TRUE;

	grouplist_clear();

	recv_set_ui_func(grouplist_recv_func, NULL);
	group_list = news_get_group_list(news_folder);
	recv_set_ui_func(NULL, NULL);
	if (group_list == NULL) {
		alertpanel_error(_("Can't retrieve newsgroup list."));
		locked = FALSE;
		return;
	}

	dont_unsubscribed = TRUE;

	gtk_clist_freeze(GTK_CLIST(clist));
	for (cur = group_list; cur != NULL ; cur = cur->next) {
		struct NNTPGroupInfo * info;

		info = (struct NNTPGroupInfo *) cur->data;

		if (fnmatch(pattern, info->name, 0) == 0) {
			gchar count_str[10];
			gchar * cols[3];
			gint count;
			GList * l;

			count = info->last - info->first;
			if (count < 0)
				count = 0;
			snprintf(count_str, 10, "%i", count);

			cols[0] = info->name;
			cols[1] = count_str;
			if (info->type == 'y')
				cols[2] = "";
			else if (info->type == 'm')
				cols[2] = "moderated";
			else if (info->type == 'n')
				cols[2] = "readonly";
			else
				cols[2] = "unkown";

			row = gtk_clist_append(GTK_CLIST(clist), cols);
			gtk_clist_set_row_data(GTK_CLIST(clist), row, info);

			l = g_list_find_custom(subscribed, info->name,
					       (GCompareFunc) g_strcasecmp);
			
			if (l != NULL)
			  gtk_clist_select_row(GTK_CLIST(clist), row, 0);
		}
	}

	gtk_clist_moveto(GTK_CLIST(clist), 0, 0, 0, 0);

	dont_unsubscribed = FALSE;

	gtk_clist_set_column_width(GTK_CLIST(clist), 0, GROUPLIST_NAMES);
	gtk_clist_set_column_justification (GTK_CLIST(clist), 1,
					    GTK_JUSTIFY_RIGHT);
	gtk_clist_set_column_auto_resize(GTK_CLIST(clist), 1, TRUE);
	gtk_clist_set_column_auto_resize(GTK_CLIST(clist), 2, TRUE);

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
	gchar * str;
	gboolean update_list;

	str = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);

	update_list = FALSE;

	if (strchr(str, '*') != NULL)
	  update_list = TRUE;

	if (update_list) {
		grouplist_dialog_set_list(str);
		g_free(str);
	}
	else {
		g_free(str);
		ack = TRUE;
		if (gtk_main_level() > 1)
			gtk_main_quit();
	}
}

static void cancel_clicked(GtkWidget *widget, gpointer data)
{
	ack = FALSE;
	if (gtk_main_level() > 1)
		gtk_main_quit();
}

static void refresh_clicked(GtkWidget *widget, gpointer data)
{
	gchar * str;
 
	if (locked) return;

	news_cancel_group_list_cache(news_folder);

	str = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
	grouplist_dialog_set_list(str);
	g_free(str);
}

static void key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		cancel_clicked(NULL, NULL);
}

static void clist_selected(GtkCList *clist, gint row, gint column,
			   GdkEventButton *event, gpointer user_data)
{
	struct NNTPGroupInfo * group;
	GList * l;

	group = (struct NNTPGroupInfo *)
	  gtk_clist_get_row_data(GTK_CLIST(clist), row);

	if (!dont_unsubscribed) {
		subscribed = g_list_append(subscribed, g_strdup(group->name));
	}
}

static void clist_unselected(GtkCList *clist, gint row, gint column,
			     GdkEventButton *event, gpointer user_data)
{
	struct NNTPGroupInfo * group;
	GList * l;

	group = (struct NNTPGroupInfo *)
	  gtk_clist_get_row_data(GTK_CLIST(clist), row);

	if (!dont_unsubscribed) {
		l = g_list_find_custom(subscribed, group->name,
				       (GCompareFunc) g_strcasecmp);
		if (l != NULL) {
		  g_free(l->data);
			subscribed = g_list_remove(subscribed, l->data);
		}
	}
}

static gboolean match_string(gchar * str, gchar * expr)
{
}

static void entry_activated(GtkEditable *editable)
{
	gchar * str;
	gboolean update_list;

	str = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);

	update_list = FALSE;

	if (strchr(str, '*') != NULL)
	  update_list = TRUE;

	if (update_list) {
		grouplist_dialog_set_list(str);
		g_free(str);
	}
	else {
		g_free(str);
		ok_clicked(NULL, NULL);
	}
}
