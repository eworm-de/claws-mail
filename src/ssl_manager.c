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

#ifdef USE_OPENSSL
#include <gtk/gtkwidget.h>
#include <glib.h>
#include <sys/types.h>
#include <dirent.h>

#include "ssl_manager.h"
#include "ssl_certificate.h"
#include "manage_window.h"
#include "utils.h"
#include "mainwindow.h"
#include "intl.h"
#include "gtksctree.h"
#include "alertpanel.h"
#include "sslcertwindow.h"

static struct SSLManager
{
	GtkWidget *window;
	GtkWidget *hbox1;
	GtkWidget *vbox1;
	GtkWidget *certlist;
	GtkWidget *view_btn;
	GtkWidget *delete_btn;
	GtkWidget *ok_btn;
} manager;

static void ssl_manager_view_cb		(GtkWidget *widget, gpointer data);
static void ssl_manager_delete_cb	(GtkWidget *widget, gpointer data);
static void ssl_manager_ok_cb		(GtkWidget *widget, gpointer data);
static void ssl_manager_load_certs	(void);

void ssl_manager_open(MainWindow *mainwin)
{
	if (!manager.window)
		ssl_manager_create();

	manage_window_set_transient(GTK_WINDOW(manager.window));
	gtk_widget_grab_focus(manager.ok_btn);

	ssl_manager_load_certs();

	gtk_widget_show(manager.window);

}

void ssl_manager_create(void) 
{
	GtkWidget *window;
	GtkWidget *hbox1;
	GtkWidget *vbox1;
	GtkWidget *certlist;
	GtkWidget *view_btn;
	GtkWidget *delete_btn;
	GtkWidget *ok_btn;
	gchar *titles[2];

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW(window),
			      _("Saved SSL Certificates"));
	gtk_container_set_border_width (GTK_CONTAINER (window), 8);
	gtk_window_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
	gtk_window_set_policy (GTK_WINDOW (window), FALSE, TRUE, FALSE);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(ssl_manager_ok_cb), NULL);
	MANAGE_WINDOW_SIGNALS_CONNECT (window);
	
	hbox1 = gtk_hbox_new(FALSE,2);
	vbox1 = gtk_vbox_new(FALSE,0);
	delete_btn = gtk_button_new_with_label(_("Delete"));
	g_signal_connect(G_OBJECT(delete_btn), "clicked",
			 G_CALLBACK(ssl_manager_delete_cb), NULL);
	view_btn = gtk_button_new_with_label(_("View"));
	g_signal_connect(G_OBJECT(view_btn), "clicked",
			 G_CALLBACK(ssl_manager_view_cb), NULL);
	ok_btn = gtk_button_new_with_label(_("OK"));
	g_signal_connect(G_OBJECT(ok_btn), "clicked",
			 G_CALLBACK(ssl_manager_ok_cb), NULL);
	gtk_widget_set_usize(ok_btn, 80, -1);
	gtk_widget_set_usize(delete_btn, 80, -1);
	gtk_widget_set_usize(view_btn, 80, -1);

	titles[0] = _("Server");
	titles[1] = _("Port");
	certlist = gtk_sctree_new_with_titles(2, 3, titles);
	gtk_clist_column_titles_show(GTK_CLIST(certlist));
	gtk_clist_set_column_width(GTK_CLIST(certlist), 0, 220);
	gtk_clist_set_selection_mode(GTK_CLIST(certlist), GTK_SELECTION_SINGLE);
	gtk_widget_set_usize(certlist, 300, 200);
	g_signal_connect(G_OBJECT(certlist), "open_row",
			 G_CALLBACK(ssl_manager_view_cb), NULL);
	gtk_box_pack_start(GTK_BOX(hbox1), certlist, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox1), vbox1, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox1), view_btn, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox1), delete_btn, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(vbox1), ok_btn, FALSE, FALSE, 0);
	
	gtk_widget_show(certlist);
	gtk_widget_show(hbox1);
	gtk_widget_show(vbox1);
	gtk_widget_show(ok_btn);
	gtk_widget_show(delete_btn);
	gtk_widget_show(view_btn);
	gtk_container_add(GTK_CONTAINER (window), hbox1);

	manager.window = window;
	manager.hbox1 = hbox1;
	manager.vbox1 = vbox1;
	manager.certlist = certlist;
	manager.view_btn = view_btn;
	manager.delete_btn = delete_btn;
	manager.ok_btn = ok_btn;

	gtk_widget_show(window);
		
}

