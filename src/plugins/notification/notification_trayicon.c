/* Notification plugin for Claws Mail
 * Copyright (C) 2005-2022 Holger Berndt and the Claws Mail Team.
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

/* This module is of course inspired by the trayicon plugin which is
 * shipped with Claws Mail, copyrighted by the Claws Mail Team. */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#  include "claws-features.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>

#ifdef NOTIFICATION_TRAYICON

#include "notification_trayicon.h"
#include "notification_prefs.h"
#include "notification_core.h"
#include "notification_hotkeys.h"
#include "notification_pixbuf.h"
#include "notification_foldercheck.h"

#include "main.h"
#include "account.h"
#include "mainwindow.h"
#include "prefs_common.h"
#include "alertpanel.h"
#include "gtk/menu.h"
#ifndef USE_ALT_ADDRBOOK
    #include "addressbook.h"
    #include "addrindex.h"
#else
    #include "addressbook-dbus.h"
#endif

#include "gtk/manage_window.h"
#include "common/utils.h"
#include "gtk/gtkutils.h"
#include "inc.h"

static void notification_trayicon_account_list_reset(const gchar *,
														gpointer,
														gboolean);

static GdkPixbuf* notification_trayicon_create(void);

static void notification_trayicon_on_popup_menu(GtkStatusIcon*,guint,
						guint,gpointer);
static gboolean notification_trayicon_on_size_changed(GtkStatusIcon*,
						      gint, gpointer);

static void trayicon_get_all_cb(GtkAction*, gpointer);
static void trayicon_get_from_account_cb(GtkAction*, gpointer);
static void trayicon_compose_cb(GtkAction*, gpointer);
static void trayicon_compose_acc_cb(GtkMenuItem*, gpointer);
static void trayicon_addressbook_cb(GtkAction*, gpointer);
static void trayicon_exit_cb(GtkAction*,gpointer);
static void trayicon_toggle_offline_cb(GtkAction*,gpointer);
#ifdef HAVE_LIBNOTIFY
static void trayicon_toggle_notify_cb(GtkAction*,gpointer);
#endif

#ifdef HAVE_LIBNOTIFY
#include <libnotify/notify.h>

#ifndef NOTIFY_CHECK_VERSION
# define NOTIFY_CHECK_VERSION(a,b,c) 0
#endif

typedef struct {
  gint count;
  gint num_mail;
  gint num_news;
  gint num_calendar;
  gint num_rss;
  gchar *msg_path;
  NotifyNotification *notification;
  GError *error;
} NotificationTrayiconPopup;

static NotificationTrayiconPopup popup;

static gboolean notification_trayicon_popup_add_msg(MsgInfo*,
						    NotificationFolderType);
static gboolean notification_trayicon_popup_create(MsgInfo*,
						   NotificationFolderType);
static void popup_timeout_fun(NotifyNotification*, gpointer);
static void notification_trayicon_popup_free_func(gpointer);
static void notification_trayicon_popup_default_action_cb(NotifyNotification*,
							  const char*,void*);
static gchar* notification_trayicon_popup_assemble_summary(void);
static gchar* notification_trayicon_popup_assemble_body(MsgInfo*);
static void   notification_trayicon_popup_count_msgs(NotificationFolderType);

G_LOCK_DEFINE_STATIC(trayicon_popup);

#endif

static GtkStatusIcon *trayicon;
static gboolean updating_menu = FALSE;
static GtkWidget *traymenu_popup;
static GtkWidget *focused_widget = NULL;

static GtkActionEntry trayicon_popup_menu_entries[] = {
	{"SysTrayiconPopup", NULL, "SysTrayiconPopup", NULL, NULL, NULL },
	{"SysTrayiconPopup/GetMail", NULL, N_("_Get Mail"), NULL, NULL, G_CALLBACK(trayicon_get_all_cb) },
	{"SysTrayiconPopup/GetMailAcc", NULL, N_("_Get Mail from account"), NULL, NULL, NULL },
	{"SysTrayiconPopup/---", NULL, "---", NULL, NULL, NULL },
	{"SysTrayiconPopup/Email", NULL, N_("_Email"), NULL, NULL, G_CALLBACK(trayicon_compose_cb) },
	{"SysTrayiconPopup/EmailAcc", NULL, N_("E_mail from account"), NULL, NULL, NULL },
	{"SysTrayiconPopup/OpenAB", NULL, N_("Open A_ddressbook"), NULL, NULL, G_CALLBACK(trayicon_addressbook_cb) },
	{"SysTrayiconPopup/Exit", NULL, N_("E_xit Claws Mail"), NULL, NULL, G_CALLBACK(trayicon_exit_cb) },
};

