/* Notification plugin for Claws Mail
 * Copyright (C) 2005-2009 Holger Berndt and the Claws Mail Team.
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

#ifdef NOTIFICATION_INDICATOR

#include "notification_indicator.h"
#include "notification_prefs.h"
#include "notification_core.h"

#include "folder.h"
#include "common/utils.h"

#include <messaging-menu.h>
#include <unity.h>

#define CLAWS_DESKTOP_FILE "claws-mail.desktop"

static MessagingMenuApp *mmapp = NULL;
static gboolean mmapp_registered = FALSE;
static UnityLauncherEntry *launcher = NULL;
static gulong mainwin_state_changed_signal_id = 0;

static void show_claws_mail(MessagingMenuApp *mmapp, const gchar *id, gpointer data);

void notification_indicator_setup(void)
{
  if(!mmapp) {
    mmapp = messaging_menu_app_new(CLAWS_DESKTOP_FILE);
  }
  if(notify_config.indicator_enabled && !mmapp_registered) {
    messaging_menu_app_register(MESSAGING_MENU_APP(mmapp));
    g_signal_connect(mmapp, "activate-source", G_CALLBACK(show_claws_mail), NULL);
    mmapp_registered = TRUE;
  }
  if(!launcher) {
    launcher = unity_launcher_entry_get_for_desktop_id(CLAWS_DESKTOP_FILE);
  }
}

void notification_indicator_destroy(void)
{
  if(!launcher) {
    unity_launcher_entry_set_count_visible(launcher, FALSE);
  }
  if(mmapp_registered) {
    messaging_menu_app_unregister(mmapp);
    mmapp_registered = FALSE;
  }
  if(mainwin_state_changed_signal_id != 0) {
    MainWindow *mainwin;
    if((mainwin = mainwindow_get_mainwindow()) != NULL)
      g_signal_handler_disconnect(mainwin->window, mainwin_state_changed_signal_id);
    mainwin_state_changed_signal_id = 0;
  }
}

static void show_claws_mail(MessagingMenuApp *mmapp, const gchar *id, gpointer data)
{
  MainWindow *mainwin;

  if((mainwin = mainwindow_get_mainwindow()) == NULL)
    return;

  notification_show_mainwindow(mainwin);
  if(data) {
    Folder *folder = data;
    FolderItem *item = folder->inbox;

    gchar *path = folder_item_get_identifier(item);
    mainwindow_jump_to(path, FALSE);
    g_free(path);
  }
}

static gboolean mainwin_state_event(GtkWidget *widget, GdkEventWindowState *event, gpointer user_data)
{
  if(notify_config.indicator_hide_minimized) {
    MainWindow *mainwin;

    if((mainwin = mainwindow_get_mainwindow()) == NULL)
      return FALSE;

    if((event->changed_mask & GDK_WINDOW_STATE_ICONIFIED) && (event->new_window_state & GDK_WINDOW_STATE_ICONIFIED)) {
      gtk_window_set_skip_taskbar_hint(GTK_WINDOW(mainwin->window), TRUE);
    }
    else if((event->changed_mask & GDK_WINDOW_STATE_ICONIFIED) && !(event->new_window_state & GDK_WINDOW_STATE_ICONIFIED)) {
      gtk_window_set_skip_taskbar_hint(GTK_WINDOW(mainwin->window), FALSE);
    }
  }
  return FALSE;
}

void notification_update_indicator(void)
{
  GList *cur_mb;
  gint total_message_count;

  if(!mainwin_state_changed_signal_id) {
    MainWindow *mainwin;

    if((mainwin = mainwindow_get_mainwindow()) != NULL)
      mainwin_state_changed_signal_id = g_signal_connect(G_OBJECT(mainwin->window), "window-state-event", G_CALLBACK(mainwin_state_event), NULL);
  }

  if(!notify_config.indicator_enabled)
    return;

  total_message_count = 0;
  /* check accounts for new/unread counts */
  for(cur_mb = folder_get_list(); cur_mb; cur_mb = cur_mb->next) {
    Folder *folder = cur_mb->data;
    NotificationMsgCount count;

    if(!folder->name) {
      debug_print("Notification plugin: Warning: Ignoring unnamed mailbox in indicator applet\n");
      continue;
    }
    gchar *id = folder->name;
    notification_core_get_msg_count_of_foldername(folder->name, &count);

    total_message_count += count.unread_msgs;

    if(count.new_msgs > 0) {
      gchar *strcount = g_strdup_printf("%d / %d", count.new_msgs, count.unread_msgs);

      if(messaging_menu_app_has_source(MESSAGING_MENU_APP(mmapp), id))
        messaging_menu_app_set_source_string(MESSAGING_MENU_APP(mmapp), id, strcount);
      else
        messaging_menu_app_append_source_with_string(MESSAGING_MENU_APP(mmapp), id, NULL, id, strcount);

      g_free(strcount);
      messaging_menu_app_draw_attention(MESSAGING_MENU_APP(mmapp), id);
    }
    else {
      if(messaging_menu_app_has_source(MESSAGING_MENU_APP(mmapp), id)) {
        messaging_menu_app_remove_attention(MESSAGING_MENU_APP(mmapp), id);
        messaging_menu_app_remove_source(MESSAGING_MENU_APP(mmapp), id);
      }
    }
  }

  unity_launcher_entry_set_count(launcher, total_message_count);
  unity_launcher_entry_set_count_visible(launcher, total_message_count > 0);
}

#endif /* NOTIFICATION_INDICATOR */
