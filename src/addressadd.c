/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2001-2022 Match Grun and the Claws Mail team
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
#include "prefs_gtk.h"
#include "addressadd.h"
#ifndef USE_ALT_ADDRBOOK
	#include "addritem.h"
	#include "addrbook.h"
	#include "addrindex.h"
	#include "ldapserver.h"
	#include "ldapupdate.h"
#else
	#include "addressbook-dbus.h"
#endif
#include "manage_window.h"
#include "alertpanel.h"

#ifndef USE_ALT_ADDRBOOK
typedef struct {
	AddressBookFile	*book;
	ItemFolder	*folder;
} FolderInfo;
#else
typedef struct {
    gchar* book;
} FolderInfo;
#endif

static struct _AddressAdd_dlg {
	GtkWidget *window;
	GtkWidget *picture;
	GtkWidget *entry_name;
	GtkWidget *label_address;
	GtkWidget *entry_remarks;
	GtkWidget *tree_folder;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;
	FolderInfo *fiSelected;
} addressadd_dlg;

static GdkPixbuf *folderXpm;
static GdkPixbuf *bookXpm;

static gboolean addressadd_cancelled;

enum {
	ADDRADD_COL_ICON,
	ADDRADD_COL_NAME,
	ADDRADD_COL_PTR,
	N_ADDRADD_COLS
};

#ifndef USE_ALT_ADDRBOOK
static FolderInfo *addressadd_create_folderinfo( AddressBookFile *abf, ItemFolder *folder )
{
	FolderInfo *fi = g_new0( FolderInfo, 1 );
	fi->book   = abf;
	fi->folder = folder;
	return fi;
}
#else
static FolderInfo *addressadd_create_folderinfo(gchar* book) {
	FolderInfo *fi = g_new0( FolderInfo, 1 );
	fi->book = book;
	return fi;
}
#endif

#ifndef USE_ALT_ADDRBOOK
static void addressadd_free_folderinfo( FolderInfo *fi ) {
	fi->book   = NULL;
	fi->folder = NULL;
	g_free( fi );
}
#else
static void addressadd_free_folderinfo( FolderInfo *fi ) {
	fi->book   = NULL;
	g_free( fi );
}
#endif

static gint addressadd_delete_event( GtkWidget *widget, GdkEventAny *event, gboolean *cancelled ) {
	addressadd_cancelled = TRUE;
	gtk_main_quit();
	return TRUE;
}

static gboolean addressadd_key_pressed( GtkWidget *widget, GdkEventKey *event, gboolean *cancelled ) {
	if (event && event->keyval == GDK_KEY_Escape) {
		addressadd_cancelled = TRUE;
		gtk_main_quit();
	}
	return FALSE;
}

/* Points addressadd_dlg.fiSelected to the selected item */
static void set_selected_ptr()
{
	addressadd_dlg.fiSelected = gtkut_tree_view_get_selected_pointer(
			GTK_TREE_VIEW(addressadd_dlg.tree_folder), ADDRADD_COL_PTR,
			NULL, NULL, NULL);
}

static void addressadd_ok( GtkWidget *widget, gboolean *cancelled ) {
	set_selected_ptr();
	addressadd_cancelled = FALSE;
	gtk_main_quit();
}

static void addressadd_cancel( GtkWidget *widget, gboolean *cancelled ) {
	set_selected_ptr();
	addressadd_cancelled = TRUE;
	gtk_main_quit();
}

static void addressadd_tree_row_activated(GtkTreeView *view,
		GtkTreePath *path, GtkTreeViewColumn *col,
		gpointer user_data)
{
	addressadd_ok(NULL, NULL);
}

static void addressadd_size_allocate_cb(GtkWidget *widget,
					 GtkAllocation *allocation)
{
	cm_return_if_fail(allocation != NULL);

	gtk_window_get_size(GTK_WINDOW(widget),
		&prefs_common.addressaddwin_width, &prefs_common.addressaddwin_height);
}

