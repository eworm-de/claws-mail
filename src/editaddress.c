/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2022 the Claws Mail team and Hiroyuki Yamamoto
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

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "alertpanel.h"
#include "stock_pixmap.h"
#include "mgutils.h"
#include "addressbook.h"
#include "addressitem.h"
#include "addritem.h"
#include "addrbook.h"
#include "manage_window.h"
#include "gtkutils.h"
#include "filesel.h"
#include "codeconv.h"
#include "editaddress.h"
#include "editaddress_other_attributes_ldap.h"
#include "prefs_common.h"
#include "menu.h"
#include "combobox.h"
#include "file-utils.h"

/* transient data */
static struct _PersonEdit_dlg personeditdlg;
static AddressBookFile *current_abf = NULL;
static ItemPerson *current_person = NULL;
static ItemFolder *current_parent_folder = NULL;
static EditAddressPostUpdateCallback edit_person_close_post_update_cb = NULL;

typedef enum {
	EMAIL_COL_EMAIL,
	EMAIL_COL_ALIAS,
	EMAIL_COL_REMARKS,
	EMAIL_COL_PTR
} PersonEditEMailColumnPos;

#define EDITPERSON_WIDTH      520
#define EDITPERSON_HEIGHT     320

#ifndef GENERIC_UMPC
# define EMAIL_N_COLS          4
# define EMAIL_COL_WIDTH_EMAIL 180
# define EMAIL_COL_WIDTH_ALIAS 80
#else
# define EMAIL_N_COLS          2
# define EMAIL_COL_WIDTH_EMAIL 130
# define EMAIL_COL_WIDTH_ALIAS 130
#endif

#ifndef GENERIC_UMPC
# define ATTRIB_COL_WIDTH_NAME  240
# define ATTRIB_COL_WIDTH_VALUE 0
#else
# define ATTRIB_COL_WIDTH_NAME  120
# define ATTRIB_COL_WIDTH_VALUE 120
#endif

#define PAGE_BASIC             0
#define PAGE_EMAIL             1
#define PAGE_ATTRIBUTES        2

static gboolean addressbook_edit_person_close( gboolean cancelled );
static GList *edit_person_build_email_list();
static GList *edit_person_build_attrib_list();

static gchar* edit_person_get_common_name_from_widgets(void)
{
	gchar *cn = NULL; /* cn must be freed by caller */

#ifndef GENERIC_UMPC
	{
		cn = gtk_editable_get_chars( GTK_EDITABLE(personeditdlg.entry_name), 0, -1 );
		if ( cn == NULL || *cn == '\0' ) {
			gchar *first = gtk_editable_get_chars( GTK_EDITABLE(personeditdlg.entry_first), 0, -1 );
			gchar *last = gtk_editable_get_chars( GTK_EDITABLE(personeditdlg.entry_last), 0, -1 );
			cn = g_strdup_printf("%s%s%s", first?first:"", (first && last && *first && *last)?" ":"", last?last:"");
			g_free(first);
			g_free(last);
		}
		if ( cn == NULL || *cn == '\0' ) {
			g_free(cn);
			cn = gtk_editable_get_chars( GTK_EDITABLE(personeditdlg.entry_nick), 0, -1 );
		}
	}
#else
	{
		gchar *first = gtk_editable_get_chars( GTK_EDITABLE(personeditdlg.entry_first), 0, -1 );
		gchar *last = gtk_editable_get_chars( GTK_EDITABLE(personeditdlg.entry_last), 0, -1 );
		cn = g_strdup_printf("%s%s%s", first?first:"", (first && last && *first && *last)?" ":"", last?last:"");
		g_free(first);
		g_free(last);
	}
#endif
	if ( cn != NULL )
		g_strstrip(cn);
	return cn;
}

static void edit_person_status_show( gchar *msg ) {
	if( personeditdlg.statusbar != NULL ) {
		gtk_statusbar_pop( GTK_STATUSBAR(personeditdlg.statusbar), personeditdlg.status_cid );
		if( msg ) {
			gtk_statusbar_push( GTK_STATUSBAR(personeditdlg.statusbar), personeditdlg.status_cid, msg );
		}
	}
}

static void edit_person_cancel(GtkWidget *widget, gboolean *cancelled) {
	*cancelled = TRUE;
	if (prefs_common.addressbook_use_editaddress_dialog)
		gtk_main_quit();
	else
		addressbook_edit_person_close( *cancelled );
}

static void edit_person_ok(GtkWidget *widget, gboolean *cancelled) {
	GList *listEMail = edit_person_build_email_list();
	GList *listAttrib = edit_person_build_attrib_list();
	gchar *cn = edit_person_get_common_name_from_widgets();

	if( (cn == NULL || *cn == '\0') && listEMail == NULL && listAttrib == NULL ) {
		gint val;

		val = alertpanel( _("Add New Person"),
#ifndef GENERIC_UMPC
				_("Adding a new person requires at least one of the\n"
				  "following values to be set:\n"
				  " - Display Name\n"
				  " - First Name\n"
				  " - Last Name\n"
				  " - Nickname\n"
				  " - any email address\n"
				  " - any additional attribute\n\n"
				  "Click OK to keep editing this contact.\n"
				  "Click Cancel to close without saving."),
#else
				_("Adding a new person requires at least one of the\n"
				  "following values to be set:\n"
				  " - First Name\n"
				  " - Last Name\n"
				  " - any email address\n"
				  " - any additional attribute\n\n"
				  "Click OK to keep editing this contact.\n"
				  "Click Cancel to close without saving."),
#endif
				NULL, _("_Cancel"), NULL, _("_OK"), NULL, NULL, ALERTFOCUS_SECOND );
		if( val == G_ALERTDEFAULT ) {
			edit_person_cancel(widget, cancelled);
		}
		g_free( cn );
		return;
	}
	g_free( cn );

	*cancelled = FALSE;
	if (prefs_common.addressbook_use_editaddress_dialog)
		gtk_main_quit();
	else
		addressbook_edit_person_close( *cancelled );
}

/* Updated up/down buttons sensitivity, depending on list
 * cursor position */
static void edit_person_email_update_buttons()
{
	GtkTreeModel *model;
	GtkTreeIter iter, otheriter;
	GtkTreePath *path;
	gboolean has_prev;
	ItemEMail *email;

	email = gtkut_tree_view_get_selected_pointer(
			GTK_TREE_VIEW(personeditdlg.view_email), EMAIL_COL_PTR,
			&model, NULL, &iter);

	if (email) {
		if (!personeditdlg.read_only) {
			otheriter = iter;
			path = gtk_tree_model_get_path(model, &otheriter);
			has_prev = gtk_tree_path_prev(path);
			if (has_prev) {
				gtk_tree_model_get_iter(model, &otheriter, path);
			}
			gtk_widget_set_sensitive(personeditdlg.email_up, has_prev);

			otheriter = iter;
			gtk_widget_set_sensitive(personeditdlg.email_down,
					gtk_tree_model_iter_next(model, &otheriter));
		}
	} else {
		gtk_widget_set_sensitive(personeditdlg.email_del, FALSE);
		gtk_widget_set_sensitive(personeditdlg.email_up, FALSE);
		gtk_widget_set_sensitive(personeditdlg.email_down, FALSE);
	}
}

static void edit_person_email_cursor_changed(GtkTreeView *view,
		gpointer user_data)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	ItemEMail *email;

	email = gtkut_tree_view_get_selected_pointer(
			GTK_TREE_VIEW(personeditdlg.view_email), EMAIL_COL_PTR,
			&model, NULL, &iter);

	if (email != NULL) {
		if (email->address != NULL)
			gtk_entry_set_text(GTK_ENTRY(personeditdlg.entry_email),
					email->address);
		if (ADDRITEM_NAME(email) != NULL)
			gtk_entry_set_text(GTK_ENTRY(personeditdlg.entry_alias),
					ADDRITEM_NAME(email));
		if (email->remarks != NULL)
			gtk_entry_set_text(GTK_ENTRY(personeditdlg.entry_remarks),
					email->remarks);
	}

	edit_person_email_update_buttons();
}


static gint edit_person_delete_event(GtkWidget *widget, GdkEventAny *event, gboolean *cancelled) {
	*cancelled = TRUE;
	if (prefs_common.addressbook_use_editaddress_dialog)
		gtk_main_quit();
	else
		addressbook_edit_person_close( *cancelled );
	return TRUE;
}

static gboolean edit_person_key_pressed(GtkWidget *widget, GdkEventKey *event, gboolean *cancelled) {
	if (prefs_common.addressbook_use_editaddress_dialog) {
	if (event && event->keyval == GDK_KEY_Escape) {
		*cancelled = TRUE;
		gtk_main_quit();
	}
	}
	return FALSE;
}

static gchar *_title_new_ = NULL;
static gchar *_title_edit_ = NULL;

static void edit_person_set_widgets_title( gchar *text )
{
	gchar *label = NULL;

	cm_return_if_fail( text != NULL );

	gtk_label_set_text(GTK_LABEL(personeditdlg.title), "");
	label = g_markup_printf_escaped("<b>%s</b>", text);
	gtk_label_set_markup(GTK_LABEL(personeditdlg.title), label);
	g_free(label);
}

