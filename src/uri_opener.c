/*
 * Claws Mail -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2007 Colin Leroy <colin@colino.net> and 
 * the Claws Mail team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gtk/gtkwidget.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <sys/types.h>
#include <dirent.h>

#include "manage_window.h"
#include "utils.h"
#include "mainwindow.h"
#include "gtkutils.h"
#include "alertpanel.h"
#include "textview.h"
#include "prefs_common.h"
#include "prefs_common.h"

enum {
	URI_OPENER_URL,
	URI_OPENER_DATA,
	N_URI_OPENER_COLUMNS
};


static struct URIOpener
{
	GtkWidget *window;
	GtkWidget *hbox1;
	GtkWidget *vbox1;
	GtkWidget *label;
	GtkWidget *urilist;
	GtkWidget *open_btn;
	GtkWidget *open_all_btn;
	GtkWidget *close_btn;
	MessageView *msgview;
	GSList    *uris;
} opener;

static void uri_opener_load_uris (void);
static void uri_opener_open_cb		(GtkWidget *widget, gpointer data);
static void uri_opener_close_cb	(GtkWidget *widget, gpointer data);
static gboolean key_pressed		(GtkWidget *widget, GdkEventKey *event,
					 gpointer data);
static void uri_opener_double_clicked(GtkTreeView		*list_view,
				   	GtkTreePath		*path,
				   	GtkTreeViewColumn	*column,
				   	gpointer		 data);
static void uri_opener_create(void);
void uri_opener_open(MessageView *msgview, GSList *uris)
{
	g_return_if_fail(msgview);
	g_return_if_fail(msgview->mimeview);
	g_return_if_fail(msgview->mimeview->textview);
	g_return_if_fail(msgview);
	if (!opener.window)
		uri_opener_create();

	manage_window_set_transient(GTK_WINDOW(opener.window));
	gtk_widget_grab_focus(opener.close_btn);
	
	if (uris == NULL) {
		alertpanel_notice(_("There are no URLs in this email."));
		return;
	}
	
	opener.msgview = msgview;
	opener.uris = g_slist_copy(uris);
	uri_opener_load_uris();

	gtk_widget_show(opener.window);
	gtk_widget_grab_focus(opener.urilist);
	gtk_window_set_modal(GTK_WINDOW(opener.window), TRUE);
}

static GtkListStore* uri_opener_create_data_store(void)
{
	return gtk_list_store_new(N_URI_OPENER_COLUMNS,
				  G_TYPE_STRING,
  				  G_TYPE_POINTER,
				  -1);
}

static void uri_opener_create_list_view_columns(GtkWidget *list_view)
{
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes
		(_("Available URLs:"),
		 renderer,
		 "text", URI_OPENER_URL,
		 NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(list_view), column);		
}

static GtkWidget *uri_opener_list_view_create	(void)
{
	GtkTreeView *list_view;
	GtkTreeSelection *selector;
	GtkTreeModel *model;

	model = GTK_TREE_MODEL(uri_opener_create_data_store());
	list_view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(model));
	g_object_unref(model);	
	
	gtk_tree_view_set_rules_hint(list_view, prefs_common.use_stripes_everywhere);
	
	selector = gtk_tree_view_get_selection(list_view);
	gtk_tree_selection_set_mode(selector, GTK_SELECTION_BROWSE);

	g_signal_connect(G_OBJECT(list_view), "row_activated",
	                 G_CALLBACK(uri_opener_double_clicked),
			 list_view);

	/* create the columns */
	uri_opener_create_list_view_columns(GTK_WIDGET(list_view));

	return GTK_WIDGET(list_view);

}

static void uri_opener_create(void) 
{
	GtkWidget *window;
	GtkWidget *hbox1;
	GtkWidget *vbox1;
	GtkWidget *label;
	GtkWidget *urilist;
	GtkWidget *open_btn;
	GtkWidget *open_all_btn;
	GtkWidget *close_btn;
	GtkWidget *scrolledwin;

	window = gtkut_window_new(GTK_WINDOW_TOPLEVEL, "uri_opener");
	gtk_window_set_title (GTK_WINDOW(window),
			      Q_("Dialog title|Open URLs"));

	gtk_container_set_border_width (GTK_CONTAINER (window), 8);
	gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
	gtk_window_set_resizable(GTK_WINDOW (window), TRUE);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(uri_opener_close_cb), NULL);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(key_pressed), NULL);
	MANAGE_WINDOW_SIGNALS_CONNECT (window);

	vbox1 = gtk_vbox_new(FALSE, 6);
	gtkut_stock_button_set_create(&hbox1, 
				      &close_btn, GTK_STOCK_CLOSE,
				      &open_all_btn, _("Open _All"),
				      &open_btn, GTK_STOCK_OPEN);

	g_signal_connect(G_OBJECT(open_btn), "clicked",
			 G_CALLBACK(uri_opener_open_cb), GINT_TO_POINTER(FALSE));

	g_signal_connect(G_OBJECT(open_all_btn), "clicked",
			 G_CALLBACK(uri_opener_open_cb), GINT_TO_POINTER(TRUE));

	g_signal_connect(G_OBJECT(close_btn), "clicked",
			 G_CALLBACK(uri_opener_close_cb), NULL);

	urilist = uri_opener_list_view_create();
	
	label = gtk_label_new(_("Please select the URL to open."));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(vbox1), label, FALSE, TRUE, 0);
	
	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
				       
	gtk_widget_set_size_request(scrolledwin, 500, 250);

	gtk_container_add(GTK_CONTAINER(scrolledwin), urilist);
	gtk_box_pack_start(GTK_BOX(vbox1), scrolledwin, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 0);
	
	gtk_widget_show(label);
	gtk_widget_show(scrolledwin);
	gtk_widget_show(urilist);
	gtk_widget_show(hbox1);
	gtk_widget_show(vbox1);
	gtk_widget_show(close_btn);
	gtk_widget_show(open_btn);
	gtk_widget_show(open_all_btn);
	gtk_container_add(GTK_CONTAINER (window), vbox1);

	opener.window = window;
	opener.hbox1 = hbox1;
	opener.vbox1 = vbox1;
	opener.label = label;
	opener.urilist = urilist;
	opener.open_btn = open_btn;
	opener.open_all_btn = open_all_btn;
	opener.close_btn = close_btn;

}

