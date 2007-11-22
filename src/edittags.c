/*
 * Claws Mail -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2007 The Claws Mail Team
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
 * 
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "menu.h"
#include "edittags.h"
#include "prefs_gtk.h"
#include "utils.h"
#include "gtkutils.h"
#include "inputdialog.h"
#include "manage_window.h"
#include "mainwindow.h"
#include "prefs_common.h"
#include "alertpanel.h"
#include "summaryview.h"
#include "tags.h"
#include "gtkutils.h"
#include "manual.h"

enum {
	TAG_SELECTED,
	TAG_SELECTED_INCONSISTENT,
	TAG_NAME,
	TAG_DATA,
	N_TAG_EDIT_COLUMNS
};

static gint tag_cmp_func (GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer userdata)
 {
	gchar *name1, *name2;

	gtk_tree_model_get(model, a, TAG_NAME, &name1, -1);
	gtk_tree_model_get(model, b, TAG_NAME, &name2, -1);
	
	if (name1 == NULL)
		return name2 == NULL ? 0:1;
	
	if (name2 == NULL)
		return name1 == NULL ? 0:1;
	
	return g_utf8_collate(name1,name2);
}

static void apply_window_create(void);

static struct TagApplyWindow
{
	GtkWidget *window;
	GtkWidget *hbox1;
	GtkWidget *vbox1;
	GtkWidget *label;
	GtkWidget *taglist;
	GtkWidget *close_btn;
	GtkWidget *add_entry;
	GtkWidget *add_btn;
	GSList *msglist;
	gboolean has_tag_col;
} applywindow;

static void apply_window_load_tags (void);
static void apply_window_insert_check_column(GtkWidget *list_view);

void tag_apply_open(GSList *msglist)
{
	if (!applywindow.window)
		apply_window_create();

	manage_window_set_transient(GTK_WINDOW(applywindow.window));
	gtk_widget_grab_focus(applywindow.close_btn);
	
	applywindow.msglist = msglist;
	apply_window_load_tags();

	if (msglist && !applywindow.has_tag_col) {
		apply_window_insert_check_column(applywindow.taglist);
		applywindow.has_tag_col = TRUE;
	}
	if (!msglist && applywindow.has_tag_col) {
		gtk_tree_view_remove_column(GTK_TREE_VIEW(applywindow.taglist),
			gtk_tree_view_get_column(GTK_TREE_VIEW(applywindow.taglist), 0));
		applywindow.has_tag_col = FALSE;
	} 

	gtk_widget_show(applywindow.window);
	gtk_widget_grab_focus(applywindow.taglist);
	gtk_window_set_modal(GTK_WINDOW(applywindow.window), TRUE);
}

static GtkListStore* apply_window_create_data_store(void)
{
	GtkListStore *store = gtk_list_store_new(N_TAG_EDIT_COLUMNS,
				  G_TYPE_BOOLEAN,
				  G_TYPE_BOOLEAN,
				  G_TYPE_STRING,
  				  G_TYPE_POINTER,
				  -1);
	GtkTreeSortable *sortable = GTK_TREE_SORTABLE(store);

	gtk_tree_sortable_set_sort_func(sortable, 0, tag_cmp_func,
                                    NULL, NULL);

	return store;
}

static void tag_apply_selected_toggled(GtkCellRendererToggle *widget,
		gchar *path,
		GtkWidget *list_view);
static void tag_apply_selected_edited(GtkCellRendererText *widget,
		gchar *arg1, gchar *arg2,
		GtkWidget *list_view);

static void apply_window_insert_check_column(GtkWidget *list_view)
{
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	renderer = gtk_cell_renderer_toggle_new();
	g_object_set(renderer,
		     "radio", FALSE,
		     "activatable", TRUE,
		     NULL);
	column = gtk_tree_view_column_new_with_attributes
		("",
		 renderer,
		 "active", TAG_SELECTED,
		 "inconsistent", TAG_SELECTED_INCONSISTENT,
		 NULL);
	gtk_tree_view_column_set_alignment (column, 0.5);
	gtk_tree_view_insert_column(GTK_TREE_VIEW(list_view), column, 0);		
	g_signal_connect(G_OBJECT(renderer), "toggled",
			 G_CALLBACK(tag_apply_selected_toggled),
			 list_view);
}

static void apply_window_create_list_view_columns(GtkWidget *list_view)
{
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	renderer = gtk_cell_renderer_text_new();
	g_object_set(G_OBJECT(renderer), "editable", TRUE, NULL);

	column = gtk_tree_view_column_new_with_attributes
		(_("Tag"),
		 renderer,
		 "text", TAG_NAME,
		 NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(list_view), column);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(list_view),
					TAG_NAME);
	g_signal_connect(G_OBJECT(renderer), "edited",
			 G_CALLBACK(tag_apply_selected_edited),
			 list_view);

}

static GtkItemFactory *apply_popup_factory = NULL;
static GtkWidget *apply_popup_menu = NULL;

static void apply_popup_delete (void *obj, guint action, void *data)
{
	GtkTreeIter sel;
	GtkTreeModel *model;
	gint id;
	SummaryView *summaryview = NULL;
	
	if (!gtk_tree_selection_get_selected(gtk_tree_view_get_selection
				(GTK_TREE_VIEW(applywindow.taglist)),
				&model, &sel))
		return;				

	if (alertpanel(_("Delete tag"),
		       _("Do you really want to delete this tag?"),
		       GTK_STOCK_CANCEL, GTK_STOCK_DELETE, NULL) != G_ALERTALTERNATE)
		return;

	/* XXX: Here's the reason why we need to store the original 
	 * pointer: we search the slist for it. */
	gtk_tree_model_get(model, &sel,
			   TAG_DATA, &id,
			   -1);
	gtk_list_store_remove(GTK_LIST_STORE(model), &sel);
	if (mainwindow_get_mainwindow() != NULL)
		summaryview = mainwindow_get_mainwindow()->summaryview;
	if (summaryview)
		summary_set_tag(summaryview, -id, NULL);
	tags_remove_tag(id);
	tags_write_tags();
}

