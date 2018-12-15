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

#include "folder.h"
#include "folderview.h"
#include "codeconv.h"
#include "gtk/gtkutils.h"

#include "notification_core.h"
#include "notification_plugin.h"
#include "notification_prefs.h"
#include "notification_banner.h"
#include "notification_popup.h"
#include "notification_command.h"
#include "notification_lcdproc.h"
#include "notification_trayicon.h"
#include "notification_indicator.h"

#ifdef HAVE_LIBCANBERRA_GTK
# include <canberra-gtk.h>
#endif

typedef struct {
  GSList *collected_msgs;
  GSList *folder_items;
  gboolean unread_also;
  gint max_msgs;
  gint num_msgs;
} TraverseCollect;

static gboolean notification_traverse_collect(GNode*, gpointer);
static void     notification_new_unnotified_do_msg(MsgInfo*);
static gboolean notification_traverse_hash_startup(GNode*, gpointer);

static GHashTable *msg_count_hash;
static NotificationMsgCount msg_count;

#ifdef HAVE_LIBCANBERRA_GTK
static gboolean canberra_new_email_is_playing = FALSE;
#endif

static void msg_count_hash_update_func(FolderItem*, gpointer);
static void msg_count_update_from_hash(gpointer, gpointer, gpointer);
static void msg_count_clear(NotificationMsgCount*);
static void msg_count_add(NotificationMsgCount*,NotificationMsgCount*);
static void msg_count_copy(NotificationMsgCount*,NotificationMsgCount*);

void notification_core_global_includes_changed(void)
{
#ifdef NOTIFICATION_BANNER
  notification_update_banner();
#endif

  if(msg_count_hash) {
    g_hash_table_destroy(msg_count_hash);
    msg_count_hash = NULL;
  }
  notification_update_msg_counts(NULL);
}

/* Hide/show main window */
void notification_toggle_hide_show_window(void)
{
  MainWindow *mainwin;
	GdkWindow *gdkwin;

  if((mainwin = mainwindow_get_mainwindow()) == NULL)
    return;

	gdkwin = gtk_widget_get_window(GTK_WIDGET(mainwin->window));
  if(gtk_widget_get_visible(GTK_WIDGET(mainwin->window))) {
    if((gdk_window_get_state(gdkwin) & GDK_WINDOW_STATE_ICONIFIED)
       || mainwindow_is_obscured()) {
      notification_show_mainwindow(mainwin);
    }
    else {
      main_window_hide(mainwin);
    }
  }
  else {
    notification_show_mainwindow(mainwin);
  }
}

void notification_update_msg_counts(FolderItem *removed_item)
{
  if(!msg_count_hash)
    msg_count_hash = g_hash_table_new_full(g_str_hash,g_str_equal,
					   g_free,g_free);

  folder_func_to_all_folders(msg_count_hash_update_func, msg_count_hash);

  if(removed_item) {
    gchar *identifier;
    identifier = folder_item_get_identifier(removed_item);
    if(identifier) {
      g_hash_table_remove(msg_count_hash, identifier);
      g_free(identifier);
    }
  }
  msg_count_clear(&msg_count);
  g_hash_table_foreach(msg_count_hash, msg_count_update_from_hash, NULL);
#ifdef NOTIFICATION_LCDPROC
  notification_update_lcdproc();
#endif
#ifdef NOTIFICATION_TRAYICON
  notification_update_trayicon();
#endif
#ifdef NOTIFICATION_INDICATOR
  notification_update_indicator();
#endif
  notification_update_urgency_hint();
}

static void msg_count_clear(NotificationMsgCount *count)
{
  count->new_msgs          = 0;
  count->unread_msgs       = 0;
  count->unreadmarked_msgs = 0;
  count->marked_msgs       = 0;
  count->total_msgs        = 0;
}

/* c1 += c2 */
static void msg_count_add(NotificationMsgCount *c1,NotificationMsgCount *c2)
{
  c1->new_msgs          += c2->new_msgs;
  c1->unread_msgs       += c2->unread_msgs;
  c1->unreadmarked_msgs += c2->unreadmarked_msgs;
  c1->marked_msgs       += c2->marked_msgs;
  c1->total_msgs        += c2->total_msgs;
}

