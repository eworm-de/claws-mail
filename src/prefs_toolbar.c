/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2002 Hiroyuki Yamamoto & the Sylpheed-Claws team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/*
 * General functions for accessing address book files.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gtk/gtkoptionmenu.h>
#include <gdk/gdkkeysyms.h>
#include <stdio.h>
#include <stdlib.h>

#include "stock_pixmap.h"
#include "manage_window.h"
#include "gtkutils.h"
#include "mainwindow.h"
#include "alertpanel.h"
#include "prefs_common.h"

#include "utils.h"

#include "toolbar.h"
#include "prefs_toolbar.h"
#include "prefswindow.h"
#include "prefs_gtk.h"

enum
{
	SET_ICON	  = 0,
	SET_FILENAME	  = 1,
	SET_TEXT	  = 2,
	SET_EVENT	  = 3,
	SET_ICON_TEXT	  = 4,		/*!< "icon" text (separator) */ 
	SET_ICON_IS_TEXT  = 5,		/*!< icon is text representation */
	N_SET_COLUMNS
};

typedef struct _ToolbarPage
{
	PrefsPage  page;

	GtkWidget *window;		/* do not modify */

	GtkWidget *list_view_icons;
	GtkWidget *list_view_set;
	GtkWidget *combo_action;
	GtkWidget *combo_entry;
	GtkWidget *combo_list;
	GtkWidget *label_icon_text;
	GtkWidget *label_action_sel;
	GtkWidget *entry_icon_text;
	GtkWidget *combo_syl_action;
	GtkWidget *combo_syl_list;
	GtkWidget *combo_syl_entry;

	ToolbarType source;
	GList      *combo_action_list;

} ToolbarPage;

#define ERROR_MSG _("Selected Action already set.\nPlease choose another Action from List")

static void prefs_toolbar_populate               (ToolbarPage *prefs_toolbar);
static gboolean is_duplicate                     (ToolbarPage *prefs_toolbar,
						  gchar            *chosen_action);
static void prefs_toolbar_save                   (PrefsPage 	   *_page);

static void prefs_toolbar_register               (GtkButton        *button,
						  ToolbarPage *prefs_toolbar);
static void prefs_toolbar_substitute             (GtkButton        *button,
						  ToolbarPage *prefs_toolbar);
static void prefs_toolbar_delete                 (GtkButton        *button,
						  ToolbarPage *prefs_toolbar);

static void prefs_toolbar_up                     (GtkButton        *button,
						  ToolbarPage *prefs_toolbar);

static void prefs_toolbar_down                   (GtkButton        *button,
						  ToolbarPage *prefs_toolbar);

static void prefs_toolbar_create                 (ToolbarPage *prefs_toolbar);

static void prefs_toolbar_selection_changed      (GtkList *list, 
						  ToolbarPage *prefs_toolbar);

static GtkWidget *create_icon_list_view		 (ToolbarPage *prefs_toolbar);

static GtkWidget *create_set_list_view		 (ToolbarPage *prefs_toolbar);

static gboolean icon_list_selected		 (GtkTreeSelection *selector,
						  GtkTreeModel *model, 
						  GtkTreePath *path,
						  gboolean currently_selected,
						  ToolbarPage *prefs_toolbar);

static gboolean set_list_selected		 (GtkTreeSelection *selector,
						  GtkTreeModel *model, 
						  GtkTreePath *path,
						  gboolean currently_selected,
						  ToolbarPage *prefs_toolbar);

void prefs_toolbar_create_widget(PrefsPage *_page, GtkWindow *window, gpointer data)
{
	ToolbarPage *prefs_toolbar = (ToolbarPage *) _page;
	gchar *win_titles[3];
	win_titles[TOOLBAR_MAIN]    = _("Main toolbar configuration");
	win_titles[TOOLBAR_COMPOSE] = _("Compose toolbar configuration");  
	win_titles[TOOLBAR_MSGVIEW] = _("Message view toolbar configuration");  

	prefs_toolbar->window = GTK_WIDGET(window);

	toolbar_read_config_file(prefs_toolbar->source);

	prefs_toolbar_create(prefs_toolbar);
	prefs_toolbar_populate(prefs_toolbar);
}

void prefs_toolbar_save(PrefsPage *_page)
{
	ToolbarPage *prefs_toolbar = (ToolbarPage *) _page;
	GtkTreeView *list_view = GTK_TREE_VIEW(prefs_toolbar->list_view_set);
	GtkTreeModel *model = gtk_tree_view_get_model(list_view);
	GtkTreeIter iter;
	
	toolbar_clear_list(prefs_toolbar->source);

	if (!gtk_tree_model_iter_n_children(model, NULL)
	||  !gtk_tree_model_get_iter_first(model, &iter))
		toolbar_set_default(prefs_toolbar->source);
	else {
		do {
			ToolbarItem *item;
			gchar *fname, *text, *event; 
			
			item = g_new0(ToolbarItem, 1);

			gtk_tree_model_get(model, &iter,
					   SET_FILENAME, &fname,
					   SET_TEXT, &text,
					   SET_EVENT, &event,
					   -1);

			/* XXX: remember that G_TYPE_STRING returned by model
			 * is owned by caller of gtk_tree_model_get() */
			item->file  = fname;
			item->text  = text;
			item->index = toolbar_ret_val_from_descr(event);
			g_free(event);

			/* TODO: save A_SYL_ACTIONS only if they are still active */
			toolbar_set_list_item(item, prefs_toolbar->source);

			g_free(item->file);
			g_free(item->text);
			g_free(item);
		} while (gtk_tree_model_iter_next(model, &iter));
	}

	toolbar_save_config_file(prefs_toolbar->source);

	if (prefs_toolbar->source == TOOLBAR_MAIN) 
		main_window_reflect_prefs_all_real(TRUE);
	else if (prefs_toolbar->source == TOOLBAR_COMPOSE)
		compose_reflect_prefs_pixmap_theme();
	else if (prefs_toolbar->source == TOOLBAR_MSGVIEW)
		messageview_reflect_prefs_pixmap_theme();
}

