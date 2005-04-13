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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <gtk/gtkoptionmenu.h>
#include <gdk/gdkkeysyms.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "intl.h"
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

static struct Filtering {
	GtkWidget *window;

	GtkWidget *ok_btn;
	GtkWidget *cond_entry;
	GtkWidget *action_entry;

	GtkWidget *cond_clist;
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
static void prefs_filtering_select	(GtkCList	*clist,
					 gint		 row,
					 gint		 column,
					 GdkEvent	*event);

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
static gint prefs_filtering_clist_set_row	(gint row, FilteringProp * prop);
					  
static void prefs_filtering_reset_dialog	(void);
static gboolean prefs_filtering_rename_path_func(GNode *node, gpointer data);
static gboolean prefs_filtering_delete_path_func(GNode *node, gpointer data);

static void delete_path(GSList ** p_filters, const gchar * path);

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
	GtkWidget *hbox1;
	GtkWidget *reg_hbox;
	GtkWidget *arrow;
	GtkWidget *btn_hbox;

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
	GtkWidget *cond_clist;

	GtkWidget *btn_vbox;
	GtkWidget *spc_vbox;
	GtkWidget *top_btn;
	GtkWidget *up_btn;
	GtkWidget *down_btn;
	GtkWidget *bottom_btn;

	gchar *title[1];

	debug_print("Creating filtering configuration window...\n");

	window = gtk_window_new (GTK_WINDOW_DIALOG);
	gtk_container_set_border_width (GTK_CONTAINER (window), 8);
	gtk_window_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal (GTK_WINDOW (window), TRUE);
	gtk_window_set_policy (GTK_WINDOW (window), FALSE, TRUE, FALSE);

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (vbox);
	gtk_container_add (GTK_CONTAINER (window), vbox);

	gtkut_button_set_create(&confirm_area, &ok_btn, _("OK"),
				&cancel_btn, _("Cancel"), NULL, NULL);
	gtk_widget_show (confirm_area);
	gtk_box_pack_end (GTK_BOX(vbox), confirm_area, FALSE, FALSE, 0);
	gtk_widget_grab_default (ok_btn);

	gtk_window_set_title (GTK_WINDOW(window),
			      	    _("Filtering/Processing configuration"));

