/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2003-2025 the Claws Mail team and Match Grun
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
 * Export address book to LDIF file.
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
#include "exportldif.h"
#include "utils.h"
#include "manage_window.h"
#include "filesel.h"
#include "combobox.h"

#define PAGE_FILE_INFO             0
#define PAGE_DN                    1
#define PAGE_FINISH                2

#define EXPORTLDIF_WIDTH           480
#define EXPORTLDIF_HEIGHT          -1

/**
 * Dialog object.
 */
static struct _ExpLdif_Dlg {
	GtkWidget *window;
	GtkWidget *notebook;
	GtkWidget *labelBook;
	GtkWidget *entryLdif;
	GtkWidget *entrySuffix;
	GtkWidget *optmenuRDN;
	GtkWidget *checkUseDN;
	GtkWidget *checkEMail;
	GtkWidget *labelOutBook;
	GtkWidget *labelOutFile;
	GtkWidget *btnPrev;
	GtkWidget *btnNext;
	GtkWidget *btnCancel;
	GtkWidget *statusbar;
	gint      status_cid;
	gboolean  cancelled;
} expldif_dlg;

static struct _AddressFileSelection _exp_ldif_file_selector_;

static ExportLdifCtl *_exportCtl_ = NULL;
static AddressCache *_addressCache_ = NULL;

/**
 * Display message in status field.
 * \param msg Message to display.
 */
static void export_ldif_status_show( gchar *msg ) {
	if( expldif_dlg.statusbar != NULL ) {
		gtk_statusbar_pop(
			GTK_STATUSBAR(expldif_dlg.statusbar),
			expldif_dlg.status_cid );
		if( msg ) {
			gtk_statusbar_push(
				GTK_STATUSBAR(expldif_dlg.statusbar),
				expldif_dlg.status_cid, msg );
		}
	}
}

/**
 * Select and display status message appropriate for the page being displayed.
 */
static void export_ldif_message( void ) {
	gchar *sMsg = NULL;
	gint pageNum;

	pageNum = gtk_notebook_get_current_page( GTK_NOTEBOOK(expldif_dlg.notebook) );
	if( pageNum == PAGE_FILE_INFO ) {
		sMsg = _( "Please specify output directory and LDIF filename to create." );
	}
	else if( pageNum == PAGE_DN ) {
		sMsg = _( "Specify parameters to format distinguished name." );
	}
	else if( pageNum == PAGE_FINISH ) {
		sMsg = _( "File exported successfully." );
	}
	export_ldif_status_show( sMsg );
}

/**
 * Callback function to cancel LDIF file selection dialog.
 * \param widget Widget (button).
 * \param data   User data.
 */
static void export_ldif_cancel( GtkWidget *widget, gpointer data ) {
	gint pageNum;

	pageNum = gtk_notebook_get_current_page( GTK_NOTEBOOK(expldif_dlg.notebook) );
	if( pageNum != PAGE_FINISH ) {
		expldif_dlg.cancelled = TRUE;
	}
	gtk_main_quit();
}

/**
 * Callback function to handle dialog close event.
 * \param widget Widget (dialog).
 * \param event  Event object.
 * \param data   User data.
 */
static gint export_ldif_delete_event( GtkWidget *widget, GdkEventAny *event, gpointer data ) {
	export_ldif_cancel( widget, data );
	return TRUE;
}

/**
 * Callback function to respond to dialog key press events.
 * \param widget Widget.
 * \param event  Event object.
 * \param data   User data.
 */
static gboolean export_ldif_key_pressed( GtkWidget *widget, GdkEventKey *event, gpointer data ) {
	if (event && event->keyval == GDK_KEY_Escape) {
		export_ldif_cancel( widget, data );
	}
	return FALSE;
}

/**
 * Test whether we can move off file page.
 * \return <i>TRUE</i> if OK to move off page.
 */