static void prefs_toolbar_destroy_widget(PrefsPage *_page)
{
	ToolbarPage *prefs_toolbar = (ToolbarPage *) _page;

	g_list_free(prefs_toolbar->combo_action_list);
	prefs_toolbar->combo_action_list = NULL;
}

static void prefs_toolbar_set_displayed(ToolbarPage *prefs_toolbar)
{
	GSList *cur;
	GtkTreeView *list_view_set = GTK_TREE_VIEW(prefs_toolbar->list_view_set);
	GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model
						(list_view_set));
	GSList *toolbar_list = toolbar_get_list(prefs_toolbar->source);
	GtkTreeIter iter;

	gtk_list_store_clear(store);

	/* set currently active toolbar entries */
	for (cur = toolbar_list; cur != NULL; cur = cur->next) {
		ToolbarItem *item = (ToolbarItem*) cur->data;

		gtk_list_store_append(store, &iter);
	
		if (item->index != A_SEPARATOR) {
			GdkPixbuf *pix;
			StockPixmap icon = stock_pixmap_get_icon(item->file);
			
			stock_pixbuf_gdk(prefs_toolbar->window, icon, &pix);

			gtk_list_store_set(store, &iter, 
					   SET_ICON, pix,
					   SET_FILENAME, item->file,
					   SET_TEXT, item->text,
					   SET_EVENT, toolbar_ret_descr_from_val(item->index),
					   SET_ICON_TEXT, NULL,	
					   SET_ICON_IS_TEXT, FALSE,
					   -1);
		} else {
			gtk_list_store_set(store, &iter,
					   SET_ICON, NULL,
					   SET_FILENAME, toolbar_ret_descr_from_val(A_SEPARATOR),
					   SET_TEXT, (const gchar *) "", 
					   SET_EVENT, toolbar_ret_descr_from_val(A_SEPARATOR),
					   SET_ICON_TEXT, (const gchar *) SEPARATOR_PIXMAP,
					   SET_ICON_IS_TEXT, TRUE,
					   -1);
		}
	}

	/* select first */
	gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter);
	gtk_tree_selection_select_iter(gtk_tree_view_get_selection
						(list_view_set),
				       &iter);	
}

static void prefs_toolbar_populate(ToolbarPage *prefs_toolbar)
{
	gint i;
	GSList *cur;
	GList *syl_actions = NULL;
	GtkTreeView *list_view_icons = GTK_TREE_VIEW
			(prefs_toolbar->list_view_icons);
	GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model
						(list_view_icons));			
	gchar *act;
	GtkTreeIter iter;
	
	gtk_list_store_clear(store);

	/* set available icons */
	gtk_list_store_append(store, &iter);
	gtk_list_store_set(store, &iter,
			   SET_ICON, NULL,
			   SET_ICON_TEXT, SEPARATOR_PIXMAP,
			   SET_ICON_IS_TEXT, TRUE,
			   SET_FILENAME, toolbar_ret_descr_from_val(A_SEPARATOR),
			   -1);

	prefs_toolbar->combo_action_list = toolbar_get_action_items(prefs_toolbar->source);
	gtk_combo_set_popdown_strings(GTK_COMBO(prefs_toolbar->combo_action),	
				      prefs_toolbar->combo_action_list);
	gtk_combo_set_value_in_list(GTK_COMBO(prefs_toolbar->combo_action), 0, FALSE);
	gtk_entry_set_text(GTK_ENTRY(prefs_toolbar->combo_entry), prefs_toolbar->combo_action_list->data);

	/* get currently defined sylpheed actions */
	if (prefs_common.actions_list != NULL) {

		for (cur = prefs_common.actions_list; cur != NULL; cur = cur->next) {
			act = (gchar *)cur->data;
			syl_actions = g_list_append(syl_actions, act);
		} 

		gtk_combo_set_popdown_strings(GTK_COMBO(prefs_toolbar->combo_syl_action), syl_actions);
		gtk_combo_set_value_in_list(GTK_COMBO(prefs_toolbar->combo_syl_action), 0, FALSE);
		gtk_entry_set_text(GTK_ENTRY(prefs_toolbar->combo_syl_entry), syl_actions->data);
		prefs_toolbar_selection_changed(GTK_LIST(prefs_toolbar->combo_syl_list), prefs_toolbar);
		g_list_free(syl_actions);
	}

	for (i = 0; i < STOCK_PIXMAP_SYLPHEED_LOGO; i++) {
		GdkPixbuf *pixbuf;

		stock_pixbuf_gdk(prefs_toolbar->window, i, &pixbuf);
		
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
				   SET_ICON, pixbuf,
				   SET_FILENAME, stock_pixmap_get_name((StockPixmap) i),
				   SET_ICON_TEXT, NULL,
				   SET_ICON_IS_TEXT, FALSE,
				   -1);
 	}

	gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter);
	gtk_tree_selection_select_iter(gtk_tree_view_get_selection
						(list_view_icons),
				       &iter);		

	prefs_toolbar_set_displayed(prefs_toolbar);

	toolbar_clear_list(prefs_toolbar->source);
}

