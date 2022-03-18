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
 * Import LDIF address book data.
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

#include "addrbook.h"
#include "addressbook.h"
#include "addressitem.h"
#include "gtkutils.h"
#include "stock_pixmap.h"
#include "prefs_common.h"
#include "manage_window.h"
#include "mgutils.h"
#include "ldif.h"
#include "utils.h"
#include "filesel.h"

#define IMPORTLDIF_GUESS_NAME      "LDIF Import"

#define PAGE_FILE_INFO             0
#define PAGE_ATTRIBUTES            1
#define PAGE_FINISH                2

#define IMPORTLDIF_WIDTH           390
#define IMPORTLDIF_HEIGHT          300

#define FIELDS_COL_WIDTH_RESERVED  10
#define FIELDS_COL_WIDTH_SELECT    10
#define FIELDS_COL_WIDTH_FIELD     140
#define FIELDS_COL_WIDTH_ATTRIB    140

typedef enum {
	FIELD_COL_RESERVED,
	FIELD_COL_SELECT,
	FIELD_COL_FIELD,
	FIELD_COL_ATTRIB,
	FIELD_COL_PTR,
	FIELDS_N_COLS
} ImpLdif_FieldColPos;

/**
 * LDIF dialog definition.
 */
static struct _ImpLdif_Dlg {
	GtkWidget *window;
	GtkWidget *notebook;
	GtkWidget *entryFile;
	GtkWidget *entryName;
	GtkWidget *view_fields;
	GtkWidget *entryField;
	GtkWidget *entryAttrib;
	GtkWidget *checkSelect;
	GtkWidget *btnModify;
	GtkWidget *labelBook;
	GtkWidget *labelFile;
	GtkWidget *labelRecords;
	GtkWidget *btnPrev;
	GtkWidget *btnNext;
	GtkWidget *btnProceed;
	GtkWidget *btnCancel;
	GtkWidget *statusbar;
	gint      status_cid;
	gint      rowCount;
	gchar     *nameBook;
	gchar     *fileName;
	gboolean  cancelled;
} impldif_dlg;

static struct _AddressFileSelection _imp_ldif_file_selector_;
static AddressBookFile *_importedBook_;
static AddressIndex *_imp_addressIndex_;
static LdifFile *_ldifFile_ = NULL;

static GdkPixbuf *markxpm;

/**
 * Structure of error message table.
 */
typedef struct _ErrMsgTableEntry ErrMsgTableEntry;
struct _ErrMsgTableEntry {
	gint	code;
	gchar	*description;
};

static gchar *_errMsgUnknown_ = N_( "Unknown" );

/**
 * Lookup table of error messages for general errors. Note that a NULL
 * description signifies the end of the table.
 */
static ErrMsgTableEntry _lutErrorsLDIF_[] = {
	{ MGU_SUCCESS,		N_("Success") },
	{ MGU_BAD_ARGS,		N_("Bad arguments") },
	{ MGU_NO_FILE,		N_("File not specified") },
	{ MGU_OPEN_FILE,	N_("Error opening file") },
	{ MGU_ERROR_READ,	N_("Error reading file") },
	{ MGU_EOF,		N_("End of file encountered") },
	{ MGU_OO_MEMORY,	N_("Error allocating memory") },
	{ MGU_BAD_FORMAT,	N_("Bad file format") },
	{ MGU_ERROR_WRITE,	N_("Error writing to file") },
	{ MGU_OPEN_DIRECTORY,	N_("Error opening directory") },
	{ MGU_NO_PATH,      	N_("No path specified") },
	{ 0,			NULL }
};

/**
 * Lookup message for specified error code.
 * \param lut  Lookup table.
 * \param code Code to lookup.
 * \return Description associated to code.
 */
static gchar *imp_ldif_err2string( ErrMsgTableEntry lut[], gint code ) {
        gchar *desc = NULL;
        ErrMsgTableEntry entry;
        gint i;

        for( i = 0; ; i++ ) {
                entry = lut[ i ];
                if( entry.description == NULL ) break;
                if( entry.code == code ) {
                        desc = entry.description;
                        break;
                }
        }
        if( ! desc ) {
		desc = _errMsgUnknown_;
        }
        return desc;
}

/**
 * Display message in status field.
 * \param msg Message to display.
 */
static void imp_ldif_status_show( gchar *msg ) {
	if( impldif_dlg.statusbar != NULL ) {
		gtk_statusbar_pop( GTK_STATUSBAR(impldif_dlg.statusbar),
			impldif_dlg.status_cid );
		if( msg ) {
			gtk_statusbar_push(
				GTK_STATUSBAR(impldif_dlg.statusbar),
				impldif_dlg.status_cid, msg );
		}
	}
}

/**
 * Select and display status message appropriate for the page being displayed.
 */
