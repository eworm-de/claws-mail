/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2002 Hiroyuki Yamamoto and the Sylpheed-Claws Team
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <string.h>
#include <gtk/gtk.h>
#include <gtk/gtktext.h>
#include <gdk/gdkkeysyms.h>

#include "intl.h"
#include "utils.h"
#include "prefswindow.h"
#include "gtkutils.h"

typedef struct _PrefsWindow PrefsWindow;
typedef struct _PrefsTreeNode PrefsTreeNode;

struct _PrefsWindow
{
	GtkWidget *window;
	GtkWidget *table1;
	GtkWidget *scrolledwindow1;
	GtkWidget *ctree;
	GtkWidget *table2;
	GtkWidget *pagelabel;
	GtkWidget *labelframe;
	GtkWidget *frame;
	GtkWidget *notebook;
	GtkWidget *confirm_area;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;
	GtkWidget *apply_btn;

	GtkWidget *empty_page;

	gpointer 	  data;
	GSList	  	 *prefs_pages;
	GtkDestroyNotify  func;
};

struct _PrefsTreeNode
{
	PrefsPage *page;
	gfloat     treeweight;
};

static gboolean ctree_select_row(GtkCTree *ctree, GList *node, gint column, gpointer user_data)
{
	PrefsTreeNode *prefsnode;
	PrefsPage *page;
	PrefsWindow *prefswindow = (PrefsWindow *) user_data;
	gchar *labeltext;
	gint pagenum, i;

	prefsnode = gtk_ctree_node_get_row_data(GTK_CTREE(ctree), GTK_CTREE_NODE(node));
	page = prefsnode->page;

	debug_print("%f\n", prefsnode->treeweight);

	if (page == NULL) {
		gtk_label_set_text(GTK_LABEL(prefswindow->pagelabel), "");
		pagenum = gtk_notebook_page_num(GTK_NOTEBOOK(prefswindow->notebook),
						prefswindow->empty_page);
		gtk_notebook_set_page(GTK_NOTEBOOK(prefswindow->notebook), pagenum);
		return FALSE;
	}

	if (!page->page_open) {
		page->create_widget(page, GTK_WINDOW(prefswindow->window), prefswindow->data);
		gtk_container_add(GTK_CONTAINER(prefswindow->notebook), page->widget);
		page->page_open = TRUE;
	}

	i = 0;
	while (page->path[i + 1] != 0)
		i++;
	labeltext = page->path[i];

	gtk_label_set_text(GTK_LABEL(prefswindow->pagelabel), labeltext);

	pagenum = gtk_notebook_page_num(GTK_NOTEBOOK(prefswindow->notebook),
					page->widget);
	gtk_notebook_set_page(GTK_NOTEBOOK(prefswindow->notebook), pagenum);

	return FALSE;
}

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

static void apply_button_released(GtkButton *button, gpointer user_data)
{
	PrefsWindow *prefswindow = (PrefsWindow *) user_data;

	save_all_pages(prefswindow->prefs_pages);
}

static void close_prefs_window(PrefsWindow *prefswindow)
{
	debug_print("prefs window closed\n");

	close_all_pages(prefswindow->prefs_pages);
	gtk_widget_destroy(prefswindow->window);
	g_slist_free(prefswindow->prefs_pages);
	if (prefswindow->func != NULL)
		prefswindow->func(prefswindow->data);
	g_free(prefswindow);
}

static void ok_button_released(GtkButton *button, gpointer user_data)
{
	PrefsWindow *prefswindow = (PrefsWindow *) user_data;

	if (query_can_close_all_pages(prefswindow->prefs_pages)) {
		save_all_pages(prefswindow->prefs_pages);
		close_prefs_window(prefswindow);
	}		
}

static void cancel_button_released(GtkButton *button, gpointer user_data)
{
	PrefsWindow *prefswindow = (PrefsWindow *) user_data;

	close_prefs_window(prefswindow);
}

static gboolean window_closed(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	PrefsWindow *prefswindow = (PrefsWindow *) user_data;

	close_prefs_window(prefswindow);
	return FALSE;
}

struct name_search
{
	gchar *text;
	GtkCTreeNode *node;
};

static gboolean find_child_by_name(GtkCTree *ctree, GtkCTreeNode *node, struct name_search *name_search)
{
	gchar *text = NULL;

	text = GTK_CELL_TEXT(GTK_CTREE_ROW(node)->row.cell[0])->text;
	if (text == NULL)
		return FALSE;

	if (strcmp(text, name_search->text) == 0)
		name_search->node = node;

	return FALSE;
}

