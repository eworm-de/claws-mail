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

#ifndef NOTIFICATION_FOLDERCHECK_H
#define NOTIFICATION_FOLDERCHECK_H NOTIFICATION_FOLDERCHECK_H

#include <gtk/gtk.h>

#ifdef HAVE_CONFIG_H
#  include "claws-features.h"
#endif

void     notification_foldercheck_sel_folders_cb(GtkButton*, gpointer);
guint    notification_register_folder_specific_list(gchar*);
GSList*  notification_foldercheck_get_list(guint);
void     notification_free_folder_specific_array(void);
void     notification_foldercheck_write_array(void);
gboolean notification_foldercheck_read_array(void);

#endif /* NOTIFICATION_FOLDERCHECK_H */
