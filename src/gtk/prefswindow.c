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

#include "intl.h"
#include "prefswindow.h"
#include "gtkutils.h"

typedef struct _PrefsWindow PrefsWindow;

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
	GtkWidget *page_widget;
	GtkWidget *confirm_area;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;
	GtkWidget *apply_btn;

	gpointer   data;
	GSList	  *prefs_pages;
};

static gboolean ctree_select_row(GtkCTree *ctree, GList *node, gint column, gpointer user_data)
{
	PrefsPage *page;
	PrefsWindow *prefswindow = (PrefsWindow *) user_data;
	gchar *labeltext;

	page = (PrefsPage *) gtk_ctree_node_get_row_data(GTK_CTREE(ctree), GTK_CTREE_NODE(node));

	if (prefswindow->page_widget != NULL)
		gtk_container_remove(GTK_CONTAINER(prefswindow->table2), prefswindow->page_widget);

	if (page == NULL) {
		GtkWidget *widget;
		
		widget = gtk_label_new("");

		gtk_label_set_text(GTK_LABEL(prefswindow->pagelabel), "");
		gtk_table_attach(GTK_TABLE(prefswindow->table2), widget, 0, 1, 1, 2, GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);
		prefswindow->page_widget = widget;
		return FALSE;
	}

	if (!page->page_open) {
		page->create_widget(page, GTK_WINDOW(prefswindow->window), prefswindow->data);
		gtk_widget_ref(page->widget);
		gtk_widget_show_all(page->widget);
		page->page_open = TRUE;
	}

	labeltext = (gchar *) strrchr(page->path, '/');
	if (labeltext == NULL)
		labeltext = page->path;
	else
		labeltext = labeltext + 1;
	gtk_label_set_text(GTK_LABEL(prefswindow->pagelabel), labeltext);

	prefswindow->page_widget = page->widget;
	gtk_table_attach(GTK_TABLE(prefswindow->table2), prefswindow->page_widget, 0, 1, 1, 2, GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 8, 8);

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

static void close_all_pages(GSList *prefs_pages)
{
	GSList *cur;

	for (cur = prefs_pages; cur != NULL; cur = g_slist_next(cur)) {
		PrefsPage *page = (PrefsPage *) cur->data;

		if (page->page_open) {
			gtk_widget_unref(page->widget);
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

static void ok_button_released(GtkButton *button, gpointer user_data)
{
	PrefsWindow *prefswindow = (PrefsWindow *) user_data;

	save_all_pages(prefswindow->prefs_pages);
	gtk_widget_destroy(prefswindow->window);
	close_all_pages(prefswindow->prefs_pages);
	g_slist_free(prefswindow->prefs_pages);
	g_free(prefswindow);
}

static void cancel_button_released(GtkButton *button, gpointer user_data)
{
	PrefsWindow *prefswindow = (PrefsWindow *) user_data;

	gtk_widget_destroy(prefswindow->window);
	close_all_pages(prefswindow->prefs_pages);
	g_slist_free(prefswindow->prefs_pages);
	g_free(prefswindow);
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

void prefswindow_open(GSList *prefs_pages, gpointer data)
{
	static gchar *titles [] = {"Page Index"};
	GSList *cur;
	gint optsize;
	PrefsWindow *prefswindow;

	prefswindow = g_new0(PrefsWindow, 1);

	prefswindow->data = data;
	prefswindow->prefs_pages = g_slist_copy(prefs_pages);

	prefswindow->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(prefswindow->window), _("Preferences"));
	gtk_window_set_default_size(GTK_WINDOW(prefswindow->window), 600, 340);
	gtk_window_position (GTK_WINDOW(prefswindow->window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal (GTK_WINDOW (prefswindow->window), TRUE);
	gtk_window_set_policy (GTK_WINDOW(prefswindow->window), FALSE, TRUE, FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(prefswindow->window), 4);

	prefswindow->table1 = gtk_table_new(2, 2, FALSE);
	gtk_container_add(GTK_CONTAINER(prefswindow->window), prefswindow->table1);

	prefswindow->scrolledwindow1 = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(prefswindow->scrolledwindow1), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_table_attach(GTK_TABLE(prefswindow->table1), prefswindow->scrolledwindow1, 0, 1, 0, 1, GTK_FILL, GTK_FILL | GTK_EXPAND, 2, 2);

	prefswindow->ctree = gtk_ctree_new_with_titles(1, 0, titles);
	gtk_container_add(GTK_CONTAINER(prefswindow->scrolledwindow1), prefswindow->ctree);

	prefswindow->frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(prefswindow->frame), GTK_SHADOW_IN);
	gtk_table_attach(GTK_TABLE(prefswindow->table1), prefswindow->frame, 1, 2, 0, 1, GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);

	prefswindow->table2 = gtk_table_new(1, 2, FALSE);
	gtk_container_add(GTK_CONTAINER(prefswindow->frame), prefswindow->table2);

	prefswindow->labelframe = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(prefswindow->labelframe), GTK_SHADOW_OUT);
	gtk_table_attach(GTK_TABLE(prefswindow->table2), prefswindow->labelframe, 0, 1, 0, 1, GTK_FILL | GTK_EXPAND, GTK_FILL, 1, 1);

	prefswindow->pagelabel = gtk_label_new("");
	gtk_label_set_justify(GTK_LABEL(prefswindow->pagelabel), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(prefswindow->pagelabel), 0, 0.0);
	gtk_container_add(GTK_CONTAINER(prefswindow->labelframe), prefswindow->pagelabel);

	prefswindow->page_widget = gtk_label_new("");
	gtk_table_attach(GTK_TABLE(prefswindow->table2), prefswindow->page_widget, 0, 1, 1, 2, GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 8, 8);

	/* actually we should create a tree here */
	for (cur = prefs_pages; cur != NULL; cur = g_slist_next(cur)) {
		PrefsPage *page = (PrefsPage *)cur->data;
		GtkCTreeNode *node = NULL;
		gchar *text[2], **split, *part;
		int i;
		struct name_search name_search;

		split = g_strsplit(page->path, "/", 0);
		for (i = 0; split[i] != NULL; i++) {
			part = split[i];			
			name_search.text = part;
			name_search.node = NULL;

			gtk_ctree_post_recursive_to_depth(GTK_CTREE(prefswindow->ctree), node, node != NULL ? GTK_CTREE_ROW(node)->level + 1 : 1, GTK_CTREE_FUNC(find_child_by_name), &name_search);

			if (name_search.node) {
				node = name_search.node;
			} else {
				text[0] = part;
				node = gtk_ctree_insert_node(GTK_CTREE(prefswindow->ctree), node, NULL, text, 0, NULL, NULL, NULL, NULL, FALSE, TRUE);
			}
		}
		g_strfreev(split);
		gtk_ctree_node_set_row_data(GTK_CTREE(prefswindow->ctree), node, page);
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

	gtkut_button_set_create(&prefswindow->confirm_area,
				&prefswindow->ok_btn,		_("OK"),
				&prefswindow->cancel_btn,	_("Cancel"),
				&prefswindow->apply_btn,	_("Apply"));

	gtk_table_attach(GTK_TABLE(prefswindow->table1), prefswindow->confirm_area, 0, 2, 1, 2, GTK_FILL | GTK_EXPAND, GTK_FILL, 2, 2);

	gtk_signal_connect(GTK_OBJECT(prefswindow->ok_btn), "released", GTK_SIGNAL_FUNC(ok_button_released), prefswindow);
	gtk_signal_connect(GTK_OBJECT(prefswindow->cancel_btn), "released", GTK_SIGNAL_FUNC(cancel_button_released), prefswindow);
	gtk_signal_connect(GTK_OBJECT(prefswindow->apply_btn), "released", GTK_SIGNAL_FUNC(apply_button_released), prefswindow);

	gtk_widget_show_all(prefswindow->window);
}
