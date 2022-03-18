/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2001-2022 the Claws Mail team and Match Grun
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

/*
 * LDAP Base DN selection dialog.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#ifdef USE_LDAP

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "prefs_common.h"
#include "ldaputil.h"
#include "mgutils.h"
#include "gtkutils.h"
#include "manage_window.h"

static struct _LDAPEdit_basedn {
	GtkWidget *window;
	GtkWidget *host_label;
	GtkWidget *port_label;
	GtkWidget *basedn_entry;
	GtkWidget *basedn_list;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;
	GtkWidget *statusbar;
	gint status_cid;
} ldapedit_basedn;

static gboolean ldapedit_basedn_cancelled;
static gboolean ldapedit_basedn_bad_server;

/*
* Edit functions.
*/
static void edit_ldap_bdn_status_show( gchar *msg ) {
	if( ldapedit_basedn.statusbar != NULL ) {
		gtk_statusbar_pop( GTK_STATUSBAR(ldapedit_basedn.statusbar), ldapedit_basedn.status_cid );
		if( msg ) {
			gtk_statusbar_push( GTK_STATUSBAR(ldapedit_basedn.statusbar), ldapedit_basedn.status_cid, msg );
		}
	}
}

static gint edit_ldap_bdn_delete_event( GtkWidget *widget, GdkEventAny *event, gboolean *cancelled ) {
	ldapedit_basedn_cancelled = TRUE;
	gtk_main_quit();
	return TRUE;
}

static gboolean edit_ldap_bdn_key_pressed( GtkWidget *widget, GdkEventKey *event, gboolean *cancelled ) {
	if (event && event->keyval == GDK_KEY_Escape) {
		ldapedit_basedn_cancelled = TRUE;
		gtk_main_quit();
	}
	return FALSE;
}

static void edit_ldap_bdn_ok( GtkWidget *widget, gboolean *cancelled ) {
	ldapedit_basedn_cancelled = FALSE;
	gtk_main_quit();
}

static void edit_ldap_bdn_cancel( GtkWidget *widget, gboolean *cancelled ) {
	ldapedit_basedn_cancelled = TRUE;
	gtk_main_quit();
}

static void set_selected()
{
	GtkWidget *entry = ldapedit_basedn.basedn_entry;
	GtkWidget *view = ldapedit_basedn.basedn_list;
	gchar *text;

	if (entry == NULL || view == NULL)
		return;

	text = gtkut_tree_view_get_selected_pointer(
			GTK_TREE_VIEW(view), 0, NULL, NULL, NULL);

	if (text == NULL)
		return;

	gtk_entry_set_text(GTK_ENTRY(entry), text);
	g_free(text);
}

static void basedn_list_cursor_changed(GtkTreeView *view,
		gpointer user_data)
{
	set_selected();
}

static void basedn_list_row_activated(GtkTreeView *view,
		GtkTreePath *path,
		GtkTreeViewColumn *column,
		gpointer user_data)
{
	set_selected();
	edit_ldap_bdn_ok(NULL, NULL);
}

