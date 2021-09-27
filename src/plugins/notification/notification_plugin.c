/* Notification plugin for Claws Mail
 * Copyright (C) 2005-2007 Holger Berndt
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

#include "common/claws.h"
#include "common/version.h"
#include "common/utils.h"
#include "common/defs.h"
#include "folder.h"
#include "common/hooks.h"
#include "procmsg.h"
#include "mainwindow.h"
#include "main.h"
#include "gtk/gtkutils.h"

#include "notification_plugin.h"
#include "notification_core.h"
#include "notification_prefs.h"
#include "notification_banner.h"
#include "notification_lcdproc.h"
#include "notification_trayicon.h"
#include "notification_indicator.h"
#include "notification_hotkeys.h"
#include "notification_foldercheck.h"
#include "notification_pixbuf.h"
#include "plugin.h"

#if HAVE_LIBNOTIFY
#  include <libnotify/notify.h>
#endif

/* delay in ms until the tray icon shall be embedded in the
   system tray before it is assumed that the tray doesn't
   work and the main window is shown again */
#define CM_NOTIFICATION_TRAYICON_SAFETY_NET_DELAY_MS 5000

static gboolean my_folder_item_update_hook(gpointer, gpointer);
static gboolean my_folder_update_hook(gpointer, gpointer);
static gboolean my_msginfo_update_hook(gpointer, gpointer);
static gboolean my_offline_switch_hook(gpointer, gpointer);
static gboolean my_main_window_close_hook(gpointer, gpointer);
static gboolean my_main_window_got_iconified_hook(gpointer, gpointer);
static gboolean my_account_list_changed_hook(gpointer, gpointer);
static gboolean my_update_theme_hook(gpointer, gpointer);

#ifdef NOTIFICATION_TRAYICON
static gboolean trayicon_startup_idle(gpointer);
#endif

static gulong hook_f_item;
static gulong hook_f;
static gulong hook_m_info;
static gulong hook_offline;
static gulong hook_mw_close;
static gulong hook_got_iconified;
static gulong hook_account;
static gulong hook_theme_changed;

#ifdef NOTIFICATION_BANNER
static GSList* banner_collected_msgs;
#endif

void notification_update_urgency_hint(void)
{
  MainWindow *mainwin;
  mainwin = mainwindow_get_mainwindow();
  if(mainwin) {
    NotificationMsgCount count;
    gboolean active;
    active = FALSE;
    if(notify_config.urgency_hint_new || notify_config.urgency_hint_unread) {
      notification_core_get_msg_count(NULL, &count);
      if(notify_config.urgency_hint_new)
        active = (active || (count.new_msgs > 0));
      if(notify_config.urgency_hint_unread)
        active = (active || (count.unread_msgs > 0));
    }
    gtk_window_set_urgency_hint(GTK_WINDOW(mainwin->window), active);
  }
}

static gboolean my_account_list_changed_hook(gpointer source,
					     gpointer data)
{
  gboolean retVal = FALSE;

#ifdef NOTIFICATION_TRAYICON
  notification_update_msg_counts(NULL);
  retVal = notification_trayicon_account_list_changed(source, data);

#endif
  return retVal;
}

static gboolean my_update_theme_hook(gpointer source, gpointer data)
{
  notification_pixbuf_free_all();
#ifdef NOTIFICATION_TRAYICON
  notification_update_trayicon();
#endif
  return FALSE;
}

static gboolean my_main_window_got_iconified_hook(gpointer source,
						  gpointer data)
{
  gboolean retVal = FALSE;
#ifdef NOTIFICATION_TRAYICON
  notification_update_msg_counts(NULL);
  retVal = notification_trayicon_main_window_got_iconified(source, data);
#endif
  return retVal;
}

static gboolean my_main_window_close_hook(gpointer source, gpointer data)
{
  gboolean retVal = FALSE;
#ifdef NOTIFICATION_TRAYICON
  notification_update_msg_counts(NULL);
  retVal = notification_trayicon_main_window_close(source, data);
#endif
  return retVal;
}

static gboolean my_offline_switch_hook(gpointer source, gpointer data)
{
#ifdef NOTIFICATION_TRAYICON
  notification_update_msg_counts(NULL);
#endif
  return FALSE;
}