static void addressadd_create( void ) {
	GtkWidget *window;
	GtkWidget *vbox, *hbox;
	GtkWidget *frame;
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *picture;
	GtkWidget *entry_name;
	GtkWidget *label_addr;
	GtkWidget *entry_rems;
	GtkWidget *tree_folder;
	GtkWidget *vlbox;
	GtkWidget *tree_win;
	GtkWidget *hbbox;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;
	static GdkGeometry geometry;
	GtkTreeStore *store;
	GtkTreeViewColumn *col;
	GtkCellRenderer *rdr;
	GtkTreeSelection *sel;

	window = gtkut_window_new(GTK_WINDOW_TOPLEVEL, "addressadd");
	gtk_container_set_border_width( GTK_CONTAINER(window), VBOX_BORDER );
	gtk_window_set_title( GTK_WINDOW(window), _("Add to address book") );
	gtk_window_set_position( GTK_WINDOW(window), GTK_WIN_POS_MOUSE );
	gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DIALOG);
	g_signal_connect( G_OBJECT(window), "delete_event",
			  G_CALLBACK(addressadd_delete_event), NULL );
	g_signal_connect( G_OBJECT(window), "key_press_event",
			  G_CALLBACK(addressadd_key_pressed), NULL );
	g_signal_connect(G_OBJECT(window), "size_allocate",
			 G_CALLBACK(addressadd_size_allocate_cb), NULL);
	
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 4);
	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	picture = gtk_image_new();
	gtk_box_pack_start(GTK_BOX(hbox), picture, FALSE, FALSE, 0);

	table = gtk_grid_new();
	gtk_box_pack_start(GTK_BOX(hbox), table, TRUE, TRUE, 0);
	gtk_grid_set_row_spacing(GTK_GRID(table), VSPACING_NARROW);
	gtk_grid_set_column_spacing(GTK_GRID(table), HSPACING_NARROW);

	frame = gtk_frame_new(_("Contact"));
	gtk_frame_set_label_align(GTK_FRAME(frame), 0.01, 0.5);
	gtk_container_add(GTK_CONTAINER(frame), hbox);
	gtk_container_set_border_width( GTK_CONTAINER(frame), 4);
	gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);

	/* First row */
	label = gtk_label_new(_("Name"));
	gtk_grid_attach(GTK_GRID(table), label, 0, 0, 1, 1);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);

	entry_name = gtk_entry_new();
	gtk_widget_set_size_request(entry_name, 150, -1);
	gtk_entry_set_text (GTK_ENTRY(entry_name),"");
	
	gtk_grid_attach(GTK_GRID(table), entry_name, 1, 0, 1, 1);
	gtk_widget_set_hexpand(entry_name, TRUE);
	gtk_widget_set_halign(entry_name, GTK_ALIGN_FILL);

	/* Second row */
	label = gtk_label_new(_("Address"));
	gtk_grid_attach(GTK_GRID(table), label, 0, 1, 1, 1);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);

	label_addr = gtk_label_new("");
	gtk_widget_set_size_request(label_addr, 150, -1);
	gtk_grid_attach(GTK_GRID(table), label_addr, 1, 1, 1, 1);
	gtk_label_set_xalign(GTK_LABEL(label_addr), 0.0);

	/* Third row */
	label = gtk_label_new(_("Remarks"));
	gtk_grid_attach(GTK_GRID(table), label, 0, 2, 1, 1);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);

	entry_rems = gtk_entry_new();
	gtk_widget_set_size_request(entry_rems, 150, -1);
	gtk_grid_attach(GTK_GRID(table), entry_rems, 1, 2, 1, 1);
	gtk_widget_set_hexpand(entry_rems, TRUE);
	gtk_widget_set_halign(entry_rems, GTK_ALIGN_FILL);

	/* Address book/folder tree */
	vlbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, VBOX_BORDER);
	gtk_box_pack_start(GTK_BOX(vbox), vlbox, TRUE, TRUE, 0);
	gtk_container_set_border_width( GTK_CONTAINER(vlbox), 4);

	tree_win = gtk_scrolled_window_new( NULL, NULL );
	gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(tree_win),
				        GTK_POLICY_AUTOMATIC,
				        GTK_POLICY_AUTOMATIC );
	gtk_box_pack_start( GTK_BOX(vlbox), tree_win, TRUE, TRUE, 0 );

	store = gtk_tree_store_new(N_ADDRADD_COLS,
			GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_POINTER);

	tree_folder = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	g_object_unref(store);
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store), ADDRADD_COL_NAME, GTK_SORT_ASCENDING);
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(tree_folder), prefs_common.use_stripes_everywhere);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree_folder), TRUE);
	gtk_tree_view_set_enable_tree_lines(GTK_TREE_VIEW(tree_folder), FALSE);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(tree_folder),
			ADDRADD_COL_NAME);

	/* Column with addressbook/folder icon and name. It has two
	 * renderers, so we create it a bit differently. */
	col = gtk_tree_view_column_new();
	rdr = gtk_cell_renderer_pixbuf_new();
	gtk_cell_renderer_set_padding(rdr, 0, 0);
	gtk_tree_view_column_pack_start(col, rdr, FALSE);
	gtk_tree_view_column_set_attributes(col, rdr,
			"pixbuf", ADDRADD_COL_ICON, NULL);
	rdr = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(col, rdr, TRUE);
	gtk_tree_view_column_set_attributes(col, rdr,
			"markup", ADDRADD_COL_NAME, NULL);
	gtk_tree_view_column_set_title(col, _("Select Address Book Folder"));
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree_folder), col);

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_folder));
	gtk_tree_selection_set_mode(sel, GTK_SELECTION_BROWSE);

	gtk_container_add( GTK_CONTAINER(tree_win), tree_folder );

	g_signal_connect(G_OBJECT(tree_folder), "row-activated",
			G_CALLBACK(addressadd_tree_row_activated), NULL);

	/* Button panel */
	gtkut_stock_button_set_create(&hbbox, &cancel_btn, NULL, _("_Cancel"),
				      &ok_btn, NULL, _("_OK"),
				      NULL, NULL, NULL);
	gtk_box_pack_end(GTK_BOX(vbox), hbbox, FALSE, FALSE, 0);
	gtk_container_set_border_width( GTK_CONTAINER(hbbox), HSPACING_NARROW );
	gtk_widget_grab_default(ok_btn);

	g_signal_connect(G_OBJECT(ok_btn), "clicked",
			 G_CALLBACK(addressadd_ok), NULL);
	g_signal_connect(G_OBJECT(cancel_btn), "clicked",
			 G_CALLBACK(addressadd_cancel), NULL);

	if (!geometry.min_height) {
		geometry.min_width = 300;
		geometry.min_height = 350;
	}

	gtk_window_set_geometry_hints(GTK_WINDOW(window), NULL, &geometry,
				      GDK_HINT_MIN_SIZE);
	gtk_window_set_default_size(GTK_WINDOW(window),
				    prefs_common.addressaddwin_width,
				    prefs_common.addressaddwin_height);

	addressadd_dlg.window        = window;
	addressadd_dlg.picture       = picture;
	addressadd_dlg.entry_name    = entry_name;
	addressadd_dlg.label_address = label_addr;
	addressadd_dlg.entry_remarks = entry_rems;
	addressadd_dlg.tree_folder   = tree_folder;
	addressadd_dlg.ok_btn        = ok_btn;
	addressadd_dlg.cancel_btn    = cancel_btn;

	gtk_widget_show_all( window );

	stock_pixbuf_gdk(STOCK_PIXMAP_BOOK, &bookXpm );
	stock_pixbuf_gdk(STOCK_PIXMAP_DIR_OPEN, &folderXpm );
}