static void edit_person_set_window_title( gint pageNum ) {
	gchar *sTitle;

	if( _title_new_ == NULL ) {
		_title_new_ = g_strdup( _("Add New Person") );
		_title_edit_ = g_strdup( _("Edit Person Details") );
	}

	if( pageNum == PAGE_BASIC ) {
		if( personeditdlg.editNew ) {
			if (prefs_common.addressbook_use_editaddress_dialog)
				gtk_window_set_title( GTK_WINDOW(personeditdlg.container), _title_new_ );
			else
				edit_person_set_widgets_title( _title_new_ );
		}
		else {
			if (prefs_common.addressbook_use_editaddress_dialog)
				gtk_window_set_title( GTK_WINDOW(personeditdlg.container), _title_edit_ );
			else
				edit_person_set_widgets_title( _title_edit_ );
		}
	}
	else {
		if( personeditdlg.entry_name == NULL ) {
			sTitle = g_strdup( _title_edit_ );
		}
		else {
			gchar *name;
			name = gtk_editable_get_chars( GTK_EDITABLE(personeditdlg.entry_name), 0, -1 );
			g_strstrip(name);
			if ( *name != '\0' )
				sTitle = g_strdup_printf( "%s - %s", _title_edit_, name );
			else
				sTitle = g_strdup( _title_edit_ );
			g_free( name );
		}
		if (prefs_common.addressbook_use_editaddress_dialog)
			gtk_window_set_title( GTK_WINDOW(personeditdlg.container), sTitle );
		else
			edit_person_set_widgets_title( sTitle );
		g_free( sTitle );
	}
}

static void edit_person_email_clear( gpointer data ) {
	gtk_entry_set_text( GTK_ENTRY(personeditdlg.entry_email), "" );
	gtk_entry_set_text( GTK_ENTRY(personeditdlg.entry_alias), "" );
	gtk_entry_set_text( GTK_ENTRY(personeditdlg.entry_remarks), "" );
}

static void edit_person_attrib_clear( gpointer data ) {
	if (!personeditdlg.ldap) {
		gtk_entry_set_text( GTK_ENTRY(gtk_bin_get_child(GTK_BIN((personeditdlg.entry_atname)))), "" );
		gtk_entry_set_text( GTK_ENTRY(personeditdlg.entry_atvalue), "" );
	}
}

static void edit_person_switch_page( GtkNotebook *notebook, gpointer page,
					gint pageNum, gpointer user_data)
{
	edit_person_set_window_title( pageNum );
	edit_person_status_show( "" );
}

/*
* Load clist with a copy of person's email addresses.
*/
static void edit_person_load_email( ItemPerson *person ) {
	GList *node = person->listEMail;
	GtkTreeModel *model = gtk_tree_view_get_model(
			GTK_TREE_VIEW(personeditdlg.view_email));
	GtkTreeIter iter;

	while( node ) {
		ItemEMail *emorig = ( ItemEMail * ) node->data;
		ItemEMail *email = addritem_copyfull_item_email( emorig );

		gtk_list_store_append(GTK_LIST_STORE(model), &iter);
		gtk_list_store_set(GTK_LIST_STORE(model), &iter,
				EMAIL_COL_EMAIL, email->address,
#ifndef GENERIC_UMPC
				EMAIL_COL_ALIAS, email->obj.name,
				EMAIL_COL_REMARKS, email->remarks,
#endif
				EMAIL_COL_PTR, email,
				-1);

		node = g_list_next( node );
	}
}

static void edit_person_email_move_up( gpointer data ) {
	GtkTreeModel *model;
	GtkTreeIter iter, otheriter;
	GtkTreePath *path;
	ItemEMail *email;
	gboolean has_prev;

	edit_person_email_clear( NULL );
	edit_person_status_show( NULL );

	email = gtkut_tree_view_get_selected_pointer(
			GTK_TREE_VIEW(personeditdlg.view_email), EMAIL_COL_PTR,
			&model, NULL, &iter);

	if( email ) {
		otheriter = iter;
		/* GTK+2 does not have gtk_tree_model_iter_previous(), so
		 * we have to go through GtkTreePath */
		path = gtk_tree_model_get_path(model, &otheriter);
		has_prev = gtk_tree_path_prev(path);
		if (has_prev) {
			gtk_tree_model_get_iter(model, &otheriter, path);
			gtk_list_store_move_before(GTK_LIST_STORE(model), &iter, &otheriter);
		}
		gtk_tree_path_free(path);
	}

	edit_person_email_update_buttons();
}

static void edit_person_email_move_down( gpointer data ) {
	GtkTreeModel *model;
	GtkTreeIter iter, otheriter;
	ItemEMail *email;

	edit_person_email_clear( NULL );
	edit_person_status_show( NULL );

	email = gtkut_tree_view_get_selected_pointer(
			GTK_TREE_VIEW(personeditdlg.view_email), EMAIL_COL_PTR,
			&model, NULL, &iter);

	if( email ) {
		otheriter = iter;
		if (gtk_tree_model_iter_next(model, &otheriter)) {
			gtk_list_store_move_after(GTK_LIST_STORE(model), &iter, &otheriter);
		}
	}

	edit_person_email_update_buttons();
}

static void edit_person_attrib_cursor_changed(GtkTreeView *view,
		gpointer user_data)
{
	UserAttribute *attrib = gtkut_tree_view_get_selected_pointer(
			view, ATTRIB_COL_PTR, NULL, NULL, NULL);

	if( attrib && !personeditdlg.read_only && !personeditdlg.ldap ) {
		gtk_entry_set_text( GTK_ENTRY(gtk_bin_get_child(GTK_BIN((personeditdlg.entry_atname))) ), attrib->name );
		gtk_entry_set_text( GTK_ENTRY(personeditdlg.entry_atvalue), attrib->value );
		gtk_widget_set_sensitive(personeditdlg.attrib_del, TRUE);
	} else {
		gtk_widget_set_sensitive(personeditdlg.attrib_del, FALSE);
	}
	edit_person_status_show( NULL );
}


static void edit_person_email_delete( gpointer data ) {
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeSelection *sel;
	ItemEMail *email;
	gboolean has_row = FALSE;
	gint n;

	edit_person_email_clear( NULL );
	edit_person_status_show( NULL );

	email = gtkut_tree_view_get_selected_pointer(
			GTK_TREE_VIEW(personeditdlg.view_email), EMAIL_COL_PTR,
			&model, &sel, &iter);

	if( email ) {
		/* Remove list entry and set iter to next row, if any */
		has_row = gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
		addritem_free_item_email( email );
		email = NULL;
	}

	/* Position hilite bar */
	if (!has_row) {
		/* The removed row was the last in the list, so iter is not
		 * valid. Find out if there is at least one row remaining
		 * in the list, and select the last one if so. */
		n = gtk_tree_model_iter_n_children(model, NULL);
		if (n > 0 && gtk_tree_model_iter_nth_child(model, &iter, NULL, n-1)) {
			/* It exists */
			has_row = TRUE;
		}
	}

	if (has_row)
		gtk_tree_selection_select_iter(sel, &iter);

	edit_person_email_cursor_changed(
			GTK_TREE_VIEW(personeditdlg.view_email), NULL);
}

static ItemEMail *edit_person_email_edit( gboolean *error, ItemEMail *email ) {
	ItemEMail *retVal = NULL;
	gchar *sEmail, *sAlias, *sRemarks, *sEmail_;

	*error = TRUE;
	sEmail_ = gtk_editable_get_chars( GTK_EDITABLE(personeditdlg.entry_email), 0, -1 );
	sAlias = gtk_editable_get_chars( GTK_EDITABLE(personeditdlg.entry_alias), 0, -1 );
	sRemarks = gtk_editable_get_chars( GTK_EDITABLE(personeditdlg.entry_remarks), 0, -1 );
	sEmail = mgu_email_check_empty( sEmail_ );
	g_free( sEmail_ );

	if( sEmail ) {
		if( email == NULL ) {
			email = addritem_create_item_email();
		}
		addritem_email_set_address( email, sEmail );
		addritem_email_set_alias( email, sAlias );
		addritem_email_set_remarks( email, sRemarks );
		retVal = email;
		*error = FALSE;
	}
	else {
		edit_person_status_show( _( "An Email address must be supplied." ) );
	}

	g_free( sEmail );
	g_free( sAlias );
	g_free( sRemarks );

	return retVal;
}

static void edit_person_email_modify( gpointer data ) {
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean errFlg = FALSE;
	ItemEMail *email;

	email = gtkut_tree_view_get_selected_pointer(
			GTK_TREE_VIEW(personeditdlg.view_email), EMAIL_COL_PTR,
			&model, NULL, &iter);

	if( email ) {
		edit_person_email_edit( &errFlg, email );
		if( ! errFlg ) {
			gtk_list_store_set(GTK_LIST_STORE(model), &iter,
					EMAIL_COL_EMAIL, email->address,
#ifndef GENERIC_UMPC
					EMAIL_COL_ALIAS, email->obj.name,
					EMAIL_COL_REMARKS, email->remarks,
#endif
					-1);
			edit_person_email_clear( NULL );
		}
	}
}

static void edit_person_email_add( gpointer data ) {
	GtkTreeModel *model;
	GtkTreeIter iter, otheriter;
	GtkTreeSelection *sel;
	gboolean errFlg = FALSE, prev_exists = FALSE;
	ItemEMail *email = NULL;

	email = gtkut_tree_view_get_selected_pointer(
			GTK_TREE_VIEW(personeditdlg.view_email), EMAIL_COL_PTR,
			&model, &sel, &otheriter);
	if (email != NULL)
		prev_exists = TRUE;

	email = edit_person_email_edit( &errFlg, NULL );
	if( ! errFlg ) {
		if (prev_exists)
			gtk_list_store_insert_after(GTK_LIST_STORE(model), &iter, &otheriter);
		else /* list is empty */
			gtk_list_store_append(GTK_LIST_STORE(model), &iter);

		gtk_list_store_set(GTK_LIST_STORE(model), &iter,
					EMAIL_COL_EMAIL, email->address,
#ifndef GENERIC_UMPC
					EMAIL_COL_ALIAS, email->obj.name,
					EMAIL_COL_REMARKS, email->remarks,
#endif
					EMAIL_COL_PTR, email,
					-1);
		gtk_tree_selection_select_iter(sel, &iter);
		edit_person_email_update_buttons();

		edit_person_email_clear( NULL );
	}
}

