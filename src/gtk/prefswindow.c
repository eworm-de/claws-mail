/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2022 the Claws Mail Team and Hiroyuki Yamamoto
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

#include <glib.h>
#include <glib/gi18n.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "utils.h"
#include "prefswindow.h"
#include "gtkutils.h"
#include "prefs_common.h"
#include "gtk/manage_window.h"

static void prefs_size_allocate_cb(GtkWidget *widget,
							 GtkAllocation *allocation, gpointer *user_data);
static GtkTreeStore *prefswindow_create_data_store	(void);
static GtkWidget *prefswindow_tree_view_create		(PrefsWindow* prefswindow);
static void prefs_filtering_create_tree_view_columns	(GtkWidget *tree_view);
static gboolean prefswindow_row_selected		(GtkTreeSelection *selector,
							 GtkTreeModel *model, 
							 GtkTreePath *path,
							 gboolean currently_selected,
							 gpointer data);

static void save_all_pages(GSList *prefs_pages)
{
	GSList *cur;

	for (cur = prefs_pages; cur != NULL; cur = g_slist_next(cur)) {
		PrefsPage *page = (PrefsPage *) cur->data;

		if (page->page_open) {
			page->save_page(page);
		}
	}
}

static gboolean query_can_close_all_pages(GSList *prefs_pages)
{
	GSList *cur;

	for (cur = prefs_pages; cur != NULL; cur = g_slist_next(cur)) {
		PrefsPage *page = (PrefsPage *) cur->data;

		if (page->can_close)
			if (!page->can_close(page))
				return FALSE;
	}
	return TRUE;
}

static void close_all_pages(GSList *prefs_pages)
{
	GSList *cur;

	for (cur = prefs_pages; cur != NULL; cur = g_slist_next(cur)) {
		PrefsPage *page = (PrefsPage *) cur->data;

		if (page->page_open) {
			page->destroy_widget(page);
			page->page_open = FALSE;
		}
	}	
}

#ifdef GENERIC_UMPC
static void prefs_show_sections(PrefsWindow *prefswindow)
{
	gint max;
	GtkWidget *paned = prefswindow->paned;
	
        g_object_get (G_OBJECT(paned),
                        "max-position",
                        &max, NULL);

	gtk_widget_show(gtk_paned_get_child1(GTK_PANED(paned)));
	gtk_widget_hide(gtk_paned_get_child2(GTK_PANED(paned)));
	gtk_paned_set_position(GTK_PANED(paned), max);
}

static void prefs_show_page(PrefsWindow *prefswindow)
{
	gint min;
	GtkWidget *paned = prefswindow->paned;
	
        g_object_get (G_OBJECT(paned),
                        "min-position",
                        &min, NULL);

	gtk_widget_hide(gtk_paned_get_child1(GTK_PANED(paned)));
	gtk_widget_show(gtk_paned_get_child2(GTK_PANED(paned)));
	gtk_paned_set_position(GTK_PANED(paned), min);
}
#endif

static void apply_button_clicked(GtkButton *button, gpointer user_data)
{
	PrefsWindow *prefswindow = (PrefsWindow *) user_data;

	prefswindow->dialog_response = PREFSWINDOW_RESPONSE_APPLY;

	save_all_pages(prefswindow->prefs_pages);
#ifdef GENERIC_UMPC
	prefs_show_sections(prefswindow);
#endif

	if (prefswindow->apply_cb)
		prefswindow->apply_cb(prefswindow);
}

static void close_prefs_window(PrefsWindow *prefswindow)
{
	debug_print("prefs window closed\n");

	close_all_pages(prefswindow->prefs_pages);

	if (prefswindow->close_cb)
		prefswindow->close_cb(prefswindow);

	gtk_widget_destroy(prefswindow->window);
	g_slist_free(prefswindow->prefs_pages);
	if(prefswindow->func != NULL)
		prefswindow->func(prefswindow->data);
	g_free(prefswindow);
}

static void ok_button_clicked(GtkButton *button, gpointer user_data)
{
	PrefsWindow *prefswindow = (PrefsWindow *) user_data;

	prefswindow->dialog_response = PREFSWINDOW_RESPONSE_OK;

	if (query_can_close_all_pages(prefswindow->prefs_pages)) {
		save_all_pages(prefswindow->prefs_pages);
		close_prefs_window(prefswindow);
	}		
}