static gboolean my_folder_item_update_hook(gpointer source, gpointer data)
{
  FolderItemUpdateData *update_data = source;
  FolderType ftype;
  gchar *uistr;

  g_return_val_if_fail(source != NULL, FALSE);

  if (folder_has_parent_of_type(update_data->item, F_DRAFT))
      return FALSE;

#if defined(NOTIFICATION_LCDPROC) || defined(NOTIFICATION_TRAYICON) || defined(NOTIFICATION_INDICATOR)
    notification_update_msg_counts(NULL);
#else
    if(notify_config.urgency_hint_new || notify_config.urgency_hint_unread)
    	notification_update_msg_counts(NULL);
#endif

  /* Check if the folder types is to be notified about */
  ftype = update_data->item->folder->klass->type;
  uistr = update_data->item->folder->klass->uistr;
  if(!notify_include_folder_type(ftype, uistr))
    return FALSE;

  if(update_data->update_flags & F_ITEM_UPDATE_MSGCNT) {
#ifdef NOTIFICATION_BANNER
    notification_update_banner();
#endif
#if defined(NOTIFICATION_POPUP) || defined(NOTIFICATION_COMMAND)
    notification_new_unnotified_msgs(update_data);
#endif
  }
  return FALSE;
}

static gboolean my_folder_update_hook(gpointer source, gpointer data)
{
  FolderUpdateData *hookdata;

  g_return_val_if_fail(source != NULL, FALSE);
  hookdata = source;

#if defined(NOTIFICATION_LCDPROC) || defined(NOTIFICATION_TRAYICON)
  if(hookdata->update_flags & FOLDER_REMOVE_FOLDERITEM)
    notification_update_msg_counts(hookdata->item);
  else
    notification_update_msg_counts(NULL);
#endif

  return FALSE;

}


static gboolean my_msginfo_update_hook(gpointer source, gpointer data)
{
  return notification_notified_hash_msginfo_update((MsgInfoUpdate*)source);
}

