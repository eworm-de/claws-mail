/*
 * Notification plugin for Claws Mail
 * Claws Mail -- A GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2005-2021 Holger Berndt and the Claws Mail team
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
#  include "claws-features.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>

#ifdef NOTIFICATION_BANNER

#include <gtk/gtk.h>

#include "gtk/gtkutils.h"

#include "prefs_common.h"
#include "mainwindow.h"
#include "menu.h"
#include "procmsg.h"
#include "messageview.h"
#include "compose.h"

#include "notification_core.h"
#include "notification_prefs.h"
#include "notification_banner.h"

typedef struct {
	GtkWidget *table;
} NotificationBannerEntry;

typedef struct {
	GtkWidget *window;
	GtkWidget *scrolled_win;
	GtkWidget *viewport;

	NotificationBannerEntry *entries;
	guint timeout_id;
	gboolean scrolling;
} NotificationBanner;

typedef struct {
	gint banner_width;
	GtkAdjustment *adj;
} ScrollingData;


static void       notification_banner_create(GSList*);
static gboolean   scroller(gpointer data);
static GtkWidget* create_entrybox(GSList*);
static gboolean   notification_banner_configure(GtkWidget*, GdkEventConfigure*,
						gpointer);
static gboolean notification_banner_swap_colors(GtkWidget*,GdkEventCrossing*,gpointer);
static gboolean notification_banner_button_press(GtkWidget*,GdkEventButton*,gpointer);
static void notification_banner_show_popup(GtkWidget*,GdkEventButton*);
static void notification_banner_popup_done(GtkMenuShell*,gpointer);

static void banner_menu_reply_cb(GtkAction *,gpointer);


static NotificationBanner banner;
static ScrollingData      sdata;

static GtkWidget *banner_popup;
static GtkUIManager *banner_ui_manager;
static GtkActionGroup *banner_action_group;

static gboolean banner_popup_open = FALSE;

static MsgInfo *current_msginfo = NULL;

/* Corresponding mutexes */
G_LOCK_DEFINE_STATIC(banner);
G_LOCK_DEFINE_STATIC(sdata);

static GtkActionEntry banner_popup_entries[] = 
{
	{"BannerPopup",		NULL, "BannerPopup", NULL, NULL, NULL },
	{"BannerPopup/Reply",	NULL, N_("_Reply"), NULL, NULL, G_CALLBACK(banner_menu_reply_cb) },
};


void notification_banner_show(GSList *msg_list)
{
	G_LOCK(banner);
	if((notify_config.banner_show != NOTIFY_BANNER_SHOW_NEVER) &&
	   (g_slist_length(msg_list) ||
	   (notify_config.banner_show == NOTIFY_BANNER_SHOW_ALWAYS)))
	  	notification_banner_create(msg_list);
	else 
	  	notification_banner_destroy();
	G_UNLOCK(banner);
}

void notification_banner_destroy(void)
{
	if(banner.window) {
		if(banner.entries) {
			g_free(banner.entries);
			banner.entries = NULL;
		}
		gtk_widget_destroy(banner.window);
		banner.window = NULL;
		G_LOCK(sdata);
		sdata.adj = NULL;
		sdata.banner_width = 0;
		G_UNLOCK(sdata);
		if(banner.timeout_id) {
			g_source_remove(banner.timeout_id);
			banner.timeout_id = 0;
		}
	}
}

