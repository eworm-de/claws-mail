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
 * Edit vCard address book data.
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

#include "addressbook.h"
#include "prefs_common.h"
#include "addressitem.h"
#include "vcard.h"
#include "mgutils.h"
#include "filesel.h"
#include "gtkutils.h"
#include "codeconv.h"
#include "manage_window.h"

#define ADDRESSBOOK_GUESS_VCARD  "MyGnomeCard"

static struct _VCardEdit {
	GtkWidget *window;
	GtkWidget *name_entry;
	GtkWidget *file_entry;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;
	GtkWidget *statusbar;
	gint status_cid;
} vcardedit;

/*
* Edit functions.
*/
static void edit_vcard_status_show( gchar *msg ) {
	if( vcardedit.statusbar != NULL ) {
		gtk_statusbar_pop( GTK_STATUSBAR(vcardedit.statusbar), vcardedit.status_cid );
		if( msg ) {
			gtk_statusbar_push( GTK_STATUSBAR(vcardedit.statusbar), vcardedit.status_cid, msg );
		}
	}
}

static void edit_vcard_ok( GtkWidget *widget, gboolean *cancelled ) {
	*cancelled = FALSE;
	gtk_main_quit();
}

static void edit_vcard_cancel( GtkWidget *widget, gboolean *cancelled ) {
	*cancelled = TRUE;
	gtk_main_quit();
}

static void edit_vcard_file_check( void ) {
	gint t;
	gchar *sFile;
	gchar *sFSFile;
	gchar *sMsg;

	sFile = gtk_editable_get_chars( GTK_EDITABLE(vcardedit.file_entry), 0, -1 );
	sFSFile = conv_filename_from_utf8( sFile );
	t = vcard_test_read_file( sFSFile );
	g_free( sFSFile );
	g_free( sFile );
	if( t == MGU_SUCCESS ) {
		sMsg = "";
	}
	else if( t == MGU_BAD_FORMAT ) {
		sMsg = _("File does not appear to be vCard format.");
	}
	else {
		sMsg = _("Could not read file.");
	}
	edit_vcard_status_show( sMsg );
}

static void edit_vcard_file_select( void ) {
	gchar *sFile;
	gchar *sUTF8File;

	sFile = filesel_select_file_open( _("Select vCard File"), NULL);

	if( sFile ) {
		sUTF8File = conv_filename_to_utf8( sFile );
		gtk_entry_set_text( GTK_ENTRY(vcardedit.file_entry), sUTF8File );
		g_free( sUTF8File );
		g_free( sFile );
		edit_vcard_file_check();
	}
}

static gint edit_vcard_delete_event( GtkWidget *widget, GdkEventAny *event, gboolean *cancelled ) {
	*cancelled = TRUE;
	gtk_main_quit();
	return TRUE;
}

static gboolean edit_vcard_key_pressed( GtkWidget *widget, GdkEventKey *event, gboolean *cancelled ) {
	if (event && event->keyval == GDK_KEY_Escape) {
		*cancelled = TRUE;
		gtk_main_quit();
	}
	return FALSE;
}

