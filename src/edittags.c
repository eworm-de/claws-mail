/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2007-2022 Colin Leroy and The Claws Mail Team
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
	gint res;

	gtk_tree_model_get(model, a, TAG_NAME, &name1, -1);
	gtk_tree_model_get(model, b, TAG_NAME, &name2, -1);
	
	if (name1 == NULL)
		return name2 == NULL ? 0:1;
	
	if (name2 == NULL)
		return 1;
	
	res = g_utf8_collate(name1,name2);
	g_free(name1);
	g_free(name2);

	return res;
}

static void tags_window_create(void);

static struct TagsWindow
{
	GtkWidget *window;
	GtkWidget *hbox1;
	GtkWidget *vbox1;
	GtkWidget *label;
	GtkWidget *taglist;
	GtkWidget *close_btn;
	GtkWidget *add_entry;
	GtkWidget *add_btn;
	GtkWidget *del_btn;
	GSList *msglist;
	gboolean has_tag_col;
	GdkCursor *watch_cursor;
	gboolean busy;
} tagswindow;

#define TAGS_WINDOW_LOCK() {								\
	gdk_window_set_cursor(gtk_widget_get_window(tagswindow.window), tagswindow.watch_cursor);	\
	tagswindow.busy = TRUE;							\
}

#define TAGS_WINDOW_UNLOCK() {								\
	gdk_window_set_cursor(gtk_widget_get_window(tagswindow.window), NULL);			\
	tagswindow.busy = FALSE;							\
}

static void tags_window_load_tags (void);
static void tags_window_insert_check_column(GtkWidget *list_view);
static void new_tag_set_add_sensitivity(void);

void tags_window_open(GSList *msglist)
{
	if (!tagswindow.window)
		tags_window_create();

	manage_window_set_transient(GTK_WINDOW(tagswindow.window));
	gtk_widget_grab_focus(tagswindow.close_btn);
	
	tagswindow.msglist = msglist;
	tags_window_load_tags();

	if (msglist != NULL && !tagswindow.has_tag_col) {
		tags_window_insert_check_column(tagswindow.taglist);
		tagswindow.has_tag_col = TRUE;
	}
	if (msglist == NULL && tagswindow.has_tag_col) {
		gtk_tree_view_remove_column(GTK_TREE_VIEW(tagswindow.taglist),
			gtk_tree_view_get_column(GTK_TREE_VIEW(tagswindow.taglist), 0));
		tagswindow.has_tag_col = FALSE;
	} 
	if (msglist == NULL) {
		gtk_widget_hide(tagswindow.label);
		gtk_widget_show(tagswindow.del_btn);
		gtk_window_set_title(GTK_WINDOW(tagswindow.window),
				     C_("Window title", "Tags configuration"));
	} else {
		gtk_widget_show(tagswindow.label);
		gtk_widget_hide(tagswindow.del_btn);
		gtk_window_set_title(GTK_WINDOW(tagswindow.window),
				     C_("Window title", "Modify tags"));
	}

	gtk_widget_show(tagswindow.window);
	gtk_widget_grab_focus(tagswindow.taglist);
	gtk_window_set_modal(GTK_WINDOW(tagswindow.window), TRUE);
}

static GtkListStore* tags_window_create_data_store(void)
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

static void tags_selected_toggled(GtkCellRendererToggle *widget,
		gchar *path,
		GtkWidget *list_view);
static void tags_selected_edited(GtkCellRendererText *widget,
		gchar *arg1, gchar *arg2,
		GtkWidget *list_view);

static void tags_window_insert_check_column(GtkWidget *list_view)
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
			 G_CALLBACK(tags_selected_toggled),
			 list_view);
}

static void tags_window_create_list_view_columns(GtkWidget *list_view)
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
			 G_CALLBACK(tags_selected_edited),
			 list_view);

}

static GtkWidget *tags_popup_menu = NULL;

