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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * General functions for accessing address book files.
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

#include "intl.h"
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

typedef enum
{
	COL_ICON	= 0,
	COL_FILENAME	= 1,
	COL_TEXT	= 2,
	COL_EVENT	= 3
} DisplayedItemsColumnPos;

# define N_DISPLAYED_ITEMS_COLS	4

typedef struct _ToolbarPage
{
	PrefsPage  page;

	GtkWidget *window;		/* do not modify */

	GtkWidget *clist_icons;
	GtkWidget *clist_set;
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

#define CELL_SPACING 24
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

static void prefs_toolbar_select_row_set         (GtkCList *clist, 
						  gint row, 
						  gint column,
						  GdkEvent *event, 
						  ToolbarPage *prefs_toolbar);

static void prefs_toolbar_select_row_icons       (GtkCList *clist, 
						  gint row, 
						  gint column,
						  GdkEvent *event, 
						  ToolbarPage *prefs_toolbar);

static void prefs_toolbar_create                 (ToolbarPage *prefs_toolbar);

static void prefs_toolbar_selection_changed      (GtkList *list, 
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
	gint row = 0;
	GtkCList *clist = GTK_CLIST(prefs_toolbar->clist_set);
	gchar *entry = NULL;
	ToolbarItem *item;

	toolbar_clear_list(prefs_toolbar->source);

	if (clist->rows == 0) {
		toolbar_set_default(prefs_toolbar->source);
	}
	else {
		do {
			item = g_new0(ToolbarItem, 1);
			
			gtk_clist_get_text(clist, row, 1, &entry);
			item->file = g_strdup(entry);
			
			gtk_clist_get_text(clist, row, 2, &entry);
			item->text = g_strdup(entry);
			
			gtk_clist_get_text(clist, row, 3, &entry);	
			item->index = toolbar_ret_val_from_descr(entry);

			/* TODO: save A_SYL_ACTIONS only if they are still active */
			toolbar_set_list_item(item, prefs_toolbar->source);

			g_free(item->file);
			g_free(item->text);
			g_free(item);
			row++;
			
		} while(gtk_clist_get_text(clist, row, 3, &entry));
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
	GdkPixmap *xpm;
	GdkBitmap *xpmmask;
	gchar *activ[4];
	GSList *cur;
	GtkCList *clist_set = GTK_CLIST(prefs_toolbar->clist_set);
	GSList *toolbar_list = toolbar_get_list(prefs_toolbar->source);

	gtk_clist_clear(clist_set);
	gtk_clist_freeze(clist_set);

	/* set currently active toolbar entries */
	for (cur = toolbar_list; cur != NULL; cur = cur->next) {
		ToolbarItem *item = (ToolbarItem*) cur->data;
	
		if (item->index != A_SEPARATOR) {
			gint row_num;
			StockPixmap icon = stock_pixmap_get_icon(item->file);
			
			stock_pixmap_gdk(prefs_toolbar->window, icon,
					  &xpm, &xpmmask);
			activ[0] = g_strdup("");
			activ[1] = g_strdup(item->file);
			activ[2] = g_strdup(item->text);
			activ[3] = g_strdup(toolbar_ret_descr_from_val(item->index));
			row_num  = gtk_clist_append(clist_set, activ);
			gtk_clist_set_pixmap(clist_set, 
					      row_num, 0, xpm, xpmmask);

		} else {
			activ[0] = g_strdup(SEPARATOR_PIXMAP);
			activ[1] = g_strdup(toolbar_ret_descr_from_val(A_SEPARATOR));
			activ[2] = g_strdup("");
			activ[3] = g_strdup(toolbar_ret_descr_from_val(A_SEPARATOR));
			gtk_clist_append(clist_set, activ);
		}

		g_free(activ[0]);
		g_free(activ[1]);
		g_free(activ[2]);
		g_free(activ[3]);
	}

	gtk_clist_thaw(clist_set);
	gtk_clist_columns_autosize(clist_set);
	gtk_clist_set_row_height(clist_set, CELL_SPACING);
	gtk_clist_select_row(clist_set, 0, 0);
}

static void prefs_toolbar_populate(ToolbarPage *prefs_toolbar)
{
	gint i;
	GSList *cur;
	GList *syl_actions = NULL;
	GtkCList *clist_icons = GTK_CLIST(prefs_toolbar->clist_icons);
	GdkPixmap *xpm;
	GdkBitmap *xpmmask;
	gchar *avail[2];
	gchar *act;
	
	gtk_clist_clear(clist_icons);
	gtk_clist_freeze(clist_icons);

	/* set available icons */
	avail[0] = g_strdup(SEPARATOR_PIXMAP);
	avail[1] = g_strdup(toolbar_ret_descr_from_val(A_SEPARATOR));
	gtk_clist_append(clist_icons, avail);
	g_free(avail[0]);
	g_free(avail[1]);

	prefs_toolbar->combo_action_list = toolbar_get_action_items(prefs_toolbar->source);
	gtk_combo_set_popdown_strings(GTK_COMBO(prefs_toolbar->combo_action), prefs_toolbar->combo_action_list);
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

		avail[0] = g_strdup("");
		avail[1] = g_strdup(stock_pixmap_get_name((StockPixmap)i));

		stock_pixmap_gdk(prefs_toolbar->window, i,
				  &xpm, &xpmmask);
		gtk_clist_append(clist_icons, avail);
		gtk_clist_set_pixmap(clist_icons, 
				       i + 1, 0, xpm, xpmmask);
		
		g_free(avail[0]);
		g_free(avail[1]);
 	}

	gtk_clist_thaw(clist_icons);
	gtk_clist_columns_autosize(clist_icons);
	gtk_clist_set_row_height(clist_icons, CELL_SPACING);
	gtk_clist_select_row(clist_icons, 0, 0);

	prefs_toolbar_set_displayed(prefs_toolbar);

	toolbar_clear_list(prefs_toolbar->source);
}

static gboolean is_duplicate(ToolbarPage *prefs_toolbar, gchar *chosen_action)
{
	GtkCList *clist = GTK_CLIST(prefs_toolbar->clist_set);
	gchar *entry;
	gint row = 0;
	gchar *syl_act = toolbar_ret_descr_from_val(A_SYL_ACTIONS);

	g_return_val_if_fail(chosen_action != NULL, TRUE);
	if (clist->rows == 0) 
		return FALSE;
	
	/* allow duplicate entries (A_SYL_ACTIONS) */
	if (g_strcasecmp(syl_act, chosen_action) == 0)
		return FALSE;

	do {
		gtk_clist_get_text(clist, row, 3, &entry);
		if ( g_strcasecmp(chosen_action, entry) == 0)
			return TRUE;
		row++;
	} while ((gtk_clist_get_text(clist, row, 3, &entry)) && (row <= clist->rows));
	
	return FALSE;
}

static void prefs_toolbar_default(GtkButton *button, ToolbarPage *prefs_toolbar)
{
	toolbar_clear_list(prefs_toolbar->source);
	toolbar_set_default(prefs_toolbar->source);
	prefs_toolbar_set_displayed(prefs_toolbar);
}

static void get_action_name(gchar *entry, gchar **menu)
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
	GtkCList *clist_set   = GTK_CLIST(prefs_toolbar->clist_set);
	GtkCList *clist_icons = GTK_CLIST(prefs_toolbar->clist_icons);
	gchar *syl_act = toolbar_ret_descr_from_val(A_SYL_ACTIONS);
	gint row_icons = 0;
	gint row_set = 0;
	GdkPixmap *xpm;
	GdkBitmap *xpmmask;
	gchar *item[4] = {NULL, NULL, NULL, NULL};

	if (clist_icons->rows == 0) return; 

	if (clist_icons->selection) {
		if (clist_icons->selection->data) 
			row_icons = GPOINTER_TO_INT(clist_icons->selection->data);
	} else
		return;

	gtk_clist_get_text(clist_icons, row_icons, 1, &item[1]);
	item[3] = g_strdup(gtk_entry_get_text(GTK_ENTRY(prefs_toolbar->combo_entry)));
	
	/* SEPARATOR or other ? */
	if (g_strcasecmp(item[1], toolbar_ret_descr_from_val(A_SEPARATOR)) == 0) {
		item[0] = g_strdup(SEPARATOR_PIXMAP);
		item[2] = g_strdup("");
		item[3] = g_strdup(toolbar_ret_descr_from_val(A_SEPARATOR));
		
		row_set = gtk_clist_append(GTK_CLIST(prefs_toolbar->clist_set), item);

		g_free(item[0]);
	} else {

		if (is_duplicate(prefs_toolbar, item[3])) {
			alertpanel_error(ERROR_MSG);
			g_free(item[3]);
			return;
		}

		stock_pixmap_gdk(prefs_toolbar->window, stock_pixmap_get_icon(item[1]),
				  &xpm, &xpmmask);

		if (g_strcasecmp(item[3], syl_act) == 0) {

			const gchar *entry = gtk_entry_get_text(GTK_ENTRY(prefs_toolbar->combo_syl_entry));
			get_action_name(entry, &item[2]);
		}
		else {
			item[2] = g_strdup(gtk_entry_get_text(GTK_ENTRY(prefs_toolbar->entry_icon_text)));
		}

		row_set = gtk_clist_append(GTK_CLIST(prefs_toolbar->clist_set), item);
		gtk_clist_set_pixmap(clist_set, row_set, 0, xpm, xpmmask);
	}

	gtk_clist_moveto(clist_set, row_set, 0, row_set/clist_set->rows, 0);
	gtk_clist_select_row(clist_set, row_set, 0);

	g_free(item[2]);
	g_free(item[3]);
}

