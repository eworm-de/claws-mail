/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2025 the Claws Mail team and Hiroyuki Yamamoto
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
#include "folder_item_prefs.h"
#include "filtering.h"
#include "addr_compl.h"
#include "manual.h"
#include "combobox.h"
#include "menu.h"
#include "account.h"

#include "matcher_parser.h"
#include "matcher.h"
#include "prefs_filtering_action.h"

enum {
	PREFS_FILTERING_ENABLED,
	PREFS_FILTERING_NAME,
	PREFS_FILTERING_ACCOUNT_ID,
	PREFS_FILTERING_ACCOUNT_NAME,
	PREFS_FILTERING_RULE,
	PREFS_FILTERING_PROP,
	N_PREFS_FILTERING_COLUMNS
};

struct _Filtering {
	GtkWidget *window;

	GtkWidget *help_btn;
	GtkWidget *ok_btn;
	GtkWidget *name_entry;
	GtkWidget *account_label;
	GtkWidget *account_combobox;
	GtkListStore *account_combobox_list;
	GtkWidget *cond_entry;
	GtkWidget *action_entry;

	GtkWidget *cond_list_view;

	GtkTreeViewColumn *account_name_column;
};

typedef struct _Filtering Filtering;

static Filtering  filtering;

static GSList ** p_processing_list = NULL;

/* widget creating functions */
static void prefs_filtering_create		(void);

static void prefs_filtering_set_dialog	(const gchar *header,
					 const gchar *key);
static void prefs_filtering_set_list	(void);

/* callback functions */
static gboolean prefs_filtering_search_func_cb (GtkTreeModel *model, gint column, 
						const gchar *key, GtkTreeIter *iter, 
						gpointer search_data);
static void prefs_filtering_register_cb	(gpointer action, gpointer data);
static void prefs_filtering_substitute_cb	(gpointer action, gpointer data);
static void prefs_filtering_delete_cb	(gpointer action, gpointer data);
static void prefs_filtering_delete_all_cb(gpointer action, gpointer data);
static void prefs_filtering_clear_cb(gpointer action, gpointer data);
static void prefs_filtering_duplicate_cb(gpointer action, gpointer data);
static void prefs_filtering_top		(gpointer action, gpointer data);
static void prefs_filtering_page_up	(gpointer action, gpointer data);
static void prefs_filtering_up		(gpointer action, gpointer data);
static void prefs_filtering_down	(gpointer action, gpointer data);
static void prefs_filtering_page_down	(gpointer action, gpointer data);
static void prefs_filtering_bottom	(gpointer action, gpointer data);
static gint prefs_filtering_deleted	(GtkWidget	*widget,
					 GdkEventAny	*event,
					 gpointer	 data);
static void prefs_filtering_row_selected(GtkTreeSelection *selection,
					 GtkTreeView *list_view);
static gboolean prefs_filtering_key_pressed(GtkWidget	*widget,
					 GdkEventKey	*event,
					 gpointer	 data);
static void prefs_filtering_cancel	(gpointer action, gpointer data);
static void prefs_filtering_ok		(gpointer action, gpointer data);

static void prefs_filtering_condition_define	(gpointer action, gpointer data);
static void prefs_filtering_action_define	(gpointer action, gpointer data);
static gint prefs_filtering_list_view_set_row	(gint row, FilteringProp * prop);
					  
static void prefs_filtering_reset_dialog	(void);
static gboolean prefs_filtering_rename_tag_func(GNode *node, gpointer data);
static gboolean prefs_filtering_rename_path_func(GNode *node, gpointer data);
static gboolean prefs_filtering_delete_path_func(GNode *node, gpointer data);

static void delete_path(GSList ** p_filters, const gchar * path);


static GtkListStore* prefs_filtering_create_data_store	(void);
static gint prefs_filtering_list_view_insert_rule	(GtkListStore *list_store,
							 gint row,
							 gboolean enabled,
							 const gchar *name,
							 gint account_id,
							 const gchar *account_name,
							 const gchar *rule, 
							 gboolean prop);
static gchar *prefs_filtering_list_view_get_rule	(GtkWidget *list, 
							 gint row);
static void prefs_filtering_list_view_get_rule_info	(GtkWidget *list, 
							 gint row,
							 gboolean *enabled,
							 gchar **name,
							 gint *account_id);

static GtkWidget *prefs_filtering_list_view_create	(void);
static void prefs_filtering_create_list_view_columns	(GtkWidget *list_view);

static void prefs_filtering_select_row(GtkTreeView *list_view, GtkTreePath *path);

static void prefs_filtering_account_option_menu_populate(void);

static gulong signal_id = 0; /* filtering.help_btn clicked signal */

static int modified = FALSE;

void prefs_filtering_open(GSList ** p_processing,
			  const gchar *title,
			  const gchar *help_url_anchor,
			  const gchar *header,
			  const gchar *key,
			  gboolean per_account_filtering)
{
	if (prefs_rc_is_readonly(FILTERING_RC))
		return;

	inc_lock();

	if (!filtering.window) {
		prefs_filtering_create();
	} else {
		gtk_list_store_clear(filtering.account_combobox_list);
		prefs_filtering_account_option_menu_populate();
	}

	gtk_tree_view_column_set_visible(filtering.account_name_column,
									 per_account_filtering);

	manage_window_set_transient(GTK_WINDOW(filtering.window));
	gtk_widget_grab_focus(filtering.ok_btn);
	
	if (title != NULL)
		gtk_window_set_title(GTK_WINDOW(filtering.window), title);
	else
		gtk_window_set_title (GTK_WINDOW(filtering.window),
				      _("Filtering/Processing configuration"));

	if (help_url_anchor != NULL) {
		if (signal_id != 0) {
			g_signal_handler_disconnect(
					G_OBJECT(filtering.help_btn),
					signal_id);
		}

		signal_id = g_signal_connect(G_OBJECT(filtering.help_btn),
				"clicked",
				G_CALLBACK(manual_open_with_anchor_cb),
				(gchar*)help_url_anchor);
	}
	else {
		gtk_widget_set_sensitive(filtering.help_btn, FALSE);
	}

        p_processing_list = p_processing;
        
	prefs_filtering_set_dialog(header, key);
	if (per_account_filtering) {
		gtk_widget_show(filtering.account_label);
		gtk_widget_show(filtering.account_combobox);
	} else {
		gtk_widget_hide(filtering.account_label);
		gtk_widget_hide(filtering.account_combobox);
		combobox_select_by_data(GTK_COMBO_BOX(filtering.account_combobox), 0);
	}

	gtk_widget_show(filtering.window);
	gtk_window_set_modal(GTK_WINDOW(filtering.window), TRUE);

	start_address_completion(NULL);
}

static void prefs_filtering_size_allocate_cb(GtkWidget *widget,
					 GtkAllocation *allocation)
{
	cm_return_if_fail(allocation != NULL);

	gtk_window_get_size(GTK_WINDOW(widget),
		&prefs_common.filteringwin_width, &prefs_common.filteringwin_height);
}

/* prefs_filtering_close() - just to have one common exit point */
static void prefs_filtering_close(void)
{
	GtkListStore *store;

	end_address_completion();

	store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW
				(filtering.cond_list_view)));
	gtk_list_store_clear(store);
	gtk_widget_hide(filtering.window);
	gtk_window_set_modal(GTK_WINDOW(filtering.window), FALSE);
	inc_unlock();
}