gint plugin_init(gchar **error)
{
  gchar *rcpath;

  /* Version check */
  /* No be able to test against new-contacts */
  if(!check_plugin_version(MAKE_NUMERIC_VERSION(3,8,1,46),
			   VERSION_NUMERIC, _("Notification"), error))
    return -1;

  hook_f_item = hooks_register_hook(FOLDER_ITEM_UPDATE_HOOKLIST,
				    my_folder_item_update_hook, NULL);
  if(hook_f_item == 0) {
    *error = g_strdup(_("Failed to register folder item update hook in the "
			"Notification plugin"));
    return -1;
  }

  hook_f = hooks_register_hook(FOLDER_UPDATE_HOOKLIST,
			       my_folder_update_hook, NULL);
  if(hook_f == 0) {
    *error = g_strdup(_("Failed to register folder update hook in the "
			"Notification plugin"));
    hooks_unregister_hook(FOLDER_ITEM_UPDATE_HOOKLIST, hook_f_item);
    return -1;
  }


  hook_m_info = hooks_register_hook(MSGINFO_UPDATE_HOOKLIST,
				    my_msginfo_update_hook, NULL);
  if(hook_m_info == 0) {
    *error = g_strdup(_("Failed to register msginfo update hook in the "
			"Notification plugin"));
    hooks_unregister_hook(FOLDER_ITEM_UPDATE_HOOKLIST, hook_f_item);
    hooks_unregister_hook(FOLDER_UPDATE_HOOKLIST, hook_f);
    return -1;
  }

  hook_offline = hooks_register_hook(OFFLINE_SWITCH_HOOKLIST,
				     my_offline_switch_hook, NULL);
  if(hook_offline == 0) {
    *error = g_strdup(_("Failed to register offline switch hook in the "
			"Notification plugin"));
    hooks_unregister_hook(FOLDER_ITEM_UPDATE_HOOKLIST, hook_f_item);
    hooks_unregister_hook(FOLDER_UPDATE_HOOKLIST, hook_f);
    hooks_unregister_hook(MSGINFO_UPDATE_HOOKLIST, hook_m_info);
    return -1;
  }

  hook_mw_close = hooks_register_hook(MAIN_WINDOW_CLOSE,
				      my_main_window_close_hook, NULL);
  if(hook_mw_close == 0) {
    *error = g_strdup(_("Failed to register main window close hook in the "
			"Notification plugin"));
    hooks_unregister_hook(FOLDER_ITEM_UPDATE_HOOKLIST, hook_f_item);
    hooks_unregister_hook(FOLDER_UPDATE_HOOKLIST, hook_f);
    hooks_unregister_hook(MSGINFO_UPDATE_HOOKLIST, hook_m_info);
    hooks_unregister_hook(OFFLINE_SWITCH_HOOKLIST, hook_offline);
    return -1;
  }

  hook_got_iconified = hooks_register_hook(MAIN_WINDOW_GOT_ICONIFIED,
					   my_main_window_got_iconified_hook,
					   NULL);
  if(hook_got_iconified == 0) {
    *error = g_strdup(_("Failed to register got iconified hook in the "
			"Notification plugin"));
    hooks_unregister_hook(FOLDER_ITEM_UPDATE_HOOKLIST, hook_f_item);
    hooks_unregister_hook(FOLDER_UPDATE_HOOKLIST, hook_f);
    hooks_unregister_hook(MSGINFO_UPDATE_HOOKLIST, hook_m_info);
    hooks_unregister_hook(OFFLINE_SWITCH_HOOKLIST, hook_offline);
    hooks_unregister_hook(MAIN_WINDOW_CLOSE, hook_mw_close);
    return -1;
  }

  hook_account = hooks_register_hook(ACCOUNT_LIST_CHANGED_HOOKLIST,
				     my_account_list_changed_hook, NULL);
  if (hook_account == 0) {
    *error = g_strdup(_("Failed to register account list changed hook in the "
			"Notification plugin"));
    hooks_unregister_hook(FOLDER_ITEM_UPDATE_HOOKLIST, hook_f_item);
    hooks_unregister_hook(FOLDER_UPDATE_HOOKLIST, hook_f);
    hooks_unregister_hook(MSGINFO_UPDATE_HOOKLIST, hook_m_info);
    hooks_unregister_hook(OFFLINE_SWITCH_HOOKLIST, hook_offline);
    hooks_unregister_hook(MAIN_WINDOW_CLOSE, hook_mw_close);
    hooks_unregister_hook(MAIN_WINDOW_GOT_ICONIFIED, hook_got_iconified);
    return -1;
  }

  hook_theme_changed = hooks_register_hook(THEME_CHANGED_HOOKLIST, my_update_theme_hook, NULL);
  if(hook_theme_changed == 0) {
    *error = g_strdup(_("Failed to register theme change hook in the "
      "Notification plugin"));
    hooks_unregister_hook(FOLDER_ITEM_UPDATE_HOOKLIST, hook_f_item);
    hooks_unregister_hook(FOLDER_UPDATE_HOOKLIST, hook_f);
    hooks_unregister_hook(MSGINFO_UPDATE_HOOKLIST, hook_m_info);
    hooks_unregister_hook(OFFLINE_SWITCH_HOOKLIST, hook_offline);
    hooks_unregister_hook(MAIN_WINDOW_CLOSE, hook_mw_close);
    hooks_unregister_hook(MAIN_WINDOW_GOT_ICONIFIED, hook_got_iconified);
    hooks_unregister_hook(ACCOUNT_LIST_CHANGED_HOOKLIST, hook_account);
    return -1;
  }

  /* Configuration */
  prefs_set_default(notify_param);
  rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMON_RC, NULL);
  prefs_read_config(notify_param, "NotificationPlugin", rcpath, NULL);
  g_free(rcpath);

  /* Folder specific stuff */
  notification_foldercheck_read_array();

  notification_notified_hash_startup_init();

  notify_gtk_init();

#ifdef NOTIFICATION_INDICATOR
  notification_indicator_setup();
#endif
#ifdef NOTIFICATION_BANNER
  notification_update_banner();
#endif
#ifdef NOTIFICATION_LCDPROC
  notification_lcdproc_connect();
#endif
#ifdef NOTIFICATION_TRAYICON
  if(notify_config.trayicon_enabled &&
		 notify_config.trayicon_hide_at_startup && claws_is_starting()) {
    MainWindow *mainwin = mainwindow_get_mainwindow();

		g_timeout_add(CM_NOTIFICATION_TRAYICON_SAFETY_NET_DELAY_MS, trayicon_startup_idle,NULL);
    if(mainwin && gtk_widget_get_visible(GTK_WIDGET(mainwin->window)))
      main_window_hide(mainwin);
    main_set_show_at_startup(FALSE);
  }