static gboolean list_find_email(const gchar *addr)
{
	GtkWidget *view = personeditdlg.view_email;
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
	GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
	GtkTreeIter iter;
	ItemEMail *email;

	if (!gtk_tree_model_get_iter_first(model, &iter))
		return FALSE;

	do {
		gtk_tree_model_get(model, &iter, EMAIL_COL_PTR, &email, -1);
		if (!g_ascii_strcasecmp(email->address, addr)) {
			gtk_tree_selection_select_iter(sel, &iter);
			return TRUE;
		}
	} while (gtk_tree_model_iter_next(model, &iter));

	return FALSE;
}

static gboolean list_find_attribute(const gchar *attr)
{
	GtkWidget *view = personeditdlg.view_attrib;
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
	GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
	GtkTreeIter iter;
	UserAttribute *attrib;

	if (!gtk_tree_model_get_iter_first(model, &iter))
		return FALSE;

	do {
		gtk_tree_model_get(model, &iter, ATTRIB_COL_PTR, &attrib, -1);
		if (!g_ascii_strcasecmp(attrib->name, attr)) {
			gtk_tree_selection_select_iter(sel, &iter);
			return TRUE;
		}
	} while (gtk_tree_model_iter_next(model, &iter));

	return FALSE;
}

/*
* Load list with a copy of person's email addresses.
*/
static void edit_person_load_attrib( ItemPerson *person ) {
	GList *node = person->listAttrib;
	GtkWidget *view = personeditdlg.view_attrib;
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
	GtkTreeIter iter;

	while( node ) {
		UserAttribute *atorig = ( UserAttribute * ) node->data;
		UserAttribute *attrib = addritem_copy_attribute( atorig );

		debug_print("name: %s value: %s\n", attrib->name, attrib->value);

		gtk_list_store_append(GTK_LIST_STORE(model), &iter);
		gtk_list_store_set(GTK_LIST_STORE(model), &iter,
				ATTRIB_COL_NAME, attrib->name,
				ATTRIB_COL_VALUE, attrib->value,
				ATTRIB_COL_PTR, attrib,
				-1);

		node = g_list_next( node );
	}
}

static void edit_person_attrib_delete(gpointer data) {
	UserAttribute *attrib;
	GtkTreeModel *model;
	GtkTreeSelection *sel;
	GtkTreeIter iter;
	gboolean has_row = FALSE;
	gint n;

	edit_person_attrib_clear(NULL);
	edit_person_status_show(NULL);

	attrib = gtkut_tree_view_get_selected_pointer(
			GTK_TREE_VIEW(personeditdlg.view_attrib), ATTRIB_COL_PTR,
			&model, &sel, &iter);

	if (attrib) {
		/* Remove list entry, and set iter to next row, if any */
		has_row = gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
		addritem_free_attribute(attrib);
		attrib = NULL;
	}

	/* Position hilite bar */
	if (!has_row) {
		/* The removed row was the last in the list, so iter is not
		 * valid. Find out if there is at least one row remaining
		 * in the list, and select the last one if so. */
		n = gtk_tree_model_iter_n_children(model, NULL);
		if (n > 0 && gtk_tree_model_iter_nth_child(model, &iter, NULL, n-1)) {
			/* It exists. */
			has_row = TRUE;
		}
	}

	if (has_row)
		gtk_tree_selection_select_iter(sel, &iter);

	edit_person_attrib_cursor_changed(
			GTK_TREE_VIEW(personeditdlg.view_attrib), NULL);
}

static UserAttribute *edit_person_attrib_edit( gboolean *error, UserAttribute *attrib ) {
	UserAttribute *retVal = NULL;
	gchar *sName, *sValue, *sName_, *sValue_;

	*error = TRUE;
	sName_ = gtk_editable_get_chars( GTK_EDITABLE(gtk_bin_get_child(GTK_BIN((personeditdlg.entry_atname)))), 0, -1 );
	sValue_ = gtk_editable_get_chars( GTK_EDITABLE(personeditdlg.entry_atvalue), 0, -1 );
	sName = mgu_email_check_empty( sName_ );
	sValue = mgu_email_check_empty( sValue_ );
	g_free( sName_ );
	g_free( sValue_ );

	if( sName && sValue ) {
		if( attrib == NULL ) {
			attrib = addritem_create_attribute();
		}
		addritem_attrib_set_name( attrib, sName );
		addritem_attrib_set_value( attrib, sValue );
		retVal = attrib;
		*error = FALSE;
	}
	else {
		edit_person_status_show( _( "A Name and Value must be supplied." ) );
	}

	g_free( sName );
	g_free( sValue );

	return retVal;
}

static void edit_person_attrib_modify(gpointer data) {
	gboolean errFlg = FALSE;
	GtkTreeModel *model;
	GtkTreeIter iter;
	UserAttribute *attrib;

	attrib = gtkut_tree_view_get_selected_pointer(
			GTK_TREE_VIEW(personeditdlg.view_attrib), ATTRIB_COL_PTR,
			&model, NULL, &iter);
	if (attrib) {
		edit_person_attrib_edit(&errFlg, attrib);
		if (!errFlg) {
			gtk_list_store_set(GTK_LIST_STORE(model), &iter,
					ATTRIB_COL_NAME, attrib->name,
					ATTRIB_COL_VALUE, attrib->value,
					-1);
			edit_person_attrib_clear(NULL);
		}
	}
}

static void edit_person_attrib_add(gpointer data) {
	gboolean errFlg = FALSE;
	GtkTreeModel *model;
	GtkTreeSelection *sel;
	GtkTreeIter iter, iter2;
	UserAttribute *prev_attrib, *attrib;

	attrib = edit_person_attrib_edit(&errFlg, NULL);
	if (attrib == NULL) /* input for new attribute is not valid */
		return;

	prev_attrib = gtkut_tree_view_get_selected_pointer(
			GTK_TREE_VIEW(personeditdlg.view_attrib), ATTRIB_COL_PTR,
			&model, &sel, &iter);

	if (prev_attrib == NULL) {
		/* No row selected, or list empty, add new attribute as first row. */
		gtk_list_store_insert(GTK_LIST_STORE(model), &iter, 0);
	} else {
		/* Add it after the currently selected row. */
		gtk_list_store_insert_after(GTK_LIST_STORE(model), &iter2,
				&iter);
		iter = iter2;
	}

	/* Fill out the new row. */
	gtk_list_store_set(GTK_LIST_STORE(model), &iter,
			ATTRIB_COL_NAME, attrib->name,
			ATTRIB_COL_VALUE, attrib->value,
			ATTRIB_COL_PTR, attrib,
			-1);
	gtk_tree_selection_select_iter(sel, &iter);
	edit_person_attrib_clear(NULL);
}

/*!
 *\brief	Save Gtk object size to prefs dataset
 */
static void edit_person_size_allocate_cb(GtkWidget *widget,
					 GtkAllocation *allocation)
{
	cm_return_if_fail(allocation != NULL);

	gtk_window_get_size(GTK_WINDOW(widget),
		&prefs_common.addressbookeditpersonwin_width, &prefs_common.addressbookeditpersonwin_height);
}

/* build edit person widgets, return a pointer to the main container of the widgetset (a vbox) */
static GtkWidget* addressbook_edit_person_widgets_create( GtkWidget* container, gboolean *cancelled )
{
	GtkWidget *vbox;
	GtkWidget *vnbox;
	GtkWidget *notebook;
	GtkWidget *hbbox;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	 gtk_container_set_border_width(GTK_CONTAINER(vbox), BORDER_WIDTH); 
	gtk_widget_show(vbox);
	gtk_container_add(GTK_CONTAINER(container), vbox);

	vnbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	gtk_container_set_border_width(GTK_CONTAINER(vnbox), 4);
	gtk_widget_show(vnbox);
	gtk_box_pack_start(GTK_BOX(vbox), vnbox, TRUE, TRUE, 0);

	/* Notebook */
	notebook = gtk_notebook_new();
	gtk_widget_show(notebook);
	gtk_box_pack_start(GTK_BOX(vnbox), notebook, TRUE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(notebook), 6);

	/* Button panel */
	if (prefs_common.addressbook_use_editaddress_dialog)
	gtkut_stock_button_set_create(&hbbox, &cancel_btn, NULL, _("_Cancel"),
				      &ok_btn, NULL, _("_OK"),
				      NULL, NULL, NULL);
	else
		gtkut_stock_with_text_button_set_create(&hbbox,
					  &cancel_btn, NULL, _("Discard"),
				      &ok_btn, NULL, _("Apply"),
				      NULL, NULL, NULL);
	gtk_box_pack_end(GTK_BOX(vnbox), hbbox, FALSE, FALSE, 0);
	gtk_widget_grab_default(ok_btn);

	g_signal_connect(G_OBJECT(ok_btn), "clicked",
			 G_CALLBACK(edit_person_ok), cancelled);
	g_signal_connect(G_OBJECT(cancel_btn), "clicked",
			 G_CALLBACK(edit_person_cancel), cancelled);
	g_signal_connect(G_OBJECT(notebook), "switch_page",
			 G_CALLBACK(edit_person_switch_page), NULL );

	gtk_widget_show_all(vbox);

	personeditdlg.notebook   = notebook;
	personeditdlg.ok_btn     = ok_btn;
	personeditdlg.cancel_btn = cancel_btn;

	return vbox;
}