static GtkItemFactoryEntry apply_popup_entries[] =
{
	{N_("/_Delete"),		NULL, apply_popup_delete, 0, NULL, NULL},
};

static gint apply_list_btn_pressed(GtkWidget *widget, GdkEventButton *event,
				    GtkTreeView *list_view)
{
	if (event && event->button == 3) {
		if (!apply_popup_menu) {
			gint n_entries = sizeof(apply_popup_entries) /
				sizeof(apply_popup_entries[0]);
			apply_popup_menu = menu_create_items(apply_popup_entries, n_entries,
						      "<TagPopupMenu>", &apply_popup_factory,
						      list_view);
		}
		gtk_menu_popup(GTK_MENU(apply_popup_menu), 
			       NULL, NULL, NULL, NULL, 
			       event->button, event->time);

		return FALSE;
	}
	return FALSE;
}

static gboolean apply_list_popup_menu(GtkWidget *widget, gpointer data)
{
	GtkTreeView *list_view = (GtkTreeView *)data;
	GdkEventButton event;
	
	event.button = 3;
	event.time = gtk_get_current_event_time();
	
	apply_list_btn_pressed(NULL, &event, list_view);

	return TRUE;
}

static GtkWidget *apply_window_list_view_create	(void)
{
	GtkTreeView *list_view;
	GtkTreeSelection *selector;
	GtkTreeModel *model;

	model = GTK_TREE_MODEL(apply_window_create_data_store());
	list_view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(model));
	g_object_unref(model);	
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(model), TAG_NAME, GTK_SORT_ASCENDING);

	gtk_tree_view_set_rules_hint(list_view, prefs_common.use_stripes_everywhere);
	
	selector = gtk_tree_view_get_selection(list_view);
	gtk_tree_selection_set_mode(selector, GTK_SELECTION_BROWSE);

	/* create the columns */
	apply_window_create_list_view_columns(GTK_WIDGET(list_view));

#ifndef MAEMO
	g_signal_connect(G_OBJECT(list_view), "popup-menu",
			 G_CALLBACK(apply_list_popup_menu), list_view);
#else
	gtk_widget_tap_and_hold_setup(GTK_WIDGET(list_view), NULL, NULL,
			GTK_TAP_AND_HOLD_NONE | GTK_TAP_AND_HOLD_NO_INTERNALS);
	g_signal_connect(G_OBJECT(list_view), "tap-and-hold",
			 G_CALLBACK(apply_list_popup_menu), list_view);
