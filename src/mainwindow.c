/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2025 the Claws Mail team and Hiroyuki Yamamoto
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "defs.h"

#ifdef G_OS_WIN32
#define UNICODE
#define _UNICODE
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <string.h>
#include <stdarg.h>

#include "main.h"
#include "mainwindow.h"
#include "folderview.h"
#include "folder_item_prefs.h"
#include "foldersel.h"
#include "quicksearch.h"
#include "summaryview.h"
#include "summary_search.h"
#include "messageview.h"
#include "mimeview.h"
#include "message_search.h"
#include "headerview.h"
#include "menu.h"
#include "stock_pixmap.h"
#include "folder.h"
#include "inc.h"
#include "log.h"
#include "compose.h"
#include "procmsg.h"
#include "import.h"
#include "export.h"
#include "edittags.h"
#include "password.h"
#include "prefs_common.h"
#include "prefs_actions.h"
#include "prefs_filtering.h"
#include "prefs_account.h"
#include "prefs_summary_column.h"
#include "prefs_folder_column.h"
#include "prefs_template.h"
#include "action.h"
#include "account.h"
#ifndef USE_ALT_ADDRBOOK
	#include "addressbook.h"
#else
	#include "addressbook-dbus.h"
#endif
#include "logwindow.h"
#include "manage_window.h"
#include "alertpanel.h"
#include "statusbar.h"
#include "inputdialog.h"
#include "utils.h"
#include "gtkutils.h"
#include "codeconv.h"
#include "about.h"
#include "manual.h"
#include "version.h"
#include "ssl_manager.h"
#include "sslcertwindow.h"
#include "prefs_gtk.h"
#include "pluginwindow.h"
#include "hooks.h"
#include "progressindicator.h"
#include "localfolder.h"
#include "filtering.h"
#include "folderutils.h"
#include "foldersort.h"
#include "icon_legend.h"
#include "colorlabel.h"
#include "tags.h"
#include "textview.h"
#include "imap.h"
#include "news.h"
#include "socket.h"
#include "printing.h"
#include "send_message.h"
#ifdef G_OS_WIN32
#include "w32_reg.h"
#endif

/* list of all instantiated MainWindow */
static GList *mainwin_list = NULL;

static GdkCursor *watch_cursor = NULL;
static GdkCursor *hand_cursor = NULL;

static gint iconified_count = 0;

static void main_window_menu_callback_block	(MainWindow	*mainwin);
static void main_window_menu_callback_unblock	(MainWindow	*mainwin);

static void main_window_show_cur_account	(MainWindow	*mainwin);
#ifndef GENERIC_UMPC
static void main_window_separation_change	(MainWindow	*mainwin,
						 LayoutType	 layout_mode);
#endif
static void main_window_set_widgets		(MainWindow	*mainwin,
						 LayoutType	 layout_mode);

#ifndef GENERIC_UMPC
static gboolean ac_label_button_pressed		(GtkWidget	*widget,
						 GdkEventButton	*event,
						 gpointer	 data);
#endif
static gint main_window_close_cb		(GtkWidget	*widget,
						 GdkEventAny	*event,
						 gpointer	 data);

static void main_window_size_allocate_cb	(GtkWidget	*widget,
						 GtkAllocation	*allocation,
						 gpointer	 data);
static void folder_window_size_allocate_cb	(GtkWidget	*widget,
						 GtkAllocation	*allocation,
						 gpointer	 data);
static void message_window_size_allocate_cb	(GtkWidget	*widget,
						 GtkAllocation	*allocation,
						 gpointer	 data);

static void update_folderview_cb (GtkAction	*action,
				  gpointer	 data);
static void add_mailbox_cb	 (GtkAction	*action,
				  gpointer	 data);
static void foldersort_cb	 (GtkAction	*action,
				  gpointer	 data);
static void import_mbox_cb	 (GtkAction	*action,
				  gpointer	 data);
static void export_mbox_cb	 (GtkAction	*action,
				  gpointer	 data);
static void export_list_mbox_cb  (GtkAction	*action,
				  gpointer	 data);
static void empty_trash_cb	 (GtkAction	*action,
				  gpointer	 data);
static void save_as_cb		 (GtkAction	*action,
				  gpointer	 data);
static void page_setup_cb	 (GtkAction	*action,
				  gpointer	 data);
static void print_cb		 (GtkAction	*action,
				  gpointer	 data);
static void app_exit_cb		 (GtkAction	*action,
				  gpointer	 data);


static void search_cb		 (GtkAction	*action,
				  gpointer	 data);
static void search_folder_cb	 (GtkAction	*action,
				  gpointer	 data);

static void toggle_message_cb	 (GtkAction	*action,
				  gpointer	 data);
static void toggle_toolbar_cb	 (GtkAction *action, GtkRadioAction *current, gpointer data);
static void toggle_col_headers_cb(GtkAction	*action,
				  gpointer	 data);
#ifndef GENERIC_UMPC
static void toggle_statusbar_cb	 (GtkAction	*action,
				  gpointer	 data);
static void set_layout_cb	 (GtkAction *action, GtkRadioAction *current, gpointer data);
#endif
static void addressbook_open_cb	(GtkAction	*action,
				  gpointer	 data);
static void log_window_show_cb	(GtkAction	*action,
				  gpointer	 data);
static void filtering_debug_window_show_cb	(GtkAction	*action,
				  gpointer	 data);
#ifdef G_OS_WIN32
static void debug_log_show_cb(GtkAction *action, gpointer data);
#endif

static void inc_cancel_cb		(GtkAction	*action,
				  gpointer	 data);
static void send_cancel_cb		(GtkAction	*action,
				  gpointer	 data);

static void open_msg_cb			(GtkAction	*action,
				  gpointer	 data);

static void view_source_cb		(GtkAction	*action,
				  gpointer	 data);

static void show_all_header_cb		(GtkAction	*action,
				  gpointer	 data);
static void toggle_fullscreen_cb	(GtkAction	*action,
				  gpointer	 data);
static void toggle_menubar_cb	(GtkAction	*action,
				  gpointer	 data);

static void hide_quotes_cb(GtkAction	*action,
				  gpointer	 data);

static void move_to_cb			(GtkAction	*action,
				  gpointer	 data);
static void copy_to_cb			(GtkAction	*action,
				  gpointer	 data);
static void delete_cb			(GtkAction	*action,
				  gpointer	 data);
static void delete_trash_cb			(GtkAction	*action,
				  gpointer	 data);

static void cancel_cb                   (GtkAction	*action,
				  gpointer	 data);

static void mark_cb			(GtkAction	*action,
				  gpointer	 data);
static void unmark_cb			(GtkAction	*action,
				  gpointer	 data);

static void mark_as_unread_cb		(GtkAction	*action,
				  gpointer	 data);
static void mark_as_read_cb		(GtkAction	*action,
				  gpointer	 data);
static void mark_all_read_cb		(GtkAction	*action,
				  gpointer	 data);
static void mark_all_unread_cb		(GtkAction	*action,
				  gpointer	 data);
static void mark_as_spam_cb		(GtkAction	*action,
				  gpointer	 data);
static void mark_as_ham_cb		(GtkAction	*action,
				  gpointer	 data);

static void ignore_thread_cb		(GtkAction	*action,
				  gpointer	 data);
static void unignore_thread_cb		(GtkAction	*action,
				  gpointer	 data);
static void watch_thread_cb		(GtkAction	*action,
				  gpointer	 data);
static void unwatch_thread_cb		(GtkAction	*action,
				  gpointer	 data);
static void lock_msgs_cb		(GtkAction	*action,
				  gpointer	 data);
static void unlock_msgs_cb		(GtkAction	*action,
				  gpointer	 data);

static void reedit_cb			(GtkAction	*action,
				  gpointer	 data);

static void add_address_cb		(GtkAction	*action,
				  gpointer	 data);

static void set_charset_cb		(GtkAction *action, GtkRadioAction *current, gpointer data);

static void set_decode_cb		(GtkAction *action, GtkRadioAction *current, gpointer data);

static void hide_read_messages   (GtkAction	*action,
				  gpointer	 data);
static void hide_read_threads   (GtkAction	*action,
				  gpointer	 data);
static void hide_del_messages   (GtkAction	*action,
				  gpointer	 data);

static void thread_cb		 (GtkAction	*action,
				  gpointer	 data);
static void expand_threads_cb	 (GtkAction	*action,
				  gpointer	 data);
static void collapse_threads_cb	 (GtkAction	*action,
				  gpointer	 data);

static void set_summary_display_item_cb	 (GtkAction	*action,
				  gpointer	 data);
static void set_folder_display_item_cb	 (GtkAction	*action,
				  gpointer	 data);
static void sort_summary_cb	 (GtkAction *action, GtkRadioAction *current, gpointer data);
static void sort_summary_type_cb (GtkAction *action, GtkRadioAction *current, gpointer data);
static void attract_by_subject_cb(GtkAction	*action,
				  gpointer	 data);

static void delete_duplicated_cb (GtkAction	*action,
				  gpointer	 data);
static void delete_duplicated_all_cb (GtkAction	*action,
				  gpointer	 data);
static void filter_cb		 (GtkAction	*action,
				  gpointer	 data);
static void filter_list_cb	 (GtkAction	*action,
				  gpointer	 data);
static void process_cb		 (GtkAction	*action,
				  gpointer	 data);
static void execute_summary_cb	 (GtkAction	*action,
				  gpointer	 data);
static void expunge_summary_cb	 (GtkAction	*action,
				  gpointer	 data);
static void update_summary_cb	 (GtkAction	*action,
				  gpointer	 data);

static void prev_cb		 (GtkAction	*action,
				  gpointer	 data);
static void next_cb		 (GtkAction	*action,
				  gpointer	 data);
static void next_unread_cb	 (GtkAction	*action,
				  gpointer	 data);
static void prev_unread_cb	 (GtkAction	*action,
				  gpointer	 data);

static void prev_new_cb		 (GtkAction	*action,
				  gpointer	 data);
static void next_new_cb		 (GtkAction	*action,
				  gpointer	 data);
static void prev_marked_cb	 (GtkAction	*action,
				  gpointer	 data);
static void next_marked_cb	 (GtkAction	*action,
				  gpointer	 data);
static void prev_labeled_cb	 (GtkAction	*action,
				  gpointer	 data);
static void next_labeled_cb	 (GtkAction	*action,
				  gpointer	 data);
static void prev_history_cb	 (GtkAction	*action,
				  gpointer	 data);
static void next_history_cb	 (GtkAction	*action,
				  gpointer	 data);
static void parent_cb		 (GtkAction	*action,
				  gpointer	 data);

static void goto_folder_cb	 (GtkAction	*action,
				  gpointer	 data);
static void goto_unread_folder_cb(GtkAction	*action,
				  gpointer	 data);

static void scroll_prev_line_cb  (GtkAction      *action,
                                  gpointer        data);
static void scroll_next_line_cb  (GtkAction      *action,
                                  gpointer        data);
static void scroll_prev_page_cb  (GtkAction      *action,
                                  gpointer        data);
static void scroll_next_page_cb  (GtkAction      *action,
                                  gpointer        data);

static void copy_cb		 (GtkAction	*action,
				  gpointer	 data);
static void allsel_cb		 (GtkAction	*action,
				  gpointer	 data);
static void select_thread_cb	 (GtkAction	*action,
				  gpointer	 data);
static void trash_thread_cb	 (GtkAction	*action,
				  gpointer	 data);
static void delete_thread_cb	 (GtkAction	*action,
				  gpointer	 data);

static void create_filter_cb	 (GtkAction	*action,
				  gpointer	 data);
static void create_processing_cb (GtkAction	*action,
				  gpointer	 data);
static void open_urls_cb	 (GtkAction	*action,
				  gpointer	 data);

static void prefs_template_open_cb	(GtkAction	*action,
				  gpointer	 data);
static void prefs_actions_open_cb	(GtkAction	*action,
				  gpointer	 data);
static void prefs_tags_open_cb		(GtkAction	*action,
				  gpointer	 data);
static void prefs_account_open_cb	(GtkAction	*action,
				  gpointer	 data);

static void prefs_pre_processing_open_cb  (GtkAction	*action,
				  gpointer	 data);

static void prefs_post_processing_open_cb (GtkAction	*action,
				  gpointer	 data);

static void prefs_filtering_open_cb 	(GtkAction	*action,
				  gpointer	 data);
#ifdef USE_GNUTLS
static void ssl_manager_open_cb 	(GtkAction	*action,
				  gpointer	 data);
#endif
static void new_account_cb	 (GtkAction	*action,
				  gpointer	 data);

static void account_selector_menu_cb	 (GtkMenuItem	*menuitem,
					  gpointer	 data);
static void account_receive_menu_cb	 (GtkMenuItem	*menuitem,
					  gpointer	 data);
#ifndef GENERIC_UMPC
static void account_compose_menu_cb	 (GtkMenuItem	*menuitem,
					  gpointer	 data);
#endif
static void prefs_open_cb	(GtkAction	*action,
				  gpointer	 data);
static void plugins_open_cb	(GtkAction	*action,
				  gpointer	 data);

static void online_switch_clicked(GtkButton     *btn, 
				  gpointer data);

static void manual_open_cb	 (GtkAction	*action,
				  gpointer	 data);
static void manual_faq_open_cb	 (GtkAction	*action,
				  gpointer	 data);

static void legend_open_cb	 (GtkAction	*action,
				  gpointer	 data);

#ifdef G_OS_WIN32
static void set_default_client_cb (GtkAction	*action,
				  gpointer	 data);
#endif

static void scan_tree_func	 (Folder	*folder,
				  FolderItem	*item,
				  gpointer	 data);
				  
static void toggle_work_offline_cb(GtkAction	*action,
				  gpointer	 data);

static void addr_harvest_cb	 ( GtkAction	*action,
				  gpointer	 data );

static void addr_harvest_msg_cb	 ( GtkAction	*action,
				  gpointer	 data );
static void sync_cb		 ( GtkAction	*action,
				  gpointer	 data );

static void forget_session_passwords_cb	(GtkAction	*action,
					 gpointer	 data );
#ifndef PASSWORD_CRYPTO_OLD
static void forget_primary_passphrase_cb	(GtkAction	*action,
					 gpointer	 data );
#endif
static gboolean mainwindow_focus_in_event	(GtkWidget	*widget, 
						 GdkEventFocus	*focus,
						 gpointer	 data);
static gboolean mainwindow_visibility_event_cb	(GtkWidget	*widget, 
						 GdkEventVisibility	*state,
						 gpointer	 data);
static gboolean mainwindow_state_event_cb	(GtkWidget	*widget, 
						 GdkEventWindowState	*state,
						 gpointer	 data);
static void main_window_reply_cb			(GtkAction	*action,
				  gpointer	 data);
static gboolean mainwindow_progressindicator_hook	(gpointer 	 source,
						 gpointer 	 userdata);

static gint mailing_list_create_submenu(MainWindow *mainwindow,
				       MsgInfo *msginfo);

static gint mailing_list_populate_submenu(GtkWidget *menu, const gchar * list_header);

static void get_url_part(const gchar **buf, gchar *url_decoded);

static void mailing_list_compose(GtkWidget *w, gpointer *data);

static void mailing_list_open_uri(GtkWidget *w, gpointer *data);

static void mainwindow_quicksearch		(GtkAction	*action,
				  gpointer	 data);
static gboolean any_folder_want_synchronise(void);

static void save_part_as_cb(GtkAction *action, gpointer data);
static void view_part_as_text_cb(GtkAction *action, gpointer data);
static void open_part_cb(GtkAction *action, gpointer data);
#ifndef G_OS_WIN32
static void open_part_with_cb(GtkAction *action, gpointer data);
#endif
static void check_signature_cb(GtkAction *action, gpointer data);
static void goto_next_part_cb(GtkAction *action, gpointer data);
static void goto_prev_part_cb(GtkAction *action, gpointer data);
#define DO_ACTION(name, act)	{ if (!strcmp(a_name, name)) action = act; }

static void mainwindow_nothing_cb	   (GtkAction *action, gpointer data)
{

}

static void about_cb(GtkAction *gaction, gpointer data)
{
	about_show();
}

static void	mw_inc_mail_cb			(GtkAction *gaction, gpointer data)
{
	inc_mail_cb(data, 0, NULL);
}
static void 	mw_inc_all_account_mail_cb		(GtkAction *gaction, gpointer data)
{
	inc_all_account_mail_cb(data, 0, NULL);
}
static void 	mw_send_queue_cb			(GtkAction *gaction, gpointer data)
{
	send_queue_cb(data, 0, NULL);
}
static void 	mw_compose_mail_cb			(GtkAction *gaction, gpointer data)
{
	compose_mail_cb(data, 0, NULL);
}
static void 	mw_compose_news_cb			(GtkAction *gaction, gpointer data)
{
	compose_news_cb(data, 0, NULL);
}