	gtk_signal_connect (GTK_OBJECT(window), "delete_event",
			    GTK_SIGNAL_FUNC(prefs_filtering_deleted), NULL);
	gtk_signal_connect (GTK_OBJECT(window), "key_press_event",
			    GTK_SIGNAL_FUNC(prefs_filtering_key_pressed), NULL);
	MANAGE_WINDOW_SIGNALS_CONNECT (window);
	gtk_signal_connect (GTK_OBJECT(ok_btn), "clicked",
			    GTK_SIGNAL_FUNC(prefs_filtering_ok), NULL);
	gtk_signal_connect (GTK_OBJECT(cancel_btn), "clicked",
			    GTK_SIGNAL_FUNC(prefs_filtering_cancel), NULL);

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_box_pack_start (GTK_BOX (vbox), vbox1, TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), 2);

	cond_label = gtk_label_new (_("Condition"));
	gtk_widget_show (cond_label);
	gtk_misc_set_alignment (GTK_MISC (cond_label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (vbox1), cond_label, FALSE, FALSE, 0);

	hbox1 = gtk_hbox_new (FALSE, VSPACING);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), 2);

	cond_entry = gtk_entry_new ();
	gtk_widget_show (cond_entry);
	gtk_box_pack_start (GTK_BOX (hbox1), cond_entry, TRUE, TRUE, 0);

	cond_btn = gtk_button_new_with_label (_("Define ..."));
	gtk_widget_show (cond_btn);
	gtk_box_pack_start (GTK_BOX (hbox1), cond_btn, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (cond_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_filtering_condition_define),
			    NULL);

	action_label = gtk_label_new (_("Action"));
	gtk_widget_show (action_label);
	gtk_misc_set_alignment (GTK_MISC (action_label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (vbox1), action_label, FALSE, FALSE, 0);

	hbox1 = gtk_hbox_new (FALSE, VSPACING);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), 2);

	action_entry = gtk_entry_new ();
	gtk_widget_show (action_entry);
	gtk_box_pack_start (GTK_BOX (hbox1), action_entry, TRUE, TRUE, 0);

	action_btn = gtk_button_new_with_label (_("Define ..."));
	gtk_widget_show (action_btn);
	gtk_box_pack_start (GTK_BOX (hbox1), action_btn, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (action_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_filtering_action_define),
			    NULL);

	/* register / substitute / delete */

	reg_hbox = gtk_hbox_new (FALSE, 4);
	gtk_widget_show (reg_hbox);
	gtk_box_pack_start (GTK_BOX (vbox1), reg_hbox, FALSE, FALSE, 0);

	arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_OUT);
	gtk_widget_show (arrow);
	gtk_box_pack_start (GTK_BOX (reg_hbox), arrow, FALSE, FALSE, 0);
	gtk_widget_set_usize (arrow, -1, 16);

	btn_hbox = gtk_hbox_new (TRUE, 4);
	gtk_widget_show (btn_hbox);
	gtk_box_pack_start (GTK_BOX (reg_hbox), btn_hbox, FALSE, FALSE, 0);

	reg_btn = gtk_button_new_with_label (_("Add"));
	gtk_widget_show (reg_btn);
	gtk_box_pack_start (GTK_BOX (btn_hbox), reg_btn, FALSE, TRUE, 0);
	gtk_signal_connect (GTK_OBJECT (reg_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_filtering_register_cb), NULL);

	subst_btn = gtk_button_new_with_label (_("  Replace  "));
	gtk_widget_show (subst_btn);
	gtk_box_pack_start (GTK_BOX (btn_hbox), subst_btn, FALSE, TRUE, 0);
	gtk_signal_connect (GTK_OBJECT (subst_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_filtering_substitute_cb),
			    NULL);

	del_btn = gtk_button_new_with_label (_("Delete"));
	gtk_widget_show (del_btn);
	gtk_box_pack_start (GTK_BOX (btn_hbox), del_btn, FALSE, TRUE, 0);
	gtk_signal_connect (GTK_OBJECT (del_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_filtering_delete_cb), NULL);

	cond_hbox = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (cond_hbox);
	gtk_box_pack_start (GTK_BOX (vbox1), cond_hbox, TRUE, TRUE, 0);

	cond_scrolledwin = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (cond_scrolledwin);
	gtk_widget_set_usize (cond_scrolledwin, -1, 150);
	gtk_box_pack_start (GTK_BOX (cond_hbox), cond_scrolledwin,
			    TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (cond_scrolledwin),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);

	title[0] = _("Current filtering/processing rules");
	cond_clist = gtk_clist_new_with_titles(1, title);
	gtk_widget_show (cond_clist);
	gtk_container_add (GTK_CONTAINER (cond_scrolledwin), cond_clist);
	gtk_clist_set_column_width (GTK_CLIST (cond_clist), 0, 80);
	gtk_clist_set_selection_mode (GTK_CLIST (cond_clist),
				      GTK_SELECTION_BROWSE);
	GTK_WIDGET_UNSET_FLAGS (GTK_CLIST (cond_clist)->column[0].button,
				GTK_CAN_FOCUS);
	gtk_signal_connect (GTK_OBJECT (cond_clist), "select_row",
			    GTK_SIGNAL_FUNC (prefs_filtering_select), NULL);

	btn_vbox = gtk_vbox_new (FALSE, 8);
	gtk_widget_show (btn_vbox);
	gtk_box_pack_start (GTK_BOX (cond_hbox), btn_vbox, FALSE, FALSE, 0);

	top_btn = gtk_button_new_with_label (_("Top"));
	gtk_widget_show (top_btn);
	gtk_box_pack_start (GTK_BOX (btn_vbox), top_btn, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (top_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_filtering_top), NULL);

	PACK_VSPACER (btn_vbox, spc_vbox, VSPACING_NARROW_2);

	up_btn = gtk_button_new_with_label (_("Up"));
	gtk_widget_show (up_btn);
	gtk_box_pack_start (GTK_BOX (btn_vbox), up_btn, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (up_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_filtering_up), NULL);

	down_btn = gtk_button_new_with_label (_("Down"));
	gtk_widget_show (down_btn);
	gtk_box_pack_start (GTK_BOX (btn_vbox), down_btn, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (down_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_filtering_down), NULL);

	PACK_VSPACER (btn_vbox, spc_vbox, VSPACING_NARROW_2);

	bottom_btn = gtk_button_new_with_label (_("Bottom"));
	gtk_widget_show (bottom_btn);
	gtk_box_pack_start (GTK_BOX (btn_vbox), bottom_btn, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (bottom_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_filtering_bottom), NULL);

	gtk_widget_set_usize(window, 500, -1);

	gtk_widget_show_all(window);

	filtering.window    = window;
	filtering.ok_btn = ok_btn;

	filtering.cond_entry = cond_entry;
	filtering.action_entry = action_entry;
	filtering.cond_clist   = cond_clist;
}

