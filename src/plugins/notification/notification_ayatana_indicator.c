/* Notification plugin for Claws Mail
 * Copyright (C) 2005-2024 Holger Berndt, Jan Willamowius and the Claws Mail Team.
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

#ifdef NOTIFICATION_AYATANA_INDICATOR

#include "notification_indicator.h"
#include "notification_prefs.h"
#include "notification_core.h"

//#include "folder.h"
#include "common/utils.h"

#include "libayatana-appindicator/app-indicator.h"

static AppIndicator *ayatana_indicator = NULL;
static GtkWidget *status = NULL;

void notification_ayatana_indicator_enable(void)
{
  if(!notify_config.ayatana_indicator_enabled)
    return;

  if (!ayatana_indicator) {
    ayatana_indicator = app_indicator_new ("claws-mail", "mail-read-symbolic", APP_INDICATOR_CATEGORY_COMMUNICATIONS);
    app_indicator_set_attention_icon_full(ayatana_indicator, "mail-read-symbolic", "");

    // MUST have menu, otherwise indicator won't show up
    GtkWidget *menu = gtk_menu_new ();
    status = gtk_menu_item_new_with_label (""); /* filled later */
    g_signal_connect(G_OBJECT(status), "activate", G_CALLBACK(notification_toggle_hide_show_window), NULL);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), status);
    gtk_widget_show (status);
    GtkWidget *quit = gtk_menu_item_new_with_label (_("Quit Claws Mail"));
    g_signal_connect (quit, "activate", G_CALLBACK (gtk_main_quit), ayatana_indicator);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), quit);
    gtk_widget_show (quit);
    app_indicator_set_menu (ayatana_indicator, GTK_MENU (menu));
  }
  app_indicator_set_status (ayatana_indicator, APP_INDICATOR_STATUS_ACTIVE);
}

void notification_ayatana_indicator_disable(void)
{
  if (ayatana_indicator) {
    // hide the indicator, don't destroy it, so it can be activated again later
    app_indicator_set_status (ayatana_indicator, APP_INDICATOR_STATUS_PASSIVE);
  }
}

void notification_update_ayatana_indicator(void)
{
  NotificationMsgCount count;
  GSList *list = NULL;

  if(!notify_config.ayatana_indicator_enabled || !ayatana_indicator)
    return;

  notification_core_get_msg_count(list, &count);

  gchar * buf = g_strdup_printf(_("New %d, Unread: %d, Total: %d"),
			count.new_msgs, count.unread_msgs,
			count.total_msgs);

  gtk_menu_item_set_label(GTK_MENU_ITEM(status), buf);
  if (count.new_msgs > 0) {
    app_indicator_set_icon_full(ayatana_indicator, "indicator-messages-new", "");
  } else if (count.unread_msgs > 0) {
    app_indicator_set_icon_full(ayatana_indicator, "mail-unread-symbolic", "");
  } else {
    app_indicator_set_icon_full(ayatana_indicator, "mail-read-symbolic", "");
  }
  g_free(buf);
}

#endif /* NOTIFICATION_AYATANA_INDICATOR */