/* Helper function used by addressadd_tree_clear(). */
static gboolean close_dialog_foreach_func(GtkTreeModel *model,
		GtkTreePath *path,
		GtkTreeIter *iter,
		gpointer user_data)
{
	FolderInfo *fi;

	gtk_tree_model_get(model, iter, ADDRADD_COL_PTR, &fi, -1);
	if (fi != NULL) {
		addressadd_free_folderinfo(fi);
	}
	gtk_tree_store_set(GTK_TREE_STORE(model), iter, ADDRADD_COL_PTR, NULL, -1);
	return FALSE;
}

/* Function to remove all rows from the tree view, and free the
 * FolderInfo pointers in them in pointer column. */
static void addressadd_tree_clear()
{
	GtkWidget *view = addressadd_dlg.tree_folder;
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));

	gtk_tree_model_foreach(model, close_dialog_foreach_func, NULL);
	gtk_tree_store_clear(GTK_TREE_STORE(model));
}

#ifndef USE_ALT_ADDRBOOK
static void addressadd_load_folder( GtkTreeIter *parent_iter,
		ItemFolder *parentFolder, FolderInfo *fiParent )
{
	GtkWidget *view = addressadd_dlg.tree_folder;
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
	GList *list;
	ItemFolder *folder;
	gchar *name;
	FolderInfo *fi;

	list = parentFolder != NULL? parentFolder->listFolder: NULL;
	while( list ) {
		folder = list->data;
		name = g_strdup( ADDRITEM_NAME(folder) );
		fi = addressadd_create_folderinfo( fiParent->book, folder );

		gtk_tree_store_append(GTK_TREE_STORE(model), &iter, parent_iter);
		gtk_tree_store_set(GTK_TREE_STORE(model), &iter,
				ADDRADD_COL_ICON, folderXpm,
				ADDRADD_COL_NAME, name,
				ADDRADD_COL_PTR, fi,
				-1);
		g_free( name );

		addressadd_load_folder( parent_iter, folder, fi );

		list = g_list_next( list );
	}
}