static void prefs_filtering_account_option_menu_populate(void)
{
	GList *accounts = NULL;
	GtkTreeIter iter;

	accounts = account_get_list();

	cm_return_if_fail(accounts != NULL);

	COMBOBOX_ADD(filtering.account_combobox_list, C_("Filtering Account Menu", "All"), 0);
	COMBOBOX_ADD(filtering.account_combobox_list, NULL, 0);
	for (; accounts != NULL; accounts = accounts->next) {
		PrefsAccount *ac = (PrefsAccount *)accounts->data;

		COMBOBOX_ADD_ESCAPED(filtering.account_combobox_list, ac->account_name, ac->account_id);
	}
}

static GtkWidget *prefs_filtering_account_option_menu(Filtering *filtering)
{
	GtkWidget *optmenu = NULL;
	GtkWidget *optmenubox = NULL;
	GtkListStore *menu = NULL;
	
	optmenubox = gtk_event_box_new();
	optmenu = gtkut_sc_combobox_create(optmenubox, FALSE);
	menu = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(optmenu)));

	filtering->account_combobox = optmenu;
	filtering->account_combobox_list = menu;

	prefs_filtering_account_option_menu_populate();

	return optmenubox;
}

static void prefs_filtering_create(void)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *help_btn;
	GtkWidget *cancel_btn;
	GtkWidget *ok_btn;
	GtkWidget *confirm_area;

	GtkWidget *vbox1;
	GtkWidget *reg_hbox;
	GtkWidget *arrow;
	GtkWidget *btn_hbox;

	GtkWidget *name_label;
	GtkWidget *name_entry;
	GtkWidget *account_label;
	GtkWidget *account_opt_menu;
	GtkWidget *cond_label;
	GtkWidget *cond_entry;
	GtkWidget *cond_btn;
	GtkWidget *action_label;
	GtkWidget *action_entry;
	GtkWidget *action_btn;

	GtkWidget *reg_btn;
	GtkWidget *subst_btn;
	GtkWidget *del_btn;
	GtkWidget *clear_btn;

	GtkWidget *cond_hbox;
	GtkWidget *cond_scrolledwin;
	GtkWidget *cond_list_view;

	GtkWidget *btn_vbox;
	GtkWidget *top_btn;
	GtkWidget *up_btn;
	GtkWidget *down_btn;
#ifndef GENERIC_UMPC
	GtkWidget *page_up_btn;
	GtkWidget *page_down_btn;
#endif
	GtkWidget *bottom_btn;
	GtkWidget *table;
	static GdkGeometry geometry;

	debug_print("Creating filtering configuration window...\n");

	window = gtkut_window_new(GTK_WINDOW_TOPLEVEL, "prefs_filtering");
	gtk_container_set_border_width (GTK_CONTAINER (window), 8);
	gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
	gtk_window_set_resizable(GTK_WINDOW (window), TRUE);
	gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DIALOG);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_show (vbox);
	gtk_container_add (GTK_CONTAINER (window), vbox);

	gtkut_stock_button_set_create_with_help(&confirm_area, &help_btn,
			&cancel_btn, NULL, _("_Cancel"),
			&ok_btn, NULL, _("_OK"),
			NULL, NULL, NULL);
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

	vbox1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, VSPACING);
	gtk_widget_show (vbox1);
	gtk_box_pack_start (GTK_BOX (vbox), vbox1, FALSE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), 2);

	table = gtk_grid_new();
	gtk_widget_show(table);
	gtk_grid_set_row_spacing(GTK_GRID(table), VSPACING_NARROW_2);
	gtk_grid_set_column_spacing(GTK_GRID(table), 4);
	gtk_box_pack_start (GTK_BOX (vbox1), table, TRUE, TRUE, 0);

	name_label = gtk_label_new (_("Name"));
	gtk_widget_show (name_label);
	gtk_label_set_xalign (GTK_LABEL (name_label), 1.0);
	gtk_grid_attach(GTK_GRID(table), name_label, 0, 0, 1, 1);

	name_entry = gtk_entry_new ();
	gtk_widget_show (name_entry);
	gtk_grid_attach(GTK_GRID(table), name_entry, 1, 0, 1, 1);
	gtk_widget_set_hexpand(name_entry, TRUE);
	gtk_widget_set_halign(name_entry, GTK_ALIGN_FILL);

	account_label = gtk_label_new (_("Account"));
	gtk_widget_show (account_label);
	gtk_label_set_xalign (GTK_LABEL (account_label), 1.0);
	gtk_grid_attach(GTK_GRID(table), account_label, 0, 1, 1, 1);

	account_opt_menu = prefs_filtering_account_option_menu(&filtering);
	gtk_widget_show (account_opt_menu);
	gtk_grid_attach(GTK_GRID(table), account_opt_menu, 1, 1, 1, 1);
	combobox_select_by_data(GTK_COMBO_BOX(filtering.account_combobox), 0);

	cond_label = gtk_label_new (_("Condition"));
	gtk_widget_show (cond_label);
	gtk_label_set_xalign (GTK_LABEL (cond_label), 1.0);
	gtk_grid_attach(GTK_GRID(table), cond_label, 0, 2, 1, 1);

	cond_entry = gtk_entry_new ();
	gtk_widget_show (cond_entry);
	gtk_grid_attach(GTK_GRID(table), cond_entry, 1, 2, 1, 1);
	gtk_widget_set_hexpand(cond_entry, TRUE);
	gtk_widget_set_halign(cond_entry, GTK_ALIGN_FILL);

	cond_btn =  gtk_button_new_with_mnemonic (_(" Def_ine... "));
	gtk_widget_show (cond_btn);
	gtk_grid_attach(GTK_GRID(table), cond_btn, 2, 2, 1, 1);
	g_signal_connect(G_OBJECT (cond_btn), "clicked",
			 G_CALLBACK(prefs_filtering_condition_define),
			 NULL);

	action_label = gtk_label_new (_("Action"));
	gtk_widget_show (action_label);
	gtk_label_set_xalign (GTK_LABEL (action_label), 1.0);
	gtk_grid_attach(GTK_GRID(table), action_label, 0, 3, 1, 1);

	action_entry = gtk_entry_new ();
	gtk_widget_show (action_entry);
	gtk_grid_attach(GTK_GRID(table), action_entry, 1, 3, 1, 1);
	gtk_widget_set_hexpand(action_entry, TRUE);
	gtk_widget_set_halign(action_entry, GTK_ALIGN_FILL);

	action_btn =  gtk_button_new_with_mnemonic (_(" De_fine... "));
	gtk_widget_show (action_btn);
	gtk_grid_attach(GTK_GRID(table), action_btn, 2, 3, 1, 1);
	g_signal_connect(G_OBJECT (action_btn), "clicked",
			 G_CALLBACK(prefs_filtering_action_define),
			 NULL);
			 
	/* register / substitute / delete */
	reg_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_widget_show (reg_hbox);
	gtk_box_pack_start (GTK_BOX (vbox1), reg_hbox, FALSE, FALSE, 0);

	arrow = gtk_image_new_from_icon_name("pan-down-symbolic", GTK_ICON_SIZE_MENU);
	gtk_widget_show (arrow);
	gtk_box_pack_start (GTK_BOX (reg_hbox), arrow, FALSE, FALSE, 0);
	gtk_widget_set_size_request (arrow, -1, 16);

	btn_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_widget_show (btn_hbox);
	gtk_box_pack_start (GTK_BOX (reg_hbox), btn_hbox, FALSE, FALSE, 0);

	reg_btn = gtkut_stock_button("list-add", _("_Add"));
	gtk_widget_show (reg_btn);
	gtk_box_pack_start (GTK_BOX (btn_hbox), reg_btn, FALSE, TRUE, 0);
	g_signal_connect(G_OBJECT (reg_btn), "clicked",
			 G_CALLBACK(prefs_filtering_register_cb), NULL);
	CLAWS_SET_TIP(reg_btn,
			_("Append the new rule above to the list"));

	subst_btn = gtkut_stock_button("edit-redo", _("_Replace"));
	gtk_widget_show (subst_btn);
	gtk_box_pack_start (GTK_BOX (btn_hbox), subst_btn, FALSE, TRUE, 0);
	g_signal_connect(G_OBJECT (subst_btn), "clicked",
			 G_CALLBACK(prefs_filtering_substitute_cb),
			 NULL);
	CLAWS_SET_TIP(subst_btn,
			_("Replace the selected rule in list with the rule above"));

	del_btn = gtkut_stock_button("edit-delete", _("D_elete"));
	gtk_box_pack_start (GTK_BOX (btn_hbox), del_btn, FALSE, TRUE, 0);
	g_signal_connect(G_OBJECT (del_btn), "clicked",
			G_CALLBACK(prefs_filtering_delete_cb), NULL);
	CLAWS_SET_TIP(del_btn,
			_("Delete the selected rule from the list"));

	clear_btn = gtkut_stock_button("edit-clear", _("C_lear"));
	gtk_widget_show (clear_btn);
	gtk_box_pack_start (GTK_BOX (btn_hbox), clear_btn, FALSE, TRUE, 0);
	g_signal_connect(G_OBJECT (clear_btn), "clicked",
			G_CALLBACK(prefs_filtering_clear_cb), NULL);
	CLAWS_SET_TIP(clear_btn,
			_("Clear all the input fields in the dialog"));

	cond_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (cond_hbox);
	gtk_box_pack_start (GTK_BOX (vbox), cond_hbox, TRUE, TRUE, 0);

	cond_scrolledwin = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (cond_scrolledwin);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(cond_scrolledwin),
					    GTK_SHADOW_ETCHED_IN);
	gtk_widget_set_size_request (cond_scrolledwin, -1, 150);
	gtk_box_pack_start (GTK_BOX (cond_hbox), cond_scrolledwin,
			    TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (cond_scrolledwin),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);

	cond_list_view = prefs_filtering_list_view_create(); 	
	gtk_widget_show (cond_list_view);
	gtk_container_add (GTK_CONTAINER (cond_scrolledwin), cond_list_view);

	btn_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_widget_show (btn_vbox);
	gtk_box_pack_start (GTK_BOX (cond_hbox), btn_vbox, FALSE, FALSE, 0);

	top_btn = gtkut_stock_button("go-top", _("_Top"));
	gtk_widget_show (top_btn);
	gtk_box_pack_start (GTK_BOX (btn_vbox), top_btn, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT (top_btn), "clicked",
			 G_CALLBACK(prefs_filtering_top), NULL);
	CLAWS_SET_TIP(top_btn,
			_("Move the selected rule to the top"));