/* c1 = c2 */
static void msg_count_copy(NotificationMsgCount *c1,NotificationMsgCount *c2)
{
  c1->new_msgs          = c2->new_msgs;
  c1->unread_msgs       = c2->unread_msgs;
  c1->unreadmarked_msgs = c2->unreadmarked_msgs;
  c1->marked_msgs       = c2->marked_msgs;
  c1->total_msgs        = c2->total_msgs;
}

gboolean get_flat_gslist_from_nodes_traverse_func(GNode *node, gpointer data)
{
  if(node->data) {
    GSList **list = data;
    *list = g_slist_prepend(*list, node->data);
  }
  return FALSE;
}

GSList* get_flat_gslist_from_nodes(GNode *node)
{
  GSList *retval = NULL;

  g_node_traverse(node, G_PRE_ORDER, G_TRAVERSE_ALL, -1, get_flat_gslist_from_nodes_traverse_func, &retval);
  return retval;
}

void notification_core_get_msg_count_of_foldername(gchar *foldername, NotificationMsgCount *count)
{
  GList *list;
  GSList *f_list;

  Folder *walk_folder;
  Folder *folder = NULL;

  for(list = folder_get_list(); list != NULL; list = list->next) {
    walk_folder = list->data;
    if(g_strcmp0(foldername, walk_folder->name) == 0) {
      folder = walk_folder;
      break;
    }
  }
  if(!folder) {
    debug_print("Notification plugin: Error: Could not find folder %s\n", foldername);
    return;
  }

  msg_count_clear(count);
  f_list = get_flat_gslist_from_nodes(folder->node);
  notification_core_get_msg_count(f_list, count);
  g_slist_free(f_list);
}

void notification_core_get_msg_count(GSList *folder_list,
				     NotificationMsgCount *count)
{
  GSList *walk;

  if(!folder_list)
    msg_count_copy(count,&msg_count);
  else {
    msg_count_clear(count);
    for(walk = folder_list; walk; walk = walk->next) {
      gchar *identifier;
      NotificationMsgCount *item_count;
      FolderItem *item = (FolderItem*) walk->data;
      identifier = folder_item_get_identifier(item);
      if(identifier) {
	item_count = g_hash_table_lookup(msg_count_hash,identifier);
	g_free(identifier);
	if(item_count)
	  msg_count_add(count, item_count);
      }
    }
  }
}

static void msg_count_hash_update_func(FolderItem *item, gpointer data)
{
  gchar *identifier;
  NotificationMsgCount *count;
  GHashTable *hash = data;

  if(!notify_include_folder_type(item->folder->klass->type,
				 item->folder->klass->uistr))
    return;

  identifier = folder_item_get_identifier(item);
  if(!identifier)
    return;

  count = g_hash_table_lookup(hash, identifier);

  if(!count) {
    count = g_new0(NotificationMsgCount,1);
    g_hash_table_insert(hash, identifier, count);
  }
  else
    g_free(identifier);

  count->new_msgs          = item->new_msgs;
  count->unread_msgs       = item->unread_msgs;
  count->unreadmarked_msgs = item->unreadmarked_msgs;
  count->marked_msgs       = item->marked_msgs;
  count->total_msgs        = item->total_msgs;
}

static void msg_count_update_from_hash(gpointer key, gpointer value,
				       gpointer data)
{
  NotificationMsgCount *count = value;
  msg_count_add(&msg_count,count);
}


/* Replacement for the post-filtering hook:
   Pseudocode by Colin:
hook on FOLDER_ITEM_UPDATE_HOOKLIST
 if hook flags & F_ITEM_UPDATE_MSGCOUNT
  scan mails (folder_item_get_msg_list)
   if MSG_IS_NEW(msginfo->flags) and not in hashtable
    notify()
    add to hashtable
   procmsg_msg_list_free

hook on MSGINFO_UPDATE_HOOKLIST
 if hook flags & MSGINFO_UPDATE_FLAGS
  if !MSG_IS_NEW(msginfo->flags)
   remove from hashtable, it's now useless
*/

/* This hash table holds all mails that we already notified about,
   and that still are marked as "new". The keys are the msgid's,
   the values are just 1's stored in a pointer. */
static GHashTable *notified_hash = NULL;