static void edit_ldap_bdn_create(void) {
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *host_label;
	GtkWidget *port_label;
	GtkWidget *basedn_list;
	GtkWidget *vlbox;
	GtkWidget *lwindow;
	GtkWidget *basedn_entry;
	GtkWidget *hbbox;
	GtkWidget *hsep;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;
	GtkWidget *hsbox;
	GtkWidget *statusbar;
	GtkListStore *store;
	GtkTreeSelection *sel;
	GtkTreeViewColumn *col;
	GtkCellRenderer *rdr;

	window = gtkut_window_new(GTK_WINDOW_TOPLEVEL, "editldap_basedn");
	gtk_container_set_border_width(GTK_CONTAINER(window), 0);
	gtk_window_set_title(GTK_WINDOW(window), _("Edit LDAP - Select Search Base"));
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DIALOG);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(edit_ldap_bdn_delete_event), NULL );
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(edit_ldap_bdn_key_pressed), NULL );

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_container_add(GTK_CONTAINER(window), vbox);
	gtk_container_set_border_width( GTK_CONTAINER(vbox), 0 );

	table = gtk_grid_new();
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
	gtk_container_set_border_width( GTK_CONTAINER(table), 8 );
	gtk_grid_set_row_spacing(GTK_GRID(table), 8);
	gtk_grid_set_column_spacing(GTK_GRID(table), 8);

	/* First row */
	label = gtk_label_new(_("Hostname"));
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_grid_attach(GTK_GRID(table), label, 0, 0, 1, 1);

	host_label = gtk_label_new("");
	gtk_label_set_xalign(GTK_LABEL(host_label), 0.0);
	gtk_grid_attach(GTK_GRID(table), host_label, 1, 0, 1, 1);

	/* Second row */
	label = gtk_label_new(_("Port"));
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_grid_attach(GTK_GRID(table), label, 0, 1, 1, 1);

	port_label = gtk_label_new("");
	gtk_label_set_xalign(GTK_LABEL(port_label), 0.0);
	gtk_grid_attach(GTK_GRID(table), label, 1, 1, 1, 1);

	/* Third row */
	label = gtk_label_new(_("Search Base"));
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_grid_attach(GTK_GRID(table), label, 0, 2, 1, 1);

	basedn_entry = gtk_entry_new();
	gtk_grid_attach(GTK_GRID(table), basedn_entry, 1, 2, 1, 1);
	gtk_widget_set_hexpand(basedn_entry, TRUE);
	gtk_widget_set_halign(basedn_entry, GTK_ALIGN_FILL);

	/* Basedn list */
	vlbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_box_pack_start(GTK_BOX(vbox), vlbox, TRUE, TRUE, 0);
	gtk_container_set_border_width( GTK_CONTAINER(vlbox), 8 );

	lwindow = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(lwindow),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(vlbox), lwindow, TRUE, TRUE, 0);

	store = gtk_list_store_new(1, G_TYPE_STRING, -1);

	basedn_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	g_object_unref(store);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(basedn_list), TRUE);
	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(basedn_list));
	gtk_tree_selection_set_mode(sel, GTK_SELECTION_BROWSE);

	rdr = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes(
			_("Available Search Base(s)"), rdr,
			"markup", 0, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(basedn_list), col);

	gtk_container_add(GTK_CONTAINER(lwindow), basedn_list);

	g_signal_connect(G_OBJECT(basedn_list), "cursor-changed",
			G_CALLBACK(basedn_list_cursor_changed), NULL);
	g_signal_connect(G_OBJECT(basedn_list), "row-activated",
			G_CALLBACK(basedn_list_row_activated), NULL);


	/* Status line */
	hsbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_end(GTK_BOX(vbox), hsbox, FALSE, FALSE, BORDER_WIDTH);
	statusbar = gtk_statusbar_new();
	gtk_box_pack_start(GTK_BOX(hsbox), statusbar, TRUE, TRUE, BORDER_WIDTH);

	/* Button panel */
	gtkut_stock_button_set_create(&hbbox, &cancel_btn, NULL, _("_Cancel"),
				      &ok_btn, NULL, _("_OK"),
				      NULL, NULL, NULL);
	gtk_box_pack_end(GTK_BOX(vbox), hbbox, FALSE, FALSE, 0);
	gtk_container_set_border_width( GTK_CONTAINER(hbbox), 0 );
	gtk_widget_grab_default(ok_btn);

	hsep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_box_pack_end(GTK_BOX(vbox), hsep, FALSE, FALSE, 0);

	g_signal_connect(G_OBJECT(ok_btn), "clicked",
			 G_CALLBACK(edit_ldap_bdn_ok), NULL);
	g_signal_connect(G_OBJECT(cancel_btn), "clicked",
			 G_CALLBACK(edit_ldap_bdn_cancel), NULL);

	gtk_widget_show_all(vbox);

	ldapedit_basedn.window     = window;
	ldapedit_basedn.host_label = host_label;
	ldapedit_basedn.port_label = port_label;
	ldapedit_basedn.basedn_entry = basedn_entry;
	ldapedit_basedn.basedn_list  = basedn_list;
	ldapedit_basedn.ok_btn     = ok_btn;
	ldapedit_basedn.cancel_btn = cancel_btn;
	ldapedit_basedn.statusbar  = statusbar;
	ldapedit_basedn.status_cid =
		gtk_statusbar_get_context_id(
			GTK_STATUSBAR(statusbar), "Edit LDAP Select Base DN" );
}

