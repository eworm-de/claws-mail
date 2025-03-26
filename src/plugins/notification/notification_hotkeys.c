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

#ifdef NOTIFICATION_HOTKEYS

#include "notification_hotkeys.h"
#include "notification_prefs.h"
#include "notification_core.h"

#include "gtkhotkey.h"

#define HOTKEYS_APP_ID "claws-mail"
#define HOTKEY_KEY_ID_TOGGLED "toggle-mainwindow"

static GtkHotkeyInfo *hotkey_toggle_mainwindow = NULL;

static void hotkey_toggle_mainwindow_activated(GtkHotkeyInfo *hotkey, guint event_time, gpointer data)
{
  g_return_if_fail(GTK_HOTKEY_IS_INFO(hotkey));
  debug_print("Notification plugin: Toggled hide/show window due to hotkey %s activation\n", gtk_hotkey_info_get_signature(hotkey));
  notification_toggle_hide_show_window();
}

static void unbind_toggle_mainwindow()
{
  GError *error = NULL;
  GtkHotkeyRegistry *registry;

  /* clean up old hotkey */
  if(hotkey_toggle_mainwindow) {
    if(gtk_hotkey_info_is_bound(hotkey_toggle_mainwindow)) {
      error = NULL;
      gtk_hotkey_info_unbind(hotkey_toggle_mainwindow, &error);
      if(error) {
        debug_print("Notification plugin: Failed to unbind toggle hotkey\n");
        g_error_free(error);
        return;
      }
    }
    g_object_unref(hotkey_toggle_mainwindow);
    hotkey_toggle_mainwindow = NULL;
  }
  registry = gtk_hotkey_registry_get_default();
  if(gtk_hotkey_registry_has_hotkey(registry, HOTKEYS_APP_ID, HOTKEY_KEY_ID_TOGGLED)) {
    error = NULL;
    gtk_hotkey_registry_delete_hotkey(registry, HOTKEYS_APP_ID, HOTKEY_KEY_ID_TOGGLED, &error);
    if(error) {
      debug_print("Notification plugin: Failed to unregister toggle hotkey: %s\n", error->message);
      g_error_free(error);
      return;
    }
  }
}

static void update_hotkey_binding_toggle_mainwindow()
{
  GError *error = NULL;

  /* don't do anything if no signature is given */
  if(!notify_config.hotkeys_toggle_mainwindow || !strcmp(notify_config.hotkeys_toggle_mainwindow, ""))
    return;

  unbind_toggle_mainwindow();

  /* (re)create hotkey info */
  hotkey_toggle_mainwindow = gtk_hotkey_info_new(HOTKEYS_APP_ID, HOTKEY_KEY_ID_TOGGLED, notify_config.hotkeys_toggle_mainwindow, NULL);
  if(!hotkey_toggle_mainwindow) {
    debug_print("Notification plugin: Failed to create toggle hotkey for '%s'\n", notify_config.hotkeys_toggle_mainwindow);
    return;
  }

  /* try to register hotkey */
  error = NULL;
  gtk_hotkey_info_bind(hotkey_toggle_mainwindow, &error);
  if(error) {
    debug_print("Notification plugin: Failed to bind toggle hotkey to '%s': %s\n", notify_config.hotkeys_toggle_mainwindow, error->message);
    g_error_free(error);
    return;
  }

  g_signal_connect(hotkey_toggle_mainwindow, "activated", G_CALLBACK(hotkey_toggle_mainwindow_activated), NULL);
}

void notification_hotkeys_update_bindings()
{
  debug_print("Notification plugin: Updating keybindings..\n");
  if(notify_config.hotkeys_enabled) {
    update_hotkey_binding_toggle_mainwindow();
  }
  else
    notification_hotkeys_unbind_all();
}

void notification_hotkeys_unbind_all()
{
  debug_print("Notification plugin: Unbinding all keybindings..\n");
  unbind_toggle_mainwindow();
}

#endif /* NOTIFICATION_HOTKEYS */