/* make sure to keep the tables below in sync with the default_menurc[] in prefs_other.c */
static GtkActionEntry mainwin_entries[] =
{
	{"Menu",                        NULL, "Menu", NULL, NULL, NULL },
/* menus */
	{"File",                        NULL, N_("_File"), NULL, NULL, NULL },
	{"Edit",                        NULL, N_("_Edit"), NULL, NULL, NULL },
	{"View",                        NULL, N_("_View"), NULL, NULL, NULL },
	{"Message",                     NULL, N_("_Message"), NULL, NULL, NULL },
	{"Tools",                       NULL, N_("_Tools"), NULL, NULL, NULL },
	{"Configuration",               NULL, N_("_Configuration"), NULL, NULL, NULL },
	{"Help",                        NULL, N_("_Help"), NULL, NULL, NULL },

/* File menu */
	{"File/AddMailbox",             NULL, N_("_Add mailbox"), NULL, NULL, NULL },
	{"File/AddMailbox/MH",          NULL, N_("MH..."), NULL, NULL, G_CALLBACK(add_mailbox_cb) },
	{"File/---",                    NULL, "---", NULL, NULL, NULL },

	{"File/SortMailboxes",          NULL, N_("Change mailbox order..."), NULL, NULL, G_CALLBACK(foldersort_cb) },

	/* {"File/---",                 NULL, "---", NULL, NULL, NULL }, */
	{"File/ImportMbox",             NULL, N_("_Import mbox file..."), NULL, NULL, G_CALLBACK(import_mbox_cb) },
	{"File/ExportMbox",             NULL, N_("_Export to mbox file..."), NULL, NULL, G_CALLBACK(export_mbox_cb) },
	{"File/ExportSelMbox",          NULL, N_("_Export selected to mbox file..."), NULL, NULL, G_CALLBACK(export_list_mbox_cb) },
	/* {"File/---",                 NULL, "---", NULL, NULL, NULL }, */
	{"File/EmptyTrashes",           NULL, N_("Empty all _Trash folders"), "<shift>D", NULL, G_CALLBACK(empty_trash_cb) },
	/* {"File/---",                 NULL, "---", NULL, NULL, NULL }, */

	{"File/SaveAs",                 NULL, N_("_Save email as..."), "<control>S", NULL, G_CALLBACK(save_as_cb) },
	{"File/SavePartAs",             NULL, N_("_Save part as..."), "Y", NULL, G_CALLBACK(save_part_as_cb) },
	/* {"File/---",                 NULL, "---", NULL, NULL, NULL }, */

	{"File/PageSetup",              NULL, N_("Page setup..."), NULL, NULL, G_CALLBACK(page_setup_cb) },
	{"File/Print",                  NULL, N_("_Print..."), "<control>P", NULL, G_CALLBACK(print_cb) },
	/* {"File/---",                 NULL, "---", NULL, NULL, NULL }, */
	{"File/SynchroniseFolders",     NULL, N_("Synchronise folders"), "<shift><control>S", NULL, G_CALLBACK(sync_cb) }, 
	/* {"File/---",                 NULL, "---", NULL, NULL, NULL }, */
	{"File/Exit",                   NULL, N_("E_xit"), "<control>Q", NULL, G_CALLBACK(app_exit_cb) }, 

/* Edit menu */
	{"Edit/Copy",                   NULL, N_("_Copy"), "<control>C", NULL, G_CALLBACK(copy_cb) }, 
	{"Edit/SelectAll",              NULL, N_("Select _all"), "<control>A", NULL, G_CALLBACK(allsel_cb) }, 
	{"Edit/SelectThread",           NULL, N_("Select _thread"), NULL, NULL, G_CALLBACK(select_thread_cb) }, 
	{"Edit/---",                    NULL, "---", NULL, NULL, NULL },
	{"Edit/Find",                   NULL, N_("_Find in current message..."), "<control>F", NULL, G_CALLBACK(search_cb) },
	{"Edit/SearchFolder",			NULL, N_("_Search folder..."), "<shift><control>F", NULL, G_CALLBACK(search_folder_cb) },
	{"Edit/QuickSearch",			NULL, N_("_Quick search"), "slash", NULL, G_CALLBACK(mainwindow_quicksearch) },

/* View menu */
	{"View/ShowHide",               NULL, N_("Show or hi_de"), NULL, NULL, NULL },
	{"View/ShowHide/Toolbar",       NULL, N_("_Toolbar"), NULL, NULL, NULL },

	{"View/SetColumns",             NULL, N_("Set displayed _columns"), NULL, NULL, NULL },
	{"View/SetColumns/Folderlist",  NULL, N_("In _folder list..."), NULL, NULL, G_CALLBACK(set_folder_display_item_cb) },
	{"View/SetColumns/Messagelist", NULL, N_("In _message list..."), NULL, NULL, G_CALLBACK(set_summary_display_item_cb) },
	{"View/---",                    NULL, "---", NULL, NULL, NULL },


#ifndef GENERIC_UMPC
	{"View/Layout",                 NULL, N_("La_yout"), NULL, NULL, NULL },
#endif
	{"View/Sort",                   NULL, N_("_Sort"), NULL, NULL, NULL },
	{"View/Sort/---",               NULL, "---", NULL, NULL, NULL }, 
	{"View/Sort/AttractSubj",		NULL, N_("_Attract by subject"), NULL, NULL, G_CALLBACK(attract_by_subject_cb) }, 

	{"View/ExpandThreads",			NULL, N_("E_xpand all threads"), NULL, NULL, G_CALLBACK(expand_threads_cb) }, 
	{"View/CollapseThreads",		NULL, N_("Co_llapse all threads"), NULL, NULL, G_CALLBACK(collapse_threads_cb) }, 

	{"View/Goto",                   NULL, N_("_Go to"), NULL, NULL, NULL },
	{"View/Goto/Prev",              NULL, N_("_Previous message"), "P", NULL, G_CALLBACK(prev_cb) },
	{"View/Goto/Next",              NULL, N_("_Next message"), "N", NULL, G_CALLBACK(next_cb) },
	{"View/Goto/---",               NULL, "---", NULL, NULL, NULL },
	{"View/Goto/PrevUnread",        NULL, N_("P_revious unread message"), "<shift>P", NULL, G_CALLBACK(prev_unread_cb) },
	{"View/Goto/NextUnread",        NULL, N_("N_ext unread message"), "<shift>N", NULL, G_CALLBACK(next_unread_cb) },
	/* {"View/Goto/---",            NULL, "---", NULL, NULL, NULL }, */
	{"View/Goto/PrevNew",           NULL, N_("Previous ne_w message"), NULL, NULL, G_CALLBACK(prev_new_cb) },
	{"View/Goto/NextNew",           NULL, N_("Ne_xt new message"), NULL, NULL, G_CALLBACK(next_new_cb) },
	/* {"View/Goto/---",            NULL, "---", NULL, NULL, NULL }, */
	{"View/Goto/PrevMarked",        NULL, N_("Previous _marked message"), NULL, NULL, G_CALLBACK(prev_marked_cb) },
	{"View/Goto/NextMarked",        NULL, N_("Next m_arked message"), NULL, NULL, G_CALLBACK(next_marked_cb) },
	/* {"View/Goto/---",            NULL, "---", NULL, NULL, NULL }, */
	{"View/Goto/PrevLabeled",       NULL, N_("Previous _labeled message"), NULL, NULL, G_CALLBACK(prev_labeled_cb) },
	{"View/Goto/NextLabeled",       NULL, N_("Next la_beled message"), NULL, NULL, G_CALLBACK(next_labeled_cb) },
	/* {"View/Goto/---",            NULL, "---", NULL, NULL, NULL }, */
	{"View/Goto/PrevHistory",       NULL, N_("Previously opened message"), "<alt>Left", NULL, G_CALLBACK(prev_history_cb) },
	{"View/Goto/NextHistory",       NULL, N_("Next opened message"), "<alt>Right", NULL, G_CALLBACK(next_history_cb) },
	/* {"View/Goto/---",            NULL, "---", NULL, NULL, NULL }, */
	{"View/Goto/ParentMessage",     NULL, N_("Parent message"), "<control>Up", NULL, G_CALLBACK(parent_cb) },
	/* {"View/Goto/---",            NULL, "---", NULL, NULL, NULL }, */
	{"View/Goto/NextUnreadFolder",  NULL, N_("Next unread _folder"), "<shift>G", NULL, G_CALLBACK(goto_unread_folder_cb) },
	{"View/Goto/Folder",            NULL, N_("F_older..."), "G", NULL, G_CALLBACK(goto_folder_cb) },
	/* {"View/Goto/---",            NULL, "---", NULL, NULL, NULL }, */
	{"View/Goto/NextPart",          NULL, N_("Next part"), "A", NULL, G_CALLBACK(goto_next_part_cb) },
	{"View/Goto/PrevPart",          NULL, N_("Previous part"), "Z", NULL, G_CALLBACK(goto_prev_part_cb) },

	/* {"View/Scroll/---",          NULL, "---", NULL, NULL, NULL }, */
	{"View/Scroll",                 NULL, N_("Message scroll"), NULL, NULL, NULL },
	{"View/Scroll/PrevLine",        NULL, N_("Previous line"), NULL, NULL, G_CALLBACK(scroll_prev_line_cb) },
	{"View/Scroll/NextLine",        NULL, N_("Next line"), NULL, NULL, G_CALLBACK(scroll_next_line_cb) },
	{"View/Scroll/PrevPage",        NULL, N_("Previous page"), NULL, NULL, G_CALLBACK(scroll_prev_page_cb) },
	{"View/Scroll/NextPage",        NULL, N_("Next page"), NULL, NULL, G_CALLBACK(scroll_next_page_cb) },

	/* {"View/---",                 NULL, "---", NULL, NULL, NULL }, */
	{"View/Encoding",               NULL, N_("Character _encoding"), NULL, NULL, NULL }, /* set_charset_cb */
	{"View/Encoding/---",           NULL, "---", NULL, NULL, NULL },
#define ENC_ACTION(cs_char,c_char,string) \
	{ "View/Encoding/" cs_char, NULL, N_(string), NULL, NULL, c_char }

	{"View/Encoding/Western",       NULL, N_("Western European"), NULL, NULL, NULL },
	{"View/Encoding/Baltic",        NULL, N_("Baltic"), NULL, NULL, NULL },
	{"View/Encoding/Hebrew",        NULL, N_("Hebrew"), NULL, NULL, NULL },
	{"View/Encoding/Arabic",        NULL, N_("Arabic"), NULL, NULL, NULL },
	{"View/Encoding/Cyrillic",      NULL, N_("Cyrillic"), NULL, NULL, NULL },
	{"View/Encoding/Japanese",      NULL, N_("Japanese"), NULL, NULL, NULL },
	{"View/Encoding/Chinese",       NULL, N_("Chinese"), NULL, NULL, NULL },
	{"View/Encoding/Korean",        NULL, N_("Korean"), NULL, NULL, NULL },
	{"View/Encoding/Thai",          NULL, N_("Thai"), NULL, NULL, NULL },

	{"View/Decode",                 NULL, N_("Decode"), NULL, NULL, NULL }, /* set_decode_cb */
	{"View/Decode/---",             NULL, "---", NULL, NULL, NULL },

#define DEC_ACTION(cs_type,c_type,string) \
	{ "View/Decode/" cs_type, NULL, N_(string), NULL, NULL, c_type }

	/* {"View/---",                 NULL, "---", NULL, NULL, NULL }, */
	{"View/OpenNewWindow",          NULL, N_("Open in new _window"), "<control><alt>N", NULL, G_CALLBACK(open_msg_cb) },
	{"View/MessageSource",          NULL, N_("Mess_age source"), "<control>U", NULL, G_CALLBACK(view_source_cb) },
	/* {"View/---",                 NULL, "---", NULL, NULL, NULL }, */
	{"View/Part",                   NULL, N_("Message part"), NULL, NULL, NULL },
	{"View/Part/AsText",            NULL, N_("View as text"), "T", NULL, G_CALLBACK(view_part_as_text_cb) },
	{"View/Part/Open",              NULL, N_("Open"), "L", NULL, G_CALLBACK(open_part_cb) },
#ifndef G_OS_WIN32
	{"View/Part/OpenWith",          NULL, N_("Open with..."), "O", NULL, G_CALLBACK(open_part_with_cb) },
#endif
	/* {"View/---",                 NULL, "---", NULL, NULL, NULL }, */

	{"View/Quotes",                 NULL, N_("Quotes"), NULL, NULL, NULL }, 
	/* {"View/---",                 NULL, "---", NULL, NULL, NULL }, */
	{"View/UpdateSummary",          NULL, N_("_Update summary"), "<control><alt>U", NULL, G_CALLBACK(update_summary_cb) },

/* Message menu */
	{"Message/Receive",                              NULL, N_("Recei_ve"), NULL, NULL, NULL },
	{"Message/Receive/CurrentAccount",               NULL, N_("Get from _current account"), "<control>I", NULL, G_CALLBACK(mw_inc_mail_cb) },
	{"Message/Receive/AllAccounts",                  NULL, N_("Get from _all accounts"), "<shift><control>I", NULL, G_CALLBACK(mw_inc_all_account_mail_cb) },
	{"Message/Receive/CancelReceiving",              NULL, N_("Cancel receivin_g"), NULL, NULL, G_CALLBACK(inc_cancel_cb) },
	{"Message/Receive/---",                          NULL, "---", NULL, NULL, NULL },
	{"Message/Receive/PlaceHolder",                  NULL, "PlaceHolder,", NULL, NULL, G_CALLBACK(mainwindow_nothing_cb) },
	{"Message/SendQueue",                            NULL, N_("_Send queued messages"), NULL, NULL, G_CALLBACK(mw_send_queue_cb) },
	{"Message/CancelSending",                        NULL, N_("Cancel sending"), NULL, NULL, G_CALLBACK(send_cancel_cb) },

	{"Message/---",                                  NULL, "---", NULL, NULL, NULL },

	{"Message/ComposeEmail",                         NULL, N_("Compose a_n email message"), "<control>M", NULL, G_CALLBACK(mw_compose_mail_cb) },
	{"Message/ComposeNews",                          NULL, N_("Compose a news message"), NULL, NULL, G_CALLBACK(mw_compose_news_cb) },

	{"Message/Reply",                                NULL, N_("_Reply"), "<control>R", NULL, G_CALLBACK(main_window_reply_cb) }, /* COMPOSE_REPLY */
	{"Message/ReplyTo",                              NULL, N_("Repl_y to"), NULL, NULL, NULL }, 
	{"Message/ReplyTo/All",                          NULL, N_("_All"), "<shift><control>R", NULL, G_CALLBACK(main_window_reply_cb) }, /* COMPOSE_REPLY_TO_ALL */
	{"Message/ReplyTo/Sender",                       NULL, N_("_Sender"), NULL, NULL, G_CALLBACK(main_window_reply_cb) }, /* COMPOSE_REPLY_TO_SENDER */
	{"Message/ReplyTo/List",                         NULL, N_("Mailing _list"), "<control>L", NULL, G_CALLBACK(main_window_reply_cb) }, /* COMPOSE_REPLY_TO_LIST */
	{"Message/FollowupReply",                        NULL, N_("Follow-up and reply to"), NULL, NULL, G_CALLBACK(main_window_reply_cb) }, /* COMPOSE_FOLLOWUP_AND_REPLY_TO */
	/* {"Message/---",                               NULL, "---", NULL, NULL, NULL }, */

	{"Message/Forward",                              NULL, N_("_Forward"), "<control><alt>F", NULL, G_CALLBACK(main_window_reply_cb) }, /* COMPOSE_FORWARD_INLINE */
	{"Message/ForwardAtt",                           NULL, N_("For_ward as attachment"), NULL, NULL, G_CALLBACK(main_window_reply_cb) }, /* COMPOSE_FORWARD_AS_ATTACH */
	{"Message/Redirect",                             NULL, N_("Redirec_t"), NULL, NULL, G_CALLBACK(main_window_reply_cb) }, /* COMPOSE_REDIRECT */

	{"Message/MailingList",                          NULL, N_("Mailing-_List"), NULL, NULL, NULL }, 
	{"Message/MailingList/Post",                     NULL, N_("Post"), NULL, NULL, NULL }, 
	{"Message/MailingList/Post/PlaceHolder",         NULL, "Placeholder", NULL, NULL, G_CALLBACK(mainwindow_nothing_cb) },
	{"Message/MailingList/Help",                     NULL, N_("Help"), NULL, NULL, NULL }, 
	{"Message/MailingList/Help/PlaceHolder",         NULL, "Placeholder", NULL, NULL, G_CALLBACK(mainwindow_nothing_cb) },
	{"Message/MailingList/Subscribe",                NULL, N_("Subscribe"), NULL, NULL, NULL }, 
	{"Message/MailingList/Subscribe/PlaceHolder",    NULL, "Placeholder", NULL, NULL, G_CALLBACK(mainwindow_nothing_cb) },
	{"Message/MailingList/Unsubscribe",              NULL, N_("Unsubscribe"), NULL, NULL, NULL }, 
	{"Message/MailingList/Unsubscribe/PlaceHolder",  NULL, "Placeholder", NULL, NULL, G_CALLBACK(mainwindow_nothing_cb) },
	{"Message/MailingList/ViewArchive",              NULL, N_("View archive"), NULL, NULL, NULL }, 
	{"Message/MailingList/ViewArchive/PlaceHolder",  NULL, "Placeholder", NULL, NULL, G_CALLBACK(mainwindow_nothing_cb) },
	{"Message/MailingList/ContactOwner",             NULL, N_("Contact owner"), NULL, NULL, NULL }, 
	{"Message/MailingList/ContactOwner/PlaceHolder", NULL, "Placeholder", NULL, NULL, G_CALLBACK(mainwindow_nothing_cb) },
	/* separation */

	{"Message/Move",                             NULL, N_("M_ove..."), "<control>O", NULL, G_CALLBACK(move_to_cb) },
	{"Message/Copy",                             NULL, N_("_Copy..."), "<shift><control>O", NULL, G_CALLBACK(copy_to_cb) },
	{"Message/Trash",                            NULL, N_("Move to _trash"), "<control>D", NULL, G_CALLBACK(delete_trash_cb) },
	{"Message/Delete",                           NULL, N_("_Delete..."), NULL, NULL, G_CALLBACK(delete_cb) },
	{"Message/TrashThread",                      NULL, N_("Move thread to tr_ash"), NULL, NULL, G_CALLBACK(trash_thread_cb) }, 
	{"Message/DeleteThread",                     NULL, N_("Delete t_hread"), NULL, NULL, G_CALLBACK(delete_thread_cb) }, 
	{"Message/CancelNews",                       NULL, N_("Cancel a news message"), NULL, NULL, G_CALLBACK(cancel_cb) },
	/* separation */
	{"Message/Marks",                            NULL, N_("_Marks"), NULL, NULL, NULL },
	{"Message/Marks/Mark",                       NULL, N_("_Mark"), "<shift>asterisk", NULL, G_CALLBACK(mark_cb) },
	{"Message/Marks/Unmark",                     NULL, N_("_Unmark"), "U", NULL, G_CALLBACK(unmark_cb) },
	{"Message/Marks/---",                        NULL, "---", NULL, NULL, NULL },

	{"Message/Marks/MarkRead",                   NULL, N_("Mark as rea_d"), NULL, NULL, G_CALLBACK(mark_as_read_cb) },
	{"Message/Marks/MarkUnread",                 NULL, N_("Mark as unr_ead"), "<shift>exclam", NULL, G_CALLBACK(mark_as_unread_cb) },
	/* separation */
	{"Message/Marks/MarkAllRead",                NULL, N_("Mark all read in folder"), NULL, NULL, G_CALLBACK(mark_all_read_cb) },
	{"Message/Marks/MarkAllUnread",              NULL, N_("Mark all unread in folder"), NULL, NULL, G_CALLBACK(mark_all_unread_cb) },
	/* separation */
	{"Message/Marks/IgnoreThread",               NULL, N_("Ignore thread"), NULL, NULL, G_CALLBACK(ignore_thread_cb) },
	{"Message/Marks/UnignoreThread",             NULL, N_("Unignore thread"), NULL, NULL, G_CALLBACK(unignore_thread_cb) },
	{"Message/Marks/WatchThread",                NULL, N_("Watch thread"), NULL, NULL, G_CALLBACK(watch_thread_cb) },
	{"Message/Marks/UnwatchThread",              NULL, N_("Unwatch thread"), NULL, NULL, G_CALLBACK(unwatch_thread_cb) },
	/* separation */

	{"Message/Marks/MarkSpam",                   NULL, N_("Mark as _spam"), NULL, NULL, G_CALLBACK(mark_as_spam_cb) },
	{"Message/Marks/MarkHam",                    NULL, N_("Mark as _ham"), NULL, NULL, G_CALLBACK(mark_as_ham_cb) },
	/* separation */

	{"Message/Marks/Lock",                       NULL, N_("Lock"), NULL, NULL, G_CALLBACK(lock_msgs_cb) },
	{"Message/Marks/Unlock",                     NULL, N_("Unlock"), NULL, NULL, G_CALLBACK(unlock_msgs_cb) },

	{"Message/ColorLabel",                       NULL, N_("Color la_bel"), NULL, NULL, NULL },
	{"Message/Tags",                             NULL, N_("Ta_gs"), NULL, NULL, NULL },
	/*{"Message/---",                            NULL, "---", NULL, NULL, NULL },*/

	{"Message/Reedit",                           NULL, N_("Re-_edit"), NULL, NULL, G_CALLBACK(reedit_cb) },
	/*{"Message/---",                            NULL, "---", NULL, NULL, NULL },*/

	{"Message/CheckSignature",                   NULL, N_("Check signature"), "C", NULL, G_CALLBACK(check_signature_cb) },

/* Tools menu */

	{"Tools/AddressBook",                        NULL, N_("_Address book"), "<shift><control>A", NULL, G_CALLBACK(addressbook_open_cb) }, 
	{"Tools/AddSenderToAB",                      NULL, N_("Add sender to address boo_k"), NULL, NULL, G_CALLBACK(add_address_cb) }, 

	{"Tools/CollectAddresses",                   NULL, N_("C_ollect addresses"), NULL, NULL, NULL }, 
	{"Tools/CollectAddresses/FromFolder",        NULL, N_("From current _folder..."), NULL, NULL, G_CALLBACK(addr_harvest_cb) }, 
	{"Tools/CollectAddresses/FromSelected",      NULL, N_("From selected _messages..."), NULL, NULL, G_CALLBACK(addr_harvest_msg_cb) }, 
	{"Tools/---",                                NULL, "---", NULL, NULL, NULL },

	{"Tools/FilterFolder",                       NULL, N_("_Filter all messages in folder"), NULL, NULL, G_CALLBACK(filter_cb) }, 
	{"Tools/FilterSelected",                     NULL, N_("Filter _selected messages"), NULL, NULL, G_CALLBACK(filter_list_cb) }, 
	{"Tools/RunProcessing",                      NULL, N_("Run folder pr_ocessing rules"), NULL, NULL, G_CALLBACK(process_cb) }, 

	{"Tools/CreateFilterRule",                   NULL, N_("_Create filter rule"), NULL, NULL, NULL },
	{"Tools/CreateFilterRule/Automatically",     NULL, N_("_Automatically"), NULL, NULL, G_CALLBACK(create_filter_cb) }, /* FILTER_BY_AUTO */
	{"Tools/CreateFilterRule/ByFrom",            NULL, N_("By _From"), NULL, NULL, G_CALLBACK(create_filter_cb) }, /* FILTER_BY_FROM */
	{"Tools/CreateFilterRule/ByTo",              NULL, N_("By _To"), NULL, NULL, G_CALLBACK(create_filter_cb) }, /* FILTER_BY_TO     */
	{"Tools/CreateFilterRule/BySubject",         NULL, N_("By _Subject"), NULL, NULL, G_CALLBACK(create_filter_cb) }, /* FILTER_BY_SUBJECT */
	{"Tools/CreateFilterRule/BySender",          NULL, N_("By S_ender"), NULL, NULL, G_CALLBACK(create_filter_cb) }, /* FILTER_BY_SENDER */

	{"Tools/CreateProcessingRule",               NULL, N_("Create processing rule"), NULL, NULL, NULL },
	{"Tools/CreateProcessingRule/Automatically", NULL, N_("_Automatically"), NULL, NULL, G_CALLBACK(create_processing_cb) }, 
	{"Tools/CreateProcessingRule/ByFrom",        NULL, N_("By _From"), NULL, NULL, G_CALLBACK(create_processing_cb) }, 
	{"Tools/CreateProcessingRule/ByTo",          NULL, N_("By _To"), NULL, NULL, G_CALLBACK(create_processing_cb) }, 
	{"Tools/CreateProcessingRule/BySubject",     NULL, N_("By _Subject"), NULL, NULL, G_CALLBACK(create_processing_cb) }, 
	{"Tools/CreateProcessingRule/BySender",      NULL, N_("By S_ender"), NULL, NULL, G_CALLBACK(create_processing_cb) },
	/* {"Tools/---",                             NULL, "---", NULL, NULL, NULL }, */

	{"Tools/ListUrls",                           NULL, N_("List _URLs..."), "<shift><control>U", NULL, G_CALLBACK(open_urls_cb) }, 

	/* {"Tools/---",                             NULL, "---", NULL, NULL, NULL }, */
	{"Tools/Actions",                            NULL, N_("Actio_ns"), NULL, NULL, NULL },
	{"Tools/Actions/PlaceHolder",                NULL, "Placeholder", NULL, NULL, G_CALLBACK(mainwindow_nothing_cb) },
	/* {"Tools/---",                             NULL, "---", NULL, NULL, NULL }, */

	{"Tools/CheckNewMessages",                   NULL, N_("Ch_eck for new messages in all folders"), NULL, NULL, G_CALLBACK(update_folderview_cb) }, 
	{"Tools/DeleteDuplicates",                   NULL, N_("Delete du_plicated messages"), NULL, NULL, NULL },
	{"Tools/DeleteDuplicates/SelFolder",         NULL, N_("In selected folder"), NULL, NULL, G_CALLBACK(delete_duplicated_cb) },
	{"Tools/DeleteDuplicates/AllFolders",        NULL, N_("In all folders"), NULL, NULL, G_CALLBACK(delete_duplicated_all_cb) },
	/* {"Tools/---",                             NULL, "---", NULL, NULL, NULL }, */

	{"Tools/Execute",                            NULL, N_("E_xecute"), "X", NULL, G_CALLBACK(execute_summary_cb) }, 
	{"Tools/Expunge",                            NULL, N_("Exp_unge"), "<control>E", NULL, G_CALLBACK(expunge_summary_cb) }, 
#ifdef USE_GNUTLS
	/* {"Tools/---",                             NULL, "---", NULL, NULL, NULL }, */
	{"Tools/TLSCertificates",                    NULL, N_("TLS cer_tificates"), NULL, NULL, G_CALLBACK(ssl_manager_open_cb) }, 
#endif
	/* {"Tools/---",                             NULL, "---", NULL, NULL, NULL }, */
	{"Tools/FilteringLog",                       NULL, N_("Filtering Lo_g"), NULL, NULL, G_CALLBACK(filtering_debug_window_show_cb) }, 
	{"Tools/NetworkLog",                         NULL, N_("Network _Log"), "<shift><control>L", NULL, G_CALLBACK(log_window_show_cb) }, 
#ifdef G_OS_WIN32
	{"Tools/DebugLog",                           NULL, N_("Debug _Log"), NULL, NULL, G_CALLBACK(debug_log_show_cb) },
#endif
	/* {"Tools/---",                             NULL, "---", NULL, NULL, NULL }, */
	{"Tools/ForgetSessionPasswords",             NULL, N_("_Forget all session passwords"), NULL, NULL, G_CALLBACK(forget_session_passwords_cb) }, 
#ifndef PASSWORD_CRYPTO_OLD
	{"Tools/ForgetPrimaryPassphrase",             NULL, N_("Forget _primary passphrase"), NULL, NULL, G_CALLBACK(forget_primary_passphrase_cb) },
#endif

/* Configuration menu */	
	{"Configuration/ChangeAccount",              NULL, N_("C_hange current account"), NULL, NULL, NULL },
	{"Configuration/ChangeAccount/PlaceHolder",  NULL, "Placeholder", NULL, NULL, G_CALLBACK(mainwindow_nothing_cb) },
	{"Configuration/AccountPrefs",               NULL, N_("_Preferences for current account..."), NULL, NULL, G_CALLBACK(prefs_account_open_cb) },
	{"Configuration/CreateAccount",              NULL, N_("Create _new account..."), NULL, NULL, G_CALLBACK(new_account_cb) },
	{"Configuration/EditAccounts",               NULL, N_("_Edit accounts..."), NULL, NULL, G_CALLBACK(account_edit_open) },
	{"Configuration/---",                        NULL, "---", NULL, NULL, NULL }, 

	{"Configuration/Preferences",                NULL, N_("P_references..."), NULL, NULL, G_CALLBACK(prefs_open_cb) },
	{"Configuration/PreProcessing",              NULL, N_("Pre-pr_ocessing..."), NULL, NULL, G_CALLBACK(prefs_pre_processing_open_cb) },
	{"Configuration/PostProcessing",             NULL, N_("Post-pro_cessing..."), NULL, NULL, G_CALLBACK(prefs_post_processing_open_cb) },
	{"Configuration/Filtering",                  NULL, N_("_Filtering..."), NULL, NULL, G_CALLBACK(prefs_filtering_open_cb) },
	{"Configuration/Templates",                  NULL, N_("_Templates..."), NULL, NULL, G_CALLBACK(prefs_template_open_cb) },
	{"Configuration/Actions",                    NULL, N_("_Actions..."), NULL, NULL, G_CALLBACK(prefs_actions_open_cb) },
	{"Configuration/Tags",                       NULL, N_("Tag_s..."), NULL, NULL, G_CALLBACK(prefs_tags_open_cb) },
	/* {"Configuration/---",                     NULL, "---", NULL, NULL, NULL }, */
	{"Configuration/Plugins",                    NULL, N_("Plu_gins..."), NULL, NULL, G_CALLBACK(plugins_open_cb) },

/* Help menu */
	{"Help/Manual",                              NULL, N_("_Manual"), NULL, NULL, G_CALLBACK(manual_open_cb) }, 
	{"Help/FAQ",                                 NULL, N_("_Online User-contributed FAQ"), NULL, NULL, G_CALLBACK(manual_faq_open_cb) }, 
	{"Help/IconLegend",                          NULL, N_("Icon _Legend"), NULL, NULL, G_CALLBACK(legend_open_cb) }, 
#ifdef G_OS_WIN32
	{"Help/SetDefault",                          NULL, N_("Set as default client"), NULL, NULL, G_CALLBACK(set_default_client_cb) }, 
#endif
	{"Help/---",                                 NULL, "---", NULL, NULL, NULL }, 
	{"Help/About",                               NULL, N_("_About"), NULL, NULL, G_CALLBACK(about_cb) }, 
};

static GtkToggleActionEntry mainwin_toggle_entries[] = {
	{"File/OfflineMode",                     NULL, N_("Offline _mode"), "<control>W", NULL, G_CALLBACK(toggle_work_offline_cb), FALSE }, /*toggle*/
	{"View/ShowHide/MenuBar",                NULL, N_("Men_ubar"), "<control>F12", NULL, G_CALLBACK(toggle_menubar_cb), FALSE }, /* toggle */
	{"View/ShowHide/MessageView",            NULL, N_("_Message view"), "V", NULL, G_CALLBACK(toggle_message_cb), FALSE }, /* toggle */
#ifndef GENERIC_UMPC
	{"View/ShowHide/StatusBar",              NULL, N_("Status _bar"), NULL, NULL, G_CALLBACK(toggle_statusbar_cb), FALSE }, /* toggle */
#endif
	{"View/ShowHide/ColumnHeaders",          NULL, N_("Column headers"), NULL, NULL, G_CALLBACK(toggle_col_headers_cb), FALSE }, /* toggle */
	{"View/ThreadView",                      NULL, N_("Th_read view"), "<control>T", NULL, G_CALLBACK(thread_cb), FALSE }, /* toggle */
	{"View/HideReadThreads",                 NULL, N_("Hide read threads"), NULL, NULL, G_CALLBACK(hide_read_threads), FALSE }, /* toggle */
	{"View/HideReadMessages",                NULL, N_("_Hide read messages"), NULL, NULL, G_CALLBACK(hide_read_messages), FALSE }, /* toggle */
	{"View/HideDelMessages",                 NULL, N_("Hide deleted messages"), NULL, NULL, G_CALLBACK(hide_del_messages), FALSE }, /* toggle */
	{"View/FullScreen",                      NULL, N_("_Fullscreen"), "F11", NULL, G_CALLBACK(toggle_fullscreen_cb), FALSE }, /* toggle */
	{"View/AllHeaders",                      NULL, N_("Show all _headers"), "<control>H", NULL, G_CALLBACK(show_all_header_cb), FALSE }, /* toggle */
	{"View/Quotes/CollapseAll",              NULL, N_("_Collapse all"), "<shift><control>Q", NULL, G_CALLBACK(hide_quotes_cb), FALSE }, /* 1 toggle */
	{"View/Quotes/Collapse2",                NULL, N_("Collapse from level _2"), NULL, NULL, G_CALLBACK(hide_quotes_cb), FALSE }, /* 2 toggle */
	{"View/Quotes/Collapse3",                NULL, N_("Collapse from level _3"), NULL, NULL, G_CALLBACK(hide_quotes_cb), FALSE }, /* 3 toggle */
};

static GtkRadioActionEntry mainwin_showhide_radio_entries[] = { /* toggle_toolbar_cb */
	{"View/ShowHide/Toolbar/TextBelowIcon",  NULL, N_("Text _below icons"), NULL, NULL, TOOLBAR_BOTH }, /* radio TOOLBAR_BOTH */
	{"View/ShowHide/Toolbar/TextBesideIcon", NULL, N_("Text be_side icons"), NULL, NULL, TOOLBAR_BOTH_HORIZ }, /* radio TOOLBAR_BOTH_HORIZ */
	{"View/ShowHide/Toolbar/IconOnly",       NULL, N_("_Icons only"), NULL, NULL, TOOLBAR_ICON }, /* radio TOOLBAR_ICON */
	{"View/ShowHide/Toolbar/TextOnly",       NULL, N_("_Text only"), NULL, NULL, TOOLBAR_TEXT }, /* radio TOOLBAR_TEXT */
#ifndef GENERIC_UMPC
	{"View/ShowHide/Toolbar/Hide",           NULL, N_("_Hide"), NULL, NULL, TOOLBAR_NONE }, /* radio TOOLBAR_NONE */
#endif
};
#ifndef GENERIC_UMPC
static GtkRadioActionEntry mainwin_layout_radio_entries[] = { /* set_layout_cb */
	{"View/Layout/Standard",                 NULL, N_("_Standard"), NULL, NULL, NORMAL_LAYOUT }, /* radio NORMAL_LAYOUT */
	{"View/Layout/ThreeColumns",             NULL, N_("_Three columns"), NULL, NULL, VERTICAL_LAYOUT }, /* radio VERTICAL_LAYOUT */
	{"View/Layout/WideMessage",              NULL, N_("_Wide message"), NULL, NULL, WIDE_LAYOUT }, /* radio WIDE_LAYOUT */
	{"View/Layout/WideMessageList",          NULL, N_("W_ide message list"), NULL, NULL, WIDE_MSGLIST_LAYOUT }, /* radio WIDE_MSGLIST_LAYOUT */
	{"View/Layout/SmallScreen",              NULL, N_("S_mall screen"), NULL, NULL, SMALL_LAYOUT }, /* radio SMALL_LAYOUT */
};
#endif
static GtkRadioActionEntry mainwin_sort_radio_entries[] = { /* sort_summary_cb */
	{"View/Sort/Number",                     NULL, N_("By _number"), NULL, NULL, SORT_BY_NUMBER }, /* radio SORT_BY_NUMBER */
	{"View/Sort/Size",                       NULL, N_("By s_ize"), NULL, NULL, SORT_BY_SIZE }, /* radio SORT_BY_SIZE */
	{"View/Sort/Date",                       NULL, N_("By _date"), NULL, NULL, SORT_BY_DATE }, /* radio SORT_BY_DATE */
	{"View/Sort/ThreadDate",                 NULL, N_("By thread date"), NULL, NULL, SORT_BY_THREAD_DATE }, /* radio SORT_BY_THREAD_DATE */
	{"View/Sort/From",                       NULL, N_("By _From"), NULL, NULL, SORT_BY_FROM }, /* radio SORT_BY_FROM */
	{"View/Sort/To",                         NULL, N_("By _To"), NULL, NULL, SORT_BY_TO }, /* radio SORT_BY_TO */
	{"View/Sort/Subject",                    NULL, N_("By s_ubject"), NULL, NULL, SORT_BY_SUBJECT }, /* radio SORT_BY_SUBJECT */
	{"View/Sort/Color",                      NULL, N_("By _color label"), NULL, NULL, SORT_BY_LABEL }, /* radio SORT_BY_LABEL */
	{"View/Sort/Tag",                        NULL, N_("By tag"), NULL, NULL, SORT_BY_TAGS }, /* radio SORT_BY_TAGS */
	{"View/Sort/Mark",                       NULL, N_("By _mark"), NULL, NULL, SORT_BY_MARK }, /* radio SORT_BY_MARK */
	{"View/Sort/Status",                     NULL, N_("By _status"), NULL, NULL, SORT_BY_STATUS }, /* radio SORT_BY_STATUS */
	{"View/Sort/Attachment",                 NULL, N_("By a_ttachment"), NULL, NULL, SORT_BY_MIME }, /* radio SORT_BY_MIME */
	{"View/Sort/Score",                      NULL, N_("By score"), NULL, NULL, SORT_BY_SCORE }, /* radio SORT_BY_SCORE */
	{"View/Sort/Locked",                     NULL, N_("By locked"), NULL, NULL, SORT_BY_LOCKED }, /* radio SORT_BY_LOCKED */
	{"View/Sort/DontSort",                   NULL, N_("D_on't sort"), NULL, NULL, SORT_BY_NONE }, /* radio SORT_BY_NONE */
};

static GtkRadioActionEntry mainwin_sorttype_radio_entries[] = { /* sort_summary_type_cb */
	{"View/Sort/Ascending",                  NULL, N_("Ascending"), NULL, NULL, SORT_ASCENDING }, /* radio SORT_ASCENDING */
	{"View/Sort/Descending",                 NULL, N_("Descending"), NULL, NULL, SORT_DESCENDING }, /* radio SORT_DESCENDING */
};

static GtkRadioActionEntry mainwin_radio_enc_entries[] =
{
	ENC_ACTION(CS_AUTO, C_AUTO, N_("_Automatic")), /* RADIO set_charset_cb */
	ENC_ACTION(CS_US_ASCII, C_US_ASCII, N_("7bit ASCII (US-ASC_II)")), /* RADIO set_charset_cb */
	ENC_ACTION(CS_UTF_8, C_UTF_8, N_("Unicode (_UTF-8)")), /* RADIO set_charset_cb */
	ENC_ACTION("Western/"CS_ISO_8859_1, C_ISO_8859_1, "ISO-8859-_1"), /* RADIO set_charset_cb */
	ENC_ACTION("Western/"CS_ISO_8859_15, C_ISO_8859_15, "ISO-8859-15"), /* RADIO set_charset_cb */
	ENC_ACTION("Western/"CS_WINDOWS_1252, C_WINDOWS_1252, "Windows-1252"), /* RADIO set_charset_cb */
	ENC_ACTION(CS_ISO_8859_2, C_ISO_8859_2, N_("Central European (ISO-8859-_2)")), /* RADIO set_charset_cb */
	ENC_ACTION("Baltic/"CS_ISO_8859_13, C_ISO_8859_13, "ISO-8859-13"), /* RADIO set_charset_cb */
	ENC_ACTION("Baltic/"CS_ISO_8859_4, C_ISO_8859_14, "ISO-8859-_4"), /* RADIO set_charset_cb */
	ENC_ACTION(CS_ISO_8859_7, C_ISO_8859_7, N_("Greek (ISO-8859-_7)")), /* RADIO set_charset_cb */
	ENC_ACTION("Hebrew/"CS_ISO_8859_8, C_ISO_8859_8, "ISO-8859-_8"), /* RADIO set_charset_cb */
	ENC_ACTION("Hebrew/"CS_WINDOWS_1255, C_WINDOWS_1255, "Windows-1255"), /* RADIO set_charset_cb */
	ENC_ACTION("Arabic/"CS_ISO_8859_6, C_ISO_8859_6, "ISO-8859-_6"), /* RADIO set_charset_cb */
	ENC_ACTION("Arabic/"CS_WINDOWS_1256, C_WINDOWS_1256, "Windows-1256"), /* RADIO set_charset_cb */
	ENC_ACTION(CS_ISO_8859_9, C_ISO_8859_9, N_("Turkish (ISO-8859-_9)")), /* RADIO set_charset_cb */
	ENC_ACTION("Cyrillic/"CS_ISO_8859_5, C_ISO_8859_5, "ISO-8859-_5"), /* RADIO set_charset_cb */
	ENC_ACTION("Cyrillic/"CS_KOI8_R, C_KOI8_R, "KOI8-_R"), /* RADIO set_charset_cb */
	ENC_ACTION("Cyrillic/"CS_MACCYR, C_MACCYR, "_Mac-Cyrillic"), /* RADIO set_charset_cb */
	ENC_ACTION("Cyrillic/"CS_KOI8_U, C_KOI8_U, "KOI8-_U"), /* RADIO set_charset_cb */
	ENC_ACTION("Cyrillic/"CS_WINDOWS_1251, C_WINDOWS_1251, "Windows-1251"), /* RADIO set_charset_cb */
	ENC_ACTION("Japanese/"CS_ISO_2022_JP, C_ISO_2022_JP, "ISO-2022-_JP"), /* RADIO set_charset_cb */
	ENC_ACTION("Japanese/"CS_ISO_2022_JP_2, C_ISO_2022_JP_2, "ISO-2022-JP-_2"), /* RADIO set_charset_cb */
	ENC_ACTION("Japanese/"CS_EUC_JP, C_EUC_JP, "_EUC-JP"), /* RADIO set_charset_cb */
	ENC_ACTION("Japanese/"CS_SHIFT_JIS, C_SHIFT_JIS, "_Shift-JIS"), /* RADIO set_charset_cb */
	ENC_ACTION("Chinese/"CS_GB18030, C_GB18030, "_GB18030"), /* RADIO set_charset_cb */
	ENC_ACTION("Chinese/"CS_GB2312, C_GB2312, "_GB2312"), /* RADIO set_charset_cb */
	ENC_ACTION("Chinese/"CS_GBK, C_GBK, "GB_K"), /* RADIO set_charset_cb */
	ENC_ACTION("Chinese/"CS_BIG5, C_BIG5, "_Big5-JP"), /* RADIO set_charset_cb */
	ENC_ACTION("Chinese/"CS_EUC_TW, C_EUC_TW, "EUC-_TW"), /* RADIO set_charset_cb */
	ENC_ACTION("Korean/"CS_EUC_KR, C_EUC_KR, "_EUC-KR"), /* RADIO set_charset_cb */
	ENC_ACTION("Korean/"CS_ISO_2022_KR, C_ISO_2022_KR, "_ISO-2022-KR"), /* RADIO set_charset_cb */
	ENC_ACTION("Thai/"CS_TIS_620, C_TIS_620, "_TIS-620-KR"), /* RADIO set_charset_cb */
	ENC_ACTION("Thai/"CS_WINDOWS_874, C_WINDOWS_874, "_Windows-874"), /* RADIO set_charset_cb */
};