static void prefs_toolbar_substitute(GtkButton *button, ToolbarPage *prefs_toolbar)
{
	GtkCList *clist_set   = GTK_CLIST(prefs_toolbar->clist_set);
	GtkCList *clist_icons = GTK_CLIST(prefs_toolbar->clist_icons);
	gchar *syl_act = toolbar_ret_descr_from_val(A_SYL_ACTIONS);
	gint row_icons = 0;
	gint row_set = 0;
	GdkPixmap *xpm;
	GdkBitmap *xpmmask;
	gchar *item[4] = {NULL, NULL, NULL, NULL};
	gchar *ac_set;

	/* no rows or nothing selected */
	if ((clist_set->rows == 0) || (clist_set->selection == 0)) return; 

	if (clist_icons->selection) {
		if (clist_icons->selection->data) 
			row_icons = GPOINTER_TO_INT(clist_icons->selection->data);
	} else
		return;
	
	if (clist_set->selection) {
		if (clist_set->selection->data) 
			row_set = GPOINTER_TO_INT(clist_set->selection->data);
	} else
		return;
	
	gtk_clist_get_text(clist_icons, row_icons, 1, &item[1]);
	gtk_clist_get_text(clist_set, row_set, 3, &ac_set);
	item[3] = g_strdup(gtk_entry_get_text(GTK_ENTRY(prefs_toolbar->combo_entry)));

	if (g_strcasecmp(item[1], toolbar_ret_descr_from_val(A_SEPARATOR)) == 0) {
		item[0] = g_strdup(SEPARATOR_PIXMAP);
		item[2] = g_strdup("");
		item[3] = g_strdup(toolbar_ret_descr_from_val(A_SEPARATOR));

		gtk_clist_remove(clist_set, row_set);
		row_set = gtk_clist_insert(clist_set, row_set, item);

		g_free(item[0]);
	} else {

		if ((is_duplicate(prefs_toolbar, item[3])) && (g_strcasecmp(item[3], ac_set) != 0)){
			alertpanel_error(ERROR_MSG);
			g_free(item[3]);
			return;
		}

		stock_pixmap_gdk(prefs_toolbar->window, stock_pixmap_get_icon(item[1]),
				  &xpm, &xpmmask);

		if (g_strcasecmp(item[3], syl_act) == 0) {

			const gchar *entry = gtk_entry_get_text(GTK_ENTRY(prefs_toolbar->combo_syl_entry));
			get_action_name(entry, &item[2]);
		} else {
			item[2] = g_strdup(gtk_entry_get_text(GTK_ENTRY(prefs_toolbar->entry_icon_text)));
		}

		gtk_clist_remove(clist_set, row_set);
		row_set = gtk_clist_insert(clist_set, row_set, item);
		gtk_clist_set_pixmap(clist_set, row_set, 0, xpm, xpmmask);
	}
	
	gtk_clist_moveto(clist_set, row_set, 0, row_set/clist_set->rows, 0);
	gtk_clist_select_row(clist_set, row_set, 0);

	g_free(item[2]);
	g_free(item[3]);
}