static gboolean is_duplicate(ToolbarPage *prefs_toolbar, gchar *chosen_action)
{
	GtkTreeView *list_view_set = GTK_TREE_VIEW
					(prefs_toolbar->list_view_set);
	GtkTreeModel *model_set = gtk_tree_view_get_model(list_view_set);					
	gchar *entry;
	gchar *syl_act = toolbar_ret_descr_from_val(A_SYL_ACTIONS);
	GtkTreeIter iter;
	gboolean result;

	g_return_val_if_fail(chosen_action != NULL, TRUE);

	if (!gtk_tree_model_iter_n_children(model_set, NULL))
		return FALSE;
	
	/* allow duplicate entries (A_SYL_ACTIONS) */
	if (g_utf8_collate(syl_act, chosen_action) == 0)
		return FALSE;

	if (!gtk_tree_model_get_iter_first(model_set, &iter))
		return FALSE;

	result = FALSE;
	do {
		gtk_tree_model_get(model_set, &iter,
				   SET_EVENT, &entry, 
				   -1);
		if (g_utf8_collate(chosen_action, entry) == 0) 
			result = TRUE;
		g_free(entry);			
	} while (!result && gtk_tree_model_iter_next(model_set, &iter));

	return result;
}

static void prefs_toolbar_default(GtkButton *button, ToolbarPage *prefs_toolbar)
{
	toolbar_clear_list(prefs_toolbar->source);
	toolbar_set_default(prefs_toolbar->source);
	prefs_toolbar_set_displayed(prefs_toolbar);
}

/*!
 *\return	String that should be freed by caller.
 */
static void get_action_name(const gchar *entry, gchar **menu)
{
	gchar *act, *act_p;

	if (prefs_common.actions_list != NULL) {
		
		act = g_strdup(entry);
		act_p = strstr(act, ": ");
		if (act_p != NULL)
			act_p[0] = 0x00;
		/* freed by calling func */
		*menu = act;
	}
}

static void prefs_toolbar_register(GtkButton *button, ToolbarPage *prefs_toolbar)
{
	GtkTreeView *list_view_set   = GTK_TREE_VIEW(prefs_toolbar->list_view_set);
	GtkTreeView *list_view_icons = GTK_TREE_VIEW(prefs_toolbar->list_view_icons);
	GtkTreeModel *model_icons = gtk_tree_view_get_model(list_view_icons);
	gchar *syl_act = toolbar_ret_descr_from_val(A_SYL_ACTIONS);
	GtkListStore *store_set;
	GtkTreeIter iter;
	gchar *fname;

	/* move selection in icon list view to set list view */

	if (!gtk_tree_model_iter_n_children(model_icons, NULL))
		return;
	
	if (!gtk_tree_selection_get_selected
			(gtk_tree_view_get_selection(list_view_icons),
			 NULL, &iter))
		return;

	gtk_tree_model_get(model_icons, &iter,
			   SET_FILENAME, &fname,
			   -1);

	store_set = GTK_LIST_STORE(gtk_tree_view_get_model(list_view_set));

	/* SEPARATOR or other ? */
	if (g_utf8_collate(fname, toolbar_ret_descr_from_val(A_SEPARATOR)) == 0) {
		gtk_list_store_append(store_set, &iter);
		gtk_list_store_set(store_set, &iter,
				   SET_ICON, NULL,
				   SET_FILENAME, NULL,
				   SET_TEXT, NULL,
				   SET_EVENT, toolbar_ret_descr_from_val(A_SEPARATOR),
				   SET_ICON_TEXT, (const gchar *) SEPARATOR_PIXMAP,
				   SET_ICON_IS_TEXT, TRUE,
				   -1);
	} else {
		GdkPixbuf *pixbuf;
		gchar *event, *text;
		
		event = g_strdup(gtk_entry_get_text
				(GTK_ENTRY(prefs_toolbar->combo_entry)));

		if (is_duplicate(prefs_toolbar, event)) {
			alertpanel_error(ERROR_MSG);
			g_free(event);
			g_free(fname);
			return;
		}

		stock_pixbuf_gdk(prefs_toolbar->window, 
				 stock_pixmap_get_icon(fname),
				 &pixbuf);

		if (g_utf8_collate(event, syl_act) == 0) {
			const gchar *entry = gtk_entry_get_text(GTK_ENTRY(prefs_toolbar->combo_syl_entry));
			get_action_name(entry, &text);
		} else
			text = gtk_editable_get_chars
				(GTK_EDITABLE(prefs_toolbar->entry_icon_text), 0, -1);

		gtk_list_store_append(store_set, &iter);
		gtk_list_store_set(store_set, &iter,
				   SET_ICON, pixbuf,
				   SET_FILENAME, fname,
				   SET_TEXT, text,
				   SET_EVENT, event,
				   SET_ICON_TEXT, NULL,
				   SET_ICON_IS_TEXT, FALSE,
				   -1);
		g_free(text);
		g_free(event);
	}

	g_free(fname);
	
	gtk_tree_selection_select_iter(gtk_tree_view_get_selection
						(list_view_set),
				       &iter);
}

