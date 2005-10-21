/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2003 Hiroyuki Yamamoto and the Sylpheed-Claws team
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
#include <string.h>
#include <errno.h>

#include "main.h"
#include "prefs_gtk.h"
#include "prefs_matcher.h"
#include "prefs_filtering.h"
#include "prefs_common.h"
#include "mainwindow.h"
#include "foldersel.h"
#include "manage_window.h"
#include "inc.h"
#include "utils.h"
#include "gtkutils.h"
#include "alertpanel.h"
#include "folder.h"
#include "filtering.h"
#include "addr_compl.h"
#include "colorlabel.h"

#include "matcher_parser.h"
#include "matcher.h"
#include "prefs_filtering_action.h"

enum {
	PREFS_FILTERING_NAME,
	PREFS_FILTERING_RULE,
	PREFS_FILTERING_PROP,
	N_PREFS_FILTERING_COLUMNS
};

static struct Filtering {
	GtkWidget *window;

	GtkWidget *ok_btn;
	GtkWidget *name_entry;
	GtkWidget *cond_entry;
	GtkWidget *action_entry;

	GtkWidget *cond_list_view;
} filtering;

static GSList ** p_processing_list = NULL;

/* widget creating functions */
static void prefs_filtering_create		(void);

static void prefs_filtering_set_dialog	(const gchar *header,
					 const gchar *key);
static void prefs_filtering_set_list	(void);

/* callback functions */
static void prefs_filtering_register_cb	(void);
static void prefs_filtering_substitute_cb	(void);
static void prefs_filtering_delete_cb	(void);
static void prefs_filtering_top		(void);
static void prefs_filtering_up		(void);
static void prefs_filtering_down	(void);
static void prefs_filtering_bottom	(void);
static gint prefs_filtering_deleted	(GtkWidget	*widget,
					 GdkEventAny	*event,
					 gpointer	 data);
static gboolean prefs_filtering_key_pressed(GtkWidget	*widget,
					 GdkEventKey	*event,
					 gpointer	 data);
static void prefs_filtering_cancel	(void);
static void prefs_filtering_ok		(void);

static void prefs_filtering_condition_define	(void);
static void prefs_filtering_action_define(void);
static gint prefs_filtering_list_view_set_row	(gint row, FilteringProp * prop);
					  
static void prefs_filtering_reset_dialog	(void);
static gboolean prefs_filtering_rename_path_func(GNode *node, gpointer data);
static gboolean prefs_filtering_delete_path_func(GNode *node, gpointer data);

static void delete_path(GSList ** p_filters, const gchar * path);


static GtkListStore* prefs_filtering_create_data_store	(void);
static gint prefs_filtering_list_view_insert_rule	(GtkListStore *list_store,
							 gint row,
							 const gchar *name, 
							 const gchar *rule, 
							 gboolean prop);
static gchar *prefs_filtering_list_view_get_rule	(GtkWidget *list, 
							 gint row);
static gchar *prefs_filtering_list_view_get_rule_name	(GtkWidget *list, 
							 gint row);

static GtkWidget *prefs_filtering_list_view_create	(void);
static void prefs_filtering_create_list_view_columns	(GtkWidget *list_view);
static gint prefs_filtering_get_selected_row		(GtkWidget *list_view);
static gboolean prefs_filtering_list_view_select_row	(GtkWidget *list, gint row);

static gboolean prefs_filtering_selected		(GtkTreeSelection *selector,
							 GtkTreeModel *model, 
							 GtkTreePath *path,
							 gboolean currently_selected,
							 gpointer data);

void prefs_filtering_open(GSList ** p_processing,
			  const gchar * title,
			  const gchar *header,
			  const gchar *key)
{
	if (prefs_rc_is_readonly(FILTERING_RC))
		return;

	inc_lock();

	if (!filtering.window) {
		prefs_filtering_create();
	}

	manage_window_set_transient(GTK_WINDOW(filtering.window));
	gtk_widget_grab_focus(filtering.ok_btn);
	
	if (title != NULL)
		gtk_window_set_title(GTK_WINDOW(filtering.window), title);
	else
		gtk_window_set_title (GTK_WINDOW(filtering.window),
				      _("Filtering/Processing configuration"));
	
        p_processing_list = p_processing;
        
	prefs_filtering_set_dialog(header, key);

	gtk_widget_show(filtering.window);

	start_address_completion();
}

static void prefs_filtering_size_allocate_cb(GtkWidget *widget,
					 GtkAllocation *allocation)
{
	g_return_if_fail(allocation != NULL);

	prefs_common.filteringwin_width = allocation->width;
	prefs_common.filteringwin_height = allocation->height;
}

/* prefs_filtering_close() - just to have one common exit point */
static void prefs_filtering_close(void)
{
	end_address_completion();
	
	gtk_widget_hide(filtering.window);
	inc_unlock();
}

