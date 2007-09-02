/*
 * Claws Mail -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2007 Hiroyuki Yamamoto & The Claws Mail Team
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
	PREFS_TAGS_STRING,	/*!< string pointer managed by list store, 
				 *   and never touched or retrieved by 
				 *   us */ 
	PREFS_TAGS_ID,		/*!< pointer to string that is not managed by 
				 *   the list store, and which is retrieved
				 *   and touched by us */
	N_PREFS_TAGS_COLUMNS
};

static struct Tags
{
	GtkWidget *window;

	GtkWidget *ok_btn;

	GtkWidget *name_entry;
	GtkWidget *tags_list_view;
} tags;

static int modified = FALSE;

/* widget creating functions */
static void prefs_tags_create	(MainWindow *mainwin);
static void prefs_tags_set_dialog	(void);
static gint prefs_tags_clist_set_row	(GtkTreeIter *row);

/* callback functions */
static void prefs_tags_register_cb	(GtkWidget	*w,
					 gpointer	 data);
static void prefs_tags_substitute_cb	(GtkWidget	*w,
					 gpointer	 data);
static void prefs_tags_delete_cb	(GtkWidget	*w,
					 gpointer	 data);
static gint prefs_tags_deleted		(GtkWidget	*widget,
					 GdkEventAny	*event,
					 gpointer	*data);
static gboolean prefs_tags_key_pressed(GtkWidget	*widget,
					  GdkEventKey	*event,
					  gpointer	 data);
static void prefs_tags_ok		(GtkWidget	*w,
					 gpointer	 data);


static GtkListStore* prefs_tags_create_data_store	(void);

static void prefs_tags_list_view_insert_tag	(GtkWidget *list_view,
							 GtkTreeIter *row_iter,
							 gchar *tag,
							 gint id);
static GtkWidget *prefs_tags_list_view_create	(void);
static void prefs_tags_create_list_view_columns	(GtkWidget *list_view);
static gboolean prefs_tags_selected			(GtkTreeSelection *selector,
							 GtkTreeModel *model, 
							 GtkTreePath *path,
							 gboolean currently_selected,
							 gpointer data);

void prefs_tags_open(MainWindow *mainwin)
{
	if (!tags.window)
		prefs_tags_create(mainwin);

	manage_window_set_transient(GTK_WINDOW(tags.window));
	gtk_widget_grab_focus(tags.ok_btn);

	prefs_tags_set_dialog();

	gtk_widget_show(tags.window);
}

/*!
 *\brief	Save Gtk object size to prefs dataset
 */
static void prefs_tags_size_allocate_cb(GtkWidget *widget,
					 GtkAllocation *allocation)
{
	g_return_if_fail(allocation != NULL);

	prefs_common.tagswin_width = allocation->width;
	prefs_common.tagswin_height = allocation->height;
}

