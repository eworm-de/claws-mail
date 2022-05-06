/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2002-2022 Match Grun and the Claws Mail team
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
 * Export address book to HTML file.
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
#include "prefs_common.h"
#include "alertpanel.h"
#include "mgutils.h"
#include "addrcache.h"
#include "addressitem.h"
#include "exporthtml.h"
#include "utils.h"
#include "manage_window.h"
#include "filesel.h"
#include "combobox.h"

#define PAGE_FILE_INFO             0
#define PAGE_FORMAT                1
#define PAGE_FINISH                2

/**
 * Dialog object.
 */
static struct _ExpHtml_Dlg {
	GtkWidget *window;
	GtkWidget *notebook;
	GtkWidget *labelBook;
	GtkWidget *entryHtml;
	GtkWidget *optmenuCSS;
	GtkWidget *optmenuName;
	GtkWidget *checkBanding;
	GtkWidget *checkLinkEMail;
	GtkWidget *checkAttributes;
	GtkWidget *labelOutBook;
	GtkWidget *labelOutFile;
	GtkWidget *btnPrev;
	GtkWidget *btnNext;
	GtkWidget *btnCancel;
	GtkWidget *statusbar;
	gint      status_cid;
	gboolean  cancelled;
} exphtml_dlg;

static struct _AddressFileSelection _exp_html_file_selector_;

static ExportHtmlCtl *_exportCtl_ = NULL;
static AddressCache *_addressCache_ = NULL;

/**
 * Display message in status field.
 * \param msg Message to display.
 */
static void export_html_status_show( gchar *msg ) {
	if( exphtml_dlg.statusbar != NULL ) {
		gtk_statusbar_pop(
			GTK_STATUSBAR(exphtml_dlg.statusbar),
			exphtml_dlg.status_cid );
		if( msg ) {
			gtk_statusbar_push(
				GTK_STATUSBAR(exphtml_dlg.statusbar),
				exphtml_dlg.status_cid, msg );
		}
	}
}

/**
 * Select and display status message appropriate for the page being displayed.
 */
static void export_html_message( void ) {
	gchar *sMsg = NULL;
	gint pageNum;

	pageNum = gtk_notebook_get_current_page( GTK_NOTEBOOK(exphtml_dlg.notebook) );
	if( pageNum == PAGE_FILE_INFO ) {
		sMsg = _( "Please specify output directory and file to create." );
	}
	else if( pageNum == PAGE_FORMAT ) {
		sMsg = _( "Select stylesheet and formatting." );
	}
	else if( pageNum == PAGE_FINISH ) {
		sMsg = _( "File exported successfully." );
	}
	export_html_status_show( sMsg );
}

/**
 * Callback function to cancel HTML file selection dialog.
 * \param widget Widget (button).
 * \param data   User data.
 */
static void export_html_cancel( GtkWidget *widget, gpointer data ) {
	gint pageNum;

	pageNum = gtk_notebook_get_current_page( GTK_NOTEBOOK(exphtml_dlg.notebook) );
	if( pageNum != PAGE_FINISH ) {
		exphtml_dlg.cancelled = TRUE;
	}
	gtk_main_quit();
}

/**
 * Callback function to handle dialog close event.
 * \param widget Widget (dialog).
 * \param event  Event object.
 * \param data   User data.
 */
static gint export_html_delete_event( GtkWidget *widget, GdkEventAny *event, gpointer data ) {
	export_html_cancel( widget, data );
	return TRUE;
}

/**
 * Callback function to respond to dialog key press events.
 * \param widget Widget.
 * \param event  Event object.
 * \param data   User data.
 */
static gboolean export_html_key_pressed( GtkWidget *widget, GdkEventKey *event, gpointer data ) {
	if (event && event->keyval == GDK_KEY_Escape) {
		export_html_cancel( widget, data );
	}
	return FALSE;
}

/**
 * Test whether we can move off file page.
 * \return <i>TRUE</i> if OK to move off page.
 */