gint compare_func(GtkCList *clist, gconstpointer ptr1, gconstpointer ptr2)
{
	PrefsTreeNode *prefsnode1 = ((GtkCListRow *)ptr1)->data;
	PrefsTreeNode *prefsnode2 = ((GtkCListRow *)ptr2)->data;

	if (prefsnode1 == NULL || prefsnode2 == NULL)
		return 0;

	return prefsnode1->treeweight > prefsnode2->treeweight ? -1 : 
	       prefsnode1->treeweight < prefsnode2->treeweight ?  1 : 
							          0;
}

static gboolean prefswindow_key_pressed(GtkWidget *widget, GdkEventKey *event,
				    	PrefsWindow *data)
{
	GtkWidget *focused_child;
	
	if (event) {
		switch (event->keyval) {
			case GDK_Escape :
				cancel_button_released(NULL, data);
				break;
			case GDK_Return : 
			case GDK_KP_Enter :
				focused_child = gtkut_get_focused_child
					(GTK_CONTAINER(data->notebook));
				/* Press ok, if the focused child is not a text view
				 * and text (anything that accepts return) (can pass
				 * NULL to any of the GTK_xxx() casts) */
				if (!GTK_IS_TEXT(focused_child))
					ok_button_released(NULL, data);
				break;
			default:
				break;
		}
	}
	return FALSE;
}

