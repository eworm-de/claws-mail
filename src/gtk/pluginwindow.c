/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2002 Hiroyuki Yamamoto and the Sylpheed-Claws Team
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

#include <string.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "intl.h"
#include "plugin.h"

#include "filesel.h"
#include "alertpanel.h"
#include "../inc.h"

typedef struct _PluginWindow
{
	GtkWidget *window;
	GtkWidget *plugin_list;
	GtkWidget *plugin_desc;
	GtkWidget *unload_btn;

	Plugin *selected_plugin;
} PluginWindow;

static void close_cb(GtkButton *button, PluginWindow *pluginwindow)
{
	gtk_widget_destroy(pluginwindow->window);
	g_free(pluginwindow);
	plugin_save_list();
	inc_unlock();
}

static void set_plugin_list(PluginWindow *pluginwindow)
{
	GSList *plugins, *cur;
	gchar *text[1];
	gint row;
	GtkCList *clist = GTK_CLIST(pluginwindow->plugin_list);
	GtkTextBuffer *textbuf;
	GtkTextIter start_iter, end_iter;

	plugins = plugin_get_list();
	gtk_clist_freeze(clist);
	gtk_clist_clear(clist);
	textbuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(pluginwindow->plugin_desc));
	gtk_text_buffer_get_start_iter(textbuf, &start_iter);
	gtk_text_buffer_get_end_iter(textbuf, &end_iter);
	gtk_text_buffer_delete(textbuf, &start_iter, &end_iter);
	gtk_widget_set_sensitive(pluginwindow->unload_btn, FALSE);

	for(cur = plugins; cur != NULL; cur = g_slist_next(cur)) {
		Plugin *plugin = (Plugin *) cur->data;

		text[0] = (gchar *) plugin_get_name(plugin);
		row = gtk_clist_append(clist, text);
		gtk_clist_set_row_data(clist, row, plugin);
	}
	gtk_clist_thaw(clist);
	if (pluginwindow->selected_plugin == NULL)
		gtk_clist_select_row (clist, 0, -1);
}

static void select_row_cb(GtkCList *clist, gint row, gint column,
			  GdkEventButton *event, PluginWindow *pluginwindow)
{
	Plugin *plugin;
	GtkTextView *plugin_desc = GTK_TEXT_VIEW(pluginwindow->plugin_desc);
	GtkTextBuffer *textbuf = gtk_text_view_get_buffer(plugin_desc);
	GtkTextIter start_iter, end_iter;
	const gchar *text;

	plugin = (Plugin *) gtk_clist_get_row_data(clist, row);
	pluginwindow->selected_plugin = plugin;

	if (pluginwindow->selected_plugin != NULL) {
		gtk_text_buffer_get_start_iter(textbuf, &start_iter);
		gtk_text_buffer_get_end_iter(textbuf, &end_iter);
		gtk_text_buffer_delete(textbuf, &start_iter, &end_iter);
		text = plugin_get_desc(plugin);
		gtk_text_buffer_insert(textbuf, &start_iter, text, strlen(text));
		gtk_widget_set_sensitive(pluginwindow->unload_btn, TRUE);
	} else {
		gtk_widget_set_sensitive(pluginwindow->unload_btn, FALSE);
	}
}

static void unselect_row_cb(GtkCList * clist, gint row, gint column,
			    GdkEventButton * event, PluginWindow *pluginwindow)
{
	gtk_widget_set_sensitive(pluginwindow->unload_btn, FALSE);	
}

static void unload_cb(GtkButton *button, PluginWindow *pluginwindow)
{
	Plugin *plugin = pluginwindow->selected_plugin;

	g_return_if_fail(plugin != NULL);
	plugin_unload(plugin);
	pluginwindow->selected_plugin = NULL;
	set_plugin_list(pluginwindow);
}

static void load_cb(GtkButton *button, PluginWindow *pluginwindow)
{
	gchar *file, *error = NULL;

	file = filesel_select_file_open(_("Select Plugin to load"), PLUGINDIR);
	if (file == NULL)
		return;

	plugin_load(file, &error);
	if (error != NULL) {
		alertpanel_error("The following error occured while loading the plugin:\n%s\n", error);
		g_free(error);
	}

	set_plugin_list(pluginwindow);		
}

static void pluginwindow_key_pressed(GtkWidget *widget, GdkEventKey *event,
				     PluginWindow *pluginwindow)
{
	if (event) {
		switch (event->keyval) {
			case GDK_Escape : 
			case GDK_Return : 
			case GDK_KP_Enter :
				close_cb(NULL, pluginwindow);
				break;
			case GDK_Insert : 
			case GDK_KP_Insert :
			case GDK_KP_Add : 
			case GDK_plus :
				load_cb(NULL, pluginwindow);
				break;
			case GDK_Delete : 
			case GDK_KP_Delete :
			case GDK_KP_Subtract : 
			case GDK_minus :
				unload_cb(NULL, pluginwindow);
				break;
			default :
				break;
		}
	}
}