static void imp_ldif_message( void ) {
	gchar *sMsg = NULL;
	gint pageNum;

	pageNum = gtk_notebook_get_current_page( GTK_NOTEBOOK(impldif_dlg.notebook) );
	if( pageNum == PAGE_FILE_INFO ) {
		sMsg = _( "Please specify address book name and file to import." );
	}
	else if( pageNum == PAGE_ATTRIBUTES ) {
		sMsg = _( "Select and rename LDIF field names to import." );
	}
	else if( pageNum == PAGE_FINISH ) {
		sMsg = _( "File imported." );
	}
	imp_ldif_status_show( sMsg );
}

/**
 * Update the line (represented by the GtkTreeIter) with data
 * from the Ldif_FieldRec.
 */
static void _populate_iter(GtkListStore *store, GtkTreeIter *iter,
		Ldif_FieldRec *rec)
{
	gtk_list_store_set(store, iter,
			FIELD_COL_FIELD, rec->tagName,
			FIELD_COL_ATTRIB, rec->userName,
			FIELD_COL_PTR, rec,
			-1);
	gtk_list_store_set(store, iter,
			FIELD_COL_SELECT, rec->selected ? markxpm : NULL, -1);
	gtk_list_store_set(store, iter,
			FIELD_COL_RESERVED, rec->reserved ? markxpm : NULL, -1);
}

/**
 * Load list with LDIF fields read from file.
 * \param ldf LDIF control data.
 */
static void imp_ldif_load_fields( LdifFile *ldf ) {
	GtkWidget *view = impldif_dlg.view_fields;
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
	GtkTreeIter iter;
	GList *node, *list;

	impldif_dlg.rowCount = 0;

	if( ! ldf->accessFlag ) return;

	gtk_list_store_clear(GTK_LIST_STORE(model));

	list = ldif_get_fieldlist( ldf );
	node = list;
	while( node ) {
		Ldif_FieldRec *rec = node->data;

		gtk_list_store_append(GTK_LIST_STORE(model), &iter);
		_populate_iter(GTK_LIST_STORE(model), &iter, rec);
		impldif_dlg.rowCount++;
		node = g_list_next( node );
	}
	g_list_free( list );
	list = NULL;
	ldif_set_accessed( ldf, FALSE );
}

/**
 * Test whether we can move off fields page.
 * \return <i>TRUE</i> if OK to move off page.
 */
static gboolean imp_ldif_field_move() {
	gboolean retVal = FALSE;
	gchar *newFile;
	AddressBookFile *abf = NULL;

	if( _importedBook_ ) {
		addrbook_free_book( _importedBook_ );
	}

	abf = addrbook_create_book();
	addrbook_set_path( abf, _imp_addressIndex_->filePath );
	addrbook_set_name( abf, impldif_dlg.nameBook );
	newFile = addrbook_guess_next_file( abf );
	addrbook_set_file( abf, newFile );
	g_free( newFile );

	/* Import data into file */
	if( ldif_import_data( _ldifFile_, abf->addressCache ) == MGU_SUCCESS ) {
		addrbook_save_data( abf );
		_importedBook_ = abf;
		retVal = TRUE;
	}
	else {
		addrbook_free_book( abf );
	}

	return retVal;
}

static void _update_selected_row()
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	Ldif_FieldRec *rec;

	rec = gtkut_tree_view_get_selected_pointer(
			GTK_TREE_VIEW(impldif_dlg.view_fields), FIELD_COL_PTR,
			&model, NULL, &iter);
	if (rec == NULL)
		return;

	ldif_field_set_name(rec, gtk_entry_get_text(
				GTK_ENTRY(impldif_dlg.entryAttrib)));
	ldif_field_set_selected(rec, gtk_toggle_button_get_active(
				GTK_TOGGLE_BUTTON(impldif_dlg.checkSelect)));

	_populate_iter(GTK_LIST_STORE(model), &iter, rec);
}

static void imp_ldif_modify_pressed(GtkButton *widget, gpointer user_data)
{
	_update_selected_row();
}

static void imp_ldif_entryattrib_activate(GtkEntry *entry, gpointer user_data)
{
	_update_selected_row();
}

/**
 * Test whether we can move off file page.
 * \return <i>TRUE</i> if OK to move off page.
 */