static gboolean exp_html_move_file( void ) {
	gchar *sFile, *msg, *reason;
	AlertValue aval;

	sFile = gtk_editable_get_chars( GTK_EDITABLE(exphtml_dlg.entryHtml), 0, -1 );
	g_strstrip( sFile );
	gtk_entry_set_text( GTK_ENTRY(exphtml_dlg.entryHtml), sFile );
	exporthtml_parse_filespec( _exportCtl_, sFile );
	g_free( sFile );

	/* Test for directory */
	if( g_file_test(_exportCtl_->dirOutput,
				G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR) ) {
		return TRUE;
	}

	/* Prompt to create */
	msg = g_strdup_printf( _(
		"The HTML output directory '%s'\n" \
		"does not exist. Do you want to create it?" ),
		_exportCtl_->dirOutput );
	aval = alertpanel(_("Create directory" ),
			  msg, NULL, _("_No"), NULL, _("_Yes"), NULL, NULL,
			  ALERTFOCUS_FIRST );
	g_free( msg );
	if( aval != G_ALERTALTERNATE ) return FALSE;

	/* Create directory */
	if( ! exporthtml_create_dir( _exportCtl_ ) ) {
		reason = exporthtml_get_create_msg( _exportCtl_ );
		msg = g_strdup_printf( _(
			"Could not create output directory for HTML file:\n%s" ),
			reason );
		aval = alertpanel_full(_("Failed to Create Directory"), msg,
				       "window-close", _("_Close"), NULL, NULL,
				       NULL, NULL, ALERTFOCUS_FIRST, FALSE,
				       NULL, ALERT_ERROR);
		g_free( msg );
		return FALSE;
	}

	return TRUE;
}

/**
 * Test whether we can move off format page.
 * \return <i>TRUE</i> if OK to move off page.
 */
static gboolean exp_html_move_format( void ) {
	gboolean retVal = FALSE;
	gint id;

	/* Set stylesheet */
	id = combobox_get_active_data(GTK_COMBO_BOX(exphtml_dlg.optmenuCSS));
	exporthtml_set_stylesheet( _exportCtl_, id );

	/* Set name format */
	id = combobox_get_active_data(GTK_COMBO_BOX(exphtml_dlg.optmenuName));
	exporthtml_set_name_format( _exportCtl_, id );

	exporthtml_set_banding( _exportCtl_,
		gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON( exphtml_dlg.checkBanding ) ) );
	exporthtml_set_link_email( _exportCtl_,
		gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON( exphtml_dlg.checkLinkEMail ) ) );
	exporthtml_set_attributes( _exportCtl_,
		gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON( exphtml_dlg.checkAttributes ) ) );

	/* Process export */
	exporthtml_process( _exportCtl_, _addressCache_ );
	if( _exportCtl_->retVal == MGU_SUCCESS ) {
		retVal = TRUE;
	}
	else {
		export_html_status_show( _( "Error creating HTML file" ) );
	}
	return retVal;
}

/**
 * Display finish page.
 */
static void exp_html_finish_show( void ) {
	gtk_label_set_text( GTK_LABEL(exphtml_dlg.labelOutFile), _exportCtl_->path );
	gtk_widget_set_sensitive( exphtml_dlg.btnNext, FALSE );
	gtk_widget_grab_focus( exphtml_dlg.btnCancel );
}

/**
 * Callback function to select previous page.
 * \param widget Widget (button).
 */
static void export_html_prev( GtkWidget *widget ) {
	gint pageNum;

	pageNum = gtk_notebook_get_current_page( GTK_NOTEBOOK(exphtml_dlg.notebook) );
	if( pageNum == PAGE_FORMAT ) {
		/* Goto file page stuff */
		gtk_notebook_set_current_page(
			GTK_NOTEBOOK(exphtml_dlg.notebook), PAGE_FILE_INFO );
		gtk_widget_set_sensitive( exphtml_dlg.btnPrev, FALSE );
	}
	else if( pageNum == PAGE_FINISH ) {
		/* Goto format page */
		gtk_notebook_set_current_page(
			GTK_NOTEBOOK(exphtml_dlg.notebook), PAGE_FORMAT );
		gtk_widget_set_sensitive( exphtml_dlg.btnNext, TRUE );
	}
	export_html_message();
}

/**
 * Callback function to select previous page.
 * \param widget Widget (button).
 */