#endif
  my_account_list_changed_hook(NULL,NULL);

  if(notify_config.urgency_hint_new || notify_config.urgency_hint_unread)
	notification_update_msg_counts(NULL);

#ifdef NOTIFICATION_HOTKEYS
  notification_hotkeys_update_bindings();
#endif

  debug_print("Notification plugin loaded\n");

  return 0;
}

gboolean plugin_done(void)
{
  hooks_unregister_hook(FOLDER_ITEM_UPDATE_HOOKLIST, hook_f_item);
  hooks_unregister_hook(FOLDER_UPDATE_HOOKLIST, hook_f);
  hooks_unregister_hook(MSGINFO_UPDATE_HOOKLIST, hook_m_info);
  hooks_unregister_hook(OFFLINE_SWITCH_HOOKLIST, hook_offline);
  hooks_unregister_hook(MAIN_WINDOW_CLOSE, hook_mw_close);
  hooks_unregister_hook(MAIN_WINDOW_GOT_ICONIFIED, hook_got_iconified);
  hooks_unregister_hook(ACCOUNT_LIST_CHANGED_HOOKLIST, hook_account);
  hooks_unregister_hook(THEME_CHANGED_HOOKLIST, hook_theme_changed);

  notify_save_config();

  notify_gtk_done();

  /* foldercheck cleanup */
  notification_foldercheck_write_array();
  notification_free_folder_specific_array();

#ifdef NOTIFICATION_BANNER
  notification_collected_msgs_free(banner_collected_msgs);
  banner_collected_msgs = NULL;
  notification_banner_destroy();
#endif
#ifdef NOTIFICATION_LCDPROC
  notification_lcdproc_disconnect();
#endif
#ifdef NOTIFICATION_TRAYICON
  notification_trayicon_destroy();
#endif
#ifdef NOTIFICATION_INDICATOR
  notification_indicator_destroy();
#endif
  notification_core_free();

#ifdef HAVE_LIBNOTIFY
  if(notify_is_initted())
    notify_uninit();
#endif

#ifdef NOTIFICATION_HOTKEYS
  notification_hotkeys_unbind_all();
#endif

  notification_pixbuf_free_all();

  debug_print("Notification plugin unloaded\n");

  /* Returning FALSE here means that g_module_close() will not be called on the plugin.
   * This is necessary, as needed libraries are not designed to be unloaded. */
  return FALSE;
}

const gchar *plugin_name(void)
{
  return _("Notification");
}

const gchar *plugin_desc(void)
{
  return _("This plugin provides various ways "
    "to notify the user of new and unread email.\n"
    "The plugin is extensively configurable in the "
    "plugins section of the preferences dialog.\n\n"
    "Feedback to <berndth@gmx.de> is welcome.");
}

const gchar *plugin_type(void)
{
  return "GTK3";
}

const gchar *plugin_licence(void)
{
  return "GPL3+";
}

const gchar *plugin_version(void)
{
  return VERSION;
}

struct PluginFeature *plugin_provides(void)
{
  static struct PluginFeature features[] =
    { {PLUGIN_NOTIFIER, N_("Various tools")},
      {PLUGIN_NOTHING, NULL}};
  return features;
}

#ifdef NOTIFICATION_BANNER
void notification_update_banner(void)
{
  notification_collected_msgs_free(banner_collected_msgs);
  banner_collected_msgs = NULL;

  if(notify_config.banner_show != NOTIFY_BANNER_SHOW_NEVER) {
    guint id;
    GSList *folder_list = NULL;

    if(notify_config.banner_folder_specific) {
      id = notification_register_folder_specific_list
				(BANNER_SPECIFIC_FOLDER_ID_STR);
      folder_list = notification_foldercheck_get_list(id);
    }

    if(!(notify_config.banner_folder_specific && (folder_list == NULL)))
      banner_collected_msgs =
				notification_collect_msgs(notify_config.banner_include_unread,
																	notify_config.banner_folder_specific ?
																	folder_list : NULL, notify_config.banner_max_msgs);
  }

  notification_banner_show(banner_collected_msgs);
}
#endif

#ifdef NOTIFICATION_TRAYICON
static gboolean trayicon_startup_idle(gpointer data)
{
	/* if the trayicon is not available,
		 simulate click on it to show mainwindow */
	if(!notification_trayicon_is_available())
		notification_trayicon_on_activate(NULL,data);
	return FALSE;
}
#endif