static gboolean imp_ldif_file_move() {
	gboolean retVal = FALSE;
	gchar *sName;
	gchar *sFile;
	gchar *sMsg = NULL;
	gboolean errFlag = FALSE;

	sFile = gtk_editable_get_chars( GTK_EDITABLE(impldif_dlg.entryFile), 0, -1 );
	g_strstrip( sFile );

	sName = gtk_editable_get_chars( GTK_EDITABLE(impldif_dlg.entryName), 0, -1 );
	g_strstrip( sName );

	g_free( impldif_dlg.nameBook );
	g_free( impldif_dlg.fileName );
	impldif_dlg.nameBook = sName;
	impldif_dlg.fileName = sFile;

	gtk_entry_set_text( GTK_ENTRY(impldif_dlg.entryFile), sFile );
	gtk_entry_set_text( GTK_ENTRY(impldif_dlg.entryName), sName );

	if( *sFile == '\0' ) {
		sMsg = _( "Please select a file." );
		gtk_widget_grab_focus(impldif_dlg.entryFile);
		errFlag = TRUE;
	}

	if( ! errFlag && *sName == '\0' ) {
		sMsg = _( "Address book name must be supplied." );
		gtk_widget_grab_focus(impldif_dlg.entryName);
		errFlag = TRUE;
	}

	if( ! errFlag ) {
		/* Read attribute list */
		ldif_set_file( _ldifFile_, sFile );
		if( ldif_read_tags( _ldifFile_ ) == MGU_SUCCESS ) {
			/* Load fields */
			/* ldif_print_file( _ldifFile_, stdout ); */
			imp_ldif_load_fields( _ldifFile_ );
			retVal = TRUE;
		}
		else {
			sMsg = imp_ldif_err2string( _lutErrorsLDIF_, _ldifFile_->retVal );
		}
	}
	imp_ldif_status_show( sMsg );

	return retVal;
}

/**
 * Display finish page.
 */
static void imp_ldif_finish_show() {
	gchar *sMsg;
	gchar *name;

	name = gtk_editable_get_chars( GTK_EDITABLE(impldif_dlg.entryName), 0, -1 );
	gtk_label_set_text( GTK_LABEL(impldif_dlg.labelBook), name );
	g_free( name );
	gtk_label_set_text( GTK_LABEL(impldif_dlg.labelFile), _ldifFile_->path );
	gtk_label_set_text( GTK_LABEL(impldif_dlg.labelRecords), itos( _ldifFile_->importCount ) );
	gtk_widget_set_sensitive( impldif_dlg.btnPrev, FALSE );
	gtk_widget_hide( impldif_dlg.btnNext );
	gtk_widget_show( impldif_dlg.btnProceed );
	gtk_widget_set_sensitive( impldif_dlg.btnProceed, FALSE );
	if( _ldifFile_->retVal == MGU_SUCCESS ) {
		sMsg = _( "LDIF file imported successfully." );
	}
	else {
		sMsg = imp_ldif_err2string( _lutErrorsLDIF_, _ldifFile_->retVal );
	}
	imp_ldif_status_show( sMsg );
	gtk_widget_grab_focus(impldif_dlg.btnCancel);
}

/**
 * Callback function to select previous page.
 * \param widget Widget (button).
 */
static void imp_ldif_prev( GtkWidget *widget ) {
	gint pageNum;

	pageNum = gtk_notebook_get_current_page( GTK_NOTEBOOK(impldif_dlg.notebook) );
	if( pageNum == PAGE_ATTRIBUTES ) {
		/* Goto file page stuff */
		gtk_notebook_set_current_page(
			GTK_NOTEBOOK(impldif_dlg.notebook), PAGE_FILE_INFO );
		gtk_widget_set_sensitive( impldif_dlg.btnPrev, FALSE );
		gtk_widget_hide( impldif_dlg.btnProceed );
		gtk_widget_show( impldif_dlg.btnNext );
	}
	imp_ldif_message();
}

/**
 * Callback function to select next page.
 * \param widget Widget (button).
 */
static void imp_ldif_next( GtkWidget *widget ) {
	gint pageNum;

	pageNum = gtk_notebook_get_current_page( GTK_NOTEBOOK(impldif_dlg.notebook) );
	if( pageNum == PAGE_FILE_INFO ) {
		/* Goto attributes stuff */
		if( imp_ldif_file_move() ) {
			gtk_notebook_set_current_page(
				GTK_NOTEBOOK(impldif_dlg.notebook), PAGE_ATTRIBUTES );
			imp_ldif_message();
			gtk_widget_set_sensitive( impldif_dlg.btnPrev, TRUE );
			gtk_widget_hide( impldif_dlg.btnNext );
			gtk_widget_show( impldif_dlg.btnProceed );
			gtk_widget_set_sensitive( impldif_dlg.btnProceed, TRUE );
		}
		else {
			gtk_widget_set_sensitive( impldif_dlg.btnPrev, FALSE );
			_ldifFile_->dirtyFlag = TRUE;
		}
	}
	else if( pageNum == PAGE_ATTRIBUTES ) {
		/* Goto finish stuff */
		if( imp_ldif_field_move() ) {
			gtk_notebook_set_current_page(
				GTK_NOTEBOOK(impldif_dlg.notebook), PAGE_FINISH );
			gtk_button_set_label(GTK_BUTTON(impldif_dlg.btnCancel),
					     _("_Close"));
			gtk_button_set_image(GTK_BUTTON(impldif_dlg.btnCancel),
				gtk_image_new_from_icon_name("window-close", GTK_ICON_SIZE_BUTTON));
			imp_ldif_finish_show();
		}
	}
}