void pluginwindow_create()
{
	PluginWindow *pluginwindow;
	/* ---------------------- code made by glade ---------------------- */
	GtkWidget *window;
	GtkWidget *vbox1;
	GtkWidget *hbox2;
	GtkWidget *scrolledwindow2;
	GtkWidget *plugin_list;
	GtkWidget *label12;
	GtkWidget *vbox2;
	GtkWidget *frame2;
	GtkWidget *label13;
	GtkWidget *scrolledwindow3;
	GtkWidget *plugin_desc;
	GtkWidget *hbuttonbox1;
	GtkWidget *load_btn;
	GtkWidget *unload_btn;
	GtkWidget *close_btn;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_container_set_border_width(GTK_CONTAINER(window), 8);
	gtk_window_set_default_size(GTK_WINDOW(window), 480, 300);
	gtk_window_set_title(GTK_WINDOW(window), _("Plugins"));
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);

	vbox1 = gtk_vbox_new(FALSE, 4);
	gtk_widget_show(vbox1);
	gtk_container_add(GTK_CONTAINER(window), vbox1);

	hbox2 = gtk_hbox_new(FALSE, 8);
	gtk_widget_show(hbox2);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox2, TRUE, TRUE, 0);

	scrolledwindow2 = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(scrolledwindow2);
	gtk_box_pack_start(GTK_BOX(hbox2), scrolledwindow2, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW
				       (scrolledwindow2), GTK_POLICY_NEVER,
				       GTK_POLICY_AUTOMATIC);

	plugin_list = gtk_clist_new(1);
	gtk_widget_show(plugin_list);
	gtk_container_add(GTK_CONTAINER(scrolledwindow2), plugin_list);
	gtk_clist_set_column_width(GTK_CLIST(plugin_list), 0, 80);
	gtk_clist_column_titles_show(GTK_CLIST(plugin_list));
	gtk_clist_set_selection_mode(GTK_CLIST (plugin_list), GTK_SELECTION_BROWSE);
	gtk_widget_grab_focus(GTK_WIDGET(plugin_list));

	label12 = gtk_label_new(_("Plugins"));
	gtk_widget_show(label12);
	gtk_clist_set_column_widget(GTK_CLIST(plugin_list), 0, label12);
	gtk_label_set_justify(GTK_LABEL(label12), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(label12), 0, 0.5);

	vbox2 = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(vbox2);
	gtk_box_pack_start(GTK_BOX(hbox2), vbox2, TRUE, TRUE, 0);

	frame2 = gtk_frame_new(NULL);
	gtk_widget_show(frame2);
	gtk_box_pack_start(GTK_BOX(vbox2), frame2, FALSE, TRUE, 0);

	label13 = gtk_label_new(_("Description"));
	gtk_widget_show(label13);
	gtk_container_add(GTK_CONTAINER(frame2), label13);
	gtk_misc_set_alignment(GTK_MISC(label13), 0, 0.5);
	gtk_misc_set_padding(GTK_MISC(label13), 2, 2);

	scrolledwindow3 = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(scrolledwindow3);
	gtk_box_pack_start(GTK_BOX(vbox2), scrolledwindow3, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW
				       (scrolledwindow3), GTK_POLICY_NEVER,
				       GTK_POLICY_AUTOMATIC);

	plugin_desc = gtk_text_view_new();
	gtk_widget_show(plugin_desc);
	gtk_container_add(GTK_CONTAINER(scrolledwindow3), plugin_desc);

	hbuttonbox1 = gtk_hbutton_box_new();
	gtk_widget_show(hbuttonbox1);
	gtk_box_pack_start(GTK_BOX(vbox1), hbuttonbox1, FALSE, FALSE, 0);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(hbuttonbox1),
				  GTK_BUTTONBOX_END);
	gtk_button_box_set_spacing(GTK_BUTTON_BOX(hbuttonbox1), 0);

	load_btn = gtk_button_new_with_label(_("Load Plugin"));
	gtk_widget_show(load_btn);
	gtk_container_add(GTK_CONTAINER(hbuttonbox1), load_btn);
	GTK_WIDGET_SET_FLAGS(load_btn, GTK_CAN_DEFAULT);

	unload_btn = gtk_button_new_with_label(_("Unload Plugin"));
	gtk_widget_show(unload_btn);
	gtk_container_add(GTK_CONTAINER(hbuttonbox1), unload_btn);
	GTK_WIDGET_SET_FLAGS(unload_btn, GTK_CAN_DEFAULT);

	close_btn = gtk_button_new_with_label(_("Close"));
	gtk_widget_show(close_btn);
	gtk_container_add(GTK_CONTAINER(hbuttonbox1), close_btn);
	GTK_WIDGET_SET_FLAGS(close_btn, GTK_CAN_DEFAULT);
	/* ----------------------------------------------------------- */

	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(plugin_desc), GTK_WRAP_WORD);
	gtk_widget_set_sensitive(GTK_WIDGET(unload_btn), FALSE);

	pluginwindow = g_new0(PluginWindow, 1);

	g_signal_connect(G_OBJECT(load_btn), "released",
			 G_CALLBACK(load_cb), pluginwindow);
	g_signal_connect(G_OBJECT(unload_btn), "released",
			 G_CALLBACK(unload_cb), pluginwindow);
	g_signal_connect(G_OBJECT(close_btn), "released",
			 G_CALLBACK(close_cb), pluginwindow);
	g_signal_connect(G_OBJECT(plugin_list), "select-row",
			 G_CALLBACK(select_row_cb), pluginwindow);
	g_signal_connect(G_OBJECT(plugin_list), "unselect-row",
			 G_CALLBACK(unselect_row_cb), pluginwindow);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			   G_CALLBACK(pluginwindow_key_pressed), pluginwindow);

	pluginwindow->window = window;
	pluginwindow->plugin_list = plugin_list;
	pluginwindow->plugin_desc = plugin_desc;
	pluginwindow->unload_btn = unload_btn;
	pluginwindow->selected_plugin = NULL;

	set_plugin_list(pluginwindow);

	inc_lock();
	gtk_widget_show(window);
}