static void prefs_tags_create(MainWindow *mainwin)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *help_btn;
	GtkWidget *ok_btn;
	GtkWidget *confirm_area;

	GtkWidget *vbox1;
	GtkWidget *table;

	GtkWidget *name_label;
	GtkWidget *name_entry;

	GtkWidget *reg_hbox;
	GtkWidget *btn_hbox;
	GtkWidget *arrow;
	GtkWidget *reg_btn;
	GtkWidget *subst_btn;
	GtkWidget *del_btn;

	GtkWidget *cond_hbox;
	GtkWidget *cond_scrolledwin;
	GtkWidget *cond_list_view;


	GtkWidget *btn_vbox;
	static GdkGeometry geometry;

	debug_print("Creating tags configuration window...\n");

	window = gtkut_window_new(GTK_WINDOW_TOPLEVEL, "prefs_tags");

	gtk_container_set_border_width(GTK_CONTAINER (window), 8);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	gtk_window_set_resizable(GTK_WINDOW(window), TRUE);

	vbox = gtk_vbox_new(FALSE, 6);
	gtk_widget_show(vbox);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	gtkut_stock_button_set_create_with_help(&confirm_area, &help_btn,
			&ok_btn, GTK_STOCK_OK,
			NULL, NULL,
			NULL, NULL);
	gtk_widget_show(confirm_area);
	gtk_box_pack_end(GTK_BOX(vbox), confirm_area, FALSE, FALSE, 0);
	gtk_widget_grab_default(ok_btn);

	gtk_window_set_title(GTK_WINDOW(window), _("Tags configuration"));
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(prefs_tags_deleted), NULL);
	g_signal_connect(G_OBJECT(window), "size_allocate",
			 G_CALLBACK(prefs_tags_size_allocate_cb), NULL);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(prefs_tags_key_pressed), NULL);
	MANAGE_WINDOW_SIGNALS_CONNECT(window);
	g_signal_connect(G_OBJECT(ok_btn), "clicked",
			 G_CALLBACK(prefs_tags_ok), mainwin);
	g_signal_connect(G_OBJECT(help_btn), "clicked",
			 G_CALLBACK(manual_open_with_anchor_cb),
			 MANUAL_ANCHOR_TAGS);

	vbox1 = gtk_vbox_new(FALSE, VSPACING);
	gtk_widget_show(vbox1);
	gtk_box_pack_start(GTK_BOX(vbox), vbox1, TRUE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(vbox1), 2);

	table = gtk_table_new(2, 2, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table), VSPACING_NARROW_2);
	gtk_table_set_col_spacings (GTK_TABLE (table), 4);
	gtk_widget_show(table);
	gtk_box_pack_start (GTK_BOX (vbox1), table, FALSE, FALSE, 0);

	name_label = gtk_label_new (_("Tag name"));
	gtk_widget_show (name_label);
	gtk_misc_set_alignment (GTK_MISC (name_label), 1, 0.5);
  	gtk_table_attach (GTK_TABLE (table), name_label, 0, 1, 0, 1,
                    	  (GtkAttachOptions) (GTK_FILL),
                    	  (GtkAttachOptions) (0), 0, 0);

	name_entry = gtk_entry_new ();
	gtk_widget_show (name_entry);
  	gtk_table_attach (GTK_TABLE (table), name_entry, 1, 2, 0, 1,
                    	  (GtkAttachOptions) (GTK_FILL|GTK_EXPAND),
                    	  (GtkAttachOptions) (0), 0, 0);

	/* register / delete */

	reg_hbox = gtk_hbox_new(FALSE, 4);
	gtk_widget_show(reg_hbox);
	gtk_box_pack_start(GTK_BOX(vbox1), reg_hbox, FALSE, FALSE, 0);

	arrow = gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_OUT);
	gtk_widget_show(arrow);
	gtk_box_pack_start(GTK_BOX(reg_hbox), arrow, FALSE, FALSE, 0);
	gtk_widget_set_size_request(arrow, -1, 16);

	btn_hbox = gtk_hbox_new(TRUE, 4);
	gtk_widget_show(btn_hbox);
	gtk_box_pack_start(GTK_BOX(reg_hbox), btn_hbox, FALSE, FALSE, 0);

	reg_btn = gtk_button_new_from_stock(GTK_STOCK_ADD);
	gtk_widget_show(reg_btn);
	gtk_box_pack_start(GTK_BOX(btn_hbox), reg_btn, FALSE, TRUE, 0);
	g_signal_connect(G_OBJECT(reg_btn), "clicked",
			 G_CALLBACK(prefs_tags_register_cb), NULL);

	subst_btn = gtkut_get_replace_btn(_("Replace"));
	gtk_widget_show(subst_btn);
	gtk_box_pack_start(GTK_BOX(btn_hbox), subst_btn, FALSE, TRUE, 0);
	g_signal_connect(G_OBJECT(subst_btn), "clicked",
			 G_CALLBACK(prefs_tags_substitute_cb),
			 NULL);

	del_btn = gtk_button_new_from_stock(GTK_STOCK_DELETE);
	gtk_widget_show(del_btn);
	gtk_box_pack_start(GTK_BOX(btn_hbox), del_btn, FALSE, TRUE, 0);
	g_signal_connect(G_OBJECT(del_btn), "clicked",
			 G_CALLBACK(prefs_tags_delete_cb), NULL);

	cond_hbox = gtk_hbox_new(FALSE, 8);
	gtk_widget_show(cond_hbox);
	gtk_box_pack_start(GTK_BOX(vbox1), cond_hbox, TRUE, TRUE, 0);

	cond_scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(cond_scrolledwin);
	gtk_widget_set_size_request(cond_scrolledwin, -1, 150);
	gtk_box_pack_start(GTK_BOX(cond_hbox), cond_scrolledwin,
			   TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW (cond_scrolledwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);

	cond_list_view = prefs_tags_list_view_create();				       
	gtk_widget_show(cond_list_view);
	gtk_container_add(GTK_CONTAINER (cond_scrolledwin), cond_list_view);

	btn_vbox = gtk_vbox_new(FALSE, 8);
	gtk_widget_show(btn_vbox);
	gtk_box_pack_start(GTK_BOX(cond_hbox), btn_vbox, FALSE, FALSE, 0);

	if (!geometry.min_height) {
		geometry.min_width = 486;
		geometry.min_height = 322;
	}

	gtk_window_set_geometry_hints(GTK_WINDOW(window), NULL, &geometry,
				      GDK_HINT_MIN_SIZE);
	gtk_widget_set_size_request(window, prefs_common.tagswin_width,
				    prefs_common.tagswin_height);

	gtk_widget_show(window);

	tags.window = window;
	tags.ok_btn = ok_btn;

	tags.name_entry = name_entry;

	tags.tags_list_view = cond_list_view;
}