/**
 * Callback function to cancel and close dialog.
 * \param widget Widget (button).
 * \param data   User data.
 */
static void imp_ldif_cancel( GtkWidget *widget, gpointer data ) {
	gint pageNum;

	pageNum = gtk_notebook_get_current_page( GTK_NOTEBOOK(impldif_dlg.notebook) );
	if( pageNum != PAGE_FINISH ) {
		impldif_dlg.cancelled = TRUE;
	}
	gtk_main_quit();
}


/**
 * Create LDIF file selection dialog.
 * \param afs Address file selection data.
 */
static void imp_ldif_file_select_create( AddressFileSelection *afs ) {
	gchar *file = filesel_select_file_open(_("Select LDIF File"), NULL);

	if (file == NULL)
		afs->cancelled = TRUE;
	else {
		afs->cancelled = FALSE;
		gtk_entry_set_text( GTK_ENTRY(impldif_dlg.entryFile), file );
		g_free(file);
	}
}

/**
 * Callback function to display LDIF file selection dialog.
 */
static void imp_ldif_file_select( void ) {
	imp_ldif_file_select_create( & _imp_ldif_file_selector_ );
}

/**
 * Callback function to handle dialog close event.
 * \param widget Widget (dialog).
 * \param event  Event object.
 * \param data   User data.
 */
static gint imp_ldif_delete_event( GtkWidget *widget, GdkEventAny *event, gpointer data ) {
	imp_ldif_cancel( widget, data );
	return TRUE;
}

/**
 * Callback function to respond to dialog key press events.
 * \param widget Widget.
 * \param event  Event object.
 * \param data   User data.
 */
static gboolean imp_ldif_key_pressed( GtkWidget *widget, GdkEventKey *event, gpointer data ) {
	if (event && event->keyval == GDK_KEY_Escape) {
		imp_ldif_cancel( widget, data );
	}
	return FALSE;
}

/**
 * Format notebook "file" page.
 * \param pageNum Page (tab) number.
 * \param pageLbl Page (tab) label.
 */
static void imp_ldif_page_file( gint pageNum, gchar *pageLbl ) {
	GtkWidget *vbox;
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *entryFile;
	GtkWidget *entryName;
	GtkWidget *btnFile;

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_container_add( GTK_CONTAINER( impldif_dlg.notebook ), vbox );
	gtk_container_set_border_width( GTK_CONTAINER (vbox), BORDER_WIDTH );

	label = gtk_label_new( pageLbl );
	gtk_widget_show( label );
	gtk_notebook_set_tab_label(
		GTK_NOTEBOOK( impldif_dlg.notebook ),
		gtk_notebook_get_nth_page(
			GTK_NOTEBOOK( impldif_dlg.notebook ), pageNum ),
		label );

	table = gtk_grid_new();
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
	gtk_container_set_border_width( GTK_CONTAINER(table), 8 );
	gtk_grid_set_row_spacing(GTK_GRID(table), 8);
	gtk_grid_set_column_spacing(GTK_GRID(table), 8);

	/* First row */
	label = gtk_label_new(_("Address Book"));
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_grid_attach(GTK_GRID(table), label, 0, 0, 1, 1);

	entryName = gtk_entry_new();
	gtk_grid_attach(GTK_GRID(table), entryName, 1, 0, 1, 1);
	gtk_widget_set_hexpand(entryName, TRUE);
	gtk_widget_set_halign(entryName, GTK_ALIGN_FILL);

	CLAWS_SET_TIP(entryName, _( 
		"Specify the name for the address book that will " \
		"be created from the LDIF file data." ));

	/* Second row */
	label = gtk_label_new(_("File Name"));
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_grid_attach(GTK_GRID(table), label, 0, 1, 1, 1);

	entryFile = gtk_entry_new();
	gtk_grid_attach(GTK_GRID(table), entryFile, 1, 1, 1, 1);
	gtk_widget_set_hexpand(entryFile, TRUE);
	gtk_widget_set_halign(entryFile, GTK_ALIGN_FILL);

	CLAWS_SET_TIP(entryFile,
		_( "The full file specification of the LDIF file to import." ));

	btnFile = gtkut_get_browse_file_btn(_("B_rowse"));
	gtk_grid_attach(GTK_GRID(table), btnFile, 2, 1, 1, 1);

	CLAWS_SET_TIP(btnFile,
		_( "Select the LDIF file to import." ));

	gtk_widget_show_all(vbox);

	/* Button handler */
	g_signal_connect(G_OBJECT(btnFile), "clicked",
			 G_CALLBACK(imp_ldif_file_select), NULL);

	impldif_dlg.entryFile = entryFile;
	impldif_dlg.entryName = entryName;

}