static GtkRadioActionEntry mainwin_radio_dec_entries[] =
{
	DEC_ACTION("AutoDetect", 0, N_("_Auto detect")),	/* set_decode_cb */
	/* --- */
	DEC_ACTION("8bit", ENC_8BIT, "_8bit"),
	DEC_ACTION("QP", ENC_QUOTED_PRINTABLE, "_Quoted printable"),
	DEC_ACTION("B64", ENC_BASE64, "_Base64"),
	DEC_ACTION("Uuencode", ENC_X_UUENCODE, "_Uuencode"),
};

static gboolean offline_ask_sync = TRUE;
static gboolean is_obscured = FALSE;

#define N_COLOR_LABELS colorlabel_get_color_count()

static void mainwindow_colorlabel_menu_item_activate_item_cb(GtkMenuItem *menu_item,
							  gpointer data)
{
	MainWindow *mainwin;
	GtkMenuShell *menu;
	GtkCheckMenuItem **items;
	gint n;
	GList *children, *cur;
	GSList *sel;

	mainwin = (MainWindow *)data;
	cm_return_if_fail(mainwin != NULL);

	sel = summary_get_selection(mainwin->summaryview);
	if (!sel) return;

	menu = GTK_MENU_SHELL(mainwin->colorlabel_menu);
	cm_return_if_fail(menu != NULL);

	Xalloca(items, (N_COLOR_LABELS + 1) * sizeof(GtkWidget *), return);

	/* NOTE: don't return prematurely because we set the "dont_toggle"
	 * state for check menu items. This would be bad! */
	g_object_set_data(G_OBJECT(menu), "dont_toggle",
			  GINT_TO_POINTER(1));

	/* clear items. get item pointers. */
	children = gtk_container_get_children(GTK_CONTAINER(menu));
	for (n = 0, cur = children; cur != NULL && cur->data != NULL; cur = cur->next) {
		if (GTK_IS_CHECK_MENU_ITEM(cur->data)) {
			gtk_check_menu_item_set_active
				(GTK_CHECK_MENU_ITEM(cur->data), FALSE);
			items[n] = GTK_CHECK_MENU_ITEM(cur->data);
			n++;
		}
	}

	g_list_free(children);

	if (n == (N_COLOR_LABELS + 1)) {
		/* iterate all messages and set the state of the appropriate
		 * items */
		for (; sel != NULL; sel = sel->next) {
			MsgInfo *msginfo;
			gint clabel;

			msginfo = (MsgInfo *)sel->data;
			if (msginfo) {
				clabel = MSG_GET_COLORLABEL_VALUE(msginfo->flags);
				if (!gtk_check_menu_item_get_active(items[clabel]))
					gtk_check_menu_item_set_active
						(items[clabel], TRUE);
			}
		}
	} else
		g_warning("invalid number of color elements (%d)", n);

	g_slist_free(sel);
	/* reset "dont_toggle" state */
	g_object_set_data(G_OBJECT(menu), "dont_toggle",
			  GINT_TO_POINTER(0));
}

static void mainwindow_colorlabel_menu_item_activate_cb(GtkWidget *widget,
						     gpointer data)
{
	guint color = GPOINTER_TO_UINT(data);
	MainWindow *mainwin;

	mainwin = g_object_get_data(G_OBJECT(widget), "mainwin");
	cm_return_if_fail(mainwin != NULL);

	/* "dont_toggle" state set? */
	if (g_object_get_data(G_OBJECT(mainwin->colorlabel_menu),
				"dont_toggle"))
		return;

	summary_set_colorlabel(mainwin->summaryview, color, NULL);
}

static void mainwindow_tags_menu_item_activate_item_cb(GtkMenuItem *menu_item,
							  gpointer data)
{
	MainWindow *mainwin;
	GtkMenuShell *menu;
	GList *children, *cur;
	GSList *sel;
	GHashTable *menu_table = g_hash_table_new_full(
					g_direct_hash,
					g_direct_equal,
					NULL, NULL);
	GHashTable *menu_allsel_table = g_hash_table_new_full(
					g_direct_hash,
					g_direct_equal,
					NULL, NULL);
	gint sel_len;
	mainwin = (MainWindow *)data;
	cm_return_if_fail(mainwin != NULL);

	sel = summary_get_selection(mainwin->summaryview);
	if (!sel) return;

	menu = GTK_MENU_SHELL(mainwin->tags_menu);
	cm_return_if_fail(menu != NULL);

	/* NOTE: don't return prematurely because we set the "dont_toggle"
	 * state for check menu items */
	g_object_set_data(G_OBJECT(menu), "dont_toggle",
			  GINT_TO_POINTER(1));

	/* clear items. get item pointers. */
	children = gtk_container_get_children(GTK_CONTAINER(menu));
	for (cur = children; cur != NULL && cur->data != NULL; cur = cur->next) {
		if (GTK_IS_CHECK_MENU_ITEM(cur->data)) {
			gint id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(cur->data),
				"tag_id"));
			gtk_check_menu_item_set_active
				(GTK_CHECK_MENU_ITEM(cur->data), FALSE);
				
			g_hash_table_insert(menu_table, GINT_TO_POINTER(id), GTK_CHECK_MENU_ITEM(cur->data));
			g_hash_table_insert(menu_allsel_table, GINT_TO_POINTER(id), GINT_TO_POINTER(0));
		}
	}

	g_list_free(children);

	/* iterate all messages and set the state of the appropriate
	 * items */
	sel_len = 0;
	for (; sel != NULL; sel = sel->next) {
		MsgInfo *msginfo;
		GSList *tags = NULL;
		GtkCheckMenuItem *item;
		msginfo = (MsgInfo *)sel->data;
		sel_len++;
		if (msginfo) {
			tags =  msginfo->tags;
			if (!tags)
				continue;

			for (; tags; tags = tags->next) {
				gint num_checked = GPOINTER_TO_INT(g_hash_table_lookup(menu_allsel_table, tags->data));
				item = g_hash_table_lookup(menu_table, GINT_TO_POINTER(tags->data));
				if (item && !gtk_check_menu_item_get_active(item)) {
					gtk_check_menu_item_set_active
						(item, TRUE);
				}
				num_checked++;
				g_hash_table_replace(menu_allsel_table, tags->data, GINT_TO_POINTER(num_checked));
			}
		}
	}

	children = gtk_container_get_children(GTK_CONTAINER(menu));
	for (cur = children; cur != NULL && cur->data != NULL; cur = cur->next) {
		if (GTK_IS_CHECK_MENU_ITEM(cur->data)) {
			gint id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(cur->data),
				"tag_id"));
			gint num_checked = GPOINTER_TO_INT(g_hash_table_lookup(menu_allsel_table, GINT_TO_POINTER(id)));
			if (num_checked < sel_len && num_checked > 0)
				gtk_check_menu_item_set_inconsistent(GTK_CHECK_MENU_ITEM(cur->data), TRUE);
			else
				gtk_check_menu_item_set_inconsistent(GTK_CHECK_MENU_ITEM(cur->data), FALSE);
		}
	}
	g_list_free(children);
	g_slist_free(sel);
	g_hash_table_destroy(menu_table);
	g_hash_table_destroy(menu_allsel_table);
	/* reset "dont_toggle" state */
	g_object_set_data(G_OBJECT(menu), "dont_toggle",
			  GINT_TO_POINTER(0));
}

static void mainwindow_tags_menu_item_activate_cb(GtkWidget *widget,
						     gpointer data)
{
	gint id = GPOINTER_TO_INT(data);
	gboolean set = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget));
	MainWindow *mainwin;

	mainwin = g_object_get_data(G_OBJECT(widget), "mainwin");
	cm_return_if_fail(mainwin != NULL);

	/* "dont_toggle" state set? */
	if (g_object_get_data(G_OBJECT(mainwin->tags_menu),
				"dont_toggle"))
		return;

	if (!set)
		id = -id;
	summary_set_tag(mainwin->summaryview, id, NULL);
}

void mainwin_accel_changed_cb (GtkAccelGroup *accelgroup, guint keyval, GdkModifierType modifier,
				  GClosure *closure, GtkMenuItem *item)
{
	GList *closures = gtk_widget_list_accel_closures(GTK_WIDGET(item));
	GList *cur;
	for (cur = closures; cur; cur = cur->next) {
		if (closure == cur->data) {
			GtkLabel *label = g_object_get_data(G_OBJECT(item), "accel_label");
			gchar *new_accel;
			
			if (keyval == GDK_KEY_Delete) {
				const gchar *accel_path;
				accel_path = gtk_menu_item_get_accel_path(item);
				keyval = 0; modifier = 0;
				gtk_accel_map_change_entry (accel_path, keyval, modifier, TRUE);
			}
			new_accel = gtk_accelerator_get_label(keyval, modifier);
			gtk_label_set_text(label, new_accel);
			g_free(new_accel);
		}
	}
	g_list_free(closures);
}

static void mainwindow_colorlabel_menu_create(MainWindow *mainwin, gboolean refresh)
{
	GtkWidget *label_menuitem;
	GtkWidget *menu;
	GtkWidget *item;
	gint i;
	gchar *accel_path = NULL;

	label_menuitem = gtk_ui_manager_get_widget(mainwin->ui_manager, "/Menu/Message/ColorLabel");
	g_signal_connect(G_OBJECT(label_menuitem), "activate",
			 G_CALLBACK(mainwindow_colorlabel_menu_item_activate_item_cb),
			   mainwin);
	gtk_widget_show(label_menuitem);

	menu = gtk_menu_new();
	gtk_menu_set_accel_group (GTK_MENU (menu), 
		gtk_ui_manager_get_accel_group(mainwin->ui_manager));

	/* create sub items. for the menu item activation callback we pass the
	 * index of label_colors[] as data parameter. for the None color we
	 * pass an invalid (high) value. also we attach a data pointer so we
	 * can always get back the Mainwindow pointer. */

	item = gtk_check_menu_item_new_with_label(_("None"));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	g_signal_connect(G_OBJECT(item), "activate",
			 G_CALLBACK(mainwindow_colorlabel_menu_item_activate_cb),
			   GUINT_TO_POINTER(0));
	g_object_set_data(G_OBJECT(item), "mainwin", mainwin);
	gtk_widget_show(item);

	accel_path = g_strdup_printf("<ClawsColorLabels>/None");
	gtk_menu_item_set_accel_path(GTK_MENU_ITEM(item), accel_path);
	g_free(accel_path);
	gtk_accel_map_add_entry("<ClawsColorLabels>/None", GDK_KEY_0, GDK_CONTROL_MASK);

	/* create pixmap/label menu items */
	for (i = 0; i < N_COLOR_LABELS; i++) {
		item = colorlabel_create_check_color_menu_item(
			i, refresh, MAINWIN_COLORMENU);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		g_signal_connect(G_OBJECT(item), "activate",
				 G_CALLBACK(mainwindow_colorlabel_menu_item_activate_cb),
				 GUINT_TO_POINTER(i + 1));
		g_object_set_data(G_OBJECT(item), "mainwin",
				  mainwin);
		gtk_widget_show(item);
		accel_path = g_strdup_printf("<ClawsColorLabels>/%d", i+1);
		gtk_menu_item_set_accel_path(GTK_MENU_ITEM(item), accel_path);
		if (i < 9)
			gtk_accel_map_add_entry(accel_path, GDK_KEY_1+i, GDK_CONTROL_MASK);
		g_free(accel_path);
		g_signal_connect (gtk_ui_manager_get_accel_group(mainwin->ui_manager), 
			"accel-changed", G_CALLBACK (mainwin_accel_changed_cb), item);


	}
	gtk_widget_show(menu);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(label_menuitem), menu);
	mainwin->colorlabel_menu = menu;
}

static void mainwindow_tags_menu_item_apply_tags_activate_cb(GtkWidget *widget,
						     gpointer data)
{
	MainWindow *mainwin;

	mainwin = g_object_get_data(G_OBJECT(widget), "mainwin");
	cm_return_if_fail(mainwin != NULL);

	/* "dont_toggle" state set? */
	if (g_object_get_data(G_OBJECT(mainwin->tags_menu),
				"dont_toggle"))
		return;
	
	tags_window_open(summary_get_selection(mainwin->summaryview));
}

static gint mainwin_tag_cmp_list(gconstpointer a, gconstpointer b)
{
	gint id_a = GPOINTER_TO_INT(a);
	gint id_b = GPOINTER_TO_INT(b);
	const gchar *tag_a = tags_get_tag(id_a);
	const gchar *tag_b = tags_get_tag(id_b);
	
 		
 	if (tag_a == NULL)
 		return tag_b == NULL ? 0:1;
 	
 	if (tag_b == NULL)
 		return 1;
 
 	return g_utf8_collate(tag_a, tag_b);
}

static void mainwindow_tags_menu_create(MainWindow *mainwin, gboolean refresh)
{
	GtkWidget *label_menuitem;
	GtkWidget *menu;
	GtkWidget *item;
	GSList *cur = tags_get_list();
	GSList *orig = NULL;
	gboolean existing_tags = FALSE;
	gchar *accel_path;
	cur = orig = g_slist_sort(cur, mainwin_tag_cmp_list);

	label_menuitem = gtk_ui_manager_get_widget(mainwin->ui_manager, "/Menu/Message/Tags");
	g_signal_connect(G_OBJECT(label_menuitem), "activate",
			 G_CALLBACK(mainwindow_tags_menu_item_activate_item_cb),
			   mainwin);

	gtk_widget_show(label_menuitem);

	menu = gtk_menu_new();
	gtk_menu_set_accel_group (GTK_MENU (menu), 
		gtk_ui_manager_get_accel_group(mainwin->ui_manager));

	/* create tags menu items */
	for (; cur; cur = cur->next) {
		gint id = GPOINTER_TO_INT(cur->data);
		const gchar *tag = tags_get_tag(id);

		item = gtk_check_menu_item_new_with_label(tag);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		g_signal_connect(G_OBJECT(item), "activate",
				 G_CALLBACK(mainwindow_tags_menu_item_activate_cb),
				 GINT_TO_POINTER(id));
		g_object_set_data(G_OBJECT(item), "mainwin",
				  mainwin);
		g_object_set_data(G_OBJECT(item), "tag_id",
				  GINT_TO_POINTER(id));
		gtk_widget_show(item);
		accel_path = g_strconcat("<ClawsTags>/",tag, NULL);
		gtk_menu_item_set_accel_path(GTK_MENU_ITEM(item), accel_path);
		g_free(accel_path);
		existing_tags = TRUE;
	}
	if (existing_tags) {
		/* separator */
		item = gtk_separator_menu_item_new();
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		gtk_widget_show(item);
	}

	item = gtk_menu_item_new_with_label(_("Modify tags..."));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	g_signal_connect(G_OBJECT(item), "activate",
			 G_CALLBACK(mainwindow_tags_menu_item_apply_tags_activate_cb),
			 NULL);
	g_object_set_data(G_OBJECT(item), "mainwin",
			  mainwin);
	gtk_widget_show(item);
	accel_path = g_strdup_printf("<ClawsTags>/ModifyTags");
	gtk_menu_item_set_accel_path(GTK_MENU_ITEM(item), accel_path);
	g_free(accel_path);
	gtk_accel_map_add_entry("<ClawsTags>/ModifyTags", GDK_KEY_T, GDK_CONTROL_MASK|GDK_SHIFT_MASK);
	g_slist_free(orig);
	gtk_widget_show(menu);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(label_menuitem), menu);
	mainwin->tags_menu = menu;
}
#ifndef GENERIC_UMPC
static void warning_btn_pressed(GtkButton *btn, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	log_window_show_error(mainwin->logwin);
	gtk_widget_hide(mainwin->warning_btn);
}

static gboolean warning_visi_notify(GtkWidget *widget,
				       GdkEventVisibility *event,
				       MainWindow *mainwindow)
{
	gdk_window_set_cursor(gtk_widget_get_window(mainwindow->warning_btn), hand_cursor);
	return FALSE;
}

static gboolean warning_leave_notify(GtkWidget *widget,
				      GdkEventCrossing *event,
				      MainWindow *mainwindow)
{
	gdk_window_set_cursor(gtk_widget_get_window(mainwindow->warning_btn), NULL);
	return FALSE;
}

static gboolean warning_enter_notify(GtkWidget *widget,
				      GdkEventCrossing *event,
				      MainWindow *mainwindow)
{
	gdk_window_set_cursor(gtk_widget_get_window(mainwindow->warning_btn), hand_cursor);
	return FALSE;
}
#endif
void mainwindow_show_error(void)
{
	MainWindow *mainwin = mainwindow_get_mainwindow();
	gtk_widget_show(mainwin->warning_btn);
}

void mainwindow_clear_error(MainWindow *mainwin)
{
	gtk_widget_hide(mainwin->warning_btn);
}

#define BREAK_ON_MODIFIER_KEY() \
	if ((event->state & (GDK_MOD1_MASK|GDK_CONTROL_MASK)) != 0) break

static gboolean mainwindow_key_pressed (GtkWidget *widget, GdkEventKey *event,
				    gpointer data)
{
	MainWindow *mainwin = (MainWindow*) data;

	if (!mainwin || !event) 
		return FALSE;

	if (quicksearch_has_focus(mainwin->summaryview->quicksearch))
	{
		GtkWidget *entry =
			quicksearch_get_entry(mainwin->summaryview->quicksearch);
		gboolean handled;
		g_signal_emit_by_name(entry, "key-press-event", event, &handled);
		if (handled) {
			return TRUE;
		}
	}

	switch (event->keyval) {
	case GDK_KEY_Q:             /* Quit */
		BREAK_ON_MODIFIER_KEY();

		if (gtk_window_is_active(GTK_WINDOW(mainwin->window))) {
			app_exit_cb(NULL, mainwin);
		}
		return FALSE;
	case GDK_KEY_space:
		BREAK_ON_MODIFIER_KEY();
		if (gtk_window_is_active(GTK_WINDOW(mainwin->window))) {
			if (mainwin->folderview != NULL && mainwin->summaryview != NULL
			    && ((!mainwin->summaryview->displayed
		        	&& !mainwin->summaryview->selected) 
				|| (mainwin->summaryview->folder_item
				    && mainwin->summaryview->folder_item->total_msgs == 0))) {
				g_signal_stop_emission_by_name(G_OBJECT(widget), 
                                	       "key_press_event");
				folderview_select_next_with_flag(mainwin->folderview, MSG_UNREAD);
			}
		}
		break;
	default:
		break;
	}
	return FALSE;
}

#undef BREAK_ON_MODIFIER_KEY

MainWindow *main_window_create()
{
	MainWindow *mainwin;
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *menubar;
	GtkWidget *handlebox;
	GtkWidget *vbox_body;
	GtkWidget *menuitem;
#ifndef GENERIC_UMPC
	GtkWidget *hbox_stat;
	GtkWidget *statusbar;
	GtkWidget *progressbar;
	GtkWidget *statuslabel;
	GtkWidget *ac_button;
	GtkWidget *ac_label;
 	GtkWidget *online_pixmap;
	GtkWidget *offline_pixmap;
	GtkWidget *warning_btn;
#endif
	GtkWidget *online_switch;
	GtkWidget *offline_switch;
	FolderView *folderview;
	SummaryView *summaryview;
	MessageView *messageview;
	GtkWidget *ac_menu;

	static GdkGeometry geometry;

	debug_print("Creating main window...\n");
	mainwin = g_new0(MainWindow, 1);

	/* main window */
	window = GTK_WIDGET(gtkut_window_new(GTK_WINDOW_TOPLEVEL, "mainwindow"));
	gtk_window_set_title(GTK_WINDOW(window), PROG_VERSION);
	gtk_window_set_resizable(GTK_WINDOW(window), TRUE);
#ifdef GENERIC_UMPC
	prefs_common.layout_mode = SMALL_LAYOUT;
#endif
	if (!geometry.min_height) {
		geometry.min_width = 320;
		geometry.min_height = 200;
	}
	gtk_window_set_geometry_hints(GTK_WINDOW(window), NULL, &geometry,
				      GDK_HINT_MIN_SIZE);

	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(main_window_close_cb), mainwin);
	MANAGE_WINDOW_SIGNALS_CONNECT(window);
	g_signal_connect(G_OBJECT(window), "focus_in_event",
			 G_CALLBACK(mainwindow_focus_in_event),
			 mainwin);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(mainwindow_key_pressed), mainwin);

	gtk_widget_realize(window);
	gtk_widget_add_events(window, GDK_KEY_PRESS_MASK|GDK_KEY_RELEASE_MASK);
	

	gtkut_widget_set_app_icon(window);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show(vbox);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	/* menu bar */

	mainwin->ui_manager = gtk_ui_manager_new();
	mainwin->action_group = cm_menu_create_action_group_full(mainwin->ui_manager,"Menu", mainwin_entries,
			G_N_ELEMENTS(mainwin_entries), (gpointer)mainwin);
	gtk_action_group_add_toggle_actions(mainwin->action_group, mainwin_toggle_entries,
			G_N_ELEMENTS(mainwin_toggle_entries), (gpointer)mainwin);
	gtk_action_group_add_radio_actions(mainwin->action_group, mainwin_showhide_radio_entries,
			G_N_ELEMENTS(mainwin_showhide_radio_entries), C_AUTO, G_CALLBACK(toggle_toolbar_cb), (gpointer)mainwin);
#ifndef GENERIC_UMPC
	gtk_action_group_add_radio_actions(mainwin->action_group, mainwin_layout_radio_entries,
			G_N_ELEMENTS(mainwin_layout_radio_entries), C_AUTO, G_CALLBACK(set_layout_cb), (gpointer)mainwin);
#endif
	gtk_action_group_add_radio_actions(mainwin->action_group, mainwin_sort_radio_entries,
			G_N_ELEMENTS(mainwin_sort_radio_entries), C_AUTO, G_CALLBACK(sort_summary_cb), (gpointer)mainwin);
	gtk_action_group_add_radio_actions(mainwin->action_group, mainwin_sorttype_radio_entries,
			G_N_ELEMENTS(mainwin_sorttype_radio_entries), C_AUTO, G_CALLBACK(sort_summary_type_cb), (gpointer)mainwin);
	gtk_action_group_add_radio_actions(mainwin->action_group, mainwin_radio_enc_entries,
			G_N_ELEMENTS(mainwin_radio_enc_entries), C_AUTO, G_CALLBACK(set_charset_cb), (gpointer)mainwin);
	gtk_action_group_add_radio_actions(mainwin->action_group, mainwin_radio_dec_entries,
			G_N_ELEMENTS(mainwin_radio_dec_entries), C_AUTO, G_CALLBACK(set_decode_cb), (gpointer)mainwin);

	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/", "Menu", NULL, GTK_UI_MANAGER_MENUBAR)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu", "File", "File", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu", "Edit", "Edit", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu", "View", "View", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu", "Message", "Message", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu", "Tools", "Tools", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu", "Configuration", "Configuration", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu", "Help", "Help", GTK_UI_MANAGER_MENU)

/* File menu */
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/File", "AddMailbox", "File/AddMailbox", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/File/AddMailbox", "MH", "File/AddMailbox/MH", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/File", "Separator1", "File/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/File", "SortMailboxes", "File/SortMailboxes", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/File", "Separator2", "File/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/File", "ImportMbox", "File/ImportMbox", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/File", "ExportMbox", "File/ExportMbox", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/File", "ExportSelMbox", "File/ExportSelMbox", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/File", "Separator3", "File/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/File", "EmptyTrashes", "File/EmptyTrashes", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/File", "Separator4", "File/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/File", "SaveAs", "File/SaveAs", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/File", "SavePartAs", "File/SavePartAs", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/File", "Separator5", "File/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/File", "PageSetup", "File/PageSetup", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/File", "Print", "File/Print", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/File", "Separator6", "File/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/File", "OfflineMode", "File/OfflineMode", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/File", "SynchroniseFolders", "File/SynchroniseFolders", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/File", "Separator7", "File/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/File", "Exit", "File/Exit", GTK_UI_MANAGER_MENUITEM)

/* Edit menu */
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Edit", "Copy", "Edit/Copy", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Edit", "SelectAll", "Edit/SelectAll", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Edit", "SelectThread", "Edit/SelectThread", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Edit", "Separator1", "Edit/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Edit", "Find", "Edit/Find", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Edit", "SearchFolder", "Edit/SearchFolder", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Edit", "QuickSearch", "Edit/QuickSearch", GTK_UI_MANAGER_MENUITEM)

/* View menu */
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View", "ShowHide", "View/ShowHide", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/ShowHide", "MenuBar", "View/ShowHide/MenuBar", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/ShowHide", "Toolbar", "View/ShowHide/Toolbar", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/ShowHide/Toolbar", "TextBelowIcon", "View/ShowHide/Toolbar/TextBelowIcon", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/ShowHide/Toolbar", "TextBesideIcon", "View/ShowHide/Toolbar/TextBesideIcon", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/ShowHide/Toolbar", "IconOnly", "View/ShowHide/Toolbar/IconOnly", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/ShowHide/Toolbar", "TextOnly", "View/ShowHide/Toolbar/TextOnly", GTK_UI_MANAGER_MENUITEM)
#ifndef GENERIC_UMPC
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/ShowHide/Toolbar", "Hide", "View/ShowHide/Toolbar/Hide", GTK_UI_MANAGER_MENUITEM)
#endif
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/ShowHide", "MessageView", "View/ShowHide/MessageView", GTK_UI_MANAGER_MENUITEM)
#ifndef GENERIC_UMPC
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/ShowHide", "StatusBar", "View/ShowHide/StatusBar", GTK_UI_MANAGER_MENUITEM)
#endif
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/ShowHide", "ColumnHeaders", "View/ShowHide/ColumnHeaders", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View", "SetColumns", "View/SetColumns", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/SetColumns", "Folderlist", "View/SetColumns/Folderlist", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/SetColumns", "Messagelist", "View/SetColumns/Messagelist", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View", "Separator1", "View/---", GTK_UI_MANAGER_SEPARATOR)

	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View", "FullScreen", "View/FullScreen", GTK_UI_MANAGER_MENUITEM)