#ifndef GENERIC_UMPC
	page_up_btn = gtk_button_new_with_mnemonic (_("Page u_p"));
	gtk_button_set_image(GTK_BUTTON(page_up_btn),
			gtk_image_new_from_icon_name("go-up-symbolic",GTK_ICON_SIZE_BUTTON));
	gtk_widget_show (page_up_btn);
	gtk_box_pack_start (GTK_BOX (btn_vbox), page_up_btn, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT (page_up_btn), "clicked",
			 G_CALLBACK(prefs_filtering_page_up), NULL);
	CLAWS_SET_TIP(page_up_btn,
			_("Move the selected rule one page up"));
#endif

	up_btn = gtkut_stock_button("go-up", _("_Up"));
	gtk_widget_show (up_btn);
	gtk_box_pack_start (GTK_BOX (btn_vbox), up_btn, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT (up_btn), "clicked",
			 G_CALLBACK(prefs_filtering_up), NULL);
	CLAWS_SET_TIP(up_btn,
			_("Move the selected rule up"));

	down_btn = gtkut_stock_button("go-down", _("_Down"));
	gtk_widget_show (down_btn);
	gtk_box_pack_start (GTK_BOX (btn_vbox), down_btn, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT (down_btn), "clicked",
			 G_CALLBACK(prefs_filtering_down), NULL);
	CLAWS_SET_TIP(down_btn,
			_("Move the selected rule down"));

#ifndef GENERIC_UMPC
	page_down_btn = gtk_button_new_with_mnemonic (_("Page dow_n"));
	gtk_button_set_image(GTK_BUTTON(page_down_btn),
			gtk_image_new_from_icon_name("go-down-symbolic",GTK_ICON_SIZE_BUTTON));
	gtk_widget_show (page_down_btn);
	gtk_box_pack_start (GTK_BOX (btn_vbox), page_down_btn, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT (page_down_btn), "clicked",
			 G_CALLBACK(prefs_filtering_page_down), NULL);
	CLAWS_SET_TIP(page_down_btn,
			_("Move the selected rule one page down"));
#endif

	bottom_btn = gtkut_stock_button("go-bottom", _("_Bottom"));
	gtk_widget_show (bottom_btn);
	gtk_box_pack_start (GTK_BOX (btn_vbox), bottom_btn, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT (bottom_btn), "clicked",
			 G_CALLBACK(prefs_filtering_bottom), NULL);
	CLAWS_SET_TIP(bottom_btn,
			_("Move the selected rule to the bottom"));

	if (!geometry.min_height) {
		geometry.min_width = 500;
		geometry.min_height = 460;
	}

	gtk_window_set_geometry_hints(GTK_WINDOW(window), NULL, &geometry,
				      GDK_HINT_MIN_SIZE);
	gtk_window_set_default_size(GTK_WINDOW(window), prefs_common.filteringwin_width,
				    prefs_common.filteringwin_height);

	gtk_widget_show_all(window);

	filtering.window    = window;
	filtering.help_btn = help_btn;
	filtering.ok_btn = ok_btn;

	filtering.name_entry     = name_entry;
	filtering.cond_entry     = cond_entry;
	filtering.action_entry   = action_entry;
	filtering.cond_list_view = cond_list_view;
	filtering.account_label  = account_label;
}

static void rename_tag(GSList * filters,
			const gchar * old_tag, const gchar * new_tag);

