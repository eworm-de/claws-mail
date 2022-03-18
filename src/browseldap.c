/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2003-2022 Match Grun and the Claws Mail team
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
 * Browse LDAP entry.
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

#include <pthread.h>
#include "gtkutils.h"
#include "stock_pixmap.h"
#include "prefs_common.h"
#include "browseldap.h"
#include "addritem.h"
#include "addrindex.h"
#include "manage_window.h"

#include "ldapquery.h"
#include "ldapserver.h"
#include "ldaplocate.h"

typedef enum {
	COL_NAME,
	COL_VALUE,
	N_COLS
} LDAPEntryColumnPos;

#define BROWSELDAP_WIDTH    450
#define BROWSELDAP_HEIGHT   420

#define COL_WIDTH_NAME      140
#define COL_WIDTH_VALUE     140

static struct _LDAPEntry_dlg {
	GtkWidget *window;
	GtkWidget *label_server;
	GtkWidget *label_address;
	GtkWidget *list_view;
	GtkWidget *close_btn;
} browseldap_dlg;

/**
 * Message queue.
 */
static GList *_displayQueue_ = NULL;

/**
 * Mutex to protect callback from multiple threads.
 */
static pthread_mutex_t _browseMutex_ = PTHREAD_MUTEX_INITIALIZER;

/**
 * Current query ID.
 */
static gint _queryID_ = 0;

/**
 * Completion idle ID.
 */
static guint _browseIdleID_ = 0;

/**
 * Search complete indicator.
 */
static gboolean _searchComplete_ = FALSE;

/**
 * Callback entry point for each LDAP entry processed. The background thread
 * (if any) appends the address list to the display queue.
 * 
 * \param qry        LDAP query object.
 * \param queryID    Query ID of search request.
 * \param listEMail  List of zero of more email objects that met search
 *                   criteria.
 * \param data       User data.
 */
static gint browse_callback_entry(
		LdapQuery *qry, gint queryID, GList *listValues, gpointer data )
{
	GList *node;
	NameValuePair *nvp;

	debug_print("browse_callback_entry...\n");
	pthread_mutex_lock( & _browseMutex_ );
	/* Append contents to end of display queue */
	node = listValues;
	while( node ) {
		nvp = ( NameValuePair * ) node->data;
		debug_print("adding to list: %s->%s\n",
				nvp->name?nvp->name:"null",
				nvp->value?nvp->value:"null");
		_displayQueue_ = g_list_append( _displayQueue_, nvp );
		node->data = NULL;
		node = g_list_next( node );
	}
	pthread_mutex_unlock( & _browseMutex_ );
	/* g_print( "browse_callback_entry...done\n" ); */

	return 0;
}

/**
 * Callback entry point for end of LDAP locate search.
 * 
 * \param qry     LDAP query object.
 * \param queryID Query ID of search request.
 * \param status  Status/error code.
 * \param data    User data.
 */
static gint browse_callback_end(
		LdapQuery *qry, gint queryID, gint status, gpointer data )
{
	debug_print("search completed\n");
	_searchComplete_ = TRUE;
	return 0;
}

/**
 * Clear the display queue.
 */
static void browse_clear_queue( void ) {
	/* Clear out display queue */
	pthread_mutex_lock( & _browseMutex_ );

	ldapqry_free_list_name_value( _displayQueue_ );
	g_list_free( _displayQueue_ );
	_displayQueue_ = NULL;

	pthread_mutex_unlock( & _browseMutex_ );
}

/**
 * Close window callback.
 * \param widget    Widget.
 * \param event     Event.
 * \param cancelled Cancelled flag.
 */
static gint browse_delete_event(
		GtkWidget *widget, GdkEventAny *event, gboolean *cancelled )
{
	gtk_main_quit();
	return TRUE;
}

/**
 * Respond to key press in window.
 * \param widget    Widget.
 * \param event     Event.
 * \param cancelled Cancelled flag.
 */
static void browse_key_pressed(
		GtkWidget *widget, GdkEventKey *event, gboolean *cancelled )
{
	if (event && event->keyval == GDK_KEY_Escape) {
		gtk_main_quit();
	}
}

/**
 * Callback to close window.
 * \param widget    Widget.
 * \param cancelled Cancelled flag.
 */
static void browse_close( GtkWidget *widget, gboolean *cancelled ) {
	gtk_main_quit();
}

/**
 * Create the window to display data.
 */