static void addressadd_load_data( AddressIndex *addrIndex ) {
	AddressDataSource *ds;
	GList *list, *nodeDS;
	gchar *name;
	ItemFolder *rootFolder;
	AddressBookFile *abf;
	FolderInfo *fi;
	GtkWidget *view = addressadd_dlg.tree_folder;
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
	GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));

	addressadd_tree_clear();

	list = addrindex_get_interface_list( addrIndex );
	while( list ) {
		AddressInterface *ainterface = list->data;
		if( ainterface->type == ADDR_IF_BOOK || 
				ainterface->type == ADDR_IF_LDAP ) {
			nodeDS = ainterface->listSource;
			while( nodeDS ) {
				ds = nodeDS->data;
				name = g_strdup( addrindex_ds_get_name( ds ) );

				/* Read address book */
				if( ! addrindex_ds_get_read_flag( ds ) ) {
					addrindex_ds_read_data( ds );
				}

				/* Add node for address book */
				abf = ds->rawDataSource;
				fi = addressadd_create_folderinfo( abf, NULL );

				gtk_tree_store_append(GTK_TREE_STORE(model), &iter, NULL);
				gtk_tree_store_set(GTK_TREE_STORE(model), &iter,
						ADDRADD_COL_ICON, bookXpm,
						ADDRADD_COL_NAME, name,
						ADDRADD_COL_PTR, fi,
						-1);
				g_free( name );

				rootFolder = addrindex_ds_get_root_folder( ds );
				addressadd_load_folder( &iter, rootFolder, fi );

				nodeDS = g_list_next( nodeDS );
			}
		}
		list = g_list_next( list );
	}

	if (gtk_tree_model_get_iter_first(model, &iter))
		gtk_tree_selection_select_iter(sel, &iter);
}
#else
static void addressadd_load_data() {
	GSList *list;
	FolderInfo *fi = NULL;
	GError* error = NULL;
	GtkWidget *view = addressadd_dlg.tree_folder;
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
	GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
	gchar *name;

	addressadd_tree_clear();

	list = addressbook_dbus_get_books(&error);
	for (; list; list = g_slist_next(list)) {
		name = (gchar *) list->data;
		fi = addressadd_create_folderinfo(name);
		gtk_tree_store_append(GTK_TREE_STORE(model), &iter, NULL);
		gtk_tree_store_set(GTK_TREE_STORE(model), &iter,
				ADDRADD_COL_ICON, bookXpm,ADDRADD_COL_NAME, (gchar *)list->data,
				ADDRADD_COL_PTR, fi,
				-1);
	}

	if (gtk_tree_model_get_iter_first(model, &iter))
                gtk_tree_selection_select_iter(sel, &iter);
}
#endif