#endif
	g_signal_connect(G_OBJECT(list_view), "button-press-event",
			G_CALLBACK(apply_list_btn_pressed), list_view);
	return GTK_WIDGET(list_view);

}

static void apply_window_close(void) 
{
	if (applywindow.msglist)
		g_slist_free(applywindow.msglist);
	applywindow.msglist = NULL;
	gtk_widget_hide(applywindow.window);
	main_window_reflect_tags_changes(mainwindow_get_mainwindow());
}

static void apply_window_close_cb(GtkWidget *widget,
			         gpointer data) 
{
	apply_window_close();
}

static void apply_window_list_view_insert_tag(GtkWidget *list_view,
						  GtkTreeIter *row_iter,
						  gint tag);

typedef struct FindTagInStore {
	gint		 tag_id;
	GtkTreePath	*path;
	GtkTreeIter	 iter;
} FindTagInStore;

static gboolean find_tag_in_store(GtkTreeModel *model,
				      GtkTreePath  *path,
				      GtkTreeIter  *iter,
				      FindTagInStore *data)
{
	gpointer tmp;
	gtk_tree_model_get(model, iter, TAG_DATA, &tmp, -1);

	if (data->tag_id == GPOINTER_TO_INT(tmp)) {
		data->path = path; /* signal we found it */
		data->iter = *iter;
		return TRUE;
	}

	return FALSE; 
}

static void apply_window_add_tag(void)
{
	gchar *new_tag = gtk_editable_get_chars(GTK_EDITABLE(applywindow.add_entry), 0, -1);
	g_strstrip(new_tag);
	if (new_tag && *new_tag) {
		gint id = tags_get_id_for_str(new_tag);
		FindTagInStore fis;
		if (id == -1) {
			id = tags_add_tag(new_tag);
			tags_write_tags();
			if (mainwindow_get_mainwindow())
				summary_set_tag(
					mainwindow_get_mainwindow()->summaryview, 
					id, NULL);
			main_window_reflect_tags_changes(mainwindow_get_mainwindow());
			apply_window_list_view_insert_tag(applywindow.taglist, NULL, id);
		} 
		fis.tag_id = id;
		fis.path = NULL;
		gtk_tree_model_foreach(gtk_tree_view_get_model
				(GTK_TREE_VIEW(applywindow.taglist)), 
				(GtkTreeModelForeachFunc) find_tag_in_store,
			       	&fis);
		if (fis.path) {
			GtkTreeSelection *selection;
			GtkTreePath* path;
			GtkTreeModel *model = gtk_tree_view_get_model(
				GTK_TREE_VIEW(applywindow.taglist));

			if (mainwindow_get_mainwindow())
				summary_set_tag(
					mainwindow_get_mainwindow()->summaryview, 
					id, NULL);
			selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(applywindow.taglist));
			gtk_tree_selection_select_iter(selection, &fis.iter);
			path = gtk_tree_model_get_path(model, &fis.iter);
			/* XXX returned path may not be valid??? create new one to be sure */ 
			gtk_tree_view_set_cursor(GTK_TREE_VIEW(applywindow.taglist), path, NULL, FALSE);
			apply_window_list_view_insert_tag(applywindow.taglist, &fis.iter, id);
			gtk_tree_path_free(path);
		}
	} else {
		alertpanel_error(_("Tag is not set."));
	}
	g_free(new_tag);
}

static void apply_window_add_tag_cb(GtkWidget *widget,
			         gpointer data) 
{
	apply_window_add_tag();
	gtk_entry_set_text(GTK_ENTRY(applywindow.add_entry), "");
	gtk_widget_grab_focus(applywindow.taglist);
}

static void apply_window_del_tag_cb(GtkWidget *widget,
			         gpointer data) 
{
	apply_popup_delete(NULL, 0, NULL);
	gtk_widget_grab_focus(applywindow.taglist);
}

static gboolean apply_window_key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		apply_window_close();
	else if (event && event->keyval == GDK_Delete)
		apply_popup_delete(NULL, 0, NULL);
	return FALSE;
}