static gboolean exp_ldif_move_file( void ) {
	gchar *sFile, *msg, *reason;
	gboolean errFlag = FALSE;
	AlertValue aval;

	sFile = gtk_editable_get_chars( GTK_EDITABLE(expldif_dlg.entryLdif), 0, -1 );
	g_strstrip( sFile );
	gtk_entry_set_text( GTK_ENTRY(expldif_dlg.entryLdif), sFile );
	exportldif_parse_filespec( _exportCtl_, sFile );

	/* Test that something was supplied */
	if( *sFile == '\0'|| strlen( sFile ) < 1 ) {
		gtk_widget_grab_focus( expldif_dlg.entryLdif );
		errFlag = TRUE;
	}
	g_free( sFile );
	if( errFlag ) return FALSE;

	/* Test for directory */
	if( g_file_test(_exportCtl_->dirOutput,
				G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR) ) {
		return TRUE;
	}

	/* Prompt to create */
	msg = g_strdup_printf( _(
		"LDIF Output Directory '%s'\n" \
		"does not exist. OK to create new directory?" ),
		_exportCtl_->dirOutput );
	aval = alertpanel( _("Create Directory" ),
		msg, NULL, _("_No"), NULL, _("_Yes"), NULL, NULL, ALERTFOCUS_FIRST );
	g_free( msg );
	if( aval != G_ALERTALTERNATE ) return FALSE;

	/* Create directory */
	if( ! exportldif_create_dir( _exportCtl_ ) ) {
		reason = exportldif_get_create_msg( _exportCtl_ );
		msg = g_strdup_printf( _(
			"Could not create output directory for LDIF file:\n%s" ),
			reason );
		aval = alertpanel_full(_("Failed to Create Directory"), msg,
				       NULL, _("Close"), NULL, NULL, NULL, NULL,
				       ALERTFOCUS_FIRST, FALSE, NULL, ALERT_ERROR);
		g_free( msg );
		return FALSE;
	}

	return TRUE;
}

/**
 * Test whether we can move off distinguished name page.
 * \return <i>TRUE</i> if OK to move off page.
 */
static gboolean exp_ldif_move_dn( void ) {
	gboolean retVal = FALSE;
	gboolean errFlag = FALSE;
	gchar *suffix;
	gint id;

	/* Set suffix */
	suffix = gtk_editable_get_chars( GTK_EDITABLE(expldif_dlg.entrySuffix), 0, -1 );
	g_strstrip( suffix );

	/* Set RDN format */
	id = combobox_get_active_data(GTK_COMBO_BOX(expldif_dlg.optmenuRDN));
	exportldif_set_rdn( _exportCtl_, id );

	exportldif_set_suffix( _exportCtl_, suffix );
	exportldif_set_use_dn( _exportCtl_,
		gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON( expldif_dlg.checkUseDN ) ) );
	exportldif_set_exclude_email( _exportCtl_,
		gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON( expldif_dlg.checkEMail ) ) );

	if( *suffix == '\0' || strlen( suffix ) < 1 ) {
		AlertValue aval;

		aval = alertpanel(
			_( "Suffix was not supplied" ),
			_(
				"A suffix is required if data is to be used " \
				"for an LDAP server. Are you sure you wish " \
				"to proceed without a suffix?"
			 ),
			 NULL, _("_No"), NULL, _("_Yes"), NULL, NULL, ALERTFOCUS_FIRST );
		if( aval != G_ALERTALTERNATE ) {
			gtk_widget_grab_focus( expldif_dlg.entrySuffix );
			errFlag = TRUE;
		}
	}

	if( ! errFlag ) {
		/* Process export */
		exportldif_process( _exportCtl_, _addressCache_ );
		if( _exportCtl_->retVal == MGU_SUCCESS ) {
			retVal = TRUE;
		}
		else {
			export_ldif_status_show( _( "Error creating LDIF file" ) );
		}
	}

	return retVal;
}

/**
 * Display finish page.
 */
static void exp_ldif_finish_show( void ) {
	gtk_label_set_text( GTK_LABEL(expldif_dlg.labelOutFile), _exportCtl_->path );
	gtk_widget_set_sensitive( expldif_dlg.btnNext, FALSE );
	gtk_widget_grab_focus( expldif_dlg.btnCancel );
}

/**
 * Callback function to select previous page.
 * \param widget Widget (button).
 */
