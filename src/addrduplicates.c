/* Claws Mail -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2007 Holger Berndt <hb@claws-mail.org> 
 * and the Claws Mail team
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
#endif

#include <glib/gi18n.h>
#include <string.h>

#include "defs.h"

#ifdef USE_LDAP
#include "ldapserver.h"
#include "ldapupdate.h"
#endif
#include "addrduplicates.h"
#include "addrindex.h"
#include "addrbook.h"
#include "addressbook.h"
#include "editaddress.h"
#include "alertpanel.h"
#include "gtkutils.h"
#include "inc.h"
#include "utils.h"

typedef struct
{
	ItemPerson        *person;
	AddressDataSource *ds;
}
AddrDupListEntry;

enum {
    COL_BOOK = 0,
    COL_NAME,
    COL_ITEM,
    COL_DS,
    NUM_COLS
};

static gboolean create_dialog();
static void refresh_addr_hash(void);
static void refresh_stores(void);
static void present_finder_results(GtkWindow*);
static void cb_finder_results_dialog_destroy(GtkWindow*, gpointer);
static void destroy_addr_hash_val(gpointer);
static GSList* deep_copy_hash_val(GSList*);
static void fill_hash_table();
static gint collect_emails(ItemPerson*, AddressDataSource*);
static gboolean is_not_duplicate(gpointer, gpointer, gpointer);
static gint books_compare(gconstpointer, gconstpointer);
static GtkWidget* create_email_view(GtkListStore*);
static GtkWidget* create_detail_view(GtkListStore*);
static void append_to_email_store(gpointer,gpointer,gpointer);
static void email_selection_changed(GtkTreeSelection*,gpointer);
static void detail_selection_changed(GtkTreeSelection*,gpointer);
static void detail_row_activated(GtkTreeView*,GtkTreePath*,
                                 GtkTreeViewColumn*,
                                 gpointer);
static void cb_del_btn_clicked(GtkButton *, gpointer);
static gboolean delete_item(ItemPerson*, AddressDataSource*);

static GHashTable *addr_hash;
static gboolean include_same_book = TRUE;
static gboolean include_other_books = TRUE;

static GtkListStore *email_store;
static GtkListStore *detail_store;

static GtkWidget *del_btn;

void addrduplicates_find(GtkWindow *parent)
{
	if(create_dialog()) {
		refresh_addr_hash();
		present_finder_results(parent);
	}
}

static gboolean create_dialog()
{
	gboolean want_search;
	GtkWidget *vbox;
	GtkWidget *check_same_book;
	GtkWidget *check_other_book;
	AlertValue val;

	want_search = FALSE;

	vbox = gtk_vbox_new(FALSE, 0);
	check_same_book = gtk_check_button_new_with_label(_("Show duplicates in "
	                  "the same book"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_same_book),
	                             include_same_book);
	gtk_box_pack_start(GTK_BOX(vbox), check_same_book, FALSE, FALSE, 0);
	gtk_widget_show(check_same_book);
	check_other_book = gtk_check_button_new_with_label(_("Show duplicates in "
	                   "different books"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_other_book),
	                             include_other_books);
	gtk_box_pack_start(GTK_BOX(vbox), check_other_book, FALSE, FALSE, 0);
	gtk_widget_show(check_other_book);

	/* prevent checkboxes from being destroyed on dialog close */
	g_object_ref(check_same_book);
	g_object_ref(check_other_book);

	val = alertpanel_full(_("Find address book email duplicates"),
	                      _("Claws-Mail will now search for duplicate email "
	                        "addresses in the addressbook."),
	                      _("Cancel"),_("Search"),NULL, FALSE, vbox, ALERT_NOTICE,
	                      G_ALERTALTERNATE);
	if(val == G_ALERTALTERNATE) {
		want_search = TRUE;

		/* save options */
		include_same_book =
		    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_same_book));
		include_other_books =
		    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_other_book));

	}

	g_object_unref(check_same_book);
	g_object_unref(check_other_book);
	return want_search;
}

static void refresh_addr_hash(void)
{
	if(addr_hash)
		g_hash_table_destroy(addr_hash);
	addr_hash = g_hash_table_new_full(g_str_hash, g_str_equal,
	                                  g_free, destroy_addr_hash_val);
	fill_hash_table();
}

