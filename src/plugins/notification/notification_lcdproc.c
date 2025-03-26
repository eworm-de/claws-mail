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

#ifdef NOTIFICATION_LCDPROC

#include "notification_lcdproc.h"
#include "notification_prefs.h"
#include "notification_core.h"

#include "common/socket.h"

#include <string.h>
#include <sys/socket.h>

#define NOTIFICATION_LCDPROC_BUFFER_SIZE 8192

void notification_sock_puts(SockInfo*, gchar*);
void notification_lcdproc_send(gchar*);

static SockInfo *sock = NULL;

void notification_lcdproc_connect(void)
{
  gint len, count;
  gchar buf[NOTIFICATION_LCDPROC_BUFFER_SIZE];

  if(!notify_config.lcdproc_enabled)
    return;

  if(sock)
    notification_lcdproc_disconnect();

  sock = sock_connect(notify_config.lcdproc_hostname,
		      notify_config.lcdproc_port);
  /*
   * Quietly return when a connection fails; next attempt
   * will be made when some folder info has been changed.
   */
  if(sock == NULL || sock->state == CONN_FAILED) {
    debug_print("Could not connect to LCDd\n");
    if(sock && sock->state == CONN_FAILED) {
      sock_close(sock, TRUE);
      sock = NULL;
    }
    return;
  }
  else
    debug_print("Connected to LCDd\n");
  
  sock_set_nonblocking_mode(sock, TRUE);

  /* Friendly people say "hello" first */
  notification_sock_puts(sock, "hello");

  /* FIXME: Ouch. Is this really the way to go? */
  count = 50;
  len = 0;
  while((len <= 0) && (count-- >= 0)) {
    g_usleep(125000);
    len = sock_read(sock, buf, NOTIFICATION_LCDPROC_BUFFER_SIZE);
  }
  
/*
 * This might not be a LCDProc server.
 * FIXME: check LCD size, LCDd version etc
 */
  
  if (len <= 0) {
    debug_print("Notification plugin: Can't communicate with "
		"LCDd server! Are you sure that "
		"there is a LCDd server running on %s:%d?\n",
		notify_config.lcdproc_hostname, notify_config.lcdproc_port);
    notification_lcdproc_disconnect();
    return;
  }
  
  notification_lcdproc_send("client_set -name \"{Claws-Mail}\"");

  notification_lcdproc_send("screen_add msg_counts");
  notification_lcdproc_send("screen_set msg_counts -name {Claws-Mail Message Count}");
  
  notification_lcdproc_send("widget_add msg_counts title title");
  notification_lcdproc_send("widget_set msg_counts title {Claws-Mail}");
  notification_lcdproc_send("widget_add msg_counts line1 string");
  notification_lcdproc_send("widget_add msg_counts line2 string");
  notification_lcdproc_send("widget_add msg_counts line3 string");

  notification_update_msg_counts(NULL);
}

void notification_lcdproc_disconnect(void)
{
  if(sock) {
#ifndef G_OS_WIN32
    shutdown(sock->sock, SHUT_RDWR);
#endif
    sock_close(sock, TRUE);
    sock = NULL;
  }
}

void notification_update_lcdproc(void)
{
  NotificationMsgCount count;
  gchar *buf;

  if(!notify_config.lcdproc_enabled || !sock)
    return;

  if(!sock || sock->state == CONN_FAILED) {
    notification_lcdproc_connect();
    return;
  }
  
  notification_core_get_msg_count(NULL, &count);

  if((count.new_msgs + count.unread_msgs) > 0) {
    buf =
      g_strdup_printf("widget_set msg_counts line1 1 2 {%s: %d}",_("New"),
		      count.new_msgs);
    notification_lcdproc_send(buf);
    buf =
      g_strdup_printf("widget_set msg_counts line2 1 3 {%s: %d}",_("Unread"),
		      count.unread_msgs);
    notification_lcdproc_send(buf);
    buf =
      g_strdup_printf("widget_set msg_counts line3 1 4 {%s: %d}",_("Total"),
		      count.total_msgs);
    notification_lcdproc_send(buf);
  }
  else {
    buf = g_strdup_printf("widget_set msg_counts line1 1 2 {%s}",
			  _("No new messages"));
    notification_lcdproc_send(buf);
    buf = g_strdup_printf("widget_set msg_counts line2 1 3 {}");
    notification_lcdproc_send(buf);
    buf = g_strdup_printf("widget_set msg_counts line3 1 4 {}");
    notification_lcdproc_send(buf);
  }

  g_free(buf);
}

void notification_sock_puts(SockInfo *socket, gchar *string)
{
  sock_write(socket, string, strlen(string));
  sock_write(socket, "\n", 1);
}

void notification_lcdproc_send(gchar *string)
{
  notification_sock_puts(sock, string);
  /* TODO: Check return message from LCDd */
}

#endif /* NOTIFICATION_LCDPROC */