static GtkToggleActionEntry trayicon_popup_toggle_menu_entries[] =
{
	{"SysTrayiconPopup/ToggleOffline", NULL, N_("_Work Offline"), NULL, NULL, G_CALLBACK(trayicon_toggle_offline_cb), FALSE },
#ifdef HAVE_LIBNOTIFY
	{"SysTrayiconPopup/ShowBubbles", NULL, N_("Show Trayicon Notifications"), NULL, NULL, G_CALLBACK(trayicon_toggle_notify_cb), FALSE },
#endif
};


void notification_trayicon_msg(MsgInfo *msginfo)
{
#ifndef HAVE_LIBNOTIFY
  return;

#else
  FolderType ftype;
  NotificationFolderType nftype;
  gchar *uistr;

  nftype = F_TYPE_MAIL;

  if(!msginfo || !notify_config.trayicon_enabled ||
     !notify_config.trayicon_popup_enabled ||
     !MSG_IS_NEW(msginfo->flags))
    return;

  if(notify_config.trayicon_folder_specific) {
    guint id;
    GSList *list;
    gchar *identifier;
    gboolean found = FALSE;

    if(!(msginfo->folder))
      return;

    identifier = folder_item_get_identifier(msginfo->folder);

    id =
      notification_register_folder_specific_list
      (TRAYICON_SPECIFIC_FOLDER_ID_STR);
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

  ftype = msginfo->folder->folder->klass->type;

  G_LOCK(trayicon_popup);
  /* Check out which type to notify about */
  switch(ftype) {
  case F_MH:
  case F_MBOX:
  case F_MAILDIR:
  case F_IMAP:
    nftype = F_TYPE_MAIL;
    break;
  case F_NEWS:
    nftype = F_TYPE_NEWS;
    break;
  case F_UNKNOWN:
    if((uistr = msginfo->folder->folder->klass->uistr) == NULL) {
      G_UNLOCK(trayicon_popup);
      return;
    }
    else if(!strcmp(uistr, "vCalendar"))
      nftype = F_TYPE_CALENDAR;
    else if(!strcmp(uistr, "RSSyl"))
      nftype = F_TYPE_RSS;
    else {
      debug_print("Notification Plugin: Unknown folder type %d\n",ftype);
      G_UNLOCK(trayicon_popup);
      return;
    }
    break;
  default:
    debug_print("Notification Plugin: Unknown folder type %d\n",ftype);
    G_UNLOCK(trayicon_popup);
    return;
  }


  notification_trayicon_popup_add_msg(msginfo, nftype);

  G_UNLOCK(trayicon_popup);

#endif /* HAVE_LIBNOTIFY */
}

void notification_trayicon_destroy(void)
{
  if(trayicon) {
    gtk_status_icon_set_visible(trayicon, FALSE);
    g_object_unref(trayicon);
    trayicon = NULL;
  }
}

void notification_update_trayicon()
{
  gchar *buf;
  static GdkPixbuf *old_icon = NULL;
  GdkPixbuf *new_icon;
  gint offset;
  NotificationMsgCount count;
  GSList *list;

  if(!notify_config.trayicon_enabled)
    return;

  if(notify_config.trayicon_folder_specific) {
    guint id;
    id =
      notification_register_folder_specific_list
      (TRAYICON_SPECIFIC_FOLDER_ID_STR);
    list = notification_foldercheck_get_list(id);
  }
  else
    list = NULL;

  notification_core_get_msg_count(list, &count);

  if(!trayicon) {

#ifdef NOTIFICATION_HOTKEYS
    notification_hotkeys_update_bindings();
#endif

    old_icon = notification_trayicon_create();
    if(!trayicon) {
      debug_print("Notification plugin: Could not create trayicon\n");
      return;
    }
  }

  /* Tooltip */
  buf = g_strdup_printf(_("New %d, Unread: %d, Total: %d"),
			count.new_msgs, count.unread_msgs,
			count.total_msgs);
  gtk_status_icon_set_tooltip_text(trayicon, buf);

  g_free(buf);

  /* Pixmap */
  (prefs_common_get_prefs()->work_offline) ? (offset = 1) : (offset = 0);

  if((count.new_msgs > 0) && (count.unreadmarked_msgs > 0))
    new_icon =
      notification_pixbuf_get(NOTIFICATION_TRAYICON_NEWMARKEDMAIL+offset);
  else if(count.new_msgs > 0)
    new_icon =
      notification_pixbuf_get(NOTIFICATION_TRAYICON_NEWMAIL+offset);
  else if(count.unreadmarked_msgs > 0)
    new_icon =
      notification_pixbuf_get(NOTIFICATION_TRAYICON_UNREADMARKEDMAIL+offset);
  else if(count.unread_msgs > 0)
    new_icon =
      notification_pixbuf_get(NOTIFICATION_TRAYICON_UNREADMAIL+offset);
  else
    new_icon =
      notification_pixbuf_get(NOTIFICATION_TRAYICON_NOMAIL+offset);

  if(new_icon != old_icon) {
    gtk_status_icon_set_from_pixbuf(trayicon, new_icon);
    old_icon = new_icon;
  }
}

gboolean notification_trayicon_main_window_close(gpointer source, gpointer data)
{
  if(!notify_config.trayicon_enabled)
    return FALSE;

  if(source) {
    gboolean *close_allowed = (gboolean*)source;

    if(notify_config.trayicon_close_to_tray) {
      MainWindow *mainwin = mainwindow_get_mainwindow();

      *close_allowed = FALSE;
      if(mainwin && gtk_widget_get_visible(GTK_WIDGET(mainwin->window))) {
	focused_widget = gtk_window_get_focus(GTK_WINDOW(mainwin->window));
	main_window_hide(mainwin);
      }
    }
  }
  return FALSE;
}

gboolean notification_trayicon_main_window_got_iconified(gpointer source,
							 gpointer data)
{
  MainWindow *mainwin = mainwindow_get_mainwindow();

  if(!notify_config.trayicon_enabled)
    return FALSE;

  if(notify_config.trayicon_hide_when_iconified &&
     mainwin && gtk_widget_get_visible(GTK_WIDGET(mainwin->window))
     && !gtk_window_get_skip_taskbar_hint(GTK_WINDOW(mainwin->window))) {
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(mainwin->window), TRUE);
  }
  return FALSE;
}