static void prefs_filtering_create(void)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;
	GtkWidget *confirm_area;

	GtkWidget *vbox1;
	GtkWidget *reg_hbox;
	GtkWidget *arrow;
	GtkWidget *btn_hbox;

	GtkWidget *name_label;
	GtkWidget *name_entry;
	GtkWidget *cond_label;
	GtkWidget *cond_entry;
	GtkWidget *cond_btn;
	GtkWidget *action_label;
	GtkWidget *action_entry;
	GtkWidget *action_btn;

	GtkWidget *reg_btn;
	GtkWidget *subst_btn;
	GtkWidget *del_btn;

	GtkWidget *cond_hbox;
	GtkWidget *cond_scrolledwin;
	GtkWidget *cond_list_view;

	GtkWidget *btn_vbox;
	GtkWidget *spc_vbox;
	GtkWidget *top_btn;
	GtkWidget *up_btn;
	GtkWidget *down_btn;
	GtkWidget *bottom_btn;
	GtkWidget *table;
	static GdkGeometry geometry;

	debug_print("Creating filtering configuration window...\n");

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_container_set_border_width (GTK_CONTAINER (window), 8);
	gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal (GTK_WINDOW (window), TRUE);
	gtk_window_set_resizable(GTK_WINDOW (window), TRUE);

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (vbox);
	gtk_container_add (GTK_CONTAINER (window), vbox);

	gtkut_stock_button_set_create(&confirm_area, &ok_btn, GTK_STOCK_OK,
				      &cancel_btn, GTK_STOCK_CANCEL, NULL, NULL);
	gtk_widget_show (confirm_area);
	gtk_box_pack_end (GTK_BOX(vbox), confirm_area, FALSE, FALSE, 0);
	gtk_widget_grab_default (ok_btn);

	gtk_window_set_title (GTK_WINDOW(window),
			      	    _("Filtering/Processing configuration"));

	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(prefs_filtering_deleted), NULL);
	g_signal_connect(G_OBJECT(window), "size_allocate",
			 G_CALLBACK(prefs_filtering_size_allocate_cb), NULL);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(prefs_filtering_key_pressed), NULL);
	MANAGE_WINDOW_SIGNALS_CONNECT (window);
	g_signal_connect(G_OBJECT(ok_btn), "clicked",
			 G_CALLBACK(prefs_filtering_ok), NULL);
	g_signal_connect(G_OBJECT(cancel_btn), "clicked",
			 G_CALLBACK(prefs_filtering_cancel), NULL);

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_box_pack_start (GTK_BOX (vbox), vbox1, FALSE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), 2);

	table = gtk_table_new(3, 3, FALSE);
	gtk_widget_show(table);
	gtk_box_pack_start (GTK_BOX (vbox1), table, TRUE, TRUE, 0);
	

	name_label = gtk_label_new (_("Name: "));
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

	cond_label = gtk_label_new (_("Condition: "));
	gtk_widget_show (cond_label);
	gtk_misc_set_alignment (GTK_MISC (cond_label), 1, 0.5);
  	gtk_table_attach (GTK_TABLE (table), cond_label, 0, 1, 1, 2,
                    	  (GtkAttachOptions) (GTK_FILL),
                    	  (GtkAttachOptions) (0), 0, 0);

	cond_entry = gtk_entry_new ();
	gtk_widget_show (cond_entry);
  	gtk_table_attach (GTK_TABLE (table), cond_entry, 1, 2, 1, 2,
                    	  (GtkAttachOptions) (GTK_FILL|GTK_EXPAND),
                    	  (GtkAttachOptions) (0), 0, 0);

	cond_btn = gtk_button_new_with_label (_("Define ..."));
	gtk_widget_show (cond_btn);
  	gtk_table_attach (GTK_TABLE (table), cond_btn, 2, 3, 1, 2,
                    	  (GtkAttachOptions) (GTK_FILL),
                    	  (GtkAttachOptions) (0), 2, 2);
	g_signal_connect(G_OBJECT (cond_btn), "clicked",
			 G_CALLBACK(prefs_filtering_condition_define),
			 NULL);

	action_label = gtk_label_new (_("Action: "));
	gtk_widget_show (action_label);
	gtk_misc_set_alignment (GTK_MISC (action_label), 1, 0.5);
  	gtk_table_attach (GTK_TABLE (table), action_label, 0, 1, 2, 3,
                    	  (GtkAttachOptions) (GTK_FILL),
                    	  (GtkAttachOptions) (0), 0, 0);

	action_entry = gtk_entry_new ();
	gtk_widget_show (action_entry);
  	gtk_table_attach (GTK_TABLE (table), action_entry, 1, 2, 2, 3,
                    	  (GtkAttachOptions) (GTK_FILL|GTK_EXPAND),
                    	  (GtkAttachOptions) (0), 0, 0);

	action_btn = gtk_button_new_with_label (_("Define ..."));
	gtk_widget_show (action_btn);
  	gtk_table_attach (GTK_TABLE (table), action_btn, 2, 3, 2, 3,
                    	  (GtkAttachOptions) (GTK_FILL),
                    	  (GtkAttachOptions) (0), 2, 2);
	g_signal_connect(G_OBJECT (action_btn), "clicked",
			 G_CALLBACK(prefs_filtering_action_define),
			 NULL);
			 
	/* register / substitute / delete */
	reg_hbox = gtk_hbox_new (FALSE, 4);
	gtk_widget_show (reg_hbox);
	gtk_box_pack_start (GTK_BOX (vbox1), reg_hbox, FALSE, FALSE, 0);

	arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_OUT);
	gtk_widget_show (arrow);
	gtk_box_pack_start (GTK_BOX (reg_hbox), arrow, FALSE, FALSE, 0);
	gtk_widget_set_size_request (arrow, -1, 16);

	btn_hbox = gtk_hbox_new (TRUE, 4);
	gtk_widget_show (btn_hbox);
	gtk_box_pack_start (GTK_BOX (reg_hbox), btn_hbox, FALSE, FALSE, 0);

	reg_btn = gtk_button_new_from_stock (GTK_STOCK_ADD);
	gtk_widget_show (reg_btn);
	gtk_box_pack_start (GTK_BOX (btn_hbox), reg_btn, FALSE, TRUE, 0);
	g_signal_connect(G_OBJECT (reg_btn), "clicked",
			 G_CALLBACK(prefs_filtering_register_cb), NULL);

	subst_btn = gtk_button_new_with_label (_("  Replace  "));
	gtk_widget_show (subst_btn);
	gtk_box_pack_start (GTK_BOX (btn_hbox), subst_btn, FALSE, TRUE, 0);
	g_signal_connect(G_OBJECT (subst_btn), "clicked",
			 G_CALLBACK(prefs_filtering_substitute_cb),
			 NULL);

	del_btn = gtk_button_new_from_stock (GTK_STOCK_DELETE);
	gtk_widget_show (del_btn);
	gtk_box_pack_start (GTK_BOX (btn_hbox), del_btn, FALSE, TRUE, 0);
	g_signal_connect(G_OBJECT (del_btn), "clicked",
			G_CALLBACK(prefs_filtering_delete_cb), NULL);

	cond_hbox = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (cond_hbox);
	gtk_box_pack_start (GTK_BOX (vbox), cond_hbox, TRUE, TRUE, 0);

	cond_scrolledwin = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (cond_scrolledwin);
	gtk_widget_set_size_request (cond_scrolledwin, -1, 150);
	gtk_box_pack_start (GTK_BOX (cond_hbox), cond_scrolledwin,
			    TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (cond_scrolledwin),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);

	cond_list_view = prefs_filtering_list_view_create(); 	
	gtk_widget_show (cond_list_view);
	gtk_container_add (GTK_CONTAINER (cond_scrolledwin), cond_list_view);

	btn_vbox = gtk_vbox_new (FALSE, 8);
	gtk_widget_show (btn_vbox);
	gtk_box_pack_start (GTK_BOX (cond_hbox), btn_vbox, FALSE, FALSE, 0);

	top_btn = gtk_button_new_from_stock (GTK_STOCK_GOTO_TOP);
	gtk_widget_show (top_btn);
	gtk_box_pack_start (GTK_BOX (btn_vbox), top_btn, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT (top_btn), "clicked",
			 G_CALLBACK(prefs_filtering_top), NULL);

	PACK_VSPACER (btn_vbox, spc_vbox, VSPACING_NARROW_2);

	up_btn = gtk_button_new_from_stock (GTK_STOCK_GO_UP);
	gtk_widget_show (up_btn);
	gtk_box_pack_start (GTK_BOX (btn_vbox), up_btn, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT (up_btn), "clicked",
			 G_CALLBACK(prefs_filtering_up), NULL);

	down_btn = gtk_button_new_from_stock (GTK_STOCK_GO_DOWN);
	gtk_widget_show (down_btn);
	gtk_box_pack_start (GTK_BOX (btn_vbox), down_btn, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT (down_btn), "clicked",
			 G_CALLBACK(prefs_filtering_down), NULL);

	PACK_VSPACER (btn_vbox, spc_vbox, VSPACING_NARROW_2);

	bottom_btn = gtk_button_new_from_stock (GTK_STOCK_GOTO_BOTTOM);
	gtk_widget_show (bottom_btn);
	gtk_box_pack_start (GTK_BOX (btn_vbox), bottom_btn, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT (bottom_btn), "clicked",
			 G_CALLBACK(prefs_filtering_bottom), NULL);

	if (!geometry.min_height) {
		geometry.min_width = 500;
		geometry.min_height = 400;
	}

	gtk_window_set_geometry_hints(GTK_WINDOW(window), NULL, &geometry,
				      GDK_HINT_MIN_SIZE);
	gtk_widget_set_size_request(window, prefs_common.filteringwin_width,
				    prefs_common.filteringwin_height);

	gtk_widget_show_all(window);

	filtering.window    = window;
	filtering.ok_btn = ok_btn;

	filtering.name_entry     = name_entry;
	filtering.cond_entry     = cond_entry;
	filtering.action_entry   = action_entry;
	filtering.cond_list_view = cond_list_view;
}