static void destroy_addr_hash_val(gpointer value)
{
	GSList *list = (GSList*) value;
	GSList *walk;

	for(walk = list; walk; walk = walk->next) {
		AddrDupListEntry *entry = (AddrDupListEntry*) walk->data;
		if(entry)
			g_free(entry);
	}
	if(list)
		g_slist_free(list);
}

static GSList* deep_copy_hash_val(GSList *in)
{
	GSList *walk;
	GSList *out = NULL;

	out = g_slist_copy(in);
	for(walk = out; walk; walk = walk->next) {
		AddrDupListEntry *out_entry;
		AddrDupListEntry *in_entry = walk->data;

		out_entry = g_new0(AddrDupListEntry,1);
		out_entry->person = in_entry->person;
		out_entry->ds     = in_entry->ds;
		walk->data = out_entry;
	}

	return out;
}

static void fill_hash_table()
{
	addrindex_load_person_ds(collect_emails);
	g_hash_table_foreach_remove(addr_hash,is_not_duplicate, NULL);
}

static gboolean is_not_duplicate(gpointer key, gpointer value,
                                 gpointer user_data)
{
	gboolean is_in_same_book;
	gboolean is_in_other_books;
	GSList *books;
	GSList *walk;
	gboolean retval;
	GSList *list = value;

	/* remove everything that is just in one book */
	if(g_slist_length(list) <= 1)
		return TRUE;

	/* work on a shallow copy */
	books = g_slist_copy(list);

	/* sorting the list makes it easier to check for books */
	books = g_slist_sort(books, books_compare);

	/* check if a book appears twice */
	is_in_same_book = FALSE;
	for(walk = books; walk && walk->next; walk = walk->next) {
		if(books_compare(walk->data, walk->next->data) == 0) {
			is_in_same_book = TRUE;
			break;
		}
	}

	/* check is at least two different books appear in the list */
	is_in_other_books = FALSE;
	if(books && books->next) {
		for(walk = books->next; walk; walk = walk->next) {
			if(books_compare(walk->data, books->data) != 0) {
				is_in_other_books = TRUE;
				break;
			}
		}
	}

	/* delete the shallow copy */
	g_slist_free(books);

	retval = FALSE;
	if(is_in_same_book && include_same_book)
		retval = TRUE;
	if(is_in_other_books && include_other_books)
		retval = TRUE;
	retval = !retval;

	return retval;
}

static gint collect_emails(ItemPerson *itemperson, AddressDataSource *ds)
{
	gchar *addr;
	GList *nodeM;
	GSList *old_val;
	GSList *new_val;
	AddrDupListEntry *entry;

	/* Process each E-Mail address */
	nodeM = itemperson->listEMail;
	while(nodeM) {
		ItemEMail *email = nodeM->data;

		addr = g_strdup(email->address);
		g_strdown(addr);
		old_val = g_hash_table_lookup(addr_hash, addr);
		if(old_val)
			new_val = deep_copy_hash_val(old_val);
		else
			new_val = NULL;

		entry = g_new0(AddrDupListEntry,1);
		entry->person = itemperson;
		entry->ds     = ds;

		new_val = g_slist_prepend(new_val, entry);
		g_hash_table_insert(addr_hash, addr, new_val);

		nodeM = g_list_next(nodeM);
	}
	return 0;
}

static gint books_compare(gconstpointer a, gconstpointer b)
{
	const AddrDupListEntry *entry1;
	const AddrDupListEntry *entry2;
	entry1 = a;
	entry2 = b;
	return strcmp(addrindex_ds_get_name(entry1->ds),
	              addrindex_ds_get_name(entry2->ds));
}

