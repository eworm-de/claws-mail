/* Notification plugin for Claws Mail
 * Copyright (C) 2005-2007 Holger Berndt and the Claws Mail Team.
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

#ifndef NOTIFICATION_TRAYICON_H
#define NOTIFICATION_TRAYICON_H NOTIFICATION_TRAYICON_H

#ifdef HAVE_CONFIG_H
#  include "claws-features.h"
#endif

#ifdef NOTIFICATION_TRAYICON

#include <glib.h>

#include "procmsg.h"

#define TRAYICON_SPECIFIC_FOLDER_ID_STR "trayicon"

void notification_trayicon_msg(MsgInfo*);

void notification_trayicon_destroy(void);

/* creates it, if necessary */
void notification_update_trayicon(void);

gboolean notification_trayicon_main_window_close(gpointer, gpointer);
gboolean notification_trayicon_main_window_got_iconified(gpointer, gpointer);
gboolean notification_trayicon_account_list_changed(gpointer, gpointer);

gboolean notification_trayicon_is_available(void);
void notification_trayicon_on_activate(GtkStatusIcon*,gpointer);

#endif /* NOTIFICATION_TRAYICON */

#endif /* include guard */