#ifndef USE_ALT_ADDRBOOK 
gboolean addressadd_selection( AddressIndex *addrIndex, const gchar *name, 
		const gchar *address, const gchar *remarks, GdkPixbuf *picture ) {
#else
gboolean addressadd_selection(const gchar *name, const gchar *address,
							  const gchar *remarks, GdkPixbuf *picture ) {
#endif
	gboolean retVal = FALSE;
#ifndef USE_ALT_ADDRBOOK 
	ItemPerson *person = NULL;
#endif
	FolderInfo *fi = NULL;
	addressadd_cancelled = FALSE;

	if( ! addressadd_dlg.window ) addressadd_create();

	addressadd_dlg.fiSelected = NULL;
#ifndef USE_ALT_ADDRBOOK
	addressadd_load_data( addrIndex );
#else
	addressadd_load_data();
#endif
	gtk_widget_show(addressadd_dlg.window);
	gtk_window_set_modal(GTK_WINDOW(addressadd_dlg.window), TRUE);
	gtk_widget_grab_focus(addressadd_dlg.ok_btn);

	manage_window_set_transient(GTK_WINDOW(addressadd_dlg.window));

	gtk_entry_set_text( GTK_ENTRY(addressadd_dlg.entry_name ), "" );
	gtk_label_set_text( GTK_LABEL(addressadd_dlg.label_address ), "" );
	gtk_entry_set_text( GTK_ENTRY(addressadd_dlg.entry_remarks ), "" );
	if( name )
		gtk_entry_set_text( GTK_ENTRY(addressadd_dlg.entry_name ), name );
	if( address )
		gtk_label_set_text( GTK_LABEL(addressadd_dlg.label_address ), address );
	if( remarks )
		gtk_entry_set_text( GTK_ENTRY(addressadd_dlg.entry_remarks ), remarks );
	if( picture ) {
		gtk_image_set_from_pixbuf(GTK_IMAGE(addressadd_dlg.picture), picture);
		gtk_widget_show(GTK_WIDGET(addressadd_dlg.picture));
	} else {
		gtk_widget_hide(GTK_WIDGET(addressadd_dlg.picture));
	}
	gtk_main();
	gtk_widget_hide( addressadd_dlg.window );
	gtk_window_set_modal(GTK_WINDOW(addressadd_dlg.window), FALSE);
	if( ! addressadd_cancelled ) {
		if( addressadd_dlg.fiSelected ) {
			gchar *returned_name;
			gchar *returned_remarks;
			returned_name = gtk_editable_get_chars( GTK_EDITABLE(addressadd_dlg.entry_name), 0, -1 );
			returned_remarks = gtk_editable_get_chars( GTK_EDITABLE(addressadd_dlg.entry_remarks), 0, -1 );

			fi = addressadd_dlg.fiSelected;
			
#ifndef USE_ALT_ADDRBOOK
			person = addrbook_add_contact( fi->book, fi->folder, 
							returned_name, 
							address, 
							returned_remarks);
			
			if (person != NULL) {
				person->status = ADD_ENTRY;

				if (picture) {
					GError *error = NULL;
					gchar *name = g_strconcat( get_rc_dir(), G_DIR_SEPARATOR_S, ADDRBOOK_DIR, G_DIR_SEPARATOR_S, 
								ADDRITEM_ID(person), ".png", NULL );
					gdk_pixbuf_save(picture, name, "png", &error, NULL);
					if (error) {
						g_warning("failed to save image: %s", error->message);
						g_error_free(error);
					}
					addritem_person_set_picture( person, ADDRITEM_ID(person) ) ;
					g_free( name );
				}
			}
#else
			ContactData* contact = g_new0(ContactData, 1);
			GError* error = NULL;
			
			if (returned_name)
				contact->cn = g_strdup(returned_name);
			else
				contact->cn = g_strdup(address);
			
			contact->name = g_strdup(returned_name);
			contact->email = g_strdup(address);
			contact->remarks = g_strdup(returned_remarks);
			contact->book = g_strdup(fi->book);
			contact->picture = picture;
			
            if (addressbook_dbus_add_contact(contact, &error) == 0) {
				debug_print("Added to addressbook:\n%s\n%s\n%s\n%s\n",
							 returned_name, address, returned_remarks, fi->book);
				retVal = TRUE;
			}
			else {
				retVal = FALSE;
				if (error) {
					GtkWidget* dialog = gtk_message_dialog_new (
								GTK_WINDOW(addressadd_dlg.window),
                                GTK_DIALOG_DESTROY_WITH_PARENT,
                                GTK_MESSAGE_ERROR,
                                GTK_BUTTONS_CLOSE,
                                "%s", error->message);
					gtk_dialog_run (GTK_DIALOG (dialog));
					gtk_widget_destroy (dialog);
					g_error_free(error);
				}
			}
			contact_data_free(&contact);
#endif
#ifdef USE_LDAP
			if (person != NULL && fi->book->type == ADBOOKTYPE_LDAP) {
				LdapServer *server = (LdapServer *) fi->book;
				ldapsvr_set_modified(server, TRUE);
				ldapsvr_update_book(server, person);
				if (server->retVal != LDAPRC_SUCCESS) {
					alertpanel( _("Add address(es)"),
						_("Can't add the specified address"),
						"window-close", _("_Close"), NULL, NULL,
						NULL, NULL, ALERTFOCUS_FIRST );
					return server->retVal;
				}
			}
#endif
			g_free(returned_name);
			g_free(returned_remarks);
#ifndef USE_ALT_ADDRBOOK
			if( person ) retVal = TRUE;
#endif
		}
	}

	addressadd_tree_clear();

	return retVal;
}

/*
* End of Source.
*/

