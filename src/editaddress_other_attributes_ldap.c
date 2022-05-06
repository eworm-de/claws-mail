/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2021 Michael Rasmussen and the Claws Mail team
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

#ifdef USE_LDAP

#include "defs.h"

#include "mgutils.h"
#include "addressbook.h"
#include "addressitem.h"
#include "addritem.h"
#include "addrbook.h"
#include "manage_window.h"
#include "gtkutils.h"
#include "codeconv.h"
#include "editaddress.h"
#include "editaddress_other_attributes_ldap.h"
#include "prefs_common.h"

#define ATTRIB_COL_WIDTH_NAME	120
#define ATTRIB_COL_WIDTH_VALUE	180

PersonEditDlg *personEditDlg;
gboolean attrib_adding = FALSE, attrib_saving = FALSE;

int get_attribute_index(const gchar *string_literal) {
	int i = 0;
	/*int count = sizeof(ATTRIBUTE) / sizeof(*ATTRIBUTE);*/
	const gchar **attribute = ATTRIBUTE;

	cm_return_val_if_fail(string_literal != NULL, -1);
	while (*attribute) {
		debug_print("Comparing %s to %s\n", *attribute, string_literal);
		if (strcmp(*attribute++, string_literal) == 0)
			return i;
		i++;
	}
	return -1;
}

static void edit_person_status_show(gchar *msg) {
	if (personEditDlg->statusbar != NULL) {
		gtk_statusbar_pop(GTK_STATUSBAR(personEditDlg->statusbar), personEditDlg->status_cid);
		if(msg) {
			gtk_statusbar_push(GTK_STATUSBAR(personEditDlg->statusbar), personEditDlg->status_cid, msg);
		}
	}
}

static void edit_person_attrib_clear(gpointer data) {
	gtk_combo_box_set_active(GTK_COMBO_BOX(personEditDlg->entry_atname), 0);
	gtk_entry_set_text(GTK_ENTRY(personEditDlg->entry_atvalue), "");
}