/* Remove message from the notified_hash if
 *  - the message flags changed
 *  - the message is not new
 *  - the message is in the hash
*/
gboolean notification_notified_hash_msginfo_update(MsgInfoUpdate *msg_update)
{
  g_return_val_if_fail(msg_update != NULL, FALSE);

  if((msg_update->flags & MSGINFO_UPDATE_FLAGS) &&
     !MSG_IS_NEW(msg_update->msginfo->flags)) {

    MsgInfo *msg;
    gchar *msgid;

    msg = msg_update->msginfo;
    if(msg->msgid)
      msgid = msg->msgid;
    else {
      debug_print("Notification Plugin: Message has no message ID!\n");
      msgid = "";
    }

    g_return_val_if_fail(msg != NULL, FALSE);

    if(g_hash_table_lookup(notified_hash, msgid) != NULL) {

      debug_print("Notification Plugin: Removing message id %s from hash "
		  "table\n", msgid);
      g_hash_table_remove(notified_hash, msgid);
    }
  }
  return FALSE;
}

/* On startup, mark all new mails as already notified
 * (by including them in the hash) */
void notification_notified_hash_startup_init(void)
{
  GList *folder_list, *walk;
  Folder *folder;

  if(!notified_hash) {
    notified_hash = g_hash_table_new_full(g_str_hash, g_str_equal,
					  g_free, NULL);
    debug_print("Notification Plugin: Hash table created\n");
  }

  folder_list = folder_get_list();
  for(walk = folder_list; walk != NULL; walk = g_list_next(walk)) {
    folder = walk->data;

    g_node_traverse(folder->node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
		    notification_traverse_hash_startup, NULL);
  }
}

static gboolean notification_traverse_hash_startup(GNode *node, gpointer data)
{
  GSList *walk;
  GSList *msg_list;
  FolderItem *item = (FolderItem*) node->data;
  gint new_msgs_left;

  if(!(item->new_msgs))
    return FALSE;

  new_msgs_left = item->new_msgs;
  msg_list = folder_item_get_msg_list(item);

  for(walk = msg_list; walk; walk = g_slist_next(walk)) {
    MsgInfo *msg = (MsgInfo*) walk->data;
    if(MSG_IS_NEW(msg->flags)) {
      gchar *msgid;

      if(msg->msgid)
	msgid = msg->msgid;
      else {
	debug_print("Notification Plugin: Message has no message ID!\n");
	msgid = "";
      }

      /* If the message id is not yet in the hash, add it */
      g_hash_table_insert(notified_hash, g_strdup(msgid),
			  GINT_TO_POINTER(1));
      debug_print("Notification Plugin: Init: Added msg id %s to the hash\n",
		  msgid);
      /* Decrement left count and check if we're already done */
      new_msgs_left--;
      if(new_msgs_left == 0)
	break;
    }
  }
  procmsg_msg_list_free(msg_list);
  return FALSE;
}

void notification_core_free(void)
{
  if(notified_hash) {
    g_hash_table_destroy(notified_hash);
    notified_hash = NULL;
  }
  if(msg_count_hash) {
    g_hash_table_destroy(msg_count_hash);
    msg_count_hash = NULL;
  }
  debug_print("Notification Plugin: Freed internal data\n");
}

void notification_new_unnotified_msgs(FolderItemUpdateData *update_data)
{
  GSList *msg_list, *walk;

  g_return_if_fail(notified_hash != NULL);

  msg_list = folder_item_get_msg_list(update_data->item);

  for(walk = msg_list; walk; walk = g_slist_next(walk)) {
    MsgInfo *msg;
    msg = (MsgInfo*) walk->data;

    if(MSG_IS_NEW(msg->flags)) {
      gchar *msgid;

      if(msg->msgid)
	msgid = msg->msgid;
      else {
	debug_print("Notification Plugin: Message has not message ID!\n");
	msgid = "";
      }

      debug_print("Notification Plugin: Found msg %s, "
		  "checking if it is in hash...\n", msgid);
      /* Check if message is in hash table */
      if(g_hash_table_lookup(notified_hash, msgid) != NULL)
	debug_print("yes.\n");
      else {
	/* Add to hashtable */
	g_hash_table_insert(notified_hash, g_strdup(msgid),
			    GINT_TO_POINTER(1));
	debug_print("no, added to table.\n");

	/* Do the notification */
	notification_new_unnotified_do_msg(msg);
      }

    } /* msg is 'new' */
  } /* for all messages */
  procmsg_msg_list_free(msg_list);
}