static void imp_ldif_field_list_cursor_changed(GtkTreeView *view,
		gpointer user_data)
{
	Ldif_FieldRec *rec;

	gtk_entry_set_text( GTK_ENTRY(impldif_dlg.entryAttrib), "" );

	rec = gtkut_tree_view_get_selected_pointer(view, FIELD_COL_PTR,
			NULL, NULL, NULL);

	if( rec != NULL) {
		/* Update widget contents */
		gtk_label_set_text(
			GTK_LABEL(impldif_dlg.entryField), rec->tagName );
		if( rec->userName )
			gtk_entry_set_text(
				GTK_ENTRY(impldif_dlg.entryAttrib), rec->userName );
		gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON(impldif_dlg.checkSelect),
			rec->selected );

		/* Disable widgets for reserved fields */
		gtk_widget_set_sensitive(
			impldif_dlg.entryAttrib, ! rec->reserved );
		gtk_widget_set_sensitive(
			impldif_dlg.checkSelect, ! rec->reserved );
		gtk_widget_set_sensitive(
			impldif_dlg.btnModify,   ! rec->reserved );
	}
	gtk_widget_grab_focus(impldif_dlg.entryAttrib);
}

static void imp_ldif_field_list_row_activated(GtkTreeView *view,
		GtkTreePath *path, GtkTreeViewColumn *col,
		gpointer user_data)
{
	GtkTreeModel *model = gtk_tree_view_get_model(view);
	GtkTreeIter iter;
	gboolean ok;
	Ldif_FieldRec *rec;

	ok = gtk_tree_model_get_iter(model, &iter, path);
	if (!ok) {
		return; /* Huh? */
	}

	gtk_tree_model_get(model, &iter, FIELD_COL_PTR, &rec, -1);
	cm_return_if_fail(rec != NULL);

	/* Flip the "selected" state for the record, and update the
	 * "selected" column in the list view, as well as the
	 * "selected" checkbox. */
	ldif_field_toggle(rec);
	gtk_list_store_set(GTK_LIST_STORE(model), &iter,
			FIELD_COL_SELECT, rec->selected ? markxpm : NULL, -1);
	gtk_toggle_button_set_active(
		GTK_TOGGLE_BUTTON(impldif_dlg.checkSelect),
		rec->selected );
}

/**
 * Format notebook fields page.
 * \param pageNum Page (tab) number.
 * \param pageLbl Page (tab) label.
 */