static void prefs_toolbar_delete(GtkButton *button, ToolbarPage *prefs_toolbar)
{
	GtkCList *clist_set = GTK_CLIST(prefs_toolbar->clist_set);
	gint row_set = 0;

	if (clist_set->rows == 0) return; 
	if (clist_set->selection) {
		if (clist_set->selection->data) 
			row_set = GPOINTER_TO_INT(clist_set->selection->data);
	} else
		return;
	
	if (clist_set->row_list != NULL) {
			
		row_set = GPOINTER_TO_INT(clist_set->selection->data);
		gtk_clist_remove(clist_set, row_set);
		gtk_clist_columns_autosize(clist_set);
		
		if (clist_set->rows > 0)
			gtk_clist_select_row(clist_set,(row_set == 0) ? 0:row_set - 1, 0);
	}
}

static void prefs_toolbar_up(GtkButton *button, ToolbarPage *prefs_toolbar)
{
	GtkCList *clist = GTK_CLIST(prefs_toolbar->clist_set);
	gint row = 0;

	if (!clist->selection) return;
	if (clist->selection->data)
		row = GPOINTER_TO_INT(clist->selection->data);

	if (row >= 0) {
		gtk_clist_row_move(clist, row, row - 1);
		if(gtk_clist_row_is_visible(clist, row - 1) != GTK_VISIBILITY_FULL) {
			gtk_clist_moveto(clist, row - 1, 0, 0, 0);
		} 
	}
}