static void cancel_button_clicked(GtkButton *button, gpointer user_data)
{
	PrefsWindow *prefswindow = (PrefsWindow *) user_data;

	close_prefs_window(prefswindow);
}

static gboolean window_closed(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	PrefsWindow *prefswindow = (PrefsWindow *) user_data;

#ifdef GENERIC_UMPC
	save_all_pages(prefswindow->prefs_pages);
#endif
	close_prefs_window(prefswindow);

	return FALSE;
}

static gboolean prefswindow_key_pressed(GtkWidget *widget, GdkEventKey *event,
				    PrefsWindow *data)
{
	GtkWidget *focused_child;

	if (event) {
		switch (event->keyval) {
			case GDK_KEY_Escape :
				cancel_button_clicked(NULL, data);
				break;
			case GDK_KEY_Return : 
			case GDK_KEY_KP_Enter :
				focused_child = gtkut_get_focused_child
					(GTK_CONTAINER(data->notebook));
				/* Press ok, if the focused child is not a text view
				 * and text (anything that accepts return) (can pass
				 * NULL to any of the GTK_xxx() casts) */
				if (!GTK_IS_TEXT_VIEW(focused_child))
					ok_button_clicked(NULL, data);
				break;
			default:
				break;
		}
	}
	return FALSE;
}

typedef struct FindNodeByName {
	const gchar *name;
	gboolean     found;
	GtkTreeIter  node;
} FindNodeByName;

static gboolean find_node_by_name(GtkTreeModel *model, GtkTreePath *path,
				  GtkTreeIter *iter, FindNodeByName *data)
{
	gchar *name;
	gboolean result = FALSE;

	gtk_tree_model_get(model, iter, PREFS_PAGE_TITLE, &name, -1);
	if (name) {
		result = strcmp(name, data->name) == 0;
		if (result) {
			data->found = TRUE;
			data->node  = *iter;
		}			
		g_free(name);
	}
	
	return result;
}

static gint prefswindow_tree_sort_by_weight(GtkTreeModel *model, 
					    GtkTreeIter  *a,
					    GtkTreeIter  *b,
					    gpointer      context)
{
	gfloat f1, f2;
	gint i1, i2;

	/* From observation sorting should keep in account the original
	 * order in the prefs_pages list. I.e. if equal weight, prefer 
	 * the index in the pages list */ 
	gtk_tree_model_get(model, a, 
			   PREFS_PAGE_INDEX,  &i1,
			   PREFS_PAGE_WEIGHT, &f1, -1);
	gtk_tree_model_get(model, b, 
			   PREFS_PAGE_INDEX,  &i2,
			   PREFS_PAGE_WEIGHT, &f2, -1);

	return f1 < f2 ? -1 : (f1 > f2 ?  1 : 
	      (i1 < i2 ?  1 : (i1 > i2 ? -1 : 0)));
}

static void prefswindow_build_page(PrefsWindow *prefswindow, PrefsPage *page)
{
	GtkWidget *scrolledwin;

	if (!page->page_open) {
		scrolledwin = gtk_scrolled_window_new(NULL, NULL);
		gtk_widget_show(scrolledwin);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
		
		page->create_widget(page, GTK_WINDOW(prefswindow->window), prefswindow->data);
		gtk_container_add(GTK_CONTAINER(scrolledwin), page->widget);

		gtk_container_add(GTK_CONTAINER(prefswindow->notebook), scrolledwin);

		page->widget	= scrolledwin;
		page->page_open	= TRUE;
	}
}

static GSList *prefswindow_build_all_pages(PrefsWindow *prefswindow, GSList *prefs_pages)
{
	GSList *cur;

	prefs_pages = g_slist_reverse(prefs_pages);
	for (cur = prefs_pages; cur != NULL; cur = g_slist_next(cur)) {
		PrefsPage *page = (PrefsPage *) cur->data;

		prefswindow_build_page(prefswindow, page);
	}
	return g_slist_reverse(prefs_pages);
}