static void addressbook_edit_vcard_create( gboolean *cancelled ) {
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *name_entry;
	GtkWidget *file_entry;
	GtkWidget *hbbox;
	GtkWidget *hsep;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;
	GtkWidget *check_btn;
	GtkWidget *file_btn;
	GtkWidget *statusbar;
	GtkWidget *hsbox;

	window = gtkut_window_new(GTK_WINDOW_TOPLEVEL, "editvcard");
	gtk_widget_set_size_request(window, 450, -1);
	gtk_container_set_border_width( GTK_CONTAINER(window), 0 );
	gtk_window_set_title(GTK_WINDOW(window), _("Edit vCard Entry"));
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DIALOG);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(edit_vcard_delete_event),
			 cancelled);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(edit_vcard_key_pressed),
			 cancelled);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_container_add(GTK_CONTAINER(window), vbox);
	gtk_container_set_border_width( GTK_CONTAINER(vbox), 0 );

	table = gtk_grid_new();
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
	gtk_container_set_border_width( GTK_CONTAINER(table), 8 );
	gtk_grid_set_row_spacing(GTK_GRID(table), 8);
	gtk_grid_set_column_spacing(GTK_GRID(table), 8);

	/* First row */
	label = gtk_label_new(_("Name"));
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_grid_attach(GTK_GRID(table), label, 0, 0, 1, 1);

	name_entry = gtk_entry_new();
	gtk_grid_attach(GTK_GRID(table), name_entry, 1, 0, 1, 1);
	gtk_widget_set_hexpand(name_entry, TRUE);
	gtk_widget_set_halign(name_entry, GTK_ALIGN_FILL);

	check_btn = gtk_button_new_with_label( _(" Check File "));
	gtk_grid_attach(GTK_GRID(table), check_btn, 2, 0, 1, 1);

	/* Second row */
	label = gtk_label_new(_("File"));
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_grid_attach(GTK_GRID(table), label, 0, 1, 1, 1);

	file_entry = gtk_entry_new();
	gtk_grid_attach(GTK_GRID(table), file_entry, 1, 1, 1, 1);
	gtk_widget_set_hexpand(file_entry, TRUE);
	gtk_widget_set_halign(file_entry, GTK_ALIGN_FILL);

	file_btn = gtkut_get_browse_file_btn(_("_Browse"));
	gtk_grid_attach(GTK_GRID(table), file_btn, 2, 1, 1, 1);

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
			 G_CALLBACK(edit_vcard_ok), cancelled);
	g_signal_connect(G_OBJECT(cancel_btn), "clicked",
			 G_CALLBACK(edit_vcard_cancel), cancelled);
	g_signal_connect(G_OBJECT(file_btn), "clicked",
			 G_CALLBACK(edit_vcard_file_select), NULL);
	g_signal_connect(G_OBJECT(check_btn), "clicked",
			 G_CALLBACK(edit_vcard_file_check), NULL);

	gtk_widget_show_all(vbox);

	vcardedit.window     = window;
	vcardedit.name_entry = name_entry;
	vcardedit.file_entry = file_entry;
	vcardedit.ok_btn     = ok_btn;
	vcardedit.cancel_btn = cancel_btn;
	vcardedit.statusbar  = statusbar;
	vcardedit.status_cid = gtk_statusbar_get_context_id( GTK_STATUSBAR(statusbar), "Edit vCard Dialog" );
}

AdapterDSource *addressbook_edit_vcard( AddressIndex *addrIndex, AdapterDSource *ads ) {
	static gboolean cancelled;
	gchar *sName;
	gchar *sFile;
	gchar *sFSFile;
	AddressDataSource *ds = NULL;
	VCardFile *vcf = NULL;
	gboolean fin;

	if( ! vcardedit.window )
		addressbook_edit_vcard_create(&cancelled);
	gtk_widget_grab_focus(vcardedit.ok_btn);
	gtk_widget_grab_focus(vcardedit.name_entry);
	gtk_widget_show(vcardedit.window);
	manage_window_set_transient(GTK_WINDOW(vcardedit.window));
	gtk_window_set_modal(GTK_WINDOW(vcardedit.window), TRUE);
	edit_vcard_status_show( "" );
	if( ads ) {
		ds = ads->dataSource;
		vcf = ds->rawDataSource;
		if ( vcard_get_name( vcf ) )
			gtk_entry_set_text(GTK_ENTRY(vcardedit.name_entry), vcard_get_name( vcf ) );
		if (vcf->path)
			gtk_entry_set_text(GTK_ENTRY(vcardedit.file_entry), vcf->path);
		gtk_window_set_title( GTK_WINDOW(vcardedit.window), _("Edit vCard Entry"));
	}
	else {
		gtk_entry_set_text(GTK_ENTRY(vcardedit.name_entry), ADDRESSBOOK_GUESS_VCARD );
		gtk_entry_set_text(GTK_ENTRY(vcardedit.file_entry), vcard_find_gnomecard() );
		gtk_window_set_title( GTK_WINDOW(vcardedit.window), _("Add New vCard Entry"));
	}

	gtk_main();
	gtk_widget_hide(vcardedit.window);
	gtk_window_set_modal(GTK_WINDOW(vcardedit.window), FALSE);
	if (cancelled == TRUE) return NULL;

	fin = FALSE;
	sName = gtk_editable_get_chars( GTK_EDITABLE(vcardedit.name_entry), 0, -1 );
	sFile = gtk_editable_get_chars( GTK_EDITABLE(vcardedit.file_entry), 0, -1 );
	sFSFile = conv_filename_from_utf8( sFile );
	if( *sName == '\0' ) fin = TRUE;
	if( *sFile == '\0' ) fin = TRUE;
	if( ! sFSFile || *sFSFile == '\0' ) fin = TRUE;

	if( ! fin ) {
		if( ! ads ) {
			vcf = vcard_create();
			ds = addrindex_index_add_datasource( addrIndex, ADDR_IF_VCARD, vcf );
			ads = addressbook_create_ds_adapter( ds, ADDR_VCARD, NULL );
		}
		addressbook_ads_set_name( ads, sName );
		vcard_set_name( vcf, sName );
		vcard_set_file( vcf, sFSFile );
	}
	g_free( sFSFile );
	g_free( sFile );
	g_free( sName );

	return ads;
}

/*
* End of Source.
*/