static void tags_popup_delete (GtkAction *action, gpointer data)
{
	GtkTreeIter sel;
	GtkTreeModel *model;
	gint id;
	SummaryView *summaryview = NULL;
	GtkTreeSelection *selection = gtk_tree_view_get_selection
				(GTK_TREE_VIEW(tagswindow.taglist));
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(tagswindow.taglist));

	if (!gtk_tree_selection_get_selected(selection, NULL, &sel))
		return;

	if (alertpanel(_("Delete tag"),
		       _("Do you really want to delete this tag?"),
		       NULL, _("_Cancel"), "edit-delete", _("D_elete"), NULL, NULL,
		       ALERTFOCUS_FIRST) != G_ALERTALTERNATE)
		return;

	TAGS_WINDOW_LOCK();

	/* XXX: Here's the reason why we need to store the original 
	 * pointer: we search the slist for it. */
	gtk_tree_model_get(model, &sel,
			   TAG_DATA, &id,
			   -1);

	/* Even though this is not documented, gtk_tree_model_get()
	 * seems to invalidate the GtkTreeIter that is passed to it,
	 * so we need to reacquire it. */
	if (!gtk_tree_selection_get_selected(selection, NULL, &sel))
		return;

	gtk_list_store_remove(GTK_LIST_STORE(model), &sel);
	if (mainwindow_get_mainwindow() != NULL)
		summaryview = mainwindow_get_mainwindow()->summaryview;
	if (summaryview)
		summary_set_tag(summaryview, -id, NULL);
	tags_remove_tag(id);
	tags_write_tags();
	TAGS_WINDOW_UNLOCK();
}

static void tags_popup_delete_all (GtkAction *action, gpointer data)
{
	GSList *cur, *tags;
	GtkTreeModel *model;
	SummaryView *summaryview = NULL;
	
	if (alertpanel(_("Delete all tags"),
		       _("Do you really want to delete all tags?"),
		       NULL, _("_Cancel"), "edit-delete", _("D_elete"), NULL, NULL,
		       ALERTFOCUS_FIRST) != G_ALERTALTERNATE)
		return;

	TAGS_WINDOW_LOCK();

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(tagswindow.taglist));
	gtk_list_store_clear(GTK_LIST_STORE(model));

	if (mainwindow_get_mainwindow() != NULL)
		summaryview = mainwindow_get_mainwindow()->summaryview;
	cur = tags = tags_get_list();
	for (; cur; cur = cur->next) {
		gint id = GPOINTER_TO_INT(cur->data);
		if (summaryview)
			summary_set_tag(summaryview, -id, NULL);
		tags_remove_tag(id);
	}
	g_slist_free(tags);
	tags_write_tags();

	TAGS_WINDOW_UNLOCK();
}

static GtkActionEntry tags_popup_actions[] =
{
	{ "EditTags", NULL, "EditTags", NULL, NULL, NULL },
	{ "EditTags/Delete", NULL, N_("_Delete"), NULL, "Delete tag", G_CALLBACK(tags_popup_delete) },
	{ "EditTags/DeleteAll", NULL, N_("Delete _all"), NULL, "Delete all tags", G_CALLBACK(tags_popup_delete_all) }
};

static gint tags_list_btn_pressed(GtkWidget *widget, GdkEventButton *event,
				  GtkTreeView *list_view)
{
	GtkUIManager *gui_manager = gtkut_ui_manager();

	if (tagswindow.busy)
		return FALSE;

	if (event && event->button == 3) {
		GtkTreeModel *model = gtk_tree_view_get_model(list_view);
		GtkTreeIter iter;
		gboolean non_empty;

		if (!tags_popup_menu) {
			(void) cm_menu_create_action_group("EditTags", tags_popup_actions,
					G_N_ELEMENTS(tags_popup_actions), (gpointer)list_view);

			MENUITEM_ADDUI("/Menus", "EditTags", "EditTags", GTK_UI_MANAGER_MENU)
			MENUITEM_ADDUI("/Menus/EditTags", "Delete", "EditTags/Delete",
					GTK_UI_MANAGER_MENUITEM)
			MENUITEM_ADDUI("/Menus/EditTags", "DeleteAll", "EditTags/DeleteAll",
					GTK_UI_MANAGER_MENUITEM)

			tags_popup_menu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(
						gtk_ui_manager_get_widget(gui_manager, "/Menus/EditTags")) );
		}

		/* grey out popup menu items if list is empty */
		non_empty = gtk_tree_model_get_iter_first(model, &iter);
		cm_menu_set_sensitive("EditTags/Delete", non_empty);
		cm_menu_set_sensitive("EditTags/DeleteAll", non_empty);

		gtk_menu_popup_at_pointer(GTK_MENU(tags_popup_menu), NULL);

		return FALSE;
	}
	return FALSE;
}

static gboolean tags_list_popup_menu(GtkWidget *widget, gpointer data)
{
	GtkTreeView *list_view = (GtkTreeView *)data;
	GdkEventButton event;
	
	event.button = 3;
	event.time = gtk_get_current_event_time();
	
	tags_list_btn_pressed(NULL, &event, list_view);

	return TRUE;
}

