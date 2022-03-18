/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2022 the Claws Mail team and Colin Leroy
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

#ifdef USE_GNUTLS
#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <sys/types.h>
#include <dirent.h>

#include "ssl_manager.h"
#include "ssl_certificate.h"
#include "manage_window.h"
#include "utils.h"
#include "mainwindow.h"
#include "alertpanel.h"
#include "sslcertwindow.h"
#include "prefs_common.h"

enum {
	SSL_MANAGER_HOST,
	SSL_MANAGER_PORT,
	SSL_MANAGER_CERT,
	SSL_MANAGER_STATUS,
	SSL_MANAGER_EXPIRY,
	SSL_MANAGER_FONT_WEIGHT,
	N_SSL_MANAGER_COLUMNS
};


static struct SSLManager
{
	GtkWidget *window;
	GtkWidget *hbox1;
	GtkWidget *vbox1;
	GtkWidget *certlist;
	GtkWidget *view_btn;
	GtkWidget *delete_btn;
	GtkWidget *close_btn;
} manager;

static void ssl_manager_view_cb		(GtkWidget *widget, gpointer data);
static void ssl_manager_delete_cb	(GtkWidget *widget, gpointer data);
static void ssl_manager_close_cb	(GtkWidget *widget, gpointer data);
static gboolean key_pressed		(GtkWidget *widget, GdkEventKey *event,
					 gpointer data);
static void ssl_manager_load_certs	(void);
static void ssl_manager_double_clicked(GtkTreeView		*list_view,
				   	GtkTreePath		*path,
				   	GtkTreeViewColumn	*column,
				   	gpointer		 data);

void ssl_manager_open(MainWindow *mainwin)
{
	if (!manager.window)
		ssl_manager_create();

	manage_window_set_transient(GTK_WINDOW(manager.window));
	gtk_widget_grab_focus(manager.close_btn);

	ssl_manager_load_certs();

	gtk_widget_show(manager.window);

}

static GtkListStore* ssl_manager_create_data_store(void)
{
	return gtk_list_store_new(N_SSL_MANAGER_COLUMNS,
				  G_TYPE_STRING,
				  G_TYPE_STRING,
  				  G_TYPE_POINTER,
				  G_TYPE_STRING,
				  G_TYPE_STRING,
			   	  G_TYPE_INT,
				  -1);
}

static void ssl_manager_create_list_view_columns(GtkWidget *list_view)
{
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "weight", PANGO_WEIGHT_NORMAL,
               	     "weight-set", TRUE, NULL);

	column = gtk_tree_view_column_new_with_attributes
		(_("Server"),
		 renderer,
		 "text", SSL_MANAGER_HOST,
		 "weight", SSL_MANAGER_FONT_WEIGHT,
		 NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(list_view), column);

	column = gtk_tree_view_column_new_with_attributes
		(_("Port"),
		 renderer,
		 "text", SSL_MANAGER_PORT,
		 NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(list_view), column);

	column = gtk_tree_view_column_new_with_attributes
		(_("Status"),
		 renderer,
		 "text", SSL_MANAGER_STATUS,
		 NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(list_view), column);

	column = gtk_tree_view_column_new_with_attributes
		(_("Expiry"),
		 renderer,
		 "text", SSL_MANAGER_EXPIRY,
		 NULL);
	gtk_tree_view_column_set_attributes
		(column, renderer,
		 "text", SSL_MANAGER_EXPIRY,
		 NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(list_view), column);
}

static GtkWidget *ssl_manager_list_view_create	(void)
{
	GtkTreeView *list_view;
	GtkTreeSelection *selector;
	GtkTreeModel *model;

	model = GTK_TREE_MODEL(ssl_manager_create_data_store());
	list_view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(model));
	
 	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(model),
                                             0, GTK_SORT_ASCENDING);
	g_object_unref(model);	
	gtk_tree_view_set_rules_hint(list_view, prefs_common.use_stripes_everywhere);
	
	selector = gtk_tree_view_get_selection(list_view);
	gtk_tree_selection_set_mode(selector, GTK_SELECTION_BROWSE);

	g_signal_connect(G_OBJECT(list_view), "row_activated",
	                 G_CALLBACK(ssl_manager_double_clicked),
			 list_view);

	/* create the columns */
	ssl_manager_create_list_view_columns(GTK_WIDGET(list_view));

	return GTK_WIDGET(list_view);
}

/*!
 *\brief	Save Gtk object size to prefs dataset
 */
static void ssl_manager_size_allocate_cb(GtkWidget *widget,
					 GtkAllocation *allocation)
{
	cm_return_if_fail(allocation != NULL);

	gtk_window_get_size(GTK_WINDOW(widget),
		&prefs_common.sslmanwin_width, &prefs_common.sslmanwin_height);
}