void prefs_filtering_rename_tag(const gchar *old_tag, const gchar *new_tag)
{
	GList * cur;
	const gchar *tags[2] = {NULL, NULL};
	tags[0] = old_tag;
	tags[1] = new_tag;
	for (cur = folder_get_list() ; cur != NULL ; cur = g_list_next(cur)) {
		Folder *folder;
		folder = (Folder *) cur->data;
		g_node_traverse(folder->node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
				prefs_filtering_rename_tag_func, tags);
	}
        
	rename_tag(pre_global_processing, old_tag, new_tag);
	rename_tag(post_global_processing, old_tag, new_tag);
	rename_tag(filtering_rules, old_tag, new_tag);
        
	prefs_matcher_write_config();
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
        GSList * cur;

	for (cur = filters; cur != NULL; cur = cur->next) {
		FilteringProp   *filtering = (FilteringProp *)cur->data;
		filtering_action_list_rename_path(filtering->action_list,
						  old_path, new_path);
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

	cm_return_val_if_fail(old_path != NULL, FALSE);
	cm_return_val_if_fail(new_path != NULL, FALSE);
	cm_return_val_if_fail(node != NULL, FALSE);

        item = node->data;
        if (!item || !item->prefs)
                return FALSE;
        filters = item->prefs->processing;

        rename_path(filters, old_path, new_path);

	return FALSE;
}

static void rename_tag(GSList * filters,
			const gchar * old_tag, const gchar * new_tag)
{
        GSList * action_cur;
        GSList * cur;

	for (cur = filters; cur != NULL; cur = cur->next) {
		FilteringProp   *filtering = (FilteringProp *)cur->data;
                
                for(action_cur = filtering->action_list ; action_cur != NULL ;
                    action_cur = action_cur->next) {

                        FilteringAction *action = action_cur->data;
                        
                        if (action->type != MATCHACTION_SET_TAG &&
			    action->type != MATCHACTION_UNSET_TAG)
				continue;
                        if (!action->destination)
				continue;
                        if (!strcmp(action->destination, old_tag)) {
				g_free(action->destination);
				action->destination = g_strdup(new_tag);
			}
                }
        }
}

static gboolean prefs_filtering_rename_tag_func(GNode *node, gpointer data)
{
	GSList *filters;
	const gchar * old_tag;
        const gchar * new_tag;
        const gchar ** tags;
	FolderItem *item;
        
        tags = data;
	old_tag = tags[0];
	new_tag = tags[1];

	cm_return_val_if_fail(old_tag != NULL, FALSE);
	cm_return_val_if_fail(new_tag != NULL, FALSE);
	cm_return_val_if_fail(node != NULL, FALSE);

        item = node->data;
        if (!item || !item->prefs)
                return FALSE;
        filters = item->prefs->processing;

        rename_tag(filters, old_tag, new_tag);

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
                        
                        if (action->type == MATCHACTION_SET_TAG ||
			    action->type == MATCHACTION_UNSET_TAG)
				continue;
                        if (!action->destination) 
				continue;
                        
                        destlen = strlen(action->destination);
                        
                        if (destlen > pathlen) {
                                prefixlen = destlen - pathlen;
                                suffix = action->destination + prefixlen;
                                
                                if ((suffix && !strncmp(path, suffix, pathlen)) && filtering) {
                                        filters = g_slist_remove(filters, filtering);
                                        filteringprop_free(filtering);
                                        filtering = NULL;
                                }
                        } else if ((strcmp(action->destination, path) == 0) && filtering) {
                                filters = g_slist_remove(filters, filtering);
                                filteringprop_free(filtering);
                                filtering = NULL;
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
	
	cm_return_val_if_fail(path != NULL, FALSE);
	cm_return_val_if_fail(node != NULL, FALSE);

        item = node->data;
        if (!item || !item->prefs)
                return FALSE;
        p_filters = &item->prefs->processing;
        
        delete_path(p_filters, path);

	return FALSE;
}

static void prefs_filtering_clear_list(GtkListStore *list_store)
{
	gtk_list_store_clear(list_store);

	/* add the place holder (New) at row 0 */
	prefs_filtering_list_view_insert_rule(list_store, -1, 
					      FALSE,
					      _("(New)"),
						  0,
					      _("(New)"),
					      _("(New)"),
					      FALSE);
}

static void prefs_filtering_set_dialog(const gchar *header, const gchar *key)
{
	GtkTreeView *list_view = GTK_TREE_VIEW(filtering.cond_list_view);
	GSList *cur;
	GSList * prefs_filtering;
	gchar *cond_str;
	GtkListStore *list_store;
	
	list_store = GTK_LIST_STORE(gtk_tree_view_get_model(list_view));
	prefs_filtering_clear_list(list_store);

        prefs_filtering = *p_processing_list;

	for(cur = prefs_filtering ; cur != NULL ; cur = g_slist_next(cur)) {
		FilteringProp * prop = (FilteringProp *) cur->data;
		gchar *account_name = NULL;

		if (prop->account_id > 0) {
			PrefsAccount *ac_prefs = account_find_from_id(prop->account_id);

			if (ac_prefs)
				account_name = ac_prefs->account_name;
		}
		if (account_name == NULL)
			account_name = (gchar *)C_("Filtering Account Menu", "All");

		cond_str = filteringprop_to_string(prop);
		subst_char(cond_str, '\t', ':');

		prefs_filtering_list_view_insert_rule(list_store, -1, 
						      prop->enabled,
						      prop->name,
							  prop->account_id,
							  account_name,
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
	combobox_select_by_data(GTK_COMBO_BOX(filtering.account_combobox), 0);
	gtk_entry_set_text(GTK_ENTRY(filtering.cond_entry), "");
	gtk_entry_set_text(GTK_ENTRY(filtering.action_entry), "");
}

static gboolean prefs_filtering_search_func_cb (GtkTreeModel *model, gint column, const gchar *key, 
						GtkTreeIter *iter, gpointer search_data) 
{
	gchar *store_string;
	gboolean retval;
	GtkTreePath *path;

	gtk_tree_model_get (model, iter, column, &store_string, -1);

	if (!store_string || !key) return FALSE;


	retval = (g_ascii_strncasecmp (key, store_string, strlen(key)) != 0);

	g_free(store_string);
	debug_print("selecting row\n");
	path = gtk_tree_model_get_path(model, iter);
	prefs_filtering_select_row(GTK_TREE_VIEW(filtering.cond_list_view), path);
	gtk_tree_path_free(path);

	return retval;
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
			gboolean enabled;
			gchar *name;
			gint account_id = 0;

			prefs_filtering_list_view_get_rule_info(
					filtering.cond_list_view, row,
					&enabled, &name, &account_id);
			prop = matcher_parser_get_filtering(filtering_str);
			if (prop) {
				prop->enabled = enabled;
				if (prop->name != NULL)
					g_free(prop->name);
				prop->name = name;
				prop->account_id = account_id;
				prefs_filtering = 
					g_slist_append(prefs_filtering, prop);
			}
		}

		g_free(filtering_str);
		row++;
	}				
	
        *p_processing_list = prefs_filtering;
}

static gint prefs_filtering_list_view_set_row(gint row, FilteringProp * prop)
{
	GtkTreeView *list_view = GTK_TREE_VIEW(filtering.cond_list_view);
	gchar *str = NULL;
	GtkListStore *list_store;
	gchar *name = NULL;
	gint account_id = 0;
	gchar *account_name = (gchar *)C_("Filtering Account Menu", "All");
	gboolean enabled = TRUE;

	if (prop)
		str = filteringprop_to_string(prop);
	if (str == NULL)
		return -1;

	if (prop) {
		if (prop->name)
			name = prop->name;
		account_id = prop->account_id;
		if (account_id > 0)
			account_name = account_find_from_id(account_id)->account_name;
		enabled = prop->enabled;
	}

	list_store = GTK_LIST_STORE(gtk_tree_view_get_model(list_view));

	row = prefs_filtering_list_view_insert_rule(list_store, row,
						    enabled,
						    name,
						    account_id,
							account_name,
						    str,
						    prop != NULL);

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

static void prefs_filtering_condition_define(gpointer action, gpointer data)
{
	gchar * cond_str;
	MatcherList * matchers = NULL;

	cond_str = gtk_editable_get_chars(GTK_EDITABLE(filtering.cond_entry), 0, -1);

	if (*cond_str != '\0') {
		matchers = matcher_parser_get_cond(cond_str, NULL);
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

static void prefs_filtering_action_define(gpointer action, gpointer data)
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
	gboolean enabled = TRUE;
	gchar * name = NULL;
	gint account_id = 0;
	gchar * cond_str = NULL;
	gchar * action_str = NULL;
	FilteringProp * prop = NULL;
	GSList * action_list;

	name = gtk_editable_get_chars(GTK_EDITABLE(filtering.name_entry), 0, -1);
	
	account_id = combobox_get_active_data(GTK_COMBO_BOX(filtering.account_combobox));

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

	cond = matcher_parser_get_cond(cond_str, NULL);

	if (cond == NULL) {
		if(alert == TRUE) alertpanel_error(_("Condition string is not valid."));
		goto fail;
	}
        
        action_list = matcher_parser_get_action_list(action_str);
	

	if (action_list == NULL) {
		if(alert == TRUE) alertpanel_error(_("Action string is not valid."));
		goto fail;
	}

	prop = filteringprop_new(enabled, name, account_id, cond, action_list);

fail:
	g_free(name);
	g_free(cond_str);
	g_free(action_str);
	return prop;
}

static void prefs_filtering_register_cb(gpointer action, gpointer data)
{
	FilteringProp *prop;
	
	prop = prefs_filtering_dialog_to_filtering(TRUE);
	if (prop == NULL)
		return;
	prefs_filtering_list_view_set_row(-1, prop);
	
	filteringprop_free(prop);

	prefs_filtering_reset_dialog();
	modified = TRUE;
}

static void prefs_filtering_substitute_cb(gpointer action, gpointer data)
{
	gint selected_row = gtkut_list_view_get_selected_row
		(filtering.cond_list_view);
	FilteringProp *prop;
	gboolean enabled;
	gchar *name;
	gint account_id;
	
	if (selected_row <= 0)
		return;

	prop = prefs_filtering_dialog_to_filtering(TRUE);

	if (prop == NULL) 
		return;

	/* prop->emabled is always TRUE here, re-use the value from the selected row 
	   as we don't substitute this value from dialog */
	prefs_filtering_list_view_get_rule_info(
			filtering.cond_list_view, selected_row,
			&enabled, &name, &account_id);
	g_free(name); /* We're not using this. */
	prop->enabled = enabled;

	prefs_filtering_list_view_set_row(selected_row, prop);

	filteringprop_free(prop);

	prefs_filtering_row_selected(gtk_tree_view_get_selection(
				GTK_TREE_VIEW(filtering.cond_list_view)),
				GTK_TREE_VIEW(filtering.cond_list_view));
	modified = TRUE;
}

static void prefs_filtering_delete_cb(gpointer action, gpointer data)
{
	GtkTreeView *list_view = GTK_TREE_VIEW(filtering.cond_list_view);
	GtkTreeModel *model;
	GtkTreeIter iter;
	gint selected_row;
	
	selected_row = gtkut_list_view_get_selected_row(filtering.cond_list_view);
	if (selected_row <= 0) 
		return;	

	if (alertpanel(_("Delete rule"),
		       _("Do you really want to delete this rule?"),
		       NULL, _("_Cancel"), "edit-delete", _("D_elete"), NULL, NULL,
		       ALERTFOCUS_SECOND) == G_ALERTDEFAULT)
		return;

	model = gtk_tree_view_get_model(list_view);	
	if (!gtk_tree_model_iter_nth_child(model, &iter, NULL, selected_row))
		return;

	gtk_list_store_remove(GTK_LIST_STORE(model), &iter);

	prefs_filtering_reset_dialog();
	modified = TRUE;
}

static void prefs_filtering_delete_all_cb(gpointer action, gpointer data)
{
	GtkListStore *list_store;
	
	if (alertpanel(_("Delete all rules"),
		       _("Do you really want to delete all the rules?"),
		       NULL, _("_Cancel"), "edit-delete", _("D_elete"), NULL, NULL,
		       ALERTFOCUS_SECOND) == G_ALERTDEFAULT)
		return;

	list_store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(filtering.cond_list_view)));
	prefs_filtering_clear_list(list_store);

	prefs_filtering_reset_dialog();
	modified = TRUE;
}

static void prefs_filtering_clear_cb(gpointer action, gpointer data)
{
	prefs_filtering_reset_dialog();
}

static void prefs_filtering_duplicate_cb(gpointer action, gpointer data)
{
	gint selected_row = gtkut_list_view_get_selected_row
		(filtering.cond_list_view);	
	FilteringProp *prop;
	gboolean enabled;
	gchar *name;
	gint account_id;

	if (selected_row <= 0) 
		return;	
	
	prop = prefs_filtering_dialog_to_filtering(TRUE);
	if (prop == NULL)
		return;

	/* prop->emabled is always TRUE here, re-use the value from the selected row 
	   as we don't substitute this value from dialog */
	prefs_filtering_list_view_get_rule_info(
			filtering.cond_list_view, selected_row,
			&enabled, &name, &account_id);
	g_free(name); /* We're not using this. */
	prop->enabled = enabled;

	prefs_filtering_list_view_set_row(-selected_row-2, prop);
	
	filteringprop_free(prop);

	prefs_filtering_reset_dialog();
	modified = TRUE;
}

static void prefs_filtering_top(gpointer action, gpointer data)
{
	gint row;
	GtkTreeIter top, sel;
	GtkTreeModel *model;

	row = gtkut_list_view_get_selected_row(filtering.cond_list_view);
	if (row <= 1) 
		return;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(filtering.cond_list_view));		
	
	if (!gtk_tree_model_iter_nth_child(model, &top, NULL, 0)
	||  !gtk_tree_model_iter_nth_child(model, &sel, NULL, row))
		return;

	gtk_list_store_move_after(GTK_LIST_STORE(model), &sel, &top);
	gtkut_list_view_select_row(filtering.cond_list_view, 1);
	modified = TRUE;
}

static void prefs_filtering_page_up(gpointer action, gpointer data)
{
	gint row, target_row;
	GtkTreeIter selected, target;
	GtkTreeModel *model;
	GtkTreePath *path;
	GdkRectangle cell_rect, view_rect;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(filtering.cond_list_view));	
	row = gtkut_list_view_get_selected_row(filtering.cond_list_view);
	if (row <= 1)
		return;

	if (!gtk_tree_model_iter_nth_child(model, &selected, NULL, row))
		return;

	/* compute number of rows per page (approximation) */
	path = gtk_tree_model_get_path(model, &selected);
	gtk_tree_view_get_cell_area(GTK_TREE_VIEW(filtering.cond_list_view), path, NULL, &cell_rect);
	gtk_tree_view_get_visible_rect(GTK_TREE_VIEW(filtering.cond_list_view), &view_rect);
	gtk_tree_path_free(path);
	target_row = row - (view_rect.height/cell_rect.height);
	if (target_row < 1)
		target_row = 1;

	if (!gtk_tree_model_iter_nth_child(model, &target, NULL, target_row))
		return;
	gtk_list_store_move_before(GTK_LIST_STORE(model), &selected, &target);
	gtkut_list_view_select_row(filtering.cond_list_view, target_row);
	modified = TRUE;
}
static void prefs_filtering_up(gpointer action, gpointer data)
{
	gint row;
	GtkTreeIter top, sel;
	GtkTreeModel *model;

	row = gtkut_list_view_get_selected_row(filtering.cond_list_view);
	if (row <= 1) 
		return;
		
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(filtering.cond_list_view));	

	if (!gtk_tree_model_iter_nth_child(model, &top, NULL, row - 1)
	||  !gtk_tree_model_iter_nth_child(model, &sel, NULL, row))
		return;

	gtk_list_store_swap(GTK_LIST_STORE(model), &top, &sel);
	gtkut_list_view_select_row(filtering.cond_list_view, row - 1);
	modified = TRUE;
}