static void addressbook_edit_person_dialog_create( gboolean *cancelled ) {
	GtkWidget *window;
	GtkWidget *hsbox;
	GtkWidget *vbox;
	GtkWidget *statusbar;
	static GdkGeometry geometry;

	window = gtkut_window_new(GTK_WINDOW_TOPLEVEL, "editaddress");
	/* gtk_container_set_border_width(GTK_CONTAINER(window), 0); */
	gtk_window_set_title(GTK_WINDOW(window), _("Edit Person Data"));
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DIALOG);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(edit_person_delete_event),
			 cancelled);
	g_signal_connect(G_OBJECT(window), "size_allocate",
			 G_CALLBACK(edit_person_size_allocate_cb),
			cancelled);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(edit_person_key_pressed),
			 cancelled);

	vbox = addressbook_edit_person_widgets_create(window, cancelled);

	/* Status line */
	hsbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_end(GTK_BOX(vbox), hsbox, FALSE, FALSE, BORDER_WIDTH);
	statusbar = gtk_statusbar_new();
	gtk_box_pack_start(GTK_BOX(hsbox), statusbar, TRUE, TRUE, BORDER_WIDTH);

	if (!geometry.min_height) {
		geometry.min_width = EDITPERSON_WIDTH;
		geometry.min_height = EDITPERSON_HEIGHT;
	}

	gtk_window_set_geometry_hints(GTK_WINDOW(window), NULL, &geometry,
				      GDK_HINT_MIN_SIZE);
	gtk_window_set_default_size(GTK_WINDOW(window),
				    prefs_common.addressbookeditpersonwin_width,
				    prefs_common.addressbookeditpersonwin_height);

	personeditdlg.container  = window;
	personeditdlg.statusbar  = statusbar;
	personeditdlg.status_cid = gtk_statusbar_get_context_id( GTK_STATUSBAR(statusbar), "Edit Person Dialog" );

}

/* parent must be a box */
static void addressbook_edit_person_widgetset_create( GtkWidget *parent, gboolean *cancelled )
{
	GtkWidget *vbox;
	GtkWidget *label;

	if ( parent == NULL )
		g_warning("addressbook_edit_person_widgetset_create: parent is NULL");

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_pack_end(GTK_BOX(parent), vbox, TRUE, TRUE, 0);

	label = gtk_label_new(_("Edit Person Data"));
	gtk_label_set_justify( GTK_LABEL(label), GTK_JUSTIFY_CENTER);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	addressbook_edit_person_widgets_create(vbox, cancelled);

	gtk_widget_set_size_request(vbox, EDITPERSON_WIDTH, EDITPERSON_HEIGHT);

	personeditdlg.container = vbox;
	personeditdlg.title = label;
	personeditdlg.statusbar  = NULL;
	personeditdlg.status_cid = 0;
}

void addressbook_edit_person_widgetset_hide( void )
{
	if ( personeditdlg.container )
		gtk_widget_hide( personeditdlg.container );
}

static void addressbook_edit_person_set_picture(void)
{
	GError *error = NULL;
	gchar *filename;
	int width, height, scalewidth, scaleheight;

	if (personeditdlg.ldap)
		return;

	if ( (filename = filesel_select_file_open(_("Choose a picture"), NULL)) ) {
		GdkPixbuf *pixbuf = NULL;
		gdk_pixbuf_get_file_info(filename, &width, &height);

		if ( width > 128 || height > 128 ) {
			if (width > height) {
				scaleheight = (height * 128) / width;
				scalewidth = 128;
			}
			else {
				scalewidth = (width * 128) / height;
				scaleheight = 128;
			}
			pixbuf = gdk_pixbuf_new_from_file_at_scale(filename, 
					scalewidth, scaleheight, TRUE, &error);
		} else {
			pixbuf = gdk_pixbuf_new_from_file(filename, &error);
		}
		if (error) {
			alertpanel_error(_("Failed to import image: \n%s"),
					error->message);
			g_error_free(error);
			error = NULL;
			/* keep the previous picture if any */
			g_free(filename);
			if (pixbuf)
				g_object_unref(pixbuf);
			return;
		}
		personeditdlg.picture_set = TRUE;
		cm_menu_set_sensitive("EditAddressPopup/UnsetPicture", personeditdlg.picture_set);
		g_free(filename);
		gtk_image_set_from_pixbuf(GTK_IMAGE(personeditdlg.image), pixbuf);
		g_object_unref(pixbuf);
	}
}	

static void addressbook_edit_person_clear_picture(void)
{
	GdkPixbuf *pixbuf;

	stock_pixbuf_gdk(STOCK_PIXMAP_ANONYMOUS, &pixbuf);
	personeditdlg.picture_set = FALSE;
	cm_menu_set_sensitive("EditAddressPopup/UnsetPicture", personeditdlg.picture_set);
	gtk_image_set_from_pixbuf(GTK_IMAGE(personeditdlg.image), pixbuf);
}

static void addressbook_edit_person_set_picture_menu_cb (GtkAction *action, gpointer data)
{
	addressbook_edit_person_set_picture();
}

static void addressbook_edit_person_unset_picture_menu_cb (GtkAction *action, gpointer data)
{
	addressbook_edit_person_clear_picture();
}

static GtkWidget *editaddr_popup_menu = NULL;
static GtkActionEntry editaddr_popup_entries[] =
{
	{"EditAddressPopup",			NULL, "EditAddressPopup", NULL, NULL, NULL },
	{"EditAddressPopup/SetPicture",		NULL, N_("_Set picture"), NULL, NULL, G_CALLBACK(addressbook_edit_person_set_picture_menu_cb) },
	{"EditAddressPopup/UnsetPicture",	NULL, N_("_Unset picture"), NULL, NULL, G_CALLBACK(addressbook_edit_person_unset_picture_menu_cb) },
};

static void addressbook_edit_person_set_picture_cb(GtkWidget *widget, 
		GdkEventButton *event, gpointer data)
{	
	if (event->button == 1) {
		addressbook_edit_person_set_picture();
	} else {
		gtk_menu_popup_at_pointer(GTK_MENU(editaddr_popup_menu), NULL);
	}
}

static gboolean addressbook_edit_person_picture_popup_menu(GtkWidget *widget, gpointer data)
{
	GdkEventButton event;
	
	event.button = 3;
	event.time = gtk_get_current_event_time();
	
	addressbook_edit_person_set_picture_cb(NULL, &event, data);

	return TRUE;
}

static void addressbook_edit_person_page_basic( gint pageNum, gchar *pageLbl ) {
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *ebox_picture;
	GtkWidget *frame_picture;
	GtkWidget *entry_name;
	GtkWidget *entry_fn;
	GtkWidget *entry_ln;
	GtkWidget *entry_nn;
	const gchar *locale;
	gint top = 0;

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20 );
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8 );

	gtk_widget_show( vbox );	

	if (!editaddr_popup_menu) {
		cm_menu_create_action_group("EditAddressPopup", editaddr_popup_entries,
			G_N_ELEMENTS(editaddr_popup_entries), (gpointer)NULL);
		MENUITEM_ADDUI("/Menus", "EditAddressPopup", "EditAddressPopup", GTK_UI_MANAGER_MENU)
		MENUITEM_ADDUI("/Menus/EditAddressPopup", "SetPicture", "EditAddressPopup/SetPicture", GTK_UI_MANAGER_MENUITEM)
		MENUITEM_ADDUI("/Menus/EditAddressPopup", "UnsetPicture", "EditAddressPopup/UnsetPicture", GTK_UI_MANAGER_MENUITEM)

		editaddr_popup_menu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(
			gtk_ui_manager_get_widget(gtkut_ui_manager(), "/Menus/EditAddressPopup")) );
	}
	/* User's picture */
	ebox_picture = gtk_event_box_new();
	frame_picture = gtk_frame_new(_("Photo"));
	
	/* Room for a photo */
	personeditdlg.image = gtk_image_new();
	addressbook_edit_person_clear_picture();

	gtk_container_add(GTK_CONTAINER(ebox_picture), personeditdlg.image);
	gtk_container_add(GTK_CONTAINER(frame_picture), ebox_picture);	
	gtk_container_add(GTK_CONTAINER( personeditdlg.notebook ), hbox );
	gtk_container_set_border_width( GTK_CONTAINER (vbox), BORDER_WIDTH );
	gtk_container_set_border_width( GTK_CONTAINER (hbox), BORDER_WIDTH );

	label = gtk_label_new_with_mnemonic( pageLbl );
	gtk_widget_show( label );
	
	gtk_box_pack_start(GTK_BOX(hbox), frame_picture, TRUE, TRUE, 0);
	
	gtk_notebook_set_tab_label(
		GTK_NOTEBOOK( personeditdlg.notebook ),
		gtk_notebook_get_nth_page( GTK_NOTEBOOK( personeditdlg.notebook ), pageNum ), label );
	
	g_signal_connect(G_OBJECT(ebox_picture), "popup-menu",
			 G_CALLBACK(addressbook_edit_person_picture_popup_menu), NULL);
	g_signal_connect(G_OBJECT(ebox_picture), "button_press_event", 
			G_CALLBACK(addressbook_edit_person_set_picture_cb), NULL);

	table = gtk_grid_new();