void ssl_manager_create(void)
{
	GtkWidget *window;
	GtkWidget *scroll;
	GtkWidget *hbox1;
	GtkWidget *vbox1;
	GtkWidget *certlist;
	GtkWidget *view_btn;
	GtkWidget *delete_btn;
	GtkWidget *close_btn;
	static GdkGeometry geometry;

	window = gtkut_window_new(GTK_WINDOW_TOPLEVEL, "ssl_manager");
	gtk_window_set_title (GTK_WINDOW(window),
			      _("Saved TLS certificates"));

	gtk_container_set_border_width (GTK_CONTAINER (window), 8);
	gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
	gtk_window_set_resizable(GTK_WINDOW (window), TRUE);
	gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DIALOG);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(ssl_manager_close_cb), NULL);
	g_signal_connect(G_OBJECT(window), "size_allocate",
			 G_CALLBACK(ssl_manager_size_allocate_cb), NULL);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(key_pressed), NULL);
	MANAGE_WINDOW_SIGNALS_CONNECT (window);

	hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	vbox1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	delete_btn = gtkut_stock_button("edit-delete", _("D_elete"));

	g_signal_connect(G_OBJECT(delete_btn), "clicked",
			 G_CALLBACK(ssl_manager_delete_cb), NULL);

	view_btn = gtkut_stock_button("dialog-information", _("_Information"));
	g_signal_connect(G_OBJECT(view_btn), "clicked",
			 G_CALLBACK(ssl_manager_view_cb), NULL);

	close_btn = gtk_button_new_with_mnemonic("_Close");
	gtk_button_set_image(GTK_BUTTON(close_btn),
			gtk_image_new_from_icon_name("window-close", GTK_ICON_SIZE_BUTTON));
	g_signal_connect(G_OBJECT(close_btn), "clicked",
			 G_CALLBACK(ssl_manager_close_cb), NULL);

	certlist = ssl_manager_list_view_create();

	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
					GTK_POLICY_NEVER,
					GTK_POLICY_AUTOMATIC);

	gtk_container_add(GTK_CONTAINER (scroll), certlist);

	gtk_box_pack_start(GTK_BOX(hbox1), scroll, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox1), vbox1, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox1), view_btn, FALSE, FALSE, 4);
	gtk_box_pack_start(GTK_BOX(vbox1), delete_btn, FALSE, FALSE, 4);
	gtk_box_pack_end(GTK_BOX(vbox1), close_btn, FALSE, FALSE, 4);

	if (!geometry.min_height) {
		geometry.min_width = 700;
		geometry.min_height = 250;
	}

	gtk_window_set_geometry_hints(GTK_WINDOW(window), NULL, &geometry,
				      GDK_HINT_MIN_SIZE);
	gtk_window_set_default_size(GTK_WINDOW(window),
				    prefs_common.sslmanwin_width,
				    prefs_common.sslmanwin_height);

	gtk_widget_show(certlist);
	gtk_widget_show(scroll);
	gtk_widget_show(hbox1);
	gtk_widget_show(vbox1);
	gtk_widget_show(close_btn);
	gtk_widget_show(delete_btn);
	gtk_widget_show(view_btn);
	gtk_container_add(GTK_CONTAINER (window), hbox1);

	manager.window = window;
	manager.hbox1 = hbox1;
	manager.vbox1 = vbox1;
	manager.certlist = certlist;
	manager.view_btn = view_btn;
	manager.delete_btn = delete_btn;
	manager.close_btn = close_btn;

	gtk_widget_show(window);
}

static void ssl_manager_list_view_insert_cert(GtkWidget *list_view,
						  GtkTreeIter *row_iter,
						  gchar *host, 
						  gchar *port,
						  SSLCertificate *cert) 
{
	char *sig_status, *exp_date;
	char buf[100];
	time_t exp_time_t;
	struct tm lt;
	PangoWeight weight = PANGO_WEIGHT_NORMAL;
	GtkTreeIter iter, *iterptr;
	GtkListStore *list_store = GTK_LIST_STORE(gtk_tree_view_get_model
					(GTK_TREE_VIEW(list_view)));

	g_return_if_fail(cert != NULL);

	exp_time_t = gnutls_x509_crt_get_expiration_time(cert->x509_cert);

	memset(buf, 0, sizeof(buf));
	if (exp_time_t > 0) {
		fast_strftime(buf, sizeof(buf)-1, prefs_common.date_format, localtime_r(&exp_time_t, &lt));
		exp_date = (*buf) ? g_strdup(buf):g_strdup("?");
	} else
		exp_date = g_strdup("");

	if (exp_time_t < time(NULL))
		weight = PANGO_WEIGHT_BOLD;

	sig_status = ssl_certificate_check_signer(cert, cert->status);

	if (sig_status == NULL)
		sig_status = g_strdup_printf(_("Correct%s"),exp_time_t < time(NULL)? _(" (expired)"): "");
	else {
		 weight = PANGO_WEIGHT_BOLD;
		 if (exp_time_t < time(NULL))
			  sig_status = g_strconcat(sig_status,_(" (expired)"),NULL);
	}

	if (row_iter == NULL) {
		/* append new */
		gtk_list_store_append(list_store, &iter);
		iterptr = &iter;
	} else
		iterptr = row_iter;

	gtk_list_store_set(list_store, iterptr,
			   SSL_MANAGER_HOST, host,
			   SSL_MANAGER_PORT, port,
			   SSL_MANAGER_CERT, cert,
		    	   SSL_MANAGER_STATUS, sig_status,
		    	   SSL_MANAGER_EXPIRY, exp_date,
			   SSL_MANAGER_FONT_WEIGHT, weight,
			   -1);

	g_free(sig_status);
	g_free(exp_date);
}