static void prefs_toolbar_substitute(GtkButton *button, ToolbarPage *prefs_toolbar)
{
	GtkTreeView *list_view_set   = GTK_TREE_VIEW(prefs_toolbar->list_view_set);
	GtkTreeView *list_view_icons = GTK_TREE_VIEW(prefs_toolbar->list_view_icons);
	GtkTreeModel *model_icons = gtk_tree_view_get_model(list_view_icons);
	GtkListStore *store_set   = GTK_LIST_STORE(gtk_tree_view_get_model(list_view_set));
	gchar *syl_act = toolbar_ret_descr_from_val(A_SYL_ACTIONS);
	GtkTreeSelection *sel_icons;
	GtkTreeSelection *sel_set;
	GtkTreeIter iter_icons;
	GtkTreeIter iter_set;
	gchar *icon_fname;

	/* replace selection in set list with the one in icon list */

	if (!gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store_set), NULL))
		return;
		
	sel_icons = gtk_tree_view_get_selection(list_view_icons);
	if (!gtk_tree_selection_get_selected(sel_icons, NULL, &iter_icons))
		return;
		
	sel_set = gtk_tree_view_get_selection(list_view_set);		
	if (!gtk_tree_selection_get_selected(sel_set, NULL, &iter_set))
		return;

	gtk_tree_model_get(model_icons, &iter_icons, 
			   SET_FILENAME, &icon_fname, 
			   -1);
	
	if (g_utf8_collate(icon_fname, toolbar_ret_descr_from_val(A_SEPARATOR)) == 0) {
		gtk_list_store_set(store_set, &iter_set, 
				   SET_ICON, NULL,
				   SET_TEXT, NULL,
				   SET_EVENT, toolbar_ret_descr_from_val(A_SEPARATOR),
				   SET_FILENAME, icon_fname,
				   SET_ICON_TEXT, (const gchar *) SEPARATOR_PIXMAP,
				   SET_ICON_IS_TEXT, TRUE,
				   -1);
	} else {
		GdkPixbuf *pixbuf;
		gchar *icon_event, *set_event, *text;

		icon_event = gtk_editable_get_chars
				(GTK_EDITABLE(prefs_toolbar->combo_entry), 
				 0, -1);

		gtk_tree_model_get(GTK_TREE_MODEL(store_set), &iter_set, 
						  SET_EVENT, &set_event,
						  -1);

		if (is_duplicate(prefs_toolbar, icon_event) 
		&&  g_utf8_collate(icon_event, set_event) != 0){
			alertpanel_error(ERROR_MSG);
			g_free(set_event);
			g_free(icon_event);
			g_free(icon_fname);
			return;
		}

		stock_pixbuf_gdk(prefs_toolbar->window, 
				 stock_pixmap_get_icon(icon_fname),
				 &pixbuf);

		if (g_utf8_collate(icon_event, syl_act) == 0) {
			const gchar *entry = gtk_entry_get_text(GTK_ENTRY(prefs_toolbar->combo_syl_entry));
			get_action_name(entry, &text);
		} else {
			text = gtk_editable_get_chars(GTK_EDITABLE(prefs_toolbar->entry_icon_text),
						      0, -1);
		}

		/* change the row */
		gtk_list_store_set(store_set, &iter_set,
				   SET_ICON, pixbuf,
				   SET_FILENAME, icon_fname,
				   SET_TEXT, text,
				   SET_EVENT, icon_event,
				   SET_ICON_TEXT, NULL,
				   SET_ICON_IS_TEXT, FALSE,
				   -1);
	
		g_free(icon_event);
		g_free(set_event);
		g_free(text);
	}
	
	g_free(icon_fname);
}

static void prefs_toolbar_delete(GtkButton *button, ToolbarPage *prefs_toolbar)
{
	GtkTreeView *list_view_set = GTK_TREE_VIEW(prefs_toolbar->list_view_set);
	GtkListStore *store_set = GTK_LIST_STORE(gtk_tree_view_get_model
							(list_view_set));
	GtkTreeIter iter_set;							

	if (!gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store_set), NULL))
		return;
	
	if (!gtk_tree_selection_get_selected(gtk_tree_view_get_selection
							(list_view_set),
					     NULL,
					     &iter_set))
		return;					     
	
	gtk_list_store_remove(store_set, &iter_set);

	/* XXX: may need to select a list item */
}

static void prefs_toolbar_up(GtkButton *button, ToolbarPage *prefs_toolbar)
{
	GtkTreePath *prev, *sel;
	GtkTreeIter isel;
	GtkListStore *store = NULL;
	GtkTreeModel *model = NULL;
	GtkTreeIter iprev;
	
	if (!gtk_tree_selection_get_selected
		(gtk_tree_view_get_selection
			(GTK_TREE_VIEW(prefs_toolbar->list_view_set)),
		 &model,	
		 &isel))
		return;
	store = (GtkListStore *)model;

	sel = gtk_tree_model_get_path(GTK_TREE_MODEL(store), &isel);
	if (!sel)
		return;
	
	/* no move if we're at row 0... */
	prev = gtk_tree_path_copy(sel);
	if (!gtk_tree_path_prev(prev)) {
		gtk_tree_path_free(prev);
		gtk_tree_path_free(sel);
		return;
	}

	gtk_tree_model_get_iter(GTK_TREE_MODEL(store),
				&iprev, prev);
	gtk_tree_path_free(sel);
	gtk_tree_path_free(prev);

	gtk_list_store_swap(store, &iprev, &isel);
}

static void prefs_toolbar_down(GtkButton *button, ToolbarPage *prefs_toolbar)
{
	GtkListStore *store = NULL;
	GtkTreeModel *model = NULL;
	GtkTreeIter next, sel;
	
	if (!gtk_tree_selection_get_selected
		(gtk_tree_view_get_selection
			(GTK_TREE_VIEW(prefs_toolbar->list_view_set)),
		 &model,
		 &sel))
		return;

	store = (GtkListStore *)model;
	next = sel;
	if (!gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &next)) 
		return;

	gtk_list_store_swap(store, &next, &sel);
}

