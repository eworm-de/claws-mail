/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2003 Hiroyuki Yamamoto
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
#include <gtk/gtk.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenubar.h>
#include <gtk/gtkitemfactory.h>
#include <gtk/gtkcheckmenuitem.h>
#include <gtk/gtkbutton.h>

#include "intl.h"
#include "menu.h"
#include "utils.h"

static void menu_item_add_accel( GtkWidget *widget, guint accel_signal_id, GtkAccelGroup *accel_group,
				 guint accel_key, GdkModifierType accel_mods, GtkAccelFlags accel_flags,
				 gpointer user_data);

static void menu_item_remove_accel(GtkWidget *widget, GtkAccelGroup *accel_group,
			           guint accel_key, GdkModifierType accel_mods,
			           gpointer user_data);

static void connect_accel_change_signals(GtkWidget* widget, GtkWidget *wid2) ;


GtkWidget *menubar_create(GtkWidget *window, GtkItemFactoryEntry *entries,
			  guint n_entries, const gchar *path, gpointer data)
{
	GtkItemFactory *factory;

	factory = gtk_item_factory_new(GTK_TYPE_MENU_BAR, path, NULL);
	gtk_item_factory_set_translate_func(factory, menu_translate,
					    NULL, NULL);
	gtk_item_factory_create_items(factory, n_entries, entries, data);
	gtk_accel_group_attach(factory->accel_group, GTK_OBJECT(window));

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

gchar *menu_translate(const gchar *path, gpointer data)
{
	gchar *retval;

	retval = gettext(path);

	return retval;
}

static void factory_print_func(gpointer data, gchar *string)
{
	GString *out_str = data;

	g_string_append(out_str, string);
	g_string_append_c(out_str, '\n');
}

GString *menu_factory_get_rc(const gchar *path)
{
	GString *string;
	GtkPatternSpec *pspec;
	gchar pattern[256];

	pspec = g_new(GtkPatternSpec, 1);
	g_snprintf(pattern, sizeof(pattern), "%s*", path);
	gtk_pattern_spec_init(pspec, pattern);
	string = g_string_new("");
	gtk_item_factory_dump_items(pspec, FALSE, factory_print_func,
				    string);
	gtk_pattern_spec_free_segs(pspec);

	return string;
}

void menu_factory_clear_rc(const gchar *rc_str)
{
	GString *string;
	gchar *p;
	gchar *start, *end;
	guint pos = 0;

	string = g_string_new(rc_str);
	while ((p = strstr(string->str + pos, "(menu-path \"")) != NULL) {
		pos = p + 12 - string->str;
		p = strchr(p + 12, '"');
		if (!p) continue;
		start = strchr(p + 1, '"');
		if (!start) continue;
		end = strchr(start + 1, '"');
		if (!end) continue;
		pos = start + 1 - string->str;
		if (end > start + 1)
			g_string_erase(string, pos, end - (start + 1));
	}

	gtk_item_factory_parse_rc_string(string->str);
	g_string_free(string, TRUE);
}

void menu_factory_copy_rc(const gchar *src_path, const gchar *dest_path)
{
	GString *string;
	gint src_path_len;
	gint dest_path_len;
	gchar *p;
	guint pos = 0;

	string = menu_factory_get_rc(src_path);
	src_path_len = strlen(src_path);
	dest_path_len = strlen(dest_path);

	while ((p = strstr(string->str + pos, src_path)) != NULL) {
		pos = p - string->str;
		g_string_erase(string, pos, src_path_len);
		g_string_insert(string, pos, dest_path);
		pos += dest_path_len;
	}

	pos = 0;
	while ((p = strchr(string->str + pos, ';')) != NULL) {
		pos = p - string->str;
		if (pos == 0 || *(p - 1) == '\n')
			g_string_erase(string, pos, 1);
	}

	menu_factory_clear_rc(string->str);
	gtk_item_factory_parse_rc_string(string->str);
	g_string_free(string, TRUE);
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

/* call backs for accelerator changes on selected menu items */
static void menu_item_add_accel( GtkWidget *widget, guint accel_signal_id, GtkAccelGroup *accel_group,
				 guint accel_key, GdkModifierType accel_mods, GtkAccelFlags accel_flags,
				 gpointer user_data)
{
	GtkWidget *connected = GTK_WIDGET(user_data);	
	if (gtk_signal_n_emissions_by_name(GTK_OBJECT(widget),"add_accelerator") > 1 ) return;
	gtk_widget_remove_accelerators(connected,"activate",FALSE);
	/* lock _this_ widget */
	gtk_accel_group_lock_entry(accel_group,accel_key,accel_mods);
	/* modify the _other_ widget */
	gtk_widget_add_accelerator(connected, "activate",
				   gtk_item_factory_from_widget(connected)->accel_group,
				   accel_key, accel_mods,
				   GTK_ACCEL_VISIBLE );
	gtk_accel_group_unlock_entry(accel_group,accel_key,accel_mods);				   
}

static void menu_item_remove_accel(GtkWidget *widget, GtkAccelGroup *accel_group,
			           guint accel_key, GdkModifierType accel_mods,
			           gpointer user_data)
{	
	GtkWidget *wid = GTK_WIDGET(user_data);

	if (gtk_signal_n_emissions_by_name(GTK_OBJECT(widget),
	    "remove_accelerator") > 2 )
		return;
	gtk_widget_remove_accelerators(wid,"activate",FALSE);
}

static void connect_accel_change_signals(GtkWidget* widget, GtkWidget *wid2) 
{
	gtk_signal_connect_after(GTK_OBJECT(widget), "add_accelerator", 
				 menu_item_add_accel, wid2);
	gtk_signal_connect_after(GTK_OBJECT(widget), "remove_accelerator", 
				 menu_item_remove_accel, wid2);
}

void menu_connect_identical_items(void)
{
	gint n;
	GtkWidget *item1;
	GtkWidget *item2;

	static const struct {	
		const gchar *path1;
		const gchar *path2;
	} pairs[] = {
		{"<Main>/Message/Reply",  			"<SummaryView>/Reply"},
		{"<Main>/Message/Reply to/all",  		"<SummaryView>/Reply to/all"},
		{"<Main>/Message/Reply to/sender", 		"<SummaryView>/Reply to/sender"},
		{"<Main>/Message/Reply to/mailing list",	"<SummaryView>/Reply to/mailing list"},
		{"<Main>/Message/Follow-up and reply to",	"<SummaryView>/Follow-up and reply to"},
		{"<Main>/Message/Forward",		 	"<SummaryView>/Forward"},
		{"<Main>/Message/Redirect",			"<SummaryView>/Redirect"},
		{"<Main>/Message/Re-edit",			"<SummaryView>/Re-edit"},
		{"<Main>/Message/Move...",			"<SummaryView>/Move..."},
		{"<Main>/Message/Copy...",			"<SummaryView>/Copy..."},
		{"<Main>/Message/Delete",			"<SummaryView>/Delete"},
		{"<Main>/Message/Cancel a news message",	"<SummaryView>/Cancel a news message"},
		{"<Main>/Message/Mark/Mark",			"<SummaryView>/Mark/Mark"},
		{"<Main>/Message/Mark/Unmark",			"<SummaryView>/Mark/Unmark"},
		{"<Main>/Message/Mark/Mark as unread",		"<SummaryView>/Mark/Mark as unread"},
		{"<Main>/Message/Mark/Mark as read",		"<SummaryView>/Mark/Mark as read"},
		{"<Main>/Message/Mark/Mark all read",		"<SummaryView>/Mark/Mark all read"},
		{"<Main>/Tools/Add sender to address book",	"<SummaryView>/Add sender to address book"},
		{"<Main>/Tools/Create filter rule/Automatically",	"<SummaryView>/Create filter rule/Automatically"},
		{"<Main>/Tools/Create filter rule/by From",	"<SummaryView>/Create filter rule/by From"},
		{"<Main>/Tools/Create filter rule/by To",	"<SummaryView>/Create filter rule/by To"},
		{"<Main>/Tools/Create filter rule/by Subject",	"<SummaryView>/Create filter rule/by Subject"},
		{"<Main>/View/Open in new window",		"<SummaryView>/View/Open in new window"},
		{"<Main>/View/Message source",			"<SummaryView>/View/Source"},
		{"<Main>/View/Show all headers",		"<SummaryView>/View/All header"},
	};

	const gint numpairs = sizeof pairs / sizeof pairs[0];
	for (n = 0; n < numpairs; n++) {
		/* get widgets from the paths */

		item1 = gtk_item_factory_get_widget
				(gtk_item_factory_from_path(pairs[n].path1),pairs[n].path1);		
		item2 = gtk_item_factory_get_widget
				(gtk_item_factory_from_path(pairs[n].path2),pairs[n].path2);		

		if (item1 && item2) {
			/* connect widgets both ways around */
			connect_accel_change_signals(item2,item1);
			connect_accel_change_signals(item1,item2);
		} else { 
			if (!item1) debug_print(" ** Menu item not found: %s\n",pairs[n].path1);
			if (!item2) debug_print(" ** Menu item not found: %s\n",pairs[n].path2);
		}				
	}
}

void menu_select_by_data(GtkMenu *menu, gpointer data)
{
	GList *children, *cur;
	GtkWidget *select_item = NULL;
	
	g_return_if_fail(menu != NULL);

	children = gtk_container_children(GTK_CONTAINER(menu));

	for (cur = children; cur != NULL; cur = g_list_next(cur)) {
		GtkObject *child = GTK_OBJECT(cur->data);

		if (gtk_object_get_user_data(child) == data) {
			select_item = GTK_WIDGET(child);
		}
	}
	if (select_item != NULL) {
		gtk_menu_shell_select_item(GTK_MENU_SHELL(menu), select_item);
		gtk_menu_shell_activate_item(GTK_MENU_SHELL(menu), select_item, FALSE);
	}

	g_list_free(children);
}