static void prefs_toolbar_down(GtkButton *button, ToolbarPage *prefs_toolbar)
{
	GtkCList *clist = GTK_CLIST(prefs_toolbar->clist_set);
	gint row = 0;

	if (!clist->selection) return;
	if (clist->selection->data)
		row = GPOINTER_TO_INT(clist->selection->data);

	if (row >= 0 && row < clist->rows - 1) {
		gtk_clist_row_move(clist, row, row + 1);
		if(gtk_clist_row_is_visible(clist, row + 1) != GTK_VISIBILITY_FULL) {
			gtk_clist_moveto(clist, row + 1, 0, 1, 0);
		} 
	}
}

static void prefs_toolbar_select_row_set(GtkCList *clist, gint row, gint column,
					 GdkEvent *event, ToolbarPage *prefs_toolbar)
{
	GtkCList *clist_ico = GTK_CLIST(prefs_toolbar->clist_icons);
	gchar *syl_act = toolbar_ret_descr_from_val(A_SYL_ACTIONS);
	gint row_set = 0;
	gint row_ico = 0;
	gint item_num = 0;
	gchar *file, *icon_text, *descr, *entry;
	GList *cur;

	if (clist->selection->data) 
		row_set = GPOINTER_TO_INT(clist->selection->data);	

	gtk_clist_get_text(clist, row_set, 1, &file);
	gtk_clist_get_text(clist, row_set, 2, &icon_text);
	gtk_clist_get_text(clist, row_set, 3, &descr);

	if (g_strcasecmp(descr, syl_act) != 0)
		gtk_entry_set_text(GTK_ENTRY(prefs_toolbar->entry_icon_text), icon_text);
	
	/* scan combo list for selected description an set combo item accordingly */
	for (cur = prefs_toolbar->combo_action_list; cur != NULL; cur = cur->next) {
		gchar *item_str = (gchar*)cur->data;
		if (g_strcasecmp(item_str, descr) == 0) {
			gtk_list_select_item(GTK_LIST(prefs_toolbar->combo_list), item_num);
			break;
		}
		else
			item_num++;
	}

	do {
		gtk_clist_get_text(clist_ico, row_ico, 1, &entry);
		row_ico++;
	} while(g_strcasecmp(entry, file) != 0);
	
	gtk_clist_select_row(clist_ico, row_ico - 1, 0);
	gtk_clist_moveto(clist_ico, row_ico - 1, 0, row_ico/clist_ico->rows, 0);
}