static void edit_ldap_bdn_load_data(
	const gchar *hostName, const gint iPort, const gint tov,
	const gchar* bindDN, const gchar *bindPW, int ssl, int tls )
{
	gchar *sHost;
	gchar *sMsg = NULL;
	gchar sPort[20];
	gboolean flgConn;
	gboolean flgDN;
	GList *baseDN = NULL;
	GtkWidget *view = ldapedit_basedn.basedn_list;
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
	GtkTreeIter iter;
	GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));

	edit_ldap_bdn_status_show( "" );

	gtk_list_store_clear(GTK_LIST_STORE(model));

	ldapedit_basedn_bad_server = TRUE;
	flgConn = flgDN = FALSE;
	sHost = g_strdup( hostName );
	sprintf( sPort, "%d", iPort );
	gtk_label_set_text(GTK_LABEL(ldapedit_basedn.host_label), hostName);
	gtk_label_set_text(GTK_LABEL(ldapedit_basedn.port_label), sPort);
	if( *sHost != '\0' ) {
		/* Test connection to server */
		if( ldaputil_test_connect( sHost, iPort, ssl, tls, tov ) ) {
			/* Attempt to read base DN */
			baseDN = ldaputil_read_basedn( sHost, iPort, bindDN, bindPW, tov, ssl, tls );
			if( baseDN ) {
				GList *node = baseDN;

				while( node ) {
					gtk_list_store_append(GTK_LIST_STORE(model), &iter);
					gtk_list_store_set(GTK_LIST_STORE(model), &iter,
							0, (gchar *)node->data, -1);
					node = g_list_next( node );
					flgDN = TRUE;
				}

				/* Select the first line */
				if (gtk_tree_model_get_iter_first(model, &iter))
					gtk_tree_selection_select_iter(sel, &iter);

				g_list_free_full( baseDN, g_free );
				baseDN = node = NULL;
			}
			ldapedit_basedn_bad_server = FALSE;
			flgConn = TRUE;
		}
	}
	g_free( sHost );

	/* Display appropriate message */
	if( flgConn ) {
		if( ! flgDN ) {
			sMsg = _( "Could not read Search Base(s) from server - please set manually" );
		}
	}
	else {
		sMsg = _( "Could not connect to server" );
	}
	edit_ldap_bdn_status_show( sMsg );
}

gchar *edit_ldap_basedn_selection( const gchar *hostName, const gint port, gchar *baseDN, const gint tov,
	       const gchar* bindDN, const gchar *bindPW, int ssl, int tls ) {
	gchar *retVal = NULL;

	ldapedit_basedn_cancelled = FALSE;
	if( ! ldapedit_basedn.window ) edit_ldap_bdn_create();
	gtk_widget_grab_focus(ldapedit_basedn.ok_btn);
	gtk_widget_show(ldapedit_basedn.window);
	manage_window_set_transient(GTK_WINDOW(ldapedit_basedn.window));
	gtk_window_set_modal(GTK_WINDOW(ldapedit_basedn.window), TRUE);
	edit_ldap_bdn_status_show( "" );
	edit_ldap_bdn_load_data( hostName, port, tov, bindDN, bindPW, ssl, tls );
	gtk_widget_show(ldapedit_basedn.window);

	gtk_entry_set_text(GTK_ENTRY(ldapedit_basedn.basedn_entry), baseDN);

	gtk_main();
	gtk_widget_hide(ldapedit_basedn.window);
	gtk_window_set_modal(GTK_WINDOW(ldapedit_basedn.window), FALSE);
	if( ldapedit_basedn_cancelled ) return NULL;
	if( ldapedit_basedn_bad_server ) return NULL;

	retVal = gtk_editable_get_chars( GTK_EDITABLE(ldapedit_basedn.basedn_entry), 0, -1 );
	g_strstrip( retVal );
	if( *retVal == '\0' ) {
		g_free( retVal );
		retVal = NULL;
	}
	return retVal;
}

#endif /* USE_LDAP */

/*
* End of Source.
*/