static void prefswindow_build_tree(GtkWidget *tree_view, GSList *prefs_pages,
									PrefsWindow *prefswindow,
									gboolean preload_pages)
{
	GtkTreeStore *store = GTK_TREE_STORE(gtk_tree_view_get_model
			(GTK_TREE_VIEW(tree_view)));
	GSList *cur;
	gint index; /* index in pages list */
#ifndef GENERIC_UMPC
	GtkTreeSelection *selection;
	GtkTreeIter iter;
#endif

	for (cur = prefs_pages, index = 0; cur != NULL; cur = g_slist_next(cur), index++) {
		PrefsPage *page = (PrefsPage *)cur->data;
		FindNodeByName find_name;
		GtkTreeIter node, child;
		PrefsTreeNode *prefs_node = NULL;
		int i;

		/* each page tree component string */
		for (i = 0; page->path[i] != NULL; i++) {
			find_name.found = FALSE;
			find_name.name  = page->path[i];
			
			/* find node to attach to 
			 * FIXME: we search the entire tree, so this is suboptimal... */
			gtk_tree_model_foreach(GTK_TREE_MODEL(store), 
					       (GtkTreeModelForeachFunc) find_node_by_name,
					       &find_name);
			if (find_name.found && (i == 0 || page->path[i] != page->path[i-1])) {
				node = find_name.node;
				gtk_tree_model_get(GTK_TREE_MODEL(store), &node,
						   PREFS_PAGE_DATA, &prefs_node,
						   -1);
			} else {
				GAuto *autoptr; 
			
				/* create a new top level */
				gtk_tree_store_append(store, &child, i == 0 ? NULL : &node);
				prefs_node = g_new0(PrefsTreeNode, 1);
				autoptr = g_auto_pointer_new(prefs_node);
				gtk_tree_store_set(store, &child,
						   PREFS_PAGE_TITLE, page->path[i],
						   PREFS_PAGE_DATA,  prefs_node,
						   PREFS_PAGE_DATA_AUTO_FREE, autoptr,
						   PREFS_PAGE_INDEX, index,
						   PREFS_PAGE_WEIGHT, 0.0f,
						   -1);
				g_auto_pointer_free(autoptr);
				node = child;
			}
		}

		/* right now we have a node and its prefs_node */
		if (!prefs_node)
			g_warning("no prefs_node :/");
		else
			prefs_node->page = page;

		/* parents "inherit" the max weight of the children */
		do {
			gfloat f;
			
			gtk_tree_model_get(GTK_TREE_MODEL(store), &node, 
					   PREFS_PAGE_WEIGHT, &f,
					   -1);
			if (page->weight > f) {
				f = page->weight;
				gtk_tree_store_set(store, &node,
						   PREFS_PAGE_WEIGHT, f,
						   -1);
			}	
			child = node;	
		} while (gtk_tree_model_iter_parent(GTK_TREE_MODEL(store),	
						    &node, &child));
	}

	gtk_tree_view_expand_all(GTK_TREE_VIEW(tree_view));

	/* set sort func & sort */
	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(store),
					PREFS_PAGE_WEIGHT,
					prefswindow_tree_sort_by_weight,
					NULL, NULL);
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store),
					     PREFS_PAGE_WEIGHT,
					     GTK_SORT_DESCENDING);

	if (preload_pages)
		prefs_pages = prefswindow_build_all_pages(prefswindow, prefs_pages);

	/* select first one or its first child if necessary */
#ifndef GENERIC_UMPC
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view));
	if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter)) {
		if (gtk_tree_model_iter_has_child(GTK_TREE_MODEL(store), &iter)) {
			GtkTreeIter parent = iter;
			if (!gtk_tree_model_iter_children(GTK_TREE_MODEL(store), &iter, &parent))
				iter = parent;
		}
		gtk_tree_selection_select_iter(selection, &iter);
	}
#endif
}