static void export_ldif_prev( GtkWidget *widget ) {
	gint pageNum;

	pageNum = gtk_notebook_get_current_page( GTK_NOTEBOOK(expldif_dlg.notebook) );
	if( pageNum == PAGE_DN ) {
		/* Goto file page stuff */
		gtk_notebook_set_current_page(
			GTK_NOTEBOOK(expldif_dlg.notebook), PAGE_FILE_INFO );
		gtk_widget_set_sensitive( expldif_dlg.btnPrev, FALSE );
	}
	else if( pageNum == PAGE_FINISH ) {
		/* Goto format page */
		gtk_notebook_set_current_page(
			GTK_NOTEBOOK(expldif_dlg.notebook), PAGE_DN );
		gtk_widget_set_sensitive( expldif_dlg.btnNext, TRUE );
	}
	export_ldif_message();
}

/**
 * Callback function to select next page.
 * \param widget Widget (button).
 */
static void export_ldif_next( GtkWidget *widget ) {
	gint pageNum;

	pageNum = gtk_notebook_get_current_page( GTK_NOTEBOOK(expldif_dlg.notebook) );
	if( pageNum == PAGE_FILE_INFO ) {
		/* Goto distinguished name page */
		if( exp_ldif_move_file() ) {
			gtk_notebook_set_current_page(
				GTK_NOTEBOOK(expldif_dlg.notebook), PAGE_DN );
			gtk_widget_set_sensitive( expldif_dlg.btnPrev, TRUE );
		}
		export_ldif_message();
	}
	else if( pageNum == PAGE_DN ) {
		/* Goto finish page */
		if( exp_ldif_move_dn() ) {
			gtk_notebook_set_current_page(
				GTK_NOTEBOOK(expldif_dlg.notebook), PAGE_FINISH );
			gtk_button_set_label(GTK_BUTTON(expldif_dlg.btnCancel),
				_("_Close"));
			gtk_button_set_image(GTK_BUTTON(expldif_dlg.btnCancel),
				gtk_image_new_from_icon_name("window-close-symbolic", GTK_ICON_SIZE_BUTTON));
			exp_ldif_finish_show();
			exportldif_save_settings( _exportCtl_ );
			export_ldif_message();
		}
	}
}

/**
 * Create LDIF file selection dialog.
 * \param afs Address file selection data.
 */
static void exp_ldif_file_select_create( AddressFileSelection *afs ) {
	gchar *file = filesel_select_file_save(_("Select LDIF output file"), NULL);
	
	if (file == NULL)
		afs->cancelled = TRUE;
	else {
		afs->cancelled = FALSE;
		gtk_entry_set_text( GTK_ENTRY(expldif_dlg.entryLdif), file );
		g_free(file);
	}
}

/**
 * Callback function to display LDIF file selection dialog.
 */
static void exp_ldif_file_select( void ) {
	exp_ldif_file_select_create( & _exp_ldif_file_selector_ );
}

/**
 * Format notebook file specification page.
 * \param pageNum Page (tab) number.
 * \param pageLbl Page (tab) label.
 */
