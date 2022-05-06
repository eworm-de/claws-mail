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
 * 
 */

/*
 * Add address to address book dialog.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "gtkutils.h"
#include "stock_pixmap.h"
#include "prefs_common.h"
#include "addressadd.h"
#include "addritem.h"
#include "addrbook.h"
#include "addrindex.h"
#include "manage_window.h"

enum {
	COL_ICON,
	COL_NAME,
	COL_PTR,
	N_COLS
};

typedef struct {
	AddressBookFile	*book;
	ItemFolder	*folder;
} FolderInfo;

static struct _AddressBookFolderSel_dlg {
	GtkWidget *window;
	GtkWidget *view_folder;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;
	gint status_cid;
	FolderInfo *fiSelected;
} addressbook_foldersel_dlg;

static GdkPixbuf *folderXpm;
static GdkPixbuf *bookXpm;

static gboolean addressbook_foldersel_cancelled;

static FolderInfo *addressbook_foldersel_create_folderinfo( AddressBookFile *abf, ItemFolder *folder )
{
	FolderInfo *fi = g_new0( FolderInfo, 1 );
	fi->book   = abf;
	fi->folder = folder;
	return fi;
}

static void addressbook_foldersel_free_folderinfo( FolderInfo *fi ) {
	fi->book   = NULL;
	fi->folder = NULL;
	g_free( fi );
}

static gint addressbook_foldersel_delete_event( GtkWidget *widget, GdkEventAny *event, gboolean *cancelled )
{
	addressbook_foldersel_cancelled = TRUE;
	gtk_main_quit();
	return TRUE;
}

static gboolean addressbook_foldersel_key_pressed( GtkWidget *widget, GdkEventKey *event, gboolean *cancelled )
{
	if ( event && event->keyval == GDK_KEY_Escape ) {
		addressbook_foldersel_cancelled = TRUE;
		gtk_main_quit();
	}
	return FALSE;
}

static void set_selected_ptr()
{
	GtkWidget *view = addressbook_foldersel_dlg.view_folder;

	addressbook_foldersel_dlg.fiSelected =
		gtkut_tree_view_get_selected_pointer(GTK_TREE_VIEW(view), COL_PTR,
				NULL, NULL, NULL);
}

static void addressbook_foldersel_ok( GtkWidget *widget, gboolean *cancelled )
{
	set_selected_ptr();
	addressbook_foldersel_cancelled = FALSE;
	gtk_main_quit();
}

static void addressbook_foldersel_cancel( GtkWidget *widget, gboolean *cancelled )
{
	set_selected_ptr();
	addressbook_foldersel_cancelled = TRUE;
	gtk_main_quit();
}

static void addressbook_foldersel_row_activated(GtkTreeView *view,
		GtkTreePath *path, GtkTreeViewColumn *col,
		gpointer user_data)
{
	addressbook_foldersel_ok(NULL, NULL);
}

static void addressbook_foldersel_size_allocate_cb(GtkWidget *widget,
					 GtkAllocation *allocation)
{
	cm_return_if_fail(allocation != NULL);

	gtk_window_get_size(GTK_WINDOW(widget),
		&prefs_common.addressbook_folderselwin_width, &prefs_common.addressbook_folderselwin_height);
}

static void addressbook_foldersel_create( void )
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *view;
	GtkWidget *vlbox;
	GtkWidget *tree_win;
	GtkWidget *hbbox;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;
	static GdkGeometry geometry;
	GtkTreeStore *store;
	GtkTreeSelection *sel;
	GtkCellRenderer *rdr;
	GtkTreeViewColumn *col;

	window = gtkut_window_new(GTK_WINDOW_TOPLEVEL, "addressbook_foldersel" );
	gtk_container_set_border_width( GTK_CONTAINER(window), 0 );
	gtk_window_set_title( GTK_WINDOW(window), _("Select Address Book Folder") );
	gtk_window_set_position( GTK_WINDOW(window), GTK_WIN_POS_MOUSE );
	gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DIALOG);
	g_signal_connect( G_OBJECT(window), "delete_event",
			  G_CALLBACK(addressbook_foldersel_delete_event), NULL );
	g_signal_connect( G_OBJECT(window), "key_press_event",
			  G_CALLBACK(addressbook_foldersel_key_pressed), NULL );
	g_signal_connect(G_OBJECT(window), "size_allocate",
			 G_CALLBACK(addressbook_foldersel_size_allocate_cb), NULL);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_container_add(GTK_CONTAINER(window), vbox);
	gtk_container_set_border_width( GTK_CONTAINER(vbox), 8 );

	/* Address book/folder tree */
	vlbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_box_pack_start(GTK_BOX(vbox), vlbox, TRUE, TRUE, 0);
	gtk_container_set_border_width( GTK_CONTAINER(vlbox), 8 );

	tree_win = gtk_scrolled_window_new( NULL, NULL );
	gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(tree_win),
				        GTK_POLICY_AUTOMATIC,
				        GTK_POLICY_AUTOMATIC );
	gtk_box_pack_start( GTK_BOX(vlbox), tree_win, TRUE, TRUE, 0 );

	store = gtk_tree_store_new(N_COLS,
			GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_POINTER);

	view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store), COL_NAME, GTK_SORT_ASCENDING);
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(view), prefs_common.use_stripes_everywhere);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), TRUE);
	gtk_tree_view_set_enable_tree_lines(GTK_TREE_VIEW(view), FALSE);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(view), COL_NAME);

	col = gtk_tree_view_column_new();
	rdr = gtk_cell_renderer_pixbuf_new();
	gtk_cell_renderer_set_padding(rdr, 0, 0);
	gtk_tree_view_column_pack_start(col, rdr, FALSE);
	gtk_tree_view_column_set_attributes(col, rdr,
			"pixbuf", COL_ICON, NULL);
	rdr = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(col, rdr, TRUE);
	gtk_tree_view_column_set_attributes(col, rdr,
			"markup", COL_NAME, NULL);
	gtk_tree_view_column_set_title(col, _("Address Book"));
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
	gtk_tree_selection_set_mode(sel, GTK_SELECTION_BROWSE);

	gtk_container_add( GTK_CONTAINER(tree_win), view );

	/* Button panel */
	gtkut_stock_button_set_create( &hbbox, &cancel_btn, NULL, _("_Cancel"),
				      &ok_btn, NULL, _("_OK"),
				      NULL, NULL, NULL );
	gtk_box_pack_end( GTK_BOX(vbox), hbbox, FALSE, FALSE, 0 );
	gtk_container_set_border_width( GTK_CONTAINER(hbbox), 0 );
	gtk_widget_grab_default( ok_btn );

	g_signal_connect( G_OBJECT(view), "row-activated",
			G_CALLBACK(addressbook_foldersel_row_activated), NULL);
	g_signal_connect( G_OBJECT(ok_btn), "clicked",
			 G_CALLBACK(addressbook_foldersel_ok), NULL );
	g_signal_connect( G_OBJECT(cancel_btn), "clicked",
			 G_CALLBACK(addressbook_foldersel_cancel), NULL );

	if ( !geometry.min_height ) {
		geometry.min_width = 300;
		geometry.min_height = 350;
	}

	gtk_window_set_geometry_hints( GTK_WINDOW(window), NULL, &geometry,
				      GDK_HINT_MIN_SIZE );
	gtk_window_set_default_size( GTK_WINDOW(window),
				    prefs_common.addressbook_folderselwin_width,
				    prefs_common.addressbook_folderselwin_height );

	gtk_widget_show_all( vbox );

	addressbook_foldersel_dlg.window      = window;
	addressbook_foldersel_dlg.view_folder = view;
	addressbook_foldersel_dlg.ok_btn      = ok_btn;
	addressbook_foldersel_dlg.cancel_btn  = cancel_btn;

	gtk_widget_show_all( window );

	stock_pixbuf_gdk(STOCK_PIXMAP_BOOK, &bookXpm);
	stock_pixbuf_gdk(STOCK_PIXMAP_DIR_OPEN, &folderXpm);
}