static void prefs_toolbar_selection_changed(GtkList *list,
					    ToolbarPage *prefs_toolbar)
{
	gchar *cur_entry = g_strdup(gtk_entry_get_text(GTK_ENTRY(prefs_toolbar->combo_entry)));
	gchar *actions_entry = toolbar_ret_descr_from_val(A_SYL_ACTIONS);

	gtk_widget_set_sensitive(prefs_toolbar->combo_syl_action, TRUE);

	if (g_utf8_collate(cur_entry, actions_entry) == 0) {
		gtk_widget_hide(prefs_toolbar->entry_icon_text);
		gtk_widget_show(prefs_toolbar->combo_syl_action);
		gtk_label_set_text(GTK_LABEL(prefs_toolbar->label_icon_text), _("Sylpheed Action"));

		if (prefs_common.actions_list == NULL) {
		    gtk_widget_set_sensitive(prefs_toolbar->combo_syl_action, FALSE);
		}

	} else {
		gtk_widget_hide(prefs_toolbar->combo_syl_action);
		gtk_widget_show(prefs_toolbar->entry_icon_text);
		gtk_label_set_text(GTK_LABEL(prefs_toolbar->label_icon_text), _("Toolbar text"));
	}

	gtk_misc_set_alignment(GTK_MISC(prefs_toolbar->label_icon_text), 1, 0.5);
	gtk_widget_show(prefs_toolbar->label_icon_text);
	g_free(cur_entry);
}