static void notification_trayicon_account_list_reset(const gchar *menuname,
													gpointer callback,
													gboolean receive)
{
    GList *cur_ac;
	GtkWidget *menu, *submenu;
    GtkWidget *menuitem;
    PrefsAccount *ac_prefs;

    GList *account_list = account_get_list();

	menu = gtk_ui_manager_get_widget(gtkut_ui_manager(), menuname);
	gtk_widget_show(menu);

	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu), NULL);
	submenu = gtk_menu_new();

	for(cur_ac = account_list; cur_ac != NULL; cur_ac = cur_ac->next) {
		ac_prefs = (PrefsAccount *)cur_ac->data;

		/* accounts list for receiving: skip SMTP-only accounts */
		if (receive && ac_prefs->protocol == A_NONE)
			continue;

		menuitem = gtk_menu_item_new_with_label
						(ac_prefs->account_name ? ac_prefs->account_name
						: _("Untitled"));
		gtk_widget_show(menuitem);
		gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menuitem);
		g_signal_connect(G_OBJECT(menuitem), "activate",
				G_CALLBACK(callback),
				ac_prefs);
	}
	gtk_widget_show(submenu);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu), submenu);
}

gboolean notification_trayicon_account_list_changed(gpointer source,
						    gpointer data)
{
	if (notify_config.trayicon_enabled) {
	  	notification_trayicon_account_list_reset("/Menus/SysTrayiconPopup/GetMailAcc",
												(gpointer)trayicon_get_from_account_cb, TRUE);
		notification_trayicon_account_list_reset("/Menus/SysTrayiconPopup/EmailAcc",
												(gpointer)trayicon_compose_acc_cb, FALSE);
	}
	return FALSE;
}