static GtkWidget *tags_window_list_view_create	(void)
{
	GtkTreeView *list_view;
	GtkTreeSelection *selector;
	GtkTreeModel *model;

	model = GTK_TREE_MODEL(tags_window_create_data_store());
	list_view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(model));
	g_object_unref(model);	
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(model), TAG_NAME, GTK_SORT_ASCENDING);

	gtk_tree_view_set_rules_hint(list_view, prefs_common.use_stripes_everywhere);
	
	selector = gtk_tree_view_get_selection(list_view);
	gtk_tree_selection_set_mode(selector, GTK_SELECTION_BROWSE);

	/* create the columns */
	tags_window_create_list_view_columns(GTK_WIDGET(list_view));

	g_signal_connect(G_OBJECT(list_view), "popup-menu",
			 G_CALLBACK(tags_list_popup_menu), list_view);
	g_signal_connect(G_OBJECT(list_view), "button-press-event",
			G_CALLBACK(tags_list_btn_pressed), list_view);
	return GTK_WIDGET(list_view);

}

static void tags_window_list_view_clear_tags(GtkWidget *list_view);

static void tags_window_close(void) 
{
	if (tagswindow.busy)
		return;

	if (tagswindow.msglist)
		g_slist_free(tagswindow.msglist);
	tagswindow.msglist = NULL;
	TAGS_WINDOW_LOCK();
	main_window_reflect_tags_changes(mainwindow_get_mainwindow());
	TAGS_WINDOW_UNLOCK();
	tags_window_list_view_clear_tags(tagswindow.taglist);
	gtk_widget_hide(tagswindow.window);
	gtk_window_set_modal(GTK_WINDOW(tagswindow.window), FALSE);
}

static void tags_window_close_cb(GtkWidget *widget,
			         gpointer data) 
{
	tags_window_close();
}

static void tags_window_list_view_insert_tag(GtkWidget *list_view,
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

static void tags_window_add_tag(void)
{
	gchar *new_tag = gtk_editable_get_chars(GTK_EDITABLE(tagswindow.add_entry), 0, -1);
	if (!new_tag)
		return;
	g_strstrip(new_tag);
	if (*new_tag) {
		gint id;
		FindTagInStore fis;
		if (!(IS_NOT_RESERVED_TAG(new_tag))) {
			alertpanel_error(_("You entered a reserved tag name, please choose another instead."));
			g_free(new_tag);
			return;
		}
		id = tags_get_id_for_str(new_tag);
		TAGS_WINDOW_LOCK();
		if (id == -1) {
			id = tags_add_tag(new_tag);
			tags_write_tags();
			if (mainwindow_get_mainwindow())
				summary_set_tag(
					mainwindow_get_mainwindow()->summaryview, 
					id, NULL);
			main_window_reflect_tags_changes(mainwindow_get_mainwindow());
			tags_window_list_view_insert_tag(tagswindow.taglist, NULL, id);
		} 
		fis.tag_id = id;
		fis.path = NULL;
		gtk_tree_model_foreach(gtk_tree_view_get_model
				(GTK_TREE_VIEW(tagswindow.taglist)), 
				(GtkTreeModelForeachFunc) find_tag_in_store,
			       	&fis);
		if (fis.path) {
			GtkTreeSelection *selection;
			GtkTreePath* path;
			GtkTreeModel *model = gtk_tree_view_get_model(
				GTK_TREE_VIEW(tagswindow.taglist));

			if (mainwindow_get_mainwindow())
				summary_set_tag(
					mainwindow_get_mainwindow()->summaryview, 
					id, NULL);
			selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tagswindow.taglist));
			gtk_tree_selection_select_iter(selection, &fis.iter);
			path = gtk_tree_model_get_path(model, &fis.iter);
			/* XXX returned path may not be valid??? create new one to be sure */ 
			gtk_tree_view_set_cursor(GTK_TREE_VIEW(tagswindow.taglist), path, NULL, FALSE);
			tags_window_list_view_insert_tag(tagswindow.taglist, &fis.iter, id);
			gtk_tree_path_free(path);
		}
		TAGS_WINDOW_UNLOCK();
	}
	g_free(new_tag);
}

static void tags_window_add_tag_cb(GtkWidget *widget, gpointer data) 
{
	if (tagswindow.busy)
		return;
	tags_window_add_tag();
	gtk_entry_set_text(GTK_ENTRY(tagswindow.add_entry), "");
	gtk_widget_grab_focus(tagswindow.taglist);
}

static void tags_window_del_tag_cb(GtkWidget *widget, gpointer data) 
{
	if (tagswindow.busy)
		return;
	tags_popup_delete(NULL, NULL);
	gtk_widget_grab_focus(tagswindow.taglist);
}