static void export_ldif_page_file( gint pageNum, gchar *pageLbl ) {
	GtkWidget *vbox;
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *labelBook;
	GtkWidget *entryLdif;
	GtkWidget *btnFile;

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_container_add( GTK_CONTAINER( expldif_dlg.notebook ), vbox );
	gtk_container_set_border_width( GTK_CONTAINER (vbox), BORDER_WIDTH );

	label = gtk_label_new( pageLbl );
	gtk_widget_show( label );
	gtk_notebook_set_tab_label(
		GTK_NOTEBOOK( expldif_dlg.notebook ),
		gtk_notebook_get_nth_page(
			GTK_NOTEBOOK( expldif_dlg.notebook ), pageNum ),
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
	label = gtk_label_new( _( "LDIF Output File" ) );
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_grid_attach(GTK_GRID(table), label, 0, 1, 1, 1);

	entryLdif = gtk_entry_new();
	gtk_grid_attach(GTK_GRID(table), entryLdif, 1, 1, 1, 1);
	gtk_widget_set_hexpand(entryLdif, TRUE);
	gtk_widget_set_halign(entryLdif, GTK_ALIGN_FILL);

	btnFile = gtkut_get_browse_file_btn(_("B_rowse"));
	gtk_grid_attach(GTK_GRID(table), btnFile, 2, 1, 1, 1);

	gtk_widget_show_all(vbox);

	/* Button handler */
	g_signal_connect(G_OBJECT(btnFile), "clicked",
			 G_CALLBACK(exp_ldif_file_select), NULL);

	expldif_dlg.labelBook = labelBook;
	expldif_dlg.entryLdif = entryLdif;
}

static void export_ldif_relative_dn_changed(GtkWidget *widget, gpointer data)
{
	gint relativeDN = combobox_get_active_data(GTK_COMBO_BOX(widget));
	GtkLabel *label = GTK_LABEL(data);

	switch(relativeDN) {
	case EXPORT_LDIF_ID_UID:
		gtk_label_set_text(label,
		_("The address book Unique ID is used to create a DN that is " \
		"formatted similar to:\n" \
		"  uid=102376,ou=people,dc=claws-mail,dc=org"));
		break;
	case EXPORT_LDIF_ID_DNAME:
		gtk_label_set_text(label,
		_("The address book Display Name is used to create a DN that " \
		"is formatted similar to:\n" \
		"  cn=John Doe,ou=people,dc=claws-mail,dc=org"));	
		break;
	case EXPORT_LDIF_ID_EMAIL:
		gtk_label_set_text(label, 
		_("The first Email Address belonging to a person is used to " \
		"create a DN that is formatted similar to:\n" \
		"  mail=john.doe@domain.com,ou=people,dc=claws-mail,dc=org"));	
		break;
	}
	
}

/**
 * Format notebook distinguished name page.
 * \param pageNum Page (tab) number.
 * \param pageLbl Page (tab) label.
 */
static void export_ldif_page_dn( gint pageNum, gchar *pageLbl ) {
	GtkWidget *vbox;
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *entrySuffix;
	GtkWidget *optmenuRDN;
	GtkWidget *labelRDN;
	GtkWidget *checkUseDN;
	GtkWidget *checkEMail;
	GtkListStore *store;
	GtkTreeIter iter;

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_container_add( GTK_CONTAINER( expldif_dlg.notebook ), vbox );
	gtk_container_set_border_width( GTK_CONTAINER (vbox), BORDER_WIDTH );

	label = gtk_label_new( pageLbl );
	gtk_widget_show( label );
	gtk_notebook_set_tab_label(
		GTK_NOTEBOOK( expldif_dlg.notebook ),
		gtk_notebook_get_nth_page(
			GTK_NOTEBOOK( expldif_dlg.notebook ), pageNum ),
		label );

	table = gtk_grid_new();
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
	gtk_container_set_border_width( GTK_CONTAINER(table), 8 );
	gtk_grid_set_row_spacing(GTK_GRID(table), 8);
	gtk_grid_set_column_spacing(GTK_GRID(table), 8);

	/* First row */
	label = gtk_label_new( _( "Suffix" ) );
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_grid_attach(GTK_GRID(table), label, 0, 0, 1, 1);

	entrySuffix = gtk_entry_new();
	gtk_grid_attach(GTK_GRID(table), entrySuffix, 1, 0, 1, 1);
	gtk_widget_set_hexpand(entrySuffix, TRUE);
	gtk_widget_set_halign(entrySuffix, GTK_ALIGN_FILL);

	CLAWS_SET_TIP(entrySuffix, _(
		"The suffix is used to create a \"Distinguished Name\" " \
		"(or DN) for an LDAP entry. Examples include:\n" \
		"  dc=claws-mail,dc=org\n" \
		"  ou=people,dc=domainname,dc=com\n" \
		"  o=Organization Name,c=Country\n"));

	/* Second row */
	label = gtk_label_new( _( "Relative DN" ) );
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_grid_attach(GTK_GRID(table), label, 0, 1, 1, 1);

	optmenuRDN = gtkut_sc_combobox_create(NULL, TRUE);
	store = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(optmenuRDN)));
	
	COMBOBOX_ADD(store, _("Unique ID"), EXPORT_LDIF_ID_UID);
	COMBOBOX_ADD(store, _("Display Name"), EXPORT_LDIF_ID_DNAME);
	COMBOBOX_ADD(store, _("Email Address"), EXPORT_LDIF_ID_EMAIL);

	gtk_grid_attach(GTK_GRID(table), optmenuRDN, 1, 1, 1, 1);

	CLAWS_SET_TIP(optmenuRDN, _(
		"The LDIF file contains several data records that " \
		"are usually loaded into an LDAP server. Each data " \
		"record in the LDIF file is uniquely identified by " \
		"a \"Distinguished Name\" (or DN). The suffix is " \
		"appended to the \"Relative Distinguished Name\" "\
		"(or RDN) to create the DN. Please select one of " \
		"the available RDN options that will be used to " \
		"create the DN."));
	
	/* Third row*/
	labelRDN = gtk_label_new("");
	gtk_label_set_line_wrap(GTK_LABEL(labelRDN), TRUE);
	gtk_label_set_justify(GTK_LABEL(labelRDN), GTK_JUSTIFY_CENTER);
	gtk_grid_attach(GTK_GRID(table), labelRDN, 0, 2, 1, 1);
		
	/* Fourth row */
	checkUseDN = gtk_check_button_new_with_label(
			_( "Use DN attribute if present in data" ) );
	gtk_grid_attach(GTK_GRID(table), checkUseDN, 1, 3, 1, 1);

	CLAWS_SET_TIP(checkUseDN, _(
		"The addressbook may contain entries that were " \
		"previously imported from an LDIF file. The " \
		"\"Distinguished Name\" (DN) user attribute, if " \
		"present in the address book data, may be used in " \
		"the exported LDIF file. The RDN selected above " \
		"will be used if the DN user attribute is not found."));

	/* Fifth row */
	checkEMail = gtk_check_button_new_with_label(
			_( "Exclude record if no Email Address" ) );
	gtk_grid_attach(GTK_GRID(table), checkEMail, 1, 4, 1, 1);

	CLAWS_SET_TIP(checkEMail, _(
		"An addressbook may contain entries without " \
		"Email Addresses. Check this option to ignore " \
		"these records."));


	gtk_widget_show_all(vbox);

	g_signal_connect(G_OBJECT(optmenuRDN), "changed",
		G_CALLBACK(export_ldif_relative_dn_changed), labelRDN);
	gtk_combo_box_set_active(GTK_COMBO_BOX(optmenuRDN), 0);


	expldif_dlg.entrySuffix = entrySuffix;
	expldif_dlg.optmenuRDN  = optmenuRDN;
	expldif_dlg.checkUseDN  = checkUseDN;
	expldif_dlg.checkEMail  = checkEMail;
}