static void imp_ldif_page_fields( gint pageNum, gchar *pageLbl ) {
	GtkWidget *vbox;
	GtkWidget *vboxt;
	GtkWidget *vboxb;
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *scrollwin;
	GtkWidget *view_fields;
	GtkWidget *entryField;
	GtkWidget *entryAttrib;
	GtkWidget *checkSelect;
	GtkWidget *btnModify;
	GtkWidget *eventBox;
	GtkListStore *store;
	GtkCellRenderer *rdr;
	GtkTreeViewColumn *col;
	GtkTreeSelection *sel;

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_container_add( GTK_CONTAINER( impldif_dlg.notebook ), vbox );
	gtk_container_set_border_width( GTK_CONTAINER (vbox), 4 );

	label = gtk_label_new( pageLbl );
	gtk_widget_show( label );
	gtk_notebook_set_tab_label(
		GTK_NOTEBOOK( impldif_dlg.notebook ),
		gtk_notebook_get_nth_page(GTK_NOTEBOOK( impldif_dlg.notebook ), pageNum ),
		label );

	/* Upper area - Field list */
	vboxt = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4 );
	gtk_container_add( GTK_CONTAINER( vbox ), vboxt );

	scrollwin = gtk_scrolled_window_new( NULL, NULL );
	gtk_container_add( GTK_CONTAINER(vboxt), scrollwin );
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);

	store = gtk_list_store_new(FIELDS_N_COLS,
			GDK_TYPE_PIXBUF, GDK_TYPE_PIXBUF,
			G_TYPE_STRING, G_TYPE_STRING,
			G_TYPE_POINTER,
			-1);

	view_fields = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	g_object_unref(store);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view_fields), TRUE);
	gtk_tree_view_set_reorderable(GTK_TREE_VIEW(view_fields), FALSE);
	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(view_fields));
	gtk_tree_selection_set_mode(sel, GTK_SELECTION_BROWSE);

	rdr = gtk_cell_renderer_pixbuf_new();
	col = gtk_tree_view_column_new_with_attributes(_("R"), rdr,
			"pixbuf", FIELD_COL_RESERVED, NULL);
	gtk_tree_view_column_set_min_width(col, FIELD_COL_RESERVED);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view_fields), col);

	col = gtk_tree_view_column_new_with_attributes(_("S"), rdr,
			"pixbuf", FIELD_COL_SELECT, NULL);
	gtk_tree_view_column_set_min_width(col, FIELD_COL_SELECT);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view_fields), col);

	rdr = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes(_("LDIF Field Name"), rdr,
			"markup", FIELD_COL_FIELD, NULL);
	gtk_tree_view_column_set_min_width(col, FIELD_COL_FIELD);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view_fields), col);

	col = gtk_tree_view_column_new_with_attributes(_("Attribute Name"), rdr,
			"markup", FIELD_COL_ATTRIB, NULL);
	gtk_tree_view_column_set_min_width(col, FIELD_COL_ATTRIB);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view_fields), col);

	gtk_container_add( GTK_CONTAINER(scrollwin), view_fields );

	/* Lower area - Edit area */
	vboxb = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4 );
	gtk_box_pack_end(GTK_BOX(vbox), vboxb, FALSE, FALSE, 2);

	/* Data entry area */
	table = gtk_grid_new();
	gtk_box_pack_start(GTK_BOX(vboxb), table, FALSE, FALSE, 0);
	gtk_grid_set_row_spacing(GTK_GRID(table), 4);
	gtk_grid_set_column_spacing(GTK_GRID(table), 4);

	/* First row */
	label = gtk_label_new(_("LDIF Field"));
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_grid_attach(GTK_GRID(table), label, 0, 0, 1, 1);

	entryField = gtk_label_new( "" );
	gtk_label_set_xalign(GTK_LABEL(entryField), 0.0);
	gtk_grid_attach(GTK_GRID(table), entryField, 1, 0, 1, 1);
	gtk_widget_set_hexpand(entryField, TRUE);
	gtk_widget_set_halign(entryField, GTK_ALIGN_FILL);

	/* Second row */
	label = gtk_label_new(_("Attribute"));
	/*
	 * Use an event box to attach some help in the form of a tooltip.
	 * Tried this for the clist but it looked bad.
	 */
	eventBox = gtk_event_box_new();
	gtk_container_add( GTK_CONTAINER(eventBox), label );
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_grid_attach(GTK_GRID(table), eventBox, 0, 1, 1, 1);

	CLAWS_SET_TIP(eventBox, _(
		"Choose the LDIF field that will be renamed or selected " \
		"for import in the list above. Reserved fields (marked " \
		"with a tick in the \"R\" column), are automatically " \
		"imported and cannot be renamed. A single click in the " \
		"Select (\"S\") column will select the field for import " \
		"with a tick. A single click anywhere in the row will " \
		"select that field for rename in the input area below " \
		"the list. A double click anywhere in the row will also " \
		"select the field for import."));

	entryAttrib = gtk_entry_new();
	gtk_grid_attach(GTK_GRID(table), entryAttrib, 1, 1, 1, 1);
	gtk_widget_set_hexpand(entryAttrib, TRUE);
	gtk_widget_set_halign(entryAttrib, GTK_ALIGN_FILL);

	CLAWS_SET_TIP(entryAttrib,
		_( "The LDIF field can be renamed to the User Attribute name." ));

	/* Next row */
	checkSelect = gtk_check_button_new_with_label( _( "Select for Import" ) );
	gtk_grid_attach(GTK_GRID(table), checkSelect, 1, 2, 1, 1);

	CLAWS_SET_TIP(checkSelect,
		_( "Select the LDIF field for import into the address book." ));

	btnModify = gtk_button_new_with_label( _(" Modify "));
	gtk_grid_attach(GTK_GRID(table), btnModify, 2, 2, 1, 1);

	CLAWS_SET_TIP(btnModify,
		_( "This button will update the list above with the data supplied." ));

	gtk_widget_show_all(vbox);

	/* Event handlers */
	g_signal_connect(G_OBJECT(view_fields), "cursor-changed",
			G_CALLBACK(imp_ldif_field_list_cursor_changed), NULL);
	g_signal_connect(G_OBJECT(view_fields), "row-activated",
			G_CALLBACK(imp_ldif_field_list_row_activated), NULL);
	g_signal_connect(G_OBJECT(btnModify), "clicked",
			G_CALLBACK(imp_ldif_modify_pressed), NULL );
	g_signal_connect(G_OBJECT(entryAttrib), "activate",
			G_CALLBACK(imp_ldif_entryattrib_activate), NULL);

	impldif_dlg.view_fields = view_fields;
	impldif_dlg.entryField  = entryField;
	impldif_dlg.entryAttrib = entryAttrib;
	impldif_dlg.checkSelect = checkSelect;
	impldif_dlg.btnModify   = btnModify;
}

/**
 * Format notebook finish page.
 * \param pageNum Page (tab) number.
 * \param pageLbl Page (tab) label.
 */