static void export_html_next( GtkWidget *widget ) {
	gint pageNum;

	pageNum = gtk_notebook_get_current_page( GTK_NOTEBOOK(exphtml_dlg.notebook) );
	if( pageNum == PAGE_FILE_INFO ) {
		/* Goto format page */
		if( exp_html_move_file() ) {
			gtk_notebook_set_current_page(
				GTK_NOTEBOOK(exphtml_dlg.notebook), PAGE_FORMAT );
			gtk_widget_set_sensitive( exphtml_dlg.btnPrev, TRUE );
		}
		export_html_message();
	}
	else if( pageNum == PAGE_FORMAT ) {
		/* Goto finish page */
		if( exp_html_move_format() ) {
			gtk_notebook_set_current_page(
				GTK_NOTEBOOK(exphtml_dlg.notebook), PAGE_FINISH );
			gtk_button_set_label(GTK_BUTTON(exphtml_dlg.btnCancel),
				_("_Close"));
			gtk_button_set_image(GTK_BUTTON(exphtml_dlg.btnCancel),
				gtk_image_new_from_icon_name("window-close", GTK_ICON_SIZE_BUTTON));

			exp_html_finish_show();
			exporthtml_save_settings( _exportCtl_ );
			export_html_message();
		}
	}
}

/**
 * Open file with web browser.
 * \param widget Widget (button).
 * \param data   User data.
 */
static void export_html_browse( GtkWidget *widget, gpointer data ) {
	gchar *uri;

	uri = g_filename_to_uri(_exportCtl_->path, NULL, NULL);
	open_uri( uri, prefs_common_get_uri_cmd() );
	g_free( uri );
}

/**
 * Create HTML file selection dialog.
 * \param afs Address file selection data.
 */
static void exp_html_file_select_create( AddressFileSelection *afs ) {
	gchar *file = filesel_select_file_save(_("Select HTML output file"), NULL);
	
	if (file == NULL)
		afs->cancelled = TRUE;
	else {
		afs->cancelled = FALSE;
		gtk_entry_set_text( GTK_ENTRY(exphtml_dlg.entryHtml), file );
		g_free(file);
	}
}

/**
 * Callback function to display HTML file selection dialog.
 */
static void exp_html_file_select( void ) {
	exp_html_file_select_create( & _exp_html_file_selector_ );
}

/**
 * Format notebook file specification page.
 * \param pageNum Page (tab) number.
 * \param pageLbl Page (tab) label.
 */
static void export_html_page_file( gint pageNum, gchar *pageLbl ) {
	GtkWidget *vbox;
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *labelBook;
	GtkWidget *entryHtml;
	GtkWidget *btnFile;

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_container_add( GTK_CONTAINER( exphtml_dlg.notebook ), vbox );
	gtk_container_set_border_width( GTK_CONTAINER (vbox), BORDER_WIDTH );

	label = gtk_label_new( pageLbl );
	gtk_widget_show( label );
	gtk_notebook_set_tab_label(
		GTK_NOTEBOOK( exphtml_dlg.notebook ),
		gtk_notebook_get_nth_page(
			GTK_NOTEBOOK( exphtml_dlg.notebook ), pageNum ),
		label );

	table = gtk_grid_new();
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
	gtk_container_set_border_width( GTK_CONTAINER(table), 8 );
	gtk_grid_set_row_spacing(GTK_GRID(table), 8);
	gtk_grid_set_column_spacing(GTK_GRID(table), 8);

	/* First row */
	label = gtk_label_new( _( "Address Book" ) );
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_grid_attach(GTK_GRID(table), label, 0, 0, 1, 1);

	labelBook = gtk_label_new( "Address book name goes here" );
	gtk_label_set_xalign(GTK_LABEL(labelBook), 0.0);
	gtk_grid_attach(GTK_GRID(table), labelBook, 1, 0, 1, 1);

	/* Second row */
	label = gtk_label_new( _( "HTML Output File" ) );
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_grid_attach(GTK_GRID(table), label, 0, 1, 1, 1);

	entryHtml = gtk_entry_new();
	gtk_grid_attach(GTK_GRID(table), entryHtml, 1, 1, 1, 1);
	gtk_widget_set_hexpand(entryHtml, TRUE);
	gtk_widget_set_halign(entryHtml, GTK_ALIGN_FILL);

	btnFile = gtkut_get_browse_file_btn(_("B_rowse"));
	gtk_grid_attach(GTK_GRID(table), btnFile, 2, 1, 1, 1);

	gtk_widget_show_all(vbox);

	/* Button handler */
	g_signal_connect(G_OBJECT(btnFile), "clicked",
			 G_CALLBACK(exp_html_file_select), NULL);

	exphtml_dlg.labelBook = labelBook;
	exphtml_dlg.entryHtml = entryHtml;
}

