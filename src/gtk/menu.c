/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2013 Hiroyuki Yamamoto and the Claws Mail team
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
#include "claws-features.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "menu.h"
#include "utils.h"
#include "gtkutils.h"
#include "defs.h"

GtkActionGroup *cm_menu_create_action_group(const gchar *name, GtkActionEntry *entries,
					    gint num_entries, gpointer data)
{
	GtkActionGroup *group = gtk_action_group_new(name);
	gtk_action_group_set_translate_func(group, menu_translate, NULL, NULL);
	gtk_action_group_add_actions(group, entries, num_entries, data);
	gtk_ui_manager_insert_action_group(gtkut_ui_manager(), group, 0);
	return group;
}

GtkActionGroup *cm_menu_create_action_group_full(GtkUIManager *manager, const gchar *name, GtkActionEntry *entries,
					    gint num_entries, gpointer data)
{
	GtkActionGroup *group = gtk_action_group_new(name);
	gtk_action_group_set_translate_func(group, menu_translate, NULL, NULL);
	gtk_action_group_add_actions(group, entries, num_entries, data);
	gtk_ui_manager_insert_action_group(manager, group, 0);
	return group;
}

gchar *menu_translate(const gchar *path, gpointer data)
{
	gchar *retval;

	retval = gettext(path);

	return retval;
}

void cm_menu_set_sensitive(gchar *menu, gboolean sensitive)
{
	GtkUIManager *gui_manager = gtkut_ui_manager();
	gchar *path = g_strdup_printf("Menus/%s", menu);

	cm_menu_set_sensitive_full(gui_manager, path, sensitive);
	g_free(path);
}

void cm_toggle_menu_set_active(gchar *menu, gboolean active)
{
	GtkUIManager *gui_manager = gtkut_ui_manager();
	gchar *path = g_strdup_printf("Menus/%s", menu);

	cm_toggle_menu_set_active_full(gui_manager, path, active);
	g_free(path);
}

void cm_menu_set_sensitive_full(GtkUIManager *gui_manager, const gchar *menu, gboolean sensitive)
{
	GtkWidget *widget;
	gchar *path = g_strdup_printf("/%s/", menu);

	widget = gtk_ui_manager_get_widget(gui_manager, path);
	if( !GTK_IS_WIDGET(widget) ) {
		g_message("Blah, '%s' is not a widget.\n", path);
	}

	if( !GTK_IS_MENU_ITEM(widget) ) {
		g_message("Blah, '%s' is not a menu item.\n", path);
	}

	gtk_widget_set_sensitive(widget, sensitive);

	if (strcmp(menu, "Menus/SummaryViewPopup/Reedit") == 0)
		(sensitive)? gtk_widget_show(widget) : gtk_widget_hide(widget);

	g_free(path);
}

gchar *cm_menu_item_get_shortcut(GtkUIManager *gui_manager, gchar *menu)
{
	GtkWidget *widget;
	gchar *path = g_strdup_printf("/%s/", menu);
	const gchar *accel = NULL;
	GtkAccelKey key;

	widget = gtk_ui_manager_get_widget(gui_manager, path);
	if( !GTK_IS_WIDGET(widget) ) {
		g_message("Blah, '%s' is not a widget.\n", path);
	}

	if( !GTK_IS_MENU_ITEM(widget) ) {
		g_message("Blah, '%s' is not a menu item.\n", path);
	}

	g_free(path);

	accel = gtk_menu_item_get_accel_path(GTK_MENU_ITEM(widget));

	if (accel && gtk_accel_map_lookup_entry(accel, &key))
		return gtk_accelerator_get_label (key.accel_key, key.accel_mods);
	else
		return g_strdup(_("None"));

}

GtkWidget *cm_menu_item_new_label_from_url(gchar *url)
{
	gint len = strlen(url);
	if (len > MAX_MENU_LABEL_LENGTH) {
		g_message("Refusing a %d bytes string as menu label\n", len);
		url[64] = '\0', url[63] = url[62] = url[61] = '.', url[60] = ' ';
		GtkWidget *newlabel = gtk_menu_item_new_with_label(url);
		gtk_widget_set_tooltip_markup(GTK_WIDGET(newlabel),
			g_strconcat("<span><b>", _("Warning:"), "</b>",
			_("This URL was too long for displaying and\n"
			"has been truncated for safety. This message could be\n"
			"corrupted, malformed or part of some DoS attempt."),
			"</span>", NULL));
		return newlabel;
	}
	
	return gtk_menu_item_new_with_label(url);
}

void cm_toggle_menu_set_active_full(GtkUIManager *gui_manager, gchar *menu, gboolean active)
{
	GtkWidget *widget;
	gchar *path = g_strdup_printf("/%s/", menu);

	widget = gtk_ui_manager_get_widget(gui_manager, path);
	if( !GTK_IS_WIDGET(widget) ) {
		g_message("Blah, '%s' is not a widget.\n", path);
	}

	if( !GTK_CHECK_MENU_ITEM(widget) ) {
		g_message("Blah, '%s' is not a check menu item.\n", path);
	}

	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(widget), active);
	g_free(path);
}

void menu_set_sensitive_all(GtkMenuShell *menu_shell, gboolean sensitive)
{
	GList *children = gtk_container_get_children(GTK_CONTAINER(menu_shell));
	GList *cur;

	for (cur = children; cur != NULL; cur = cur->next)
		gtk_widget_set_sensitive(GTK_WIDGET(cur->data), sensitive);

	g_list_free(children);
}