static gboolean list_find_attribute(const gchar *attr)
{
	GtkWidget *view = personEditDlg->view_attrib;
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

static void edit_person_combo_box_changed(GtkComboBox *opt_menu, gpointer data)
{
	GtkWidget *view = GTK_WIDGET(data);
	UserAttribute *attrib = gtkut_tree_view_get_selected_pointer(
			GTK_TREE_VIEW(view), ATTRIB_COL_PTR, NULL, NULL, NULL);
	gint option = gtk_combo_box_get_active(opt_menu);
	const gchar *str = attrib ? attrib->name:"";

	cm_return_if_fail (option < ATTRIBUTE_SIZE);
	/* A corresponding attribute in contact does not match selected option */ 
	if (strcmp(ATTRIBUTE[option], str) != 0) {
		gtk_widget_set_sensitive(personEditDlg->attrib_add, TRUE);
		gtk_widget_set_sensitive(personEditDlg->attrib_mod, TRUE);
		gtk_widget_set_sensitive(personEditDlg->attrib_del, FALSE);
		gtk_entry_set_text(GTK_ENTRY(personEditDlg->entry_atvalue), "");
		gtk_widget_grab_focus(personEditDlg->entry_atvalue);
		edit_person_status_show(NULL);
	}
}

static void edit_person_attrib_cursor_changed(GtkTreeView *view,
		gpointer user_data)
{
	UserAttribute *attrib = gtkut_tree_view_get_selected_pointer(
			view, ATTRIB_COL_PTR, NULL, NULL, NULL);

	if( attrib && !personEditDlg->read_only ) {
		gint index = get_attribute_index(attrib->name);

		if (index == -1)
			index = 0;

		gtk_combo_box_set_active(GTK_COMBO_BOX(personEditDlg->entry_atname), index);
		gtk_entry_set_text(GTK_ENTRY(personEditDlg->entry_atvalue), attrib->value);

		gtk_widget_set_sensitive(personEditDlg->attrib_del, TRUE);
	} else {
		gtk_widget_set_sensitive(personEditDlg->attrib_del, FALSE);
	}
	edit_person_status_show( NULL );
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
			GTK_TREE_VIEW(personEditDlg->view_attrib), ATTRIB_COL_PTR,
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

	if (!has_row)
		gtk_tree_selection_select_iter(sel, &iter);

	edit_person_attrib_cursor_changed(
			GTK_TREE_VIEW(personEditDlg->view_attrib), NULL);
}

static UserAttribute *edit_person_attrib_edit(gboolean *error, UserAttribute *attrib) {
	UserAttribute *retVal = NULL;
	gchar *sName, *sValue, *sName_, *sValue_;
	gint index;

	*error = TRUE;
	index = gtk_combo_box_get_active(GTK_COMBO_BOX(personEditDlg->entry_atname));
	sName_ = (gchar *) ATTRIBUTE[index];
	sValue_ = gtk_editable_get_chars(GTK_EDITABLE(personEditDlg->entry_atvalue), 0, -1);
	sName = mgu_email_check_empty(sName_);
	sValue = mgu_email_check_empty(sValue_);
	g_free(sValue_);

	if (sName && sValue) {
		debug_print("sname && svalue\n");
		if (attrib == NULL) {
			attrib = addritem_create_attribute();
		}
		addritem_attrib_set_name(attrib, sName);
		addritem_attrib_set_value(attrib, sValue);
		retVal = attrib;
		*error = FALSE;
	}
	else {
		edit_person_status_show(N_( "A Name and Value must be supplied." ));
		gtk_widget_grab_focus(personEditDlg->entry_atvalue);		
	}

	g_free(sName);
	g_free(sValue);

	return retVal;
}

static void edit_person_attrib_modify(gpointer data) {
	gboolean errFlg = FALSE;
	GtkTreeModel *model;
	GtkTreeIter iter;
	UserAttribute *attrib;

	attrib = gtkut_tree_view_get_selected_pointer(
			GTK_TREE_VIEW(personEditDlg->view_attrib), ATTRIB_COL_PTR,
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
	GtkTreeModel *model = gtk_tree_view_get_model(
			GTK_TREE_VIEW(personEditDlg->view_attrib));
	GtkTreeSelection *sel = gtk_tree_view_get_selection(
			GTK_TREE_VIEW(personEditDlg->view_attrib));
	GtkTreeIter iter, iter2;
	UserAttribute *attrib, *prev_attrib;

	attrib = edit_person_attrib_edit(&errFlg, NULL);
	if (attrib == NULL) /* input for new attribute is not valid */
		return;

	prev_attrib = gtkut_tree_view_get_selected_pointer(
			GTK_TREE_VIEW(personEditDlg->view_attrib), ATTRIB_COL_PTR,
			&model, &sel, &iter);

	if (prev_attrib == NULL) {
		/* No row selected, or list empty, add it as first row. */
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

static void edit_person_entry_att_changed (GtkWidget *entry, gpointer data)
{
	GtkTreeModel *model = gtk_tree_view_get_model(
			GTK_TREE_VIEW(personEditDlg->view_attrib));
	gboolean non_empty = (gtk_tree_model_iter_n_children(model, NULL) > 0);
	const gchar *sName;
	int index;

	if (personEditDlg->read_only)
		return;

	index = gtk_combo_box_get_active(GTK_COMBO_BOX(personEditDlg->entry_atname));
	sName = ATTRIBUTE[index];
	if (list_find_attribute(sName)) {
		gtk_widget_set_sensitive(personEditDlg->attrib_add,FALSE);
		gtk_widget_set_sensitive(personEditDlg->attrib_mod,non_empty);
		attrib_adding = FALSE;
		attrib_saving = non_empty;
	} 
	else {
		gtk_widget_set_sensitive(personEditDlg->attrib_add,TRUE);
		gtk_widget_set_sensitive(personEditDlg->attrib_mod,non_empty);
		attrib_adding = TRUE;
		attrib_saving = non_empty;
	}
}

static gboolean edit_person_entry_att_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event && (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter)) {
		if (attrib_saving)
			edit_person_attrib_modify(NULL);
		else if (attrib_adding)
			edit_person_attrib_add(NULL);
	}
	return FALSE;
}

void addressbook_edit_person_page_attrib_ldap(PersonEditDlg *dialog, gint pageNum, gchar *pageLbl) {
	GtkWidget *combo_box;
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
	GtkWidget *entry_value;
	GtkListStore *store;
	GtkTreeViewColumn *col;
	GtkCellRenderer *rdr;
	GtkTreeSelection *sel;

	personEditDlg = dialog;

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_widget_show(vbox);
	gtk_container_add(GTK_CONTAINER(personEditDlg->notebook), vbox);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), BORDER_WIDTH);

	label = gtk_label_new_with_mnemonic(pageLbl);
	gtk_widget_show(label);
	gtk_notebook_set_tab_label(
		GTK_NOTEBOOK(personEditDlg->notebook),
		gtk_notebook_get_nth_page(GTK_NOTEBOOK(personEditDlg->notebook), pageNum), label);

	/* Split into two areas */
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_container_add(GTK_CONTAINER(vbox), hbox);

	/* Attribute list */
	vboxl = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	gtk_container_add(GTK_CONTAINER(hbox), vboxl);
	gtk_container_set_border_width(GTK_CONTAINER(vboxl), 4);

	scrollwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(vboxl), scrollwin);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwin),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

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

	gtk_container_add(GTK_CONTAINER(scrollwin), view);

	/* Data entry area */
	table = gtk_grid_new();
	gtk_box_pack_start(GTK_BOX(vboxl), table, FALSE, FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(table), 4);
	gtk_grid_set_row_spacing(GTK_GRID(table), 4);
	gtk_grid_set_column_spacing(GTK_GRID(table), 4);

	/* First row */
	label = gtk_label_new(N_("Name"));
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_grid_attach(GTK_GRID(table), label, 0, 0, 1, 1);

	gchar **attribute = (gchar **) ATTRIBUTE;

	combo_box = gtk_combo_box_text_new();

	while (*attribute) {
		if (!strcmp(*attribute, "jpegPhoto")) {
			attribute++;
			continue;
		}
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_box), *attribute++);
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo_box), 0);

	gtk_grid_attach(GTK_GRID(table), combo_box, 1, 0, 1, 1);
	gtk_widget_set_hexpand(combo_box, TRUE);
	gtk_widget_set_halign(combo_box, GTK_ALIGN_FILL);

	/* Next row */
	label = gtk_label_new(N_("Value"));
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_grid_attach(GTK_GRID(table), label, 0, 1, 1, 1);

	entry_value = gtk_entry_new();
	gtk_grid_attach(GTK_GRID(table), entry_value, 1, 1, 1, 1);
	gtk_widget_set_hexpand(entry_value, TRUE);
	gtk_widget_set_halign(entry_value, GTK_ALIGN_FILL);

	/* Button box */
	vboxb = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	gtk_box_pack_start(GTK_BOX(hbox), vboxb, FALSE, FALSE, 2);

	vbuttonbox = gtk_button_box_new(GTK_ORIENTATION_VERTICAL);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(vbuttonbox), GTK_BUTTONBOX_START);
	gtk_box_set_spacing(GTK_BOX(vbuttonbox), 8);
	gtk_container_set_border_width(GTK_CONTAINER(vbuttonbox), 4);
	gtk_container_add(GTK_CONTAINER(vboxb), vbuttonbox);

	/* Buttons */
	buttonDel = gtkut_stock_button("edit-delete", _("D_elete"));
	gtk_container_add(GTK_CONTAINER(vbuttonbox), buttonDel);

	buttonMod = gtkut_stock_button("document-save", _("_Save"));
	gtk_container_add(GTK_CONTAINER(vbuttonbox), buttonMod);

	buttonAdd = gtkut_stock_button("list-add", _("_Add"));
	gtk_container_add(GTK_CONTAINER(vbuttonbox), buttonAdd);
	
	gtk_widget_set_sensitive(buttonDel,FALSE);
	gtk_widget_set_sensitive(buttonMod,FALSE);
	gtk_widget_set_sensitive(buttonAdd,FALSE);

	gtk_widget_show_all(vbox);
	
	/* Event handlers */
	g_signal_connect(G_OBJECT(view), "cursor-changed",
			G_CALLBACK(edit_person_attrib_cursor_changed), NULL);
	g_signal_connect(G_OBJECT(buttonDel), "clicked",
			  G_CALLBACK(edit_person_attrib_delete), NULL);
	g_signal_connect(G_OBJECT(buttonMod), "clicked",
			  G_CALLBACK(edit_person_attrib_modify), NULL);
	g_signal_connect(G_OBJECT(buttonAdd), "clicked",
			  G_CALLBACK(edit_person_attrib_add), NULL);
	g_signal_connect(G_OBJECT(combo_box), "changed",
			 G_CALLBACK(edit_person_entry_att_changed), NULL);
	g_signal_connect(G_OBJECT(entry_value), "key_press_event",
			 G_CALLBACK(edit_person_entry_att_pressed), NULL);
	g_signal_connect(G_OBJECT(combo_box), "changed",
			 G_CALLBACK(edit_person_combo_box_changed), view);

	personEditDlg->view_attrib  = view;
	personEditDlg->entry_atname  = combo_box;
	personEditDlg->entry_atvalue = entry_value;
	personEditDlg->attrib_add = buttonAdd;
	personEditDlg->attrib_del = buttonDel;
	personEditDlg->attrib_mod = buttonMod;
}

#endif	/* USE_LDAP */