static void prefs_toolbar_select_row_icons(GtkCList *clist, gint row, gint column,
					   GdkEvent *event, ToolbarPage *prefs_toolbar)
{
	GtkCList *clist_icons = GTK_CLIST(prefs_toolbar->clist_icons);
	gchar *text;
	
	gtk_clist_get_text(clist_icons, row, 1, &text);

	if (!text) return;

	if (g_strcasecmp(toolbar_ret_descr_from_val(A_SEPARATOR), text) == 0) {
		gtk_widget_set_sensitive(prefs_toolbar->combo_action,     FALSE);
		gtk_widget_set_sensitive(prefs_toolbar->entry_icon_text,  FALSE);
		gtk_widget_set_sensitive(prefs_toolbar->combo_syl_action, FALSE);
	} else {
		gtk_widget_set_sensitive(prefs_toolbar->combo_action,     TRUE);
		gtk_widget_set_sensitive(prefs_toolbar->entry_icon_text,  TRUE);
		gtk_widget_set_sensitive(prefs_toolbar->combo_syl_action, TRUE);
	}
}

static void prefs_toolbar_selection_changed(GtkList *list,
					    ToolbarPage *prefs_toolbar)
{
	gchar *cur_entry = g_strdup(gtk_entry_get_text(GTK_ENTRY(prefs_toolbar->combo_entry)));
	gchar *actions_entry = toolbar_ret_descr_from_val(A_SYL_ACTIONS);

	gtk_widget_set_sensitive(prefs_toolbar->combo_syl_action, TRUE);

	if (g_strcasecmp(cur_entry, actions_entry) == 0) {
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
	GtkWidget *scrolledwindow_clist_icon;
	GtkWidget *clist_icons;
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
	GtkWidget *scrolledwindow_clist_set;
	GtkWidget *clist_set;

	GtkWidget *btn_vbox;
	GtkWidget *up_btn;
	GtkWidget *down_btn;
	
	gchar *titles[N_DISPLAYED_ITEMS_COLS];
 
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
	scrolledwindow_clist_icon = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_set_border_width(GTK_CONTAINER(scrolledwindow_clist_icon), 5);
	gtk_container_add(GTK_CONTAINER(vbox_frame), scrolledwindow_clist_icon);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow_clist_icon), 
					GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	
	clist_icons = gtk_clist_new(2);
	gtk_container_add(GTK_CONTAINER(scrolledwindow_clist_icon), clist_icons);
	gtk_container_set_border_width(GTK_CONTAINER(clist_icons), 1);
	gtk_clist_set_column_width(GTK_CLIST(clist_icons), 0, 35);
	gtk_clist_set_column_width(GTK_CLIST(clist_icons), 1, 200);
	gtk_clist_column_titles_hide(GTK_CLIST(clist_icons));
	gtk_widget_set_usize(clist_icons, 225, 108); 

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
	gtk_widget_set_usize(arrow, -1, 16);

	btn_hbox = gtk_hbox_new(TRUE, 4);
	gtk_box_pack_start(GTK_BOX(reg_hbox), btn_hbox, FALSE, FALSE, 0);

	reg_btn = gtk_button_new_with_label(_("Add"));
	gtk_box_pack_start(GTK_BOX(btn_hbox), reg_btn, FALSE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(reg_btn), "clicked",
			    GTK_SIGNAL_FUNC(prefs_toolbar_register), 
			    prefs_toolbar);

	subst_btn = gtk_button_new_with_label(_("  Replace  "));
	gtk_box_pack_start(GTK_BOX(btn_hbox), subst_btn, FALSE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(subst_btn), "clicked",
			    GTK_SIGNAL_FUNC(prefs_toolbar_substitute),
			    prefs_toolbar);

	del_btn = gtk_button_new_with_label(_("Delete"));
	gtk_box_pack_start(GTK_BOX(btn_hbox), del_btn, FALSE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(del_btn), "clicked",
			    GTK_SIGNAL_FUNC(prefs_toolbar_delete), 
			    prefs_toolbar);

	default_btn = gtk_button_new_with_label(_(" Default "));
	gtk_box_pack_end(GTK_BOX(reg_hbox), default_btn, FALSE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(default_btn), "clicked",
			    GTK_SIGNAL_FUNC(prefs_toolbar_default), 
			    prefs_toolbar);

	/* currently active toolbar items */
	frame_toolbar_items = gtk_frame_new(_("Displayed toolbar items"));
	gtk_box_pack_start(GTK_BOX(main_vbox), frame_toolbar_items, TRUE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(frame_toolbar_items), 5);
	
	hbox_bottom = gtk_hbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(frame_toolbar_items), hbox_bottom);
	
	scrolledwindow_clist_set = gtk_scrolled_window_new(NULL, NULL);
	gtk_box_pack_start(GTK_BOX(hbox_bottom), scrolledwindow_clist_set, TRUE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(scrolledwindow_clist_set), 1);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow_clist_icon), 
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	titles[COL_ICON]     = _("Icon");
	titles[COL_FILENAME] = _("File name");
	titles[COL_TEXT]     = _("Icon text");
	titles[COL_EVENT]    = _("Mapped event");
	
	clist_set = gtk_clist_new_with_titles(N_DISPLAYED_ITEMS_COLS, titles);
	gtk_widget_show(clist_set);
	gtk_container_add(GTK_CONTAINER(scrolledwindow_clist_set), clist_set);
	gtk_clist_set_column_width(GTK_CLIST(clist_set), COL_ICON    , 80);
	gtk_clist_set_column_width(GTK_CLIST(clist_set), COL_FILENAME, 80);
	gtk_clist_set_column_width(GTK_CLIST(clist_set), COL_TEXT    , 80);
	gtk_clist_set_column_width(GTK_CLIST(clist_set), COL_EVENT   , 80);
	gtk_clist_column_titles_show(GTK_CLIST(clist_set));
	gtk_widget_set_usize(clist_set, 225, 120);

	btn_vbox = gtk_vbox_new(FALSE, 8);
	gtk_widget_show(btn_vbox);
	gtk_box_pack_start(GTK_BOX(hbox_bottom), btn_vbox, FALSE, FALSE, 5);

	up_btn = gtk_button_new_with_label(_("Up"));
	gtk_widget_show(up_btn);
	gtk_box_pack_start(GTK_BOX(btn_vbox), up_btn, FALSE, FALSE, 2);

	down_btn = gtk_button_new_with_label(_("Down"));
	gtk_widget_show(down_btn);
	gtk_box_pack_start(GTK_BOX(btn_vbox), down_btn, FALSE, FALSE, 0);

	gtk_signal_connect(GTK_OBJECT(clist_set), "select_row",
			   GTK_SIGNAL_FUNC(prefs_toolbar_select_row_set),
			   prefs_toolbar);
	gtk_signal_connect(GTK_OBJECT(clist_icons), "select_row",
			   GTK_SIGNAL_FUNC(prefs_toolbar_select_row_icons),
			   prefs_toolbar);
	gtk_signal_connect(GTK_OBJECT(combo_list), "selection-changed",
			   GTK_SIGNAL_FUNC(prefs_toolbar_selection_changed),
			   prefs_toolbar);
	gtk_signal_connect(GTK_OBJECT(up_btn), "clicked",
			   GTK_SIGNAL_FUNC(prefs_toolbar_up), prefs_toolbar);
	gtk_signal_connect(GTK_OBJECT(down_btn), "clicked",
			   GTK_SIGNAL_FUNC(prefs_toolbar_down), prefs_toolbar);
	
	gtk_widget_show_all(main_vbox);

	prefs_toolbar->clist_icons      = clist_icons;
	prefs_toolbar->clist_set        = clist_set;
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

	page = g_new0(ToolbarPage, 1);
	page->page.path = _("Customize Toolbars/Main Window");
	page->page.create_widget = prefs_toolbar_create_widget;
	page->page.destroy_widget = prefs_toolbar_destroy_widget;
	page->page.save_page = prefs_toolbar_save;
	page->source = TOOLBAR_MAIN;
	prefs_gtk_register_page((PrefsPage *) page);
	prefs_toolbar_mainwindow = page;

	page = g_new0(ToolbarPage, 1);
	page->page.path = _("Customize Toolbars/Message Window");
	page->page.create_widget = prefs_toolbar_create_widget;
	page->page.destroy_widget = prefs_toolbar_destroy_widget;
	page->page.save_page = prefs_toolbar_save;
	page->source = TOOLBAR_MSGVIEW;
	prefs_gtk_register_page((PrefsPage *) page);
	prefs_toolbar_messageview = page;

	page = g_new0(ToolbarPage, 1);
	page->page.path = _("Customize Toolbars/Compose Window");
	page->page.create_widget = prefs_toolbar_create_widget;
	page->page.destroy_widget = prefs_toolbar_destroy_widget;
	page->page.save_page = prefs_toolbar_save;
	page->source = TOOLBAR_COMPOSE;
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