static void browse_create( void ) {
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *label_server;
	GtkWidget *label_addr;
	GtkWidget *vlbox;
	GtkWidget *tree_win;
	GtkWidget *hbbox;
	GtkWidget *close_btn;
	GtkWidget *content_area;
	GtkWidget *view;
	GtkListStore *store;
	GtkTreeSelection *sel;
	GtkCellRenderer *rdr;
	GtkTreeViewColumn *col;

	debug_print("creating browse widget\n");
	window = gtk_dialog_new();
	gtk_widget_set_size_request( window, BROWSELDAP_WIDTH, BROWSELDAP_HEIGHT );
	gtk_container_set_border_width( GTK_CONTAINER(window), 0 );
	gtk_window_set_title( GTK_WINDOW(window), _("Browse Directory Entry") );
	gtk_window_set_position( GTK_WINDOW(window), GTK_WIN_POS_MOUSE );
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(browse_delete_event), NULL);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(browse_key_pressed), NULL);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	content_area = gtk_dialog_get_content_area(GTK_DIALOG(window));
	gtk_box_pack_start(GTK_BOX(content_area), vbox, TRUE, TRUE, 0);
	gtk_container_set_border_width( GTK_CONTAINER(vbox), 8 );

	table = gtk_grid_new();
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
	gtk_container_set_border_width( GTK_CONTAINER(table), 8 );
	gtk_grid_set_row_spacing(GTK_GRID(table), 8);
	gtk_grid_set_column_spacing(GTK_GRID(table), 8);

	/* First row */
	label = gtk_label_new(_("Server Name:"));
	gtk_grid_attach(GTK_GRID(table), label, 0, 0, 1, 1);
	gtk_label_set_xalign(GTK_LABEL(label), 1.0);

	label_server = gtk_label_new("");
	gtk_label_set_xalign(GTK_LABEL(label_server), 0.0);
	gtk_grid_attach(GTK_GRID(table), label, 1, 0, 1, 1);
	gtk_widget_set_hexpand(label_server, TRUE);
	gtk_widget_set_halign(label_server, GTK_ALIGN_FILL);

	/* Second row */
	label = gtk_label_new(_("Distinguished Name (dn):"));
	gtk_label_set_xalign(GTK_LABEL(label), 1.0);
	gtk_grid_attach(GTK_GRID(table), label, 0, 1, 1, 1);

	label_addr = gtk_label_new("");
	gtk_label_set_xalign(GTK_LABEL(label_addr), 0.0);
	gtk_grid_attach(GTK_GRID(table), label_addr, 1, 1, 1, 1);
	gtk_widget_set_hexpand(label_addr, TRUE);
	gtk_widget_set_halign(label_addr, GTK_ALIGN_FILL);

	/* Address book/folder tree */
	vlbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_box_pack_start(GTK_BOX(vbox), vlbox, TRUE, TRUE, 0);
	gtk_container_set_border_width( GTK_CONTAINER(vlbox), 8 );

	tree_win = gtk_scrolled_window_new( NULL, NULL );
	gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(tree_win),
				        GTK_POLICY_AUTOMATIC,
				        GTK_POLICY_AUTOMATIC );
	gtk_box_pack_start( GTK_BOX(vlbox), tree_win, TRUE, TRUE, 0 );

	store = gtk_list_store_new(N_COLS,
			G_TYPE_STRING, G_TYPE_STRING,
			-1);

	view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	g_object_unref(store);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), TRUE);
	gtk_tree_view_set_reorderable(GTK_TREE_VIEW(view), FALSE);
	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
	gtk_tree_selection_set_mode(sel, GTK_SELECTION_NONE);

	rdr = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes(_("LDAP Name"), rdr,
			"markup", COL_NAME, NULL);
	gtk_tree_view_column_set_min_width(col, COL_WIDTH_NAME);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);

	rdr = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes(_("Attribute Value"), rdr,
			"markup", COL_VALUE, NULL);
	gtk_tree_view_column_set_min_width(col, COL_WIDTH_VALUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);

	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store),
			COL_NAME, GTK_SORT_ASCENDING);

	gtk_container_add( GTK_CONTAINER(tree_win), view );

	/* Button panel */
	gtkut_stock_button_set_create(&hbbox, &close_btn, "window-close", _("_Close"),
				      NULL, NULL, NULL, NULL, NULL, NULL);
	gtk_box_pack_end(GTK_BOX(vbox), hbbox, FALSE, FALSE, 0);
	gtk_container_set_border_width( GTK_CONTAINER(hbbox), 0 );

	g_signal_connect(G_OBJECT(close_btn), "clicked",
			 G_CALLBACK(browse_close), NULL);
	gtk_widget_grab_default(close_btn);

	gtk_widget_show_all(vbox);

	browseldap_dlg.window        = window;
	browseldap_dlg.label_server  = label_server;
	browseldap_dlg.label_address = label_addr;
	browseldap_dlg.list_view     = view;
	browseldap_dlg.close_btn     = close_btn;

	gtk_widget_show_all( window );

}