static void imp_ldif_page_finish( gint pageNum, gchar *pageLbl ) {
	GtkWidget *vbox;
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *labelBook;
	GtkWidget *labelFile;
	GtkWidget *labelRecs;

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_container_add( GTK_CONTAINER( impldif_dlg.notebook ), vbox );
	gtk_container_set_border_width( GTK_CONTAINER (vbox), BORDER_WIDTH );

	label = gtk_label_new( pageLbl );
	gtk_widget_show( label );
	gtk_notebook_set_tab_label(
		GTK_NOTEBOOK( impldif_dlg.notebook ),
		gtk_notebook_get_nth_page( GTK_NOTEBOOK( impldif_dlg.notebook ), pageNum ),
		label );

	table = gtk_grid_new();
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
	gtk_container_set_border_width( GTK_CONTAINER(table), 8 );
	gtk_grid_set_row_spacing(GTK_GRID(table), 8);
	gtk_grid_set_column_spacing(GTK_GRID(table), 8);

	/* First row */
	label = gtk_label_new( _( "Address Book:" ) );
	gtk_label_set_xalign(GTK_LABEL(label), 1.0);
	gtk_grid_attach(GTK_GRID(table), label, 0, 0, 1, 1);

	labelBook = gtk_label_new("");
	gtk_label_set_xalign(GTK_LABEL(labelBook), 0.0);
	gtk_grid_attach(GTK_GRID(table), labelBook, 1, 0, 1, 1);

	/* Second row */
	label = gtk_label_new( _( "File Name:" ) );
	gtk_label_set_xalign(GTK_LABEL(label), 1.0);
	gtk_grid_attach(GTK_GRID(table), label, 0, 1, 1, 1);

	labelFile = gtk_label_new("");
	gtk_label_set_xalign(GTK_LABEL(labelFile), 0.0);
	gtk_grid_attach(GTK_GRID(table), labelFile, 1, 1, 1, 1);

	/* Third row */
	label = gtk_label_new( _("Records Imported:") );
	gtk_label_set_xalign(GTK_LABEL(label), 1.0);
	gtk_grid_attach(GTK_GRID(table), label, 0, 2, 1, 1);

	labelRecs = gtk_label_new("");
	gtk_label_set_xalign(GTK_LABEL(labelRecs), 0.0);
	gtk_grid_attach(GTK_GRID(table), labelRecs, 1, 2, 1, 1);

	impldif_dlg.labelBook    = labelBook;
	impldif_dlg.labelFile    = labelFile;
	impldif_dlg.labelRecords = labelRecs;
}

/**
 * Create main dialog decorations (excluding notebook pages).
 */
static void imp_ldif_dialog_create() {
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *vnbox;
	GtkWidget *notebook;
	GtkWidget *hbbox;
	GtkWidget *btnPrev;
	GtkWidget *btnNext;
	GtkWidget *btnProceed;
	GtkWidget *btnCancel;
	GtkWidget *hsbox;
	GtkWidget *statusbar;

	window = gtkut_window_new(GTK_WINDOW_TOPLEVEL, "importldif");
	gtk_widget_set_size_request(window, IMPORTLDIF_WIDTH, IMPORTLDIF_HEIGHT );
	gtk_container_set_border_width( GTK_CONTAINER(window), 0 );
	gtk_window_set_title( GTK_WINDOW(window), _("Import LDIF file into Address Book") );
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DIALOG);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(imp_ldif_delete_event),
			 NULL );
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(imp_ldif_key_pressed),
			 NULL );

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	gtk_widget_show(vbox);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	vnbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	gtk_container_set_border_width(GTK_CONTAINER(vnbox), 4);
	gtk_widget_show(vnbox);
	gtk_box_pack_start(GTK_BOX(vbox), vnbox, TRUE, TRUE, 0);

	/* Notebook */
	notebook = gtk_notebook_new();
	gtk_notebook_set_show_tabs( GTK_NOTEBOOK(notebook), FALSE );
	gtk_widget_show(notebook);
	gtk_box_pack_start(GTK_BOX(vnbox), notebook, TRUE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(notebook), 6);

	/* Status line */
	hsbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_end(GTK_BOX(vbox), hsbox, FALSE, FALSE, BORDER_WIDTH);
	statusbar = gtk_statusbar_new();
	gtk_box_pack_start(GTK_BOX(hsbox), statusbar, TRUE, TRUE, BORDER_WIDTH);

	/* Button panel */
	gtkut_stock_button_set_create(&hbbox,
					&btnPrev, "go-previous", _("_Previous"),
					&btnNext, "go-next", _("_Next"),
					&btnCancel, NULL, _("_Cancel"));

	btnProceed = gtk_button_new_with_mnemonic(_("Proceed"));
	gtk_widget_set_can_default(btnProceed, TRUE);
	gtk_box_pack_start(GTK_BOX(hbbox), btnProceed, TRUE, TRUE, 0);
	gtk_widget_hide(btnProceed);

	gtk_box_pack_end(GTK_BOX(vbox), hbbox, FALSE, FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(hbbox), 2);
	gtk_widget_grab_default(btnNext);

	/* Button handlers */
	g_signal_connect(G_OBJECT(btnPrev), "clicked",
			 G_CALLBACK(imp_ldif_prev), NULL);
	g_signal_connect(G_OBJECT(btnNext), "clicked",
			 G_CALLBACK(imp_ldif_next), NULL);
	g_signal_connect(G_OBJECT(btnProceed), "clicked",
			 G_CALLBACK(imp_ldif_next), NULL);
	g_signal_connect(G_OBJECT(btnCancel), "clicked",
			 G_CALLBACK(imp_ldif_cancel), NULL);

	gtk_widget_show_all(vbox);

	impldif_dlg.window     = window;
	impldif_dlg.notebook   = notebook;
	impldif_dlg.btnPrev    = btnPrev;
	impldif_dlg.btnNext    = btnNext;
	impldif_dlg.btnProceed = btnProceed;
	impldif_dlg.btnCancel  = btnCancel;
	impldif_dlg.statusbar  = statusbar;
	impldif_dlg.status_cid = gtk_statusbar_get_context_id(
			GTK_STATUSBAR(statusbar), "Import LDIF Dialog" );

}