static gboolean apply_window_add_key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event && (event->keyval == GDK_KP_Enter || event->keyval == GDK_Return)) {
		apply_window_add_tag();
		gtk_entry_set_text(GTK_ENTRY(applywindow.add_entry), "");
		gtk_widget_grab_focus(applywindow.taglist);
	}
		
	return FALSE;
}

static void apply_window_create(void) 
{
	GtkWidget *window;
	GtkWidget *hbox1;
	GtkWidget *vbox1;
	GtkWidget *label;
	GtkWidget *taglist;
	GtkWidget *close_btn;
	GtkWidget *scrolledwin;
	GtkWidget *new_tag_label;
	GtkWidget *new_tag_entry;
	GtkWidget *add_btn;
	GtkWidget *del_btn;

	window = gtkut_window_new(GTK_WINDOW_TOPLEVEL, "tag_apply_window");
	gtk_window_set_title (GTK_WINDOW(window),
			      Q_("Dialog title|Apply tags"));

	gtk_container_set_border_width (GTK_CONTAINER (window), 8);
	gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
	gtk_window_set_resizable(GTK_WINDOW (window), TRUE);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(apply_window_close_cb), NULL);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(apply_window_key_pressed), NULL);
	MANAGE_WINDOW_SIGNALS_CONNECT (window);

	vbox1 = gtk_vbox_new(FALSE, 6);
	hbox1 = gtk_hbox_new(FALSE, 6);
	
	new_tag_label = gtk_label_new(_("New tag:"));
	gtk_misc_set_alignment(GTK_MISC(new_tag_label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox1), new_tag_label, FALSE, FALSE, 0);
	
	new_tag_entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox1), new_tag_entry, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(new_tag_entry), "key_press_event",
			 G_CALLBACK(apply_window_add_key_pressed), NULL);
	
	add_btn = gtk_button_new_from_stock(GTK_STOCK_ADD);
	gtk_box_pack_start(GTK_BOX(hbox1), add_btn, FALSE, FALSE, 0);
	
	del_btn = gtk_button_new_from_stock(GTK_STOCK_DELETE);
	gtk_box_pack_start(GTK_BOX(hbox1), del_btn, FALSE, FALSE, 0);
	
	close_btn = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	gtk_box_pack_end(GTK_BOX(hbox1), close_btn, FALSE, FALSE, 0);

	gtk_widget_show(new_tag_label);
	gtk_widget_show(new_tag_entry);
	gtk_widget_show(close_btn);
	gtk_widget_show(add_btn);
	gtk_widget_show(del_btn);

	g_signal_connect(G_OBJECT(close_btn), "clicked",
			 G_CALLBACK(apply_window_close_cb), NULL);
	g_signal_connect(G_OBJECT(add_btn), "clicked",
			 G_CALLBACK(apply_window_add_tag_cb), NULL);
	g_signal_connect(G_OBJECT(del_btn), "clicked",
			 G_CALLBACK(apply_window_del_tag_cb), NULL);

	taglist = apply_window_list_view_create();
	
	label = gtk_label_new(_("Please select tags to apply/remove. Changes are immediate."));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(vbox1), label, FALSE, TRUE, 0);
	
	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
				       
	gtk_widget_set_size_request(scrolledwin, 500, 250);

	gtk_container_add(GTK_CONTAINER(scrolledwin), taglist);
	gtk_box_pack_start(GTK_BOX(vbox1), scrolledwin, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 0);
	
	gtk_widget_show(label);
	gtk_widget_show(scrolledwin);
	gtk_widget_show(taglist);
	gtk_widget_show(hbox1);
	gtk_widget_show(vbox1);
	gtk_widget_show(close_btn);
	gtk_container_add(GTK_CONTAINER (window), vbox1);

	applywindow.window = window;
	applywindow.hbox1 = hbox1;
	applywindow.vbox1 = vbox1;
	applywindow.label = label;
	applywindow.taglist = taglist;
	applywindow.close_btn = close_btn;
	applywindow.add_btn = add_btn;
	applywindow.add_entry = new_tag_entry;
	applywindow.has_tag_col = FALSE;
}

static void apply_window_list_view_clear_tags(GtkWidget *list_view)
{
	GtkListStore *list_store = GTK_LIST_STORE(gtk_tree_view_get_model
					(GTK_TREE_VIEW(list_view)));
	gtk_list_store_clear(list_store);
}