static char *get_server(char *str)
{
	char *ret = NULL, *tmp = g_strdup(str);
	char *first_pos = NULL, *last_pos = NULL, *previous_pos = NULL;
	int previous_dot_pos;

	first_pos = tmp;
	while ((tmp = strstr(tmp,".")) != NULL) {
		*tmp++;
		previous_pos = last_pos;
		last_pos = tmp;
	}
	previous_dot_pos = (previous_pos - first_pos);
	if (previous_dot_pos - 1 > 0)
		ret = g_strndup(first_pos, previous_dot_pos - 1);
	else 
		ret = g_strdup(first_pos);
	g_free(first_pos);
	return ret;
}

static char *get_port(char *str)
{
	char *ret = NULL, *tmp = g_strdup(str);
	char *previous_pos = NULL, *last_pos = NULL;

	while ((tmp = strstr(tmp,".")) != NULL) {
		*tmp++;
		previous_pos = last_pos;
		last_pos = tmp;
	}
	if (last_pos && previous_pos && (int)(last_pos - previous_pos - 1) > 0)
		ret = g_strndup(previous_pos, (int)(last_pos - previous_pos - 1));
	else
		ret = g_strdup("0");
	g_free(tmp);
	return ret;
	
}
static void ssl_manager_load_certs (void) 
{
	DIR *dir;
	struct dirent *d;
	gchar *path;
	int row = 0;

	path = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, 
			  "certs", G_DIR_SEPARATOR_S, NULL);

	gtk_clist_clear(GTK_CLIST(manager.certlist));

	if((dir = opendir(path)) == NULL) {
		perror("opendir");
		return;
	}
	
	while ((d = readdir(dir)) != NULL) {
		gchar *server, *port, *text[2];
		SSLCertificate *cert;

		if(!strstr(d->d_name, ".cert")) 
			continue;

		server = get_server(d->d_name);
		port = get_port(d->d_name);
		
		text[0] = g_strdup(server);
		text[1] = g_strdup(port);
		gtk_clist_append(GTK_CLIST(manager.certlist), text);
		cert = ssl_certificate_find_lookup(server, atoi(port), FALSE);
		gtk_clist_set_row_data(GTK_CLIST(manager.certlist), row, cert);
		g_free(server);
		g_free(port);
		g_free(text[0]);
		g_free(text[1]);
		row++;
	}
	closedir(dir);
	g_free(path);
}

void ssl_manager_close(void) 
{
	gtk_widget_hide(manager.window);
}

static void ssl_manager_ok_cb(GtkWidget *widget, 
			      gpointer data) 
{
	ssl_manager_close();
}
static void ssl_manager_view_cb(GtkWidget *widget, 
			      gpointer data) 
{
	SSLCertificate *cert;
	GList *rowlist;
	
	rowlist = GTK_CLIST(manager.certlist)->selection;
	if (!rowlist) 
		return;
	
	cert = gtk_ctree_node_get_row_data
			(GTK_CTREE(manager.certlist),
			 GTK_CTREE_NODE(rowlist->data));
	
	if (!cert)
		return;

	sslcertwindow_show_cert(cert);
	
	
}
static void ssl_manager_delete_cb(GtkWidget *widget, 
			      gpointer data) 
{
	SSLCertificate *cert;
	GList *rowlist;
	int val;
	
	rowlist = GTK_CLIST(manager.certlist)->selection;
	if (!rowlist) 
		return;
	
	cert = gtk_ctree_node_get_row_data
			(GTK_CTREE(manager.certlist),
			 GTK_CTREE_NODE(rowlist->data));
	
	if (!cert)
		return;
	val = alertpanel(_("Delete certificate"), 
			     _("Do you really want to delete this certificate?"),
			     _("Yes"), _("+No"), NULL);
	if (val != G_ALERTDEFAULT)
		return;
	
	ssl_certificate_delete_from_disk(cert);
	ssl_certificate_destroy(cert);
	gtk_ctree_remove_node(GTK_CTREE(manager.certlist), GTK_CTREE_NODE(rowlist->data));
}
#endif
