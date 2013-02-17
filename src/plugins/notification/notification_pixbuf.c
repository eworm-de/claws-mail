/* Notification plugin for Claws-Mail
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

#include "notification_pixbuf.h"

/* The following files were created from the respective .png or
 * xpm files with the command 
 * gdk-pixbuf-csource --raw --name=<name> file.png > <name>.h
 */
/* From the Claws-Mail distribution */
#include "raw_claws_mail_logo_64x64.h"

#include "stock_pixmap.h"

static GdkPixbuf* notification_pixbuf[NOTIFICATION_PIXBUF_LAST];

GdkPixbuf* notification_pixbuf_get(NotificationPixbuf wanted)
{
  if(!notification_pixbuf[wanted]) {
    switch(wanted) {
    case NOTIFICATION_CM_LOGO_64x64:
      notification_pixbuf[wanted] =
				gdk_pixbuf_new_from_inline(-1, raw_claws_mail_logo_64x64,
																	 FALSE, NULL);
      break;
    case NOTIFICATION_TRAYICON_NEWMAIL:
			stock_pixbuf_gdk(NULL, STOCK_PIXMAP_TRAY_NEWMAIL, &(notification_pixbuf[wanted]));
      g_object_ref(notification_pixbuf[wanted]);
      break;
    case NOTIFICATION_TRAYICON_NEWMAIL_OFFLINE:
			stock_pixbuf_gdk(NULL, STOCK_PIXMAP_TRAY_NEWMAIL_OFFLINE, &(notification_pixbuf[wanted]));
      g_object_ref(notification_pixbuf[wanted]);
      break;
    case NOTIFICATION_TRAYICON_NEWMARKEDMAIL:
			stock_pixbuf_gdk(NULL, STOCK_PIXMAP_TRAY_NEWMARKEDMAIL, &(notification_pixbuf[wanted]));
      g_object_ref(notification_pixbuf[wanted]);
      break;
    case NOTIFICATION_TRAYICON_NEWMARKEDMAIL_OFFLINE:
			stock_pixbuf_gdk(NULL, STOCK_PIXMAP_TRAY_NEWMARKEDMAIL_OFFLINE, &(notification_pixbuf[wanted]));
      g_object_ref(notification_pixbuf[wanted]);
      break;
    case NOTIFICATION_TRAYICON_NOMAIL:
			stock_pixbuf_gdk(NULL, STOCK_PIXMAP_TRAY_NOMAIL, &(notification_pixbuf[wanted]));
      g_object_ref(notification_pixbuf[wanted]);
      break;
    case NOTIFICATION_TRAYICON_NOMAIL_OFFLINE:
			stock_pixbuf_gdk(NULL, STOCK_PIXMAP_TRAY_NOMAIL_OFFLINE, &(notification_pixbuf[wanted]));
      g_object_ref(notification_pixbuf[wanted]);
      break;
    case NOTIFICATION_TRAYICON_UNREADMAIL:
			stock_pixbuf_gdk(NULL, STOCK_PIXMAP_TRAY_UNREADMAIL, &(notification_pixbuf[wanted]));
      g_object_ref(notification_pixbuf[wanted]);
      break;
    case NOTIFICATION_TRAYICON_UNREADMAIL_OFFLINE:
			stock_pixbuf_gdk(NULL, STOCK_PIXMAP_TRAY_UNREADMAIL_OFFLINE, &(notification_pixbuf[wanted]));
      g_object_ref(notification_pixbuf[wanted]);
      break;
    case NOTIFICATION_TRAYICON_UNREADMARKEDMAIL:
			stock_pixbuf_gdk(NULL, STOCK_PIXMAP_TRAY_UNREADMARKEDMAIL, &(notification_pixbuf[wanted]));
      g_object_ref(notification_pixbuf[wanted]);
      break;
    case NOTIFICATION_TRAYICON_UNREADMARKEDMAIL_OFFLINE:
			stock_pixbuf_gdk(NULL, STOCK_PIXMAP_TRAY_UNREADMARKEDMAIL_OFFLINE, &(notification_pixbuf[wanted]));
      g_object_ref(notification_pixbuf[wanted]);
      break;
    case NOTIFICATION_PIXBUF_LAST:
      break;
    }
  }
  return notification_pixbuf[wanted];
}

void notification_pixbuf_free_all(void)
{
  gint ii;

  for(ii = NOTIFICATION_CM_LOGO_64x64; ii < NOTIFICATION_PIXBUF_LAST; ii++) {
    if(notification_pixbuf[ii]) {
      g_object_unref(notification_pixbuf[ii]);
      notification_pixbuf[ii] = NULL;
    }
  }
}
