/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2004 Hiroyuki Yamamoto
 * Copyright (C) 2004 Alfons Hoogervorst
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkhbbox.h>
#include <gtk/gtksignal.h>

#include <gtk/gtktreestore.h>
#include <gtk/gtktreeview.h>

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

#include "main.h"
#include "utils.h"
#include "gtkutils.h"
#include "stock_pixmap.h"
#include "foldersel.h"
#include "alertpanel.h"
#include "manage_window.h"
#include "folder.h"

enum {
	FOLDERSEL_FOLDERNAME,
	FOLDERSEL_FOLDERITEM,
	FOLDERSEL_FOLDERPIXBUF,
	FOLDERSEL_FOLDEREXPANDER,
	N_FOLDERSEL_COLUMNS
};

static GdkPixbuf *folder_pixbuf;
static GdkPixbuf *folderopen_pixbuf;

static GtkWidget *window;
static GtkTreeView *tree_view;
static GtkWidget *entry;
static GtkWidget *ok_button;
static GtkWidget *cancel_button;

static FolderItem *folder_item;
static FolderItem *selected_item;

static gboolean cancelled;
static gboolean finished;

static GtkTreeStore *tree_store;

static void foldersel_update_tree_store		(Folder * folder, FolderSelectionType type);
static void foldersel_create			(void);
static void foldersel_init_tree_view_images	(void);

static gboolean foldersel_selected(GtkTreeSelection *selector,
				   GtkTreeModel *model, 
				   GtkTreePath *path,
				   gboolean already_selected,
				   gpointer data);

static void foldersel_ok	(GtkButton	*button,
				 gpointer	 data);
static void foldersel_cancel	(GtkButton	*button,
				 gpointer	 data);
static void foldersel_activated	(void);
static gint delete_event	(GtkWidget	*widget,
				 GdkEventAny	*event,
				 gpointer	 data);
static gboolean key_pressed	(GtkWidget	*widget,
				 GdkEventKey	*event,
				 gpointer	 data);

static gint foldersel_folder_name_compare(GtkTreeModel *model, 
					  GtkTreeIter *a, 
					  GtkTreeIter *b, 
					  gpointer context)
{
	gchar *str_a = NULL, *str_b = NULL;
	gint res = 0;
	FolderItem *fld_a = NULL, *fld_b = NULL;
	GtkTreeIter parent;

	gtk_tree_model_get(model, a, 
			   FOLDERSEL_FOLDERITEM, &fld_a,
			   -1);
	gtk_tree_model_get(model, b, 
			   FOLDERSEL_FOLDERITEM, &fld_b,
			   -1);

	/* no sort for root folder */
	if (!gtk_tree_model_iter_parent(GTK_TREE_MODEL(model), &parent, a))
		return 0;

	/* if both a and b are special folders, sort them according to 
	 * their types (which is in-order). Note that this assumes that
	 * there are no multiple folders of a special type. */
	if (fld_a->stype != F_NORMAL  && fld_b->stype != F_NORMAL)
		return fld_a->stype < fld_b->stype ? -1 : 1;

	/* if b is normal folder, and a is not, b is smaller (ends up 
	 * lower in the list) */ 
	if (fld_a->stype != F_NORMAL && fld_b->stype == F_NORMAL)
		return -1;

	/* if b is special folder, and a is not, b is larger (ends up
	 * higher in the list) */
	if (fld_a->stype == F_NORMAL && fld_b->stype != F_NORMAL)	 
		return 1;
	
	/* XXX g_utf8_collate_key() comparisons may speed things
	 * up when having large lists of folders */
	gtk_tree_model_get(model, a, 
			   FOLDERSEL_FOLDERNAME, &str_a, 
			   -1);
	gtk_tree_model_get(model, b, 
			   FOLDERSEL_FOLDERNAME, &str_b, 
			   -1);

	/* otherwise just compare the folder names */		
	res = g_utf8_collate(str_a, str_b);

	g_free(str_a);
	g_free(str_b);

	return res;
}

typedef struct FolderItemSearch {
	FolderItem  *item;
	GtkTreePath *path;
	GtkTreeIter  iter;
} FolderItemSearch;

static gboolean tree_view_folder_item_func(GtkTreeModel		*model,
					   GtkTreePath		*path,
					   GtkTreeIter		*iter,
					   FolderItemSearch	*data)
{
	FolderItem *item = NULL;

	gtk_tree_model_get(model, iter, FOLDERSEL_FOLDERITEM, &item, -1);
	
	if (data->item == item) {
		data->path = path;
		data->iter = *iter;
		return TRUE;
	}
	
	return FALSE;
}