static void rename_path(GSList * filters,
			const gchar * old_path, const gchar * new_path);

void prefs_filtering_rename_path(const gchar *old_path, const gchar *new_path)
{
	GList * cur;
	const gchar *paths[2] = {NULL, NULL};
	paths[0] = old_path;
	paths[1] = new_path;
	for (cur = folder_get_list() ; cur != NULL ; cur = g_list_next(cur)) {
		Folder *folder;
		folder = (Folder *) cur->data;
		g_node_traverse(folder->node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
				prefs_filtering_rename_path_func, paths);
	}
        
	rename_path(pre_global_processing, old_path, new_path);
	rename_path(post_global_processing, old_path, new_path);
	rename_path(filtering_rules, old_path, new_path);
        
	prefs_matcher_write_config();
}

static void rename_path(GSList * filters,
			const gchar * old_path, const gchar * new_path)
{
	gchar *base;
	gchar *prefix;
	gchar *suffix;
	gchar *dest_path;
	gchar *old_path_with_sep;
	gint destlen;
	gint prefixlen;
	gint oldpathlen;
        GSList * action_cur;
        GSList * cur;

	oldpathlen = strlen(old_path);
	old_path_with_sep = g_strconcat(old_path,G_DIR_SEPARATOR_S,NULL);

	for (cur = filters; cur != NULL; cur = cur->next) {
		FilteringProp   *filtering = (FilteringProp *)cur->data;
                
                for(action_cur = filtering->action_list ; action_cur != NULL ;
                    action_cur = action_cur->next) {

                        FilteringAction *action = action_cur->data;
                        
                        if (!action->destination) continue;
                        
                        destlen = strlen(action->destination);
                        
                        if (destlen > oldpathlen) {
                                prefixlen = destlen - oldpathlen;
                                suffix = action->destination + prefixlen;
                                
                                if (!strncmp(old_path, suffix, oldpathlen)) {
                                        prefix = g_malloc0(prefixlen + 1);
                                        strncpy2(prefix, action->destination, prefixlen);
                                        
                                        base = suffix + oldpathlen;
                                        while (*base == G_DIR_SEPARATOR) base++;
                                        if (*base == '\0')
                                                dest_path = g_strconcat(prefix,
                                                    G_DIR_SEPARATOR_S,
                                                    new_path, NULL);
                                        else
                                                dest_path = g_strconcat(prefix,
                                                    G_DIR_SEPARATOR_S,
                                                    new_path,
                                                    G_DIR_SEPARATOR_S,
                                                    base, NULL);
                                        
                                        g_free(prefix);
                                        g_free(action->destination);
                                        action->destination = dest_path;
                                } else { /* for non-leaf folders */
                                        /* compare with trailing slash */
                                        if (!strncmp(old_path_with_sep, action->destination, oldpathlen+1)) {
                                                
                                                suffix = action->destination + oldpathlen + 1;
                                                dest_path = g_strconcat(new_path,
                                                    G_DIR_SEPARATOR_S,
                                                    suffix, NULL);
                                                g_free(action->destination);
                                                action->destination = dest_path;
                                        }
                                }
                        } else {
                                /* folder-moving a leaf */
                                if (!strcmp(old_path, action->destination)) {
                                        dest_path = g_strdup(new_path);
                                        g_free(action->destination);
                                        action->destination = dest_path;
                                }
                        }
                }
        }
}