static void tag_apply_selected_toggled(GtkCellRendererToggle *widget,
		gchar *path,
		GtkWidget *list_view)
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(list_view));
	gboolean enabled = TRUE, set = FALSE;
	gpointer tmp;
	gint tag_id;
	SummaryView *summaryview = NULL;
	
	if (mainwindow_get_mainwindow() != NULL)
		summaryview = mainwindow_get_mainwindow()->summaryview;

	if (!gtk_tree_model_get_iter_from_string(model, &iter, path))
		return;

	gtk_tree_model_get(model, &iter,
			   TAG_SELECTED, &enabled,
			   TAG_DATA, &tmp,
			   -1);

	set = !enabled;
	tag_id = GPOINTER_TO_INT(tmp);

	gtk_list_store_set(GTK_LIST_STORE(model), &iter,
			   TAG_SELECTED, set,
			   TAG_SELECTED_INCONSISTENT, FALSE,
			   -1);
	
	if (summaryview)
		summary_set_tag(summaryview, set ? tag_id : -tag_id, NULL);
}

static void tag_apply_selected_edited(GtkCellRendererText *widget,
		gchar *path, gchar *new_text,
		GtkWidget *list_view)
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(list_view));
	gpointer tmp;
	gint tag_id;
	SummaryView *summaryview = NULL;

	if (mainwindow_get_mainwindow() != NULL)
		summaryview = mainwindow_get_mainwindow()->summaryview;

	if (!gtk_tree_model_get_iter_from_string(model, &iter, path))
		return;

	if (!new_text || !*new_text)
		return;

	gtk_tree_model_get(model, &iter,
			   TAG_DATA, &tmp,
			   -1);

	tag_id = GPOINTER_TO_INT(tmp);
	tags_update_tag(tag_id, new_text);
	
	gtk_list_store_set(GTK_LIST_STORE(model), &iter,
			   TAG_NAME, new_text,
			   -1);
	if (summaryview)
		summary_set_tag(summaryview, 0, NULL);
}

static void apply_window_get_selected_state(gint tag, gboolean *selected, gboolean *selected_inconsistent)
{
	GSList *cur = applywindow.msglist;
	gint num_mails = 0;
	gint num_selected = 0;
	for (; cur; cur = cur->next) {
		MsgInfo *msginfo = (MsgInfo *)cur->data;
		num_mails++;
		if (msginfo->tags && g_slist_find(msginfo->tags, GINT_TO_POINTER(tag))) {
			*selected = TRUE;
			num_selected++;
		}
	}
	if (num_selected > 0 && num_selected < num_mails)
		*selected_inconsistent = TRUE;
	else
		*selected_inconsistent = FALSE;
}

static void apply_window_list_view_insert_tag(GtkWidget *list_view,
						  GtkTreeIter *row_iter,
						  gint tag) 
{
	GtkTreeIter iter;
	GtkListStore *list_store = GTK_LIST_STORE(gtk_tree_view_get_model
					(GTK_TREE_VIEW(list_view)));
	const gchar *name = tags_get_tag(tag);
	gboolean selected = FALSE, selected_inconsistent = FALSE;

	apply_window_get_selected_state(tag, &selected, &selected_inconsistent);
	if (row_iter == NULL) {
		/* append new */
		gtk_list_store_append(list_store, &iter);
		gtk_list_store_set(list_store, &iter,
				   TAG_SELECTED, selected,
				   TAG_SELECTED_INCONSISTENT, selected_inconsistent,
				   TAG_NAME, name,
				   TAG_DATA, GINT_TO_POINTER(tag),
				   -1);
	} else {
		gtk_list_store_set(list_store, row_iter,
				   TAG_SELECTED, selected,
				   TAG_SELECTED_INCONSISTENT, selected_inconsistent,
				   TAG_NAME, name,
				   TAG_DATA, GINT_TO_POINTER(tag),
				   -1);
	}
}

static void apply_window_load_tags (void) 
{
	GSList *cur;
	GSList *tags = tags_get_list();
	apply_window_list_view_clear_tags(applywindow.taglist);
	
	cur = tags;
	for (; cur; cur = cur->next) {
		gint id = GPOINTER_TO_INT(cur->data);
		apply_window_list_view_insert_tag(applywindow.taglist, NULL, id);
	}
}