static void present_finder_results(GtkWindow *parent)
{
	GtkWidget *email_view;
	GtkWidget *detail_view;
	GtkWidget *dialog = NULL;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *hpaned;
	GtkWidget *close;
	GtkTreeSelection *email_select;
	GtkTreeSelection *detail_select;
	static GdkGeometry geometry;

	if(g_hash_table_size(addr_hash) == 0) {
		alertpanel_notice(_("No duplicate email addresses in the addressbook found"));
		return;
	}

	email_store = gtk_list_store_new(1, G_TYPE_STRING);
	refresh_stores();
	email_view = create_email_view(email_store);
	email_select = gtk_tree_view_get_selection(GTK_TREE_VIEW(email_view));
	gtk_tree_selection_set_mode(email_select,GTK_SELECTION_SINGLE);

	g_signal_connect(email_select, "changed",
	                 (GCallback)email_selection_changed, NULL);

	detail_store = gtk_list_store_new(NUM_COLS, G_TYPE_STRING, G_TYPE_STRING,
	                                  G_TYPE_POINTER, G_TYPE_POINTER);
	detail_view = create_detail_view(detail_store);
	detail_select = gtk_tree_view_get_selection(GTK_TREE_VIEW(detail_view));
	gtk_tree_selection_set_mode(detail_select,GTK_SELECTION_MULTIPLE);

	g_signal_connect(detail_select, "changed",
	                 (GCallback)detail_selection_changed, NULL);

	dialog = gtkut_window_new(GTK_WINDOW_TOPLEVEL, "address_dupes_finder");
	gtk_window_set_transient_for(GTK_WINDOW(dialog),parent);
	gtk_window_set_modal(GTK_WINDOW(dialog),TRUE);
	if(!geometry.min_height) {
		geometry.min_width = 600;
		geometry.min_height = 400;
	}
	gtk_window_set_geometry_hints(GTK_WINDOW(dialog), NULL, &geometry,
	                              GDK_HINT_MIN_SIZE);
	gtk_window_set_title(GTK_WINDOW(dialog), _("Duplicate email addresses"));


	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(dialog), vbox);

	hpaned = gtk_hpaned_new();
	gtk_box_pack_start(GTK_BOX(vbox), hpaned, TRUE, TRUE, 0);

	gtk_paned_add1(GTK_PANED(hpaned), email_view);
	gtk_paned_add2(GTK_PANED(hpaned), detail_view);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	close = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	gtk_box_pack_end(GTK_BOX(hbox), close, FALSE, FALSE, 5);

	del_btn = gtk_button_new_from_stock(GTK_STOCK_DELETE);
	gtk_box_pack_end(GTK_BOX(hbox), del_btn, FALSE, FALSE, 5);
	gtk_widget_set_sensitive(del_btn, FALSE);

	g_signal_connect(dialog, "destroy",
	                 G_CALLBACK(cb_finder_results_dialog_destroy), NULL);
	g_signal_connect_swapped(close, "clicked",
	                         G_CALLBACK(gtk_widget_destroy), dialog);
	g_signal_connect(del_btn, "clicked",
	                 G_CALLBACK(cb_del_btn_clicked), detail_view);
	inc_lock();
	gtk_widget_show_all(dialog);
}

static void cb_finder_results_dialog_destroy(GtkWindow *win, gpointer data)
{
	email_store = NULL;
	detail_store = NULL;

	if(addr_hash) {
		g_hash_table_destroy(addr_hash);
		addr_hash = NULL;
	}
	addressbook_refresh();
	inc_unlock();
}

static GtkWidget* create_email_view(GtkListStore *store)
{
	GtkWidget *view;
	GtkCellRenderer *renderer;

	view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view),
	        -1,
	        _("Address"),
	        renderer,
	        "text", 0,
	        NULL);
	g_object_unref(store);
	return view;
}

static GtkWidget* create_detail_view(GtkListStore *store)
{
	GtkWidget *view;
	GtkCellRenderer *renderer;

	view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	renderer = gtk_cell_renderer_text_new();

	/* col 1 */
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view),
	        -1,
	        _("Bookname"),
	        renderer,
	        "text", COL_BOOK,
	        NULL);
	/* col 2 */
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view),
	        -1,
	        _("Name"),
	        renderer,
	        "text", COL_NAME,
	        NULL);

	g_signal_connect(view, "row-activated",
	                 (GCallback)detail_row_activated, NULL);

	return view;
}

static void append_to_email_store(gpointer key,gpointer value,gpointer data)
{
	GtkTreeIter iter;
	GtkListStore *store = (GtkListStore*) data;

	gtk_list_store_append(store, &iter);
	gtk_list_store_set(store, &iter, 0, (gchar*) key, -1);
}

static void detail_selection_changed(GtkTreeSelection *selection, gpointer data)
{
	if(gtk_tree_selection_count_selected_rows(selection) > 0)
		gtk_widget_set_sensitive(del_btn,TRUE);
	else
		gtk_widget_set_sensitive(del_btn,FALSE);
}