/**
 * Format notebook format page.
 * \param pageNum Page (tab) number.
 * \param pageLbl Page (tab) label.
 */
static void export_html_page_format( gint pageNum, gchar *pageLbl ) {
	GtkWidget *vbox;
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *optmenuCSS;
	GtkWidget *optmenuName;
	GtkListStore *menu;
	GtkTreeIter iter;
	GtkWidget *checkBanding;
	GtkWidget *checkLinkEMail;
	GtkWidget *checkAttributes;

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_container_add( GTK_CONTAINER( exphtml_dlg.notebook ), vbox );
	gtk_container_set_border_width( GTK_CONTAINER (vbox), BORDER_WIDTH );

	label = gtk_label_new( pageLbl );
	gtk_widget_show( label );
	gtk_notebook_set_tab_label(
		GTK_NOTEBOOK( exphtml_dlg.notebook ),
		gtk_notebook_get_nth_page(
			GTK_NOTEBOOK( exphtml_dlg.notebook ), pageNum ),
		label );

	table = gtk_grid_new();
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
	gtk_container_set_border_width( GTK_CONTAINER(table), 8 );
	gtk_grid_set_row_spacing(GTK_GRID(table), 8);
	gtk_grid_set_column_spacing(GTK_GRID(table), 8);

	/* First row */
	label = gtk_label_new( _( "Stylesheet" ) );
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_grid_attach(GTK_GRID(table), label, 0, 0, 1, 1);

	optmenuCSS = gtkut_sc_combobox_create(NULL, TRUE);
	menu = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(optmenuCSS)));

	COMBOBOX_ADD(menu, _("None"), EXPORT_HTML_ID_NONE);
	COMBOBOX_ADD(menu, _("Default"), EXPORT_HTML_ID_DEFAULT);
	COMBOBOX_ADD(menu, _("Full"), EXPORT_HTML_ID_FULL);
	COMBOBOX_ADD(menu, _("Custom"), EXPORT_HTML_ID_CUSTOM);
	COMBOBOX_ADD(menu, _("Custom-2"), EXPORT_HTML_ID_CUSTOM2);
	COMBOBOX_ADD(menu, _("Custom-3"), EXPORT_HTML_ID_CUSTOM3);
	COMBOBOX_ADD(menu, _("Custom-4"), EXPORT_HTML_ID_CUSTOM4);

	gtk_grid_attach(GTK_GRID(table), optmenuCSS, 1, 0, 1, 1);

	/* Second row */
	label = gtk_label_new( _( "Full Name Format" ) );
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_grid_attach(GTK_GRID(table), label, 0, 1, 1, 1);

	optmenuName = gtkut_sc_combobox_create(NULL, TRUE);
	menu = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(optmenuName)));

	COMBOBOX_ADD(menu, _("First Name, Last Name"), EXPORT_HTML_FIRST_LAST);
	COMBOBOX_ADD(menu, _("Last Name, First Name"), EXPORT_HTML_LAST_FIRST);

	gtk_grid_attach(GTK_GRID(table), optmenuName, 1, 1, 1, 1);

	/* Third row */
	checkBanding = gtk_check_button_new_with_label( _( "Color Banding" ) );
	gtk_grid_attach(GTK_GRID(table), checkBanding, 1, 2, 1, 1);

	/* Fourth row */
	checkLinkEMail = gtk_check_button_new_with_label( _( "Format Email Links" ) );
	gtk_grid_attach(GTK_GRID(table), checkLinkEMail, 1, 3, 1, 1);

	/* Fifth row */
	checkAttributes = gtk_check_button_new_with_label( _( "Format User Attributes" ) );
	gtk_grid_attach(GTK_GRID(table), checkAttributes, 1, 4, 1, 1);

	gtk_widget_show_all(vbox);

	exphtml_dlg.optmenuCSS      = optmenuCSS;
	exphtml_dlg.optmenuName     = optmenuName;
	exphtml_dlg.checkBanding    = checkBanding;
	exphtml_dlg.checkLinkEMail  = checkLinkEMail;
	exphtml_dlg.checkAttributes = checkAttributes;
}

/**
 * Format notebook finish page.
 * \param pageNum Page (tab) number.
 * \param pageLbl Page (tab) label.
 */