#ifndef GENERIC_UMPC
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View", "Layout", "View/Layout", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Layout", "Standard", "View/Layout/Standard", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Layout", "ThreeColumns", "View/Layout/ThreeColumns", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Layout", "WideMessage", "View/Layout/WideMessage", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Layout", "WideMessageList", "View/Layout/WideMessageList", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Layout", "SmallScreen", "View/Layout/SmallScreen", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View", "Separator2", "View/---", GTK_UI_MANAGER_SEPARATOR)
#endif

	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View", "Sort", "View/Sort", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Sort", "Number", "View/Sort/Number", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Sort", "Size", "View/Sort/Size", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Sort", "Date", "View/Sort/Date", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Sort", "ThreadDate", "View/Sort/ThreadDate", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Sort", "From", "View/Sort/From", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Sort", "To", "View/Sort/To", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Sort", "Subject", "View/Sort/Subject", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Sort", "Color", "View/Sort/Color", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Sort", "Tag", "View/Sort/Tag", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Sort", "Mark", "View/Sort/Mark", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Sort", "Status", "View/Sort/Status", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Sort", "Attachment", "View/Sort/Attachment", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Sort", "Score", "View/Sort/Score", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Sort", "Locked", "View/Sort/Locked", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Sort", "DontSort", "View/Sort/DontSort", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Sort", "Separator1", "View/Sort/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Sort", "Ascending", "View/Sort/Ascending", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Sort", "Descending", "View/Sort/Descending", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Sort", "Separator2", "View/Sort/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Sort", "AttractSubj", "View/Sort/AttractSubj", GTK_UI_MANAGER_MENUITEM)

	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View", "ThreadView", "View/ThreadView", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View", "ExpandThreads", "View/ExpandThreads", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View", "CollapseThreads", "View/CollapseThreads", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View", "HideReadThreads", "View/HideReadThreads", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View", "HideReadMessages", "View/HideReadMessages", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View", "HideDelMessages", "View/HideDelMessages", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View", "Separator3", "View/---", GTK_UI_MANAGER_SEPARATOR)

	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View", "Goto", "View/Goto", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Goto", "Prev", "View/Goto/Prev", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Goto", "Next", "View/Goto/Next", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Goto", "Separator1", "View/Goto/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Goto", "PrevUnread", "View/Goto/PrevUnread", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Goto", "NextUnread", "View/Goto/NextUnread", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Goto", "Separator2", "View/Goto/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Goto", "PrevNew", "View/Goto/PrevNew", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Goto", "NextNew", "View/Goto/NextNew", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Goto", "Separator3", "View/Goto/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Goto", "PrevMarked", "View/Goto/PrevMarked", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Goto", "NextMarked", "View/Goto/NextMarked", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Goto", "Separator4", "View/Goto/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Goto", "PrevLabeled", "View/Goto/PrevLabeled", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Goto", "NextLabeled", "View/Goto/NextLabeled", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Goto", "Separator5", "View/Goto/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Goto", "PrevHistory", "View/Goto/PrevHistory", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Goto", "NextHistory", "View/Goto/NextHistory", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Goto", "Separator6", "View/Goto/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Goto", "ParentMessage", "View/Goto/ParentMessage", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Goto", "Separator7", "View/Goto/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Goto", "NextUnreadFolder", "View/Goto/NextUnreadFolder", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Goto", "Folder", "View/Goto/Folder", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Goto", "Separator8", "View/Goto/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Goto", "NextPart", "View/Goto/NextPart", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Goto", "PrevPart", "View/Goto/PrevPart", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View", "Scroll", "View/Scroll", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Scroll", "PrevLine", "View/Scroll/PrevLine", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Scroll", "NextLine", "View/Scroll/NextLine", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Scroll", "PrevPage", "View/Scroll/PrevPage", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Scroll", "NextPage", "View/Scroll/NextPage", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View", "Separator4", "View/---", GTK_UI_MANAGER_SEPARATOR)

	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View", "Encoding", "View/Encoding", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Encoding", CS_AUTO, "View/Encoding/"CS_AUTO, GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Encoding", "Separator1", "View/Encoding/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Encoding", CS_US_ASCII, "View/Encoding/"CS_US_ASCII, GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Encoding", CS_UTF_8, "View/Encoding/"CS_UTF_8, GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Encoding", "Separator2", "View/Encoding/---", GTK_UI_MANAGER_SEPARATOR)

	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Encoding", "Western", "View/Encoding/Western", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Encoding/Western", CS_ISO_8859_1, "View/Encoding/Western/"CS_ISO_8859_1, GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Encoding/Western", CS_ISO_8859_15, "View/Encoding/Western/"CS_ISO_8859_15, GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Encoding/Western", CS_WINDOWS_1252, "View/Encoding/Western/"CS_WINDOWS_1252, GTK_UI_MANAGER_MENUITEM)

	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Encoding", CS_ISO_8859_2, "View/Encoding/"CS_ISO_8859_2, GTK_UI_MANAGER_MENUITEM)

	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Encoding", "Baltic", "View/Encoding/Baltic", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Encoding/Baltic", CS_ISO_8859_13, "View/Encoding/Baltic/"CS_ISO_8859_13, GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Encoding/Baltic", CS_ISO_8859_4, "View/Encoding/Baltic/"CS_ISO_8859_4, GTK_UI_MANAGER_MENUITEM)

	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Encoding", CS_ISO_8859_7, "View/Encoding/"CS_ISO_8859_7, GTK_UI_MANAGER_MENUITEM)

	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Encoding", "Hebrew", "View/Encoding/Hebrew", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Encoding/Hebrew", CS_ISO_8859_8, "View/Encoding/Hebrew/"CS_ISO_8859_8, GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Encoding/Hebrew", CS_WINDOWS_1255, "View/Encoding/Hebrew/"CS_WINDOWS_1255, GTK_UI_MANAGER_MENUITEM)

	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Encoding", "Arabic", "View/Encoding/Arabic", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Encoding/Arabic", CS_ISO_8859_6, "View/Encoding/Arabic/"CS_ISO_8859_6, GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Encoding/Arabic", CS_WINDOWS_1256, "View/Encoding/Arabic/"CS_WINDOWS_1256, GTK_UI_MANAGER_MENUITEM)

	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Encoding", CS_ISO_8859_9, "View/Encoding/"CS_ISO_8859_9, GTK_UI_MANAGER_MENUITEM)

	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Encoding", "Cyrillic", "View/Encoding/Cyrillic", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Encoding/Cyrillic", CS_ISO_8859_5, "View/Encoding/Cyrillic/"CS_ISO_8859_5, GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Encoding/Cyrillic", CS_KOI8_R, "View/Encoding/Cyrillic/"CS_KOI8_R, GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Encoding/Cyrillic", CS_MACCYR, "View/Encoding/Cyrillic/"CS_MACCYR, GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Encoding/Cyrillic", CS_KOI8_U, "View/Encoding/Cyrillic/"CS_KOI8_U, GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Encoding/Cyrillic", CS_WINDOWS_1251, "View/Encoding/Cyrillic/"CS_WINDOWS_1251, GTK_UI_MANAGER_MENUITEM)

	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Encoding", "Japanese", "View/Encoding/Japanese", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Encoding/Japanese", CS_ISO_2022_JP, "View/Encoding/Japanese/"CS_ISO_2022_JP, GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Encoding/Japanese", CS_ISO_2022_JP_2, "View/Encoding/Japanese/"CS_ISO_2022_JP_2, GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Encoding/Japanese", CS_EUC_JP, "View/Encoding/Japanese/"CS_EUC_JP, GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Encoding/Japanese", CS_SHIFT_JIS, "View/Encoding/Japanese/"CS_SHIFT_JIS, GTK_UI_MANAGER_MENUITEM)

	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Encoding", "Chinese", "View/Encoding/Chinese", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Encoding/Chinese", CS_GB18030, "View/Encoding/Chinese/"CS_GB18030, GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Encoding/Chinese", CS_GB2312, "View/Encoding/Chinese/"CS_GB2312, GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Encoding/Chinese", CS_GBK, "View/Encoding/Chinese/"CS_GBK, GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Encoding/Chinese", CS_BIG5, "View/Encoding/Chinese/"CS_BIG5, GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Encoding/Chinese", CS_EUC_TW, "View/Encoding/Chinese/"CS_EUC_TW, GTK_UI_MANAGER_MENUITEM)

	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Encoding", "Korean", "View/Encoding/Korean", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Encoding/Korean", CS_EUC_KR, "View/Encoding/Korean/"CS_EUC_KR, GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Encoding/Korean", CS_ISO_2022_KR, "View/Encoding/Korean/"CS_ISO_2022_KR, GTK_UI_MANAGER_MENUITEM)

	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Encoding", "Thai", "View/Encoding/Thai", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Encoding/Thai", CS_TIS_620, "View/Encoding/Thai/"CS_TIS_620, GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Encoding/Thai", CS_WINDOWS_874, "View/Encoding/Thai/"CS_WINDOWS_874, GTK_UI_MANAGER_MENUITEM)

	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View", "Decode", "View/Decode", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Decode", "AutoDetect", "View/Decode/AutoDetect", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Decode", "Separator1", "View/Decode/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Decode", "8bit", "View/Decode/8bit", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Decode", "QP", "View/Decode/QP", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Decode", "B64", "View/Decode/B64", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Decode", "Uuencode", "View/Decode/Uuencode", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View", "Separator5", "View/---", GTK_UI_MANAGER_SEPARATOR)

	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View", "OpenNewWindow", "View/OpenNewWindow", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View", "MessageSource", "View/MessageSource", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View", "AllHeaders", "View/AllHeaders", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View", "Quotes", "View/Quotes", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Quotes", "CollapseAll", "View/Quotes/CollapseAll", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Quotes", "Collapse2", "View/Quotes/Collapse2", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Quotes", "Collapse3", "View/Quotes/Collapse3", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View", "Part", "View/Part", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Part", "AsText", "View/Part/AsText", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Part", "Open", "View/Part/Open", GTK_UI_MANAGER_MENUITEM)
#ifndef G_OS_WIN32
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View/Part", "OpenWith", "View/Part/OpenWith", GTK_UI_MANAGER_MENUITEM)
#endif
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View", "Separator8", "View/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/View", "UpdateSummary", "View/UpdateSummary", GTK_UI_MANAGER_MENUITEM)

/* Message menu */
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message", "Receive", "Message/Receive", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message/Receive", "CurrentAccount", "Message/Receive/CurrentAccount", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message/Receive", "AllAccounts", "Message/Receive/AllAccounts", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message/Receive", "CancelReceiving", "Message/Receive/CancelReceiving", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message/Receive", "Separator1", "Message/Receive/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message/Receive", "PlaceHolder", "Message/Receive/PlaceHolder", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message", "SendQueue", "Message/SendQueue", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message", "CancelSending", "Message/CancelSending", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message", "Separator1", "Message/---", GTK_UI_MANAGER_SEPARATOR)

	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message", "ComposeEmail", "Message/ComposeEmail", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message", "ComposeNews", "Message/ComposeNews", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message", "Reply", "Message/Reply", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message", "ReplyTo", "Message/ReplyTo", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message/ReplyTo", "All", "Message/ReplyTo/All", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message/ReplyTo", "Sender", "Message/ReplyTo/Sender", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message/ReplyTo", "List", "Message/ReplyTo/List", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message", "FollowupReply", "Message/FollowupReply", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message", "Separator2", "Message/---", GTK_UI_MANAGER_SEPARATOR)

	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message", "Forward", "Message/Forward", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message", "ForwardAtt", "Message/ForwardAtt", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message", "Redirect", "Message/Redirect", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message", "MailingList", "Message/MailingList", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message/MailingList", "Post", "Message/MailingList/Post", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message/MailingList", "Help", "Message/MailingList/Help", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message/MailingList", "Subscribe", "Message/MailingList/Subscribe", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message/MailingList", "Unsubscribe", "Message/MailingList/Unsubscribe", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message/MailingList", "ViewArchive", "Message/MailingList/ViewArchive", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message/MailingList", "ContactOwner", "Message/MailingList/ContactOwner", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message/MailingList/Post", "PlaceHolder", "Message/MailingList/Post/PlaceHolder", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message/MailingList/Help", "PlaceHolder", "Message/MailingList/Help/PlaceHolder", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message/MailingList/Subscribe", "PlaceHolder", "Message/MailingList/Subscribe/PlaceHolder", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message/MailingList/Unsubscribe", "PlaceHolder", "Message/MailingList/Unsubscribe/PlaceHolder", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message/MailingList/ViewArchive", "PlaceHolder", "Message/MailingList/ViewArchive/PlaceHolder", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message/MailingList/ContactOwner", "PlaceHolder", "Message/MailingList/ContactOwner/PlaceHolder", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message", "Separator3", "Message/---", GTK_UI_MANAGER_SEPARATOR)

	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message", "Move", "Message/Move", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message", "Copy", "Message/Copy", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message", "Trash", "Message/Trash", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message", "Delete", "Message/Delete", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message", "TrashThread", "Message/TrashThread", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message", "DeleteThread", "Message/DeleteThread", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message", "CancelNews", "Message/CancelNews", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message", "Separator4", "Message/---", GTK_UI_MANAGER_SEPARATOR)

	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message", "Marks", "Message/Marks", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message/Marks", "Mark", "Message/Marks/Mark", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message/Marks", "Unmark", "Message/Marks/Unmark", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message/Marks", "Separator1", "Message/Marks/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message/Marks", "MarkRead", "Message/Marks/MarkRead", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message/Marks", "MarkUnread", "Message/Marks/MarkUnread", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message/Marks", "Separator2", "Message/Marks/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message/Marks", "MarkAllRead", "Message/Marks/MarkAllRead", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message/Marks", "MarkAllUnread", "Message/Marks/MarkAllUnread", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message/Marks", "Separator3", "Message/Marks/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message/Marks", "IgnoreThread", "Message/Marks/IgnoreThread", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message/Marks", "UnignoreThread", "Message/Marks/UnignoreThread", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message/Marks", "WatchThread", "Message/Marks/WatchThread", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message/Marks", "UnwatchThread", "Message/Marks/UnwatchThread", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message/Marks", "Separator4", "Message/Marks/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message/Marks", "MarkSpam", "Message/Marks/MarkSpam", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message/Marks", "MarkHam", "Message/Marks/MarkHam", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message/Marks", "Separator5", "Message/Marks/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message/Marks", "Lock", "Message/Marks/Lock", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message/Marks", "Unlock", "Message/Marks/Unlock", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message", "ColorLabel", "Message/ColorLabel", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message", "Tags", "Message/Tags", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message", "Separator5", "Message/---", GTK_UI_MANAGER_SEPARATOR)

	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message", "Reedit", "Message/Reedit", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message", "Separator6", "Message/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Message", "CheckSignature", "Message/CheckSignature", GTK_UI_MANAGER_MENUITEM)

/* Tools menu */
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Tools", "AddressBook", "Tools/AddressBook", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Tools", "AddSenderToAB", "Tools/AddSenderToAB", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Tools", "CollectAddresses", "Tools/CollectAddresses", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Tools/CollectAddresses", "FromFolder", "Tools/CollectAddresses/FromFolder", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Tools/CollectAddresses", "FromSelected", "Tools/CollectAddresses/FromSelected", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Tools", "Separator1", "Tools/---", GTK_UI_MANAGER_SEPARATOR)

	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Tools", "FilterFolder", "Tools/FilterFolder", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Tools", "FilterSelected", "Tools/FilterSelected", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Tools", "RunProcessing", "Tools/RunProcessing", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Tools", "CreateFilterRule", "Tools/CreateFilterRule", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Tools/CreateFilterRule", "Automatically", "Tools/CreateFilterRule/Automatically", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Tools/CreateFilterRule", "ByFrom", "Tools/CreateFilterRule/ByFrom", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Tools/CreateFilterRule", "ByTo", "Tools/CreateFilterRule/ByTo", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Tools/CreateFilterRule", "BySubject", "Tools/CreateFilterRule/BySubject", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Tools/CreateFilterRule", "BySender", "Tools/CreateFilterRule/BySender", GTK_UI_MANAGER_MENUITEM)

	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Tools", "CreateProcessingRule", "Tools/CreateProcessingRule", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Tools/CreateProcessingRule", "Automatically", "Tools/CreateProcessingRule/Automatically", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Tools/CreateProcessingRule", "ByFrom", "Tools/CreateProcessingRule/ByFrom", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Tools/CreateProcessingRule", "ByTo", "Tools/CreateProcessingRule/ByTo", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Tools/CreateProcessingRule", "BySubject", "Tools/CreateProcessingRule/BySubject", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Tools/CreateProcessingRule", "BySender", "Tools/CreateProcessingRule/BySender", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Tools", "Separator2", "Tools/---", GTK_UI_MANAGER_SEPARATOR)
	
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Tools", "ListUrls", "Tools/ListUrls", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Tools", "Separator3", "Tools/---", GTK_UI_MANAGER_SEPARATOR)

	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Tools", "Actions", "Tools/Actions", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Tools/Actions", "PlaceHolder", "Tools/Actions/PlaceHolder", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Tools", "Separator4", "Tools/---", GTK_UI_MANAGER_SEPARATOR)

	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Tools", "CheckNewMessages", "Tools/CheckNewMessages", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Tools", "DeleteDuplicates", "Tools/DeleteDuplicates", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Tools/DeleteDuplicates", "SelFolder", "Tools/DeleteDuplicates/SelFolder", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Tools/DeleteDuplicates", "AllFolders", "Tools/DeleteDuplicates/AllFolders", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Tools", "Separator5", "Tools/---", GTK_UI_MANAGER_SEPARATOR)

	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Tools", "Execute", "Tools/Execute", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Tools", "Expunge", "Tools/Expunge", GTK_UI_MANAGER_MENUITEM)
#ifdef USE_GNUTLS
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Tools", "Separator6", "Tools/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Tools", "TLSCertificates", "Tools/TLSCertificates", GTK_UI_MANAGER_MENUITEM)
#endif
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Tools", "Separator7", "Tools/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Tools", "FilteringLog", "Tools/FilteringLog", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Tools", "NetworkLog", "Tools/NetworkLog", GTK_UI_MANAGER_MENUITEM)
#ifdef G_OS_WIN32
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Tools", "DebugLog", "Tools/DebugLog", GTK_UI_MANAGER_MENUITEM)
#endif
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Tools", "Separator8", "Tools/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Tools", "ForgetSessionPasswords", "Tools/ForgetSessionPasswords", GTK_UI_MANAGER_MENUITEM)
#ifndef PASSWORD_CRYPTO_OLD
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Tools", "ForgetPrimaryPassphrase", "Tools/ForgetPrimaryPassphrase", GTK_UI_MANAGER_MENUITEM)
#endif
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Tools", "Separator9", "Tools/---", GTK_UI_MANAGER_SEPARATOR)

/* Configuration menu */
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Configuration", "ChangeAccount", "Configuration/ChangeAccount", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Configuration/ChangeAccount", "PlaceHolder", "Configuration/ChangeAccount/PlaceHolder", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Configuration", "AccountPrefs", "Configuration/AccountPrefs", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Configuration", "CreateAccount", "Configuration/CreateAccount", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Configuration", "EditAccounts", "Configuration/EditAccounts", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Configuration", "Separator1", "Configuration/---", GTK_UI_MANAGER_SEPARATOR)

	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Configuration", "Preferences", "Configuration/Preferences", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Configuration", "PreProcessing", "Configuration/PreProcessing", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Configuration", "PostProcessing", "Configuration/PostProcessing", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Configuration", "Filtering", "Configuration/Filtering", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Configuration", "Templates", "Configuration/Templates", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Configuration", "Actions", "Configuration/Actions", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Configuration", "Tags", "Configuration/Tags", GTK_UI_MANAGER_MENUITEM)

	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Configuration", "Separator2", "Configuration/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Configuration", "Plugins", "Configuration/Plugins", GTK_UI_MANAGER_MENUITEM)

/* Help menu */
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Help", "Manual", "Help/Manual", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Help", "FAQ", "Help/FAQ", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Help", "IconLegend", "Help/IconLegend", GTK_UI_MANAGER_MENUITEM)
#ifdef G_OS_WIN32
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Help", "Separator1", "Help/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Help", "SetDefault", "Help/SetDefault", GTK_UI_MANAGER_MENUITEM)
#endif
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Help", "Separator2", "Help/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/Menu/Help", "About", "Help/About", GTK_UI_MANAGER_MENUITEM)


	menubar = gtk_ui_manager_get_widget(mainwin->ui_manager, "/Menu");
	if (prefs_common.mainwin_menubar)
		gtk_widget_show_all(menubar);
	else
		gtk_widget_hide(menubar);
	gtk_window_add_accel_group(GTK_WINDOW(window), gtk_ui_manager_get_accel_group(mainwin->ui_manager));

	/* Create a menu container for the different popup menus we
	 * will be adding from other UI elements. */
	MENUITEM_ADDUI_MANAGER(mainwin->ui_manager, "/", "Menus", "Menus", GTK_UI_MANAGER_MENUBAR)

	gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, TRUE, 0);

	handlebox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_show(handlebox);
	gtk_box_pack_start(GTK_BOX(vbox), handlebox, FALSE, FALSE, 0);
	/* link window to mainwin->window to avoid gdk warnings */
	mainwin->window       = window;
	mainwin_list = g_list_append(mainwin_list, mainwin);
	
	mainwin->toolbar = toolbar_create(TOOLBAR_MAIN, 
					  handlebox, 
					  (gpointer)mainwin);
	toolbar_set_learn_button
		(mainwin->toolbar,
		 LEARN_SPAM);

	/* vbox that contains body */
	vbox_body = gtk_box_new(GTK_ORIENTATION_VERTICAL, BORDER_WIDTH);
	gtk_widget_show(vbox_body);
	gtk_container_set_border_width(GTK_CONTAINER(vbox_body), BORDER_WIDTH);
	gtk_box_pack_start(GTK_BOX(vbox), vbox_body, TRUE, TRUE, 0);

#ifndef GENERIC_UMPC
	hbox_stat = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_widget_set_name(GTK_WIDGET(hbox_stat), "hbox_stat");
	gtk_box_pack_end(GTK_BOX(vbox_body), hbox_stat, FALSE, FALSE, 0);
	warning_btn = gtk_button_new();
	gtk_button_set_image(GTK_BUTTON(warning_btn),
			     gtk_image_new_from_icon_name("dialog-warning", GTK_ICON_SIZE_BUTTON));
	CLAWS_SET_TIP(warning_btn,
			     _("Some error(s) happened. Click here to view log."));
	
	mainwin->warning_btn      = warning_btn;
	
	g_signal_connect(G_OBJECT(warning_btn), "clicked",
			 G_CALLBACK(warning_btn_pressed), mainwin);
	g_signal_connect(G_OBJECT(warning_btn), "motion-notify-event",
			 G_CALLBACK(warning_visi_notify), mainwin);
	g_signal_connect(G_OBJECT(warning_btn), "leave-notify-event",
			 G_CALLBACK(warning_leave_notify), mainwin);
	g_signal_connect(G_OBJECT(warning_btn), "enter-notify-event",
			 G_CALLBACK(warning_enter_notify), mainwin);

	gtk_box_pack_start(GTK_BOX(hbox_stat), warning_btn, FALSE, FALSE, 0);

	statusbar = statusbar_create();
	gtk_widget_set_margin_start(statusbar, 2);
	gtk_widget_set_margin_end(statusbar, 0);
	gtk_widget_set_margin_top(statusbar, 0);
	gtk_widget_set_margin_bottom(statusbar, 0);
	gtk_box_pack_start(GTK_BOX(hbox_stat), statusbar, TRUE, TRUE, 0);

	progressbar = gtk_progress_bar_new();
	gtk_widget_set_size_request(progressbar, 120, 1);
	gtk_box_pack_start(GTK_BOX(hbox_stat), progressbar, FALSE, FALSE, 0);

	online_pixmap = stock_pixmap_widget(STOCK_PIXMAP_ONLINE);
	offline_pixmap = stock_pixmap_widget(STOCK_PIXMAP_OFFLINE);
	online_switch = gtk_button_new ();
	gtk_widget_set_can_focus(online_switch, FALSE);
	CLAWS_SET_TIP(online_switch, 
			     _("You are online. Click the icon to go offline"));
	offline_switch = gtk_button_new ();
	CLAWS_SET_TIP(offline_switch, 
			     _("You are offline. Click the icon to go online"));
	gtk_container_add (GTK_CONTAINER(online_switch), online_pixmap);
	gtk_button_set_relief (GTK_BUTTON(online_switch), GTK_RELIEF_NONE);
	g_signal_connect (G_OBJECT(online_switch), "clicked", G_CALLBACK(online_switch_clicked), mainwin);
	gtk_box_pack_start (GTK_BOX(hbox_stat), online_switch, FALSE, FALSE, 0);
	gtk_container_add (GTK_CONTAINER(offline_switch), offline_pixmap);
	gtk_button_set_relief (GTK_BUTTON(offline_switch), GTK_RELIEF_NONE);
	g_signal_connect (G_OBJECT(offline_switch), "clicked", G_CALLBACK(online_switch_clicked), mainwin);
	gtk_box_pack_start (GTK_BOX(hbox_stat), offline_switch, FALSE, FALSE, 0);
	
	statuslabel = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(hbox_stat), statuslabel, FALSE, FALSE, 0);

	ac_button = gtk_button_new();
	CLAWS_SET_TIP(ac_button, _("Select account"));
	gtk_widget_set_can_focus(ac_button, FALSE);
	gtk_widget_set_size_request(ac_button, -1, 0);
	gtk_box_pack_end(GTK_BOX(hbox_stat), ac_button, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(ac_button), "button_press_event",
			 G_CALLBACK(ac_label_button_pressed), mainwin);

	ac_label = gtk_label_new("");
	gtk_container_add(GTK_CONTAINER(ac_button), ac_label);

	gtk_widget_show_all(hbox_stat);

	gtk_widget_hide(offline_switch);
	gtk_widget_hide(progressbar);
	gtk_widget_hide(warning_btn);
#else
	online_switch = gtk_button_new ();
	offline_switch = gtk_button_new ();
	g_signal_connect (G_OBJECT(online_switch), "clicked", G_CALLBACK(online_switch_clicked), mainwin);
	g_signal_connect (G_OBJECT(offline_switch), "clicked", G_CALLBACK(online_switch_clicked), mainwin);
#endif
	/* create views */
	mainwin->folderview  = folderview  = folderview_create(mainwin);
	mainwin->summaryview = summaryview = summary_create(mainwin);
	mainwin->messageview = messageview = messageview_create(mainwin);

	/* init log instances data before creating log views */
	set_log_title(LOG_PROTOCOL, _("Network log"));
	set_log_prefs(LOG_PROTOCOL,
			&prefs_common.logwin_width,
			&prefs_common.logwin_height);
	set_log_title(LOG_DEBUG_FILTERING, _("Filtering/Processing debug log"));
	set_log_prefs(LOG_DEBUG_FILTERING,
			&prefs_common.filtering_debugwin_width,
			&prefs_common.filtering_debugwin_height);

	/* setup log windows */
	mainwin->logwin = log_window_create(LOG_PROTOCOL);
	log_window_init(mainwin->logwin);

	mainwin->filtering_debugwin = log_window_create(LOG_DEBUG_FILTERING);
	log_window_set_clipping(mainwin->logwin,
				prefs_common.cliplog,
				prefs_common.loglength);

	log_window_init(mainwin->filtering_debugwin);
	log_window_set_clipping(mainwin->filtering_debugwin,
				prefs_common.filtering_debug_cliplog,
				prefs_common.filtering_debug_loglength);
	if (prefs_common.enable_filtering_debug)
		log_message(LOG_DEBUG_FILTERING, _("filtering log enabled\n"));
	else
		log_message(LOG_DEBUG_FILTERING, _("filtering log disabled\n"));

	folderview->mainwin      = mainwin;
	folderview->summaryview  = summaryview;

	summaryview->mainwin     = mainwin;
	summaryview->folderview  = folderview;
	summaryview->messageview = messageview;
	summaryview->window      = window;

	mainwin->vbox           = vbox;
	mainwin->menubar        = menubar;
	mainwin->handlebox      = handlebox;
	mainwin->vbox_body      = vbox_body;
	mainwin->online_switch  = online_switch;
	mainwin->offline_switch    = offline_switch;
#ifndef GENERIC_UMPC
	messageview->statusbar  = statusbar;
	mainwin->statusbar      = statusbar;
	mainwin->hbox_stat      = hbox_stat;
	mainwin->progressbar    = progressbar;
	mainwin->statuslabel    = statuslabel;
	mainwin->online_pixmap  = online_pixmap;
	mainwin->offline_pixmap = offline_pixmap;
	mainwin->ac_button      = ac_button;
	mainwin->ac_label       = ac_label;
	/* set context IDs for status bar */
	mainwin->mainwin_cid = gtk_statusbar_get_context_id
		(GTK_STATUSBAR(statusbar), "Main Window");
	mainwin->folderview_cid = gtk_statusbar_get_context_id
		(GTK_STATUSBAR(statusbar), "Folder View");
	mainwin->summaryview_cid = gtk_statusbar_get_context_id
		(GTK_STATUSBAR(statusbar), "Summary View");
	mainwin->messageview_cid = gtk_statusbar_get_context_id
		(GTK_STATUSBAR(statusbar), "Message View");
	messageview->statusbar_cid = mainwin->messageview_cid;

#else
	messageview->statusbar  = NULL;
	mainwin->statusbar 	= NULL;
	mainwin->hbox_stat	= NULL;
	/* mainwin->progressbar is set in toolbar.c */
	mainwin->statuslabel    = NULL;
	mainwin->online_pixmap  = NULL;
	mainwin->offline_pixmap = NULL;
	mainwin->ac_button      = NULL;
	mainwin->ac_label       = NULL;
#endif
	
	/* allocate colors for summary view and folder view */
	summaryview->color_marked.red = summaryview->color_marked.green = 0;
	summaryview->color_marked.blue = (guint16)65535;

	summaryview->color_dim.red = summaryview->color_dim.green =
		summaryview->color_dim.blue = COLOR_DIM;

	folderview->color_new = prefs_common.color[COL_NEW];
	folderview->color_op = prefs_common.color[COL_TGT_FOLDER];

	debug_print("done.\n");

	messageview->visible = prefs_common.msgview_visible;

	main_window_set_widgets(mainwin, prefs_common.layout_mode);

	g_signal_connect(G_OBJECT(window), "size_allocate",
			 G_CALLBACK(main_window_size_allocate_cb),
			 mainwin);

	/* set menu items */
	cm_toggle_menu_set_active_full(mainwin->ui_manager, "Menu/View/Encoding/"CS_AUTO, TRUE);

	menuitem = NULL;
	switch (prefs_common.toolbar_style) {
	case TOOLBAR_NONE:
		menuitem = gtk_ui_manager_get_widget(mainwin->ui_manager, "/Menu/View/ShowHide/Toolbar/Hide");
		break;
	case TOOLBAR_ICON:
		menuitem = gtk_ui_manager_get_widget(mainwin->ui_manager, "/Menu/View/ShowHide/Toolbar/IconOnly");
		break;
	case TOOLBAR_TEXT:
		menuitem = gtk_ui_manager_get_widget(mainwin->ui_manager, "/Menu/View/ShowHide/Toolbar/TextOnly");
		break;
	case TOOLBAR_BOTH:
		menuitem = gtk_ui_manager_get_widget(mainwin->ui_manager, "/Menu/View/ShowHide/Toolbar/TextBelowIcon");
		break;
	case TOOLBAR_BOTH_HORIZ:
		menuitem = gtk_ui_manager_get_widget(mainwin->ui_manager, "/Menu/View/ShowHide/Toolbar/TextBesideIcon");
	}
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), TRUE);

	toolbar_set_style(mainwin->toolbar->toolbar, 
			  mainwin->handlebox, 
			  prefs_common.toolbar_style);
#ifndef GENERIC_UMPC
	gtk_widget_hide(mainwin->hbox_stat);
	menuitem = gtk_ui_manager_get_widget(mainwin->ui_manager, "/Menu/View/ShowHide/StatusBar");
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem),
				       prefs_common.show_statusbar);
#endif	
	menuitem = gtk_ui_manager_get_widget(mainwin->ui_manager, "/Menu/View/ShowHide/ColumnHeaders");
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem),
				       prefs_common.show_col_headers);
	/* set account selection menu */
	ac_menu = gtk_ui_manager_get_widget(mainwin->ui_manager, "/Menu/Configuration/ChangeAccount");
	mainwin->ac_menu = ac_menu;

	toolbar_main_set_sensitive(mainwin);

	/* create actions menu */
	main_window_update_actions_menu(mainwin);

	main_create_mailing_list_menu (mainwin, NULL);

	/* attach accel groups to main window */
#define	ADD_MENU_ACCEL_GROUP_TO_WINDOW(menu,win)			\
	gtk_window_add_accel_group					\
		(GTK_WINDOW(win), 					\
		 gtk_ui_manager_get_accel_group(gtkut_ui_manager()));

	ADD_MENU_ACCEL_GROUP_TO_WINDOW(summaryview->popupmenu, mainwin->window);
	
	g_signal_connect(G_OBJECT(window), "window_state_event",
			 G_CALLBACK(mainwindow_state_event_cb), mainwin);
	g_signal_connect(G_OBJECT(window), "visibility_notify_event",
			 G_CALLBACK(mainwindow_visibility_event_cb), mainwin);
	gtk_widget_add_events(GTK_WIDGET(window), GDK_VISIBILITY_NOTIFY_MASK);

	if (prefs_common.layout_mode == VERTICAL_LAYOUT ||
	    prefs_common.layout_mode == SMALL_LAYOUT) {
		summary_relayout(mainwin->summaryview);	
	}
	summary_update_unread(mainwin->summaryview, NULL);
	
	/* initialize views */
	folderview_init(folderview);
	summary_init(summaryview);
	messageview_init(messageview);