static gboolean prefs_filtering_rename_path_func(GNode *node, gpointer data)
{
	GSList *filters;
	const gchar * old_path;
        const gchar * new_path;
        const gchar ** paths;
	FolderItem *item;
        
        paths = data;
	old_path = paths[0];
	new_path = paths[1];

	g_return_val_if_fail(old_path != NULL, FALSE);
	g_return_val_if_fail(new_path != NULL, FALSE);
	g_return_val_if_fail(node != NULL, FALSE);

        item = node->data;
        if (!item || !item->prefs)
                return FALSE;
        filters = item->prefs->processing;

        rename_path(filters, old_path, new_path);

	return FALSE;
}

void prefs_filtering_delete_path(const gchar *path)
{
	GList * cur;
	for (cur = folder_get_list() ; cur != NULL ; cur = g_list_next(cur)) {
		Folder *folder;
		folder = (Folder *) cur->data;
		g_node_traverse(folder->node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
				prefs_filtering_delete_path_func, (gchar *)path);
	}
        delete_path(&pre_global_processing, path);
        delete_path(&post_global_processing, path);
        delete_path(&filtering_rules, path);
        
	prefs_matcher_write_config();
}

static void delete_path(GSList ** p_filters, const gchar * path)
{
        GSList * filters;
        GSList * duplist;
	gchar *suffix;
	gint destlen;
	gint prefixlen;
	gint pathlen;
        GSList * action_cur;
	GSList * cur;
        
	filters = *p_filters;
	pathlen = strlen(path);
	duplist = g_slist_copy(filters);
	for (cur = duplist ; cur != NULL; cur = g_slist_next(cur)) {
		FilteringProp *filtering = (FilteringProp *) cur->data;
                
                for(action_cur = filtering->action_list ; action_cur != NULL ;
                    action_cur = action_cur->next) {
                
                        FilteringAction *action;
                        
                        action = action_cur->data;
                        
                        if (!action->destination) continue;
                        
                        destlen = strlen(action->destination);
                        
                        if (destlen > pathlen) {
                                prefixlen = destlen - pathlen;
                                suffix = action->destination + prefixlen;
                                
                                if (suffix && !strncmp(path, suffix, pathlen)) {
                                        filteringprop_free(filtering);
                                        filters = g_slist_remove(filters, filtering);
                                }
                        } else if (strcmp(action->destination, path) == 0) {
                                filteringprop_free(filtering);
                                filters = g_slist_remove(filters, filtering);
                        }
                }
        }                
	g_slist_free(duplist);
        
        * p_filters = filters;
}

static gboolean prefs_filtering_delete_path_func(GNode *node, gpointer data)
{
	const gchar *path = data;
	FolderItem *item;
        GSList ** p_filters;
	
	g_return_val_if_fail(path != NULL, FALSE);
	g_return_val_if_fail(node != NULL, FALSE);

        item = node->data;
        if (!item || !item->prefs)
                return FALSE;
        p_filters = &item->prefs->processing;
        
        delete_path(p_filters, path);

	return FALSE;
}