static gboolean tags_window_key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (tagswindow.busy)
		return FALSE;
	if (event && event->keyval == GDK_KEY_Escape)
		tags_window_close();
	else if (event && event->keyval == GDK_KEY_Delete)
		tags_popup_delete(NULL, NULL);
	return FALSE;
}

static gboolean tags_window_add_key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (tagswindow.busy)
		return FALSE;

	if (event && (event->keyval == GDK_KEY_KP_Enter || event->keyval == GDK_KEY_Return)) {
		tags_window_add_tag();
		gtk_entry_set_text(GTK_ENTRY(tagswindow.add_entry), "");
		gtk_widget_grab_focus(tagswindow.taglist);
	}
		
	return FALSE;
}

/*!
 *\brief	Save Gtk object size to prefs dataset
 */
static void tags_window_size_allocate_cb(GtkWidget *widget,
					 GtkAllocation *allocation)
{
	cm_return_if_fail(allocation != NULL);

	gtk_window_get_size(GTK_WINDOW(widget),
		&prefs_common.tagswin_width, &prefs_common.tagswin_height);
}

static void tags_window_create(void) 
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
	static GdkGeometry geometry;

	window = gtkut_window_new(GTK_WINDOW_TOPLEVEL, "tags_window");

	gtk_container_set_border_width(GTK_CONTAINER(window), 8);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_resizable(GTK_WINDOW(window), TRUE);
	gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DIALOG);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(tags_window_close_cb), NULL);
	g_signal_connect(G_OBJECT(window), "size_allocate",
			 G_CALLBACK(tags_window_size_allocate_cb), NULL);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(tags_window_key_pressed), NULL);
	MANAGE_WINDOW_SIGNALS_CONNECT(window);

	vbox1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);

	new_tag_label = gtk_label_new(_("New tag:"));
	gtk_label_set_xalign(GTK_LABEL(new_tag_label), 0.0);
	gtk_box_pack_start(GTK_BOX(hbox1), new_tag_label, FALSE, FALSE, 0);

	new_tag_entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox1), new_tag_entry, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(new_tag_entry), "key_press_event",
			 G_CALLBACK(tags_window_add_key_pressed), NULL);

	add_btn = gtkut_stock_button("list-add", _("_Add"));
	gtk_box_pack_start(GTK_BOX(hbox1), add_btn, FALSE, FALSE, 0);
	CLAWS_SET_TIP(add_btn,
			_("Add the new tag"));

	del_btn = gtkut_stock_button("edit-delete", _("D_elete"));
	gtk_box_pack_start(GTK_BOX(hbox1), del_btn, FALSE, FALSE, 0);
	CLAWS_SET_TIP(del_btn,
			_("Delete the selected tag"));

	close_btn = gtkut_stock_button("window-close", _("_Close"));
	gtk_box_pack_end(GTK_BOX(hbox1), close_btn, FALSE, FALSE, 0);

	gtk_widget_show(new_tag_label);
	gtk_widget_show(new_tag_entry);
	gtk_widget_show(close_btn);
	gtk_widget_show(add_btn);

	g_signal_connect(G_OBJECT(close_btn), "clicked",
			 G_CALLBACK(tags_window_close_cb), NULL);
	g_signal_connect(G_OBJECT(add_btn), "clicked",
			 G_CALLBACK(tags_window_add_tag_cb), NULL);
	g_signal_connect(G_OBJECT(del_btn), "clicked",
			 G_CALLBACK(tags_window_del_tag_cb), NULL);

	taglist = tags_window_list_view_create();
	
	label = gtk_label_new(_("Choose the tag(s) for the message(s).\n"
				"Changes are immediately applied."));
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_box_pack_start(GTK_BOX(vbox1), label, FALSE, TRUE, 0);
	
	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
				       
	gtk_container_add(GTK_CONTAINER(scrolledwin), taglist);
	gtk_box_pack_start(GTK_BOX(vbox1), scrolledwin, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 0);

	gtk_widget_show(scrolledwin);
	gtk_widget_show(taglist);
	gtk_widget_show(hbox1);
	gtk_widget_show(vbox1);
	gtk_widget_show(close_btn);
	gtk_container_add(GTK_CONTAINER (window), vbox1);

	if (!geometry.min_height) {
		geometry.min_width = 500;
		geometry.min_height = 250;
	}

	gtk_window_set_geometry_hints(GTK_WINDOW(window), NULL, &geometry,
				      GDK_HINT_MIN_SIZE);
	gtk_window_set_default_size(GTK_WINDOW(window), prefs_common.tagswin_width,
				    prefs_common.tagswin_height);

	tagswindow.window = window;
	tagswindow.hbox1 = hbox1;
	tagswindow.vbox1 = vbox1;
	tagswindow.label = label;
	tagswindow.taglist = taglist;
	tagswindow.close_btn = close_btn;
	tagswindow.add_btn = add_btn;
	tagswindow.add_entry = new_tag_entry;
	tagswindow.del_btn = del_btn;
	tagswindow.has_tag_col = FALSE;
	tagswindow.watch_cursor = gdk_cursor_new_for_display(
			gtk_widget_get_display(window), GDK_WATCH);
	
	g_signal_connect(G_OBJECT(new_tag_entry), "changed", 
			 G_CALLBACK(new_tag_set_add_sensitivity), NULL);
	new_tag_set_add_sensitivity();
}