static void prefs_tags_set_dialog(void)
{
	GtkListStore *store;
	GSList *cur, *orig;
	GtkTreeSelection *selection;
	GtkTreeIter iter;

	store = GTK_LIST_STORE(gtk_tree_view_get_model
				(GTK_TREE_VIEW(tags.tags_list_view)));
	gtk_list_store_clear(store);

	prefs_tags_list_view_insert_tag(tags.tags_list_view,
					      NULL, _("(New)"), -1);

	for (orig = cur = tags_get_list(); cur != NULL; cur = cur->next) {
		gint id = GPOINTER_TO_INT(cur->data);
		gchar *tag = (gchar *) tags_get_tag(id);
		
		prefs_tags_list_view_insert_tag(tags.tags_list_view,
						      NULL, tag, id);
	}

	g_slist_free(orig);
	/* select first entry */
	selection = gtk_tree_view_get_selection
		(GTK_TREE_VIEW(tags.tags_list_view));
	if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store),
					  &iter))
		gtk_tree_selection_select_iter(selection, &iter);
}

static gint prefs_tags_clist_set_row(GtkTreeIter *row)
{
	gchar *tag;
	GtkListStore *store;
	gint id = -1;

	store = GTK_LIST_STORE(gtk_tree_view_get_model
				(GTK_TREE_VIEW(tags.tags_list_view)));
	

	tag = gtk_editable_get_chars(GTK_EDITABLE(tags.name_entry), 0, -1);
	g_strstrip(tag);
	if (tag[0] == '\0') {
		alertpanel_error(_("Tag is not set."));
		return -1;
	}

	if (row == NULL) {
		if ((id = tags_add_tag(tag)) != -1) {
			prefs_tags_list_view_insert_tag(tags.tags_list_view,
	                                      row, tag, id);
			tags_write_tags();
		}
	} else {
		prefs_tags_list_view_insert_tag(tags.tags_list_view,
	                              row, tag, -1);
		tags_write_tags();
	}
	return 0;
}

/* callback functions */

static void prefs_tags_register_cb(GtkWidget *w, gpointer data)
{
	prefs_tags_clist_set_row(NULL);
	modified = FALSE;
}

static void prefs_tags_substitute_cb(GtkWidget *w, gpointer data)
{
	GtkTreeIter isel, inew;
	GtkTreePath *path_sel, *path_new;
	GtkTreeSelection *selection = gtk_tree_view_get_selection
					(GTK_TREE_VIEW(tags.tags_list_view));
	GtkTreeModel *model;					

	if (!gtk_tree_selection_get_selected(selection, &model, &isel))
		return;
	if (!gtk_tree_model_get_iter_first(model, &inew))
		return;

	path_sel = gtk_tree_model_get_path(model, &isel);
	path_new = gtk_tree_model_get_path(model, &inew);

	if (path_sel && path_new 
	&&  gtk_tree_path_compare(path_sel, path_new) != 0)
		prefs_tags_clist_set_row(&isel);

	gtk_tree_path_free(path_sel);
	gtk_tree_path_free(path_new);
	modified = FALSE;
}

static void prefs_tags_delete_cb(GtkWidget *w, gpointer data)
{
	GtkTreeIter sel;
	GtkTreeModel *model;
	gint id;

	if (!gtk_tree_selection_get_selected(gtk_tree_view_get_selection
				(GTK_TREE_VIEW(tags.tags_list_view)),
				&model, &sel))
		return;				

	if (alertpanel(_("Delete tag"),
		       _("Do you really want to delete this tag?"),
		       GTK_STOCK_CANCEL, GTK_STOCK_DELETE, NULL) != G_ALERTALTERNATE)
		return;

	/* XXX: Here's the reason why we need to store the original 
	 * pointer: we search the slist for it. */
	gtk_tree_model_get(model, &sel,
			   PREFS_TAGS_ID, &id,
			   -1);
	gtk_list_store_remove(GTK_LIST_STORE(model), &sel);
	tags_remove_tag(id);
	tags_write_tags();
}