static void email_selection_changed(GtkTreeSelection *selection, gpointer data)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	gchar *email;

	if(gtk_tree_selection_get_selected(selection, &model, &iter)) {
		GSList *hashval;
		GSList *walk;

		gtk_tree_model_get(model, &iter, 0, &email, -1);

		hashval = g_hash_table_lookup(addr_hash, email);
		gtk_list_store_clear(detail_store);
		for(walk = hashval; walk; walk = walk->next) {
			AddrDupListEntry *entry = walk->data;
			if(!entry)
				continue;
			gtk_list_store_append(detail_store, &iter);
			gtk_list_store_set(detail_store, &iter,
			                   COL_BOOK, addrindex_ds_get_name(entry->ds),
			                   COL_NAME, ADDRITEM_NAME(entry->person),
			                   COL_ITEM, entry->person,
			                   COL_DS, entry->ds,
			                   -1);
		}
		g_free(email);
	}
}

static void refresh_stores(void)
{
	refresh_addr_hash();
	if(email_store)
		gtk_list_store_clear(email_store);
	if(detail_store)
		gtk_list_store_clear(detail_store);
	g_hash_table_foreach(addr_hash,append_to_email_store,email_store);
}

static void detail_row_activated(GtkTreeView       *tree_view,
                                 GtkTreePath       *path,
                                 GtkTreeViewColumn *column,
                                 gpointer           user_data)
{
	GtkTreeIter iter;
	ItemPerson *person;
	AddressDataSource *ds;
	GtkTreeModel *model;
	AddressBookFile *abf;

	model = gtk_tree_view_get_model(tree_view);

	if(!gtk_tree_model_get_iter(model,&iter,path))
		return;

	gtk_tree_model_get(model, &iter, COL_ITEM, &person, COL_DS, &ds, -1);


	if(!((ds->type == ADDR_IF_BOOK) || ds->type == ADDR_IF_LDAP)) {
		debug_print("Unsupported address datasource type for editing\n");
		return;
	}

	abf = ds->rawDataSource;
	if(addressbook_edit_person(abf,NULL,person,FALSE,NULL,NULL,TRUE))
		refresh_stores();
}

void cb_del_btn_clicked(GtkButton *button, gpointer data)
{
	GtkTreeView *detail_view;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	ItemPerson *item;
	AddressDataSource *ds;
	GList *list;
	GList *ref_list;
	GList *walk;
	GtkTreeRowReference *ref;
	AlertValue aval;

	detail_view = data;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(detail_view));

	list = gtk_tree_selection_get_selected_rows(selection, &model);

	if(!list)
		return;

	aval = alertpanel(_("Delete address(es)"),
	                  _("Really delete the address(es)?"),
	                  GTK_STOCK_CANCEL, "+"GTK_STOCK_DELETE, NULL);
	if(aval != G_ALERTALTERNATE)
		return;

	ref_list = NULL;
	for(walk = list; walk; walk = walk->next) {
		ref = gtk_tree_row_reference_new(model,(GtkTreePath*)(walk->data));
		ref_list = g_list_prepend(ref_list, ref);
	}
	g_list_foreach(list, (GFunc)gtk_tree_path_free, NULL);
	g_list_free(list);

	for(walk = ref_list; walk; walk = walk->next) {
		GtkTreePath *path;
		ref = walk->data;
		if(!gtk_tree_row_reference_valid(ref))
			continue;
		path = gtk_tree_row_reference_get_path(ref);
		if(gtk_tree_model_get_iter(model, &iter, path)) {
			gtk_tree_model_get(model, &iter, COL_ITEM, &item, COL_DS, &ds, -1);
			delete_item(item,ds);
		}
		gtk_tree_path_free(path);
	}

	g_list_foreach(ref_list, (GFunc)gtk_tree_row_reference_free, NULL);
	g_list_free(ref_list);

	refresh_stores();
}

static gboolean delete_item(ItemPerson *item, AddressDataSource *ds)
{
	AddressBookFile *abf;
	AddressInterface *iface;

	/* Test for read only */
	iface = ds->interface;
	if( iface->readOnly ) {
		alertpanel( _("Delete address"),
		            _("This address data is readonly and cannot be deleted."),
		            GTK_STOCK_CLOSE, NULL, NULL );
		return FALSE;
	}

	if(!(abf = ds->rawDataSource))
		return FALSE;

	item->status = DELETE_ENTRY;
	item = addrbook_remove_person(abf, item);

#ifdef USE_LDAP

	if (ds && ds->type == ADDR_IF_LDAP) {
		LdapServer *server = ds->rawDataSource;
		ldapsvr_set_modified(server, TRUE);
		ldapsvr_update_book(server, item);
	}

#endif

	if(item)
		addritem_free_item_person(item);

	return TRUE;
}