static void prefs_filtering_set_dialog(const gchar *header, const gchar *key)
{
	GtkTreeView *list_view = GTK_TREE_VIEW(filtering.cond_list_view);
	GSList *cur;
	GSList * prefs_filtering;
	gchar *cond_str;
	GtkListStore *list_store;
	
	list_store = GTK_LIST_STORE(gtk_tree_view_get_model(list_view));
	gtk_list_store_clear(list_store);

	/* add the place holder (New) at row 0 */
	prefs_filtering_list_view_insert_rule(list_store, -1, 
					      _("(New)"),
					      _("(New)"),
					      FALSE);

        prefs_filtering = *p_processing_list;

	for(cur = prefs_filtering ; cur != NULL ; cur = g_slist_next(cur)) {
		FilteringProp * prop = (FilteringProp *) cur->data;

		cond_str = filteringprop_to_string(prop);
		subst_char(cond_str, '\t', ':');

		prefs_filtering_list_view_insert_rule(list_store, -1, 
						      prop->name,
						      cond_str, TRUE);
		
		g_free(cond_str);
	}

	prefs_filtering_reset_dialog();

	if (header && key) {
		gchar * quoted_key;
		gchar *match_str;

		quoted_key = matcher_quote_str(key);
		
		match_str = g_strconcat(header, " ", get_matchparser_tab_str(MATCHTYPE_MATCHCASE),
					" \"", quoted_key, "\"", NULL);
		g_free(quoted_key);
		
		gtk_entry_set_text(GTK_ENTRY(filtering.cond_entry), match_str);
		g_free(match_str);
	}
}

static void prefs_filtering_reset_dialog(void)
{
	gtk_entry_set_text(GTK_ENTRY(filtering.name_entry), "");
	gtk_entry_set_text(GTK_ENTRY(filtering.cond_entry), "");
	gtk_entry_set_text(GTK_ENTRY(filtering.action_entry), "");
}

static void prefs_filtering_set_list(void)
{
	gint row = 1;
	FilteringProp *prop;
	GSList * cur;
	gchar * filtering_str;
	GSList * prefs_filtering;

        prefs_filtering = *p_processing_list;
	for (cur = prefs_filtering ; cur != NULL ; cur = g_slist_next(cur))
		filteringprop_free((FilteringProp *) cur->data);
	g_slist_free(prefs_filtering);
	prefs_filtering = NULL;
	

	while (NULL != (filtering_str = prefs_filtering_list_view_get_rule
						(filtering.cond_list_view, row))) {
		/* FIXME: this strcmp() is bogus: "(New)" should never
		 * be inserted in the storage */
		if (strcmp(filtering_str, _("(New)")) != 0) {
			gchar *name = prefs_filtering_list_view_get_rule_name(filtering.cond_list_view, row);
			prop = matcher_parser_get_filtering(filtering_str);
			g_free(filtering_str);
			if (prop) {
				prop->name = name;
				prefs_filtering = 
					g_slist_append(prefs_filtering, prop);
			}
		}
		
		row++;
	}				
	
        *p_processing_list = prefs_filtering;
}

static gint prefs_filtering_list_view_set_row(gint row, FilteringProp * prop)
{
	GtkTreeView *list_view = GTK_TREE_VIEW(filtering.cond_list_view);
	gchar *str;
	GtkListStore *list_store;
	gchar *name = NULL;
	
	str = filteringprop_to_string(prop);
	if (str == NULL)
		return -1;
	
	if (prop && prop->name)
		name = prop->name;

	list_store = GTK_LIST_STORE(gtk_tree_view_get_model(list_view));

	row = prefs_filtering_list_view_insert_rule(list_store, row, name, str, prop != NULL);

	g_free(str);

	return row;
}

static void prefs_filtering_condition_define_done(MatcherList * matchers)
{
	gchar * str;

	if (matchers == NULL)
		return;

	str = matcherlist_to_string(matchers);

	if (str != NULL) {
		gtk_entry_set_text(GTK_ENTRY(filtering.cond_entry), str);
		g_free(str);
	}
}

static void prefs_filtering_condition_define(void)
{
	gchar * cond_str;
	MatcherList * matchers = NULL;

	cond_str = gtk_editable_get_chars(GTK_EDITABLE(filtering.cond_entry), 0, -1);

	if (*cond_str != '\0') {
		matchers = matcher_parser_get_cond(cond_str);
		if (matchers == NULL)
			alertpanel_error(_("Condition string is not valid."));
	}
	
	g_free(cond_str);

	prefs_matcher_open(matchers, prefs_filtering_condition_define_done);

	if (matchers != NULL)
		matcherlist_free(matchers);
}

static void prefs_filtering_action_define_done(GSList * action_list)
{
	gchar * str;

	if (action_list == NULL)
		return;

	str = filteringaction_list_to_string(action_list);

	if (str != NULL) {
		gtk_entry_set_text(GTK_ENTRY(filtering.action_entry), str);
		g_free(str);
	}
}

static void prefs_filtering_action_define(void)
{
	gchar * action_str;
	GSList * action_list = NULL;

	action_str = gtk_editable_get_chars(GTK_EDITABLE(filtering.action_entry), 0, -1);

	if (*action_str != '\0') {
		action_list = matcher_parser_get_action_list(action_str);
		if (action_list == NULL)
			alertpanel_error(_("Action string is not valid."));
	}
	
	g_free(action_str);

	prefs_filtering_action_open(action_list,
            prefs_filtering_action_define_done);

	if (action_list != NULL) {
                GSList * cur;
		for(cur = action_list ; cur != NULL ; cur = cur->next) {
                        filteringaction_free(cur->data);
                }
        }
}


/* register / substitute delete buttons */