static void prefs_filtering_down(gpointer action, gpointer data)
{
	gint row, n_rows;
	GtkTreeIter top, sel;
	GtkTreeModel *model;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(filtering.cond_list_view));	
	n_rows = gtk_tree_model_iter_n_children(model, NULL);
	row = gtkut_list_view_get_selected_row(filtering.cond_list_view);
	if (row < 1 || row >= n_rows - 1)
		return;

	if (!gtk_tree_model_iter_nth_child(model, &top, NULL, row)
	||  !gtk_tree_model_iter_nth_child(model, &sel, NULL, row + 1))
		return;
			
	gtk_list_store_swap(GTK_LIST_STORE(model), &top, &sel);
	gtkut_list_view_select_row(filtering.cond_list_view, row + 1);
	modified = TRUE;
}

static void prefs_filtering_page_down(gpointer action, gpointer data)
{
	gint row, target_row, n_rows;
	GtkTreeIter selected, target;
	GtkTreeModel *model;
	GtkTreePath *path;
	GdkRectangle cell_rect, view_rect;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(filtering.cond_list_view));	
	n_rows = gtk_tree_model_iter_n_children(model, NULL);
	row = gtkut_list_view_get_selected_row(filtering.cond_list_view);
	if (row < 1 || row >= n_rows -1)
		return;

	if (!gtk_tree_model_iter_nth_child(model, &selected, NULL, row))
		return;

	/* compute number of rows per page (approximation) */
	path = gtk_tree_model_get_path(model, &selected);
	gtk_tree_view_get_cell_area(GTK_TREE_VIEW(filtering.cond_list_view), path, NULL, &cell_rect);
	gtk_tree_view_get_visible_rect(GTK_TREE_VIEW(filtering.cond_list_view), &view_rect);
	gtk_tree_path_free(path);
	target_row = row + (view_rect.height/cell_rect.height);
	if (target_row > n_rows-1)
		target_row = n_rows-1;

	if (!gtk_tree_model_iter_nth_child(model, &target, NULL, target_row))
		return;
	gtk_list_store_move_after(GTK_LIST_STORE(model), &selected, &target);
	gtkut_list_view_select_row(filtering.cond_list_view, target_row);
	modified = TRUE;
}