void prefswindow_open_full(const gchar *title, GSList *prefs_pages,
							 gpointer data, GDestroyNotify func,
							 gint *save_width, gint *save_height,
							 gboolean preload_pages,
							 PrefsOpenCallbackFunc open_cb,
							 PrefsApplyCallbackFunc apply_cb,
							 PrefsCloseCallbackFunc close_cb)
{
	PrefsWindow *prefswindow;
	gint x;
	gint y;
	static GdkGeometry geometry;
	GtkAdjustment *adj;
	GdkRectangle workarea = {0};

	prefswindow = g_new0(PrefsWindow, 1);

	prefswindow->data = data;
	prefswindow->func = func;
	prefswindow->prefs_pages = g_slist_copy(prefs_pages);
	prefswindow->save_width = save_width;
	prefswindow->save_height = save_height;
	prefswindow->open_cb = open_cb;
	prefswindow->apply_cb = apply_cb;
	prefswindow->close_cb = close_cb;
	prefswindow->dialog_response = PREFSWINDOW_RESPONSE_CANCEL;

	prefswindow->window = gtkut_window_new(GTK_WINDOW_TOPLEVEL, "prefswindow");
	gtk_window_set_title(GTK_WINDOW(prefswindow->window), title);

	gtk_window_set_position (GTK_WINDOW(prefswindow->window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal (GTK_WINDOW (prefswindow->window), TRUE);
	gtk_window_set_resizable (GTK_WINDOW(prefswindow->window), TRUE);
	gtk_window_set_type_hint(GTK_WINDOW(prefswindow->window), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_window_set_transient_for (GTK_WINDOW(prefswindow->window),
			GTK_WINDOW(mainwindow_get_mainwindow()->window));
	gtk_container_set_border_width(GTK_CONTAINER(prefswindow->window), 4);

	prefswindow->vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_show(prefswindow->vbox);
	
	prefswindow->paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_widget_show(prefswindow->paned);

	gtk_container_add(GTK_CONTAINER(prefswindow->window), prefswindow->vbox);

	gtk_box_pack_start(GTK_BOX(prefswindow->vbox), prefswindow->paned, TRUE, TRUE, 0);

	prefswindow->scrolledwindow1 = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(prefswindow->scrolledwindow1);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(prefswindow->scrolledwindow1),
			GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(prefswindow->scrolledwindow1),
			GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
			
	gtk_paned_add1(GTK_PANED(prefswindow->paned), prefswindow->scrolledwindow1);

	prefswindow->tree_view = prefswindow_tree_view_create(prefswindow);
	gtk_widget_show(prefswindow->tree_view);
	gtk_container_add(GTK_CONTAINER(prefswindow->scrolledwindow1), 
			  prefswindow->tree_view);

	prefswindow->vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	gtk_widget_show(prefswindow->vbox2);

	gtk_paned_add2(GTK_PANED(prefswindow->paned), prefswindow->vbox2);

	prefswindow->table2 = gtk_grid_new();
	gtk_widget_show(prefswindow->table2);
	gtk_box_pack_start(GTK_BOX(prefswindow->vbox2), prefswindow->table2, TRUE, TRUE, 0);

	prefswindow->labelframe = gtk_frame_new(NULL);
	gtk_widget_show(prefswindow->labelframe);
	gtk_frame_set_shadow_type(GTK_FRAME(prefswindow->labelframe), GTK_SHADOW_NONE);
	gtk_grid_attach(GTK_GRID(prefswindow->table2), prefswindow->labelframe, 0, 0, 1, 1);
	gtk_widget_set_hexpand(prefswindow->labelframe, TRUE);
	gtk_widget_set_halign(prefswindow->labelframe, GTK_ALIGN_FILL);

	prefswindow->pagelabel = gtk_label_new("");
	gtk_widget_set_margin_top(prefswindow->pagelabel, 4);
	gtk_widget_set_margin_bottom(prefswindow->pagelabel, 4);
	gtk_widget_set_margin_start(prefswindow->pagelabel, 4);
	gtk_widget_show(prefswindow->pagelabel);
	gtk_label_set_justify(GTK_LABEL(prefswindow->pagelabel), GTK_JUSTIFY_LEFT);
	gtk_label_set_xalign(GTK_LABEL(prefswindow->pagelabel), 0.0);
	gtk_label_set_yalign(GTK_LABEL(prefswindow->pagelabel), 0.0);
	gtk_container_add(GTK_CONTAINER(prefswindow->labelframe), prefswindow->pagelabel);

	prefswindow->notebook = gtk_notebook_new();
	gtk_widget_show(prefswindow->notebook);
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(prefswindow->notebook), TRUE);
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(prefswindow->notebook), FALSE);
	gtk_notebook_set_show_border(GTK_NOTEBOOK(prefswindow->notebook), FALSE);

	gtk_grid_attach(GTK_GRID(prefswindow->table2), prefswindow->notebook, 0, 1, 1, 1);
	gtk_widget_set_vexpand(prefswindow->notebook, TRUE);
	gtk_widget_set_hexpand(prefswindow->notebook, TRUE);
	gtk_widget_set_halign(prefswindow->notebook, GTK_ALIGN_FILL);

	prefswindow->empty_page = gtk_label_new("");
	gtk_widget_show(prefswindow->empty_page);
	gtk_container_add(GTK_CONTAINER(prefswindow->notebook), prefswindow->empty_page);

	prefswindow_build_tree(prefswindow->tree_view, prefs_pages, prefswindow,
							preload_pages);

	if (open_cb)
		open_cb(prefswindow);

	gtk_widget_grab_focus(prefswindow->tree_view);

#ifndef GENERIC_UMPC
	gtkut_stock_button_set_create(&prefswindow->confirm_area,
				      &prefswindow->apply_btn, NULL, _("_Apply"),
				      &prefswindow->cancel_btn, NULL, _("_Cancel"),
				      &prefswindow->ok_btn, NULL, _("_OK"));
#else
	gtkut_stock_button_set_create(&prefswindow->confirm_area,
				      &prefswindow->apply_btn, NULL, _("_Apply"),
				      &prefswindow->ok_btn, "window-close", _("_Close"),
				      NULL, NULL, NULL);
#endif
	gtk_widget_show_all(prefswindow->confirm_area);
	gtk_widget_show(prefswindow->vbox);
	gtk_widget_show(prefswindow->scrolledwindow1);

	gtk_box_pack_start(GTK_BOX(prefswindow->vbox), prefswindow->confirm_area, FALSE, FALSE, 0);

#ifndef GENERIC_UMPC
	g_signal_connect(G_OBJECT(prefswindow->ok_btn), "clicked", 
			 G_CALLBACK(ok_button_clicked), prefswindow);
	g_signal_connect(G_OBJECT(prefswindow->cancel_btn), "clicked", 
			 G_CALLBACK(cancel_button_clicked), prefswindow);
	g_signal_connect(G_OBJECT(prefswindow->apply_btn), "clicked", 
			 G_CALLBACK(apply_button_clicked), prefswindow);
#else
	g_signal_connect(G_OBJECT(prefswindow->ok_btn), "clicked", 
			 G_CALLBACK(ok_button_clicked), prefswindow);
	g_signal_connect(G_OBJECT(prefswindow->apply_btn), "clicked", 
			 G_CALLBACK(apply_button_clicked), prefswindow);
#endif

	g_signal_connect(G_OBJECT(prefswindow->window), "delete_event", 
			 G_CALLBACK(window_closed), prefswindow);
	g_signal_connect(G_OBJECT(prefswindow->window), "key_press_event",
			   G_CALLBACK(prefswindow_key_pressed), &(prefswindow->window));

	/* connect to callback only if we have non-NULL pointers to store size to */
	if (prefswindow->save_width && prefswindow->save_height) {
		g_signal_connect(G_OBJECT(prefswindow->window), "size_allocate",
				 G_CALLBACK(prefs_size_allocate_cb), prefswindow);
	}

	MANAGE_WINDOW_SIGNALS_CONNECT(prefswindow->window);
	
	gdk_monitor_get_workarea(gdk_display_get_primary_monitor(gdk_display_get_default()),
				 &workarea);
	x =  workarea.width;
	y =  workarea.height;
	
	if (!geometry.min_height) {
		
		if (x < 800 && y < 600) {
			geometry.min_width = 600;
			geometry.min_height = 440;
		} else {
			geometry.min_width = 700;
			geometry.min_height = 550;
		}
	}
	gtk_window_set_geometry_hints(GTK_WINDOW(prefswindow->window),
			NULL, &geometry, GDK_HINT_MIN_SIZE);
	if (prefswindow->save_width && prefswindow->save_height) {
		gtk_window_set_default_size(GTK_WINDOW(prefswindow->window),
				*(prefswindow->save_width),
					    *(prefswindow->save_height));
	}

#ifdef GENERIC_UMPC
	prefs_show_sections(prefswindow);
#endif
	gtk_widget_show(prefswindow->window);
	adj = gtk_scrolled_window_get_vadjustment(
			GTK_SCROLLED_WINDOW(prefswindow->scrolledwindow1));
	gtk_adjustment_set_value(adj, gtk_adjustment_get_lower(adj));
}

void prefswindow_open(const gchar *title, GSList *prefs_pages, gpointer data,
					 gint *save_width, gint *save_height,
					 PrefsOpenCallbackFunc open_cb,
					 PrefsApplyCallbackFunc apply_cb,
					 PrefsCloseCallbackFunc close_cb)
{
	prefswindow_open_full(title, prefs_pages, data, NULL, save_width, save_height,
						  FALSE, open_cb, apply_cb, close_cb);
}

/*!
 *\brief	Save Gtk object size to prefs dataset
 */
static void prefs_size_allocate_cb(GtkWidget *widget,
					 GtkAllocation *allocation, gpointer *user_data)
{
	PrefsWindow *prefswindow = (PrefsWindow *) user_data;

	cm_return_if_fail(allocation != NULL);

	/* don't try to save size to NULL pointers */
	if (prefswindow && prefswindow->save_width && prefswindow->save_height) {
		gtk_window_get_size(GTK_WINDOW(widget),
			prefswindow->save_width, prefswindow->save_height);
	}
}

static GtkTreeStore *prefswindow_create_data_store(void)
{
	return gtk_tree_store_new(N_PREFS_PAGE_COLUMNS,
				  G_TYPE_STRING,
				  G_TYPE_POINTER,
				  G_TYPE_AUTO_POINTER,
				  G_TYPE_FLOAT,
				  G_TYPE_INT,
				  -1);
}

static GtkWidget *prefswindow_tree_view_create(PrefsWindow *prefswindow)
{
	GtkTreeView *tree_view;
	GtkTreeSelection *selector;
	GtkTreeModel *model;

	model = GTK_TREE_MODEL(prefswindow_create_data_store());
	tree_view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(model));
	g_object_unref(model);
	gtk_tree_view_set_rules_hint(tree_view, prefs_common.use_stripes_everywhere);
	
	selector = gtk_tree_view_get_selection(tree_view);
	gtk_tree_selection_set_mode(selector, GTK_SELECTION_BROWSE);
	gtk_tree_selection_set_select_function(selector, prefswindow_row_selected,
					       prefswindow, NULL);

	/* create the columns */
	prefs_filtering_create_tree_view_columns(GTK_WIDGET(tree_view));

	return GTK_WIDGET(tree_view);
}

