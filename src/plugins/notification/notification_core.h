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

#ifndef NOTIFICATION_CORE_H
#define NOTIFICATION_CORE_H NOTIFICATION_CORE_H

#include "mainwindow.h"
#include "folder.h"
#include "procmsg.h"

typedef struct {
  gchar *from;
  gchar *subject;
  FolderItem *folder_item;
  gchar *folderitem_name;
	MsgInfo *msginfo;
} CollectedMsg;

typedef enum {
  F_TYPE_MAIL=0,
  F_TYPE_NEWS,
  F_TYPE_CALENDAR,
  F_TYPE_RSS,
  F_TYPE_LAST
} NotificationFolderType;

typedef struct 
{
  guint new_msgs;
  guint unread_msgs;
  guint unreadmarked_msgs;
  guint marked_msgs;
  guint total_msgs;
} NotificationMsgCount;

/* Collect new and possibly unread messages in all folders */
GSList*  notification_collect_msgs(gboolean, GSList*, gint);
void     notification_collected_msgs_free(GSList*);

void     notification_core_global_includes_changed(void);
void     notification_core_free(void);
void     notification_update_msg_counts(FolderItem*);
void     notification_core_get_msg_count(GSList*,NotificationMsgCount*);
void     notification_core_get_msg_count_of_foldername(gchar*, NotificationMsgCount*);
void     notification_new_unnotified_msgs(FolderItemUpdateData*);
gboolean notification_notified_hash_msginfo_update(MsgInfoUpdate*);
void     notification_notified_hash_startup_init(void);

void     notification_show_mainwindow(MainWindow *mainwin);
/* folder type specific settings */
gboolean notify_include_folder_type(FolderType, gchar*);

void notification_toggle_hide_show_window(void);

#ifdef HAVE_LIBNOTIFY
/* Sanitize a string to use with libnotify. Returns a freshly
 * allocated string that must be freed by the user. */
gchar* notification_libnotify_sanitize_str(gchar*);
/* Returns a freshly allocated copy of the input string, which
 * is guaranteed to be valid UTF8. The returned string has to
 * be freed. */
gchar* notification_validate_utf8_str(gchar*);
#endif

#endif /* NOTIFICATION_CORE_H */