static void export_html_page_finish( gint pageNum, gchar *pageLbl ) {
	GtkWidget *vbox;
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *labelBook;
	GtkWidget *labelFile;
	GtkWidget *btnBrowse;

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_container_add( GTK_CONTAINER( exphtml_dlg.notebook ), vbox );
	gtk_container_set_border_width( GTK_CONTAINER (vbox), BORDER_WIDTH );

	label = gtk_label_new( pageLbl );
	gtk_widget_show( label );
	gtk_notebook_set_tab_label(
		GTK_NOTEBOOK( exphtml_dlg.notebook ),
		gtk_notebook_get_nth_page( GTK_NOTEBOOK( exphtml_dlg.notebook ), pageNum ), label );

	table = gtk_grid_new();
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
	gtk_container_set_border_width( GTK_CONTAINER(table), 8 );
	gtk_grid_set_row_spacing(GTK_GRID(table), 8);
	gtk_grid_set_column_spacing(GTK_GRID(table), 8);

	/* First row */
	label = gtk_label_new( _( "Address Book:" ) );
	gtk_label_set_xalign(GTK_LABEL(label), 1.0);
	gtk_grid_attach(GTK_GRID(table), label, 0, 0, 1, 1);

	labelBook = gtk_label_new("Full name of address book goes here");
	gtk_label_set_xalign(GTK_LABEL(labelBook), 0.0);
	gtk_grid_attach(GTK_GRID(table), labelBook, 1, 0, 1, 1);

	/* Second row */
	label = gtk_label_new( _( "File Name:" ) );
	gtk_label_set_xalign(GTK_LABEL(label), 1.0);
	gtk_grid_attach(GTK_GRID(table), label, 0, 1, 1, 1);

	labelFile = gtk_label_new("File name goes here");
	gtk_label_set_xalign(GTK_LABEL(labelFile), 0.0);
	gtk_grid_attach(GTK_GRID(table), labelFile, 1, 1, 1, 1);

	/* Third row */
	btnBrowse = gtk_button_new_with_label( _( "Open with Web Browser" ) );
	gtk_grid_attach(GTK_GRID(table), btnBrowse, 1, 2, 1, 1);

	gtk_widget_show_all(vbox);

	/* Button handlers */
	g_signal_connect(G_OBJECT(btnBrowse), "clicked",
			 G_CALLBACK(export_html_browse), NULL);

	exphtml_dlg.labelOutBook = labelBook;
	exphtml_dlg.labelOutFile = labelFile;
}

/**
 * Create main dialog decorations (excluding notebook pages).
 */
static void export_html_dialog_create( void ) {
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *vnbox;
	GtkWidget *notebook;
	GtkWidget *hbbox;
	GtkWidget *btnPrev;
	GtkWidget *btnNext;
	GtkWidget *btnCancel;
	GtkWidget *hsbox;
	GtkWidget *statusbar;

	window = gtkut_window_new(GTK_WINDOW_TOPLEVEL, "exphtmldlg");
	gtk_widget_set_size_request(window, -1, -1 );
	gtk_container_set_border_width( GTK_CONTAINER(window), 0 );
	gtk_window_set_title( GTK_WINDOW(window),
		_("Export Address Book to HTML File") );
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DIALOG);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(export_html_delete_event),
			 NULL );
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(export_html_key_pressed),
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
	gtk_notebook_set_show_tabs( GTK_NOTEBOOK(notebook), FALSE ); /* Hide */
	/* gtk_notebook_set_show_tabs( GTK_NOTEBOOK(notebook), TRUE ); */
	gtk_widget_show(notebook);
	gtk_box_pack_start(GTK_BOX(vnbox), notebook, TRUE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(notebook), 6);

	/* Status line */
	hsbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_end(GTK_BOX(vbox), hsbox, FALSE, FALSE, BORDER_WIDTH);
	statusbar = gtk_statusbar_new();
	gtk_box_pack_start(GTK_BOX(hsbox), statusbar, TRUE, TRUE, BORDER_WIDTH);

	/* Button panel */
	gtkut_stock_button_set_create(&hbbox, &btnPrev, "go-previous", _("_Previous"),
				      &btnNext, "go-next", _("_Next"),
				      &btnCancel, NULL, _("_Cancel"));
	gtk_box_pack_end(GTK_BOX(vbox), hbbox, FALSE, FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(hbbox), 2);
	gtk_widget_grab_default(btnNext);

	/* Button handlers */
	g_signal_connect(G_OBJECT(btnPrev), "clicked",
			 G_CALLBACK(export_html_prev), NULL);
	g_signal_connect(G_OBJECT(btnNext), "clicked",
			 G_CALLBACK(export_html_next), NULL);
	g_signal_connect(G_OBJECT(btnCancel), "clicked",
			 G_CALLBACK(export_html_cancel), NULL);

	gtk_widget_show_all(vbox);

	exphtml_dlg.window     = window;
	exphtml_dlg.notebook   = notebook;
	exphtml_dlg.btnPrev    = btnPrev;
	exphtml_dlg.btnNext    = btnNext;
	exphtml_dlg.btnCancel  = btnCancel;
	exphtml_dlg.statusbar  = statusbar;
	exphtml_dlg.status_cid = gtk_statusbar_get_context_id(
			GTK_STATUSBAR(statusbar), "Export HTML Dialog" );
}