static void ssl_manager_load_certs (void) 
{
	GDir *dir;
	const gchar *d;
	GError *error = NULL;
	gchar *path;
	int row = 0;
	GtkListStore *store;

	store = GTK_LIST_STORE(gtk_tree_view_get_model
				(GTK_TREE_VIEW(manager.certlist)));

	gtk_list_store_clear(store);

	path = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, 
			  "certs", G_DIR_SEPARATOR_S, NULL);

	if((dir = g_dir_open(path, 0, &error)) == NULL) {
		debug_print("couldn't open dir '%s': %s (%d)\n", path,
				error->message, error->code);
		g_error_free(error);
        g_free(path);
		return;
	}
	
	while ((d = g_dir_read_name(dir)) != NULL) {
		gchar *server = NULL, *port = NULL, *fp = NULL;
		SSLCertificate *cert;

		if(strstr(d, ".cert") != d + (strlen(d) - strlen(".cert"))) 
			continue;

		if (get_serverportfp_from_filename(d, &server, &port, &fp)) {

			if (server != NULL && port != NULL) {
				gint portnum = atoi(port);
				if (portnum > 0 && portnum <= 65535) {
					cert = ssl_certificate_find(server, portnum, fp);
					ssl_manager_list_view_insert_cert(manager.certlist, NULL,
							server, port, cert);
				}
			}
		}
		if (server)
			g_free(server);
		if (port)
			g_free(port);
		if (fp)
			g_free(fp);
		row++;
	}
	g_dir_close(dir);
	g_free(path);
}

static void ssl_manager_close(void) 
{
	gtk_widget_hide(manager.window);
}

static void ssl_manager_close_cb(GtkWidget *widget,
			         gpointer data) 
{
	ssl_manager_close();
}

static gboolean key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (!event)
		return FALSE;

	if (event->keyval == GDK_KEY_Escape)
		ssl_manager_close();
	else if (event->keyval == GDK_KEY_Delete)
		ssl_manager_delete_cb(manager.delete_btn, NULL);

	return FALSE;
}

static void ssl_manager_double_clicked(GtkTreeView		*list_view,
				   	GtkTreePath		*path,
				   	GtkTreeViewColumn	*column,
				   	gpointer		 data)
{
	SSLCertificate *cert;
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model(list_view);

	if (!gtk_tree_model_get_iter(model, &iter, path))
		return;

	gtk_tree_model_get(model, &iter, 
			   SSL_MANAGER_CERT, &cert,
			   -1);

	if (!cert)
		return;

	sslcertwindow_show_cert(cert);

	return;
}



static void ssl_manager_delete_cb(GtkWidget *widget, 
			      gpointer data) 
{
	SSLCertificate *cert;
	int val;
	GtkTreeIter iter;
	GtkTreeModel *model;

	cert = gtkut_tree_view_get_selected_pointer(
			GTK_TREE_VIEW(manager.certlist), SSL_MANAGER_CERT,
			&model, NULL, &iter);

	if (!cert)
		return;

	val = alertpanel_full(_("Delete certificate"),
			      _("Do you really want to delete this certificate?"),
		 	      NULL, _("_Cancel"), "edit-delete", _("D_elete"), NULL, NULL,
			      ALERTFOCUS_FIRST, FALSE, NULL, ALERT_WARNING);

			     
	if (val != G_ALERTALTERNATE)
		return;
	
	ssl_certificate_delete_from_disk(cert);
	ssl_certificate_destroy(cert);
	gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
}

static void ssl_manager_view_cb(GtkWidget *widget, 
			        gpointer data) 
{
	SSLCertificate *cert;

	cert = gtkut_tree_view_get_selected_pointer(
			GTK_TREE_VIEW(manager.certlist), SSL_MANAGER_CERT,
			NULL, NULL, NULL);

	if (!cert)
		return;

	sslcertwindow_show_cert(cert);
}
#endif