/**
 * Create import LDIF dialog.
 */
static void imp_ldif_create() {
	imp_ldif_dialog_create();
	imp_ldif_page_file( PAGE_FILE_INFO, _( "File Info" ) );
	imp_ldif_page_fields( PAGE_ATTRIBUTES, _( "Attributes" ) );
	imp_ldif_page_finish( PAGE_FINISH, _( "Finish" ) );
	gtk_widget_show_all( impldif_dlg.window );
}

/**
 * Import LDIF file.
 * \param  addrIndex Address index.
 * \return Address book file of imported data, or <i>NULL</i> if import
 *         was cancelled.
 */
AddressBookFile *addressbook_imp_ldif( AddressIndex *addrIndex ) {
	GtkWidget *view;
	GtkTreeModel *model;

	_importedBook_ = NULL;
	_imp_addressIndex_ = addrIndex;

	if( ! impldif_dlg.window )
		imp_ldif_create();

	view = impldif_dlg.view_fields;
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));

	gtk_button_set_label(GTK_BUTTON(impldif_dlg.btnCancel),
			     _("_Cancel"));
	gtk_widget_hide(impldif_dlg.btnProceed);
	gtk_widget_show(impldif_dlg.btnNext);

	impldif_dlg.cancelled = FALSE;
	gtk_widget_show(impldif_dlg.window);
	manage_window_set_transient(GTK_WINDOW(impldif_dlg.window));
	gtk_widget_grab_default(impldif_dlg.btnNext);
	gtk_window_set_modal(GTK_WINDOW(impldif_dlg.window), TRUE);

	gtk_entry_set_text( GTK_ENTRY(impldif_dlg.entryName), IMPORTLDIF_GUESS_NAME );
	gtk_entry_set_text( GTK_ENTRY(impldif_dlg.entryFile), "" );
	gtk_label_set_text( GTK_LABEL(impldif_dlg.entryField), "" );
	gtk_entry_set_text( GTK_ENTRY(impldif_dlg.entryAttrib), "" );
	gtk_list_store_clear( GTK_LIST_STORE(model) );
	gtk_notebook_set_current_page( GTK_NOTEBOOK(impldif_dlg.notebook), PAGE_FILE_INFO );
	gtk_widget_set_sensitive( impldif_dlg.btnPrev, FALSE );
	gtk_widget_set_sensitive( impldif_dlg.btnNext, TRUE );
	stock_pixbuf_gdk(STOCK_PIXMAP_MARK, &markxpm );
	imp_ldif_message();
	gtk_widget_grab_focus(impldif_dlg.entryFile);

	impldif_dlg.rowCount = 0;
	g_free( impldif_dlg.nameBook );
	g_free( impldif_dlg.fileName );
	impldif_dlg.nameBook = NULL;
	impldif_dlg.fileName = NULL;

	_ldifFile_ = ldif_create();
	gtk_main();
	gtk_widget_hide(impldif_dlg.window);
	gtk_window_set_modal(GTK_WINDOW(impldif_dlg.window), FALSE);
	ldif_free( _ldifFile_ );
	_ldifFile_ = NULL;
	_imp_addressIndex_ = NULL;

	g_free( impldif_dlg.nameBook );
	g_free( impldif_dlg.fileName );
	impldif_dlg.nameBook = NULL;
	impldif_dlg.fileName = NULL;

	if( impldif_dlg.cancelled == TRUE ) return NULL;
	return _importedBook_;
}

/*
 * ============================================================================
 * End of Source.
 * ============================================================================
 */