static void notification_banner_create(GSList *msg_list)
{
  GtkRequisition requisition, requisition_after;
	GtkWidget *viewport;
	GtkWidget *hbox;
	GtkWidget *entrybox;
	GdkColor bg;
	gint banner_width;

	/* Window */
	if(!banner.window) {
		banner.window = gtkut_window_new(GTK_WINDOW_TOPLEVEL, "notification_banner");
		gtk_window_set_decorated(GTK_WINDOW(banner.window), FALSE);
		if(notify_config.banner_width > 0)
			gtk_widget_set_size_request(banner.window, notify_config.banner_width, -1);
		else
			gtk_widget_set_size_request(banner.window, gdk_screen_width(), -1);
		gtk_window_set_keep_above(GTK_WINDOW(banner.window), TRUE);
		gtk_window_set_accept_focus(GTK_WINDOW(banner.window), FALSE);
		gtk_window_set_skip_taskbar_hint(GTK_WINDOW(banner.window), TRUE);
		gtk_window_move(GTK_WINDOW(banner.window),
				notify_config.banner_root_x, notify_config.banner_root_y);
		g_signal_connect(banner.window, "configure-event",
				 G_CALLBACK(notification_banner_configure), NULL);
	} else {
		if(banner.entries) {
			g_free(banner.entries);
			banner.entries = NULL;
		}
		gtk_widget_destroy(banner.scrolled_win);
	}
	if(notify_config.banner_sticky)
		gtk_window_stick(GTK_WINDOW(banner.window));
	else
		gtk_window_unstick(GTK_WINDOW(banner.window));
	
	/* Scrolled window */
	banner.scrolled_win = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(banner.window), banner.scrolled_win);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(banner.scrolled_win),
				       GTK_POLICY_NEVER, GTK_POLICY_NEVER);

	/* Viewport */
	viewport = gtk_viewport_new(NULL,NULL);
	banner.viewport = viewport;
	gtk_container_add(GTK_CONTAINER(banner.scrolled_win),viewport);
	if(notify_config.banner_enable_colors) {
		GTKUT_GDKRGBA_TO_GDKCOLOR(notify_config.banner_color_bg,bg);
		gtk_widget_modify_bg(viewport,GTK_STATE_NORMAL,&bg);
	}

	/* Hbox */
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
	gtk_container_add(GTK_CONTAINER(viewport),hbox);

	/* Entrybox */
	entrybox = create_entrybox(msg_list);
	gtk_box_pack_start(GTK_BOX(hbox), entrybox, FALSE, FALSE, 0);
	
	gtk_widget_show_all(banner.window);
	
	/* Scrolling */
	gtk_widget_get_preferred_size(hbox, &requisition, NULL);

	if (notify_config.banner_width > 0)
		banner_width = notify_config.banner_width;
	else
		banner_width = gdk_screen_width();

	if(requisition.width > banner_width) {
		/* Line is too big for screen! */
		/* Double the entrybox into hbox */
		GtkWidget *separator, *second_entrybox;
	
		separator = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
		gtk_box_pack_start(GTK_BOX(hbox), separator,
				   FALSE, FALSE, 0);
		second_entrybox = create_entrybox(msg_list);
		gtk_box_pack_start(GTK_BOX(hbox), second_entrybox, FALSE, FALSE, 0);
        	
		gtk_widget_show_all(banner.window);
		gtk_widget_get_preferred_size(hbox, &requisition_after, NULL);
        	
		G_LOCK(sdata);
		sdata.banner_width = requisition_after.width - requisition.width;
		sdata.adj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW
						  (banner.scrolled_win));
		G_UNLOCK(sdata);
        	
		banner.scrolling = TRUE;
		if (banner.timeout_id) {
			g_source_remove(banner.timeout_id);
			banner.timeout_id = 0;
		}
		banner.timeout_id = g_timeout_add(notify_config.banner_speed, scroller,
						  NULL);
	} else {
		banner.scrolling = FALSE;
		if(banner.timeout_id) {
			g_source_remove(banner.timeout_id);
			banner.timeout_id = 0;
		}
		G_LOCK(sdata);
		sdata.banner_width = 0;
		sdata.adj = NULL;
		G_UNLOCK(sdata);
	}

	/* menu */
	banner_ui_manager = gtk_ui_manager_new();
	banner_action_group = cm_menu_create_action_group_full(banner_ui_manager,"BannerPopup",
							       banner_popup_entries,
								G_N_ELEMENTS(banner_popup_entries),
							       (gpointer)NULL);
	MENUITEM_ADDUI_MANAGER(banner_ui_manager, "/", "Menus", "Menus", GTK_UI_MANAGER_MENUBAR)
	MENUITEM_ADDUI_MANAGER(banner_ui_manager, "/Menus", "BannerPopup", "BannerPopup",
			       GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(banner_ui_manager, "/Menus/BannerPopup", "Reply", "BannerPopup/Reply",
			       GTK_UI_MANAGER_MENUITEM)
	
	banner_popup = gtk_menu_item_get_submenu(GTK_MENU_ITEM(
				gtk_ui_manager_get_widget(banner_ui_manager, "/Menus/BannerPopup")) );
	g_signal_connect(banner_popup,"selection-done", G_CALLBACK(notification_banner_popup_done),
			 NULL);
}

static gboolean notification_banner_configure(GtkWidget *widget,
					      GdkEventConfigure *event,
					      gpointer data)
{
	gtk_window_get_position(GTK_WINDOW(gtk_widget_get_toplevel(widget)),
					   &(notify_config.banner_root_x),
					   &(notify_config.banner_root_y));
	return TRUE;
}