void prefswindow_open_full(const gchar *title, GSList *prefs_pages, gpointer data, GtkDestroyNotify func)
{
	static gchar *titles[1];
	GSList *cur;
	gint optsize;
	PrefsWindow *prefswindow;

	titles[0] = _("Page Index");

	prefswindow = g_new0(PrefsWindow, 1);

	prefswindow->data = data;
	prefswindow->func = func;
	prefswindow->prefs_pages = g_slist_copy(prefs_pages);

	prefswindow->window = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_window_set_title(GTK_WINDOW(prefswindow->window), title);
	gtk_window_set_default_size(GTK_WINDOW(prefswindow->window), 600, 340);
	gtk_window_position (GTK_WINDOW(prefswindow->window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal (GTK_WINDOW (prefswindow->window), TRUE);
	gtk_window_set_policy (GTK_WINDOW(prefswindow->window), FALSE, TRUE, FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(prefswindow->window), 4);

	prefswindow->table1 = gtk_table_new(2, 2, FALSE);
	gtk_widget_show(prefswindow->table1);
	gtk_container_add(GTK_CONTAINER(prefswindow->window), prefswindow->table1);

	prefswindow->scrolledwindow1 = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(prefswindow->scrolledwindow1);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(prefswindow->scrolledwindow1), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_table_attach(GTK_TABLE(prefswindow->table1), prefswindow->scrolledwindow1, 0, 1, 0, 1, GTK_FILL, GTK_FILL | GTK_EXPAND, 2, 2);

	prefswindow->ctree = gtk_ctree_new_with_titles(1, 0, titles);
	gtk_widget_show(prefswindow->ctree);
	gtk_container_add(GTK_CONTAINER(prefswindow->scrolledwindow1), prefswindow->ctree);

	prefswindow->frame = gtk_frame_new(NULL);
	gtk_widget_show(prefswindow->frame);
	gtk_frame_set_shadow_type(GTK_FRAME(prefswindow->frame), GTK_SHADOW_IN);
	gtk_table_attach(GTK_TABLE(prefswindow->table1), prefswindow->frame, 1, 2, 0, 1, GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);

	prefswindow->table2 = gtk_table_new(1, 2, FALSE);
	gtk_widget_show(prefswindow->table2);
	gtk_container_add(GTK_CONTAINER(prefswindow->frame), prefswindow->table2);

	prefswindow->labelframe = gtk_frame_new(NULL);
	gtk_widget_show(prefswindow->labelframe);
	gtk_frame_set_shadow_type(GTK_FRAME(prefswindow->labelframe), GTK_SHADOW_OUT);
	gtk_table_attach(GTK_TABLE(prefswindow->table2), prefswindow->labelframe, 0, 1, 0, 1, GTK_FILL | GTK_EXPAND, GTK_FILL, 1, 1);

	prefswindow->pagelabel = gtk_label_new("");
	gtk_widget_show(prefswindow->pagelabel);
	gtk_label_set_justify(GTK_LABEL(prefswindow->pagelabel), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(prefswindow->pagelabel), 0, 0.0);
	gtk_container_add(GTK_CONTAINER(prefswindow->labelframe), prefswindow->pagelabel);

	prefswindow->notebook = gtk_notebook_new();
	gtk_widget_show(prefswindow->notebook);
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(prefswindow->notebook), TRUE);
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(prefswindow->notebook), FALSE);
	gtk_notebook_set_show_border(GTK_NOTEBOOK(prefswindow->notebook), FALSE);
	gtk_table_attach(GTK_TABLE(prefswindow->table2), prefswindow->notebook, 0, 1, 1, 2, GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 8, 8);

	prefswindow->empty_page = gtk_label_new("");
	gtk_widget_show(prefswindow->empty_page);
	gtk_container_add(GTK_CONTAINER(prefswindow->notebook), prefswindow->empty_page);

	/* actually we should create a tree here */
	for (cur = prefs_pages; cur != NULL; cur = g_slist_next(cur)) {
		PrefsPage *page = (PrefsPage *)cur->data;
		GtkCTreeNode *node = NULL;
		gchar *text[2], *part;
		int i;
		struct name_search name_search;
		PrefsTreeNode *prefsnode;

		for (i = 0; page->path[i] != NULL; i++) {
			part = page->path[i];
			name_search.text = part;
			name_search.node = NULL;

			gtk_ctree_post_recursive_to_depth(GTK_CTREE(prefswindow->ctree), node, node != NULL ? GTK_CTREE_ROW(node)->level + 1 : 1, GTK_CTREE_FUNC(find_child_by_name), &name_search);

			if (name_search.node) {
				node = name_search.node;
			} else {
				text[0] = part;
				node = gtk_ctree_insert_node(GTK_CTREE(prefswindow->ctree), node, NULL, text, 0, NULL, NULL, NULL, NULL, FALSE, TRUE);

				prefsnode = g_new0(PrefsTreeNode, 1);
				prefsnode->treeweight = 0.0;
				gtk_ctree_node_set_row_data_full(GTK_CTREE(prefswindow->ctree), node, prefsnode, g_free);
			}
		}

		prefsnode = (PrefsTreeNode *) GTK_CTREE_ROW(node)->row.data;
		prefsnode->page = page;

		for (; node != NULL; node = GTK_CTREE_ROW(node)->parent) {
			PrefsTreeNode *curnode = (PrefsTreeNode *) GTK_CTREE_ROW(node)->row.data;

			if (page->weight > curnode->treeweight)
				curnode->treeweight = page->weight;
		}
	}
	gtk_signal_connect(GTK_OBJECT(prefswindow->ctree), "tree-select-row", GTK_SIGNAL_FUNC(ctree_select_row), prefswindow);

	gtk_clist_set_selection_mode(GTK_CLIST(prefswindow->ctree), GTK_SELECTION_BROWSE);
	gtk_clist_column_titles_passive(GTK_CLIST(prefswindow->ctree));
	optsize = gtk_clist_optimal_column_width(GTK_CLIST(prefswindow->ctree), 0);
	gtk_clist_set_column_resizeable(GTK_CLIST(prefswindow->ctree), 0, TRUE);
	gtk_clist_set_column_auto_resize(GTK_CLIST(prefswindow->ctree), 0, FALSE);
	gtk_clist_set_column_width(GTK_CLIST(prefswindow->ctree), 0, optsize);
	gtk_clist_set_column_min_width(GTK_CLIST(prefswindow->ctree), 0, optsize);
	gtk_clist_set_column_max_width(GTK_CLIST(prefswindow->ctree), 0, optsize);
	gtk_clist_set_compare_func(GTK_CLIST(prefswindow->ctree), compare_func);
	gtk_ctree_sort_recursive(GTK_CTREE(prefswindow->ctree), NULL);
	gtk_widget_grab_focus(GTK_WIDGET(prefswindow->ctree));

	gtkut_button_set_create(&prefswindow->confirm_area,
				&prefswindow->ok_btn,		_("OK"),
				&prefswindow->cancel_btn,	_("Cancel"),
				&prefswindow->apply_btn,	_("Apply"));
	gtk_widget_show_all(prefswindow->confirm_area);

	gtk_table_attach(GTK_TABLE(prefswindow->table1), prefswindow->confirm_area, 0, 2, 1, 2, GTK_FILL | GTK_EXPAND, GTK_FILL, 2, 2);

	gtk_signal_connect(GTK_OBJECT(prefswindow->ok_btn), "released", GTK_SIGNAL_FUNC(ok_button_released), prefswindow);
	gtk_signal_connect(GTK_OBJECT(prefswindow->cancel_btn), "released", GTK_SIGNAL_FUNC(cancel_button_released), prefswindow);
	gtk_signal_connect(GTK_OBJECT(prefswindow->apply_btn), "released", GTK_SIGNAL_FUNC(apply_button_released), prefswindow);
	gtk_signal_connect(GTK_OBJECT(prefswindow->window), "delete_event", GTK_SIGNAL_FUNC(window_closed), prefswindow);
	gtk_signal_connect(GTK_OBJECT(prefswindow->window), "key_press_event",
			   GTK_SIGNAL_FUNC(prefswindow_key_pressed), &(prefswindow->window));

	gtk_widget_show(prefswindow->window);
}

void prefswindow_open(const gchar *title, GSList *prefs_pages, gpointer data)
{
	prefswindow_open_full(title, prefs_pages, data, NULL);
}