#define ATTACH_ROW(text, entry) \
{ \
	label = gtk_label_new(text); \
	gtk_grid_attach(GTK_GRID(table), label, 0, top, 1, 1); \
	gtk_label_set_xalign(GTK_LABEL(label), 0.0); \
 \
	entry = gtk_entry_new(); \
	gtk_grid_attach(GTK_GRID(table), entry, 1, top, 1, 1); \
	gtk_widget_set_hexpand(entry, TRUE); \
	gtk_widget_set_halign(entry, GTK_ALIGN_FILL); \
	top++; \
}

#define ATTACH_HIDDEN_ROW(text, entry) \
{ \
	entry = gtk_entry_new(); \
}

#ifndef GENERIC_UMPC
	ATTACH_ROW(_("Display Name"), entry_name);
#else
	ATTACH_HIDDEN_ROW(_("Display Name"), entry_name);
#endif
	locale = conv_get_current_locale();
	if (locale &&
	    (!g_ascii_strncasecmp(locale, "hu", 2) ||
	     !g_ascii_strncasecmp(locale, "ja", 2) ||
	     !g_ascii_strncasecmp(locale, "ko", 2) ||
	     !g_ascii_strncasecmp(locale, "vi", 2) ||
	     !g_ascii_strncasecmp(locale, "zh", 2))) {
		ATTACH_ROW(_("Last Name"), entry_ln);
		ATTACH_ROW(_("First Name"), entry_fn);
	} else {
		ATTACH_ROW(_("First Name"), entry_fn);
		ATTACH_ROW(_("Last Name"), entry_ln);
	}
#ifndef GENERIC_UMPC
	ATTACH_ROW(_("Nickname"), entry_nn);
#else
	ATTACH_HIDDEN_ROW(_("Nickname"), entry_nn);
#endif

#undef ATTACH_ROW
#undef ATTACH_HIDDEN_ROW
	gtk_box_pack_start(GTK_BOX(vbox), table, TRUE, TRUE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);
	gtk_container_set_border_width( GTK_CONTAINER(table), 8 );
	gtk_grid_set_row_spacing(GTK_GRID(table), 15);
	gtk_grid_set_column_spacing(GTK_GRID(table), 8);

	gtk_widget_show_all(vbox);
	personeditdlg.entry_name  = entry_name;
	personeditdlg.entry_first = entry_fn;
	personeditdlg.entry_last  = entry_ln;
	personeditdlg.entry_nick  = entry_nn;
}

static gboolean email_adding = FALSE, email_saving = FALSE;

static void edit_person_entry_email_changed (GtkWidget *entry, gpointer data)
{
	GtkTreeModel *model = gtk_tree_view_get_model(
			GTK_TREE_VIEW(personeditdlg.view_email));
	gboolean non_empty = (gtk_tree_model_iter_n_children(model, NULL) > 0);

	if (personeditdlg.read_only)
		return;

	if (gtk_entry_get_text(GTK_ENTRY(personeditdlg.entry_email)) == NULL
	||  strlen(gtk_entry_get_text(GTK_ENTRY(personeditdlg.entry_email))) == 0) {
		gtk_widget_set_sensitive(personeditdlg.email_add,FALSE);
		gtk_widget_set_sensitive(personeditdlg.email_mod,FALSE);
		email_adding = FALSE;
		email_saving = FALSE;
	} else if (list_find_email(gtk_entry_get_text(GTK_ENTRY(personeditdlg.entry_email)))) {
		gtk_widget_set_sensitive(personeditdlg.email_add,FALSE);
		gtk_widget_set_sensitive(personeditdlg.email_mod,non_empty);
		email_adding = FALSE;
		email_saving = non_empty;
	} else {
		gtk_widget_set_sensitive(personeditdlg.email_add,TRUE);
		gtk_widget_set_sensitive(personeditdlg.email_mod,non_empty);
		email_adding = TRUE;
		email_saving = non_empty;
	}
}

static gboolean edit_person_entry_email_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event && (event->keyval == GDK_KEY_KP_Enter ||
	    event->keyval == GDK_KEY_Return)) {
		if (email_saving)
			edit_person_email_modify(NULL);		
		else if (email_adding)
			edit_person_email_add(NULL);
	}
	return FALSE;
}


static void addressbook_edit_person_page_email( gint pageNum, gchar *pageLbl ) {
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *vboxl;
	GtkWidget *vboxb;
	GtkWidget *vbuttonbox;
	GtkWidget *buttonUp;
	GtkWidget *buttonDown;
	GtkWidget *buttonDel;
	GtkWidget *buttonMod;
	GtkWidget *buttonAdd;

	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *scrollwin;
	GtkWidget *view;
	GtkWidget *entry_email;
	GtkWidget *entry_alias;
	GtkWidget *entry_remarks;
	GtkListStore *store;
	GtkTreeViewColumn *col;
	GtkCellRenderer *rdr;
	GtkTreeSelection *sel;

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8 );
	gtk_widget_show( vbox );
	gtk_container_add( GTK_CONTAINER( personeditdlg.notebook ), vbox );
	gtk_container_set_border_width( GTK_CONTAINER (vbox), BORDER_WIDTH );

	label = gtk_label_new_with_mnemonic( pageLbl );
	gtk_widget_show( label );
	gtk_notebook_set_tab_label(
		GTK_NOTEBOOK( personeditdlg.notebook ),
		gtk_notebook_get_nth_page( GTK_NOTEBOOK( personeditdlg.notebook ), pageNum ), label );

	/* Split into two areas */
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_container_add( GTK_CONTAINER( vbox ), hbox );

	/* Address list */
	vboxl = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4 );
	gtk_container_add( GTK_CONTAINER( hbox ), vboxl );
	gtk_container_set_border_width( GTK_CONTAINER(vboxl), 4 );

	scrollwin = gtk_scrolled_window_new( NULL, NULL );
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(scrollwin), TRUE);

	store = gtk_list_store_new(EMAIL_N_COLS,
			G_TYPE_STRING,
#ifndef GENERIC_UMPC
			G_TYPE_STRING,
			G_TYPE_STRING,
#endif
			G_TYPE_POINTER,
			-1);

	view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	g_object_unref(store);
#ifndef GENERIC_UMPC
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), TRUE);
#else
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), FALSE);
#endif
	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
	gtk_tree_selection_set_mode(sel, GTK_SELECTION_BROWSE);

	rdr = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes(_("Email Address"), rdr,
			"markup", EMAIL_COL_EMAIL, NULL);
	gtk_tree_view_column_set_min_width(col, EMAIL_COL_WIDTH_EMAIL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);

#ifndef GENERIC_UMPC
	col = gtk_tree_view_column_new_with_attributes(_("Alias"), rdr,
			"markup", EMAIL_COL_ALIAS, NULL);
	gtk_tree_view_column_set_min_width(col, EMAIL_COL_WIDTH_ALIAS);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);

	col = gtk_tree_view_column_new_with_attributes(_("Remarks"), rdr,
			"markup", EMAIL_COL_REMARKS, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);
#endif

	gtk_container_add( GTK_CONTAINER(scrollwin), view );

	/* Data entry area */
	table = gtk_grid_new();

#ifndef GENERIC_UMPC
	gtk_container_add( GTK_CONTAINER(vboxl), scrollwin );
	gtk_box_pack_start(GTK_BOX(vboxl), table, FALSE, FALSE, 0);
#else
	gtk_box_pack_start(GTK_BOX(vboxl), table, FALSE, FALSE, 0);
	gtk_container_add( GTK_CONTAINER(vboxl), scrollwin );
#endif
	gtk_container_set_border_width( GTK_CONTAINER(table), 4 );
	gtk_grid_set_row_spacing(GTK_GRID(table), 4);
	gtk_grid_set_column_spacing(GTK_GRID(table), 4);

	entry_email = gtk_entry_new();
	entry_alias = gtk_entry_new();
	entry_remarks = gtk_entry_new();

	/* First row */
	label = gtk_label_new(_("Email Address"));
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_grid_attach(GTK_GRID(table), label, 0, 0, 1, 1);

	gtk_grid_attach(GTK_GRID(table), entry_email, 1, 0, 1, 1);
	gtk_widget_set_hexpand(entry_email, TRUE);
	gtk_widget_set_halign(entry_email, GTK_ALIGN_FILL);

#ifndef GENERIC_UMPC
	/* Next row */
	label = gtk_label_new(_("Alias"));
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_grid_attach(GTK_GRID(table), label, 0, 1, 1, 1);

	gtk_grid_attach(GTK_GRID(table), entry_alias, 1, 1, 1, 1);
	gtk_widget_set_hexpand(entry_alias, TRUE);
	gtk_widget_set_halign(entry_alias, GTK_ALIGN_FILL);

	/* Next row */
	label = gtk_label_new(_("Remarks"));
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_grid_attach(GTK_GRID(table), label, 0, 2, 1, 1);

	gtk_grid_attach(GTK_GRID(table), entry_remarks, 1, 2, 1, 1);
	gtk_widget_set_hexpand(entry_remarks, TRUE);
	gtk_widget_set_halign(entry_remarks, GTK_ALIGN_FILL);