static gboolean scroller(gpointer data)
{
	// don't scroll during popup open
	if(banner_popup_open)
		return banner.scrolling;

	while(gtk_events_pending())
		gtk_main_iteration();
	G_LOCK(sdata);
	if (sdata.adj && GTK_IS_ADJUSTMENT(sdata.adj)) {
		if (gtk_adjustment_get_value(sdata.adj) != sdata.banner_width)
			gtk_adjustment_set_value(sdata.adj,
						 gtk_adjustment_get_value(sdata.adj) + 1);
		else
			gtk_adjustment_set_value(sdata.adj, 0);
	}
	G_UNLOCK(sdata);
	while(gtk_events_pending())
		gtk_main_iteration();

	return banner.scrolling;
}

static GtkWidget* create_entrybox(GSList *msg_list)
{
	GtkWidget *entrybox;
	GdkColor fg;
	GdkColor bg;
	gint list_length;

	list_length = g_slist_length(msg_list);

	if (notify_config.banner_enable_colors) {
		GTKUT_GDKRGBA_TO_GDKCOLOR(notify_config.banner_color_bg,bg);
		GTKUT_GDKRGBA_TO_GDKCOLOR(notify_config.banner_color_fg,fg);
	}

	if (banner.entries) {
		g_free(banner.entries);
		banner.entries = NULL;
	}

	entrybox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
	if (list_length) {
		GSList *walk;
		gint ii = 0;
		banner.entries = g_new(NotificationBannerEntry, list_length);
		for (walk = msg_list; walk != NULL; walk = g_slist_next(walk)) {
			GtkWidget *label1;
			GtkWidget *label2;
			GtkWidget *label3;
			GtkWidget *label4;
			GtkWidget *label5;
			GtkWidget *label6;
			GtkWidget *ebox;
			CollectedMsg *cmsg = walk->data;
      
			if (ii > 0) {
				GtkWidget *separator;
				separator = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
				gtk_box_pack_start(GTK_BOX(entrybox), separator, FALSE, FALSE, 0);
			}

			ebox = gtk_event_box_new();
			gtk_box_pack_start(GTK_BOX(entrybox), ebox, FALSE, FALSE, 0);
			gtk_widget_set_events(ebox,
			GDK_POINTER_MOTION_MASK |
			GDK_BUTTON_PRESS_MASK);

			if (notify_config.banner_enable_colors)
				gtk_widget_modify_bg(ebox,GTK_STATE_NORMAL,&bg);

			banner.entries[ii].table = gtk_grid_new();
			gtk_container_add(GTK_CONTAINER(ebox),banner.entries[ii].table);
			g_signal_connect(ebox, "enter-notify-event",
					 	G_CALLBACK(notification_banner_swap_colors),
					 	banner.entries[ii].table);
			g_signal_connect(ebox, "leave-notify-event",
					 	G_CALLBACK(notification_banner_swap_colors),
					 	banner.entries[ii].table);
			g_signal_connect(ebox, "button-press-event",
					 	G_CALLBACK(notification_banner_button_press),
					 	cmsg);
	
			label1 = gtk_label_new(prefs_common_translated_header_name("From:"));
			gtk_label_set_xalign(GTK_LABEL(label1), 0.0);
			gtk_grid_attach(GTK_GRID(banner.entries[ii].table), label1, 0, 0, 1, 1);
			
			label2 = gtk_label_new(prefs_common_translated_header_name("Subject:"));
			gtk_label_set_xalign(GTK_LABEL(label2), 0.0);
			gtk_grid_attach(GTK_GRID(banner.entries[ii].table), label2, 0, 1, 1, 1);
			
			label3 = gtk_label_new(_("Folder:"));
			gtk_label_set_xalign(GTK_LABEL(label3), 0.0);
			gtk_grid_attach(GTK_GRID(banner.entries[ii].table), label3, 0, 2, 1, 1);
			
			label4 = gtk_label_new(cmsg->from);
			gtk_label_set_xalign(GTK_LABEL(label4), 0.0);
			gtk_grid_attach(GTK_GRID(banner.entries[ii].table), label4, 1, 0, 1, 1);
			
			label5 = gtk_label_new(cmsg->subject);
			gtk_label_set_xalign(GTK_LABEL(label5), 0.0);
			gtk_grid_attach(GTK_GRID(banner.entries[ii].table), label5, 1, 1, 1, 1);
			
			label6 = gtk_label_new(cmsg->folderitem_name);
			gtk_label_set_xalign(GTK_LABEL(label6), 0.0);
			gtk_grid_attach(GTK_GRID(banner.entries[ii].table), label6, 1, 2, 1, 1);
			
			gtk_grid_set_column_spacing(GTK_GRID(banner.entries[ii].table), 5);
			
			ii++;
			/* Color */
			if(notify_config.banner_enable_colors) {
				gtk_widget_modify_fg(label1,GTK_STATE_NORMAL,&fg);
				gtk_widget_modify_fg(label2,GTK_STATE_NORMAL,&fg);
				gtk_widget_modify_fg(label3,GTK_STATE_NORMAL,&fg);
				gtk_widget_modify_fg(label4,GTK_STATE_NORMAL,&fg);
				gtk_widget_modify_fg(label5,GTK_STATE_NORMAL,&fg);
				gtk_widget_modify_fg(label6,GTK_STATE_NORMAL,&fg);
			}
		}
  	} else {
		/* We have an empty list -- create an empty dummy element */
		GtkWidget *label;
		
		banner.entries = g_new(NotificationBannerEntry, 1);
		banner.entries[0].table = gtk_grid_new();
		label = gtk_label_new("");
		gtk_grid_attach(GTK_GRID(banner.entries[0].table), label, 0, 0, 1, 1);
		gtk_widget_set_hexpand(label, TRUE);
		gtk_widget_set_halign(label, GTK_ALIGN_FILL);
		label = gtk_label_new("");
		gtk_grid_attach(GTK_GRID(banner.entries[0].table), label, 0, 1, 1, 1);
		gtk_widget_set_hexpand(label, TRUE);
		gtk_widget_set_halign(label, GTK_ALIGN_FILL);
		label = gtk_label_new("");
		gtk_grid_attach(GTK_GRID(banner.entries[0].table), label, 0, 2, 1, 1);
		gtk_widget_set_hexpand(label, TRUE);
		gtk_widget_set_halign(label, GTK_ALIGN_FILL);
		gtk_box_pack_start(GTK_BOX(entrybox), banner.entries[0].table,
		       FALSE, FALSE, 0);
	}

	return entrybox;
}