FolderItem *foldersel_folder_sel(Folder *cur_folder, FolderSelectionType type,
				 const gchar *default_folder)
{
	GtkCTreeNode *node;

	selected_item = NULL;

	if (!window)
		foldersel_create();
	else
		gtk_widget_show(window);

	/* update the tree */		
	foldersel_update_tree_store(cur_folder, type);
	gtk_tree_view_set_model(tree_view, GTK_TREE_MODEL(tree_store));

	gtk_tree_view_expand_all(tree_view); 
	
	manage_window_set_transient(GTK_WINDOW(window));
	
	gtk_widget_grab_focus(ok_button);
	gtk_widget_grab_focus(GTK_WIDGET(tree_view));

	/* select current */
	if (folder_item) {
		FolderItemSearch fis;

		fis.item = folder_item;
		fis.path = NULL;

		/* find matching model entry */
		gtk_tree_model_foreach
			(GTK_TREE_MODEL(tree_store), 
			 (GtkTreeModelForeachFunc)tree_view_folder_item_func,
 		         &fis);
		
		if (fis.path) {
			GtkTreeSelection *selector;
		
			selector = gtk_tree_view_get_selection(tree_view);
			gtk_tree_selection_select_iter(selector, &fis.iter);
			gtk_tree_view_set_cursor(tree_view, fis.path,
						 NULL, FALSE);
			
		}
	}

	cancelled = finished = FALSE;

	while (finished == FALSE)
		gtk_main_iteration();

	gtk_widget_hide(window);
	gtk_entry_set_text(GTK_ENTRY(entry), "");

	if (!cancelled &&
	    selected_item && selected_item->path) {
		folder_item = selected_item;
		return folder_item;
	} else
		return NULL;
}

static void foldersel_insert_gnode_in_store(GtkTreeStore *store, GNode *node, GtkTreeIter *parent)
{
	FolderItem *item;
	GtkTreeIter child;
	gchar *name;
	GNode *iter;

	g_return_if_fail(node);
	g_return_if_fail(node->data);
	g_return_if_fail(store);

	item = FOLDER_ITEM(node->data);

	/* if parent == NULL, top level */
	gtk_tree_store_append(store, &child, parent);

	/* insert this node */
	name = folder_item_get_name(item);
	gtk_tree_store_set(store, &child,
			   FOLDERSEL_FOLDERNAME, name,
			   FOLDERSEL_FOLDERITEM, item,
			   FOLDERSEL_FOLDERPIXBUF, folder_pixbuf,
			   FOLDERSEL_FOLDEREXPANDER, node->children ? TRUE : FALSE,
			   -1);
	g_free(name);
	
	/* insert its children (this node as parent) */
	for (iter = node->children; iter != NULL; iter = iter->next)
		foldersel_insert_gnode_in_store(store, iter, &child);
	
}

static void foldersel_update_tree_store(Folder *cur_folder, FolderSelectionType type)
{
	GList *list;

	gtk_tree_store_clear(tree_store);		

	/* insert folder data */
	for (list = folder_get_list(); list != NULL; list = list->next) {
		Folder *folder;

		folder = FOLDER(list->data);
		g_return_if_fail(folder);
		if (type != FOLDER_SEL_ALL) {
			if (FOLDER_TYPE(folder) == F_NEWS)
				continue;
		}

		foldersel_insert_gnode_in_store(tree_store, folder->node, NULL);
	}

	/* sort */
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(tree_store), 
					     FOLDERSEL_FOLDERNAME,
					     GTK_SORT_ASCENDING);
}