static void prefs_filtering_bottom(gpointer action, gpointer data)
{
	gint row, n_rows;
	GtkTreeIter top, sel;
	GtkTreeModel *model;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(filtering.cond_list_view));	
	n_rows = gtk_tree_model_iter_n_children(model, NULL);
	row = gtkut_list_view_get_selected_row(filtering.cond_list_view);
	if (row < 1 || row >= n_rows - 1)
		return;

	if (!gtk_tree_model_iter_nth_child(model, &top, NULL, row)
	||  !gtk_tree_model_iter_nth_child(model, &sel, NULL, n_rows - 1))
		return;

	gtk_list_store_move_after(GTK_LIST_STORE(model), &top, &sel);		
	gtkut_list_view_select_row(filtering.cond_list_view, n_rows - 1);
	modified = TRUE;
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

	combobox_select_by_data(GTK_COMBO_BOX(filtering.account_combobox), prop->account_id);
		
	gtk_entry_set_text(GTK_ENTRY(filtering.cond_entry), matcher_str);

        action_str = filteringaction_list_to_string(prop->action_list);
	if (action_str != NULL)
		gtk_entry_set_text(GTK_ENTRY(filtering.action_entry), action_str);

	g_free(action_str);
	g_free(matcher_str);
}

static gint prefs_filtering_deleted(GtkWidget *widget, GdkEventAny *event,
				 gpointer data)
{
	prefs_filtering_cancel(NULL, NULL);
	return TRUE;
}

static gboolean prefs_filtering_key_pressed(GtkWidget *widget, GdkEventKey *event,
				     gpointer data)
{
	if (event && event->keyval == GDK_KEY_Escape) {
		prefs_filtering_cancel(NULL, NULL);
		return TRUE;			
	}
	return FALSE;
}

static gboolean prefs_filtering_check_mod(gboolean check_changed_list)
{
	FilteringProp * prop;
	gchar * str;
	gchar * filtering_str;
	gint row = 1;
	AlertValue val;
	
	prop = prefs_filtering_dialog_to_filtering(FALSE);
	
	if (check_changed_list) {
		if (modified && alertpanel(_("Filtering rules not saved"),
					 _("The list of filtering rules have been modified. Close anyway?"),
					 "window-close", _("_Close"), NULL, _("_Continue editing"),
					 NULL, NULL, ALERTFOCUS_SECOND) != G_ALERTDEFAULT) {
			if (prop != NULL)
				filteringprop_free(prop);
			return TRUE;
		}
	}
	
	/* check if a rule is being edited */
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
				 "window-close", _("_Close"), NULL, _("_Continue editing"),
				 NULL, NULL, ALERTFOCUS_SECOND);
			if (G_ALERTDEFAULT != val) {
				g_free(filtering_str);
				g_free(str); /* fixed two leaks: huzzah! */
				filteringprop_free(prop);
				return TRUE;
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
				 "window-close", _("_Close"), NULL, _("_Continue editing"),
				 NULL, NULL, ALERTFOCUS_SECOND);
			if (G_ALERTDEFAULT != val) {
				g_free(name);
				g_free(condition);
				g_free(action);
				return TRUE;
			}
		}
		g_free(name);
		g_free(condition);
		g_free(action);
	}
	return FALSE;
}

static void prefs_filtering_ok(gpointer action, gpointer data)
{
	if (prefs_filtering_check_mod(FALSE))
		return;
	modified = FALSE;
	prefs_filtering_set_list();
	prefs_matcher_write_config();
	prefs_filtering_close();
}