/**
 * Format notebook finish page.
 * \param pageNum Page (tab) number.
 * \param pageLbl Page (tab) label.
 */
static void export_ldif_page_finish( gint pageNum, gchar *pageLbl ) {
	GtkWidget *vbox;
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *labelBook;
	GtkWidget *labelFile;

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_container_add( GTK_CONTAINER( expldif_dlg.notebook ), vbox );
	gtk_container_set_border_width( GTK_CONTAINER (vbox), BORDER_WIDTH );

	label = gtk_label_new( pageLbl );
	gtk_widget_show( label );
	gtk_notebook_set_tab_label(
		GTK_NOTEBOOK( expldif_dlg.notebook ),
		gtk_notebook_get_nth_page( GTK_NOTEBOOK( expldif_dlg.notebook ), pageNum ), label );

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

	gtk_widget_show_all(vbox);

	expldif_dlg.labelOutBook = labelBook;
	expldif_dlg.labelOutFile = labelFile;
}

/**
 * Create main dialog decorations (excluding notebook pages).
 */
static void export_ldif_dialog_create( void ) {
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

	window = gtkut_window_new(GTK_WINDOW_TOPLEVEL, "expldifdlg");
	gtk_widget_set_size_request(window, EXPORTLDIF_WIDTH, EXPORTLDIF_HEIGHT );
	gtk_container_set_border_width( GTK_CONTAINER(window), 0 );
	gtk_window_set_title( GTK_WINDOW(window),
		_("Export Address Book to LDIF File") );
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DIALOG);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(export_ldif_delete_event),
			 NULL );
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(export_ldif_key_pressed),
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
			 G_CALLBACK(export_ldif_prev), NULL);
	g_signal_connect(G_OBJECT(btnNext), "clicked",
			 G_CALLBACK(export_ldif_next), NULL);
	g_signal_connect(G_OBJECT(btnCancel), "clicked",
			 G_CALLBACK(export_ldif_cancel), NULL);

	gtk_widget_show_all(vbox);

	expldif_dlg.window     = window;
	expldif_dlg.notebook   = notebook;
	expldif_dlg.btnPrev    = btnPrev;
	expldif_dlg.btnNext    = btnNext;
	expldif_dlg.btnCancel  = btnCancel;
	expldif_dlg.statusbar  = statusbar;
	expldif_dlg.status_cid = gtk_statusbar_get_context_id(
			GTK_STATUSBAR(statusbar), "Export LDIF Dialog" );
}

