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

#ifdef NOTIFICATION_COMMAND

#include <string.h>
#include <glib.h>
#include "common/utils.h"
#include "folder.h"
#include "notification_command.h"
#include "notification_prefs.h"
#include "notification_foldercheck.h"

typedef struct {
  gboolean blocked;
  guint timeout_id;
} NotificationCommand;

static gboolean command_timeout_fun(gpointer data);

static NotificationCommand command;

G_LOCK_DEFINE_STATIC(command);

void notification_command_msg(MsgInfo *msginfo)
{
  gchar *ret_str, *buf;
  gsize by_read = 0, by_written = 0;

  if(!msginfo || !notify_config.command_enabled || !MSG_IS_NEW(msginfo->flags))
    return;

  if(!notify_config.command_enabled || !MSG_IS_NEW(msginfo->flags))
    return;

  if(notify_config.command_folder_specific) {
    guint id;
    GSList *list;
    gchar *identifier;
    gboolean found = FALSE;

    if(!(msginfo->folder))
      return;

    identifier = folder_item_get_identifier(msginfo->folder);

    id =
      notification_register_folder_specific_list(COMMAND_SPECIFIC_FOLDER_ID_STR);
    list = notification_foldercheck_get_list(id);
    for(; (list != NULL) && !found; list = g_slist_next(list)) {
      gchar *list_identifier;
      FolderItem *list_item = (FolderItem*) list->data;
      
      list_identifier = folder_item_get_identifier(list_item);
      if(!g_strcmp0(list_identifier, identifier))
	found = TRUE;

      g_free(list_identifier);
    }
    g_free(identifier);
    
    if(!found)
      return;
  } /* folder specific */

  buf = g_strdup(notify_config.command_line);

  G_LOCK(command);

  if(!command.blocked) {
    /* execute command */
    command.blocked = TRUE;
    ret_str = g_locale_from_utf8(buf,strlen(buf),&by_read,&by_written,NULL);
    if(ret_str && by_written) {
      g_free(buf);
      buf = ret_str;
    }
    execute_command_line(buf, TRUE, NULL);
    g_free(buf);
  }

  /* block further execution for some time,
     no matter if it was blocked or not */
  if(command.timeout_id)
    g_source_remove(command.timeout_id);
  command.timeout_id = g_timeout_add(notify_config.command_timeout,
				     command_timeout_fun, NULL);    
  G_UNLOCK(command);
}

static gboolean command_timeout_fun(gpointer data)
{
  G_LOCK(command);
  command.timeout_id = 0;
  command.blocked = FALSE;
  G_UNLOCK(command);
  return FALSE;
}

#endif /* NOTIFICATION_COMMAND */