/**
 * Idler function. This function is called by the main (UI) thread during UI
 * idle time while an address search is in progress. Items from the display
 * queue are processed and appended to the address list.
 *
 * \param data Target data object.
 * \return <i>TRUE</i> to ensure that idle event do not get ignored.
 */
static gboolean browse_idle( gpointer data ) {
	GList *node;
	NameValuePair *nvp;
	GtkWidget *view = browseldap_dlg.list_view;
	GtkListStore *store = GTK_LIST_STORE(
			gtk_tree_view_get_model(GTK_TREE_VIEW(view)));
	GtkTreeIter iter;

	/* Process all entries in display queue */
	pthread_mutex_lock( & _browseMutex_ );
	if( _displayQueue_ ) {
		node = _displayQueue_;
		while( node ) {
			/* Add entry into list */
			nvp = ( NameValuePair * ) node->data;
			debug_print("Adding row to list: %s->%s\n",
						nvp->name?nvp->name:"null",
						nvp->value?nvp->value:"null");
			gtk_list_store_append(store, &iter);
			gtk_list_store_set(store, &iter,
					COL_NAME, nvp->name,
					COL_VALUE, nvp->value,
					-1);

			/* Free up entry */
			ldapqry_free_name_value( nvp );
			node->data = NULL;
			node = g_list_next( node );
		}
		g_list_free( _displayQueue_ );
		_displayQueue_ = NULL;
	}
	pthread_mutex_unlock( & _browseMutex_ );

	if( _searchComplete_ ) {
		/* Remove idler */
		if( _browseIdleID_ != 0 ) {
			g_source_remove( _browseIdleID_ );
			_browseIdleID_ = 0;
		}
	}

	return TRUE;
}

/**
 * Main entry point to browse LDAP entries.
 * \param  ds Data source to process.
 * \param  dn Distinguished name to retrieve.
 * \return <code>TRUE</code>
 */
gboolean browseldap_entry( AddressDataSource *ds, const gchar *dn ) {
	LdapServer *server;
	GtkWidget *view;
	GtkListStore *store;

	_queryID_ = 0;
	_browseIdleID_ = 0;

	server = ds->rawDataSource;

	if( ! browseldap_dlg.window ) browse_create();
	gtk_widget_grab_focus(browseldap_dlg.close_btn);
	gtk_widget_show(browseldap_dlg.window);
	manage_window_set_transient(GTK_WINDOW(browseldap_dlg.window));
	gtk_window_set_modal(GTK_WINDOW(browseldap_dlg.window), TRUE);
	gtk_widget_show(browseldap_dlg.window);

	gtk_label_set_text( GTK_LABEL(browseldap_dlg.label_address ), "" );
	if( dn ) {
		gtk_label_set_text(
			GTK_LABEL(browseldap_dlg.label_address ), dn );
	}
	gtk_label_set_text(
		GTK_LABEL(browseldap_dlg.label_server ),
		ldapsvr_get_name( server ) );

	debug_print("browsing server: %s\n", ldapsvr_get_name(server));
	/* Setup search */
	_searchComplete_ = FALSE;
	_queryID_ = ldaplocate_search_setup(
			server, dn, browse_callback_entry, browse_callback_end );
	debug_print("query id: %d\n", _queryID_);
	_browseIdleID_ = g_idle_add( (GSourceFunc) browse_idle, NULL );

	/* Start search */
	debug_print("starting search\n");
	ldaplocate_search_start( _queryID_ );

	/* Display dialog */
	gtk_main();
	gtk_widget_hide( browseldap_dlg.window );
	gtk_window_set_modal(GTK_WINDOW(browseldap_dlg.window), FALSE);
	/* Stop query */
	debug_print("stopping search\n");
	ldaplocate_search_stop( _queryID_ );

	if( _browseIdleID_ != 0 ) {
		g_source_remove( _browseIdleID_ );
		_browseIdleID_ = 0;
	}
	browse_clear_queue();

	view = browseldap_dlg.list_view;
	store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(view)));
	gtk_list_store_clear(store);

	return TRUE;
}

#endif /* USE_LDAP */

/*
* End of Source.
*/