#ifdef USE_GNUTLS
	sslcertwindow_register_hook();
#endif
	mainwin->lock_count = 0;
	mainwin->menu_lock_count = 0;
	mainwin->cursor_count = 0;

	mainwin->progressindicator_hook =
		hooks_register_hook(PROGRESSINDICATOR_HOOKLIST, mainwindow_progressindicator_hook, mainwin);

	if (!watch_cursor)
		watch_cursor = gdk_cursor_new_for_display(
				gtk_widget_get_display(mainwin->window), GDK_WATCH);
	if (!hand_cursor)
		hand_cursor = gdk_cursor_new_for_display(
				gtk_widget_get_display(mainwin->window), GDK_HAND2);

	/* init work_offline */
	if (prefs_common.work_offline)
		online_switch_clicked (GTK_BUTTON(online_switch), mainwin);

	mainwindow_colorlabel_menu_create(mainwin, FALSE);
	mainwindow_tags_menu_create(mainwin, FALSE);

	if (prefs_common.mainwin_fullscreen) {
		cm_toggle_menu_set_active_full(mainwin->ui_manager, 
			"Menu/View/FullScreen",
			TRUE);
	}
	if (prefs_common.mainwin_menubar)
		cm_toggle_menu_set_active_full(mainwin->ui_manager,"Menu/View/ShowHide/MenuBar", TRUE);
	
	return mainwin;
}

void main_window_update_actions_menu(MainWindow *mainwin)
{
	action_update_mainwin_menu(mainwin->ui_manager, "/Menu/Tools/Actions", mainwin);
}

void main_window_cursor_wait(MainWindow *mainwin)
{
	GdkDisplay *display;

	display = gdk_display_get_default();

	if (mainwin->cursor_count == 0) {
		gdk_window_set_cursor(gtk_widget_get_window(mainwin->window), watch_cursor);
		textview_cursor_wait(mainwin->messageview->mimeview->textview);
	}
	
	mainwin->cursor_count++;

	gdk_display_flush(display);
}

void main_window_cursor_normal(MainWindow *mainwin)
{
	GdkDisplay *display;

	display = gdk_display_get_default();

	if (mainwin->cursor_count)
		mainwin->cursor_count--;

	if (mainwin->cursor_count == 0) {
		gdk_window_set_cursor(gtk_widget_get_window(mainwin->window), NULL);
		textview_cursor_normal(mainwin->messageview->mimeview->textview);
	}
	gdk_display_flush(display);
}

/* lock / unlock the user-interface */
void main_window_lock(MainWindow *mainwin)
{
	if (mainwin->lock_count == 0 && mainwin->ac_button)
		gtk_widget_set_sensitive(mainwin->ac_button, FALSE);

	mainwin->lock_count++;

	main_window_set_menu_sensitive(mainwin);
	toolbar_main_set_sensitive(mainwin);
}

void main_window_unlock(MainWindow *mainwin)
{
	if (mainwin->lock_count)
		mainwin->lock_count--;

	main_window_set_menu_sensitive(mainwin);
	toolbar_main_set_sensitive(mainwin);

	if (mainwin->lock_count == 0 && mainwin->ac_button)
		gtk_widget_set_sensitive(mainwin->ac_button, TRUE);
}

static void main_window_menu_callback_block(MainWindow *mainwin)
{
	mainwin->menu_lock_count++;
}

static void main_window_menu_callback_unblock(MainWindow *mainwin)
{
	if (mainwin->menu_lock_count)
		mainwin->menu_lock_count--;
}

static guint prefs_tag = 0;

void main_window_reflect_prefs_all(void)
{
	main_window_reflect_prefs_all_real(FALSE);
}

static gboolean reflect_prefs_timeout_cb(gpointer data) 
{
	gboolean pixmap_theme_changed = GPOINTER_TO_INT(data);
	GList *cur;
	MainWindow *mainwin;
#ifndef GENERIC_UMPC
	GtkWidget *pixmap;
#endif
	for (cur = mainwin_list; cur != NULL; cur = cur->next) {
		mainwin = (MainWindow *)cur->data;

		main_window_show_cur_account(mainwin);
		main_window_set_menu_sensitive(mainwin);
		toolbar_main_set_sensitive(mainwin);

		/* pixmap themes */
		if (pixmap_theme_changed) {
			toolbar_update(TOOLBAR_MAIN, mainwin);
			messageview_reflect_prefs_pixmap_theme();
			compose_reflect_prefs_pixmap_theme();
			folderview_reinit_fonts(mainwin->folderview);
			folderview_init(mainwin->folderview);
			summary_reflect_prefs_pixmap_theme(mainwin->summaryview);
			foldersel_reflect_prefs_pixmap_theme();
#ifndef USE_ALT_ADDRBOOK
			addressbook_reflect_prefs_pixmap_theme();
#endif
#ifndef GENERIC_UMPC
			pixmap = stock_pixmap_widget(STOCK_PIXMAP_ONLINE);
			gtk_container_remove(GTK_CONTAINER(mainwin->online_switch), 
					     mainwin->online_pixmap);
			gtk_container_add (GTK_CONTAINER(mainwin->online_switch), pixmap);
			gtk_widget_show(pixmap);
			mainwin->online_pixmap = pixmap;
			pixmap = stock_pixmap_widget(STOCK_PIXMAP_OFFLINE);
			gtk_container_remove(GTK_CONTAINER(mainwin->offline_switch), 
					     mainwin->offline_pixmap);
			gtk_container_add (GTK_CONTAINER(mainwin->offline_switch), pixmap);
			gtk_widget_show(pixmap);
			mainwin->offline_pixmap = pixmap;
#endif
			hooks_invoke(THEME_CHANGED_HOOKLIST, NULL);
		}

		headerview_set_font(mainwin->messageview->headerview);
		headerview_set_visibility(mainwin->messageview->headerview,
					  prefs_common.display_header_pane);
		textview_reflect_prefs(mainwin->messageview->mimeview->textview);
		folderview_reflect_prefs();
		summary_reflect_prefs();
#ifndef GENERIC_UMPC
		summary_redisplay_msg(mainwin->summaryview);
#endif
		if (prefs_common.layout_mode == SMALL_LAYOUT) {
			if (mainwin->in_folder) {
				mainwindow_enter_folder(mainwin);
			} else {
				mainwindow_exit_folder(mainwin);
			}
		}
	}
	prefs_tag = 0;
	return FALSE;
}

void main_window_reflect_prefs_all_now(void)
{
	reflect_prefs_timeout_cb(GINT_TO_POINTER(FALSE));
}

void main_window_reflect_prefs_custom_colors(MainWindow *mainwin)
{
	GtkMenuShell *menu;
	GList *children, *cur;

	/* re-create colorlabel submenu */
	menu = GTK_MENU_SHELL(mainwin->colorlabel_menu);
	cm_return_if_fail(menu != NULL);

	/* clear items. get item pointers. */
	children = gtk_container_get_children(GTK_CONTAINER(menu));
	for (cur = children; cur != NULL && cur->data != NULL; cur = cur->next) {
		g_signal_handlers_disconnect_matched
			 (gtk_ui_manager_get_accel_group(mainwin->ui_manager), 
			 G_SIGNAL_MATCH_DATA|G_SIGNAL_MATCH_FUNC,
			 0, 0, NULL, mainwin_accel_changed_cb, cur->data);
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(cur->data), NULL);
	}
	g_list_free(children);
	mainwindow_colorlabel_menu_create(mainwin, TRUE);
	summary_reflect_prefs_custom_colors(mainwin->summaryview);
	folderview_reinit_fonts(mainwin->folderview);
}

static gint tags_tag = 0;
static gboolean main_window_reflect_tags_changes_real(gpointer data)
{
	GtkMenuShell *menu;
	GList *children, *cur;
	MainWindow *mainwin = (MainWindow *)data;

	if (summary_is_locked(mainwin->summaryview)) {
		return TRUE;
	}
	/* re-create tags submenu */
	menu = GTK_MENU_SHELL(mainwin->tags_menu);
	cm_return_val_if_fail(menu != NULL, FALSE);

	/* clear items. get item pointers. */
	children = gtk_container_get_children(GTK_CONTAINER(menu));
	for (cur = children; cur != NULL && cur->data != NULL; cur = cur->next) {
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(cur->data), NULL);
	}
	g_list_free(children);
	mainwindow_tags_menu_create(mainwin, TRUE);
	summary_reflect_tags_changes(mainwin->summaryview);
	
	tags_tag = 0;
	return FALSE;
}

void main_window_reflect_tags_changes(MainWindow *mainwin)
{
	if (tags_tag == 0) {
		tags_tag = g_timeout_add(100, main_window_reflect_tags_changes_real, 
						mainwin);
	}
}

void main_window_reflect_prefs_all_real(gboolean pixmap_theme_changed)
{
	if (prefs_tag == 0) {
		prefs_tag = g_timeout_add(100, reflect_prefs_timeout_cb, 
						GINT_TO_POINTER(pixmap_theme_changed));
	}
}

void main_window_set_summary_column(void)
{
	GList *cur;
	MainWindow *mainwin;

	for (cur = mainwin_list; cur != NULL; cur = cur->next) {
		mainwin = (MainWindow *)cur->data;
		summary_set_column_order(mainwin->summaryview);
	}
}

void main_window_set_folder_column(void)
{
	GList *cur;
	MainWindow *mainwin;

	for (cur = mainwin_list; cur != NULL; cur = cur->next) {
		mainwin = (MainWindow *)cur->data;
		folderview_set_column_order(mainwin->folderview);
	}
}

static void main_window_set_account_selector_menu(MainWindow *mainwin,
						  GList *account_list)
{
	GList *cur_ac;
	GtkWidget *menuitem;
	PrefsAccount *ac_prefs;
	GtkWidget *menu;
	gchar *accel_path;

	menu = gtk_menu_new();
	gtk_menu_set_accel_group (GTK_MENU (menu), 
		gtk_ui_manager_get_accel_group(mainwin->ui_manager));

	for (cur_ac = account_list; cur_ac != NULL; cur_ac = cur_ac->next) {
		ac_prefs = (PrefsAccount *)cur_ac->data;

		menuitem = gtk_menu_item_new_with_label
			(ac_prefs->account_name
			 ? ac_prefs->account_name : _("Untitled"));
		gtk_widget_show(menuitem);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
		g_signal_connect(G_OBJECT(menuitem), "activate",
				 G_CALLBACK(account_selector_menu_cb),
				 ac_prefs);
		accel_path = g_strconcat("<Actions>/Menu/Configuration/ChangeAccount/",(ac_prefs->account_name
			 ? ac_prefs->account_name : _("Untitled")), NULL );
		gtk_menu_item_set_accel_path(GTK_MENU_ITEM(menuitem), accel_path);
		g_free(accel_path);
	}
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(mainwin->ac_menu), menu);
}

static void main_window_set_account_receive_menu(MainWindow *mainwin,
						 GList *account_list)
{
	GList *children, *child;
	GtkWidget *menu;
	GtkWidget *menuitem;
	PrefsAccount *ac_prefs;

	menu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(
		gtk_ui_manager_get_widget(mainwin->ui_manager, "/Menu/Message/Receive")));

	/* search for separator */
	children = gtk_container_get_children(GTK_CONTAINER(menu));
	for (child = children; child != NULL; child = child->next) {
		if (child->data == gtk_ui_manager_get_widget(mainwin->ui_manager, "/Menu/Message/Receive/Separator1")) {
			child = child->next;
			break;
		}
	}

	/* destroy all previous menu item */
	while (child != NULL) {
		gtk_widget_destroy(GTK_WIDGET(child->data));
		child = child->next;
	}

	g_list_free(children);

	for (child = account_list; child != NULL; child = child->next) {
		ac_prefs = (PrefsAccount *)child->data;

		if (ac_prefs->protocol == A_NONE)
			continue;

		menuitem = gtk_menu_item_new_with_label
			(ac_prefs->account_name ? ac_prefs->account_name
			 : _("Untitled"));
		gtk_widget_show(menuitem);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
		g_signal_connect(G_OBJECT(menuitem), "activate",
				 G_CALLBACK(account_receive_menu_cb),
				 ac_prefs);
	}
}

static void main_window_set_toolbar_combo_receive_menu(MainWindow *mainwin,
						       GList *account_list)
{
	GList *cur_ac;
	GtkWidget *menuitem;
	PrefsAccount *ac_prefs;
	GtkWidget *menu = NULL;

	if (mainwin->toolbar->getall_btn == NULL) /* button doesn't exist */
		return;

	menu = gtk_menu_tool_button_get_menu(GTK_MENU_TOOL_BUTTON(mainwin->toolbar->getall_btn));
	if (menu)
		gtk_widget_destroy(menu);
	menu = gtk_menu_new();

	for (cur_ac = account_list; cur_ac != NULL; cur_ac = cur_ac->next) {
		ac_prefs = (PrefsAccount *)cur_ac->data;

		if (ac_prefs->protocol == A_NONE)
			continue;

		menuitem = gtk_menu_item_new_with_label
			(ac_prefs->account_name
			 ? ac_prefs->account_name : _("Untitled"));
		gtk_widget_show(menuitem);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
		g_signal_connect(G_OBJECT(menuitem), "activate",
				 G_CALLBACK(account_receive_menu_cb),
				 ac_prefs);
	}
	gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(mainwin->toolbar->getall_btn), menu);
}

static void main_window_set_toolbar_combo_compose_menu(MainWindow *mainwin,
						       GList *account_list)
{
#ifndef GENERIC_UMPC
	GList *cur_ac;
	GtkWidget *menuitem;
	PrefsAccount *ac_prefs;
	GtkWidget *menu = NULL;

	if (mainwin->toolbar->compose_mail_btn == NULL) /* button doesn't exist */
		return;

	menu = gtk_menu_tool_button_get_menu(GTK_MENU_TOOL_BUTTON(mainwin->toolbar->compose_mail_btn));
	if (menu)
		gtk_widget_destroy(menu);
	menu = gtk_menu_new();

	for (cur_ac = account_list; cur_ac != NULL; cur_ac = cur_ac->next) {
		ac_prefs = (PrefsAccount *)cur_ac->data;

		menuitem = gtk_menu_item_new_with_label
			(ac_prefs->account_name
			 ? ac_prefs->account_name : _("Untitled"));
		gtk_widget_show(menuitem);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
		g_signal_connect(G_OBJECT(menuitem), "activate",
				 G_CALLBACK(account_compose_menu_cb),
				 ac_prefs);
	}
	gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(mainwin->toolbar->compose_mail_btn), menu);
#endif
}

void main_window_set_account_menu(GList *account_list)
{
	GList *cur;
	MainWindow *mainwin;

	for (cur = mainwin_list; cur != NULL; cur = cur->next) {
		mainwin = (MainWindow *)cur->data;
		main_window_set_account_selector_menu(mainwin, account_list);
		main_window_set_account_receive_menu(mainwin, account_list);
		main_window_set_toolbar_combo_receive_menu(mainwin, account_list);
		main_window_set_toolbar_combo_compose_menu(mainwin, account_list);
	}
	hooks_invoke(ACCOUNT_LIST_CHANGED_HOOKLIST, NULL);
}

void main_window_set_account_menu_only_toolbar(GList *account_list)
{
	GList *cur;
	MainWindow *mainwin;

	for (cur = mainwin_list; cur != NULL; cur = cur->next) {
		mainwin = (MainWindow *)cur->data;
		main_window_set_toolbar_combo_receive_menu(mainwin, account_list);
		main_window_set_toolbar_combo_compose_menu(mainwin, account_list);
	}
}

static void main_window_show_cur_account(MainWindow *mainwin)
{
	gchar *buf;
	gchar *ac_name;

	ac_name = g_strdup(cur_account
			   ? (cur_account->account_name
			      ? cur_account->account_name : _("Untitled"))
			   : _("none"));

	if (cur_account)
		buf = g_strdup_printf("%s - %s", ac_name, PROG_VERSION);
	else
		buf = g_strdup(PROG_VERSION);
	gtk_window_set_title(GTK_WINDOW(mainwin->window), buf);
	g_free(buf);

	if (mainwin->ac_label)
		gtk_label_set_text(GTK_LABEL(mainwin->ac_label), ac_name);
	if (mainwin->ac_button)
		gtk_widget_queue_resize(mainwin->ac_button);

	g_free(ac_name);
}
#ifndef GENERIC_UMPC
static void main_window_separation_change(MainWindow *mainwin, LayoutType layout_mode)
{
	GtkWidget *folder_wid  = GTK_WIDGET_PTR(mainwin->folderview);
	GtkWidget *summary_wid = GTK_WIDGET_PTR(mainwin->summaryview);
	GtkWidget *message_wid = mainwin->messageview->vbox;

	if (layout_mode == prefs_common.layout_mode) 
		return;

	debug_print("Changing window separation type from %d to %d\n",
		    prefs_common.layout_mode, layout_mode);

	/* remove widgets from those containers */
	g_object_ref(folder_wid);
	g_object_ref(summary_wid);
	g_object_ref(message_wid);
	gtk_container_remove
		(GTK_CONTAINER(gtk_widget_get_parent(folder_wid)), folder_wid);
	gtk_container_remove
		(GTK_CONTAINER(gtk_widget_get_parent(summary_wid)), summary_wid);
	gtk_container_remove
		(GTK_CONTAINER(gtk_widget_get_parent(message_wid)), message_wid);

	gtk_widget_hide(mainwin->window);
	main_window_set_widgets(mainwin, layout_mode);
	gtk_widget_show(mainwin->window);

	g_object_unref(folder_wid);
	g_object_unref(summary_wid);
	g_object_unref(message_wid);
}
#endif
void mainwindow_reset_paned(GtkPaned *paned)
{
		gint min, max, mid;

		if (gtk_paned_get_child1(GTK_PANED(paned)))
			gtk_widget_show(gtk_paned_get_child1(GTK_PANED(paned)));
		if (gtk_paned_get_child2(GTK_PANED(paned)))
			gtk_widget_show(gtk_paned_get_child2(GTK_PANED(paned)));

GTK_EVENTS_FLUSH();
        	g_object_get (G_OBJECT(paned),
                        	"min-position",
                        	&min, NULL);
        	g_object_get (G_OBJECT(paned),
                        	"max-position",
                        	&max, NULL);
		mid = (min+max)/2;
		gtk_paned_set_position(GTK_PANED(paned), mid);
}

static void mainwin_paned_show_first(GtkPaned *paned)
{
		gint max;
        	g_object_get (G_OBJECT(paned),
                        	"max-position",
                        	&max, NULL);

		if (gtk_paned_get_child1(GTK_PANED(paned)))
			gtk_widget_show(gtk_paned_get_child1(GTK_PANED(paned)));
		if (gtk_paned_get_child2(GTK_PANED(paned)))
			gtk_widget_hide(gtk_paned_get_child2(GTK_PANED(paned)));
		gtk_paned_set_position(GTK_PANED(paned), max);
}

static void mainwin_paned_show_last(GtkPaned *paned)
{
		gint min;
        	g_object_get (G_OBJECT(paned),
                        	"min-position",
                        	&min, NULL);

		if (gtk_paned_get_child1(GTK_PANED(paned)))
			gtk_widget_hide(gtk_paned_get_child1(GTK_PANED(paned)));
		if (gtk_paned_get_child2(GTK_PANED(paned)))
			gtk_widget_show(gtk_paned_get_child2(GTK_PANED(paned)));
		gtk_paned_set_position(GTK_PANED(paned), min);
}

void main_window_toggle_message_view(MainWindow *mainwin)
{
	SummaryView *summaryview = mainwin->summaryview;
	GtkWidget *ppaned = NULL;
	GtkWidget *container = NULL;

	switch (prefs_common.layout_mode) {
	case NORMAL_LAYOUT:
	case VERTICAL_LAYOUT:
	case SMALL_LAYOUT:
		ppaned = mainwin->vpaned;
		container = mainwin->hpaned;
		if (gtk_widget_get_parent(ppaned) != NULL) {
			mainwin->messageview->visible = FALSE;
			summaryview->displayed = NULL;
			g_object_ref(ppaned);
			gtk_container_remove(GTK_CONTAINER(container), ppaned);
			gtk_widget_reparent(GTK_WIDGET_PTR(summaryview), container);
		} else {
			mainwin->messageview->visible = TRUE;
			gtk_widget_reparent(GTK_WIDGET_PTR(summaryview), ppaned);
			gtk_container_add(GTK_CONTAINER(container), ppaned);
			gtk_paned_set_position(GTK_PANED(ppaned),
					       prefs_common.summaryview_height);
			g_object_unref(ppaned);
		}
		break;
	case WIDE_LAYOUT:
		container = mainwin->vpaned;
		if (gtk_widget_get_parent(mainwin->messageview->vbox) != NULL) {
			mainwin->messageview->visible = FALSE;
			summaryview->displayed = NULL;
			g_object_ref(mainwin->messageview->vbox);
			gtk_container_remove(GTK_CONTAINER(container), mainwin->messageview->vbox);
		} else {
			mainwin->messageview->visible = TRUE;
			gtk_container_add(GTK_CONTAINER(container), mainwin->messageview->vbox);
			g_object_unref(mainwin->messageview->vbox);
		}
		break;
	case WIDE_MSGLIST_LAYOUT:
		g_warning("can't hide messageview in this wide msglist layout");
		break;
	}

	if (messageview_is_visible(mainwin->messageview))
		 gtk_image_set_from_icon_name(GTK_IMAGE(mainwin->summaryview->toggle_arrow),
					      "pan-down-symbolic", GTK_ICON_SIZE_MENU);
	else
		gtk_image_set_from_icon_name(GTK_IMAGE(mainwin->summaryview->toggle_arrow),
					     "pan-up-symbolic", GTK_ICON_SIZE_MENU);

	if (mainwin->messageview->visible == FALSE)
		messageview_clear(mainwin->messageview);

	main_window_set_menu_sensitive(mainwin);

	prefs_common.msgview_visible = mainwin->messageview->visible;

	if (messageview_is_visible(mainwin->messageview)) {
		gtk_widget_queue_resize(mainwin->hpaned);
		gtk_widget_queue_resize(mainwin->vpaned);
	}
	summary_grab_focus(summaryview);
	if (!summary_is_list(summaryview)) {
		summary_show(summaryview, summaryview->folder_item, FALSE);
	}
}

void main_window_get_size(MainWindow *mainwin)
{
	GtkAllocation allocation;

	if (mainwin_list == NULL || mainwin->messageview == NULL) {
		debug_print("called after messageview "
			    "has been deallocated!\n");
		return;
	}

	gtk_widget_get_allocation(GTK_WIDGET_PTR(mainwin->summaryview), &allocation);
	if (allocation.width > 1 && allocation.height > 1) {
		prefs_common.summaryview_width = allocation.width;

		if (messageview_is_visible(mainwin->messageview))
			prefs_common.summaryview_height = allocation.height;

		prefs_common.mainview_width = allocation.width;
	}

	gtk_window_get_size(GTK_WINDOW(mainwin->window), &allocation.width, &allocation.height);
	if (allocation.width > 1 && allocation.height > 1) {
		prefs_common.mainview_height = allocation.height;
		prefs_common.mainwin_width   = allocation.width;
		prefs_common.mainwin_height  = allocation.height;
	}

	gtk_widget_get_allocation(GTK_WIDGET_PTR(mainwin->folderview), &allocation);
	if (allocation.width > 1 && allocation.height > 1) {
		prefs_common.folderview_width  = allocation.width;
		prefs_common.folderview_height = allocation.height;
	}
	
	gtk_widget_get_allocation(GTK_WIDGET_PTR(mainwin->messageview), &allocation);
	if (allocation.width > 1 && allocation.height > 1) {
		prefs_common.msgview_width = allocation.width;
		prefs_common.msgview_height = allocation.height;
	}

/*	debug_print("summaryview size: %d x %d\n",
		    prefs_common.summaryview_width,
		    prefs_common.summaryview_height);
	debug_print("folderview size: %d x %d\n",
		    prefs_common.folderview_width,
		    prefs_common.folderview_height);
	debug_print("messageview size: %d x %d\n",
		    prefs_common.msgview_width,
		    prefs_common.msgview_height); */
}

void main_window_get_position(MainWindow *mainwin)
{
	gint x, y;

	if (prefs_common.mainwin_maximised || prefs_common.mainwin_fullscreen)
		return;

	gtk_window_get_position(GTK_WINDOW(mainwin->window), &x, &y);

	prefs_common.mainview_x = x;
	prefs_common.mainview_y = y;
	prefs_common.mainwin_x = x;
	prefs_common.mainwin_y = y;

	debug_print("main window position: %d, %d\n", x, y);
}

void main_window_progress_on(MainWindow *mainwin)
{
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(mainwin->progressbar), "");
}

void main_window_progress_off(MainWindow *mainwin)
{
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(mainwin->progressbar), 0.0);
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(mainwin->progressbar), "");
}

gboolean main_window_empty_trash(MainWindow *mainwin, gboolean confirm, gboolean for_quit)
{
	if (confirm && procmsg_have_trashed_mails_fast()) {
		AlertValue val;
		
		if (for_quit)
			val = alertpanel(_("Empty trash"),
			       _("Delete all messages in trash folders?"),
			       NULL, _("_No"), NULL, _("_Yes"), NULL, _("Don't quit"),
						 ALERTFOCUS_SECOND);
		else
			val = alertpanel(_("Empty trash"),
			       _("Delete all messages in trash folders?"),
			       NULL, _("_No"), NULL, _("_Yes"), NULL, NULL,
						 ALERTFOCUS_SECOND);
		if (val == G_ALERTALTERNATE) {
			debug_print("will empty trash\n");
		} else if (val == G_ALERTDEFAULT) {
			debug_print("will not empty trash\n");
			return TRUE;
		} else {
			return FALSE; /* cancel exit */
		}
		manage_window_focus_in(mainwin->window, NULL, NULL);
	}

	procmsg_empty_all_trash();

	if (mainwin->summaryview->folder_item &&
	    mainwin->summaryview->folder_item->stype == F_TRASH)
		folderview_grab_focus(mainwin->folderview);
	return TRUE;
}

static void main_window_add_mailbox(MainWindow *mainwin)
{
	gchar *path;
	Folder *folder;

	path = input_dialog(_("Add mailbox"),
			    _("Input the location of the mailbox.\n"
			      "The location can be either the full path or relative to the \n"
			      "home directory.\n"
			      "If the location of an existing mailbox is specified, it will be\n"
			      "scanned automatically."),
			    "Mail");
	if (!path) return;
	if (folder_find_from_path(path)) {
		alertpanel_error(_("The mailbox '%s' already exists."), path);
		g_free(path);
		return;
	}
	folder = folder_new(folder_get_class_from_string("mh"), 
			    !strcmp(path, "Mail") ? _("Mailbox") : 
			    g_path_get_basename(path), path);
	g_free(path);

	if (folder->klass->create_tree(folder) < 0) {
		alertpanel_error(_("Creation of the mailbox failed.\n"
				   "Maybe some files already exist, or you don't have the permission to write there."));
		folder_destroy(folder);
		return;
	}

	folder_add(folder);
	folder_set_ui_func(folder, scan_tree_func, mainwin);
	folder_scan_tree(folder, TRUE);
	folder_set_ui_func(folder, NULL, NULL);
}