#endif

	/* Button box */
	vboxb = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4 );
	gtk_box_pack_start(GTK_BOX(hbox), vboxb, FALSE, FALSE, 2);

	vbuttonbox = gtk_button_box_new(GTK_ORIENTATION_VERTICAL);
	gtk_button_box_set_layout( GTK_BUTTON_BOX(vbuttonbox), GTK_BUTTONBOX_START );
	gtk_box_set_spacing( GTK_BOX(vbuttonbox), 8 );
	gtk_container_set_border_width( GTK_CONTAINER(vbuttonbox), 4 );
	gtk_container_add( GTK_CONTAINER(vboxb), vbuttonbox );

	/* Buttons */
	buttonUp = gtkut_stock_button("go-up", _("_Up"));
	buttonDown = gtkut_stock_button("go-down", _("_Down"));
	buttonDel = gtkut_stock_button("edit-delete", _("D_elete"));
	buttonMod = gtkut_stock_button("document-save", _("_Save"));
	buttonAdd = gtkut_stock_button("list-add", _("_Add"));
	

#ifndef GENERIC_UMPC
	gtk_container_add( GTK_CONTAINER(vbuttonbox), buttonUp );

	gtk_container_add( GTK_CONTAINER(vbuttonbox), buttonDown );
#endif
	gtk_container_add( GTK_CONTAINER(vbuttonbox), buttonDel );

	gtk_container_add( GTK_CONTAINER(vbuttonbox), buttonMod );

	gtk_container_add( GTK_CONTAINER(vbuttonbox), buttonAdd );

	gtk_widget_show_all(vbox);

	/* Event handlers */
	g_signal_connect( G_OBJECT(view), "cursor-changed",
			  G_CALLBACK( edit_person_email_cursor_changed ), NULL );
	g_signal_connect( G_OBJECT(buttonUp), "clicked",
			  G_CALLBACK( edit_person_email_move_up ), NULL );
	g_signal_connect( G_OBJECT(buttonDown), "clicked",
			  G_CALLBACK( edit_person_email_move_down ), NULL );
	g_signal_connect( G_OBJECT(buttonDel), "clicked",
			  G_CALLBACK( edit_person_email_delete ), NULL );
	g_signal_connect( G_OBJECT(buttonMod), "clicked",
			  G_CALLBACK( edit_person_email_modify ), NULL );
	g_signal_connect( G_OBJECT(buttonAdd), "clicked",
			  G_CALLBACK( edit_person_email_add ), NULL );
	g_signal_connect(G_OBJECT(entry_email), "changed",
			 G_CALLBACK(edit_person_entry_email_changed), NULL);
	g_signal_connect(G_OBJECT(entry_email), "key_press_event",
			 G_CALLBACK(edit_person_entry_email_pressed), NULL);
	g_signal_connect(G_OBJECT(entry_alias), "key_press_event",
			 G_CALLBACK(edit_person_entry_email_pressed), NULL);
	g_signal_connect(G_OBJECT(entry_remarks), "key_press_event",
			 G_CALLBACK(edit_person_entry_email_pressed), NULL);

	personeditdlg.view_email   = view;
	personeditdlg.entry_email   = entry_email;
	personeditdlg.entry_alias   = entry_alias;
	personeditdlg.entry_remarks = entry_remarks;
	personeditdlg.email_up = buttonUp;
	personeditdlg.email_down = buttonDown;
	personeditdlg.email_del = buttonDel;
	personeditdlg.email_mod = buttonMod;
	personeditdlg.email_add = buttonAdd;
}

static gboolean attrib_adding = FALSE, attrib_saving = FALSE;

static void edit_person_entry_att_changed (GtkWidget *entry, gpointer data)
{
	GtkTreeModel *model = gtk_tree_view_get_model(
			GTK_TREE_VIEW(personeditdlg.view_attrib));
	gboolean non_empty = (gtk_tree_model_iter_n_children(model, NULL) > 0);
	const gchar *atname;

	if (personeditdlg.read_only || personeditdlg.ldap)
		return;

	atname = gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN((personeditdlg.entry_atname)))));
	if ( atname == NULL
	||  strlen(atname) == 0) {
		gtk_widget_set_sensitive(personeditdlg.attrib_add,FALSE);
		gtk_widget_set_sensitive(personeditdlg.attrib_mod,FALSE);
		attrib_adding = FALSE;
		attrib_saving = FALSE;
	} else if (list_find_attribute(atname)) {
		gtk_widget_set_sensitive(personeditdlg.attrib_add,FALSE);
		gtk_widget_set_sensitive(personeditdlg.attrib_mod,non_empty);
		attrib_adding = FALSE;
		attrib_saving = non_empty;
	} else {
		gtk_widget_set_sensitive(personeditdlg.attrib_add,TRUE);
		gtk_widget_set_sensitive(personeditdlg.attrib_mod,non_empty);
		attrib_adding = TRUE;
		attrib_saving = non_empty;
	}
}

static gboolean edit_person_entry_att_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event && (event->keyval == GDK_KEY_KP_Enter ||
	    event->keyval == GDK_KEY_Return)) {
		if (attrib_saving)
			edit_person_attrib_modify(NULL);
		else if (attrib_adding)
			edit_person_attrib_add(NULL);
	}
	return FALSE;
}

static void addressbook_edit_person_page_attrib( gint pageNum, gchar *pageLbl ) {
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *vboxl;
	GtkWidget *vboxb;
	GtkWidget *vbuttonbox;
	GtkWidget *buttonDel;
	GtkWidget *buttonMod;
	GtkWidget *buttonAdd;

	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *scrollwin;
	GtkWidget *view;
	GtkWidget *entry_name;
	GtkWidget *entry_value;
	GtkListStore *store;
	GtkTreeViewColumn *col;
	GtkCellRenderer *rdr;
	GtkTreeSelection *sel;

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8 );
	gtk_widget_show( vbox );
	gtk_container_add( GTK_CONTAINER( personeditdlg.notebook ), vbox );
	gtk_container_set_border_width( GTK_CONTAINER (vbox), BORDER_WIDTH );

	label = gtk_label_new_with_mnemonic( pageLbl );
	gtk_widget_show( label );
	gtk_notebook_set_tab_label(
		GTK_NOTEBOOK( personeditdlg.notebook ),
		gtk_notebook_get_nth_page( GTK_NOTEBOOK( personeditdlg.notebook ), pageNum ), label );

	/* Split into two areas */
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_container_add( GTK_CONTAINER( vbox ), hbox );

	/* Attribute list */
	vboxl = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4 );
	gtk_container_add( GTK_CONTAINER( hbox ), vboxl );
	gtk_container_set_border_width( GTK_CONTAINER(vboxl), 4 );

	scrollwin = gtk_scrolled_window_new( NULL, NULL );
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(scrollwin), TRUE);

	store = gtk_list_store_new(ATTRIB_N_COLS,
			G_TYPE_STRING, G_TYPE_STRING,
			G_TYPE_POINTER, -1);

	view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	g_object_unref(store);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), TRUE);
	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
	gtk_tree_selection_set_mode(sel, GTK_SELECTION_BROWSE);

	rdr = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes(_("Name"), rdr,
			"markup", ATTRIB_COL_NAME, NULL);
	gtk_tree_view_column_set_min_width(col, ATTRIB_COL_WIDTH_NAME);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);

	col = gtk_tree_view_column_new_with_attributes(_("Value"), rdr,
			"markup", ATTRIB_COL_VALUE, NULL);
	gtk_tree_view_column_set_min_width(col, ATTRIB_COL_WIDTH_VALUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);

	gtk_container_add( GTK_CONTAINER(scrollwin), view );

	/* Data entry area */
	table = gtk_grid_new();
	gtk_box_pack_start(GTK_BOX(vboxl), table, FALSE, FALSE, 0);
	gtk_container_add( GTK_CONTAINER(vboxl), scrollwin );
	gtk_container_set_border_width( GTK_CONTAINER(table), 4 );
	gtk_grid_set_row_spacing(GTK_GRID(table), 4);
	gtk_grid_set_column_spacing(GTK_GRID(table), 4);

	/* First row */
#ifndef GENERIC_UMPC
	label = gtk_label_new(_("Name"));
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_grid_attach(GTK_GRID(table), label, 0, 0, 1, 1);

	entry_name = gtk_combo_box_text_new_with_entry ();
	gtk_grid_attach(GTK_GRID(table), entry_name, 1, 0, 1, 1);
	gtk_widget_set_hexpand(entry_name, TRUE);
	gtk_widget_set_halign(entry_name, GTK_ALIGN_FILL);

	/* Next row */
	label = gtk_label_new(_("Value"));
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_grid_attach(GTK_GRID(table), label, 0, 1, 1, 1);

	entry_value = gtk_entry_new();
	gtk_grid_attach(GTK_GRID(table), entry_value, 1, 1, 1, 1);
	gtk_widget_set_hexpand(entry_value, TRUE);
	gtk_widget_set_halign(entry_value, GTK_ALIGN_FILL);
#else
	label = gtk_label_new(_("Name"));
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_grid_attach(GTK_GRID(table), label, 0, 0, 1, 1);

	entry_name = gtk_combo_box_text_new_with_entry ();
	gtk_grid_attach(GTK_GRID(table), entry_name, 1, 0, 1, 1);
	gtk_widget_set_hexpand(entry_name, TRUE);
	gtk_widget_set_halign(entry_name, GTK_ALIGN_FILL);

	/* Next row */
	label = gtk_label_new(_("Value"));
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_grid_attach(GTK_GRID(table), label, 2, 0, 1, 1);

	entry_value = gtk_entry_new();
	gtk_grid_attach(GTK_GRID(table), entry_value, 3, 0, 1, 1);
	gtk_widget_set_hexpand(entry_value, TRUE);
	gtk_widget_set_halign(entry_value, GTK_ALIGN_FILL);