static void prefs_filtering_update_hscrollbar(void)
{
	gint optwidth = gtk_clist_optimal_column_width(GTK_CLIST(filtering.cond_clist), 0);
	gtk_clist_set_column_width(GTK_CLIST(filtering.cond_clist), 0, optwidth);
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
	GtkCList *clist = GTK_CLIST(filtering.cond_clist);
	GSList *cur;
	GSList * prefs_filtering;
	gchar *cond_str[1];
	gint row;
	
	gtk_clist_freeze(clist);
	gtk_clist_clear(clist);

	cond_str[0] = _("(New)");
	row = gtk_clist_append(clist, cond_str);
	gtk_clist_set_row_data(clist, row, NULL);

        prefs_filtering = * p_processing_list;

	for(cur = prefs_filtering ; cur != NULL ; cur = g_slist_next(cur)) {
		FilteringProp * prop = (FilteringProp *) cur->data;

		cond_str[0] = filteringprop_to_string(prop);
		subst_char(cond_str[0], '\t', ':');
		row = gtk_clist_append(clist, cond_str);
		gtk_clist_set_row_data(clist, row, prop);

		g_free(cond_str[0]);
	}

	prefs_filtering_update_hscrollbar();
	gtk_clist_thaw(clist);

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

        prefs_filtering = * p_processing_list;

	for(cur = prefs_filtering ; cur != NULL ; cur = g_slist_next(cur))
		filteringprop_free((FilteringProp *) cur->data);
	g_slist_free(prefs_filtering);
	prefs_filtering = NULL;

	while (gtk_clist_get_text(GTK_CLIST(filtering.cond_clist),
				  row, 0, &filtering_str)) {
		if (strcmp(filtering_str, _("(New)")) != 0) {
			prop = matcher_parser_get_filtering(filtering_str);
			if (prop != NULL)
				prefs_filtering =
					g_slist_append(prefs_filtering, prop);
		}
		row++;
	}

        * p_processing_list = prefs_filtering;
}

static gint prefs_filtering_clist_set_row(gint row, FilteringProp * prop)
{
	GtkCList *clist = GTK_CLIST(filtering.cond_clist);
	gchar * str;
	gchar *cond_str[1];

	if (prop == NULL) {
		cond_str[0] = _("(New)");
		return gtk_clist_append(clist, cond_str);
	}

	str = filteringprop_to_string(prop);
	if (str == NULL) {
		return -1;
	}
	cond_str[0] = str;

	if (row < 0)
		row = gtk_clist_append(clist, cond_str);
	else
		gtk_clist_set_text(clist, row, 0, cond_str[0]);
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

	cond_str = gtk_entry_get_text(GTK_ENTRY(filtering.cond_entry));

	if (*cond_str != '\0') {
		matchers = matcher_parser_get_cond(cond_str);
		if (matchers == NULL)
			alertpanel_error(_("Condition string is not valid."));
	}

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

	action_str = gtk_entry_get_text(GTK_ENTRY(filtering.action_entry));

	if (*action_str != '\0') {
		action_list = matcher_parser_get_action_list(action_str);
		if (action_list == NULL)
			alertpanel_error(_("Action string is not valid."));
	}

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
	gchar * cond_str;
	gchar * action_str;
	FilteringProp * prop;
	GSList * action_list;

	cond_str = gtk_entry_get_text(GTK_ENTRY(filtering.cond_entry));
	if (*cond_str == '\0') {
		if(alert == TRUE) alertpanel_error(_("Condition string is empty."));
		return NULL;
	}

	action_str = gtk_entry_get_text(GTK_ENTRY(filtering.action_entry));
	if (*action_str == '\0') {
		if(alert == TRUE) alertpanel_error(_("Action string is empty."));
		return NULL;
	}

	cond = matcher_parser_get_cond(cond_str);

	if (cond == NULL) {
		if(alert == TRUE) alertpanel_error(_("Condition string is not valid."));
		return NULL;
	}
        
        action_list = matcher_parser_get_action_list(action_str);

	if (action_list == NULL) {
		if(alert == TRUE) alertpanel_error(_("Action string is not valid."));
		return NULL;
	}

	prop = filteringprop_new(cond, action_list);

	return prop;
}