static FilteringProp * prefs_filtering_dialog_to_filtering(gboolean alert)
{
	MatcherList * cond;
	gchar * name = NULL;
	gchar * cond_str = NULL;
	gchar * action_str = NULL;
	FilteringProp * prop = NULL;
	GSList * action_list;

	name = gtk_editable_get_chars(GTK_EDITABLE(filtering.name_entry), 0, -1);
	
	cond_str = gtk_editable_get_chars(GTK_EDITABLE(filtering.cond_entry), 0, -1);
	if (*cond_str == '\0') {
		if(alert == TRUE) alertpanel_error(_("Condition string is empty."));
		goto fail;
	}
	
	action_str = gtk_editable_get_chars(GTK_EDITABLE(filtering.action_entry), 0, -1);
	if (*action_str == '\0') {
		if(alert == TRUE) alertpanel_error(_("Action string is empty."));
		goto fail;
	}

	cond = matcher_parser_get_cond(cond_str);

	if (cond == NULL) {
		if(alert == TRUE) alertpanel_error(_("Condition string is not valid."));
		goto fail;
	}
        
        action_list = matcher_parser_get_action_list(action_str);
	

	if (action_list == NULL) {
		if(alert == TRUE) alertpanel_error(_("Action string is not valid."));
		goto fail;
	}

	prop = filteringprop_new(name, cond, action_list);

fail:
	g_free(name);
	g_free(cond_str);
	g_free(action_str);
	return prop;
}

static void prefs_filtering_register_cb(void)
{
	FilteringProp * prop;
	
	prop = prefs_filtering_dialog_to_filtering(TRUE);
	if (prop == NULL)
		return;
	prefs_filtering_list_view_set_row(-1, prop);
	
	filteringprop_free(prop);

	prefs_filtering_reset_dialog();
}

static void prefs_filtering_substitute_cb(void)
{
	gint selected_row = prefs_filtering_get_selected_row
		(filtering.cond_list_view);
	FilteringProp *prop;
	
	if (selected_row <= 0)
		return;

	prop = prefs_filtering_dialog_to_filtering(TRUE);
	if (prop == NULL) 
		return;
	prefs_filtering_list_view_set_row(selected_row, prop);

	filteringprop_free(prop);

	prefs_filtering_reset_dialog();
}

static void prefs_filtering_delete_cb(void)
{
	GtkTreeView *list_view = GTK_TREE_VIEW(filtering.cond_list_view);
	GtkTreeModel *model;
	GtkTreeIter iter;
	gint row;
	
	row = prefs_filtering_get_selected_row(filtering.cond_list_view);
	if (row <= 0) 
		return;	

	if (alertpanel(_("Delete rule"),
		       _("Do you really want to delete this rule?"),
		       GTK_STOCK_YES, GTK_STOCK_NO, NULL) == G_ALERTALTERNATE)
		return;

	model = gtk_tree_view_get_model(list_view);	
	if (!gtk_tree_model_iter_nth_child(model, &iter, NULL, row))
		return;

	gtk_list_store_remove(GTK_LIST_STORE(model), &iter);

	prefs_filtering_reset_dialog();
}

static void prefs_filtering_top(void)
{
	gint row;
	GtkTreeIter top, sel;
	GtkTreeModel *model;

	row = prefs_filtering_get_selected_row(filtering.cond_list_view);
	if (row <= 1) 
		return;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(filtering.cond_list_view));		
	
	if (!gtk_tree_model_iter_nth_child(model, &top, NULL, 0)
	||  !gtk_tree_model_iter_nth_child(model, &sel, NULL, row))
		return;

	gtk_list_store_move_after(GTK_LIST_STORE(model), &sel, &top);
	prefs_filtering_list_view_select_row(filtering.cond_list_view, 1);
}

static void prefs_filtering_up(void)
{
	gint row;
	GtkTreeIter top, sel;
	GtkTreeModel *model;

	row = prefs_filtering_get_selected_row(filtering.cond_list_view);
	if (row <= 1) 
		return;
		
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(filtering.cond_list_view));	

	if (!gtk_tree_model_iter_nth_child(model, &top, NULL, row - 1)
	||  !gtk_tree_model_iter_nth_child(model, &sel, NULL, row))
		return;

	gtk_list_store_swap(GTK_LIST_STORE(model), &top, &sel);
	prefs_filtering_list_view_select_row(filtering.cond_list_view, row - 1);
}

static void prefs_filtering_down(void)
{
	gint row, n_rows;
	GtkTreeIter top, sel;
	GtkTreeModel *model;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(filtering.cond_list_view));	
	n_rows = gtk_tree_model_iter_n_children(model, NULL);
	row = prefs_filtering_get_selected_row(filtering.cond_list_view);
	if (row < 1 || row >= n_rows - 1)
		return;

	if (!gtk_tree_model_iter_nth_child(model, &top, NULL, row)
	||  !gtk_tree_model_iter_nth_child(model, &sel, NULL, row + 1))
		return;
			
	gtk_list_store_swap(GTK_LIST_STORE(model), &top, &sel);
	prefs_filtering_list_view_select_row(filtering.cond_list_view, row + 1);
}

static void prefs_filtering_bottom(void)
{
	gint row, n_rows;
	GtkTreeIter top, sel;
	GtkTreeModel *model;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(filtering.cond_list_view));	
	n_rows = gtk_tree_model_iter_n_children(model, NULL);
	row = prefs_filtering_get_selected_row(filtering.cond_list_view);
	if (row < 1 || row >= n_rows - 1)
		return;

	if (!gtk_tree_model_iter_nth_child(model, &top, NULL, row)
	||  !gtk_tree_model_iter_nth_child(model, &sel, NULL, n_rows - 1))
		return;

	gtk_list_store_move_after(GTK_LIST_STORE(model), &top, &sel);		
	prefs_filtering_list_view_select_row(filtering.cond_list_view, n_rows - 1);
}