static gint prefs_tags_deleted(GtkWidget *widget, GdkEventAny *event,
				  gpointer *data)
{
	prefs_tags_ok(widget, data);
	return TRUE;
}

static gboolean prefs_tags_key_pressed(GtkWidget *widget, GdkEventKey *event,
					  gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		prefs_tags_ok(widget, data);
	else {
		GtkWidget *focused = gtkut_get_focused_child(
					GTK_CONTAINER(widget));
		if (focused && GTK_IS_EDITABLE(focused)) {
			modified = TRUE;
		}
	}
	return FALSE;
}

static void prefs_tags_ok(GtkWidget *widget, gpointer data)
{
	if (modified && alertpanel(_("Entry not saved"),
				 _("The entry was not saved. Close anyway?"),
				 GTK_STOCK_CLOSE, _("+_Continue editing"),
				 NULL) != G_ALERTDEFAULT) {
		return;
	}
	modified = FALSE;

	main_window_reflect_tags_changes(mainwindow_get_mainwindow());
	gtk_widget_hide(tags.window);
}

static GtkListStore* prefs_tags_create_data_store(void)
{
	return gtk_list_store_new(N_PREFS_TAGS_COLUMNS,
				  G_TYPE_STRING,	
				  G_TYPE_INT,
				  -1);
}

static void prefs_tags_list_view_insert_tag(GtkWidget *list_view,
						  GtkTreeIter *row_iter,
						  gchar *tag,
						  gint id) 
{
	GtkTreeIter iter;
	GtkListStore *list_store = GTK_LIST_STORE(gtk_tree_view_get_model
					(GTK_TREE_VIEW(list_view)));

	if (row_iter == NULL) {
		/* append new */
		gtk_list_store_append(list_store, &iter);
		gtk_list_store_set(list_store, &iter,
				   PREFS_TAGS_STRING, tag,
				   PREFS_TAGS_ID, id,
				   -1);
	} else if (id == -1) {
		/* change existing */
		gtk_tree_model_get(GTK_TREE_MODEL(list_store), row_iter,
				   PREFS_TAGS_ID, &id,
				   -1);
		gtk_list_store_set(list_store, row_iter,
				   PREFS_TAGS_STRING, tag,
				   PREFS_TAGS_ID, id,
				   -1);
		tags_update_tag(id, tag);
	}
}

static GtkWidget *prefs_tags_list_view_create(void)
{
	GtkTreeView *list_view;
	GtkTreeSelection *selector;
	GtkTreeModel *model;

	model = GTK_TREE_MODEL(prefs_tags_create_data_store());
	list_view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(model));
	g_object_unref(model);	
	
	gtk_tree_view_set_rules_hint(list_view, prefs_common.use_stripes_everywhere);

	selector = gtk_tree_view_get_selection(list_view);
	gtk_tree_selection_set_mode(selector, GTK_SELECTION_BROWSE);
	gtk_tree_selection_set_select_function(selector, prefs_tags_selected,
					       NULL, NULL);

	/* create the columns */
	prefs_tags_create_list_view_columns(GTK_WIDGET(list_view));

	return GTK_WIDGET(list_view);
}

static void prefs_tags_create_list_view_columns(GtkWidget *list_view)
{
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes
		(_("Current tags"),
		 renderer,
		 "text", PREFS_TAGS_STRING,
		 NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(list_view), column);		
}

#define ENTRY_SET_TEXT(entry, str) \
	gtk_entry_set_text(GTK_ENTRY(entry), str ? str : "")

static gboolean prefs_tags_selected(GtkTreeSelection *selector,
				       GtkTreeModel *model, 
				       GtkTreePath *path,
				       gboolean currently_selected,
				       gpointer data)
{
	gchar *tag;
	GtkTreeIter iter;
	gint id;

	if (currently_selected)
		return TRUE;

	if (!gtk_tree_model_get_iter(model, &iter, path))
		return TRUE;

	gtk_tree_model_get(model, &iter, 
			   PREFS_TAGS_ID,  &id,
			   PREFS_TAGS_STRING, &tag,
			   -1);
	if (id == -1) {
		ENTRY_SET_TEXT(tags.name_entry, "");
		return TRUE;
	}
	
	ENTRY_SET_TEXT(tags.name_entry, tag);

	return TRUE;
}