static void prefs_filtering_cancel(gpointer action, gpointer data)
{
	if (prefs_filtering_check_mod(TRUE))
		return;
	modified = FALSE;
	prefs_matcher_read_config();
	prefs_filtering_close();
}

static GtkListStore* prefs_filtering_create_data_store(void)
{
	return gtk_list_store_new(N_PREFS_FILTERING_COLUMNS,
				  G_TYPE_BOOLEAN,
				  G_TYPE_STRING,
				  G_TYPE_INT,
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
 *\param	row -1 to add a new rule to store,
			row >=0 to change an existing row
			row <-1 insert a new row after (-row-2)
 *\param	enabled TRUE if rule is enabled
 *\param	name The Name of rule
 *\param	account_id The account ID
 *\param	account_name The account name or All or (New)
 *\param	rule String representation of rule
 *\param	prop TRUE if valid filtering rule; if FALSE it's the first
 *		entry in the store ("(New)").
 *
 *\return	int Row of inserted / changed rule.
 */
static gint prefs_filtering_list_view_insert_rule(GtkListStore *list_store,
						  gint row,
						  gboolean enabled,
						  const gchar *name,
						  gint account_id,
						  const gchar *account_name,
						  const gchar *rule,
						  gboolean prop) 
{
	GtkTreeIter iter;
	GtkTreeIter sibling;

	/* check if valid row at all */
	if (row >= 0) {
		if (!gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(list_store),
						   &iter, NULL, row))
			row = -1;
	} else if (row < -1) {
		if (!gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(list_store),
						   &sibling, NULL, -row-2))
			row = -1;						   
	}

	if (row == -1 ) {
		/* append new */
		gtk_list_store_append(list_store, &iter);
		gtk_list_store_set(list_store, &iter, 
				   PREFS_FILTERING_ENABLED, enabled,
				   PREFS_FILTERING_NAME, name,
				   PREFS_FILTERING_ACCOUNT_ID, account_id,
				   PREFS_FILTERING_ACCOUNT_NAME, account_name,
				   PREFS_FILTERING_RULE, rule,
				   PREFS_FILTERING_PROP, prop,
				   -1);
		return gtk_tree_model_iter_n_children(GTK_TREE_MODEL(list_store),
						      NULL) - 1;
	} else if (row < -1) {
		/* duplicate */
		gtk_list_store_insert_after(list_store, &iter, &sibling);
		gtk_list_store_set(list_store, &iter, 
				   PREFS_FILTERING_ENABLED, enabled,
				   PREFS_FILTERING_NAME, name,
				   PREFS_FILTERING_ACCOUNT_ID, account_id,
				   PREFS_FILTERING_ACCOUNT_NAME, account_name,
				   PREFS_FILTERING_RULE, rule,
				   PREFS_FILTERING_PROP, prop,
				   -1);
		return gtk_tree_model_iter_n_children(GTK_TREE_MODEL(list_store),
						      NULL) - 1;
	} else {
		/* change existing */
		gtk_list_store_set(list_store, &iter, 
				   PREFS_FILTERING_ENABLED, enabled,
				   PREFS_FILTERING_NAME, name,
				   PREFS_FILTERING_ACCOUNT_ID, account_id,
				   PREFS_FILTERING_ACCOUNT_NAME, account_name,
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

static void prefs_filtering_list_view_get_rule_info(GtkWidget *list, gint row,
				gboolean *enabled, gchar **name, gint *account_id)
{	
	GtkTreeView *list_view = GTK_TREE_VIEW(list);
	GtkTreeModel *model = gtk_tree_view_get_model(list_view);
	GtkTreeIter iter;

	*enabled = TRUE;
	*name = NULL;

	if (gtk_tree_model_iter_nth_child(model, &iter, NULL, row)) {
		gtk_tree_model_get(model, &iter, 
				   PREFS_FILTERING_ENABLED, enabled,
				   PREFS_FILTERING_NAME, name,
				   PREFS_FILTERING_ACCOUNT_ID, account_id,
				   -1);
	}
}

static GtkActionGroup *prefs_filtering_popup_action = NULL;
static GtkWidget *prefs_filtering_popup_menu = NULL;

static GtkActionEntry prefs_filtering_popup_entries[] =
{
	{"PrefsFilteringPopup",			NULL, "PrefsFilteringPopup", NULL, NULL, NULL },
	{"PrefsFilteringPopup/Delete",		NULL, N_("_Delete"), NULL, NULL, G_CALLBACK(prefs_filtering_delete_cb) },
	{"PrefsFilteringPopup/DeleteAll",	NULL, N_("Delete _all"), NULL, NULL, G_CALLBACK(prefs_filtering_delete_all_cb) },
	{"PrefsFilteringPopup/Duplicate",	NULL, N_("D_uplicate"), NULL, NULL, G_CALLBACK(prefs_filtering_duplicate_cb) },
#ifdef GENERIC_UMPC
	{"PrefsFilteringPopup/---",		NULL, "---", NULL, NULL, NULL },
	{"PrefsFilteringPopup/PageUp",		NULL, N_("Move one page up"), NULL, NULL, G_CALLBACK(prefs_filtering_page_up) },
	{"PrefsFilteringPopup/PageDown",	NULL, N_("Move one page down"), NULL, NULL, G_CALLBACK(prefs_filtering_page_down) },
#endif
};

static void prefs_filtering_row_selected(GtkTreeSelection *selection,
					 GtkTreeView *list_view)
{
	GtkTreePath *path;
	GtkTreeIter iter;
	GtkTreeModel *model;
	
	if (!gtk_tree_selection_get_selected(selection, &model, &iter))
		return;
	
	path = gtk_tree_model_get_path(model, &iter);
	prefs_filtering_select_row(list_view, path);
	gtk_tree_path_free(path);
}

static gint prefs_filtering_list_btn_pressed(GtkWidget *widget, GdkEventButton *event,
				    GtkTreeView *list_view)
{
	if (event) {
		/* left- or right-button click */
		if (event->button == 1 || event->button == 3) {
			GtkTreePath *path = NULL;

			if (gtk_tree_view_get_path_at_pos( list_view, event->x, event->y,
								&path, NULL, NULL, NULL)) {
				prefs_filtering_select_row(list_view, path);
			}
			if (path)
				gtk_tree_path_free(path);
		}

		/* right-button click */
		if (event->button == 3) {
			GtkTreeModel *model = gtk_tree_view_get_model(list_view);
			GtkTreeIter iter;
			gboolean non_empty;
			gint row;

			if (!prefs_filtering_popup_menu) {
				prefs_filtering_popup_action = cm_menu_create_action_group("PrefsFilteringPopup", prefs_filtering_popup_entries,
					G_N_ELEMENTS(prefs_filtering_popup_entries), (gpointer)list_view);
				MENUITEM_ADDUI("/Menus", "PrefsFilteringPopup", "PrefsFilteringPopup", GTK_UI_MANAGER_MENU)
				MENUITEM_ADDUI("/Menus/PrefsFilteringPopup", "Delete", "PrefsFilteringPopup/Delete", GTK_UI_MANAGER_MENUITEM)
				MENUITEM_ADDUI("/Menus/PrefsFilteringPopup", "DeleteAll", "PrefsFilteringPopup/DeleteAll", GTK_UI_MANAGER_MENUITEM)
				MENUITEM_ADDUI("/Menus/PrefsFilteringPopup", "Duplicate", "PrefsFilteringPopup/Duplicate", GTK_UI_MANAGER_MENUITEM)
#ifdef GENERIC_UMPC
				MENUITEM_ADDUI("/Menus/PrefsFilteringPopup", "Separator1", "PrefsFilteringPopup/---", GTK_UI_MANAGER_SEPARATOR)
				MENUITEM_ADDUI("/Menus/PrefsFilteringPopup", "PageUp", "PrefsFilteringPopup/PageUp", GTK_UI_MANAGER_MENUITEM)
				MENUITEM_ADDUI("/Menus/PrefsFilteringPopup", "PageDown", "PrefsFilteringPopup/PageDown", GTK_UI_MANAGER_MENUITEM)
#endif
				prefs_filtering_popup_menu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(
					gtk_ui_manager_get_widget(gtkut_ui_manager(), "/Menus/PrefsFilteringPopup")) );
			}

			/* grey out some popup menu items if there is no selected row */
			row = gtkut_list_view_get_selected_row(GTK_WIDGET(list_view));
			cm_menu_set_sensitive("PrefsFilteringPopup/Delete", (row > 0));
			cm_menu_set_sensitive("PrefsFilteringPopup/Duplicate", (row > 0));

			/* grey out seom popup menu items if there is no row
			   (not counting the (New) one at row 0) */
			non_empty = gtk_tree_model_get_iter_first(model, &iter);
			if (non_empty)
				non_empty = gtk_tree_model_iter_next(model, &iter);
			cm_menu_set_sensitive("PrefsFilteringPopup/DeleteAll", non_empty);

			gtk_menu_popup_at_pointer(GTK_MENU(prefs_filtering_popup_menu), NULL);
		}
	}
	return FALSE;
}

static gboolean prefs_filtering_list_popup_menu(GtkWidget *widget, gpointer data)
{
	GtkTreeView *list_view = (GtkTreeView *)data;
	GdkEventButton event;
	
	event.button = 3;
	event.time = gtk_get_current_event_time();
	
	prefs_filtering_list_btn_pressed(NULL, &event, list_view);

	return TRUE;
}

/*!
 *\brief	Create list view for filtering
 */
static GtkWidget *prefs_filtering_list_view_create(void)
{
	GtkTreeView *list_view;
	GtkTreeSelection *selector;
	GtkListStore *store = prefs_filtering_create_data_store();

	list_view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(
				GTK_TREE_MODEL(store)));
	g_object_unref(store);
#ifdef GENERIC_UMPC
	g_object_set(list_view, "allow-checkbox-mode", FALSE, NULL);
#endif

	g_signal_connect(G_OBJECT(list_view), "popup-menu",
			 G_CALLBACK(prefs_filtering_list_popup_menu), list_view);
	g_signal_connect(G_OBJECT(list_view), "button-press-event",
			G_CALLBACK(prefs_filtering_list_btn_pressed), list_view);

	gtk_tree_view_set_reorderable(list_view, TRUE);
	
	selector = gtk_tree_view_get_selection(list_view);
	gtk_tree_selection_set_mode(selector, GTK_SELECTION_BROWSE);
	g_signal_connect(G_OBJECT(selector), "changed",
			 G_CALLBACK(prefs_filtering_row_selected), list_view);

	/* create the columns */
	prefs_filtering_create_list_view_columns(GTK_WIDGET(list_view));

	return GTK_WIDGET(list_view);
}

static void prefs_filtering_enable_toggled(GtkCellRendererToggle *widget,
		gchar *path,
		GtkWidget *list_view)
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(list_view));
	gboolean enabled = TRUE;

	if (!gtk_tree_model_get_iter_from_string(model, &iter, path))
		return;

	gtk_tree_model_get(model, &iter,
			   PREFS_FILTERING_ENABLED, &enabled,
			   -1);

	gtk_list_store_set(GTK_LIST_STORE(model), &iter,
			   PREFS_FILTERING_ENABLED, !enabled,
			   -1);
}