static GdkPixbuf* notification_trayicon_create(void)
{
  GdkPixbuf *trayicon_nomail;
	GtkActionGroup *action_group;

  trayicon_nomail = notification_pixbuf_get(NOTIFICATION_TRAYICON_NOMAIL);

  notification_trayicon_destroy();

  trayicon = gtk_status_icon_new_from_pixbuf(trayicon_nomail);

  g_signal_connect(G_OBJECT(trayicon), "activate",
		   G_CALLBACK(notification_trayicon_on_activate), NULL);
  g_signal_connect(G_OBJECT(trayicon), "popup-menu",
		   G_CALLBACK(notification_trayicon_on_popup_menu), NULL);
  g_signal_connect(G_OBJECT(trayicon), "size-changed",
		   G_CALLBACK(notification_trayicon_on_size_changed), NULL);

  /* Popup-Menu */
	action_group = cm_menu_create_action_group("SysTrayiconPopup", trayicon_popup_menu_entries,
																						 G_N_ELEMENTS(trayicon_popup_menu_entries), NULL);
	gtk_action_group_add_toggle_actions(action_group, trayicon_popup_toggle_menu_entries,
																			G_N_ELEMENTS(trayicon_popup_toggle_menu_entries), NULL);

	MENUITEM_ADDUI("/Menus", "SysTrayiconPopup", "SysTrayiconPopup", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI("/Menus/SysTrayiconPopup", "GetMail", "SysTrayiconPopup/GetMail", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI("/Menus/SysTrayiconPopup", "GetMailAcc", "SysTrayiconPopup/GetMailAcc", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI("/Menus/SysTrayiconPopup", "Separator1", "SysTrayiconPopup/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI("/Menus/SysTrayiconPopup", "Email", "SysTrayiconPopup/Email", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI("/Menus/SysTrayiconPopup", "EmailAcc", "SysTrayiconPopup/EmailAcc", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI("/Menus/SysTrayiconPopup", "Separator2", "SysTrayiconPopup/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI("/Menus/SysTrayiconPopup", "OpenAB", "SysTrayiconPopup/OpenAB", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI("/Menus/SysTrayiconPopup", "Separator3", "SysTrayiconPopup/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI("/Menus/SysTrayiconPopup", "ToggleOffline", "SysTrayiconPopup/ToggleOffline", GTK_UI_MANAGER_MENUITEM)
#ifdef HAVE_LIBNOTIFY
	MENUITEM_ADDUI("/Menus/SysTrayiconPopup", "ShowBubbles", "SysTrayiconPopup/ShowBubbles", GTK_UI_MANAGER_MENUITEM)
#endif
	MENUITEM_ADDUI("/Menus/SysTrayiconPopup", "Separator4", "SysTrayiconPopup/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI("/Menus/SysTrayiconPopup", "Exit", "SysTrayiconPopup/Exit", GTK_UI_MANAGER_MENUITEM)

	traymenu_popup = gtk_menu_item_get_submenu(GTK_MENU_ITEM(
				gtk_ui_manager_get_widget(gtkut_ui_manager(), "/Menus/SysTrayiconPopup")));


  return trayicon_nomail;
}

void notification_trayicon_on_activate(GtkStatusIcon *status_icon, gpointer user_data)
{
  MainWindow *mainwin = mainwindow_get_mainwindow();

  if(mainwin && gtk_widget_get_visible(GTK_WIDGET(mainwin->window)) == TRUE)
    focused_widget = gtk_window_get_focus(GTK_WINDOW(mainwin->window));

  notification_toggle_hide_show_window();

  if(mainwin && gtk_widget_get_visible(GTK_WIDGET(mainwin->window)) == TRUE)
    gtk_window_set_focus(GTK_WINDOW(mainwin->window), focused_widget);
}

static void notification_trayicon_on_popup_menu(GtkStatusIcon *status_icon,
						guint button, guint activate_time,
						gpointer user_data)
{
  MainWindow *mainwin = mainwindow_get_mainwindow();

  if(!mainwin)
    return;

  /* tell callbacks to skip any event */
  updating_menu = TRUE;
  /* initialize checkitems according to current states */
	cm_toggle_menu_set_active("SysTrayiconPopup/ToggleOffline", prefs_common_get_prefs()->work_offline);
#ifdef HAVE_LIBNOTIFY
	cm_toggle_menu_set_active("SysTrayiconPopup/ShowBubbles", notify_config.trayicon_popup_enabled);
#endif
	cm_menu_set_sensitive("SysTrayiconPopup/GetMail", mainwin->lock_count == 0);
	cm_menu_set_sensitive("SysTrayiconPopup/GetMailAcc", mainwin->lock_count == 0);
	cm_menu_set_sensitive("SysTrayiconPopup/Exit", mainwin->lock_count == 0);

  updating_menu = FALSE;

  gtk_menu_popup_at_pointer(GTK_MENU(traymenu_popup), NULL);
}

static gboolean notification_trayicon_on_size_changed(GtkStatusIcon *icon,
						      gint size,
						      gpointer user_data)
{
  notification_update_msg_counts(NULL);
  return FALSE;
}

/* popup menu callbacks */
static void trayicon_get_all_cb(GtkAction *action, gpointer data)
{
  MainWindow *mainwin = mainwindow_get_mainwindow();
  inc_all_account_mail_cb(mainwin, 0, NULL);
}

static void trayicon_get_from_account_cb(GtkAction *action, gpointer data)
{
  MainWindow *mainwin = mainwindow_get_mainwindow();
  PrefsAccount *account = (PrefsAccount *)data;
  inc_account_mail(mainwin, account);
}

static void trayicon_compose_cb(GtkAction *action, gpointer data)
{
  MainWindow *mainwin = mainwindow_get_mainwindow();
  compose_mail_cb(mainwin, 0, NULL);
}

static void trayicon_compose_acc_cb(GtkMenuItem *menuitem, gpointer data)
{
  compose_new((PrefsAccount *)data, NULL, NULL);
}

static void trayicon_addressbook_cb(GtkAction *action, gpointer data)
{
#ifndef USE_ALT_ADDRBOOK
    addressbook_open(NULL);
#else
    GError* error = NULL;

    addressbook_dbus_open(FALSE, &error);
    if (error) {
	g_warning("%s", error->message);
	g_error_free(error);
    }
#endif
}

static void trayicon_toggle_offline_cb(GtkAction *action, gpointer data)
{
  /* toggle offline mode if menu checkitem has been clicked */
  if(!updating_menu) {
    MainWindow *mainwin = mainwindow_get_mainwindow();
    main_window_toggle_work_offline(mainwin, !prefs_common_get_prefs()->work_offline, TRUE);
  }
}

#ifdef HAVE_LIBNOTIFY
static void trayicon_toggle_notify_cb(GtkAction *action, gpointer data)
{
  if(!updating_menu) {
    notify_config.trayicon_popup_enabled = !notify_config.trayicon_popup_enabled;
  }
}
#endif

static void app_exit_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
  if(prefs_common_get_prefs()->confirm_on_exit) {
    if(alertpanel(_("Exit"), _("Exit Claws Mail?"),
		  NULL, _("_Cancel"), NULL, _("_OK"),
		  NULL, NULL, ALERTFOCUS_FIRST) != G_ALERTALTERNATE) {
      return;
    }
    manage_window_focus_in(mainwin->window, NULL, NULL);
  }

  if (prefs_common_get_prefs()->clean_on_exit) {
    if (!main_window_empty_trash(mainwin, prefs_common_get_prefs()->ask_on_clean, TRUE))
      return;
  }

  app_will_exit(NULL, mainwin);
}

static void trayicon_exit_cb(GtkAction *action, gpointer data)
{
  MainWindow *mainwin = mainwindow_get_mainwindow();

  if(mainwin->lock_count == 0) {
    app_exit_cb(mainwin, 0, NULL);
  }
}

#ifdef HAVE_LIBNOTIFY
static gboolean notification_trayicon_popup_add_msg(MsgInfo *msginfo,
						    NotificationFolderType nftype)
{
  gchar *summary;
  gchar *utf8_str;
  gboolean retval;
  GdkPixbuf *pixbuf;

  g_return_val_if_fail(msginfo, FALSE);

  if(!popup.notification)
    return notification_trayicon_popup_create(msginfo,nftype);

  /* Count messages */
  notification_trayicon_popup_count_msgs(nftype);

  if(popup.msg_path) {
    g_free(popup.msg_path);
    popup.msg_path = NULL;
  }

  summary  = notification_trayicon_popup_assemble_summary();
  utf8_str = notification_trayicon_popup_assemble_body(msginfo);

  /* make sure we show a logo on many msg arrival */
  pixbuf = notification_pixbuf_get(NOTIFICATION_CM_LOGO_64x64);
  if(pixbuf)
    notify_notification_set_icon_from_pixbuf(popup.notification, pixbuf);

  retval = notify_notification_update(popup.notification, summary,
				      utf8_str, NULL);
  g_free(summary);
  g_free(utf8_str);
  if(!retval) {
    debug_print("Notification Plugin: Failed to update notification.\n");
    return FALSE;
  }
  /* Show the popup */
  notify_notification_set_hint_string(popup.notification, "desktop-entry", "claws-mail");
  if(!notify_notification_show(popup.notification, &(popup.error))) {
    debug_print("Notification Plugin: Failed to send updated notification: "
		"%s\n",	popup.error->message);
    g_clear_error(&(popup.error));
    return FALSE;
  }

  debug_print("Notification Plugin: Popup successfully modified "
	      "with libnotify.\n");

  return TRUE;
}

static gboolean notification_trayicon_popup_create(MsgInfo *msginfo,
						   NotificationFolderType nftype)
{
  gchar *summary = NULL;
  gchar *utf8_str = NULL;
  GdkPixbuf *pixbuf;
  GList *caps = NULL;
  gboolean support_actions = FALSE;

  /* init libnotify if necessary */
  if(!notify_is_initted()) {
    if(!notify_init("claws-mail")) {
      debug_print("Notification Plugin: Failed to initialize libnotify. "
		  "No popups will be shown.\n");
      return FALSE;
    }
  }

  /* Count messages */
  notification_trayicon_popup_count_msgs(nftype);

  summary  = notification_trayicon_popup_assemble_summary();
  utf8_str = notification_trayicon_popup_assemble_body(msginfo);

#if NOTIFY_CHECK_VERSION(0, 7, 0)
  popup.notification = notify_notification_new(summary, utf8_str, NULL);
#else
  popup.notification = notify_notification_new(summary, utf8_str, NULL, NULL);
  notify_notification_attach_to_status_icon(popup.notification, trayicon);
#endif

  g_free(summary);
  g_free(utf8_str);

  caps = notify_get_server_caps();
    if(caps != NULL) {
      GList *c;
      for(c = caps; c != NULL; c = c->next) {
	if(strcmp((char*)c->data, "actions") == 0 ) {
	  support_actions = TRUE;
	  break;
        }
      }

    g_list_foreach(caps, (GFunc)g_free, NULL);
    g_list_free(caps);
  }

  /* Default action */
  if (support_actions)
    notify_notification_add_action(popup.notification,
				   "default", _("Present main window"),
				   (NotifyActionCallback)
				   notification_trayicon_popup_default_action_cb,
				   GINT_TO_POINTER(nftype),
				   notification_trayicon_popup_free_func);

  if(popup.notification == NULL) {
    debug_print("Notification Plugin: Failed to create a new notification.\n");
    return FALSE;
  }

  /* Icon */
  pixbuf = NULL;
#ifndef USE_ALT_ADDRBOOK
  if(msginfo && msginfo->from) {
    gchar *icon_path;
    icon_path = addrindex_get_picture_file(msginfo->from);
    if(is_file_exist(icon_path)) {
      GError *error = NULL;
      gint w, h;

      gdk_pixbuf_get_file_info(icon_path, &w, &h);
      if((w > 64) || (h > 64))
	pixbuf = gdk_pixbuf_new_from_file_at_scale(icon_path,
						   64, 64, TRUE, &error);
      else
	pixbuf = gdk_pixbuf_new_from_file(icon_path, &error);

      if(!pixbuf) {
	debug_print("Could not load picture file: %s\n",
		    error ? error->message : "no details");
	g_error_free(error);
      }
    }
    else
      debug_print("Picture path does not exist: %s\n",icon_path);
    g_free(icon_path);
  }
#endif
  if(!pixbuf)
    pixbuf = g_object_ref(notification_pixbuf_get(NOTIFICATION_CM_LOGO_64x64));

  if(pixbuf) {
    notify_notification_set_icon_from_pixbuf(popup.notification, pixbuf);
    g_object_unref(pixbuf);
  }
  else /* This is not fatal */
    debug_print("Notification plugin: Icon could not be loaded.\n");

  /* timeout */
  notify_notification_set_timeout(popup.notification, notify_config.trayicon_popup_timeout);

  /* Category */
  notify_notification_set_category(popup.notification, "email.arrived");

  /* get notified on bubble close */
  g_signal_connect(G_OBJECT(popup.notification), "closed", G_CALLBACK(popup_timeout_fun), NULL);

  /* Show the popup */
  notify_notification_set_hint_string(popup.notification, "desktop-entry", "claws-mail");
  if(!notify_notification_show(popup.notification, &(popup.error))) {
    debug_print("Notification Plugin: Failed to send notification: %s\n",
		popup.error->message);
    g_clear_error(&(popup.error));
    g_object_unref(G_OBJECT(popup.notification));
    popup.notification = NULL;
    return FALSE;
  }

  /* Store path to message */
  if(nftype == F_TYPE_MAIL) {
    if(msginfo && msginfo->folder) {
      gchar *ident;
      ident = folder_item_get_identifier(msginfo->folder);
      popup.msg_path = g_strdup_printf("%s%s%u", ident,G_DIR_SEPARATOR_S,
				       msginfo->msgnum);
      g_free(ident);
    }
    else
      popup.msg_path = NULL;
  }

  debug_print("Notification Plugin: Popup created with libnotify.\n");

  return TRUE;
}

static void popup_timeout_fun(NotifyNotification *nn, gpointer data)
{
  G_LOCK(trayicon_popup);

  g_object_unref(G_OBJECT(popup.notification));

  popup.notification = NULL;
  g_clear_error(&(popup.error));

  popup.count = 0;
  popup.num_mail = 0;
  popup.num_news = 0;
  popup.num_calendar = 0;
  popup.num_rss = 0;

  if(popup.msg_path) {
    g_free(popup.msg_path);
    popup.msg_path = NULL;
  }

  G_UNLOCK(trayicon_popup);
}

static void notification_trayicon_popup_free_func(gpointer data)
{
  if(popup.msg_path) {
    g_free(popup.msg_path);
    popup.msg_path = NULL;
  }

  debug_print("Freed notification data\n");
}

static void notification_trayicon_popup_default_action_cb(NotifyNotification
							  *notification,
							  const char *action,
							  void *user_data)
{
  if(strcmp("default", action))
    return;

  MainWindow *mainwin;
  mainwin = mainwindow_get_mainwindow();
  if(mainwin) {
    NotificationFolderType nftype;

    /* Let mainwindow pop up */
    notification_show_mainwindow(mainwin);
    /* If there is only one new mail message, jump to this message */
    nftype = (NotificationFolderType)GPOINTER_TO_INT(user_data);
    if((popup.count == 1) && (nftype == F_TYPE_MAIL)) {
      gchar *select_str;
      G_LOCK(trayicon_popup);
      select_str = g_strdup(popup.msg_path);
      G_UNLOCK(trayicon_popup);
      debug_print("Notification plugin: Select message %s\n", select_str);
      mainwindow_jump_to(select_str, FALSE);
      g_free(select_str);
    }
  }
}

static void notification_trayicon_popup_count_msgs(NotificationFolderType nftype)
{
  switch(nftype) {
  case F_TYPE_MAIL:
    popup.num_mail++;
    break;
  case F_TYPE_NEWS:
    popup.num_news++;
    break;
  case F_TYPE_CALENDAR:
    popup.num_calendar++;
    break;
  case F_TYPE_RSS:
    popup.num_rss++;
    break;
  default:
    debug_print("Notification plugin: Unknown folder type\n");
    return;
  }
  popup.count++;
}

/* The returned value has to be freed by the caller */
static gchar* notification_trayicon_popup_assemble_summary(void)
{
  gchar *summary = NULL;

  if(popup.count == 1) {
    if(popup.num_mail)
      summary = g_strdup(_("New mail message"));
    else if(popup.num_news)
      summary = g_strdup(_("New news post"));
    else if(popup.num_calendar)
      summary = g_strdup(_("New calendar message"));
    else
      summary = g_strdup(_("New article in RSS feed"));
  } /* One new message */
  else {
    summary = g_strdup(_("New messages arrived"));
  } /* Many new messages */

  return summary;
}

/* The returned value has to be freed by the caller */
static gchar* notification_trayicon_popup_assemble_body(MsgInfo *msginfo)
{
  gchar *utf8_str;

  if(popup.count == 1) {
    if(popup.num_mail || popup.num_news) {
      gchar *from;
      gchar *subj;
      gchar *text;
	  gchar *foldname = NULL;

      from = notification_libnotify_sanitize_str(msginfo->from ?
						 msginfo->from :
						 _("(No From)"));
      subj = notification_libnotify_sanitize_str(msginfo->subject ?
						 msginfo->subject :
						 _("(No Subject)"));
  	if (notify_config.trayicon_display_folder_name) {
        foldname = notification_libnotify_sanitize_str(msginfo->folder->path);
        text = g_strconcat(from,"\n\n", subj, "\n\n", foldname, NULL);
	}
    else
        text = g_strconcat(from, "\n\n",subj, NULL);


      /* Make sure text is valid UTF8 */
      utf8_str = notification_validate_utf8_str(text);

      g_free(text);
      g_free(from);
      g_free(subj);
	  if(foldname) g_free(foldname);
    }
    else if(popup.num_calendar) {
      utf8_str = g_strdup(_("A new calendar message arrived"));
    }
    else {
      utf8_str = g_strdup(_("A new article in a RSS feed arrived"));
    }
  } /* One message */

  else {
    gchar *msg;
    gchar *tmp;
    gboolean str_empty = TRUE;

    utf8_str = g_strdup("");

    if(popup.num_mail) {
      msg = g_strdup_printf(ngettext("%d new mail message arrived",
		      		     "%d new mail messages arrived",
		            popup.num_mail),
			    popup.num_mail);
      tmp = g_strdup_printf("%s%s%s",utf8_str,"",msg);
      g_free(msg);
      g_free(utf8_str);
      utf8_str = tmp;
      str_empty = FALSE;
    }
    if(popup.num_news) {
      msg = g_strdup_printf(ngettext("%d new news post arrived",
		      		     "%d new news posts arrived",
		            popup.num_news),
			    popup.num_news);
      tmp = g_strdup_printf("%s%s%s",utf8_str,str_empty?"":"\n",msg);
      g_free(msg);
      g_free(utf8_str);
      utf8_str = tmp;
      str_empty = FALSE;
    }
    if(popup.num_calendar) {
      msg = g_strdup_printf(ngettext("%d new calendar message arrived",
		      		     "%d new calendar messages arrived",
		            popup.num_calendar),
			    popup.num_calendar);
      tmp = g_strdup_printf("%s%s%s",utf8_str,str_empty?"":"\n",msg);
      g_free(msg);
      g_free(utf8_str);
      utf8_str = tmp;
      str_empty = FALSE;
    }
    if(popup.num_rss) {
      msg = g_strdup_printf(ngettext("%d new article in RSS feeds arrived",
		      		     "%d new articles in RSS feeds arrived",
		            popup.num_rss),
			    popup.num_rss);
      tmp = g_strdup_printf("%s%s%s",utf8_str,str_empty?"":"\n",msg);
      g_free(msg);
      g_free(utf8_str);
      utf8_str = tmp;
      str_empty = FALSE;
    }
  } /* Many messages */

  return utf8_str;
}

#endif /* HAVE_LIBNOTIFY */

gboolean notification_trayicon_is_available(void)
{
	gboolean is_available;
	is_available = FALSE;

	if(trayicon) {
		if(gtk_status_icon_is_embedded(trayicon) &&
			 gtk_status_icon_get_visible(trayicon))
			is_available = TRUE;
	}

	return is_available;
}

#endif /* NOTIFICATION_TRAYICON */