#ifdef HAVE_LIBCANBERRA_GTK
static void canberra_finished_cb(ca_context *c, uint32_t id, int error, void *data)
{
  canberra_new_email_is_playing = FALSE;
}
#endif

static void notification_new_unnotified_do_msg(MsgInfo *msg)
{
#ifdef NOTIFICATION_POPUP
  notification_popup_msg(msg);
#endif
#ifdef NOTIFICATION_COMMAND
  notification_command_msg(msg);
#endif
#ifdef NOTIFICATION_TRAYICON
  notification_trayicon_msg(msg);
#endif

#ifdef HAVE_LIBCANBERRA_GTK
  /* canberra */
  if(notify_config.canberra_play_sounds && !canberra_new_email_is_playing) {
    ca_proplist *proplist;
    ca_proplist_create(&proplist);
    ca_proplist_sets(proplist,CA_PROP_EVENT_ID ,"message-new-email");
    canberra_new_email_is_playing = TRUE;
    ca_context_play_full(ca_gtk_context_get(), 0, proplist, canberra_finished_cb, NULL);
    ca_proplist_destroy(proplist);
  }
#endif
}

/* If folders is not NULL, then consider only those folder items
 * If max_msgs is not 0, stop after collecting msg_msgs messages
 */
GSList* notification_collect_msgs(gboolean unread_also, GSList *folder_items,
				  gint max_msgs)
{
  GList *folder_list, *walk;
  Folder *folder;
  TraverseCollect collect_data;

  collect_data.unread_also = unread_also;
  collect_data.collected_msgs = NULL;
  collect_data.folder_items = folder_items;
  collect_data.max_msgs = max_msgs;
  collect_data.num_msgs = 0;

  folder_list = folder_get_list();
  for(walk = folder_list; walk != NULL; walk = g_list_next(walk)) {
    folder = walk->data;

    g_node_traverse(folder->node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
		    notification_traverse_collect, &collect_data);
  }
  return collect_data.collected_msgs;
}

void notification_collected_msgs_free(GSList *collected_msgs)
{
  if(collected_msgs) {
    GSList *walk;
    for(walk = collected_msgs; walk != NULL; walk = g_slist_next(walk)) {
      CollectedMsg *msg = walk->data;
      if(msg->from)
				g_free(msg->from);
      if(msg->subject)
				g_free(msg->subject);
      if(msg->folderitem_name)
				g_free(msg->folderitem_name);
			msg->msginfo = NULL;
      g_free(msg);
    }
    g_slist_free(collected_msgs);
  }
}

static gboolean notification_traverse_collect(GNode *node, gpointer data)
{
  TraverseCollect *cdata = data;
  FolderItem *item = node->data;
  gchar *folder_id_cur;

  /* Obey global folder type limitations */
  if(!notify_include_folder_type(item->folder->klass->type,
				 item->folder->klass->uistr))
    return FALSE;

  /* If a folder_items list was given, check it first */
  if((cdata->folder_items) && (item->path != NULL) &&
     ((folder_id_cur  = folder_item_get_identifier(item)) != NULL)) {
    FolderItem *list_item;
    GSList *walk;
    gchar *folder_id_list;
    gboolean eq;
    gboolean folder_in_list = FALSE;

    for(walk = cdata->folder_items; walk != NULL; walk = g_slist_next(walk)) {
      list_item = walk->data;
      folder_id_list = folder_item_get_identifier(list_item);
      eq = !g_strcmp0(folder_id_list,folder_id_cur);
      g_free(folder_id_list);
      if(eq) {
	folder_in_list = TRUE;
	break;
      }
    }
    g_free(folder_id_cur);
    if(!folder_in_list)
      return FALSE;
  }

  if(item->new_msgs || (cdata->unread_also && item->unread_msgs)) {
    GSList *msg_list = folder_item_get_msg_list(item);
    GSList *walk;
    for(walk = msg_list; walk != NULL; walk = g_slist_next(walk)) {
      MsgInfo *msg_info = walk->data;
      CollectedMsg *cmsg;

      if((cdata->max_msgs != 0) && (cdata->num_msgs >= cdata->max_msgs))
	return FALSE;

      if(MSG_IS_NEW(msg_info->flags) ||
				 (MSG_IS_UNREAD(msg_info->flags) && cdata->unread_also)) {

				cmsg = g_new(CollectedMsg, 1);
				cmsg->from = g_strdup(msg_info->from ? msg_info->from : "");
				cmsg->subject = g_strdup(msg_info->subject ? msg_info->subject : "");
				if(msg_info->folder && msg_info->folder->name)
					cmsg->folderitem_name = g_strdup(msg_info->folder->path);
				else
					cmsg->folderitem_name = g_strdup("");

				cmsg->msginfo = msg_info;

				cdata->collected_msgs = g_slist_prepend(cdata->collected_msgs, cmsg);
				cdata->num_msgs++;
      }
    }
    procmsg_msg_list_free(msg_list);
  }

  return FALSE;
}

