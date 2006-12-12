/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2006 Hiroyuki Yamamoto
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

#ifndef __MENU_H__
#define __MENU_H__

#include <glib.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkitemfactory.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenushell.h>
#include <gtk/gtkoptionmenu.h>

#define MENU_VAL_ID "Claws::Menu::ValueID"
#define MENU_VAL_DATA "Claws::Menu::ValueDATA"

#define MENUITEM_ADD(menu, menuitem, label, data) 		 \
{ 								 \
 	if (label)						 \
 		menuitem = gtk_menu_item_new_with_label(label);	 \
 	else {							 \
 		menuitem = gtk_menu_item_new();			 \
 		gtk_widget_set_sensitive(menuitem, FALSE);	 \
 	}							 \
	gtk_widget_show(menuitem); 				 \
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem); 	 \
	if (data) 						 \
		g_object_set_data(G_OBJECT(menuitem), 		 \
				  MENU_VAL_ID, 			 \
				  GINT_TO_POINTER(data)); 	 \
}

#define menu_set_insensitive_all(menu_shell) \
	menu_set_sensitive_all(menu_shell, FALSE);

GtkWidget *menubar_create	(GtkWidget		*window,
				 GtkItemFactoryEntry	*entries,
				 guint			 n_entries,
				 const gchar		*path,
				 gpointer		 data);
GtkWidget *menu_create_items	(GtkItemFactoryEntry	*entries,
				 guint			 n_entries,
				 const gchar		*path,
				 GtkItemFactory	       **factory,
				 gpointer		 data);
GtkWidget *popupmenu_create	(GtkWidget *window,
				 GtkItemFactoryEntry *entries,
				 guint n_entries,
				 const gchar *path,
				 gpointer data);
gchar *menu_translate		(const gchar *path, gpointer data);

void menu_set_sensitive		(GtkItemFactory		*ifactory,
				 const gchar		*path,
				 gboolean		 sensitive);
void menu_set_sensitive_all	(GtkMenuShell		*menu_shell,
				 gboolean		 sensitive);

void menu_set_active		(GtkItemFactory		*ifactory,
				 const gchar		*path,
				 gboolean		 is_active);
void menu_button_position	(GtkMenu		*menu,
				 gint			*x,
				 gint			*y,
				 gboolean		*push_in,
				 gpointer		 user_data);

gint menu_find_option_menu_index(GtkOptionMenu		*optmenu,
				 gpointer		 data,
				 GCompareFunc		 func);

gpointer menu_get_option_menu_active_user_data
				(GtkOptionMenu		*optmenu);
void menu_connect_identical_items(void);

void menu_select_by_data	(GtkMenu 		*menu,
				 gpointer		 data);

#endif /* __MENU_H__ */
