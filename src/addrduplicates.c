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

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <string.h>

#include "defs.h"

#include "addrduplicates.h"
#include "addrindex.h"
#include "alertpanel.h"
#include "gtkutils.h"

typedef struct
{
	ItemPerson *person;
	gchar *book;
}
AddrDupListEntry;

enum {
    COL_BOOK = 0,
    COL_NAME,
    NUM_COLS
};

static gboolean create_dialog(void);
static void create_addr_hash(void);
static void present_finder_results(void);
static void cb_finder_results_dialog_destroy(GtkWindow*, gpointer);
static void destroy_addr_hash_val(gpointer);
static GSList* deep_copy_hash_val(GSList*);
static void fill_hash_table();
static gint collect_emails(ItemPerson*, const gchar*);
static gboolean is_not_duplicate(gpointer, gpointer, gpointer);
static gint books_compare(gconstpointer, gconstpointer);
static GtkWidget* create_email_view(GtkListStore*);
static GtkWidget* create_detail_view(GtkListStore*);
static void append_to_email_store(gpointer,gpointer,gpointer);
static void email_selection_changed(GtkTreeSelection*,gpointer);

static GHashTable *addr_hash;
static gboolean include_same_book = TRUE;
static gboolean include_other_books = TRUE;
static GtkListStore *detail_store;

void addrduplicates_find(void)
{
	if(create_dialog()) {
		create_addr_hash();
		present_finder_results();
	}
}

static gboolean create_dialog(void)
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

static void create_addr_hash(void)
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
		if(entry->book)
			g_free(entry->book);
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
		out_entry->book   = g_strdup(in_entry->book);
		walk->data = out_entry;
	}

	return out;
}

static void fill_hash_table()
{
	addrindex_load_person_attribute(NULL, collect_emails);
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

static gint collect_emails(ItemPerson *itemperson, const gchar *book)
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
		entry->book = g_strdup(book);

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
	return strcmp(entry1->book,entry2->book);
}

static void present_finder_results(void)
{
	GtkListStore *email_store;
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
	g_hash_table_foreach(addr_hash,append_to_email_store,email_store);
	email_view = create_email_view(email_store);
	email_select = gtk_tree_view_get_selection(GTK_TREE_VIEW(email_view));
	gtk_tree_selection_set_mode(email_select,GTK_SELECTION_SINGLE);

	g_signal_connect(email_select, "changed",
	                 (GCallback)email_selection_changed, NULL);

	detail_store = gtk_list_store_new(NUM_COLS, G_TYPE_STRING, G_TYPE_STRING);
	detail_view = create_detail_view(detail_store);
	detail_select = gtk_tree_view_get_selection(GTK_TREE_VIEW(detail_view));
	gtk_tree_selection_set_mode(email_select,GTK_SELECTION_SINGLE);

	dialog = gtkut_window_new(GTK_WINDOW_TOPLEVEL, "address_dupes_finder");
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
	gtk_box_pack_end(GTK_BOX(hbox), close, FALSE, FALSE, 0);

	g_signal_connect(dialog, "destroy",
	                 G_CALLBACK(cb_finder_results_dialog_destroy), NULL);
	g_signal_connect_swapped(close, "clicked",
	                         G_CALLBACK(gtk_widget_destroy), dialog);

	gtk_widget_show_all(dialog);
}

static void cb_finder_results_dialog_destroy(GtkWindow *win, gpointer data)
{
	g_hash_table_destroy(addr_hash);
	addr_hash = NULL;

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

	return view;
}

static void append_to_email_store(gpointer key,gpointer value,gpointer data)
{
	GtkTreeIter iter;
	GtkListStore *store = (GtkListStore*) data;

	gtk_list_store_append(store, &iter);
	gtk_list_store_set(store, &iter, 0, (gchar*) key, -1);
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
			                   COL_BOOK, entry->book,
			                   COL_NAME, ADDRITEM_NAME(entry->person),
			                   -1);
		}
		g_free(email);
	}
}