static void prefs_toolbar_create(ToolbarPage *prefs_toolbar)
{
	GtkWidget *main_vbox;
	GtkWidget *top_hbox;
	GtkWidget *compose_frame;
	GtkWidget *reg_hbox;
	GtkWidget *arrow;
	GtkWidget *btn_hbox;
	GtkWidget *reg_btn;
	GtkWidget *subst_btn;
	GtkWidget *del_btn;
	GtkWidget *default_btn;
	GtkWidget *vbox_frame;
	GtkWidget *table;
	GtkWidget *scrolledwindow_list_view_icons;
	GtkWidget *list_view_icons;
	GtkWidget *label_icon_text;
	GtkWidget *entry_icon_text;
	GtkWidget *label_action_sel;
	GtkWidget *empty_label;
	GtkWidget *combo_action;
	GtkWidget *combo_entry;
	GtkWidget *combo_list;
	GtkWidget *combo_syl_action;
	GtkWidget *combo_syl_entry;
	GtkWidget *combo_syl_list;
	GtkWidget *frame_toolbar_items;
	GtkWidget *hbox_bottom;
	GtkWidget *scrolledwindow_list_view_set;
	GtkWidget *list_view_set;

	GtkWidget *btn_vbox;
	GtkWidget *up_btn;
	GtkWidget *down_btn;
 
	debug_print("Creating custom toolbar window...\n");

	main_vbox = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(main_vbox);
	
	top_hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(main_vbox), top_hbox, TRUE, TRUE, 0);
  
	compose_frame = gtk_frame_new(_("Available toolbar icons"));
	gtk_box_pack_start(GTK_BOX(top_hbox), compose_frame, TRUE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(compose_frame), 5);

	vbox_frame = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(compose_frame), vbox_frame);
	
	/* available icons */
	scrolledwindow_list_view_icons = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_set_border_width(GTK_CONTAINER(scrolledwindow_list_view_icons), 5);
	gtk_container_add(GTK_CONTAINER(vbox_frame), scrolledwindow_list_view_icons);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow_list_view_icons), 
				       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	
	list_view_icons = create_icon_list_view(prefs_toolbar);
	gtk_widget_show(list_view_icons);
	gtk_container_add(GTK_CONTAINER(scrolledwindow_list_view_icons), list_view_icons);
	gtk_container_set_border_width(GTK_CONTAINER(list_view_icons), 1);
	gtk_widget_set_size_request(list_view_icons, 225, 108);

	table = gtk_table_new (2, 3, FALSE);
	gtk_container_add (GTK_CONTAINER (vbox_frame), table);
	gtk_container_set_border_width (GTK_CONTAINER (table), 8);
	gtk_table_set_row_spacings (GTK_TABLE (table), 8);
	gtk_table_set_col_spacings (GTK_TABLE (table), 8);
	
	/* icon description */
	label_icon_text = gtk_label_new("");
	gtk_misc_set_alignment(GTK_MISC(label_icon_text), 0, 0.5);
	gtk_widget_show (label_icon_text);
	gtk_table_attach (GTK_TABLE (table), label_icon_text, 0, 1, 0, 1,
			  (GtkAttachOptions) (GTK_FILL),
			  (GtkAttachOptions) (0), 0, 0);

	entry_icon_text = gtk_entry_new();
	gtk_table_attach (GTK_TABLE (table), entry_icon_text, 1, 2, 0, 1,
			  (GtkAttachOptions) (/*GTK_EXPAND | */GTK_FILL),
			  (GtkAttachOptions) (0), 0, 0);

	/* Sylpheed Action Combo Box */
	combo_syl_action = gtk_combo_new();
	combo_syl_list = GTK_COMBO(combo_syl_action)->list;
	combo_syl_entry = GTK_COMBO(combo_syl_action)->entry;
	gtk_entry_set_editable(GTK_ENTRY(combo_syl_entry), FALSE);
	gtk_table_attach (GTK_TABLE (table), combo_syl_action, 1, 2, 0, 1,
			  (GtkAttachOptions) (/*GTK_EXPAND | */GTK_FILL),
			  (GtkAttachOptions) (0), 0, 0);

	empty_label = gtk_label_new("");
	gtk_table_attach (GTK_TABLE (table), empty_label, 2, 3, 0, 1,
			  (GtkAttachOptions) (GTK_EXPAND),
			  (GtkAttachOptions) (0), 0, 0);
	/* available actions */
	label_action_sel = gtk_label_new(_("Event executed on click"));
	gtk_misc_set_alignment(GTK_MISC(label_action_sel), 0, 0.5);
	gtk_table_attach (GTK_TABLE (table), label_action_sel, 0, 1, 1, 2,
			  (GtkAttachOptions) (GTK_FILL),
			  (GtkAttachOptions) (0), 0, 0);

	combo_action = gtk_combo_new();
	gtk_table_attach (GTK_TABLE (table), combo_action, 1, 2, 1, 2,
			  (GtkAttachOptions) (GTK_FILL),
			  (GtkAttachOptions) (0), 0, 0);
	
	combo_list = GTK_COMBO(combo_action)->list;
	combo_entry = GTK_COMBO(combo_action)->entry;
	gtk_entry_set_editable(GTK_ENTRY(combo_entry), FALSE);
	
	empty_label = gtk_label_new("");
	gtk_table_attach (GTK_TABLE (table), empty_label, 2, 3, 1, 2,
			  (GtkAttachOptions) (GTK_EXPAND),
			  (GtkAttachOptions) (0), 0, 0);

	/* register / substitute / delete */
	reg_hbox = gtk_hbox_new(FALSE, 4);
	gtk_box_pack_start(GTK_BOX(main_vbox), reg_hbox, FALSE, FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(reg_hbox), 10);

	arrow = gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_OUT);
	gtk_box_pack_start(GTK_BOX(reg_hbox), arrow, FALSE, FALSE, 0);
	gtk_widget_set_size_request(arrow, -1, 16);

	btn_hbox = gtk_hbox_new(TRUE, 4);
	gtk_box_pack_start(GTK_BOX(reg_hbox), btn_hbox, FALSE, FALSE, 0);

	reg_btn = gtk_button_new_from_stock(GTK_STOCK_ADD);
	gtk_box_pack_start(GTK_BOX(btn_hbox), reg_btn, FALSE, TRUE, 0);
	g_signal_connect(G_OBJECT(reg_btn), "clicked",
			 G_CALLBACK(prefs_toolbar_register), 
			 prefs_toolbar);

	subst_btn = gtk_button_new_with_label(_("  Replace  "));
	gtk_box_pack_start(GTK_BOX(btn_hbox), subst_btn, FALSE, TRUE, 0);
	g_signal_connect(G_OBJECT(subst_btn), "clicked",
			 G_CALLBACK(prefs_toolbar_substitute),
			 prefs_toolbar);

	del_btn = gtk_button_new_from_stock(GTK_STOCK_DELETE);
	gtk_box_pack_start(GTK_BOX(btn_hbox), del_btn, FALSE, TRUE, 0);
	g_signal_connect(G_OBJECT(del_btn), "clicked",
			 G_CALLBACK(prefs_toolbar_delete), 
			  prefs_toolbar);

	default_btn = gtk_button_new_with_label(_(" Default "));
	gtk_box_pack_end(GTK_BOX(reg_hbox), default_btn, FALSE, TRUE, 0);
	g_signal_connect(G_OBJECT(default_btn), "clicked",
			 G_CALLBACK(prefs_toolbar_default), 
			 prefs_toolbar);

	/* currently active toolbar items */
	frame_toolbar_items = gtk_frame_new(_("Displayed toolbar items"));
	gtk_box_pack_start(GTK_BOX(main_vbox), frame_toolbar_items, TRUE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(frame_toolbar_items), 5);
	
	hbox_bottom = gtk_hbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(frame_toolbar_items), hbox_bottom);
	
	scrolledwindow_list_view_set = gtk_scrolled_window_new(NULL, NULL);
	gtk_box_pack_start(GTK_BOX(hbox_bottom), scrolledwindow_list_view_set, TRUE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(scrolledwindow_list_view_set), 1);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow_list_view_icons), 
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	list_view_set = create_set_list_view(prefs_toolbar); 
	gtk_widget_show(list_view_set);
	gtk_container_add(GTK_CONTAINER(scrolledwindow_list_view_set), list_view_set);
	gtk_widget_set_size_request(list_view_set, 225, 120);

	btn_vbox = gtk_vbox_new(FALSE, 8);
	gtk_widget_show(btn_vbox);
	gtk_box_pack_start(GTK_BOX(hbox_bottom), btn_vbox, FALSE, FALSE, 5);

	up_btn = gtk_button_new_from_stock(GTK_STOCK_GO_UP);
	gtk_widget_show(up_btn);
	gtk_box_pack_start(GTK_BOX(btn_vbox), up_btn, FALSE, FALSE, 2);

	down_btn = gtk_button_new_from_stock(GTK_STOCK_GO_DOWN);
	gtk_widget_show(down_btn);
	gtk_box_pack_start(GTK_BOX(btn_vbox), down_btn, FALSE, FALSE, 0);

	g_signal_connect(G_OBJECT(combo_list), "selection-changed",
			 G_CALLBACK(prefs_toolbar_selection_changed),
			 prefs_toolbar);
	g_signal_connect(G_OBJECT(up_btn), "clicked",
			 G_CALLBACK(prefs_toolbar_up), prefs_toolbar);
	g_signal_connect(G_OBJECT(down_btn), "clicked",
			 G_CALLBACK(prefs_toolbar_down), prefs_toolbar);
	
	gtk_widget_show_all(main_vbox);

	prefs_toolbar->list_view_icons  = list_view_icons;
	prefs_toolbar->list_view_set    = list_view_set;
	prefs_toolbar->combo_action     = combo_action;
	prefs_toolbar->combo_entry      = combo_entry;
	prefs_toolbar->combo_list       = combo_list;
	prefs_toolbar->entry_icon_text  = entry_icon_text;
	prefs_toolbar->combo_syl_action = combo_syl_action;
	prefs_toolbar->combo_syl_list   = combo_syl_list;
	prefs_toolbar->combo_syl_entry  = combo_syl_entry;

	prefs_toolbar->label_icon_text  = label_icon_text;
	prefs_toolbar->label_action_sel = label_action_sel;

	prefs_toolbar->page.widget = main_vbox;
}