static void uri_opener_list_view_insert_uri(GtkWidget *list_view,
						  GtkTreeIter *row_iter,
						  ClickableText *uri) 
{
	GtkTreeIter iter;
	GtkListStore *list_store = GTK_LIST_STORE(gtk_tree_view_get_model
					(GTK_TREE_VIEW(list_view)));
	gchar *visible = textview_get_visible_uri(opener.msgview->mimeview->textview, uri);
	
	if (row_iter == NULL) {
		/* append new */
		gtk_list_store_append(list_store, &iter);
		gtk_list_store_set(list_store, &iter,
				   URI_OPENER_URL, visible?visible:uri->uri,
				   URI_OPENER_DATA, uri,
				   -1);
	} else {
		gtk_list_store_set(list_store, row_iter,
				   URI_OPENER_URL, visible?visible:uri->uri,
				   URI_OPENER_DATA, uri,
				   -1);
	}
	g_free(visible);
}

static void uri_opener_list_view_clear_uris(GtkWidget *list_view)
{
	GtkListStore *list_store = GTK_LIST_STORE(gtk_tree_view_get_model
					(GTK_TREE_VIEW(list_view)));
	gtk_list_store_clear(list_store);
}

static void uri_opener_load_uris (void) 
{
	GSList *cur = opener.uris;
	
	uri_opener_list_view_clear_uris(opener.urilist);
	for (; cur; cur = cur->next) {
		ClickableText *uri = (ClickableText *)cur->data;
		uri_opener_list_view_insert_uri(opener.urilist, NULL, uri);
	}
}

static void uri_opener_close(void) 
{
	g_slist_free(opener.uris);
	opener.uris = NULL;
	gtk_widget_hide(opener.window);
}

static void uri_opener_close_cb(GtkWidget *widget,
			         gpointer data) 
{
	uri_opener_close();
}

static gboolean key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		uri_opener_close();
	return FALSE;
}

static void uri_opener_double_clicked(GtkTreeView		*list_view,
				   	GtkTreePath		*path,
				   	GtkTreeViewColumn	*column,
				   	gpointer		 data)
{
	ClickableText *uri;
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model(list_view);

	if (!gtk_tree_model_get_iter(model, &iter, path))
		return;

	gtk_tree_model_get(model, &iter, 
			   URI_OPENER_DATA, &uri,
			   -1);

	if (!uri)
		return;

	if (textview_uri_security_check(opener.msgview->mimeview->textview, uri) == TRUE) 
		open_uri(uri->uri,
			 prefs_common.uri_cmd);
}




static void uri_opener_open_cb(GtkWidget *widget, 
			        gpointer data) 
{
	ClickableText *uri;
	GtkTreeIter sel;
	GtkTreeModel *model;
	gboolean all = GPOINTER_TO_INT(data);

	if (!gtk_tree_selection_get_selected(gtk_tree_view_get_selection
				(GTK_TREE_VIEW(opener.urilist)),
				&model, &sel))
		return;
	
	if (!all) {
		gtk_tree_model_get(model, &sel,
			   URI_OPENER_DATA, &uri,
			   -1);
		if (!uri)
			return;

		if (textview_uri_security_check(opener.msgview->mimeview->textview, uri) == TRUE) 
			open_uri(uri->uri,
				 prefs_common.uri_cmd);
	} else {
		GSList *cur = opener.uris;

		for (; cur; cur = cur->next) {
			uri = (ClickableText *)cur->data;
			if (textview_uri_security_check(opener.msgview->mimeview->textview, uri) == TRUE) 
				open_uri(uri->uri,
					 prefs_common.uri_cmd);
		}
	}
}