static gboolean tree_clear_foreach_func(GtkTreeModel *model,
		GtkTreePath *path, GtkTreeIter *iter,
		gpointer user_data)
{
	FolderInfo *fi;

	gtk_tree_model_get(model, iter, COL_PTR, &fi, -1);
	if (fi != NULL) {
		addressbook_foldersel_free_folderinfo(fi);
	}
	gtk_tree_store_set(GTK_TREE_STORE(model), iter, COL_PTR, NULL, -1);
	return FALSE;
}

static void addressbook_foldersel_tree_clear()
{
	GtkWidget *view = addressbook_foldersel_dlg.view_folder;
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));

	gtk_tree_model_foreach(model, tree_clear_foreach_func, NULL);
	gtk_tree_store_clear(GTK_TREE_STORE(model));
}

static void addressbook_foldersel_load_folder( GtkTreeIter *parent_iter,
		ItemFolder *parentFolder, FolderInfo *fiParent)
{
	GtkWidget *view = addressbook_foldersel_dlg.view_folder;
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
	GtkTreeIter iter;
	GList *list;
	ItemFolder *folder;
	gchar *name;
	FolderInfo *fi;

	list = parentFolder->listFolder;
	while ( list ) {
		folder = list->data;
		name = g_strdup( ADDRITEM_NAME(folder) );

		fi = addressbook_foldersel_create_folderinfo( fiParent->book, folder );

		debug_print("adding folder '%s'\n", name);
		gtk_tree_store_append(GTK_TREE_STORE(model), &iter, parent_iter);
		gtk_tree_store_set(GTK_TREE_STORE(model), &iter,
				COL_ICON, folderXpm,
				COL_NAME, name,
				COL_PTR, fi,
				-1);
		g_free(name);

		addressbook_foldersel_load_folder( parent_iter, folder, fi );
		list = g_list_next( list );
	}
}