static void prefs_filtering_select_set(FilteringProp *prop)
{
	gchar *matcher_str;
        gchar *action_str;

	prefs_filtering_reset_dialog();

	matcher_str = matcherlist_to_string(prop->matchers);
	if (matcher_str == NULL) {
		return;
	}
	
	if (prop->name != NULL)
		gtk_entry_set_text(GTK_ENTRY(filtering.name_entry), prop->name);

	gtk_entry_set_text(GTK_ENTRY(filtering.cond_entry), matcher_str);

        action_str = filteringaction_list_to_string(prop->action_list);
	if (matcher_str == NULL) {
		return;
	}
	gtk_entry_set_text(GTK_ENTRY(filtering.action_entry), action_str);

	g_free(action_str);
	g_free(matcher_str);
}

static gint prefs_filtering_deleted(GtkWidget *widget, GdkEventAny *event,
				 gpointer data)
{
	prefs_filtering_cancel();
	return TRUE;
}

static gboolean prefs_filtering_key_pressed(GtkWidget *widget, GdkEventKey *event,
				     gpointer data)
{
	if (event && event->keyval == GDK_Escape) {
		prefs_filtering_cancel();
		return TRUE;			
	}
	return FALSE;
}

static void prefs_filtering_ok(void)
{
	FilteringProp * prop;
	gchar * str;
	gchar * filtering_str;
	gint row = 1;
        AlertValue val;
	
	prop = prefs_filtering_dialog_to_filtering(FALSE);
	if (prop != NULL) {
		str = filteringprop_to_string(prop);

		while (NULL != (filtering_str = (prefs_filtering_list_view_get_rule
							(filtering.cond_list_view,
							 row)))) {
			if (strcmp(filtering_str, str) == 0)
				break;
			row++;
			g_free(filtering_str);
		}	

		if (!filtering_str) {
			val = alertpanel(_("Entry not saved"),
				 _("The entry was not saved. Close anyway?"),
				 GTK_STOCK_YES, GTK_STOCK_NO, NULL);
			if (G_ALERTDEFAULT != val) {
				g_free(filtering_str);
				g_free(str); /* fixed two leaks: huzzah! */
				filteringprop_free(prop);
				return;
			}
		}		

		g_free(filtering_str);
		g_free(str);
		filteringprop_free(prop); /* fixed a leak: huzzah! */
	} else {
		gchar *name, *condition, *action;
		name = gtk_editable_get_chars(GTK_EDITABLE(filtering.name_entry), 0, -1);
		condition = gtk_editable_get_chars(GTK_EDITABLE(filtering.cond_entry), 0, -1);
		action = gtk_editable_get_chars(GTK_EDITABLE(filtering.action_entry), 0, -1);
		if (strlen(name) || 
		    strlen(condition) || 
		    strlen(action)) {
			val = alertpanel(_("Entry not saved"),
				 _("The entry was not saved. Close anyway?"),
				 GTK_STOCK_YES, GTK_STOCK_NO, NULL);
			if (G_ALERTDEFAULT != val) {
				g_free(name);
				g_free(condition);
				g_free(action);
				return;
			}
		}
		g_free(name);
		g_free(condition);
		g_free(action);
	}
	prefs_filtering_set_list();
	prefs_matcher_write_config();
	prefs_filtering_close();
}

static void prefs_filtering_cancel(void)
{
	prefs_matcher_read_config();
	prefs_filtering_close();
}

static GtkListStore* prefs_filtering_create_data_store(void)
{
	return gtk_list_store_new(N_PREFS_FILTERING_COLUMNS,
				  G_TYPE_STRING,
				  G_TYPE_STRING,
				  G_TYPE_BOOLEAN,
				 -1);
}

/*!
 *\brief	Insert filtering rule into store. Note that we access the
 *		tree view / store by index, which is a bit suboptimal, but
 *		at least it made GTK 2 porting easier.
 *
 *\param	list_store Store to operate on
 *\param	row -1 to add a new rule to store, else change an existing
 *		row
 *\param	rule String representation of rule
 *\param	prop TRUE if valid filtering rule; if FALSE it's the first
 *		entry in the store ("(New)").
 *
 *\return	int Row of inserted / changed rule.
 */
static gint prefs_filtering_list_view_insert_rule(GtkListStore *list_store,
						  gint row,
						  const gchar *name,
						  const gchar *rule,
						  gboolean prop) 
{
	GtkTreeIter iter;

	/* check if valid row at all */
	if (row >= 0) {
		if (!gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(list_store),
						   &iter, NULL, row))
			row = -1;						   
	}

	if (row < 0) {
		/* append new */
		gtk_list_store_append(list_store, &iter);
		gtk_list_store_set(list_store, &iter, 
				   PREFS_FILTERING_NAME, name,
				   PREFS_FILTERING_RULE, rule,
				   PREFS_FILTERING_PROP, prop,
				   -1);
		return gtk_tree_model_iter_n_children(GTK_TREE_MODEL(list_store),
						      NULL) - 1;
	} else {
		/* change existing */
		gtk_list_store_set(list_store, &iter, 
				   PREFS_FILTERING_NAME, name,
				   PREFS_FILTERING_RULE, rule,
				   -1);
		return row;				   
	}
}

/*!
 *\return	gchar * Rule at specified row - should be freed.
 */
static gchar *prefs_filtering_list_view_get_rule(GtkWidget *list, gint row)
{	
	GtkTreeView *list_view = GTK_TREE_VIEW(list);
	GtkTreeModel *model = gtk_tree_view_get_model(list_view);
	GtkTreeIter iter;
	gchar *result;

	if (!gtk_tree_model_iter_nth_child(model, &iter, NULL, row))
		return NULL;
	
	gtk_tree_model_get(model, &iter, 
			   PREFS_FILTERING_RULE, &result,
			   -1);
	
	return result;
}

