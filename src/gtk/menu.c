/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2001 Hiroyuki Yamamoto
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

#include <glib.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenubar.h>
#include <gtk/gtkitemfactory.h>
#include <gtk/gtkcheckmenuitem.h>
#include <gtk/gtkbutton.h>

#include "intl.h"
#include "menu.h"
#include "utils.h"

static gchar *menu_translate(const gchar *path, gpointer data);

GtkWidget *menubar_create(GtkWidget *window, GtkItemFactoryEntry *entries,
			  guint n_entries, const gchar *path, gpointer data)
{
	GtkItemFactory *factory;
	GtkAccelGroup *accel_group;

	accel_group = gtk_accel_group_new();
	factory = gtk_item_factory_new(GTK_TYPE_MENU_BAR, path, accel_group);
	gtk_item_factory_set_translate_func(factory, menu_translate,
					    NULL, NULL);
	gtk_item_factory_create_items(factory, n_entries, entries, data);
	gtk_accel_group_attach(accel_group, GTK_OBJECT(window));

	return gtk_item_factory_get_widget(factory, path);
}

GtkWidget *menu_create_items(GtkItemFactoryEntry *entries,
			     guint n_entries, const gchar *path,
			     GtkItemFactory **factory, gpointer data)
{
	*factory = gtk_item_factory_new(GTK_TYPE_MENU, path, NULL);
	gtk_item_factory_set_translate_func(*factory, menu_translate,
					    NULL, NULL);
	gtk_item_factory_create_items(*factory, n_entries, entries, data);

	return gtk_item_factory_get_widget(*factory, path);
}

GtkWidget *popupmenu_create(GtkWidget *window, GtkItemFactoryEntry *entries,
			     guint n_entries, const gchar *path, gpointer data)
{
	GtkItemFactory *factory;
	GtkAccelGroup *accel_group;

	accel_group = gtk_accel_group_new();
	factory = gtk_item_factory_new(GTK_TYPE_MENU, path, accel_group);
	gtk_item_factory_set_translate_func(factory, menu_translate,
					    NULL, NULL);
	gtk_item_factory_create_items(factory, n_entries, entries, data);
	gtk_accel_group_attach(accel_group, GTK_OBJECT(window));

	return gtk_item_factory_get_widget(factory, path);
}

static gchar *menu_translate(const gchar *path, gpointer data)
{
	gchar *retval;

	retval = gettext(path);

	return retval;
}

void menu_set_sensitive(GtkItemFactory *ifactory, const gchar *path,
			gboolean sensitive)
{
	GtkWidget *widget;

	g_return_if_fail(ifactory != NULL);

	widget = gtk_item_factory_get_item(ifactory, path);
	if(widget == NULL) {
		debug_print("unknown menu entry %s\n", path);
		return;
	}
	gtk_widget_set_sensitive(widget, sensitive);
}

void menu_set_sensitive_all(GtkMenuShell *menu_shell, gboolean sensitive)
{
	GList *cur;

	for (cur = menu_shell->children; cur != NULL; cur = cur->next)
		gtk_widget_set_sensitive(GTK_WIDGET(cur->data), sensitive);
}

void menu_set_toggle(GtkItemFactory *ifactory, const gchar *path,
			gboolean active)
{
	GtkWidget *widget;

	g_return_if_fail(ifactory != NULL);

	widget = gtk_item_factory_get_item(ifactory, path);
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(widget), active);
}

void menu_toggle_toggle(GtkItemFactory *ifactory, const gchar *path)
{
	GtkWidget *widget;
	
	g_return_if_fail(ifactory != NULL);
	
	widget = gtk_item_factory_get_item(ifactory, path);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(widget), !((GTK_CHECK_MENU_ITEM(widget))->active));
}

void menu_button_position(GtkMenu *menu, gint *x, gint *y, gpointer user_data)
{
	GtkWidget *button;
	GtkRequisition requisition;
	gint button_xpos, button_ypos;
	gint xpos, ypos;
	gint width, height;
	gint scr_width, scr_height;

	g_return_if_fail(user_data != NULL);
	g_return_if_fail(GTK_IS_BUTTON(user_data));

	button = GTK_WIDGET(user_data);

	gtk_widget_get_child_requisition(GTK_WIDGET(menu), &requisition);
	width = requisition.width;
	height = requisition.height;
	gdk_window_get_origin(button->window, &button_xpos, &button_ypos);

	xpos = button_xpos;
	ypos = button_ypos + button->allocation.height;

	scr_width = gdk_screen_width();
	scr_height = gdk_screen_height();

	if (xpos + width > scr_width)
		xpos -= (xpos + width) - scr_width;
	if (ypos + height > scr_height)
		ypos = button_ypos - height;
	if (xpos < 0)
		xpos = 0;
	if (ypos < 0)
		ypos = 0;

	*x = xpos;
	*y = ypos;
}

gint menu_find_option_menu_index(GtkOptionMenu *optmenu, gpointer data,
				 GCompareFunc func)
{
	GtkWidget *menu;
	GtkWidget *menuitem;
	gpointer menu_data;
	GList *cur;
	gint n;

	menu = gtk_option_menu_get_menu(optmenu);

	for (cur = GTK_MENU_SHELL(menu)->children, n = 0;
	     cur != NULL; cur = cur->next, n++) {
		menuitem = GTK_WIDGET(cur->data);
		menu_data = gtk_object_get_user_data(GTK_OBJECT(menuitem));
		if (func) {
			if (func(menu_data, data) == 0)
				return n;
		} else if (menu_data == data)
			return n;
	}

	return -1;
}