SensitiveCondMask main_window_get_current_state(MainWindow *mainwin)
{
	SensitiveCondMask state = 0;
	SummarySelection selection;
	FolderItem *item = mainwin->summaryview->folder_item;
	GList *account_list = account_get_list();
	GSList *tmp;
	
	selection = summary_get_selection_type(mainwin->summaryview);

#define UPDATE_STATE(...) \
	do { \
		state |= main_window_get_mask(__VA_ARGS__, -1); \
	} while (0)

	if (mainwin->lock_count == 0 && !claws_is_starting())
		UPDATE_STATE(M_UNLOCKED);
	if (selection != SUMMARY_NONE && selection != SUMMARY_SELECTED_NONE)
		UPDATE_STATE(M_MSG_SELECTED);
	if (item && item->total_msgs > 0)
		UPDATE_STATE(M_MSG_EXIST);
	if (item && item->path && folder_item_parent(item) && !item->no_select) {
		UPDATE_STATE(M_EXEC);
		/*		if (item->folder->type != F_NEWS) */
		UPDATE_STATE(M_ALLOW_DELETE);

		if (prefs_common.immediate_exec == 0
		    && mainwin->lock_count == 0)
			UPDATE_STATE(M_DELAY_EXEC);

		if ((selection == SUMMARY_NONE && item->hide_read_msgs)
		    || selection != SUMMARY_NONE)
			UPDATE_STATE(M_HIDE_READ_MSG);

		if ((selection == SUMMARY_NONE && item->hide_read_threads)
		    || selection != SUMMARY_NONE)
			UPDATE_STATE(M_HIDE_READ_THREADS);
	}		
	if (mainwin->summaryview->threaded)
		UPDATE_STATE(M_THREADED);
	else
		UPDATE_STATE(M_UNTHREADED);
	if (selection == SUMMARY_SELECTED_SINGLE ||
	    selection == SUMMARY_SELECTED_MULTIPLE)
		UPDATE_STATE(M_TARGET_EXIST);
	if (selection == SUMMARY_SELECTED_SINGLE)
		UPDATE_STATE(M_SINGLE_TARGET_EXIST);
	if (mainwin->summaryview->folder_item &&
	    mainwin->summaryview->folder_item->folder->klass->type == F_NEWS)
		UPDATE_STATE(M_NEWS);
	else
		UPDATE_STATE(M_NOT_NEWS);
	if (mainwin->summaryview->folder_item &&
	    (mainwin->summaryview->folder_item->stype != F_TRASH ||
	     !folder_has_parent_of_type(mainwin->summaryview->folder_item, F_TRASH)))
		UPDATE_STATE(M_NOT_TRASH);

	if (mainwin->summaryview->folder_item
	    && mainwin->summaryview->folder_item->stype != F_DRAFT)
		UPDATE_STATE(M_NOT_DRAFT);

	if (prefs_common.actions_list && g_slist_length(prefs_common.actions_list))
		UPDATE_STATE(M_ACTIONS_EXIST);

	tmp = tags_get_list();
	if (tmp && g_slist_length(tmp))
		UPDATE_STATE(M_TAGS_EXIST);
	g_slist_free(tmp);

	if (procmsg_have_queued_mails_fast() && !procmsg_is_sending())
		UPDATE_STATE(M_HAVE_QUEUED_MAILS);

	if (selection == SUMMARY_SELECTED_SINGLE &&
	    (item &&
	     (folder_has_parent_of_type(item, F_DRAFT) ||
	      folder_has_parent_of_type(item, F_OUTBOX) ||
	      folder_has_parent_of_type(item, F_QUEUE))))
		UPDATE_STATE(M_ALLOW_REEDIT);
	if (cur_account)
		UPDATE_STATE(M_HAVE_ACCOUNT);
	
	if (cur_account && cur_account->protocol != A_NONE)
		UPDATE_STATE(M_HAVE_RETRIEVABLE_ACCOUNT);

	if (item && item->prefs->processing && selection != SUMMARY_NONE)
		UPDATE_STATE(M_HAVE_PROCESSING);

	if (g_list_length(account_list) > 1)
		UPDATE_STATE(M_HAVE_MULTI_ACCOUNT);

	for (account_list = account_get_list(); account_list != NULL; account_list = account_list->next) {
		if (((PrefsAccount*)account_list->data)->protocol != A_NONE) {
			UPDATE_STATE(M_HAVE_ANY_RETRIEVABLE_ACCOUNT);
			break;
		}
	}

	for (account_list = account_get_list(); account_list != NULL; account_list = account_list->next) {
		if (((PrefsAccount*)account_list->data)->protocol == A_NNTP) {
			UPDATE_STATE(M_HAVE_NEWS_ACCOUNT);
			break;
		}
	}
	
	if (procmsg_spam_can_learn() &&
	    (mainwin->summaryview->folder_item &&
	     mainwin->summaryview->folder_item->folder->klass->type != F_UNKNOWN &&
	     mainwin->summaryview->folder_item->folder->klass->type != F_NEWS)) {
		UPDATE_STATE(M_CAN_LEARN_SPAM);
	}

	if (mainwin->summaryview->folder_item) {
		UPDATE_STATE(M_FOLDER_SELECTED);
	}

	if (inc_is_active())
		UPDATE_STATE(M_INC_ACTIVE);
	if (imap_cancel_all_enabled())
		UPDATE_STATE(M_INC_ACTIVE);

	if (send_is_active() | procmsg_is_sending())
		UPDATE_STATE(M_SEND_ACTIVE);

	if (mainwin->summaryview->deleted > 0)
		UPDATE_STATE(M_DELETED_EXISTS);

	if (mainwin->summaryview->deleted > 0 ||
	    mainwin->summaryview->moved > 0 ||
	    mainwin->summaryview->copied > 0)
		UPDATE_STATE(M_DELAY_EXEC);

	if (summary_is_list(mainwin->summaryview))
		UPDATE_STATE(M_SUMMARY_ISLIST);

	if (prefs_common.layout_mode != SMALL_LAYOUT || mainwin->in_folder)
		UPDATE_STATE(M_IN_MSGLIST);

	for (account_list = account_get_list(); account_list != NULL; account_list = account_list->next) {
		PrefsAccount *account = account_list->data;
		if (account->session_passwd || account->session_smtp_passwd) {
			UPDATE_STATE(M_SESSION_PASSWORDS);
			break;
		}
	}

#ifndef PASSWORD_CRYPTO_OLD
	if (primary_passphrase_is_entered()) {
		UPDATE_STATE(M_MASTER_PASSPHRASE);
	}
#endif

#undef UPDATE_STATE

	return state;
}

SensitiveCondMask main_window_get_mask(SensitiveCond cond, ...)
{
	va_list args;
	gint i;
	SensitiveCondMask value;
	static SensitiveCondMask SensitiveCond_table[M_MAX_RESERVED];
	static gboolean first_time = TRUE;

	if (first_time) {
		for (i = 0; i < M_MAX_RESERVED; i++)
			SensitiveCond_table[i] = (SensitiveCondMask) ((SensitiveCondMask)1 << i);
		first_time = FALSE;
	}

	value = 0;

	va_start(args, cond); 
	for (i = (gint) cond; i >= 0 && i < M_MAX_RESERVED; i = va_arg(args, gint))
		value |= SensitiveCond_table[i];
	va_end(args);

	return value;
}

void main_window_set_menu_sensitive(MainWindow *mainwin)
{
	SensitiveCondMask state;
	gboolean sensitive;
	SummaryView *summaryview;
	gchar *menu_path;
	GtkWidget *menu;
	GList *children, *cur_item;
	gboolean mimepart_selected = FALSE;

	state = main_window_get_current_state(mainwin);

#define SET_SENSITIVE(entry_str, ...) \
{ \
	SensitiveCondMask cond = main_window_get_mask(__VA_ARGS__, -1); \
	cm_menu_set_sensitive_full(mainwin->ui_manager, \
			(const gchar *) entry_str, \
			((cond & state) == cond)); \
}
	SET_SENSITIVE("Menu/File/SaveAs", M_TARGET_EXIST);
	SET_SENSITIVE("Menu/File/SavePartAs", M_SINGLE_TARGET_EXIST);
	SET_SENSITIVE("Menu/File/Print", M_TARGET_EXIST);
	SET_SENSITIVE("Menu/File/Exit", M_UNLOCKED);

	SET_SENSITIVE("Menu/Edit/SelectThread", M_TARGET_EXIST, M_SUMMARY_ISLIST);
	SET_SENSITIVE("Menu/Edit/Find", M_SINGLE_TARGET_EXIST);
	SET_SENSITIVE("Menu/Edit/QuickSearch", M_IN_MSGLIST);
	SET_SENSITIVE("Menu/Edit/SearchFolder", M_TARGET_EXIST, M_SUMMARY_ISLIST);

	SET_SENSITIVE("Menu/View/SetColumns/Folderlist", M_UNLOCKED, M_SUMMARY_ISLIST);
	SET_SENSITIVE("Menu/View/Sort", M_EXEC, M_SUMMARY_ISLIST);
	SET_SENSITIVE("Menu/View/ThreadView", M_EXEC, M_SUMMARY_ISLIST);
	SET_SENSITIVE("Menu/View/ExpandThreads", M_MSG_EXIST, M_SUMMARY_ISLIST);
	SET_SENSITIVE("Menu/View/CollapseThreads", M_MSG_EXIST, M_SUMMARY_ISLIST);
	SET_SENSITIVE("Menu/View/HideReadThreads", M_HIDE_READ_THREADS, M_SUMMARY_ISLIST, M_NOT_DRAFT);
	SET_SENSITIVE("Menu/View/HideReadMessages", M_HIDE_READ_MSG, M_SUMMARY_ISLIST, M_NOT_DRAFT);
	SET_SENSITIVE("Menu/View/HideDelMessages", M_SUMMARY_ISLIST, M_NOT_DRAFT);
	SET_SENSITIVE("Menu/View/Goto/Prev", M_MSG_SELECTED);
	SET_SENSITIVE("Menu/View/Goto/Next", M_MSG_SELECTED);
	SET_SENSITIVE("Menu/View/Goto/PrevUnread", M_MSG_SELECTED);
	SET_SENSITIVE("Menu/View/Goto/NextUnread", M_MSG_SELECTED);
	SET_SENSITIVE("Menu/View/Goto/PrevNew", M_MSG_SELECTED);
	SET_SENSITIVE("Menu/View/Goto/NextNew", M_MSG_SELECTED);
	SET_SENSITIVE("Menu/View/Goto/PrevMarked", M_MSG_SELECTED);
	SET_SENSITIVE("Menu/View/Goto/NextMarked", M_MSG_SELECTED);
	SET_SENSITIVE("Menu/View/Goto/PrevLabeled", M_MSG_SELECTED);
	SET_SENSITIVE("Menu/View/Goto/NextLabeled", M_MSG_SELECTED);
	SET_SENSITIVE("Menu/View/Goto/ParentMessage", M_SINGLE_TARGET_EXIST);
	SET_SENSITIVE("Menu/View/Goto/NextPart", M_SINGLE_TARGET_EXIST);
	SET_SENSITIVE("Menu/View/Goto/PrevPart", M_SINGLE_TARGET_EXIST);
	SET_SENSITIVE("Menu/View/OpenNewWindow", M_SINGLE_TARGET_EXIST);
	SET_SENSITIVE("Menu/View/MessageSource", M_SINGLE_TARGET_EXIST);
	SET_SENSITIVE("Menu/View/Part", M_SINGLE_TARGET_EXIST);
	SET_SENSITIVE("Menu/View/AllHeaders", M_SINGLE_TARGET_EXIST);
	SET_SENSITIVE("Menu/View/Quotes", M_SINGLE_TARGET_EXIST);

	SET_SENSITIVE("Menu/Message/Receive/CurrentAccount", M_HAVE_ACCOUNT, M_UNLOCKED, M_HAVE_RETRIEVABLE_ACCOUNT);
	SET_SENSITIVE("Menu/Message/Receive/AllAccounts", M_HAVE_ACCOUNT, M_UNLOCKED, M_HAVE_ANY_RETRIEVABLE_ACCOUNT);
	SET_SENSITIVE("Menu/Message/Receive/CancelReceiving", M_INC_ACTIVE);
	SET_SENSITIVE("Menu/Message/SendQueue", M_HAVE_ACCOUNT, M_HAVE_QUEUED_MAILS);
	SET_SENSITIVE("Menu/Message/CancelSending", M_SEND_ACTIVE);
	SET_SENSITIVE("Menu/Message/ComposeEmail", M_HAVE_ACCOUNT);
	SET_SENSITIVE("Menu/Message/ComposeNews", M_HAVE_NEWS_ACCOUNT);
	SET_SENSITIVE("Menu/Message/Reply", M_HAVE_ACCOUNT, M_TARGET_EXIST, M_SUMMARY_ISLIST);
	SET_SENSITIVE("Menu/Message/ReplyTo", M_HAVE_ACCOUNT, M_TARGET_EXIST, M_SUMMARY_ISLIST);
	SET_SENSITIVE("Menu/Message/FollowupReply", M_HAVE_ACCOUNT, M_TARGET_EXIST, M_NEWS, M_SUMMARY_ISLIST);
	SET_SENSITIVE("Menu/Message/Forward", M_HAVE_ACCOUNT, M_TARGET_EXIST, M_SUMMARY_ISLIST);
	SET_SENSITIVE("Menu/Message/ForwardAtt", M_HAVE_ACCOUNT, M_TARGET_EXIST, M_SUMMARY_ISLIST);
	SET_SENSITIVE("Menu/Message/Redirect", M_HAVE_ACCOUNT, M_TARGET_EXIST, M_SUMMARY_ISLIST);
	SET_SENSITIVE("Menu/Message/Move", M_TARGET_EXIST, M_ALLOW_DELETE, M_NOT_NEWS);
	SET_SENSITIVE("Menu/Message/Copy", M_TARGET_EXIST, M_EXEC);
	SET_SENSITIVE("Menu/Message/Trash", M_TARGET_EXIST, M_ALLOW_DELETE, M_NOT_NEWS, M_NOT_TRASH);
	SET_SENSITIVE("Menu/Message/Delete", M_TARGET_EXIST, M_ALLOW_DELETE);
	SET_SENSITIVE("Menu/Message/TrashThread", M_TARGET_EXIST, M_SUMMARY_ISLIST);
	SET_SENSITIVE("Menu/Message/DeleteThread", M_TARGET_EXIST, M_SUMMARY_ISLIST);
	SET_SENSITIVE("Menu/Message/CancelNews", M_TARGET_EXIST, M_ALLOW_DELETE, M_NEWS);
	SET_SENSITIVE("Menu/Message/Marks", M_TARGET_EXIST, M_SUMMARY_ISLIST);
	SET_SENSITIVE("Menu/Message/Marks/MarkSpam", M_TARGET_EXIST, M_CAN_LEARN_SPAM);
	SET_SENSITIVE("Menu/Message/Marks/MarkHam", M_TARGET_EXIST, M_CAN_LEARN_SPAM);
	SET_SENSITIVE("Menu/Message/Marks/IgnoreThread", M_TARGET_EXIST);
	SET_SENSITIVE("Menu/Message/Marks/UnignoreThread", M_TARGET_EXIST);
	SET_SENSITIVE("Menu/Message/Marks/Lock", M_TARGET_EXIST);
	SET_SENSITIVE("Menu/Message/Marks/Unlock", M_TARGET_EXIST);
	SET_SENSITIVE("Menu/Message/ColorLabel", M_TARGET_EXIST);
	SET_SENSITIVE("Menu/Message/Tags", M_TARGET_EXIST);
	SET_SENSITIVE("Menu/Message/Reedit", M_HAVE_ACCOUNT, M_ALLOW_REEDIT);
	SET_SENSITIVE("Menu/Message/CheckSignature", M_SINGLE_TARGET_EXIST);

	SET_SENSITIVE("Menu/Tools/AddSenderToAB", M_SINGLE_TARGET_EXIST);
	SET_SENSITIVE("Menu/Tools/CollectAddresses", M_FOLDER_SELECTED);
	SET_SENSITIVE("Menu/Tools/CollectAddresses/FromFolder", M_FOLDER_SELECTED);
	SET_SENSITIVE("Menu/Tools/CollectAddresses/FromSelected", M_TARGET_EXIST);
	SET_SENSITIVE("Menu/Tools/FilterFolder", M_MSG_EXIST, M_EXEC);
	SET_SENSITIVE("Menu/Tools/FilterSelected", M_TARGET_EXIST, M_EXEC);
	SET_SENSITIVE("Menu/Tools/RunProcessing", M_HAVE_PROCESSING);
	SET_SENSITIVE("Menu/Tools/CreateFilterRule", M_SINGLE_TARGET_EXIST, M_UNLOCKED);
	SET_SENSITIVE("Menu/Tools/CreateProcessingRule", M_SINGLE_TARGET_EXIST, M_UNLOCKED);
	SET_SENSITIVE("Menu/Tools/ListUrls", M_TARGET_EXIST);
	SET_SENSITIVE("Menu/Tools/Actions", M_TARGET_EXIST, M_ACTIONS_EXIST);
	SET_SENSITIVE("Menu/Tools/Execute", M_DELAY_EXEC);
	SET_SENSITIVE("Menu/Tools/Expunge", M_DELETED_EXISTS);
	SET_SENSITIVE("Menu/Tools/ForgetSessionPasswords", M_SESSION_PASSWORDS);
#ifndef PASSWORD_CRYPTO_OLD
	SET_SENSITIVE("Menu/Tools/ForgetPrimaryPassphrase", M_MASTER_PASSPHRASE);
#endif
	SET_SENSITIVE("Menu/Tools/DeleteDuplicates/SelFolder", M_MSG_EXIST, M_ALLOW_DELETE);

	SET_SENSITIVE("Menu/Configuration", M_UNLOCKED);
	SET_SENSITIVE("Menu/Configuration/ChangeAccount", M_HAVE_MULTI_ACCOUNT);
	SET_SENSITIVE("Menu/Configuration/AccountPrefs", M_UNLOCKED);
	SET_SENSITIVE("Menu/Configuration/CreateAccount", M_UNLOCKED);
	SET_SENSITIVE("Menu/Configuration/EditAccounts", M_UNLOCKED);
#undef SET_SENSITIVE

	menu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(
		gtk_ui_manager_get_widget(mainwin->ui_manager, "/Menu/Message/Receive")));

	children = gtk_container_get_children(GTK_CONTAINER(menu));
	for (cur_item = children; cur_item != NULL; cur_item = cur_item->next) {
		if (cur_item->data == gtk_ui_manager_get_widget(mainwin->ui_manager, "/Menu/Message/Receive/Separator1")) {
			cur_item = cur_item->next;
			break;
		}
	}

	for (; cur_item != NULL; cur_item = cur_item->next) {
		gtk_widget_set_sensitive(GTK_WIDGET(cur_item->data),
					 (main_window_get_mask(M_UNLOCKED, -1) & state) != 0);
	}

	g_list_free(children);

	main_window_menu_callback_block(mainwin);

	cm_toggle_menu_set_active_full(mainwin->ui_manager, "Menu/View/ShowHide/MessageView",
			      messageview_is_visible(mainwin->messageview));

	summaryview = mainwin->summaryview;

	switch (summaryview->sort_key) {
	case SORT_BY_NUMBER:
		menu_path = "Menu/View/Sort/Number"; break;
	case SORT_BY_SIZE:
		menu_path = "Menu/View/Sort/Size"; break;
	case SORT_BY_DATE:
		menu_path = "Menu/View/Sort/Date"; break;
	case SORT_BY_THREAD_DATE:
		menu_path = "Menu/View/Sort/ThreadDate"; break;
	case SORT_BY_FROM:
		menu_path = "Menu/View/Sort/From"; break;
	case SORT_BY_TO:
		menu_path = "Menu/View/Sort/To"; break;
	case SORT_BY_SUBJECT:
		menu_path = "Menu/View/Sort/Subject"; break;
	case SORT_BY_LABEL:
		menu_path = "Menu/View/Sort/Color"; break;
	case SORT_BY_MARK:
		menu_path = "Menu/View/Sort/Mark"; break;
	case SORT_BY_STATUS:
		menu_path = "Menu/View/Sort/Status"; break;
	case SORT_BY_MIME:
		menu_path = "Menu/View/Sort/Attachment"; break;
	case SORT_BY_SCORE:
		menu_path = "Menu/View/Sort/Score"; break;
	case SORT_BY_LOCKED:
		menu_path = "Menu/View/Sort/Locked"; break;
	case SORT_BY_TAGS:
		menu_path = "Menu/View/Sort/Tag"; break;
	case SORT_BY_NONE:
	default:
		menu_path = "Menu/View/Sort/DontSort"; break;
	}
	cm_toggle_menu_set_active_full(mainwin->ui_manager, menu_path, TRUE);

	if (summaryview->sort_type == SORT_ASCENDING) {
		cm_toggle_menu_set_active_full(mainwin->ui_manager, "Menu/View/Sort/Ascending", TRUE);
	} else {
		cm_toggle_menu_set_active_full(mainwin->ui_manager, "Menu/View/Sort/Descending", TRUE);
	}

	if (summaryview->sort_key != SORT_BY_NONE) {
		cm_menu_set_sensitive_full(mainwin->ui_manager, "Menu/View/Sort/Ascending", TRUE);
		cm_menu_set_sensitive_full(mainwin->ui_manager, "Menu/View/Sort/Descending", TRUE);
	} else {
		cm_menu_set_sensitive_full(mainwin->ui_manager, "Menu/View/Sort/Ascending", FALSE);
		cm_menu_set_sensitive_full(mainwin->ui_manager, "Menu/View/Sort/Descending", FALSE);
	}

	if (mainwin->messageview
	&&  mainwin->messageview->mimeview
	&&  mainwin->messageview->mimeview->textview)
		cm_toggle_menu_set_active_full(mainwin->ui_manager, "Menu/View/AllHeaders",
			      			prefs_common.show_all_headers);
	cm_toggle_menu_set_active_full(mainwin->ui_manager, "Menu/View/ThreadView", (state & main_window_get_mask(M_THREADED, -1)) != 0);
	cm_menu_set_sensitive_full(mainwin->ui_manager, "Menu/View/ExpandThreads", (state & main_window_get_mask(M_THREADED, -1)) != 0);
	cm_menu_set_sensitive_full(mainwin->ui_manager, "Menu/View/CollapseThreads", (state & main_window_get_mask(M_THREADED, -1)) != 0);
	cm_menu_set_sensitive_full(mainwin->ui_manager, "Menu/View/HideReadThreads", (state & main_window_get_mask(M_THREADED, -1)) != 0 && (state & main_window_get_mask(M_NOT_DRAFT, -1)) != 0);
	cm_toggle_menu_set_active_full(mainwin->ui_manager, "Menu/View/Quotes/CollapseAll", (prefs_common.hide_quotes == 1));
	cm_toggle_menu_set_active_full(mainwin->ui_manager, "Menu/View/Quotes/Collapse2", (prefs_common.hide_quotes == 2));
	cm_toggle_menu_set_active_full(mainwin->ui_manager, "Menu/View/Quotes/Collapse3", (prefs_common.hide_quotes == 3));

	if (mainwin->summaryview->folder_item && !mainwin->summaryview->folder_item->threaded)
		cm_toggle_menu_set_active_full(mainwin->ui_manager, "Menu/View/HideReadThreads", FALSE);
	if ((mainwin->summaryview->folder_item && mainwin->summaryview->folder_item->hide_read_msgs) ||
	    quicksearch_has_sat_predicate(mainwin->summaryview->quicksearch))
		cm_menu_set_sensitive_full(mainwin->ui_manager, "Menu/View/HideReadThreads", FALSE);
	if ((mainwin->summaryview->folder_item && mainwin->summaryview->folder_item->hide_read_threads) ||
	    quicksearch_has_sat_predicate(mainwin->summaryview->quicksearch))
		cm_menu_set_sensitive_full(mainwin->ui_manager, "Menu/View/HideReadMessages", FALSE);
	if (quicksearch_has_sat_predicate(mainwin->summaryview->quicksearch))
		cm_menu_set_sensitive_full(mainwin->ui_manager, "Menu/View/HideDelMessages", FALSE);

	cm_menu_set_sensitive_full(mainwin->ui_manager, "Menu/View/Goto/PrevHistory",
		messageview_nav_has_prev(mainwin->messageview));
	cm_menu_set_sensitive_full(mainwin->ui_manager, "Menu/View/Goto/NextHistory",
		messageview_nav_has_next(mainwin->messageview));

	if (mainwin->messageview 
	&&  mainwin->messageview->mimeview)
		mimepart_selected = !mimeview_tree_is_empty(mainwin->messageview->mimeview);

	cm_menu_set_sensitive_full(mainwin->ui_manager, "Menu/File/SavePartAs", mimepart_selected);
	cm_menu_set_sensitive_full(mainwin->ui_manager, "Menu/View/Goto/NextPart", mimepart_selected);
	cm_menu_set_sensitive_full(mainwin->ui_manager, "Menu/View/Goto/PrevPart", mimepart_selected);
	cm_menu_set_sensitive_full(mainwin->ui_manager, "Menu/View/Part", mimepart_selected);
	cm_menu_set_sensitive_full(mainwin->ui_manager, "Menu/Message/CheckSignature", 
				   mimepart_selected && mainwin->messageview->mimeview->signed_part);

	sensitive = TRUE;
	if (mimepart_selected) {
		MimeInfo *partinfo = messageview_get_selected_mime_part(mainwin->messageview);

		if (partinfo && (partinfo->type == MIMETYPE_MESSAGE ||
				 partinfo->type == MIMETYPE_IMAGE ||
				 partinfo->type == MIMETYPE_MULTIPART)) {
			sensitive = FALSE;
		}
	}
	cm_menu_set_sensitive_full(mainwin->ui_manager, "Menu/View/Part/AsText", sensitive);

	main_window_menu_callback_unblock(mainwin);
}

void main_create_mailing_list_menu (MainWindow *mainwin, MsgInfo *msginfo)
{
	gint is_menu = 0;
	
	if (msginfo) 
		is_menu = mailing_list_create_submenu (mainwin, msginfo);
	if (is_menu)
		cm_menu_set_sensitive_full(mainwin->ui_manager, "Menu/Message/MailingList", TRUE);
	else
		cm_menu_set_sensitive_full(mainwin->ui_manager, "Menu/Message/MailingList", FALSE);
}

static gint mailing_list_create_submenu (MainWindow *mainwin, MsgInfo *msginfo)
{
	gint menu_nb = 0;
	GtkWidget *menuitem;
	
	if (!msginfo || !msginfo->extradata) {
		cm_menu_set_sensitive_full(mainwin->ui_manager, "Menu/Message/MailingList/Post", FALSE);
		cm_menu_set_sensitive_full(mainwin->ui_manager, "Menu/Message/MailingList/Help", FALSE);
		cm_menu_set_sensitive_full(mainwin->ui_manager, "Menu/Message/MailingList/Subscribe", FALSE);
		cm_menu_set_sensitive_full(mainwin->ui_manager, "Menu/Message/MailingList/Unsubscribe", FALSE);
		cm_menu_set_sensitive_full(mainwin->ui_manager, "Menu/Message/MailingList/ViewArchive", FALSE);
		cm_menu_set_sensitive_full(mainwin->ui_manager, "Menu/Message/MailingList/ContactOwner", FALSE);
		return 0;
	}
		
	/* Mailing list post */
	if (!g_strcmp0 (msginfo->extradata->list_post, "NO")) {
		g_free(msginfo->extradata->list_post);
		msginfo->extradata->list_post = g_strdup (_("No posting allowed"));
 	}
 	menuitem = gtk_ui_manager_get_widget(mainwin->ui_manager, "/Menu/Message/MailingList/Post");
 		
 	menu_nb += mailing_list_populate_submenu (menuitem, msginfo->extradata->list_post);
 
 	/* Mailing list help */
	menuitem = gtk_ui_manager_get_widget(mainwin->ui_manager, "/Menu/Message/MailingList/Help");
	
	menu_nb += mailing_list_populate_submenu (menuitem, msginfo->extradata->list_help);

	/* Mailing list subscribe */
	menuitem = gtk_ui_manager_get_widget(mainwin->ui_manager, "/Menu/Message/MailingList/Subscribe");
	
	menu_nb += mailing_list_populate_submenu (menuitem, msginfo->extradata->list_subscribe);
		
	/* Mailing list unsubscribe */
	menuitem = gtk_ui_manager_get_widget(mainwin->ui_manager, "/Menu/Message/MailingList/Unsubscribe");
	
	menu_nb += mailing_list_populate_submenu (menuitem, msginfo->extradata->list_unsubscribe);
	
	/* Mailing list view archive */
	menuitem = gtk_ui_manager_get_widget(mainwin->ui_manager, "/Menu/Message/MailingList/ViewArchive");
	
	menu_nb += mailing_list_populate_submenu (menuitem, msginfo->extradata->list_archive);
	
	/* Mailing list contact owner */
	menuitem = gtk_ui_manager_get_widget(mainwin->ui_manager, "/Menu/Message/MailingList/ContactOwner");
	
	menu_nb += mailing_list_populate_submenu (menuitem, msginfo->extradata->list_owner);
	
	return menu_nb;
}

static gint mailing_list_populate_submenu (GtkWidget *menuitem, const gchar * list_header)
{
	GtkWidget *item, *menu;
	const gchar *url_pt ;
	gchar url_decoded[BUFFSIZE];
	GList *children, *amenu;
	gint menu_nb = 0;
	
	menu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(menuitem));
	
	/* First delete old submenu */
	/* FIXME: we can optimize this, and only change/add/delete necessary items */
	children = gtk_container_get_children(GTK_CONTAINER(menu));
	for (amenu = children; amenu; amenu = amenu->next) {
		item = GTK_WIDGET (amenu->data);
		gtk_widget_destroy (item);
	}
	g_list_free(children);
	if (list_header) {
		for (url_pt = list_header; url_pt && *url_pt;) {
			get_url_part (&url_pt, url_decoded);
			item = NULL;
			if (!g_ascii_strncasecmp(url_decoded, "mailto:", 7)) {
 				item = cm_menu_item_new_label_from_url ((url_decoded));
				g_signal_connect(G_OBJECT(item), "activate",
						 G_CALLBACK(mailing_list_compose),
						 NULL);
			}
 			else if (!g_ascii_strncasecmp(url_decoded, "http:", 5) ||
				 !g_ascii_strncasecmp(url_decoded, "https:",6)) {

				item = cm_menu_item_new_label_from_url ((url_decoded));
				g_signal_connect(G_OBJECT(item), "activate",
						 G_CALLBACK(mailing_list_open_uri),
						 NULL);
			} 
			if (item) {
				gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
				gtk_widget_show (item);
				menu_nb++;
			}
		}
	}
	if (menu_nb)
		gtk_widget_set_sensitive (menuitem, TRUE);
	else
		gtk_widget_set_sensitive (menuitem, FALSE);
		

	return menu_nb;
}

static void get_url_part (const gchar **buffer, gchar *url_decoded)
{
	gchar tmp[BUFFSIZE];
	const gchar *buf;
	gint i = 0;
	buf = *buffer;
	gboolean with_plus = TRUE;

	if (buf == 0x00) {
		*url_decoded = '\0';
		*buffer = NULL;
		return;
	}
	/* Ignore spaces, comments  and tabs () */
	for (;*buf == ' ' || *buf == '(' || *buf == '\t'; buf++)
		if (*buf == '(')
			for (;*buf != ')' && *buf != 0x00; buf++);
	
	/* First non space and non comment must be a < */
	if (*buf =='<' ) {
		buf++;
		if (!strncmp(buf, "mailto:", strlen("mailto:")))
			with_plus = FALSE;
		for (i = 0;
		     *buf != '>' && *buf != 0x00 && i < BUFFSIZE;
			tmp[i++] = *(buf++));
		if (*buf == 0) {
			*buffer = NULL;
			*url_decoded = '\0';
			return;
		}
		buf++;
	}
	else  {
		*buffer = NULL;
		*url_decoded = '\0';
		return;
	}
	
	*url_decoded = '\0';
	*buffer = NULL;
	
	if (i == BUFFSIZE) {
		return;
	}
	tmp[i] = 0x00;
	decode_uri_with_plus (url_decoded, (const gchar *)tmp, with_plus);

	/* Prepare the work for the next url in the list */
	/* after the closing bracket >, ignore space, comments and tabs */
	for (;buf && *buf && (*buf == ' ' || *buf == '(' || *buf == '\t'); buf++)
		if (*buf == '(')
			for (;*buf != ')' && *buf != 0x00; buf++);
			
	if (!buf || !*buf) {
		*buffer = NULL;
		return;
	}

	/* now first non space, non comment must be a comma */
	if (*buf != ',')
		for (;*buf != 0x00; buf++);
	else
		buf++;
	*buffer = buf;
}
	