#endif
	gtk_combo_box_set_active(GTK_COMBO_BOX(entry_name), -1);
	if (prefs_common.addressbook_custom_attributes)
		combobox_set_popdown_strings(GTK_COMBO_BOX_TEXT(entry_name),
				prefs_common.addressbook_custom_attributes);		
	/* Button box */
	vboxb = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4 );
	gtk_box_pack_start(GTK_BOX(hbox), vboxb, FALSE, FALSE, 2);

	vbuttonbox = gtk_button_box_new(GTK_ORIENTATION_VERTICAL);
	gtk_button_box_set_layout( GTK_BUTTON_BOX(vbuttonbox), GTK_BUTTONBOX_START );
	gtk_box_set_spacing( GTK_BOX(vbuttonbox), 8 );
	gtk_container_set_border_width( GTK_CONTAINER(vbuttonbox), 4 );
	gtk_container_add( GTK_CONTAINER(vboxb), vbuttonbox );

	/* Buttons */
	buttonDel = gtkut_stock_button("edit-delete", _("D_elete"));
	gtk_container_add( GTK_CONTAINER(vbuttonbox), buttonDel );

	buttonMod = gtkut_stock_button("document-save", _("_Save"));
	gtk_container_add( GTK_CONTAINER(vbuttonbox), buttonMod );

	buttonAdd = gtkut_stock_button("list-add", _("_Add"));
	gtk_container_add( GTK_CONTAINER(vbuttonbox), buttonAdd );
	
	gtk_widget_set_sensitive(buttonDel,FALSE);
	gtk_widget_set_sensitive(buttonMod,FALSE);
	gtk_widget_set_sensitive(buttonAdd,FALSE);

	gtk_widget_show_all(vbox);

	/* Event handlers */
	g_signal_connect( G_OBJECT(view), "cursor-changed",
			  G_CALLBACK( edit_person_attrib_cursor_changed ), NULL );
	g_signal_connect( G_OBJECT(buttonDel), "clicked",
			  G_CALLBACK( edit_person_attrib_delete ), NULL );
	g_signal_connect( G_OBJECT(buttonMod), "clicked",
			  G_CALLBACK( edit_person_attrib_modify ), NULL );
	g_signal_connect( G_OBJECT(buttonAdd), "clicked",
			  G_CALLBACK( edit_person_attrib_add ), NULL );
	g_signal_connect(G_OBJECT(entry_name), "changed",
			 G_CALLBACK(edit_person_entry_att_changed), NULL);
	g_signal_connect(G_OBJECT(entry_name), "key_press_event",
			 G_CALLBACK(edit_person_entry_att_pressed), NULL);
	g_signal_connect(G_OBJECT(entry_value), "key_press_event",
			 G_CALLBACK(edit_person_entry_att_pressed), NULL);

	personeditdlg.view_attrib  = view;
	personeditdlg.entry_atname  = entry_name;
	personeditdlg.entry_atvalue = entry_value;
	personeditdlg.attrib_add = buttonAdd;
	personeditdlg.attrib_del = buttonDel;
	personeditdlg.attrib_mod = buttonMod;
}

static void addressbook_edit_person_create( GtkWidget *parent, gboolean *cancelled ) {
	if (prefs_common.addressbook_use_editaddress_dialog)
		addressbook_edit_person_dialog_create( cancelled );
	else
		addressbook_edit_person_widgetset_create( parent, cancelled );
	addressbook_edit_person_page_basic( PAGE_BASIC, _( "_User Data" ) );
	addressbook_edit_person_page_email( PAGE_EMAIL, _( "_Email Addresses" ) );
#ifdef USE_LDAP
	if (personeditdlg.ldap)
		addressbook_edit_person_page_attrib_ldap(&personeditdlg, PAGE_ATTRIBUTES, _("O_ther Attributes"));
	else
#endif
		addressbook_edit_person_page_attrib( PAGE_ATTRIBUTES, _( "O_ther Attributes" ) );
	gtk_widget_show_all( personeditdlg.container );
}

/*
* Return list of email items.
*/
static GList *edit_person_build_email_list() {
	GtkWidget *view = personeditdlg.view_email;
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
	GtkTreeIter iter;
	GList *listEMail = NULL;
	ItemEMail *email;

	if (!gtk_tree_model_get_iter_first(model, &iter))
		return NULL;

	do {
		gtk_tree_model_get(model, &iter, EMAIL_COL_PTR, &email, -1);
		listEMail = g_list_append( listEMail, email );
	} while (gtk_tree_model_iter_next(model, &iter));

	return listEMail;
}

/*
* Return list of attributes.
*/
static GList *edit_person_build_attrib_list() {
	GtkTreeModel *model = gtk_tree_view_get_model(
			GTK_TREE_VIEW(personeditdlg.view_attrib));
	GtkTreeIter iter;
	GList *listAttrib = NULL;
	UserAttribute *attrib;

	/* Iterate through all the rows, selecting the one that
	 * matches. */
	if (!gtk_tree_model_get_iter_first(model, &iter))
		return FALSE;

	do {
		gtk_tree_model_get(model, &iter, ATTRIB_COL_PTR, &attrib, -1);
		listAttrib = g_list_append( listAttrib, attrib );
	} while (gtk_tree_model_iter_next(model, &iter));

	return listAttrib;
}

static void update_sensitivity(void)
{
	gtk_widget_set_sensitive(personeditdlg.entry_name,    !personeditdlg.read_only);
	gtk_widget_set_sensitive(personeditdlg.entry_first,   !personeditdlg.read_only);
	gtk_widget_set_sensitive(personeditdlg.entry_last,    !personeditdlg.read_only);
	gtk_widget_set_sensitive(personeditdlg.entry_nick,    !personeditdlg.read_only && !personeditdlg.ldap);
	gtk_widget_set_sensitive(personeditdlg.entry_email,   !personeditdlg.read_only);
	gtk_widget_set_sensitive(personeditdlg.entry_alias,   !personeditdlg.read_only && !personeditdlg.ldap);
	gtk_widget_set_sensitive(personeditdlg.entry_remarks, !personeditdlg.read_only && !personeditdlg.ldap);
	gtk_widget_set_sensitive(personeditdlg.email_up,      !personeditdlg.read_only);
	gtk_widget_set_sensitive(personeditdlg.email_down,    !personeditdlg.read_only);
	gtk_widget_set_sensitive(personeditdlg.email_del,     !personeditdlg.read_only);
	gtk_widget_set_sensitive(personeditdlg.email_mod,     !personeditdlg.read_only);
	gtk_widget_set_sensitive(personeditdlg.email_add,     !personeditdlg.read_only);
	gtk_widget_set_sensitive(personeditdlg.entry_atname,  !personeditdlg.read_only);
	gtk_widget_set_sensitive(personeditdlg.entry_atvalue, !personeditdlg.read_only);
	gtk_widget_set_sensitive(personeditdlg.attrib_add,    !personeditdlg.read_only);
	gtk_widget_set_sensitive(personeditdlg.attrib_del,    !personeditdlg.read_only);
	gtk_widget_set_sensitive(personeditdlg.attrib_mod,    !personeditdlg.read_only);
}

static void addressbook_edit_person_flush_transient( void )
{
	ItemPerson *person = current_person;
	EditAddressPostUpdateCallback callback = edit_person_close_post_update_cb;

	/* reset transient data */
	current_abf = NULL;
	current_person = NULL;
	current_parent_folder = NULL;
	edit_person_close_post_update_cb = NULL;

	/* post action to perform on addressbook side */
	if (callback)
		callback( person );
}

void addressbook_edit_person_invalidate( AddressBookFile *abf, ItemFolder *parent_folder,
										 ItemPerson *person )
{
	if (current_abf == NULL &&
		current_person == NULL &&
		current_parent_folder == NULL)
		/* edit address form is already hidden */
		return;

	/* unconditional invalidation or invalidating the currently edited item */
	if ( ( abf == NULL && person == NULL && parent_folder == NULL )
		|| (current_abf == abf ||
			current_person == person ||
			current_parent_folder == parent_folder))
		addressbook_edit_person_close( TRUE );
}