/**
 * Create export HTML dialog.
 */
static void export_html_create( void ) {
	export_html_dialog_create();
	export_html_page_file( PAGE_FILE_INFO, _( "File Info" ) );
	export_html_page_format( PAGE_FORMAT, _( "Format" ) );
	export_html_page_finish( PAGE_FINISH, _( "Finish" ) );
	gtk_widget_show_all( exphtml_dlg.window );
}

/**
 * Populate fields from control data.
 * \param ctl Export control data.
 */
static void export_html_fill_fields( ExportHtmlCtl *ctl ) {
	gtk_entry_set_text( GTK_ENTRY(exphtml_dlg.entryHtml), "" );
	if( ctl->path ) {
		gtk_entry_set_text( GTK_ENTRY(exphtml_dlg.entryHtml),
			ctl->path );
	}

	combobox_select_by_data(
			GTK_COMBO_BOX(exphtml_dlg.optmenuCSS), ctl->stylesheet );
	combobox_select_by_data(
			GTK_COMBO_BOX(exphtml_dlg.optmenuName), ctl->nameFormat );
	gtk_toggle_button_set_active(
		GTK_TOGGLE_BUTTON( exphtml_dlg.checkBanding ), ctl->banding );
	gtk_toggle_button_set_active(
		GTK_TOGGLE_BUTTON( exphtml_dlg.checkLinkEMail ), ctl->linkEMail );
	gtk_toggle_button_set_active(
		GTK_TOGGLE_BUTTON( exphtml_dlg.checkAttributes ), ctl->showAttribs );
}

/**
 * Process export address dialog.
 * \param cache Address book/data source cache.
 */
void addressbook_exp_html( AddressCache *cache ) {
	/* Set references to control data */
	_addressCache_ = cache;

	_exportCtl_ = exporthtml_create();
	exporthtml_load_settings( _exportCtl_ );

	/* Setup GUI */
	if( ! exphtml_dlg.window )
		export_html_create();

	gtk_button_set_label(GTK_BUTTON(exphtml_dlg.btnCancel),
			     _("_Cancel"));
	exphtml_dlg.cancelled = FALSE;
	gtk_widget_show(exphtml_dlg.window);
	manage_window_set_transient(GTK_WINDOW(exphtml_dlg.window));
	gtk_window_set_modal(GTK_WINDOW(exphtml_dlg.window), TRUE);
	gtk_label_set_text( GTK_LABEL(exphtml_dlg.labelBook), cache->name );
	gtk_label_set_text( GTK_LABEL(exphtml_dlg.labelOutBook), cache->name );
	export_html_fill_fields( _exportCtl_ );

	gtk_widget_grab_default(exphtml_dlg.btnNext);
	gtk_notebook_set_current_page( GTK_NOTEBOOK(exphtml_dlg.notebook), PAGE_FILE_INFO );
	gtk_widget_set_sensitive( exphtml_dlg.btnPrev, FALSE );
	gtk_widget_set_sensitive( exphtml_dlg.btnNext, TRUE );

	export_html_message();
	gtk_widget_grab_focus(exphtml_dlg.entryHtml);

	gtk_main();
	gtk_widget_hide(exphtml_dlg.window);
	gtk_window_set_modal(GTK_WINDOW(exphtml_dlg.window), FALSE);
	exporthtml_free( _exportCtl_ );
	_exportCtl_ = NULL;

	_addressCache_ = NULL;
}

/*
 * ============================================================================
 * End of Source.
 * ============================================================================
 */