static void addressbook_foldersel_load_data( AddressIndex *addrIndex )
{
	AddressDataSource *ds;
	GList *list, *nodeDS;
	gchar *name;
	ItemFolder *rootFolder;
	AddressBookFile *abf;
	FolderInfo *fi;
	GtkWidget *view = addressbook_foldersel_dlg.view_folder;
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
	GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
	GtkTreeIter iter;

	addressbook_foldersel_tree_clear();

	list = addrindex_get_interface_list( addrIndex );
	while ( list ) {
		AddressInterface *interface = list->data;
		if ( interface->type == ADDR_IF_BOOK ) {
			nodeDS = interface->listSource;
			while ( nodeDS ) {
				ds = nodeDS->data;
				name = g_strdup( addrindex_ds_get_name( ds ) );

				/* Read address book */
				if( ! addrindex_ds_get_read_flag( ds ) ) {
					addrindex_ds_read_data( ds );
				}

				/* Add node for address book */
				abf = ds->rawDataSource;
				fi = addressbook_foldersel_create_folderinfo( abf, NULL );

				debug_print("adding AB '%s'\n", name);
				gtk_tree_store_append(GTK_TREE_STORE(model), &iter, NULL);
				gtk_tree_store_set(GTK_TREE_STORE(model), &iter,
						COL_ICON, bookXpm,
						COL_NAME, name,
						COL_PTR, fi,
						-1);
				g_free(name);

				rootFolder = addrindex_ds_get_root_folder( ds );
				addressbook_foldersel_load_folder( &iter, rootFolder, fi );

				nodeDS = g_list_next( nodeDS );
			}
		}
		list = g_list_next( list );
	}

	if (gtk_tree_model_get_iter_first(model, &iter))
		gtk_tree_selection_select_iter(sel, &iter);
}

gboolean addressbook_foldersel_selection( AddressIndex *addrIndex,
					AddressBookFile **book, ItemFolder **folder, 
					const gchar* path)
{
	gboolean retVal = FALSE;
	addressbook_foldersel_cancelled = FALSE;

	if ( ! addressbook_foldersel_dlg.window )
		addressbook_foldersel_create();
	gtk_widget_grab_focus(addressbook_foldersel_dlg.ok_btn);
	gtk_widget_show(addressbook_foldersel_dlg.window);
	manage_window_set_transient(GTK_WINDOW(addressbook_foldersel_dlg.window));
	gtk_window_set_modal(GTK_WINDOW(addressbook_foldersel_dlg.window), TRUE);
	
	addressbook_foldersel_dlg.fiSelected = NULL;

	addressbook_foldersel_load_data( addrIndex );

	gtk_widget_show(addressbook_foldersel_dlg.window);

	gtk_main();
	gtk_widget_hide( addressbook_foldersel_dlg.window );
	gtk_window_set_modal(GTK_WINDOW(addressbook_foldersel_dlg.window), FALSE);
	if ( ! addressbook_foldersel_cancelled ) {

		*book = NULL;
		*folder = NULL;

		if ( addressbook_foldersel_dlg.fiSelected ) {
			*book = addressbook_foldersel_dlg.fiSelected->book;
			*folder = addressbook_foldersel_dlg.fiSelected->folder;
			retVal = TRUE;
		}
	}

	addressbook_foldersel_tree_clear();

	return retVal;
}

/*
* End of Source.
*/