/**
 * Create export LDIF dialog.
 */
static void export_ldif_create( void ) {
	export_ldif_dialog_create();
	export_ldif_page_file( PAGE_FILE_INFO, _( "File Info" ) );
	export_ldif_page_dn( PAGE_DN, _( "Distinguished Name" ) );
	export_ldif_page_finish( PAGE_FINISH, _( "Finish" ) );
	gtk_widget_show_all( expldif_dlg.window );
}

/**
 * Populate fields from control data.
 * \param ctl   Export control data.
 */
static void export_ldif_fill_fields( ExportLdifCtl *ctl ) {
	gtk_entry_set_text( GTK_ENTRY(expldif_dlg.entryLdif), "" );
	if( ctl->path ) {
		gtk_entry_set_text( GTK_ENTRY(expldif_dlg.entryLdif),
			ctl->path );
	}
	gtk_entry_set_text( GTK_ENTRY(expldif_dlg.entrySuffix), "" );
	if( ctl->suffix ) {
		gtk_entry_set_text( GTK_ENTRY(expldif_dlg.entrySuffix),
			ctl->suffix );
	}

	gtk_combo_box_set_active(
		GTK_COMBO_BOX( expldif_dlg.optmenuRDN ), ctl->rdnIndex );
	gtk_toggle_button_set_active(
		GTK_TOGGLE_BUTTON( expldif_dlg.checkUseDN ), ctl->useDN );
	gtk_toggle_button_set_active(
		GTK_TOGGLE_BUTTON( expldif_dlg.checkEMail ), ctl->excludeEMail );
}

/**
 * Process export address dialog.
 * \param cache Address book/data source cache.
 */
void addressbook_exp_ldif( AddressCache *cache ) {
	/* Set references to control data */
	_addressCache_ = cache;

	_exportCtl_ = exportldif_create();
	exportldif_load_settings( _exportCtl_ );

	/* Setup GUI */
	if( ! expldif_dlg.window )
		export_ldif_create();

	gtk_button_set_label(GTK_BUTTON(expldif_dlg.btnCancel),
			     _("_Cancel"));
	expldif_dlg.cancelled = FALSE;
	gtk_widget_show(expldif_dlg.window);
	manage_window_set_transient(GTK_WINDOW(expldif_dlg.window));
	gtk_window_set_modal(GTK_WINDOW(expldif_dlg.window), TRUE);
	gtk_label_set_text( GTK_LABEL(expldif_dlg.labelBook), cache->name );
	gtk_label_set_text( GTK_LABEL(expldif_dlg.labelOutBook), cache->name );
	export_ldif_fill_fields( _exportCtl_ );

	gtk_widget_grab_default(expldif_dlg.btnNext);
	gtk_notebook_set_current_page( GTK_NOTEBOOK(expldif_dlg.notebook), PAGE_FILE_INFO );
	gtk_widget_set_sensitive( expldif_dlg.btnPrev, FALSE );
	gtk_widget_set_sensitive( expldif_dlg.btnNext, TRUE );

	export_ldif_message();
	gtk_widget_grab_focus(expldif_dlg.entryLdif);

	gtk_main();
	gtk_widget_hide(expldif_dlg.window);
	gtk_window_set_modal(GTK_WINDOW(expldif_dlg.window), FALSE);
	exportldif_free( _exportCtl_ );
	_exportCtl_ = NULL;

	_addressCache_ = NULL;
}

/*
 * ============================================================================
 * End of Source.
 * ============================================================================
 */