gboolean notify_include_folder_type(FolderType ftype, gchar *uistr)
{
  gboolean retval;

  retval = FALSE;
  switch(ftype) {
  case F_MH:
  case F_MBOX:
  case F_MAILDIR:
  case F_IMAP:
    if(notify_config.include_mail)
      retval = TRUE;
    break;
  case F_NEWS:
    if(notify_config.include_news)
      retval = TRUE;
    break;
  case F_UNKNOWN:
    if(uistr == NULL)
      retval = FALSE;
    else if(!strcmp(uistr, "vCalendar")) {
      if(notify_config.include_calendar)
	retval = TRUE;
    }
    else if(!strcmp(uistr, "RSSyl")) {
      if(notify_config.include_rss)
	retval = TRUE;
    }
    else
      debug_print("Notification Plugin: Unknown folder type %d\n",ftype);
    break;
  default:
    debug_print("Notification Plugin: Unknown folder type %d\n",ftype);
  }

  return retval;
}

static void fix_folderview_scroll(MainWindow *mainwin)
{
	static gboolean fix_done = FALSE;

	if (fix_done)
		return;

	gtk_widget_queue_resize(mainwin->folderview->ctree);

	fix_done = TRUE;
}

void notification_show_mainwindow(MainWindow *mainwin)
{
      gtk_window_deiconify(GTK_WINDOW(mainwin->window));
      gtk_window_set_skip_taskbar_hint(GTK_WINDOW(mainwin->window), FALSE);
      main_window_show(mainwin);
      gtk_window_present(GTK_WINDOW(mainwin->window));
      fix_folderview_scroll(mainwin);
}

#ifdef HAVE_LIBNOTIFY
#define STR_MAX_LEN 511
/* Returns a newly allocated string which needs to be freed */
gchar* notification_libnotify_sanitize_str(gchar *in)
{
  gint out;
  gchar tmp_str[STR_MAX_LEN+1];

  if(in == NULL) return NULL;

  out = 0;
  while(*in) {
    if(*in == '<') {
      if(out+4 > STR_MAX_LEN) break;
      memcpy(&(tmp_str[out]),"&lt;",4);
      in++; out += 4;
    }
    else if(*in == '>') {
      if(out+4 > STR_MAX_LEN) break;
      memcpy(&(tmp_str[out]),"&gt;",4);
      in++; out += 4;
    }
    else if(*in == '&') {
      if(out+5 > STR_MAX_LEN) break;
      memcpy(&(tmp_str[out]),"&amp;",5);
      in++; out += 5;
    }
    else {
      if(out+1 > STR_MAX_LEN) break;
      tmp_str[out++] = *in++;
    }
  }
  tmp_str[out] = '\0';
  return strdup(tmp_str);
}

gchar* notification_validate_utf8_str(gchar *text)
{
  gchar *utf8_str = NULL;

  if(!g_utf8_validate(text, -1, NULL)) {
    debug_print("Notification plugin: String is not valid utf8, "
		"trying to fix it...\n");
    /* fix it */
    utf8_str = conv_codeset_strdup(text,
				   conv_get_locale_charset_str_no_utf8(),
				   CS_INTERNAL);
    /* check if the fix worked */
    if(utf8_str == NULL || !g_utf8_validate(utf8_str, -1, NULL)) {
      debug_print("Notification plugin: String is still not valid utf8, "
		  "sanitizing...\n");
      utf8_str = g_malloc(strlen(text)*2+1);
      conv_localetodisp(utf8_str, strlen(text)*2+1, text);
    }
  }
  else {
    debug_print("Notification plugin: String is valid utf8\n");
    utf8_str = g_strdup(text);
  }
  return utf8_str;
}
#endif