static void foldersel_create(void)
{
	GtkWidget *vbox;
	GtkWidget *scrolledwin;
	GtkWidget *confirm_area;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkTreeSelection *selector;

	/* create and initialize tree store */
	tree_store = gtk_tree_store_new(N_FOLDERSEL_COLUMNS,
					G_TYPE_STRING,
					G_TYPE_POINTER,
					GDK_TYPE_PIXBUF,
					G_TYPE_BOOLEAN,
					-1);

	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(tree_store),
					FOLDERSEL_FOLDERNAME,	
					foldersel_folder_name_compare, 
					NULL, NULL);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), _("Select folder"));
	gtk_container_set_border_width(GTK_CONTAINER(window), 4);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	gtk_window_set_resizable(GTK_WINDOW(window), TRUE);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(delete_event), NULL);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(key_pressed), NULL);
	MANAGE_WINDOW_SIGNALS_CONNECT(window);

	vbox = gtk_vbox_new(FALSE, 4);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_size_request(window, 300, 360);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(vbox), scrolledwin, TRUE, TRUE, 0);

	tree_view = GTK_TREE_VIEW(gtk_tree_view_new_with_model
		(GTK_TREE_MODEL(tree_store)));
	gtk_scrolled_window_set_vadjustment(GTK_SCROLLED_WINDOW(scrolledwin),
					    gtk_tree_view_get_vadjustment
						(tree_view));
	gtk_tree_view_set_headers_visible(tree_view, FALSE);
	gtk_tree_view_set_rules_hint(tree_view, TRUE);					       
	gtk_tree_view_set_search_column(tree_view, FOLDERSEL_FOLDERNAME); 

	selector = gtk_tree_view_get_selection(tree_view);
	gtk_tree_selection_set_mode(selector, GTK_SELECTION_BROWSE);
	gtk_tree_selection_set_select_function(selector, foldersel_selected,
					       NULL, NULL);

	gtk_widget_show(GTK_WIDGET(tree_view));

	/* now safe to initialize images for tree view */
	foldersel_init_tree_view_images();
	
	gtk_container_add(GTK_CONTAINER(scrolledwin), GTK_WIDGET(tree_view));
	
	column = gtk_tree_view_column_new();

	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(column, renderer, FALSE);

	/* tell renderer to act as an expander and use the
	 * appropriate image if it's not an expander (i.e.
	 * a folder without children) */
	g_object_set(renderer,
		     "pixbuf-expander-open", folderopen_pixbuf,
		     "pixbuf-expander-closed", folder_pixbuf,
		     NULL);
	gtk_tree_view_column_set_attributes(column, renderer,
                   "is-expander", FOLDERSEL_FOLDEREXPANDER,
		   "pixbuf",      FOLDERSEL_FOLDERPIXBUF,
		   NULL);
			     
	/* create text renderer */
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_set_attributes(column, renderer,
					    "text", FOLDERSEL_FOLDERNAME,
					    NULL);
	
	gtk_tree_view_append_column(tree_view, column);
					    
	entry = gtk_entry_new();
	gtk_entry_set_editable(GTK_ENTRY(entry), FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), entry, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(entry), "activate",
			 G_CALLBACK(foldersel_activated), NULL);

	gtkut_stock_button_set_create(&confirm_area,
				      &ok_button,     GTK_STOCK_OK,
				      &cancel_button, GTK_STOCK_CANCEL,
				      NULL,    	NULL);

	gtk_box_pack_end(GTK_BOX(vbox), confirm_area, FALSE, FALSE, 0);
	gtk_widget_grab_default(ok_button);

	g_signal_connect(G_OBJECT(ok_button), "clicked",
			 G_CALLBACK(foldersel_ok), NULL);
	g_signal_connect(G_OBJECT(cancel_button), "clicked",
			 G_CALLBACK(foldersel_cancel), NULL);

	gtk_widget_show_all(window);
}

static void foldersel_init_tree_view_images(void)
{
	stock_pixbuf_gdk(GTK_WIDGET(tree_view), STOCK_PIXMAP_DIR_CLOSE,
			 &folder_pixbuf);
	stock_pixbuf_gdk(GTK_WIDGET(tree_view), STOCK_PIXMAP_DIR_OPEN,
			 &folderopen_pixbuf);
}

static gboolean foldersel_selected(GtkTreeSelection *selector,
				   GtkTreeModel *model, 
				   GtkTreePath *path,
				   gboolean currently_selected,
				   gpointer data)
{
	GtkTreeIter iter;
	FolderItem *item = NULL;

	if (currently_selected)
		return TRUE;
	
	if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(model), &iter, path))
		return TRUE;

	gtk_tree_model_get(GTK_TREE_MODEL(tree_store), &iter, 
			   FOLDERSEL_FOLDERITEM, &item,
			   -1);
	
	selected_item = item;
	if (selected_item && selected_item->path) {
		gchar *id;
		id = folder_item_get_identifier(selected_item);
		gtk_entry_set_text(GTK_ENTRY(entry), id);
		g_free(id);
	} else
		gtk_entry_set_text(GTK_ENTRY(entry), "");
	
	return TRUE;
}

static void foldersel_ok(GtkButton *button, gpointer data)
{
	finished = TRUE;
}

static void foldersel_cancel(GtkButton *button, gpointer data)
{
	cancelled = TRUE;
	finished = TRUE;
}

static void foldersel_activated(void)
{
	gtk_button_clicked(GTK_BUTTON(ok_button));
}

static gint delete_event(GtkWidget *widget, GdkEventAny *event, gpointer data)
{
	foldersel_cancel(NULL, NULL);
	return TRUE;
}

static gboolean key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event) {
		if (event->keyval == GDK_Escape)
			foldersel_cancel(NULL, NULL);
		else if (event->keyval == GDK_Return)
			foldersel_ok(NULL, NULL);
	}

	return FALSE;
}