ToolbarPage *prefs_toolbar_mainwindow;
ToolbarPage *prefs_toolbar_composewindow;
ToolbarPage *prefs_toolbar_messageview;

void prefs_toolbar_init(void)
{
	ToolbarPage *page;
	static gchar *mainpath[3], *messagepath[3], *composepath[3];

	mainpath[0] = _("Customize Toolbars");
	mainpath[1] = _("Main Window");
	mainpath[2] = NULL;

	page = g_new0(ToolbarPage, 1);
	page->page.path = mainpath;
	page->page.create_widget = prefs_toolbar_create_widget;
	page->page.destroy_widget = prefs_toolbar_destroy_widget;
	page->page.save_page = prefs_toolbar_save;
	page->source = TOOLBAR_MAIN;
	page->page.weight = 50.0;
	prefs_gtk_register_page((PrefsPage *) page);
	prefs_toolbar_mainwindow = page;

	messagepath[0] = _("Customize Toolbars");
	messagepath[1] = _("Message Window");
	messagepath[2] = NULL;

	page = g_new0(ToolbarPage, 1);
	page->page.path = messagepath;
	page->page.create_widget = prefs_toolbar_create_widget;
	page->page.destroy_widget = prefs_toolbar_destroy_widget;
	page->page.save_page = prefs_toolbar_save;
	page->source = TOOLBAR_MSGVIEW;
	page->page.weight = 45.0;
	prefs_gtk_register_page((PrefsPage *) page);
	prefs_toolbar_messageview = page;

	composepath[0] = _("Customize Toolbars");
	composepath[1] = _("Compose Window");
	composepath[2] = NULL;

	page = g_new0(ToolbarPage, 1);
	page->page.path = composepath;
	page->page.create_widget = prefs_toolbar_create_widget;
	page->page.destroy_widget = prefs_toolbar_destroy_widget;
	page->page.save_page = prefs_toolbar_save;
	page->source = TOOLBAR_COMPOSE;
	page->page.weight = 40.0;
	prefs_gtk_register_page((PrefsPage *) page);
	prefs_toolbar_composewindow = page;
}

void prefs_toolbar_done(void)
{
	prefs_gtk_unregister_page((PrefsPage *) prefs_toolbar_mainwindow);
	g_free(prefs_toolbar_mainwindow);
	prefs_gtk_unregister_page((PrefsPage *) prefs_toolbar_composewindow);
	g_free(prefs_toolbar_composewindow);
	prefs_gtk_unregister_page((PrefsPage *) prefs_toolbar_messageview);
	g_free(prefs_toolbar_messageview);
}

static void set_visible_if_not_text(GtkTreeViewColumn *col,
				    GtkCellRenderer   *renderer,
			            GtkTreeModel      *model,
				    GtkTreeIter       *iter,
				    gpointer           user_data)
{
	gboolean is_text;
	GdkPixbuf *pixbuf;

	gtk_tree_model_get(model, iter, SET_ICON_IS_TEXT, &is_text, -1);
	if (is_text) {
		g_object_set(renderer, "visible", FALSE, NULL); 
	} else {
		pixbuf = NULL;
		gtk_tree_model_get(model, iter, 
				   SET_ICON, &pixbuf,
				   -1);
		/* note getting a pixbuf from a tree model increases
		 * its refcount ... */
		g_object_unref(pixbuf);
		
		g_object_set(renderer, "visible", TRUE, NULL);
		g_object_set(renderer, "pixbuf",  pixbuf, NULL);
	}
}

static GtkWidget *create_icon_list_view(ToolbarPage *prefs_toolbar)
{
	GtkTreeView *list_view;
	GtkListStore *store;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selector;

	/* XXX: for icon list we don't need SET_TEXT, and SET_EVENT */
	store = gtk_list_store_new(N_SET_COLUMNS, 
				   GDK_TYPE_PIXBUF,
				   G_TYPE_STRING,
				   G_TYPE_STRING,
				   G_TYPE_STRING,
				   G_TYPE_STRING,
				   G_TYPE_BOOLEAN,
				   -1);
	list_view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(GTK_TREE_MODEL(store)));
	g_object_unref(G_OBJECT(store));

	column = gtk_tree_view_column_new();
	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	
	/* tell pixbuf renderer it is only visible if 
	 * the icon is not represented by text */
	gtk_tree_view_column_set_cell_data_func(column, renderer,
						set_visible_if_not_text,
						NULL, NULL);
	
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	
	/* tell the text renderer it is only visible if the icon
	 * is represented by an image */
	gtk_tree_view_column_set_attributes(column, renderer,
					    "visible", SET_ICON_IS_TEXT, 
					    "text", SET_ICON_TEXT,
					    NULL);

	gtk_tree_view_append_column(list_view, column);

	column = gtk_tree_view_column_new();
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_set_attributes(column, renderer,
					    "text", SET_FILENAME,
					    NULL);
	
	gtk_tree_view_append_column(list_view, column);

	/* various other tree view attributes */
	gtk_tree_view_set_rules_hint(list_view, prefs_common.enable_rules_hint);
	gtk_tree_view_set_headers_visible(list_view, FALSE);
	
	selector = gtk_tree_view_get_selection(list_view);
	gtk_tree_selection_set_mode(selector, GTK_SELECTION_BROWSE);
	gtk_tree_selection_set_select_function
		(selector, (GtkTreeSelectionFunc) icon_list_selected,
	         prefs_toolbar, NULL);

	return GTK_WIDGET(list_view);	
}

