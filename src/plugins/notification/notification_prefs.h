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

#ifndef NOTIFICATION_PREFS_H
#define NOTIFICATION_PREFS_H NOTIFICATION_PREFS_H

#ifdef HAVE_CONFIG_H
#  include "claws-features.h"
#endif

#include <glib.h>

#include "prefs_gtk.h"

#include "notification_banner.h"

#ifdef NOTIFICATION_BANNER
typedef enum {
  NOTIFY_BANNER_SHOW_NEVER = 0,
  NOTIFY_BANNER_SHOW_ALWAYS,
  NOTIFY_BANNER_SHOW_NONEMPTY
} NotifyBannerShow;
#endif

typedef struct {
  gboolean         include_mail;
  gboolean         include_news;
  gboolean         include_rss;
  gboolean         include_calendar;
  gboolean         urgency_hint_new;
  gboolean         urgency_hint_unread;
#ifdef HAVE_LIBCANBERRA_GTK
  gboolean         canberra_play_sounds;
#endif
#ifdef NOTIFICATION_BANNER
  NotifyBannerShow banner_show;
  gint             banner_speed;
  gboolean         banner_include_unread;
  gint             banner_max_msgs;
  gboolean         banner_sticky;
  gint             banner_root_x;
  gint             banner_root_y;
  gboolean         banner_folder_specific;
  gboolean         banner_enable_colors;
  GdkRGBA          banner_color_bg;
  GdkRGBA          banner_color_fg;
	gint             banner_width;
#endif
#ifdef NOTIFICATION_POPUP
  gboolean         popup_show;
  gint             popup_timeout;
  gboolean         popup_folder_specific;
#ifndef HAVE_LIBNOTIFY
  gboolean         popup_sticky;
  gint             popup_root_x;
  gint             popup_root_y;
  gint             popup_width;
  gboolean         popup_enable_colors;
  GdkRGBA          popup_color_bg;
  GdkRGBA          popup_color_fg;
#else /* HAVE_LIBNOTIFY */
  gboolean         popup_display_folder_name;
#endif /* HAVE_LIBNOTIFY */
#endif /* Popup */
#ifdef NOTIFICATION_COMMAND
  gboolean         command_enabled;
  gint             command_timeout;
  gboolean         command_folder_specific;
  gchar*           command_line;
#endif
#ifdef NOTIFICATION_LCDPROC
  gboolean         lcdproc_enabled;
  gchar*           lcdproc_hostname;
  guint 	   lcdproc_port;

#endif
#ifdef NOTIFICATION_TRAYICON
  gboolean         trayicon_enabled;
  gboolean         trayicon_hide_at_startup;
  gboolean         trayicon_close_to_tray;
  gboolean         trayicon_hide_when_iconified;
  gboolean         trayicon_folder_specific;
#ifdef HAVE_LIBNOTIFY
  gboolean         trayicon_display_folder_name;
  gboolean         trayicon_popup_enabled;
  gint             trayicon_popup_timeout;
#endif /* HAVE_LIBNOTIFY */
#endif /* Trayicon */
#ifdef NOTIFICATION_INDICATOR
  gboolean indicator_enabled;
  gboolean indicator_hide_minimized;
#endif /* NOTIFICATION_INDICATOR */
#ifdef NOTIFICATION_HOTKEYS
  gboolean hotkeys_enabled;
  gchar* hotkeys_toggle_mainwindow;
#endif /* Hotkeys */
} NotifyPrefs;

extern NotifyPrefs notify_config;
extern PrefParam   notify_param[];

void notify_gtk_init(void);
void notify_gtk_done(void);
void notify_save_config(void);

#endif /* NOTIFICATION_PREFS_H */