static void prefs_filtering_create_tree_view_columns(GtkWidget *tree_view)
{
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes
		(_("Page Index"),
		 renderer,
		 "text", PREFS_PAGE_TITLE,
		 NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);		
}

static gboolean prefswindow_row_selected(GtkTreeSelection *selector,
					 GtkTreeModel *model, 
					 GtkTreePath *path,
					 gboolean currently_selected,
					 gpointer data)
{
	PrefsTreeNode *prefsnode;
	PrefsPage *page;
	PrefsWindow *prefswindow = (PrefsWindow *) data;
	gfloat lower;
	gchar *labeltext;
	gint pagenum, i;
	GtkTreeIter iter;
	GtkAdjustment *adj;
	gchar *markup;

#ifndef GENERIC_UMPC
	if (currently_selected) 
		return TRUE;
#endif
	if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(model), &iter, path))
		return TRUE;

	gtk_tree_model_get(model, &iter, PREFS_PAGE_DATA, &prefsnode, -1);
	page = prefsnode->page;

	debug_print("%f\n", prefsnode->treeweight);

	if (page == NULL) {
		gtk_label_set_text(GTK_LABEL(prefswindow->pagelabel), "");
		pagenum = gtk_notebook_page_num(GTK_NOTEBOOK(prefswindow->notebook),
						prefswindow->empty_page);
		gtk_notebook_set_current_page(GTK_NOTEBOOK(prefswindow->notebook),
					      pagenum);
		return TRUE;
	}

	prefswindow_build_page(prefswindow, page);

	i = 0;
	while (page->path[i + 1] != 0)
		i++;
	labeltext = page->path[i];

	markup = g_markup_printf_escaped("<span weight=\"bold\">\%s</span>", labeltext);
	gtk_label_set_markup(GTK_LABEL(prefswindow->pagelabel), markup);
	g_free(markup);

	pagenum = gtk_notebook_page_num(GTK_NOTEBOOK(prefswindow->notebook),
					page->widget);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(prefswindow->notebook),
				      pagenum);

	adj = gtk_scrolled_window_get_vadjustment(
			GTK_SCROLLED_WINDOW(page->widget));
	lower = gtk_adjustment_get_lower(adj);
	gtk_adjustment_set_value(adj, lower);
	adj = gtk_scrolled_window_get_hadjustment(
			GTK_SCROLLED_WINDOW(page->widget));
	gtk_adjustment_set_value(adj, lower);

#ifdef GENERIC_UMPC
	prefs_show_page(prefswindow);
#endif
	return TRUE;
}