static gchar *prefs_filtering_list_view_get_rule_name(GtkWidget *list, gint row)
{	
	GtkTreeView *list_view = GTK_TREE_VIEW(list);
	GtkTreeModel *model = gtk_tree_view_get_model(list_view);
	GtkTreeIter iter;
	gchar *result = NULL;

	if (!gtk_tree_model_iter_nth_child(model, &iter, NULL, row))
		return NULL;
	
	gtk_tree_model_get(model, &iter, 
			   PREFS_FILTERING_NAME, &result,
			   -1);
	
	return result;
}

/*!
 *\brief	Create list view for filtering
 */
static GtkWidget *prefs_filtering_list_view_create(void)
{
	GtkTreeView *list_view;
	GtkTreeSelection *selector;

	list_view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(GTK_TREE_MODEL
		(prefs_filtering_create_data_store())));
	
	gtk_tree_view_set_rules_hint(list_view, prefs_common.enable_rules_hint);
	gtk_tree_view_set_reorderable(list_view, TRUE);
	
	selector = gtk_tree_view_get_selection(list_view);
	gtk_tree_selection_set_mode(selector, GTK_SELECTION_BROWSE);
	gtk_tree_selection_set_select_function(selector, prefs_filtering_selected,
					       NULL, NULL);

	/* create the columns */
	prefs_filtering_create_list_view_columns(GTK_WIDGET(list_view));

	return GTK_WIDGET(list_view);
}

static void prefs_filtering_create_list_view_columns(GtkWidget *list_view)
{
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes
		(_("Name"),
		 renderer,
		 "text", PREFS_FILTERING_NAME,
		 NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(list_view), column);
	gtk_tree_view_column_set_resizable(column, TRUE);
		
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes
		(_("Rule"),
		 renderer,
		 "text", PREFS_FILTERING_RULE,
		 NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(list_view), column);		
}

static gboolean gtkut_tree_iter_comp(GtkTreeModel *model, 
				     GtkTreeIter *iter1, 
				     GtkTreeIter *iter2)
{
	GtkTreePath *path1 = gtk_tree_model_get_path(model, iter1);
	GtkTreePath *path2 = gtk_tree_model_get_path(model, iter2);
	gboolean result;

	result = gtk_tree_path_compare(path1, path2) == 0;

	gtk_tree_path_free(path1);
	gtk_tree_path_free(path2);
	
	return result;
}

/*!
 *\brief	Get selected row number.
 */
static gint prefs_filtering_get_selected_row(GtkWidget *list_view)
{
	GtkTreeView *view = GTK_TREE_VIEW(list_view);
	GtkTreeModel *model = gtk_tree_view_get_model(view);
	int n_rows = gtk_tree_model_iter_n_children(model, NULL);
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	int row;

	if (n_rows == 0) 
		return -1;
	
	selection = gtk_tree_view_get_selection(view);
	if (!gtk_tree_selection_get_selected(selection, &model, &iter))
		return -1;
	
	/* get all iterators and compare them... */
	for (row = 0; row < n_rows; row++) {
		GtkTreeIter itern;

		gtk_tree_model_iter_nth_child(model, &itern, NULL, row);
		if (gtkut_tree_iter_comp(model, &iter, &itern))
			return row;
	}
	
	return -1;
}

static gboolean prefs_filtering_list_view_select_row(GtkWidget *list, gint row)
{
	GtkTreeView *list_view = GTK_TREE_VIEW(list);
	GtkTreeSelection *selection = gtk_tree_view_get_selection(list_view);
	GtkTreeModel *model = gtk_tree_view_get_model(list_view);
	GtkTreeIter iter;
	GtkTreePath *path;

	if (!gtk_tree_model_iter_nth_child(model, &iter, NULL, row))
		return FALSE;
	
	gtk_tree_selection_select_iter(selection, &iter);

	path = gtk_tree_model_get_path(model, &iter);
	gtk_tree_view_set_cursor(list_view, path, NULL, FALSE);
	gtk_tree_path_free(path);
	
	return TRUE;
}

/*!
 *\brief	Triggered when a row is selected
 */
static gboolean prefs_filtering_selected(GtkTreeSelection *selector,
					 GtkTreeModel *model, 
					 GtkTreePath *path,
					 gboolean currently_selected,
					 gpointer data)
{
	if (currently_selected)
		return TRUE;
	else {		
		gboolean has_prop  = FALSE;
		GtkTreeIter iter;

		gtk_tree_model_get_iter(model, &iter, path);
		gtk_tree_model_get(model, &iter,
				   PREFS_FILTERING_PROP, &has_prop,
				   -1);

		if (has_prop) {
			FilteringProp *prop;
			gchar *filtering_str = NULL;
			gchar *name = NULL;

			gtk_tree_model_get(model, &iter,
					   PREFS_FILTERING_RULE, &filtering_str,
					   -1);
			gtk_tree_model_get(model, &iter,
					   PREFS_FILTERING_NAME, &name,
					   -1);

			prop = matcher_parser_get_filtering(filtering_str);
			if (prop) { 
				prop->name = g_strdup(name);
				prefs_filtering_select_set(prop);
				filteringprop_free(prop);
			}				
			g_free(name);
			g_free(filtering_str);
		} else
			prefs_filtering_reset_dialog();
	}		

	return TRUE;
}