gint prefs_tags_create_new(MainWindow *mainwin)
{
	gchar *new_tag = input_dialog(_("New tag"),
				_("New tag name:"), 
				""); 
	gint id = -1;
	if (!new_tag || !(*new_tag))
		return -1;
	
	g_strstrip(new_tag);
	id = tags_get_id_for_str(new_tag);
	if (id != -1) {
		g_free(new_tag);
		return id;
	}
	id = tags_add_tag(new_tag);
	g_free(new_tag);
	
	tags_write_tags();
	return id;
	
}

enum {
	TAG_SELECTED,
	TAG_SELECTED_INCONSISTENT,
	TAG_NAME,
	TAG_DATA,
	N_TAG_EDIT_COLUMNS
};

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
} applywindow;

static void apply_window_load_tags (void);

void tag_apply_open(GSList *msglist)
{
	g_return_if_fail(msglist);

	if (!applywindow.window)
		apply_window_create();

	manage_window_set_transient(GTK_WINDOW(applywindow.window));
	gtk_widget_grab_focus(applywindow.close_btn);
	
	applywindow.msglist = msglist;
	apply_window_load_tags();

	gtk_widget_show(applywindow.window);
	gtk_widget_grab_focus(applywindow.taglist);
	gtk_window_set_modal(GTK_WINDOW(applywindow.window), TRUE);
}

static GtkListStore* apply_window_create_data_store(void)
{
	return gtk_list_store_new(N_TAG_EDIT_COLUMNS,
				  G_TYPE_BOOLEAN,
				  G_TYPE_BOOLEAN,
				  G_TYPE_STRING,
  				  G_TYPE_POINTER,
				  -1);
}

static void tag_apply_selected_toggled(GtkCellRendererToggle *widget,
		gchar *path,
		GtkWidget *list_view);

static void apply_window_create_list_view_columns(GtkWidget *list_view)
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
	gtk_tree_view_append_column(GTK_TREE_VIEW(list_view), column);		
	g_signal_connect(G_OBJECT(renderer), "toggled",
			 G_CALLBACK(tag_apply_selected_toggled),
			 list_view);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes
		(_("Tag"),
		 renderer,
		 "text", TAG_NAME,
		 NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(list_view), column);
	gtk_tree_view_column_set_resizable(column, TRUE);

	gtk_tree_view_set_search_column(GTK_TREE_VIEW(list_view),
					TAG_NAME);
}

static GtkWidget *apply_window_list_view_create	(void)
{
	GtkTreeView *list_view;
	GtkTreeSelection *selector;
	GtkTreeModel *model;

	model = GTK_TREE_MODEL(apply_window_create_data_store());
	list_view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(model));
	g_object_unref(model);	
	
	gtk_tree_view_set_rules_hint(list_view, prefs_common.use_stripes_everywhere);
	
	selector = gtk_tree_view_get_selection(list_view);
	gtk_tree_selection_set_mode(selector, GTK_SELECTION_BROWSE);

	/* create the columns */
	apply_window_create_list_view_columns(GTK_WIDGET(list_view));

	return GTK_WIDGET(list_view);

}

static void apply_window_close(void) 
{
	g_slist_free(applywindow.msglist);
	applywindow.msglist = NULL;
	gtk_widget_hide(applywindow.window);
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
		if (id == -1) {
			id = tags_add_tag(new_tag);
			tags_write_tags();
			if (mainwindow_get_mainwindow())
				summary_set_tag(
					mainwindow_get_mainwindow()->summaryview, 
					id, NULL);
			main_window_reflect_tags_changes(mainwindow_get_mainwindow());
			apply_window_list_view_insert_tag(applywindow.taglist, NULL, id);
		} else {
			FindTagInStore fis;
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
		}
		g_free(new_tag);
	} else {
		alertpanel_error(_("Tag is not set."));
	}
}

static void apply_window_add_tag_cb(GtkWidget *widget,
			         gpointer data) 
{
	apply_window_add_tag();
	gtk_entry_set_text(GTK_ENTRY(applywindow.add_entry), "");
	gtk_widget_grab_focus(applywindow.taglist);
}

static gboolean apply_window_key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		apply_window_close();
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
	
	close_btn = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	gtk_box_pack_end(GTK_BOX(hbox1), close_btn, FALSE, FALSE, 0);

	gtk_widget_show(new_tag_label);
	gtk_widget_show(new_tag_entry);
	gtk_widget_show(close_btn);
	gtk_widget_show(add_btn);
	g_signal_connect(G_OBJECT(close_btn), "clicked",
			 G_CALLBACK(apply_window_close_cb), NULL);
	g_signal_connect(G_OBJECT(add_btn), "clicked",
			 G_CALLBACK(apply_window_add_tag_cb), NULL);

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