static GtkWidget *create_set_list_view(ToolbarPage *prefs_toolbar)
{
	GtkTreeView *list_view;
	GtkListStore *store;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selector;

	store = gtk_list_store_new(N_SET_COLUMNS, 
				   GDK_TYPE_PIXBUF,
				   G_TYPE_STRING,
				   G_TYPE_STRING,
				   G_TYPE_STRING,
				   G_TYPE_STRING,
				   G_TYPE_BOOLEAN,
				   -1);
	list_view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(GTK_TREE_MODEL(store)));
	g_object_unref(G_OBJECT(store));

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("Icon"));
	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	
	/* tell pixbuf renderer it is only visible if 
	 * the icon is not represented by text */
	gtk_tree_view_column_set_cell_data_func(column, renderer,
						set_visible_if_not_text,
						NULL, NULL);
	
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	
	/* tell the text renderer it is only visible if the icon
	 * is represented by an image */
	gtk_tree_view_column_set_attributes(column, renderer,
					    "visible", SET_ICON_IS_TEXT,
					    "text", SET_ICON_TEXT,
					    NULL);

	gtk_tree_view_append_column(list_view, column);

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("File name"));
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	gtk_tree_view_column_set_attributes(column, renderer,
					    "text", SET_FILENAME,
					    NULL);
	
	gtk_tree_view_append_column(list_view, column);

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("Icon text"));
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	gtk_tree_view_column_set_attributes(column, renderer,
					    "text", SET_TEXT,
					    NULL);
	gtk_tree_view_append_column(list_view, column);

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("Mapped event"));
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	gtk_tree_view_column_set_attributes(column, renderer,
					    "text", SET_EVENT,
					    NULL);
	gtk_tree_view_append_column(list_view, column);

	/* various other tree view attributes */
	gtk_tree_view_set_rules_hint(list_view, prefs_common.enable_rules_hint);
	
	selector = gtk_tree_view_get_selection(list_view);
	gtk_tree_selection_set_mode(selector, GTK_SELECTION_BROWSE);
	gtk_tree_selection_set_select_function
		(selector, (GtkTreeSelectionFunc) set_list_selected,
	         prefs_toolbar, NULL);

	return GTK_WIDGET(list_view);	

}

static gboolean icon_list_selected(GtkTreeSelection *selector,
			           GtkTreeModel *model, 
				   GtkTreePath *path,
				   gboolean currently_selected,
				   ToolbarPage *prefs_toolbar)
{
	GtkTreeIter iter;
	gchar *text;
	
	if (currently_selected ||!gtk_tree_model_get_iter(model, &iter, path))
		return TRUE;

	text = NULL;
	gtk_tree_model_get(model, &iter, 
			   SET_FILENAME, &text,
			   -1);
	if (!text) 
		return TRUE;

	if (g_utf8_collate(toolbar_ret_descr_from_val(A_SEPARATOR), text) == 0) {
		gtk_widget_set_sensitive(prefs_toolbar->combo_action,     FALSE);
		gtk_widget_set_sensitive(prefs_toolbar->entry_icon_text,  FALSE);
		gtk_widget_set_sensitive(prefs_toolbar->combo_syl_action, FALSE);
	} else {
		gtk_widget_set_sensitive(prefs_toolbar->combo_action,     TRUE);
		gtk_widget_set_sensitive(prefs_toolbar->entry_icon_text,  TRUE);
		gtk_widget_set_sensitive(prefs_toolbar->combo_syl_action, TRUE);
	}
	
	g_free(text);

	return TRUE;
}

static gboolean set_list_selected(GtkTreeSelection *selector,
			          GtkTreeModel *model, 
				  GtkTreePath *path,
				  gboolean currently_selected,
				  ToolbarPage *prefs_toolbar)
{
	GtkTreeIter iter;
	GtkTreeView *list_ico = GTK_TREE_VIEW(prefs_toolbar->list_view_icons);
	gchar *syl_act = toolbar_ret_descr_from_val(A_SYL_ACTIONS);
	gchar *file, *icon_text, *descr;
	GList *cur;
	gint item_num;
	GtkTreeModel *model_ico;
	
	if (currently_selected ||!gtk_tree_model_get_iter(model, &iter, path))
		return TRUE;
	
	gtk_tree_model_get(model, &iter,
			   SET_FILENAME, &file,
			   SET_TEXT, &icon_text,
			   SET_EVENT, &descr,
			   -1);

	if (g_utf8_collate(descr, syl_act)) 
		gtk_entry_set_text(GTK_ENTRY(prefs_toolbar->entry_icon_text), 
				   icon_text);
	
	/* scan combo list for selected description an set combo item accordingly */
	for (cur = prefs_toolbar->combo_action_list, item_num = 0; cur != NULL; 
	     cur = cur->next) {
		gchar *item_str = (gchar*)cur->data;
		if (g_utf8_collate(item_str, descr) == 0) {
			gtk_list_select_item(GTK_LIST(prefs_toolbar->combo_list), item_num);
			break;
		}
		else
			item_num++;
	}

	model_ico = gtk_tree_view_get_model(list_ico);
	if (gtk_tree_model_get_iter_first(model_ico, &iter)) {
		gchar *entry;
		gboolean found = FALSE;
		
		/* find in icon list, scroll to item */
		do {
			entry = NULL;
			gtk_tree_model_get(model_ico, &iter,
					   SET_FILENAME, &entry,
					   -1);
			found = entry && !g_utf8_collate(entry, file);
			g_free(entry);
			if (found) 
				break;
		} while(gtk_tree_model_iter_next(model_ico, &iter));

		if (found) {
			GtkTreePath *path;

			gtk_tree_selection_select_iter
				(gtk_tree_view_get_selection(list_ico), &iter);	
			path = gtk_tree_model_get_path(model_ico, &iter); 
			gtk_tree_view_set_cursor(list_ico, path, NULL, FALSE);
			gtk_tree_path_free(path);
		}
	}

	g_free(file);
	g_free(icon_text);
	g_free(descr);

	return TRUE;
}