static void prefs_filtering_register_cb(void)
{
	FilteringProp * prop;
	
	prop = prefs_filtering_dialog_to_filtering(TRUE);
	if (prop == NULL)
		return;
	prefs_filtering_clist_set_row(-1, prop);

	filteringprop_free(prop);
	
	prefs_filtering_update_hscrollbar();
}

static void prefs_filtering_substitute_cb(void)
{
	GtkCList *clist = GTK_CLIST(filtering.cond_clist);
	gint row;
	FilteringProp * prop;
	
	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	if (row == 0) return;

	prop = prefs_filtering_dialog_to_filtering(TRUE);
	if (prop == NULL)
		return;
	prefs_filtering_clist_set_row(row, prop);

	filteringprop_free(prop);
	
	prefs_filtering_update_hscrollbar();
}

static void prefs_filtering_delete_cb(void)
{
	GtkCList *clist = GTK_CLIST(filtering.cond_clist);
	gint row;

	if (!clist->selection) return;
	row = GPOINTER_TO_INT(clist->selection->data);
	if (row == 0) return;

	if (alertpanel(_("Delete rule"),
		       _("Do you really want to delete this rule?"),
		       _("Yes"), _("No"), NULL) == G_ALERTALTERNATE)
		return;

	gtk_clist_remove(clist, row);

	prefs_filtering_reset_dialog();

	prefs_filtering_update_hscrollbar();
}

static void prefs_filtering_top(void)
{
	GtkCList *clist = GTK_CLIST(filtering.cond_clist);
	gint row;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	if (row > 1)
		gtk_clist_row_move(clist, row, 1);
}

static void prefs_filtering_up(void)
{
	GtkCList *clist = GTK_CLIST(filtering.cond_clist);
	gint row;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	if (row > 1) {
		gtk_clist_row_move(clist, row, row - 1);
		if (gtk_clist_row_is_visible(clist, row - 1) != GTK_VISIBILITY_FULL) 
			gtk_clist_moveto(clist, row - 1, 0, 0, 0); 
	}
}

static void prefs_filtering_down(void)
{
	GtkCList *clist = GTK_CLIST(filtering.cond_clist);
	gint row;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	if (row > 0 && row < clist->rows - 1) {
		gtk_clist_row_move(clist, row, row + 1);
		if (gtk_clist_row_is_visible(clist, row + 1) != GTK_VISIBILITY_FULL)
			gtk_clist_moveto(clist, row + 1, 0, 1, 0); 
	}
}

static void prefs_filtering_bottom(void)
{
	GtkCList *clist = GTK_CLIST(filtering.cond_clist);
	gint row;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	if (row > 0 && row < clist->rows - 1)
		gtk_clist_row_move(clist, row, clist->rows - 1);
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

	gtk_entry_set_text(GTK_ENTRY(filtering.cond_entry), matcher_str);

        action_str = filteringaction_list_to_string(prop->action_list);
	if (matcher_str == NULL) {
		return;
	}
	gtk_entry_set_text(GTK_ENTRY(filtering.action_entry), action_str);

	g_free(action_str);
	g_free(matcher_str);
}

static void prefs_filtering_select(GtkCList *clist, gint row, gint column,
				GdkEvent *event)
{
	FilteringProp * prop;
	gchar * filtering_str;

	if (row == 0) {
		prefs_filtering_reset_dialog();
		return;
	}

        if (!gtk_clist_get_text(GTK_CLIST(filtering.cond_clist),
				row, 0, &filtering_str))
		return;
	
	prop = matcher_parser_get_filtering(filtering_str);
	if (prop == NULL)
		return;

	prefs_filtering_select_set(prop);

	filteringprop_free(prop);
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
	if (event && event->keyval == GDK_Escape)
		prefs_filtering_cancel();
	return TRUE;			
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

		while (gtk_clist_get_text(GTK_CLIST(filtering.cond_clist),
					  row, 0, &filtering_str)) {
			if (strcmp(filtering_str, str) == 0) break;
			row++;
		}
		if (!filtering_str || strcmp(filtering_str, str) != 0) {
			val = alertpanel(_("Entry not saved"),
				 _("The entry was not saved. Close anyway?"),
				 _("Yes"), _("No"), NULL);
			if (G_ALERTDEFAULT != val) {
				g_free(str);
				filteringprop_free(prop);
				return;
			}
		}
		g_free(str);
		filteringprop_free(prop);
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
