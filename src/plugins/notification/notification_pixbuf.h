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

#ifndef NOTIFICATION_PIXBUF_H
#define NOTIFICATION_PIXBUF_H NOTIFICATION_PIXBUF_H

#include <gdk/gdk.h>

/* By convention, the online icons for the trayicon are 
 * immediately followed by the offline icons, so one can do
 * offline = array[online+1] */
typedef enum {
  NOTIFICATION_CM_LOGO_64x64 = 0,
  NOTIFICATION_TRAYICON_NEWMAIL,
  NOTIFICATION_TRAYICON_NEWMAIL_OFFLINE,
  NOTIFICATION_TRAYICON_NEWMARKEDMAIL,
  NOTIFICATION_TRAYICON_NEWMARKEDMAIL_OFFLINE,
  NOTIFICATION_TRAYICON_NOMAIL,
  NOTIFICATION_TRAYICON_NOMAIL_OFFLINE,
  NOTIFICATION_TRAYICON_UNREADMAIL,
  NOTIFICATION_TRAYICON_UNREADMAIL_OFFLINE,
  NOTIFICATION_TRAYICON_UNREADMARKEDMAIL,
  NOTIFICATION_TRAYICON_UNREADMARKEDMAIL_OFFLINE,
  NOTIFICATION_PIXBUF_LAST
} NotificationPixbuf;

/* The reference to the returned GdkPixbuf's belongs to notification_pixbuf,
 * and shall not be removed outside. */
GdkPixbuf* notification_pixbuf_get(NotificationPixbuf);

void notification_pixbuf_free_all(void);

#endif /* NOTIFICATION_PIXBUF_H */