static void tags_window_list_view_clear_tags(GtkWidget *list_view)
{
	GtkListStore *list_store = GTK_LIST_STORE(gtk_tree_view_get_model
					(GTK_TREE_VIEW(list_view)));
	gtk_list_store_clear(list_store);
}


static void tags_selected_toggled(GtkCellRendererToggle *widget,
		gchar *path,
		GtkWidget *list_view)
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(list_view));
	gboolean enabled = TRUE, set = FALSE;
	gpointer tmp;
	gint tag_id;
	SummaryView *summaryview = NULL;

	if (tagswindow.busy)
		return;

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
	
	TAGS_WINDOW_LOCK();
	if (summaryview)
		summary_set_tag(summaryview, set ? tag_id : -tag_id, NULL);
	TAGS_WINDOW_UNLOCK();
}

static void tags_selected_edited(GtkCellRendererText *widget,
		gchar *path, gchar *new_text,
		GtkWidget *list_view)
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(list_view));
	gpointer tmp;
	gint tag_id;
	SummaryView *summaryview = NULL;
	gboolean selected;

	if (tagswindow.busy)
		return;

	if (mainwindow_get_mainwindow() != NULL)
		summaryview = mainwindow_get_mainwindow()->summaryview;

	if (!gtk_tree_model_get_iter_from_string(model, &iter, path))
		return;

	if (!new_text || !*new_text)
		return;

	gtk_tree_model_get(model, &iter,
			   TAG_SELECTED, &selected,
			   TAG_DATA, &tmp,
			   -1);

	tag_id = GPOINTER_TO_INT(tmp);
	
	TAGS_WINDOW_LOCK();
	if (selected) {
		if (summaryview)
			summary_set_tag(summaryview, -tag_id, NULL);
	}
	
	tags_update_tag(tag_id, new_text);
	
	gtk_list_store_set(GTK_LIST_STORE(model), &iter,
			   TAG_NAME, new_text,
			   -1);
	if (selected) {
		if (summaryview)
			summary_set_tag(summaryview, tag_id, NULL);
	} else  {
		if (summaryview)
			summary_set_tag(summaryview, 0, NULL);
	}	
	TAGS_WINDOW_UNLOCK();
}

static void tags_window_get_selected_state(gint tag, gboolean *selected, gboolean *selected_inconsistent)
{
	GSList *cur = tagswindow.msglist;
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

static void tags_window_list_view_insert_tag(GtkWidget *list_view,
					     GtkTreeIter *row_iter,
					     gint tag) 
{
	GtkTreeIter iter;
	GtkListStore *list_store = GTK_LIST_STORE(gtk_tree_view_get_model
					(GTK_TREE_VIEW(list_view)));
	const gchar *name = tags_get_tag(tag);
	gboolean selected = FALSE, selected_inconsistent = FALSE;

	tags_window_get_selected_state(tag, &selected, &selected_inconsistent);
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

static void new_tag_set_add_sensitivity()
{
	const gchar *newtag = gtk_entry_get_text((GtkEntry *)tagswindow.add_entry);
	gchar *trim_text = g_strdup(newtag);
	g_strstrip(trim_text);
	gtk_widget_set_sensitive(tagswindow.add_btn, (*trim_text != 0));
	g_free(trim_text);

}

static void tags_window_load_tags (void) 
{
	GSList *cur, *tags;
	gint id;
	tags_window_list_view_clear_tags(tagswindow.taglist);
	
	cur = tags = tags_get_list();
	for (; cur; cur = cur->next) {
		id = GPOINTER_TO_INT(cur->data);
		tags_window_list_view_insert_tag(tagswindow.taglist, NULL, id);
	}
	g_slist_free(tags);
}