static void mailing_list_compose (GtkWidget *w, gpointer *data)
{
	const gchar *mailto;
	PrefsAccount *account = NULL;
	FolderItem   *folder_item = NULL;

	mailto = gtk_label_get_text(GTK_LABEL (gtk_bin_get_child(GTK_BIN((w)))));
	if (mainwindow_get_mainwindow()) {
		folder_item = mainwindow_get_mainwindow()->summaryview->folder_item;
		if (folder_item && folder_item->prefs && folder_item->prefs->enable_default_account)
			account = account_find_from_id(folder_item->prefs->default_account);
		if (folder_item && !account)
			account = account_find_from_item(folder_item);
	}
	if (mailto)
		compose_new_with_folderitem(account, folder_item, mailto+7);
}
 
 static void mailing_list_open_uri (GtkWidget *w, gpointer *data)
{
 
 	const gchar *mailto;
 
	mailto = gtk_label_get_text(GTK_LABEL (gtk_bin_get_child(GTK_BIN((w)))));
	if (mailto)
 		open_uri (mailto, prefs_common_get_uri_cmd());
} 
	
static void fix_folderview_scroll(MainWindow *mainwin)
{
	static gboolean fix_done = FALSE;

	if (fix_done)
		return;

	gtk_widget_queue_resize(mainwin->folderview->ctree);

	fix_done = TRUE;
}

void main_window_popup(MainWindow *mainwin)
{
	static gboolean first_start = TRUE;

	if (!gtk_widget_get_visible(GTK_WIDGET(mainwin->window)))
		main_window_show(mainwin);

	if (prefs_common.mainwin_maximised)
		gtk_window_maximize(GTK_WINDOW(mainwin->window));

	if (first_start) {
		first_start = FALSE;
	} else {
		gtkut_window_popup(mainwin->window);
	}

	if (prefs_common.layout_mode == SMALL_LAYOUT) {
		if (mainwin->in_folder) {
			mainwindow_enter_folder(mainwin);
		} else {
			mainwindow_exit_folder(mainwin);
		}
	}
	fix_folderview_scroll(mainwin);
}

void main_window_show(MainWindow *mainwin)
{
	gtk_widget_show(mainwin->window);
	gtk_widget_show(mainwin->vbox_body);
#ifndef GENERIC_UMPC
        gtk_window_move(GTK_WINDOW(mainwin->window),
                                 prefs_common.mainwin_x,
                                 prefs_common.mainwin_y);
#endif
}

void main_window_hide(MainWindow *mainwin)
{
	main_window_get_size(mainwin);
	main_window_get_position(mainwin);

	gtk_widget_hide(mainwin->window);
	gtk_widget_hide(mainwin->vbox_body);
}

static void main_window_set_widgets(MainWindow *mainwin, LayoutType layout_mode)
{
	GtkWidget *folderwin = NULL;
	GtkWidget *messagewin = NULL;
	GtkWidget *hpaned;
	GtkWidget *vpaned;
	GtkWidget *vbox_body = mainwin->vbox_body;
	gboolean first_set = (mainwin->hpaned == NULL);
	debug_print("Setting widgets...\n");

#ifndef GENERIC_UMPC
	mainwin->messageview->statusbar = mainwin->statusbar;
	mainwin->messageview->statusbar_cid = mainwin->messageview_cid;
#endif
	/* clean top-most container */
	if (mainwin->hpaned) {
		if (gtk_widget_get_parent(mainwin->hpaned) == mainwin->vpaned)
			gtk_widget_destroy(mainwin->vpaned);
		else
			gtk_widget_destroy(mainwin->hpaned);
	}

	cm_menu_set_sensitive_full(mainwin->ui_manager, "Menu/View/ShowHide/MessageView", 
		(layout_mode != WIDE_MSGLIST_LAYOUT && layout_mode != SMALL_LAYOUT));
	switch (layout_mode) {
	case VERTICAL_LAYOUT:
	case NORMAL_LAYOUT:
	case SMALL_LAYOUT:
		hpaned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
		if (layout_mode == VERTICAL_LAYOUT)
			vpaned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
		else
			vpaned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
		gtk_box_pack_start(GTK_BOX(vbox_body), hpaned, TRUE, TRUE, 0);
		gtk_paned_add1(GTK_PANED(hpaned),
			       GTK_WIDGET_PTR(mainwin->folderview));
		gtk_widget_show(hpaned);

		if (messageview_is_visible(mainwin->messageview)) {
			gtk_paned_add2(GTK_PANED(hpaned), vpaned);
			gtk_paned_pack1(GTK_PANED(vpaned),
				       GTK_WIDGET_PTR(mainwin->summaryview), TRUE, TRUE);
		} else {
			gtk_paned_add2(GTK_PANED(hpaned),
				       GTK_WIDGET_PTR(mainwin->summaryview));
			g_object_ref(vpaned);
		}
		gtk_paned_add2(GTK_PANED(vpaned),
			       GTK_WIDGET_PTR(mainwin->messageview));
		gtk_widget_show(vpaned);
		if (layout_mode == SMALL_LAYOUT && first_set) {
			mainwin_paned_show_first(GTK_PANED(hpaned));
		}

		gtk_paned_set_position(GTK_PANED(hpaned),
				       prefs_common_get_prefs()->folderview_width);

		if (layout_mode == NORMAL_LAYOUT &&
		    messageview_is_visible(mainwin->messageview)) {
			gtk_paned_set_position(GTK_PANED(vpaned),
					       prefs_common_get_prefs()->summaryview_height);
		}
		if (layout_mode == VERTICAL_LAYOUT &&
		    messageview_is_visible(mainwin->messageview)) {
			gtk_paned_set_position(GTK_PANED(vpaned),
					       prefs_common_get_prefs()->summaryview_width);
		}

		break;
	case WIDE_LAYOUT:
		vpaned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
		hpaned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
		gtk_box_pack_start(GTK_BOX(vbox_body), vpaned, TRUE, TRUE, 0);
		gtk_paned_add1(GTK_PANED(vpaned), hpaned);

		gtk_paned_add1(GTK_PANED(hpaned),
			       GTK_WIDGET_PTR(mainwin->folderview));
		gtk_paned_add2(GTK_PANED(hpaned),
			       GTK_WIDGET_PTR(mainwin->summaryview));

		gtk_widget_show(hpaned);

		if (messageview_is_visible(mainwin->messageview)) {
			gtk_paned_add2(GTK_PANED(vpaned),
			       GTK_WIDGET_PTR(mainwin->messageview));	
		} else {
			g_object_ref(GTK_WIDGET_PTR(mainwin->messageview));
		}
		gtk_widget_show(vpaned);

		gtk_paned_set_position(GTK_PANED(hpaned),
				prefs_common_get_prefs()->folderview_width);

		if (messageview_is_visible(mainwin->messageview)) {
			gtk_paned_set_position(GTK_PANED(vpaned),
					prefs_common_get_prefs()->summaryview_height);
		}

		break;
	case WIDE_MSGLIST_LAYOUT:
		vpaned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
		hpaned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
		gtk_box_pack_start(GTK_BOX(vbox_body), vpaned, TRUE, TRUE, 0);

		gtk_paned_add1(GTK_PANED(vpaned),
			       GTK_WIDGET_PTR(mainwin->summaryview));
		gtk_paned_add1(GTK_PANED(hpaned),
			       GTK_WIDGET_PTR(mainwin->folderview));

		gtk_widget_show(hpaned);

		if (messageview_is_visible(mainwin->messageview)) {
			gtk_paned_add2(GTK_PANED(hpaned),
			       GTK_WIDGET_PTR(mainwin->messageview));	
		} else {
			g_object_ref(GTK_WIDGET_PTR(mainwin->messageview));
		}
		gtk_paned_add2(GTK_PANED(vpaned), hpaned);
		gtk_widget_show(vpaned);

		gtk_paned_set_position(GTK_PANED(hpaned),
				prefs_common_get_prefs()->folderview_width);
		gtk_paned_set_position(GTK_PANED(vpaned),
				prefs_common_get_prefs()->summaryview_height);

		break;
	default:
		g_warning("unknown layout");
		return;
	}

	mainwin->hpaned = hpaned;
	mainwin->vpaned = vpaned;

	if (layout_mode == SMALL_LAYOUT) {
		if (mainwin->messageview->visible)
			main_window_toggle_message_view(mainwin);
	} 

	if (layout_mode == SMALL_LAYOUT && first_set) {
		gtk_widget_realize(mainwin->window);
		gtk_widget_realize(mainwin->folderview->ctree);
		gtk_widget_realize(mainwin->summaryview->hbox);
		gtk_widget_realize(mainwin->summaryview->hbox_l);
		gtk_paned_set_position(GTK_PANED(mainwin->hpaned), 800);
	} else {
		gtk_window_set_default_size(GTK_WINDOW(mainwin->window),
				    prefs_common.mainwin_width,
				    prefs_common.mainwin_height);
	} 
	/* remove headerview if not in prefs */
	headerview_set_visibility(mainwin->messageview->headerview,
				  prefs_common.display_header_pane);

	if (messageview_is_visible(mainwin->messageview))
		gtk_image_set_from_icon_name(GTK_IMAGE(mainwin->summaryview->toggle_arrow),
					      "pan-down-symbolic", GTK_ICON_SIZE_MENU);
	else
		gtk_image_set_from_icon_name(GTK_IMAGE(mainwin->summaryview->toggle_arrow),
					      "pan-up-symbolic", GTK_ICON_SIZE_MENU);

	gtk_window_move(GTK_WINDOW(mainwin->window),
			prefs_common.mainwin_x,
			prefs_common.mainwin_y);

	/* CLAWS: previous "gtk_widget_show_all" makes noticeview
	 * and mimeview icon list/ctree lose track of their visibility states */
	if (!noticeview_is_visible(mainwin->messageview->noticeview)) 
		gtk_widget_hide(GTK_WIDGET_PTR(mainwin->messageview->noticeview));
	if (!noticeview_is_visible(mainwin->messageview->mimeview->siginfoview)) 
		gtk_widget_hide(GTK_WIDGET_PTR(mainwin->messageview->mimeview->siginfoview));
	if (mainwin->messageview->mimeview->ctree_mode)
		gtk_widget_hide(mainwin->messageview->mimeview->icon_mainbox);
	else 
		gtk_widget_hide(mainwin->messageview->mimeview->ctree_mainbox);

	prefs_common.layout_mode = layout_mode;

	cm_toggle_menu_set_active_full(mainwin->ui_manager, "Menu/View/ShowHide/MessageView", 
		 messageview_is_visible(mainwin->messageview));

#ifndef GENERIC_UMPC
	switch (prefs_common.layout_mode) {
	case NORMAL_LAYOUT:
		cm_toggle_menu_set_active_full(mainwin->ui_manager, "Menu/View/Layout/Standard", TRUE);
		break;
	case VERTICAL_LAYOUT:
		cm_toggle_menu_set_active_full(mainwin->ui_manager, "Menu/View/Layout/ThreeColumns", TRUE);
		break;
	case WIDE_LAYOUT:
		cm_toggle_menu_set_active_full(mainwin->ui_manager, "Menu/View/Layout/WideMessage", TRUE);
		break;
	case WIDE_MSGLIST_LAYOUT:
		cm_toggle_menu_set_active_full(mainwin->ui_manager, "Menu/View/Layout/WideMessageList", TRUE);
		break;
	case SMALL_LAYOUT:
		cm_toggle_menu_set_active_full(mainwin->ui_manager, "Menu/View/Layout/SmallScreen", TRUE);
		break;
	}
#endif

	if (folderwin) {
		g_signal_connect
			(G_OBJECT(folderwin), "size_allocate",
			 G_CALLBACK(folder_window_size_allocate_cb),
			 mainwin);
	}
	if (messagewin) {
		g_signal_connect
			(G_OBJECT(messagewin), "size_allocate",
			 G_CALLBACK(message_window_size_allocate_cb),
			 mainwin);
	}

	debug_print("Setting widgets done.\n");
}

void main_window_destroy_all(void)
{
	while (mainwin_list != NULL) {
		MainWindow *mainwin = (MainWindow*)mainwin_list->data;

		/* free toolbar stuff */
		toolbar_clear_list(TOOLBAR_MAIN);
		toolbar_destroy(mainwin->toolbar);

		summaryview_destroy(mainwin->summaryview);
		mainwin->folderview->mainwin = NULL;
		mainwin->summaryview->mainwin = NULL;
		mainwin->messageview->mainwin = NULL;

		g_free(mainwin->toolbar);		
		mainwin_list = g_list_remove(mainwin_list, mainwin);
		g_free(mainwin);
	}
	g_list_free(mainwin_list);
	mainwin_list = NULL;
}

#ifndef GENERIC_UMPC
static gboolean ac_label_button_pressed(GtkWidget *widget, GdkEventButton *event,
				    gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	GtkWidget *menu = NULL;
	
	if (!event) return FALSE;

	gtk_button_set_relief(GTK_BUTTON(widget), GTK_RELIEF_NORMAL);
	
	menu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(mainwin->ac_menu));

	gtk_menu_popup_at_widget(GTK_MENU(menu), widget, 3, 3, NULL);

	return TRUE;
}
#endif
static gint main_window_close_cb(GtkWidget *widget, GdkEventAny *event,
				 gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	gboolean close_allowed = TRUE;

	hooks_invoke(MAIN_WINDOW_CLOSE, &close_allowed);

	if (close_allowed && mainwin->lock_count == 0)
		app_exit_cb(NULL, data);

	return TRUE;
}

static void main_window_size_allocate_cb(GtkWidget *widget,
					 GtkAllocation *allocation,
					 gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	main_window_get_size(mainwin);
}

static void folder_window_size_allocate_cb(GtkWidget *widget,
					   GtkAllocation *allocation,
					   gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	main_window_get_size(mainwin);
}

static void message_window_size_allocate_cb(GtkWidget *widget,
					    GtkAllocation *allocation,
					    gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	main_window_get_size(mainwin);
}

static void add_mailbox_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	main_window_add_mailbox(mainwin);
}

static void update_folderview_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_show(mainwin->summaryview, NULL, FALSE);
	folderview_check_new_all();
}

static void foldersort_cb(GtkAction *action, gpointer data)
{
	foldersort_open();
}

static void import_mbox_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	/* only notify if import has failed */
	if (import_mbox(mainwin->summaryview->folder_item, NULL) == -1) {
		alertpanel_error(_("Mbox import has failed."));
	}
}

static void export_mbox_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	/* only notify if export has failed */
	if (export_mbox(mainwin->summaryview->folder_item) == -1) {
		alertpanel_error(_("Export to mbox has failed."));
	}
}

static void export_list_mbox_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	/* only notify if export has failed */
	if (summaryview_export_mbox_list(mainwin->summaryview) == -1) {
		alertpanel_error(_("Export to mbox has failed."));
	}
}

static void empty_trash_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	main_window_empty_trash(mainwin, TRUE, FALSE);
}

static void save_as_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_save_as(mainwin->summaryview);
}

static void print_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_print(mainwin->summaryview);
}

static void page_setup_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	GtkWindow *win;

	win = (mainwin ? GTK_WINDOW(mainwin->window) : NULL);

	printing_page_setup(win);
}

static void app_exit_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	if (prefs_common.clean_on_exit) {
		if (!main_window_empty_trash(mainwin, prefs_common.ask_on_clean, TRUE))
			return;
	}

	if (prefs_common.confirm_on_exit) {
		if (alertpanel(_("Exit"), _("Exit Claws Mail?"),
			       NULL, _("_Cancel"), NULL, _("_Quit"),
			       NULL, NULL, ALERTFOCUS_FIRST)
		    != G_ALERTALTERNATE)
			return;
		manage_window_focus_in(mainwin->window, NULL, NULL);
	}

	app_will_exit(NULL, mainwin);
}

static void search_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	message_search(mainwin->messageview);
}

static void search_folder_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	FolderItem *item = mainwin->summaryview->folder_item;
	cm_return_if_fail(item != NULL);
	summary_search(mainwin->summaryview);
}

static void mainwindow_quicksearch(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summaryview_activate_quicksearch(mainwin->summaryview, TRUE);
}

static void toggle_message_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	gboolean active;

	active = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));

	if (active != messageview_is_visible(mainwin->messageview))
		summary_toggle_view(mainwin->summaryview);
}

static void toggle_toolbar_cb(GtkAction *action, GtkRadioAction *current, gpointer data)
{
	gint value = gtk_radio_action_get_current_value (GTK_RADIO_ACTION (current));
	MainWindow *mainwin = (MainWindow *)data;
	toolbar_toggle(value, mainwin);
}

static void main_window_reply_cb(GtkAction *gaction, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	MessageView *msgview = (MessageView*)mainwin->messageview;
	GSList *msginfo_list = NULL;
	gint action = COMPOSE_REPLY;
	const gchar *a_name = gtk_action_get_name(gaction);

	DO_ACTION("Message/Reply", COMPOSE_REPLY);
	DO_ACTION("Message/ReplyTo/All", COMPOSE_REPLY_TO_ALL);
	DO_ACTION("Message/ReplyTo/Sender", COMPOSE_REPLY_TO_SENDER);
	DO_ACTION("Message/ReplyTo/List", COMPOSE_REPLY_TO_LIST);
	DO_ACTION("Message/Forward", COMPOSE_FORWARD_INLINE);
	DO_ACTION("Message/ForwardAtt", COMPOSE_FORWARD_AS_ATTACH);
	DO_ACTION("Message/Redirect", COMPOSE_REDIRECT);
	DO_ACTION("Message/FollowupReply", COMPOSE_FOLLOWUP_AND_REPLY_TO);

	cm_return_if_fail(msgview != NULL);

	msginfo_list = summary_get_selection(mainwin->summaryview);
	cm_return_if_fail(msginfo_list != NULL);

	if (!summary_has_opened_message(mainwin->summaryview)) {
		compose_reply_from_messageview(NULL, msginfo_list, action);
	} else if (summary_is_opened_message_selected(mainwin->summaryview)) {
		compose_reply_from_messageview(msgview, msginfo_list, action);
	} else {
		compose_reply_from_messageview(msgview, NULL, action);
	}

	g_slist_free(msginfo_list);
}

static void toggle_col_headers_cb(GtkAction *gaction, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	FolderView *folderview = mainwin->folderview;
	SummaryView *summaryview = mainwin->summaryview;
	MimeView *mimeview = mainwin->messageview->mimeview;

	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (gaction))) {
		gtk_cmclist_column_titles_show(GTK_CMCLIST(folderview->ctree));
		gtk_cmclist_column_titles_show(GTK_CMCLIST(summaryview->ctree));
  		gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(mimeview->ctree), TRUE);
		prefs_common.show_col_headers = TRUE;
	} else {
		gtk_cmclist_column_titles_hide(GTK_CMCLIST(folderview->ctree));
		gtk_cmclist_column_titles_hide(GTK_CMCLIST(summaryview->ctree));
   		gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(mimeview->ctree), FALSE);		
		prefs_common.show_col_headers = FALSE;
	}
}

#ifndef GENERIC_UMPC
static void toggle_statusbar_cb(GtkAction *gaction, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (gaction))) {
		gtk_widget_show(mainwin->hbox_stat);
		prefs_common.show_statusbar = TRUE;
	} else {
		gtk_widget_hide(mainwin->hbox_stat);
		prefs_common.show_statusbar = FALSE;
	}
}

static void set_layout_cb(GtkAction *action, GtkRadioAction *current, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	gint value = gtk_radio_action_get_current_value (GTK_RADIO_ACTION (current));
	LayoutType layout_mode = value;
	LayoutType old_layout_mode = prefs_common.layout_mode;
	if (mainwin->menu_lock_count) {
		return;
	}
	if (!gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (current))) {
		return;
	}
	
	if (layout_mode == prefs_common.layout_mode) {
		return;
	}
	
	if (!mainwin->messageview->visible && layout_mode != SMALL_LAYOUT)
		main_window_toggle_message_view(mainwin);
	else if (mainwin->messageview->visible && layout_mode == SMALL_LAYOUT)
		main_window_toggle_message_view(mainwin);

	main_window_separation_change(mainwin, layout_mode);
	mainwindow_reset_paned(GTK_PANED(mainwin->vpaned));
	if (old_layout_mode == SMALL_LAYOUT && layout_mode != SMALL_LAYOUT) {
		mainwindow_reset_paned(GTK_PANED(mainwin->hpaned));
		if (layout_mode == VERTICAL_LAYOUT)
			mainwindow_reset_paned(GTK_PANED(mainwin->vpaned));
	}
	if (old_layout_mode != SMALL_LAYOUT && layout_mode == SMALL_LAYOUT) {
		mainwin_paned_show_first(GTK_PANED(mainwin->hpaned));
		mainwindow_exit_folder(mainwin);
	}
	summary_relayout(mainwin->summaryview);	
	summary_update_unread(mainwin->summaryview, NULL);
}
#endif

void main_window_toggle_work_offline (MainWindow *mainwin, gboolean offline,
					gboolean ask_sync)
{
	static gboolean switching = FALSE;

	if (switching)
		return;

	switching = TRUE;

	offline_ask_sync = ask_sync;
	if (offline)
		online_switch_clicked (GTK_BUTTON(mainwin->online_switch), mainwin);
	else
		online_switch_clicked (GTK_BUTTON(mainwin->offline_switch), mainwin);
	offline_ask_sync = TRUE;

	inc_reset_offline_override_timers();

	switching = FALSE;
}

static void toggle_work_offline_cb (GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	main_window_toggle_work_offline(mainwin, gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)), TRUE);
}

static gboolean any_folder_want_synchronise(void)
{
	GList *folderlist = folder_get_list();

	/* see if there are synchronised folders */
	for (; folderlist; folderlist = folderlist->next) {
		Folder *folder = (Folder *)folderlist->data;
		if (folder_want_synchronise(folder)) {
			return TRUE;
		}
	}
	
	return FALSE;
}

static void mainwindow_check_synchronise(MainWindow *mainwin, gboolean ask)
{
	
	if (!any_folder_want_synchronise())
		return;

	if (offline_ask_sync && ask && alertpanel(_("Folder synchronisation"),
			_("Do you want to synchronise your folders now?"),
			NULL, _("_No"), NULL, _("_Synchronise"), NULL, NULL,
			ALERTFOCUS_SECOND) != G_ALERTALTERNATE)
		return;
	
	if (offline_ask_sync)
		folder_synchronise(NULL);
}

static void online_switch_clicked (GtkButton *btn, gpointer data) 
{
	MainWindow *mainwin;
	gboolean have_connectivity;

#ifdef HAVE_NETWORKMANAGER_SUPPORT
	have_connectivity = networkmanager_is_online(NULL); 
#else
	have_connectivity = TRUE;
#endif

	mainwin = (MainWindow *) data;
	
	cm_return_if_fail(mainwin != NULL);
	
	if (btn == GTK_BUTTON(mainwin->online_switch)) {
#ifndef GENERIC_UMPC
		gtk_widget_hide (mainwin->online_switch);
		gtk_widget_show (mainwin->offline_switch);
#endif
		cm_toggle_menu_set_active_full(mainwin->ui_manager, "Menu/File/OfflineMode", TRUE);

		inc_autocheck_timer_remove();
			
		/* go offline */
		if (prefs_common.work_offline)
			return;

		if(have_connectivity)
			mainwindow_check_synchronise(mainwin, TRUE);
		prefs_common.work_offline = TRUE;
		imap_disconnect_all(have_connectivity);
		nntp_disconnect_all(have_connectivity);
		hooks_invoke(OFFLINE_SWITCH_HOOKLIST, NULL);
	} else {
		/*go online */
		if (!prefs_common.work_offline)
			return;
#ifndef GENERIC_UMPC
		gtk_widget_hide (mainwin->offline_switch);
		gtk_widget_show (mainwin->online_switch);
#endif
		cm_toggle_menu_set_active_full(mainwin->ui_manager, "Menu/File/OfflineMode", FALSE);
		prefs_common.work_offline = FALSE;
		inc_autocheck_timer_set();
		refresh_resolvers();
		hooks_invoke(OFFLINE_SWITCH_HOOKLIST, NULL);
	}
}

static void addressbook_open_cb(GtkAction *action, gpointer data)
{
#ifndef USE_ALT_ADDRBOOK
	addressbook_open(NULL);
#else
	GError* error = NULL;
	
	addressbook_dbus_open(FALSE, &error);
	if (error) {
		g_warning("failed to open address book: %s", error->message);
		g_error_free(error);
	}
#endif
}

static void log_window_show_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	log_window_show(mainwin->logwin);
}

static void filtering_debug_window_show_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	log_window_show(mainwin->filtering_debugwin);
}

#ifdef G_OS_WIN32
static void debug_log_show_cb(GtkAction *action, gpointer data)
{
	GError *error = NULL;
	gchar *logpath8 = win32_debug_log_path();
	gunichar2 *logpath16;

	debug_print("opening '%s'\n", logpath8);

	logpath16 = g_utf8_to_utf16(logpath8, -1, NULL, NULL, &error);

	if (error != NULL) {
		g_warning("couldn't convert debug log path '%s' to UTF-16: %s",
				logpath8, error->message);
		g_error_free(error);
		g_free(logpath16);
		return;
	}

	HINSTANCE ret = ShellExecute(NULL, NULL, (LPCWSTR)logpath16,
			NULL, NULL, SW_SHOW);

	g_free(logpath8);
	g_free(logpath16);
	debug_print("ShellExecute result: %"G_GSIZE_FORMAT"\n", (gsize)ret);
}
#endif

static void inc_cancel_cb(GtkAction *action, gpointer data)
{
	inc_cancel_all();
	imap_cancel_all();
}

static void send_cancel_cb(GtkAction *action, gpointer data)
{
	send_cancel();
}

static void move_to_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_move_to(mainwin->summaryview);
}

static void copy_to_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_copy_to(mainwin->summaryview);
}

static void delete_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_delete(mainwin->summaryview);
}

static void delete_trash_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_delete_trash(mainwin->summaryview);
}

static void cancel_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_cancel(mainwin->summaryview);
}

static void open_msg_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_open_msg(mainwin->summaryview, TRUE, FALSE);
}

static void view_source_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_view_source(mainwin->summaryview);
}

static void show_all_header_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	if (mainwin->menu_lock_count) return;
	prefs_common.show_all_headers = 
			gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	summary_display_msg_selected(mainwin->summaryview,
				     gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)));
}

static void toggle_fullscreen_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	if (mainwin->menu_lock_count) return;
	if (!gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action))) {
		gtk_window_unfullscreen(GTK_WINDOW(mainwin->window));
		prefs_common.mainwin_fullscreen = FALSE;
	}
	else {
		prefs_common.mainwin_fullscreen = TRUE;
		gtk_window_fullscreen(GTK_WINDOW(mainwin->window));
	}
}

static void toggle_menubar_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	if (mainwin->menu_lock_count) return;
	if (!gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action))) {
		gtk_widget_hide(GTK_WIDGET(mainwin->menubar));
		prefs_common.mainwin_menubar = FALSE;
	} else {
		gtk_widget_show(GTK_WIDGET(mainwin->menubar));
		prefs_common.mainwin_menubar = TRUE;
	}
}

static void hide_quotes_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	if (mainwin->menu_lock_count) return;

	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action))) {
		const gchar *a_name = gtk_action_get_name(GTK_ACTION(action));
		if (!strcmp(a_name, "View/Quotes/CollapseAll")) prefs_common.hide_quotes = 1;
		else if (!strcmp(a_name, "View/Quotes/Collapse2")) prefs_common.hide_quotes = 2;
		else if (!strcmp(a_name, "View/Quotes/Collapse3")) prefs_common.hide_quotes = 3;
	} else
		prefs_common.hide_quotes = 0;

	mainwin->menu_lock_count++;
	cm_toggle_menu_set_active_full(mainwin->ui_manager, "Menu/View/Quotes/CollapseAll", (prefs_common.hide_quotes == 1));
	cm_toggle_menu_set_active_full(mainwin->ui_manager, "Menu/View/Quotes/Collapse2", (prefs_common.hide_quotes == 2));
	cm_toggle_menu_set_active_full(mainwin->ui_manager, "Menu/View/Quotes/Collapse3", (prefs_common.hide_quotes == 3));
	mainwin->menu_lock_count--;

	summary_redisplay_msg(mainwin->summaryview);
}

static void mark_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_mark(mainwin->summaryview);
}

static void unmark_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_unmark(mainwin->summaryview);
}

static void mark_as_read_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_mark_as_read(mainwin->summaryview);
}

static void mark_as_unread_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_mark_as_unread(mainwin->summaryview);
}

static void mark_all_read_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_mark_all_read(mainwin->summaryview, TRUE);
}

static void mark_all_unread_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_mark_all_unread(mainwin->summaryview, TRUE);
}

static void mark_as_spam_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_mark_as_spam(mainwin->summaryview, TRUE, NULL);
}

static void mark_as_ham_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_mark_as_spam(mainwin->summaryview, FALSE, NULL);
}

static void ignore_thread_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_ignore_thread(mainwin->summaryview);
}

static void unignore_thread_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_unignore_thread(mainwin->summaryview);
}

static void watch_thread_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_watch_thread(mainwin->summaryview);
}

static void unwatch_thread_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_unwatch_thread(mainwin->summaryview);
}

static void lock_msgs_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_msgs_lock(mainwin->summaryview);
}

static void unlock_msgs_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_msgs_unlock(mainwin->summaryview);
}


static void reedit_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_reedit(mainwin->summaryview);
}