static void prefs_filtering_create_list_view_columns(GtkWidget *list_view)
{
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	renderer = gtk_cell_renderer_toggle_new();
	g_object_set(renderer,
		     "radio", FALSE,
		     "activatable", TRUE,
		     NULL);
	column = gtk_tree_view_column_new_with_attributes
		(_("Enable"), /* FIXME : Enable, Enabled, or 'E' ? */
		 renderer,
		 "active", PREFS_FILTERING_ENABLED,
		 NULL);
	gtk_tree_view_column_set_alignment (column, 0.5);
	gtk_tree_view_append_column(GTK_TREE_VIEW(list_view), column);
	g_signal_connect(G_OBJECT(renderer), "toggled",
			 G_CALLBACK(prefs_filtering_enable_toggled),
			 list_view);

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
		(_("Account"),
		 renderer,
		 "text", PREFS_FILTERING_ACCOUNT_NAME,
		 NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(list_view), column);
	gtk_tree_view_column_set_resizable(column, TRUE);

	filtering.account_name_column = column;
		
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes
		(_("Rule"),
		 renderer,
		 "text", PREFS_FILTERING_RULE,
		 NULL);

	gtk_tree_view_set_search_column(GTK_TREE_VIEW(list_view), PREFS_FILTERING_NAME);
	gtk_tree_view_set_search_equal_func(GTK_TREE_VIEW(list_view), prefs_filtering_search_func_cb , NULL, NULL);
	
	gtk_tree_view_append_column(GTK_TREE_VIEW(list_view), column);		
}

/*!
 *\brief	Triggered when a row has to be selected
 */
static void prefs_filtering_select_row(GtkTreeView *list_view, GtkTreePath *path)
{
	GtkTreeModel *model = gtk_tree_view_get_model(list_view);

	if (path && model) {
		GtkTreeSelection *selection;
		gboolean has_prop = FALSE;
		GtkTreeIter iter;

		/* select row */
		selection = gtk_tree_view_get_selection(list_view);
		gtk_tree_selection_select_path(selection, path);

		/* update dialog from selection */
		gtk_tree_model_get_iter(model, &iter, path);
		gtk_tree_model_get(model, &iter,
				   PREFS_FILTERING_PROP, &has_prop,
				   -1);

		if (has_prop) {
			FilteringProp *prop;
			gchar *filtering_str = NULL;
			gchar *name = NULL;
			gint account_id = 0;

			gtk_tree_model_get(model, &iter,
					   PREFS_FILTERING_RULE, &filtering_str,
					   PREFS_FILTERING_NAME, &name,
					   PREFS_FILTERING_ACCOUNT_ID, &account_id,
					   -1);

			prop = matcher_parser_get_filtering(filtering_str);
			if (prop) {
				if (prop->name != NULL)
					g_free(prop->name);
				prop->name = g_strdup(name);
				prop->account_id = account_id;
				prefs_filtering_select_set(prop);
				filteringprop_free(prop);
			}				
			g_free(name);
			g_free(filtering_str);
		} else
			prefs_filtering_reset_dialog();
	}
}