static gboolean notification_banner_swap_colors(GtkWidget *widget,
						GdkEventCrossing *event,
						gpointer data)
{
	GList *children;
	GList *walk;
	GdkColor *old_bg;
	
	children = gtk_container_get_children(GTK_CONTAINER(data));

	old_bg = gdk_color_copy(&(gtk_widget_get_style(widget)->bg[GTK_STATE_NORMAL]));
	if (children)
		gtk_widget_modify_bg(widget,GTK_STATE_NORMAL,
				     &(gtk_widget_get_style(GTK_WIDGET(children->data))->fg[GTK_STATE_NORMAL]));
	
	for (walk = children; walk; walk = walk->next)
		gtk_widget_modify_fg(GTK_WIDGET(walk->data),GTK_STATE_NORMAL,old_bg);
	
	g_list_free(children);
	gdk_color_free(old_bg);
	return FALSE;
}

static gboolean notification_banner_button_press(GtkWidget *widget,
						 GdkEventButton *button,
						 gpointer data)
{
	gboolean done;
	done = FALSE;
	if (button->type == GDK_BUTTON_PRESS) {
		CollectedMsg *cmsg = (CollectedMsg*) data;
		if (button->button == 1) {
			/* jump to that message */
			if (cmsg->msginfo) {
				gchar *path;
				path = procmsg_get_message_file_path(cmsg->msginfo);
				mainwindow_jump_to(path, TRUE);
				g_free(path);
			}
			done = TRUE;
		} else if (button->button == 2) {
      			gtk_window_begin_move_drag(GTK_WINDOW(gtk_widget_get_toplevel(widget)), 
				 		button->button, button->x_root, button->y_root,
				 		button->time);
		} else if (button->button == 3) {
			current_msginfo = cmsg->msginfo;
			notification_banner_show_popup(widget, button);
			banner_popup_open = TRUE;
			done = TRUE;
		}
	}
	return done;
}

static void notification_banner_show_popup(GtkWidget *widget,
					   GdkEventButton *event)
{
  	gtk_menu_popup_at_pointer(GTK_MENU(banner_popup), NULL);
}

static void notification_banner_popup_done(GtkMenuShell *menushell,
					   gpointer user_data)
{
	current_msginfo = NULL;
	banner_popup_open = FALSE;
}

static void banner_menu_reply_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin;
	MessageView *messageview;
	GSList *msginfo_list = NULL;

	if(!(mainwin = mainwindow_get_mainwindow()))
		return;

	if(!(messageview = (MessageView*)mainwin->messageview))
		return;

	g_return_if_fail(current_msginfo);

	msginfo_list = g_slist_prepend(msginfo_list, current_msginfo);
	compose_reply_from_messageview(messageview, msginfo_list,
				       prefs_common_get_prefs()->reply_with_quote ?
				       COMPOSE_REPLY_WITH_QUOTE : COMPOSE_REPLY_WITHOUT_QUOTE);
	g_slist_free(msginfo_list);
}

#endif /* NOTIFICATION_BANNER */