static void open_urls_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	if (!mainwin->summaryview->displayed && mainwin->summaryview->selected) {
		summary_display_msg_selected(mainwin->summaryview, 
					     prefs_common.show_all_headers);
	}
	messageview_list_urls(mainwin->messageview);
}

static void add_address_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_add_address(mainwin->summaryview);
}

static void set_charset_cb(GtkAction *action, GtkRadioAction *current, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	gint value = gtk_radio_action_get_current_value (GTK_RADIO_ACTION (current));
	const gchar *str;

	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (current))) {
		str = conv_get_charset_str((CharSet)value);
		
		g_free(mainwin->messageview->forced_charset);
		mainwin->messageview->forced_charset = str ? g_strdup(str) : NULL;
		procmime_force_charset(str);
		
		summary_redisplay_msg(mainwin->summaryview);
		
		debug_print("forced charset: %s\n", str ? str : "Auto-Detect");
	}
}

static void set_decode_cb(GtkAction *action, GtkRadioAction *current, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	gint value = gtk_radio_action_get_current_value (GTK_RADIO_ACTION (current));
	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (current))) {
		mainwin->messageview->forced_encoding = (EncodingType)value;
		
		summary_redisplay_msg(mainwin->summaryview);
		
		debug_print("forced encoding: %d\n", value);
	}
}

static void hide_read_messages (GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	GtkWidget *menuitem = gtk_ui_manager_get_widget(mainwin->ui_manager, "/Menu/View/HideReadMessages");
	if (!mainwin->summaryview->folder_item
	    || g_object_get_data(G_OBJECT(menuitem), "dont_toggle"))
		return;
	summary_toggle_show_read_messages(mainwin->summaryview);
}

static void hide_del_messages (GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	GtkWidget *menuitem = gtk_ui_manager_get_widget(mainwin->ui_manager, "/Menu/View/HideDelMessages");
	if (!mainwin->summaryview->folder_item
	    || g_object_get_data(G_OBJECT(menuitem), "dont_toggle"))
		return;
	summary_toggle_show_del_messages(mainwin->summaryview);
}

static void hide_read_threads (GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	GtkWidget *menuitem = gtk_ui_manager_get_widget(mainwin->ui_manager, "/Menu/View/HideReadThreads");
	if (!mainwin->summaryview->folder_item
	    || g_object_get_data(G_OBJECT(menuitem), "dont_toggle"))
		return;
	summary_toggle_show_read_threads(mainwin->summaryview);
}

static void thread_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	gboolean threaded = FALSE;
	guint selected_msgnum = 0;

	if (mainwin->menu_lock_count) return;
	if (!mainwin->summaryview->folder_item) return;

	main_window_cursor_wait(mainwin);

	selected_msgnum = summary_get_msgnum(mainwin->summaryview, mainwin->summaryview->selected);

	threaded = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));

	mainwin->summaryview->folder_item->threaded = threaded; 

	mainwin->summaryview->threaded = threaded;

	summary_show(mainwin->summaryview, 
			mainwin->summaryview->folder_item, FALSE);
	summary_select_by_msgnum(mainwin->summaryview, selected_msgnum, FALSE);

	main_window_cursor_normal(mainwin);
}

static void expand_threads_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_expand_threads(mainwin->summaryview);
}

static void collapse_threads_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_collapse_threads(mainwin->summaryview);
}

static void set_summary_display_item_cb(GtkAction *action, gpointer data)
{
	prefs_summary_column_open();
}

static void set_folder_display_item_cb(GtkAction *action, gpointer data)
{
	prefs_folder_column_open();
}

static void sort_summary_cb(GtkAction *action, GtkRadioAction *current, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	gint value = gtk_radio_action_get_current_value (GTK_RADIO_ACTION (current));

	if (mainwin->menu_lock_count) return;

	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (current))) {
		summary_sort(mainwin->summaryview, (FolderSortKey)value,
			     mainwin->summaryview->sort_type);
	}
}

static void sort_summary_type_cb(GtkAction *gaction, GtkRadioAction *current, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	gint value = gtk_radio_action_get_current_value (GTK_RADIO_ACTION (current));

	if (mainwin->menu_lock_count) return;

	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (current)))
		summary_sort(mainwin->summaryview,
			     mainwin->summaryview->sort_key, (FolderSortType)value);
}

static void attract_by_subject_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_attract_by_subject(mainwin->summaryview);
}

void mainwindow_delete_duplicated(MainWindow *mainwin)
{
	FolderItem *item;

	item = folderview_get_selected_item(mainwin->folderview);
	if (item) {
		gint result;

		main_window_cursor_wait(mainwin);
		STATUSBAR_PUSH(mainwin, _("Deleting duplicated messages..."));
		result = folderutils_delete_duplicates(item, prefs_common.immediate_exec ?
					      DELETE_DUPLICATES_REMOVE : DELETE_DUPLICATES_SETFLAG);
		STATUSBAR_POP(mainwin);
		main_window_cursor_normal(mainwin);

		switch (result) {
		case -1:
			break;
		case 0:
			alertpanel_notice(_("No duplicate message found in selected folder.\n"));
			break;
		default: {
				gchar *msg;

				if (prefs_common.immediate_exec) {
					msg = ngettext("Deleted %d duplicate message in selected folder.\n",
							       "Deleted %d duplicate messages in selected folder.\n",
							   result);
				} else {
					msg = ngettext("Marked %d duplicate message for deletion in selected folder.\n",
							       "Marked %d duplicate messages for deletion in selected folder.\n",
							   result);
				}
				alertpanel_notice(msg, result);
			}
		}
	}
}

static void delete_duplicated_cb(GtkAction *action, gpointer data)
{
	mainwindow_delete_duplicated((MainWindow *)data);
}

struct DelDupsData
{
	guint	dups;
	guint	folders;
};

static void deldup_all(FolderItem *item, gpointer _data)
{
	struct DelDupsData *data = _data;
	gint result;
	
	result = folderutils_delete_duplicates(item, DELETE_DUPLICATES_REMOVE);
	if (result >= 0) {
		data->dups += result;
		data->folders += 1;
	}
}

void mainwindow_delete_duplicated_all(MainWindow *mainwin)
{
	struct DelDupsData data = {0, 0};

	main_window_cursor_wait(mainwin);
	STATUSBAR_PUSH(mainwin, _("Deleting duplicated messages in all folders..."));
	folder_func_to_all_folders(deldup_all, &data);
	STATUSBAR_POP(mainwin);
	main_window_cursor_normal(mainwin);

	if (data.dups > 0) {
		alertpanel_notice(ngettext("Deleted %d duplicate message in %d folders.\n",
				   "Deleted %d duplicate messages in %d folders.\n",
				   data.dups),
				   data.dups, data.folders);
	} else {
		alertpanel_notice(_("No duplicate message found in %d folders.\n"), data.folders);
	}
}

static void delete_duplicated_all_cb(GtkAction *action, gpointer mw)
{
	MainWindow *mainwin = (MainWindow *)mw;

	mainwindow_delete_duplicated_all(mainwin);
}

static void filter_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_filter(mainwin->summaryview, FALSE);
}

static void filter_list_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_filter(mainwin->summaryview, TRUE);
}

static void process_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	FolderItem *item = mainwin->summaryview->folder_item;	
	cm_return_if_fail(item != NULL);

	item->processing_pending = TRUE;
	folder_item_apply_processing(item);	
	item->processing_pending = FALSE;
}

static void execute_summary_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_execute(mainwin->summaryview);
}

static void expunge_summary_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_expunge(mainwin->summaryview);
}

static void update_summary_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	FolderItem *fitem;
	FolderView *folderview = mainwin->folderview;

	if (!mainwin->summaryview->folder_item) return;
	if ((fitem = folderview_get_opened_item(folderview)) == NULL) return;

	folder_update_op_count();
	folder_item_scan(fitem);
	summary_show(mainwin->summaryview, fitem, FALSE);
}

static void prev_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_select_prev(mainwin->summaryview);
}

static void next_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_select_next(mainwin->summaryview);
}

static void prev_unread_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_select_prev_unread(mainwin->summaryview);
}

static void next_unread_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_select_next_unread(mainwin->summaryview);
}

static void prev_new_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_select_prev_new(mainwin->summaryview);
}

static void next_new_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_select_next_new(mainwin->summaryview);
}

static void prev_marked_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_select_prev_marked(mainwin->summaryview);
}

static void next_marked_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_select_next_marked(mainwin->summaryview);
}

static void prev_labeled_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_select_prev_labeled(mainwin->summaryview);
}

static void next_labeled_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_select_next_labeled(mainwin->summaryview);
}

static void prev_history_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	MsgInfo *info = messageview_nav_get_prev(mainwin->messageview);
	if (info) {
		if (info->folder != mainwin->summaryview->folder_item)
			folderview_select(mainwin->folderview, info->folder);
		summary_select_by_msgnum(mainwin->summaryview, info->msgnum,
				OPEN_SELECTED_ON_PREVNEXT);
		procmsg_msginfo_free(&info);
		main_window_set_menu_sensitive(mainwindow_get_mainwindow());
		toolbar_main_set_sensitive(mainwindow_get_mainwindow());
	}
}

static void next_history_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	MsgInfo *info = messageview_nav_get_next(mainwin->messageview);
	if (info) {
		if (info->folder != mainwin->summaryview->folder_item)
			folderview_select(mainwin->folderview, info->folder);
		summary_select_by_msgnum(mainwin->summaryview, info->msgnum,
				OPEN_SELECTED_ON_PREVNEXT);
		procmsg_msginfo_free(&info);
		main_window_set_menu_sensitive(mainwindow_get_mainwindow());
		toolbar_main_set_sensitive(mainwindow_get_mainwindow());
	}
}

static void parent_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_select_parent(mainwin->summaryview);
}

static void goto_folder_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	FolderItem *to_folder;

	to_folder = foldersel_folder_sel(NULL, FOLDER_SEL_ALL, NULL, FALSE,
			_("Select folder to go to"));

	if (to_folder)
		folderview_select(mainwin->folderview, to_folder);
}

static void goto_unread_folder_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	folderview_select_next_with_flag(mainwin->folderview, MSG_UNREAD);
}

static void scroll_prev_line_cb(GtkAction *action, gpointer data)
{
        MainWindow *mainwin = (MainWindow *)data;
        mimeview_scroll_one_line(mainwin->messageview->mimeview,TRUE);
}

static void scroll_next_line_cb(GtkAction *action, gpointer data)
{
        MainWindow *mainwin = (MainWindow *)data;
        mimeview_scroll_one_line(mainwin->messageview->mimeview,FALSE);
}

static void scroll_prev_page_cb(GtkAction *action, gpointer data)
{
        MainWindow *mainwin = (MainWindow *)data;
        mimeview_scroll_page(mainwin->messageview->mimeview,TRUE);
}

static void scroll_next_page_cb(GtkAction *action, gpointer data)
{
        MainWindow *mainwin = (MainWindow *)data;
        mimeview_scroll_page(mainwin->messageview->mimeview,FALSE);
}

static void copy_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	messageview_copy_clipboard(mainwin->messageview);
}

static void allsel_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	MessageView *msgview = mainwin->messageview;

	if (messageview_is_visible(msgview) &&
		 (gtk_widget_has_focus(msgview->mimeview->textview->text)))
		messageview_select_all(mainwin->messageview);
	else
		summary_select_all(mainwin->summaryview);
}

static void select_thread_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_select_thread(mainwin->summaryview, FALSE, FALSE);
}

static void trash_thread_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_select_thread(mainwin->summaryview, FALSE, TRUE);
}

static void delete_thread_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_select_thread(mainwin->summaryview, TRUE, FALSE);
}

static void create_filter_cb(GtkAction *gaction, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	const gchar *a_name = gtk_action_get_name(gaction);
	gint action = -1;

	DO_ACTION("Tools/CreateFilterRule/Automatically", FILTER_BY_AUTO);
	DO_ACTION("Tools/CreateFilterRule/ByFrom", FILTER_BY_FROM);
	DO_ACTION("Tools/CreateFilterRule/ByTo", FILTER_BY_TO);
	DO_ACTION("Tools/CreateFilterRule/BySubject", FILTER_BY_SUBJECT);
	DO_ACTION("Tools/CreateFilterRule/BySender", FILTER_BY_SENDER);
	summary_filter_open(mainwin->summaryview, (PrefsFilterType)action, 0);
}

static void create_processing_cb(GtkAction *gaction, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	const gchar *a_name = gtk_action_get_name(gaction);
	gint action = -1;

	DO_ACTION("Tools/CreateProcessingRule/Automatically", FILTER_BY_AUTO);
	DO_ACTION("Tools/CreateProcessingRule/ByFrom", FILTER_BY_FROM);
	DO_ACTION("Tools/CreateProcessingRule/ByTo", FILTER_BY_TO);
	DO_ACTION("Tools/CreateProcessingRule/BySubject", FILTER_BY_SUBJECT);
	DO_ACTION("Tools/CreateProcessingRule/BySender", FILTER_BY_SENDER);
	summary_filter_open(mainwin->summaryview, (PrefsFilterType)action, 1);
}

static void prefs_pre_processing_open_cb(GtkAction *action, gpointer data)
{
	prefs_filtering_open(&pre_global_processing,
			     _("Processing rules to apply before folder rules"),
			     MANUAL_ANCHOR_PROCESSING,
			     NULL, NULL, FALSE);
}

static void prefs_post_processing_open_cb(GtkAction *action, gpointer data)
{
	prefs_filtering_open(&post_global_processing,
			     _("Processing rules to apply after folder rules"),
			     MANUAL_ANCHOR_PROCESSING,
			     NULL, NULL, FALSE);
}

static void prefs_filtering_open_cb(GtkAction *action, gpointer data)
{
	prefs_filtering_open(&filtering_rules,
			     _("Filtering configuration"),
			     MANUAL_ANCHOR_FILTERING,
			     NULL, NULL, TRUE);
}

static void prefs_template_open_cb(GtkAction *action, gpointer data)
{
	prefs_template_open();
}

static void prefs_actions_open_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	prefs_actions_open(mainwin);
}

static void prefs_tags_open_cb(GtkAction *action, gpointer data)
{
	tags_window_open(NULL);
}
#ifdef USE_GNUTLS
static void ssl_manager_open_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	ssl_manager_open(mainwin);
}
#endif
static void prefs_account_open_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	if (!cur_account) {
		new_account_cb(NULL, mainwin);
	} else {
		account_open(cur_account, FALSE);
	}
}

static void new_account_cb(GtkAction *action, gpointer data)
{
	account_edit_open(NULL, NULL);
	if (!compose_get_compose_list()) account_add();
}

static void account_selector_menu_cb(GtkMenuItem *menuitem, gpointer data)
{
	FolderItem *item = NULL;
	cur_account = (PrefsAccount *)data;
	
	if (!mainwindow_get_mainwindow())
		return;
	main_window_show_cur_account(mainwindow_get_mainwindow());
	toolbar_update(TOOLBAR_MAIN, mainwindow_get_mainwindow());
	main_window_set_menu_sensitive(mainwindow_get_mainwindow());
	toolbar_main_set_sensitive(mainwindow_get_mainwindow());
	item = folderview_get_selected_item(
			mainwindow_get_mainwindow()->folderview);
	if (item) {
		toolbar_set_compose_button
			(mainwindow_get_mainwindow()->toolbar,
			 FOLDER_TYPE(item->folder) == F_NEWS ? 
			 COMPOSEBUTTON_NEWS : COMPOSEBUTTON_MAIL);
	}
}

static void account_receive_menu_cb(GtkMenuItem *menuitem, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)mainwin_list->data;
	PrefsAccount *account = (PrefsAccount *)data;

	inc_account_mail(mainwin, account);
}
#ifndef GENERIC_UMPC
static void account_compose_menu_cb(GtkMenuItem *menuitem, gpointer data)
{
	PrefsAccount *account = (PrefsAccount *)data;

	compose_new_with_folderitem(account, NULL, NULL);
}
#endif
static void prefs_open_cb(GtkAction *action, gpointer data)
{
	prefs_gtk_open();
}

static void plugins_open_cb(GtkAction *action, gpointer data)
{
	pluginwindow_create();
}

static void manual_open_cb(GtkAction *action, gpointer data)
{
	manual_open(MANUAL_MANUAL_CLAWS, NULL);
}

static void manual_faq_open_cb(GtkAction *action, gpointer data)
{
	manual_open(MANUAL_FAQ_CLAWS, NULL);
}

static void legend_open_cb(GtkAction *action, gpointer data)
{
	legend_show();
}

#ifdef G_OS_WIN32
static void set_default_client_cb(GtkAction *action, gpointer data)
{
	char exename[MAX_PATH];
	gchar *binary_icon = NULL;
	gchar *binary_compose = NULL;
	gchar *binary_run = NULL;
	gboolean r;
	if ( !GetModuleFileNameA (0, exename, sizeof (exename)) ) {
		alertpanel_error(_("Can not register as default client: impossible to get executable path."));
		return;
	}
	binary_icon = g_strconcat(exename, ",0", NULL);
	binary_compose = g_strconcat(exename, " --compose %1", NULL);
	binary_run = g_strconcat(exename, NULL);

	/* Try to set the Mail Start menu item to Claws. It may fail if we're not root; we don't care */
	r = write_w32_registry_string(HKEY_LOCAL_MACHINE,
		"Software\\Clients\\Mail",
		"",
		"Claws Mail");
	
	r = write_w32_registry_string(HKEY_CURRENT_USER,
		"Software\\Clients\\Mail\\Claws Mail",
		"",
		"Claws Mail");
	if (r)
		r = write_w32_registry_string(HKEY_CURRENT_USER,
			"Software\\Clients\\Mail\\Claws Mail",
			"DLLPath",
			"");
	if (r)
		r = write_w32_registry_string(HKEY_CURRENT_USER,
			"Software\\Clients\\Mail\\Claws Mail\\Protocols\\mailto",
			"",
			"URL:MailTo-Protocol");
	if (r)
		r = write_w32_registry_string(HKEY_CURRENT_USER,
			"Software\\Clients\\Mail\\Claws Mail\\Protocols\\mailto",
			"URL Protocol",
			"");
	if (r)
		r = write_w32_registry_dword (HKEY_CURRENT_USER,
			"Software\\Clients\\Mail\\Claws Mail\\Protocols\\mailto",
			"EditFlags",
			2);
	if (r)
		r = write_w32_registry_string(HKEY_CURRENT_USER,
			"Software\\Clients\\Mail\\Claws Mail\\Protocols\\mailto",
			"FriendlyTypeName",
			"Claws-Mail URL");
	if (r)
		r = write_w32_registry_string(HKEY_CURRENT_USER,
			"Software\\Clients\\Mail\\Claws Mail\\Protocols\\mailto\\DefaultIcon",
			"",
			binary_icon);
	if (r)
		r = write_w32_registry_string(HKEY_CURRENT_USER,
			"Software\\Clients\\Mail\\Claws Mail\\Protocols\\mailto\\shell\\open\\command",
			"",
			binary_compose);
	if (r)
		r = write_w32_registry_string(HKEY_CURRENT_USER,
			"Software\\Clients\\Mail\\Claws Mail\\shell\\open\\command",
			"",
			binary_run);
	
	if (r)
		r = write_w32_registry_string(HKEY_CURRENT_USER,
			"Software\\Classes\\mailto",
			"",
			"URL:MailTo-Protocol");
	if (r)
		r = write_w32_registry_string(HKEY_CURRENT_USER,
			"Software\\Classes\\mailto",
			"URL Protocol",
			"");
	if (r)
		r = write_w32_registry_dword (HKEY_CURRENT_USER,
			"Software\\Classes\\mailto",
			"EditFlags",
			2);
	if (r)
		r = write_w32_registry_string(HKEY_CURRENT_USER,
			"Software\\Classes\\mailto",
			"FriendlyTypeName",
			"Claws-Mail URL");
	if (r)
		r = write_w32_registry_string(HKEY_CURRENT_USER,
			"Software\\Classes\\mailto\\DefaultIcon",
			"",
			binary_icon);
	if (r)
		r = write_w32_registry_string(HKEY_CURRENT_USER,
			"Software\\Classes\\mailto\\shell\\open\\command",
			"",
			binary_compose);
	
	if (r) {
		SendMessage(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)"Software\\Clients\\Mail");
		alertpanel_notice(_("Claws Mail has been registered as default client."));
	} else {
		alertpanel_error(_("Can not register as default client: impossible to write to the registry."));
	}
	g_free(binary_icon);
	g_free(binary_compose);
	g_free(binary_run);
}
#endif

static void scan_tree_func(Folder *folder, FolderItem *item, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	gchar *str;

	if (item->path)
		str = g_strdup_printf(_("Scanning folder %s%c%s..."),
				      LOCAL_FOLDER(folder)->rootpath,
				      G_DIR_SEPARATOR,
				      item->path);
	else
		str = g_strdup_printf(_("Scanning folder %s..."),
				      LOCAL_FOLDER(folder)->rootpath);

	STATUSBAR_PUSH(mainwin, str);
	STATUSBAR_POP(mainwin);
	g_free(str);
}

static gboolean mainwindow_focus_in_event(GtkWidget *widget, GdkEventFocus *focus,
					  gpointer data)
{
	SummaryView *summary;

	cm_return_val_if_fail(data, FALSE);
	if (!g_list_find(mainwin_list, data))
		return TRUE;
	summary = ((MainWindow *)data)->summaryview;
	cm_return_val_if_fail(summary, FALSE);

	if (GTK_CMCLIST(summary->ctree)->selection && 
	    g_list_length(GTK_CMCLIST(summary->ctree)->selection) > 1)
		return FALSE;

	return FALSE;
}

static gboolean mainwindow_visibility_event_cb(GtkWidget *widget, GdkEventVisibility *event,
					  gpointer data)
{
	is_obscured = (event->state == GDK_VISIBILITY_FULLY_OBSCURED);
	return FALSE;
}

static gboolean mainwindow_state_event_cb(GtkWidget *widget, GdkEventWindowState *state,
					  gpointer data)
{
	if (!claws_is_starting()
		&& state->changed_mask&GDK_WINDOW_STATE_ICONIFIED
		&& state->new_window_state&GDK_WINDOW_STATE_ICONIFIED) {

		if (iconified_count > 0)
			hooks_invoke(MAIN_WINDOW_GOT_ICONIFIED, NULL);
		iconified_count++;
	} else if (!claws_is_starting()) {
		prefs_common.mainwin_maximised = 
			((state->new_window_state&GDK_WINDOW_STATE_MAXIMIZED) != 0);
	}
	if (state->new_window_state == 0)
		gtk_window_set_skip_taskbar_hint(GTK_WINDOW(widget), FALSE);
	return FALSE;
}

gboolean mainwindow_is_obscured(void)
{
	return is_obscured;
}

/*
 * Harvest addresses for selected folder.
 */
static void addr_harvest_cb( GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	addressbook_harvest( mainwin->summaryview->folder_item, FALSE, NULL );
}

/*
 * Harvest addresses for selected messages in summary view.
 */
static void addr_harvest_msg_cb( GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	summary_harvest_address( mainwin->summaryview );
}

/*!
 *\brief	get a MainWindow
 *
 *\return	MainWindow * The first mainwindow in the mainwin_list
 */
MainWindow *mainwindow_get_mainwindow(void)
{
	if (mainwin_list && mainwin_list->data)
		return (MainWindow *)(mainwin_list->data);
	else
		return NULL;
}

static gboolean mainwindow_progressindicator_hook(gpointer source, gpointer userdata)
{
	ProgressData *data = (ProgressData *) source;
	MainWindow *mainwin = (MainWindow *) userdata;

	switch (data->cmd) {
	case PROGRESS_COMMAND_START:
	case PROGRESS_COMMAND_STOP:
		gtk_progress_bar_set_fraction
			(GTK_PROGRESS_BAR(mainwin->progressbar), 0.0);
		break;
	case PROGRESS_COMMAND_SET_PERCENTAGE:
		gtk_progress_bar_set_fraction
			(GTK_PROGRESS_BAR(mainwin->progressbar), data->value);
		break;		
	}
	while (gtk_events_pending()) gtk_main_iteration ();

	return FALSE;
}

static void sync_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	mainwindow_check_synchronise(mainwin, FALSE);
}

static void forget_session_passwords_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	GList *list = NULL;
        gint fgtn = 0;
	gint accs = 0;

	main_window_lock(mainwin);
	for (list = account_get_list(); list != NULL; list = list->next) {
		PrefsAccount *account = list->data;
		if (account->session_passwd) {
			g_free(account->session_passwd);
			account->session_passwd = NULL;
			++fgtn;
		}
		if (account->session_smtp_passwd) {
			g_free(account->session_smtp_passwd);
			account->session_smtp_passwd = NULL;
			++fgtn;
		}
		++accs;
	}
	main_window_unlock(mainwin);
	alertpanel_notice(ngettext("Forgotten %d password in %d accounts.\n",
				   "Forgotten %d passwords in %d accounts.\n",
				   fgtn), fgtn, accs);	
}

#ifndef PASSWORD_CRYPTO_OLD
static void forget_primary_passphrase_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	main_window_lock(mainwin);
	primary_passphrase_forget();
	main_window_unlock(mainwin);
}
#endif

void mainwindow_learn (MainWindow *mainwin, gboolean is_spam)
{
	summary_mark_as_spam(mainwin->summaryview, is_spam, NULL);
}

void mainwindow_jump_to(const gchar *target, gboolean popup)
{
	gchar *tmp = NULL;
	FolderItem *item = NULL;
	gchar *msg = NULL;
	MainWindow *mainwin = mainwindow_get_mainwindow();
	gchar *from_uri = NULL;
	if (!target)
		return;
		
	if (!mainwin) {
		g_print("not initialized\n");
		return;
	}

	if ((from_uri = g_filename_from_uri(target, NULL, NULL)) != NULL)
		tmp = from_uri;
	else
		tmp = g_strdup(target);

	strcrlftrunc(tmp);

	if ((item = folder_find_item_from_identifier(tmp))) {
		g_print("selecting folder '%s'\n", tmp);
		folderview_select(mainwin->folderview, item);
		if (popup)
			main_window_popup(mainwin);
		g_free(tmp);
		return;
	}
	
	msg = strrchr(tmp, '/');
	if (msg) {
		*msg++ = '\0';
		if ((item = folder_find_item_from_identifier(tmp))) {
			g_print("selecting folder '%s'\n", tmp);
			folderview_select(mainwin->folderview, item);
		} else if ((item = folder_find_item_from_real_path(tmp))) {
			g_print("selecting folder '%s'\n", tmp);
			folderview_select(mainwin->folderview, item);
		} else {
			g_print("'%s' not found\n", tmp);
		}
		if (item && msg && atoi(msg)) {
			g_print("selecting message %d\n", atoi(msg));
			summary_select_by_msgnum(mainwin->summaryview, atoi(msg), TRUE);
			if (popup)
				main_window_popup(mainwin);
			g_free(tmp);
			return;
		} else if (item && msg[0] == '<' && msg[strlen(msg)-1] == '>') {
			MsgInfo *msginfo = NULL;
			msg++;
			msg[strlen(msg)-1] = '\0';
			msginfo = folder_item_get_msginfo_by_msgid(item, msg);
			if (msginfo) {
				g_print("selecting message %s\n", msg);
				summary_select_by_msgnum(mainwin->summaryview, msginfo->msgnum, TRUE);
				if (popup)
					main_window_popup(mainwin);
				g_free(tmp);
				procmsg_msginfo_free(&msginfo);
				return;
			} else {
				g_print("'%s' not found\n", msg);
			}
		} else {
			g_print("'%s' not found\n", msg);
		}
	} else {
		g_print("'%s' not found\n", tmp);
	}
	
	g_free(tmp);
}

void mainwindow_exit_folder(MainWindow *mainwin) {
	if (prefs_common.layout_mode == SMALL_LAYOUT) {
		folderview_close_opened(mainwin->folderview, FALSE);
		mainwin_paned_show_first(GTK_PANED(mainwin->hpaned));
		folderview_grab_focus(mainwin->folderview);
	}
	mainwin->in_folder = FALSE;
	main_window_set_menu_sensitive(mainwin);
}

void mainwindow_enter_folder(MainWindow *mainwin) {
	if (prefs_common.layout_mode == SMALL_LAYOUT) {
		mainwin_paned_show_last(GTK_PANED(mainwin->hpaned));
	}
	mainwin->in_folder = TRUE;
	main_window_set_menu_sensitive(mainwin);
}

static void save_part_as_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	if (mainwin->messageview 
	&&  mainwin->messageview->mimeview)
		mimeview_save_as(mainwin->messageview->mimeview);
}

static void view_part_as_text_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	if (mainwin->messageview 
	&&  mainwin->messageview->mimeview)
		mimeview_display_as_text(mainwin->messageview->mimeview);
}

static void open_part_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	if (mainwin->messageview 
	&&  mainwin->messageview->mimeview)
		mimeview_launch(mainwin->messageview->mimeview, NULL);
}
#ifndef G_OS_WIN32
static void open_part_with_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	if (mainwin->messageview 
	&&  mainwin->messageview->mimeview)
		mimeview_open_with(mainwin->messageview->mimeview);
}
#endif
static void check_signature_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	if (mainwin->messageview 
	&&  mainwin->messageview->mimeview)
		mimeview_check_signature(mainwin->messageview->mimeview);
}

static void goto_next_part_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	if (mainwin->messageview 
	&&  mainwin->messageview->mimeview)
		mimeview_select_next_part(mainwin->messageview->mimeview);
}

static void goto_prev_part_cb(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	if (mainwin->messageview 
	&&  mainwin->messageview->mimeview)
		mimeview_select_prev_part(mainwin->messageview->mimeview);
}

void mainwindow_import_mbox(const gchar* mbox_file)
{
	MainWindow *mainwin = mainwindow_get_mainwindow();

	strcrlftrunc((gchar *) mbox_file);
	/* only notify if import has failed */
	if (import_mbox(mainwin->summaryview->folder_item, mbox_file) == -1) {
		alertpanel_error(_("Mbox import has failed."));
	}
}