static gboolean addressbook_edit_person_close( gboolean cancelled )
{
	GList *listEMail = NULL;
	GList *listAttrib = NULL;
	GError *error = NULL;
	GtkTreeModel *model;

	listEMail = edit_person_build_email_list();
	listAttrib = edit_person_build_attrib_list();
	if( cancelled ) {
		model = gtk_tree_view_get_model(GTK_TREE_VIEW(personeditdlg.view_email));
		gtk_list_store_clear(GTK_LIST_STORE(model));
		model = gtk_tree_view_get_model(GTK_TREE_VIEW(personeditdlg.view_attrib));
		gtk_list_store_clear(GTK_LIST_STORE(model));
		addritem_free_list_email( listEMail );
		addritem_free_list_attribute( listAttrib );

		if (!prefs_common.addressbook_use_editaddress_dialog)
			gtk_widget_hide( personeditdlg.container );

		/* no callback, as we're discarding the form */
		edit_person_close_post_update_cb = NULL;
		addressbook_edit_person_flush_transient();
		current_person = NULL;

		/* set focus to the address list (this is done by the post_update_cb usually) */
		addressbook_address_list_set_focus();
		return FALSE;
	}

	if( current_person && current_abf ) {
		/* Update email/attribute list for existing current_person */
		addrbook_update_address_list( current_abf, current_person, listEMail );
		addrbook_update_attrib_list( current_abf, current_person, listAttrib );
	}
	else {
		/* Create new current_person and email/attribute list */
		if( ! cancelled && current_abf ) {
			current_person = addrbook_add_address_list( current_abf, current_parent_folder, listEMail );
			addrbook_add_attrib_list( current_abf, current_person, listAttrib );
		}
	}
	listEMail = NULL;
	listAttrib = NULL;

	if(!cancelled && current_person != NULL) {
		/* Set current_person stuff */		

		gchar *name;
		gchar *cn = edit_person_get_common_name_from_widgets();

		addritem_person_set_common_name( current_person, cn );
		g_free( cn );

		if (personeditdlg.picture_set) { 
			GdkPixbuf * pixbuf = gtk_image_get_pixbuf(GTK_IMAGE(personeditdlg.image));

			if (!current_person->picture)
				name = g_strconcat( get_rc_dir(), G_DIR_SEPARATOR_S, ADDRBOOK_DIR, G_DIR_SEPARATOR_S, 
						ADDRITEM_ID(current_person), ".png", NULL );
			else 
				name = g_strconcat( get_rc_dir(), G_DIR_SEPARATOR_S, ADDRBOOK_DIR, G_DIR_SEPARATOR_S, 
						current_person->picture, ".png", NULL );

			gdk_pixbuf_save(pixbuf, name, "png", &error, NULL);
			if (error) {
				alertpanel_error(_("Failed to save image: \n%s"),
						error->message);
				g_error_free(error);
			} else {
				debug_print("saved picture to %s\n", name);
			}
			if (!current_person->picture)
				addritem_person_set_picture( current_person, ADDRITEM_ID(current_person) ) ;
			g_free( name );
		} else {
			if (!current_person->picture)
				name = g_strconcat( get_rc_dir(), G_DIR_SEPARATOR_S, ADDRBOOK_DIR, G_DIR_SEPARATOR_S, 
						ADDRITEM_ID(current_person), ".png", NULL );
			else 
				name = g_strconcat( get_rc_dir(), G_DIR_SEPARATOR_S, ADDRBOOK_DIR, G_DIR_SEPARATOR_S, 
						current_person->picture, ".png", NULL );
			claws_unlink(name);
			g_free(name);
		}
		name = gtk_editable_get_chars( GTK_EDITABLE(personeditdlg.entry_first), 0, -1 );
		addritem_person_set_first_name( current_person, name );
		g_free( name );
		name = gtk_editable_get_chars( GTK_EDITABLE(personeditdlg.entry_last), 0, -1 );
		addritem_person_set_last_name( current_person, name );
		g_free( name );
		name = gtk_editable_get_chars( GTK_EDITABLE(personeditdlg.entry_nick), 0, -1 );
		addritem_person_set_nick_name( current_person, name );
		g_free( name );
	}

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(personeditdlg.view_email));
	gtk_list_store_clear(GTK_LIST_STORE(model));
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(personeditdlg.view_attrib));
	gtk_list_store_clear(GTK_LIST_STORE(model));

	if (!prefs_common.addressbook_use_editaddress_dialog)
		gtk_widget_hide( personeditdlg.container );

	addressbook_edit_person_flush_transient();

	return TRUE;
}
 
/*
* Edit person.
* Enter: abf    Address book.
*        parent Parent folder for person (or NULL if adding to root folder). Argument is
*               only required for new objects).
*        person Person to edit, or NULL for a new person object.
*        pgMail If TRUE, E-Mail page will be activated.
* Return: Edited object, or NULL if cancelled.*/
ItemPerson *addressbook_edit_person( AddressBookFile *abf, ItemFolder *parent_folder, ItemPerson *person,
									 gboolean pgMail, GtkWidget *parent_container,
									 void (*post_update_cb) (ItemPerson *person),
									 gboolean get_focus) {
	static gboolean cancelled;
	GError *error = NULL;
	GdkPixbuf *pixbuf = NULL;
	GtkTreeModel *model;
	GtkTreeSelection *sel;
	GtkTreeIter iter;

	/* set transient data */
	current_abf = abf;
	current_person = person;
	current_parent_folder = parent_folder;
	edit_person_close_post_update_cb = post_update_cb;
	personeditdlg.ldap = (abf && abf->type == ADBOOKTYPE_LDAP)? TRUE : FALSE;

	if( personeditdlg.container ) {
		gtk_widget_destroy(personeditdlg.container);
		personeditdlg.container = NULL;
	}
	addressbook_edit_person_create(parent_container, &cancelled);

	/* typically, get focus when dialog mode is enabled, or when editing a new address */
	if( get_focus ) {
		gtk_widget_grab_focus(personeditdlg.ok_btn);
		gtk_widget_grab_focus(personeditdlg.entry_name);
	}
	
	personeditdlg.read_only = (current_abf == NULL);
	update_sensitivity();

	gtk_widget_show(personeditdlg.container);
	if (prefs_common.addressbook_use_editaddress_dialog)
		manage_window_set_transient(GTK_WINDOW(personeditdlg.container));
	else
		if (get_focus)
			addressbook_address_list_disable_some_actions();

	/* Clear all fields */
	edit_person_status_show( "" );

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(personeditdlg.view_email));
	gtk_list_store_clear(GTK_LIST_STORE(model));
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(personeditdlg.view_attrib));
	gtk_list_store_clear(GTK_LIST_STORE(model));

	gtk_entry_set_text(GTK_ENTRY(personeditdlg.entry_name), "" );
	gtk_entry_set_text(GTK_ENTRY(personeditdlg.entry_first), "" );
	gtk_entry_set_text(GTK_ENTRY(personeditdlg.entry_last), "" );
	gtk_entry_set_text(GTK_ENTRY(personeditdlg.entry_nick), "" );

	personeditdlg.editNew = FALSE;
	if( current_person ) {
		gchar *filename = NULL;

		if( ADDRITEM_NAME(current_person) )
			gtk_entry_set_text(GTK_ENTRY(personeditdlg.entry_name), ADDRITEM_NAME(person) );

		cm_menu_set_sensitive("EditAddressPopup/SetPicture", !personeditdlg.ldap);
		cm_menu_set_sensitive("EditAddressPopup/UnsetPicture", !personeditdlg.ldap);
		if( current_person->picture ) {	
			filename = g_strconcat( get_rc_dir(), G_DIR_SEPARATOR_S, ADDRBOOK_DIR, G_DIR_SEPARATOR_S, 
							current_person->picture, ".png", NULL );
			if (is_file_exist(filename)) {
				pixbuf = gdk_pixbuf_new_from_file(filename, &error);
				if (error) {
					debug_print("Failed to import image: %s\n",
							error->message);
					g_error_free(error);
					goto no_img;
				}
				personeditdlg.picture_set = TRUE;
				cm_menu_set_sensitive("EditAddressPopup/UnsetPicture", !personeditdlg.ldap && personeditdlg.picture_set);
			} else {
				goto no_img;
			}
			gtk_image_set_from_pixbuf(GTK_IMAGE(personeditdlg.image), pixbuf);
		} else {
no_img:
			addressbook_edit_person_clear_picture();
		}
		
		g_free(filename);
		if (pixbuf) {
			g_object_unref(pixbuf);
			pixbuf = NULL;
		}

		if( current_person->firstName )
			gtk_entry_set_text(GTK_ENTRY(personeditdlg.entry_first), current_person->firstName );
		if( current_person->lastName )
			gtk_entry_set_text(GTK_ENTRY(personeditdlg.entry_last), current_person->lastName );
		if( current_person->nickName )
			gtk_entry_set_text(GTK_ENTRY(personeditdlg.entry_nick), current_person->nickName );
		edit_person_load_email( current_person );
		edit_person_load_attrib( current_person );
		gtk_entry_set_text(GTK_ENTRY(personeditdlg.entry_atvalue), "");
	}
	else {
		personeditdlg.editNew = TRUE;
	}

	/* Select appropriate start page */
	if( pgMail ) {
		gtk_notebook_set_current_page( GTK_NOTEBOOK(personeditdlg.notebook), PAGE_EMAIL );
	}
	else {
		gtk_notebook_set_current_page( GTK_NOTEBOOK(personeditdlg.notebook), PAGE_BASIC );
	}

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(personeditdlg.view_email));
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(personeditdlg.view_email));
	if (gtk_tree_model_get_iter_first(model, &iter)) {
		gtk_tree_selection_select_iter(sel, &iter);
		edit_person_email_cursor_changed(
				GTK_TREE_VIEW(personeditdlg.view_email), NULL);
	}

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(personeditdlg.view_attrib));
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(personeditdlg.view_attrib));
	if (gtk_tree_model_get_iter_first(model, &iter))
		gtk_tree_selection_select_iter(sel, &iter);
	edit_person_attrib_cursor_changed(
			GTK_TREE_VIEW(personeditdlg.view_attrib), NULL);

	edit_person_email_clear( NULL );
	edit_person_attrib_clear( NULL );

	if (prefs_common.addressbook_use_editaddress_dialog) {
		gtk_main();
		gtk_widget_hide( personeditdlg.container );

		if (!addressbook_edit_person_close( cancelled )) {
			return NULL;
		}
	}
	
	return person;
}

void addressbook_edit_reload_attr_list( void )
{
	if (personeditdlg.entry_atname) {
		combobox_unset_popdown_strings(GTK_COMBO_BOX_TEXT(personeditdlg.entry_atname));
		if (prefs_common.addressbook_custom_attributes)
			combobox_set_popdown_strings(GTK_COMBO_BOX_TEXT(personeditdlg.entry_atname),
					prefs_common.addressbook_custom_attributes);
	}
}

/*
* End of Source.
*/

