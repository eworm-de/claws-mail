/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2002 Hiroyuki Yamamoto
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "defs.h"

#include <glib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkcontainer.h>
#include <gtk/gtkstatusbar.h>
#include <gtk/gtkprogressbar.h>
#include <gtk/gtkhpaned.h>
#include <gtk/gtkvpaned.h>
#include <gtk/gtkcheckmenuitem.h>
#include <gtk/gtkitemfactory.h>
#include <gtk/gtkeditable.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkhandlebox.h>
#include <gtk/gtktoolbar.h>
#include <gtk/gtkbutton.h>
#include <string.h>

#include "intl.h"
#include "main.h"
#include "mainwindow.h"
#include "folderview.h"
#include "foldersel.h"
#include "summaryview.h"
#include "summary_search.h"
#include "messageview.h"
#include "message_search.h"
#include "headerview.h"
#include "menu.h"
#include "stock_pixmap.h"
#include "folder.h"
#include "inc.h"
#include "compose.h"
#include "procmsg.h"
#include "import.h"
#include "export.h"
#include "prefs_common.h"
#include "prefs_filter.h"
#include "prefs_actions.h"
#include "prefs_filtering.h"
#include "prefs_scoring.h"
#include "prefs_account.h"
#include "prefs_folder_item.h"
#include "prefs_summary_column.h"
#include "prefs_template.h"
#include "account.h"
#include "addressbook.h"
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
#include "selective_download.h"

#define AC_LABEL_WIDTH	240

#define STATUSBAR_PUSH(mainwin, str) \
{ \
	gtk_statusbar_push(GTK_STATUSBAR(mainwin->statusbar), \
			   mainwin->mainwin_cid, str); \
	gtkut_widget_wait_for_draw(mainwin->hbox_stat); \
}

#define STATUSBAR_POP(mainwin) \
{ \
	gtk_statusbar_pop(GTK_STATUSBAR(mainwin->statusbar), \
			  mainwin->mainwin_cid); \
}

/* list of all instantiated MainWindow */
static GList *mainwin_list = NULL;

static GdkCursor *watch_cursor;

static void main_window_menu_callback_block	(MainWindow	*mainwin);
static void main_window_menu_callback_unblock	(MainWindow	*mainwin);

static void main_window_show_cur_account	(MainWindow	*mainwin);

static void main_window_set_widgets		(MainWindow	*mainwin,
						 SeparateType	 type);
static void main_window_toolbar_create		(MainWindow	*mainwin,
						 GtkWidget	*container);

/* callback functions */
static void toolbar_inc_cb		(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_inc_all_cb		(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_send_cb		(GtkWidget	*widget,
					 gpointer	 data);

static void toolbar_compose_cb		(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_compose_news_cb	(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_compose_mail_cb	(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_reply_cb		(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_reply_popup_cb	(GtkWidget	*widget,
					 GdkEventButton *event,
					 gpointer	 data);
static void toolbar_reply_popup_closed_cb(GtkMenuShell	*menu_shell,
					 gpointer	 data);
static void toolbar_reply_to_all_cb	(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_reply_to_all_popup_cb(GtkWidget	*widget,
					 GdkEventButton *event,
					 gpointer	 data);
static void toolbar_reply_to_all_popup_closed_cb(GtkMenuShell	*menu_shell,
					 gpointer	 data);
static void toolbar_reply_to_sender_cb	(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_reply_to_sender_popup_cb(GtkWidget	*widget,
					 GdkEventButton *event,
					 gpointer	 data);
static void toolbar_reply_to_sender_popup_closed_cb(GtkMenuShell	*menu_shell,
					 gpointer	 data);
static void toolbar_forward_cb		(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_forward_popup_cb	(GtkWidget	*widget,
					 GdkEventButton *event,
					 gpointer	 data);
static void toolbar_forward_popup_closed_cb(GtkMenuShell	*menu_shell,
					 gpointer	 data);

static void toolbar_delete_cb		(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_exec_cb		(GtkWidget	*widget,
					 gpointer	 data);

static void toolbar_next_unread_cb	(GtkWidget	*widget,
					 gpointer	 data);

static void toolbar_prefs_cb		(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_account_cb		(GtkWidget	*widget,
					 gpointer	 data);

#if 0
static void toolbar_account_button_pressed	(GtkWidget	*widget,
						 GdkEventButton	*event,
						 gpointer	 data);
#endif
static void ac_label_button_pressed		(GtkWidget	*widget,
						 GdkEventButton	*event,
						 gpointer	 data);
static void ac_menu_popup_closed		(GtkMenuShell	*menu_shell,
						 gpointer	 data);

static gint main_window_close_cb	(GtkWidget	*widget,
					 GdkEventAny	*event,
					 gpointer	 data);
static gint folder_window_close_cb	(GtkWidget	*widget,
					 GdkEventAny	*event,
					 gpointer	 data);
static gint message_window_close_cb	(GtkWidget	*widget,
					 GdkEventAny	*event,
					 gpointer	 data);

static void add_mailbox_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void add_mbox_cb 	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void update_folderview_cb (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void new_folder_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void rename_folder_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void delete_folder_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void import_mbox_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void export_mbox_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void empty_trash_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);

static void save_as_cb		 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void print_cb		 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void app_exit_cb		 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);

static void search_cb		 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);

static void toggle_folder_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void toggle_message_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void toggle_toolbar_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void toggle_statusbar_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void separate_widget_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);

static void addressbook_open_cb	(MainWindow	*mainwin,
				 guint		 action,
				 GtkWidget	*widget);
static void log_window_show_cb	(MainWindow	*mainwin,
				 guint		 action,
				 GtkWidget	*widget);
static void sel_download_cb     (MainWindow *mainwin, 
				 guint action,
				 GtkWidget *widget);

static void inc_mail_cb			(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);
static void inc_all_account_mail_cb	(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);
static void inc_cancel_cb		(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);

static void send_queue_cb		(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);

static void compose_cb			(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);
static void compose_mail_cb(MainWindow *mainwin, guint action,
			    GtkWidget *widget);
static void compose_news_cb(MainWindow *mainwin, guint action,
			    GtkWidget *widget);
static void reply_cb			(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);

static void open_msg_cb			(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);

static void view_source_cb		(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);

static void show_all_header_cb		(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);

static void reedit_cb			(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);

static void move_to_cb			(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);
static void copy_to_cb			(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);
static void delete_cb			(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);

static void mark_cb			(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);
static void unmark_cb			(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);

static void mark_as_unread_cb		(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);
static void mark_as_read_cb		(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);
static void mark_all_read_cb		(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);
static void add_address_cb		(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);

static void set_charset_cb		(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);

static void hide_read_messages   (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);

static void thread_cb		 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void expand_threads_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void collapse_threads_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);

static void set_display_item_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void sort_summary_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void sort_summary_type_cb (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void attract_by_subject_cb(MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);

static void delete_duplicated_cb (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void filter_cb		 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void execute_summary_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void update_summary_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);

static void prev_cb		 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void next_cb		 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);

static void prev_unread_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void next_unread_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void prev_new_cb		 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void next_new_cb		 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void prev_marked_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void next_marked_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void prev_labeled_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void next_labeled_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);

static void goto_folder_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);

static void copy_cb		 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void allsel_cb		 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void selthread_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void create_filter_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);

static void prefs_common_open_cb (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void prefs_filter_open_cb (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void prefs_scoring_open_cb (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void prefs_filtering_open_cb (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void prefs_account_open_cb(MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void prefs_template_open_cb	(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);
static void prefs_actions_open_cb	(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);

static void new_account_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);

static void account_menu_cb	 (GtkMenuItem	*menuitem,
				  gpointer	 data);

static void online_switch_clicked(GtkButton     *btn, 
				  gpointer data);

static void manual_open_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void faq_open_cb		 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);

static void scan_tree_func	 (Folder	*folder,
				  FolderItem	*item,
				  gpointer	 data);
				  
static void activate_compose_button (MainWindow *mainwin,
				ToolbarStyle      style,
				ComposeButtonType type);

static void key_pressed (GtkWidget *widget, 
				GdkEventKey *event,
				gpointer data);

static void set_toolbar_style(MainWindow *mainwin);

static void toggle_work_offline_cb(MainWindow *mainwin, guint action, GtkWidget *widget);

static void addr_harvest_cb	 ( MainWindow  *mainwin,
				   guint       action,
				   GtkWidget   *widget );

static void addr_harvest_msg_cb	 ( MainWindow  *mainwin,
				   guint       action,
				   GtkWidget   *widget );

#define  SEPARATE_ACTION 500 

static GtkItemFactoryEntry mainwin_entries[] =
{
	{N_("/_File"),				NULL, NULL, 0, "<Branch>"},
	{N_("/_File/_Add mailbox..."),		NULL, add_mailbox_cb, 0, NULL},
	{N_("/_File/_Add mbox mailbox..."),     NULL, add_mbox_cb, 0, NULL},
	{N_("/_File/_Rescan folder tree"),	NULL, update_folderview_cb, 0, NULL},
	{N_("/_File/_Folder"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_File/_Folder/Create _new folder..."),
						NULL, new_folder_cb, 0, NULL},
	{N_("/_File/_Folder/_Rename folder..."),NULL, rename_folder_cb, 0, NULL},
	{N_("/_File/_Folder/_Delete folder"),	NULL, delete_folder_cb, 0, NULL},
	{N_("/_File/_Import mbox file..."),	NULL, import_mbox_cb, 0, NULL},
	{N_("/_File/_Export to mbox file..."),	NULL, export_mbox_cb, 0, NULL},
	{N_("/_File/Empty _trash"),		"<shift>D", empty_trash_cb, 0, NULL},
	{N_("/_File/_Work offline"),		"<control>W", toggle_work_offline_cb, 0, "<ToggleItem>"},						
	{N_("/_File/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_File/_Save as..."),		"<control>S", save_as_cb, 0, NULL},
	{N_("/_File/_Print..."),		NULL, print_cb, 0, NULL},
	{N_("/_File/---"),			NULL, NULL, 0, "<Separator>"},
	/* {N_("/_File/_Close"),		"<alt>W", app_exit_cb, 0, NULL}, */
	{N_("/_File/E_xit"),			"<control>Q", app_exit_cb, 0, NULL},

	{N_("/_Edit"),				NULL, NULL, 0, "<Branch>"},
	{N_("/_Edit/_Copy"),			"<control>C", copy_cb, 0, NULL},
	{N_("/_Edit/Select _all"),		"<control>A", allsel_cb, 0, NULL},
	{N_("/_Edit/Select thread"),		"<control>Z", selthread_cb, 0, NULL},
	{N_("/_Edit/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Edit/_Find in current message..."),
						"<control>F", search_cb, 0, NULL},
	{N_("/_Edit/_Search folder..."),	"<shift><control>F", search_cb, 1, NULL},
	{N_("/_View"),				NULL, NULL, 0, "<Branch>"},
	{N_("/_View/Show or hi_de"),		NULL, NULL, 0, "<Branch>"},
	{N_("/_View/Show or hi_de/_Folder tree"),
						NULL, toggle_folder_cb, 0, "<ToggleItem>"},
	{N_("/_View/Show or hi_de/_Message view"),
						"V", toggle_message_cb, 0, "<ToggleItem>"},
	{N_("/_View/Show or hi_de/_Toolbar"),
						NULL, NULL, 0, "<Branch>"},
	{N_("/_View/Show or hi_de/_Toolbar/Icon _and text"),
						NULL, toggle_toolbar_cb, TOOLBAR_BOTH, "<RadioItem>"},
	{N_("/_View/Show or hi_de/_Toolbar/_Icon"),
						NULL, toggle_toolbar_cb, TOOLBAR_ICON, "/View/Show or hide/Toolbar/Icon and text"},
	{N_("/_View/Show or hi_de/_Toolbar/_Text"),
						NULL, toggle_toolbar_cb, TOOLBAR_TEXT, "/View/Show or hide/Toolbar/Icon and text"},
	{N_("/_View/Show or hi_de/_Toolbar/_None"),
						NULL, toggle_toolbar_cb, TOOLBAR_NONE, "/View/Show or hide/Toolbar/Icon and text"},
	{N_("/_View/Show or hi_de/Status _bar"),
						NULL, toggle_statusbar_cb, 0, "<ToggleItem>"},
	{N_("/_View/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_View/Separate f_older tree"),	NULL, separate_widget_cb, SEPARATE_FOLDER, "<ToggleItem>"},
	{N_("/_View/Separate m_essage view"),	NULL, separate_widget_cb, SEPARATE_MESSAGE, "<ToggleItem>"},
	{N_("/_View/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_View/_Sort"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_View/_Sort/by _number"),		NULL, sort_summary_cb, SORT_BY_NUMBER, "<RadioItem>"},
	{N_("/_View/_Sort/by s_ize"),		NULL, sort_summary_cb, SORT_BY_SIZE, "/View/Sort/by number"},
	{N_("/_View/_Sort/by _date"),		NULL, sort_summary_cb, SORT_BY_DATE, "/View/Sort/by number"},
	{N_("/_View/_Sort/by _from"),		NULL, sort_summary_cb, SORT_BY_FROM, "/View/Sort/by number"},
	{N_("/_View/_Sort/by _subject"),	NULL, sort_summary_cb, SORT_BY_SUBJECT, "/View/Sort/by number"},
	{N_("/_View/_Sort/by _color label"),
						NULL, sort_summary_cb, SORT_BY_LABEL, "/View/Sort/by number"},
	{N_("/_View/_Sort/by _mark"),		NULL, sort_summary_cb, SORT_BY_MARK, "/View/Sort/by number"},
	{N_("/_View/_Sort/by _unread"),		NULL, sort_summary_cb, SORT_BY_UNREAD, "/View/Sort/by number"},
	{N_("/_View/_Sort/by a_ttachment"),
						NULL, sort_summary_cb, SORT_BY_MIME, "/View/Sort/by number"},
	{N_("/_View/_Sort/by score"),		NULL, sort_summary_cb, SORT_BY_SCORE, "/View/Sort/by number"},
	{N_("/_View/_Sort/by locked"),		NULL, sort_summary_cb, SORT_BY_LOCKED, "/View/Sort/by number"},
	{N_("/_View/_Sort/D_on't sort"),	NULL, sort_summary_cb, SORT_BY_NONE, "/View/Sort/by number"},
	{N_("/_View/_Sort/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_View/_Sort/Ascending"),		NULL, sort_summary_type_cb, SORT_ASCENDING, "<RadioItem>"},
	{N_("/_View/_Sort/Descending"),		NULL, sort_summary_type_cb, SORT_DESCENDING, "/View/Sort/Ascending"},
	{N_("/_View/_Sort/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_View/_Sort/_Attract by subject"),
						NULL, attract_by_subject_cb, 0, NULL},
	{N_("/_View/Th_read view"),		"<control>T", thread_cb, 0, "<ToggleItem>"},
	{N_("/_View/E_xpand all threads"),	NULL, expand_threads_cb, 0, NULL},
	{N_("/_View/Co_llapse all threads"),	NULL, collapse_threads_cb, 0, NULL},
	{N_("/_View/_Hide read messages"),	NULL, hide_read_messages, 0, "<ToggleItem>"},
	{N_("/_View/Set display _item..."),	NULL, set_display_item_cb, 0, NULL},

	{N_("/_View/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_View/_Go to"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_View/_Go to/_Prev message"),	"P", prev_cb, 0, NULL},
	{N_("/_View/_Go to/_Next message"),	"N", next_cb, 0, NULL},
	{N_("/_View/_Go to/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_View/_Go to/P_rev unread message"),
						"<shift>P", prev_unread_cb, 0, NULL},
	{N_("/_View/_Go to/N_ext unread message"),
						"<shift>N", next_unread_cb, 0, NULL},
	{N_("/_View/_Go to/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_View/_Go to/Prev ne_w message"),	NULL, prev_new_cb, 0, NULL},
	{N_("/_View/_Go to/Ne_xt new message"),	NULL, next_new_cb, 0, NULL},
	{N_("/_View/_Go to/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_View/_Go to/Prev _marked message"),
						NULL, prev_marked_cb, 0, NULL},
	{N_("/_View/_Go to/Next m_arked message"),
						NULL, next_marked_cb, 0, NULL},
	{N_("/_View/_Go to/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_View/_Go to/Prev _labeled message"),
						NULL, prev_labeled_cb, 0, NULL},
	{N_("/_View/_Go to/Next la_beled message"),
						NULL, next_labeled_cb, 0, NULL},
	{N_("/_View/_Go to/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_View/_Go to/Other _folder..."),	"G", goto_folder_cb, 0, NULL},
	{N_("/_View/---"),			NULL, NULL, 0, "<Separator>"},

#define CODESET_SEPARATOR \
	{N_("/_View/_Code set/---"),		NULL, NULL, 0, "<Separator>"}
#define CODESET_ACTION(action) \
	 NULL, set_charset_cb, action, "/View/Code set/Auto detect"

	{N_("/_View/_Code set"),		NULL, NULL, 0, "<Branch>"},
	{N_("/_View/_Code set/_Auto detect"),
	 NULL, set_charset_cb, C_AUTO, "<RadioItem>"},
	{N_("/_View/_Code set/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_View/_Code set/7bit ascii (US-ASC_II)"),
	 CODESET_ACTION(C_US_ASCII)},

#if HAVE_LIBJCONV
	{N_("/_View/_Code set/Unicode (_UTF-8)"),
	 CODESET_ACTION(C_UTF_8)},
	CODESET_SEPARATOR,
#endif
	{N_("/_View/_Code set/Western European (ISO-8859-_1)"),
	 CODESET_ACTION(C_ISO_8859_1)},
	{N_("/_View/_Code set/Western European (ISO-8859-15)"),
	 CODESET_ACTION(C_ISO_8859_15)},
	CODESET_SEPARATOR,
#if HAVE_LIBJCONV
	{N_("/_View/_Code set/Central European (ISO-8859-_2)"),
	 CODESET_ACTION(C_ISO_8859_2)},
	CODESET_SEPARATOR,
	{N_("/_View/_Code set/_Baltic (ISO-8859-13)"),
	 CODESET_ACTION(C_ISO_8859_13)},
	{N_("/_View/_Code set/Baltic (ISO-8859-_4)"),
	 CODESET_ACTION(C_ISO_8859_4)},
	CODESET_SEPARATOR,
	{N_("/_View/_Code set/Greek (ISO-8859-_7)"),
	 CODESET_ACTION(C_ISO_8859_7)},
	CODESET_SEPARATOR,
	{N_("/_View/_Code set/Turkish (ISO-8859-_9)"),
	 CODESET_ACTION(C_ISO_8859_9)},
	CODESET_SEPARATOR,
	{N_("/_View/_Code set/Cyrillic (ISO-8859-_5)"),
	 CODESET_ACTION(C_ISO_8859_5)},
	{N_("/_View/_Code set/Cyrillic (KOI8-_R)"),
	 CODESET_ACTION(C_KOI8_R)},
	{N_("/_View/_Code set/Cyrillic (Windows-1251)"),
	 CODESET_ACTION(C_WINDOWS_1251)},
	CODESET_SEPARATOR,
#endif
	{N_("/_View/_Code set/Japanese (ISO-2022-_JP)"),
	 CODESET_ACTION(C_ISO_2022_JP)},
#if HAVE_LIBJCONV
	{N_("/_View/_Code set/Japanese (ISO-2022-JP-2)"),
	 CODESET_ACTION(C_ISO_2022_JP_2)},
#endif
	{N_("/_View/_Code set/Japanese (_EUC-JP)"),
	 CODESET_ACTION(C_EUC_JP)},
	{N_("/_View/_Code set/Japanese (_Shift__JIS)"),
	 CODESET_ACTION(C_SHIFT_JIS)},
#if HAVE_LIBJCONV
	CODESET_SEPARATOR,
	{N_("/_View/_Code set/Simplified Chinese (_GB2312)"),
	 CODESET_ACTION(C_GB2312)},
	{N_("/_View/_Code set/Traditional Chinese (_Big5)"),
	 CODESET_ACTION(C_BIG5)},
	{N_("/_View/_Code set/Traditional Chinese (EUC-_TW)"),
	 CODESET_ACTION(C_EUC_TW)},
	{N_("/_View/_Code set/Chinese (ISO-2022-_CN)"),
	 CODESET_ACTION(C_ISO_2022_CN)},
	CODESET_SEPARATOR,
	{N_("/_View/_Code set/Korean (EUC-_KR)"),
	 CODESET_ACTION(C_EUC_KR)},
	{N_("/_View/_Code set/Korean (ISO-2022-KR)"),
	 CODESET_ACTION(C_ISO_2022_KR)},
	CODESET_SEPARATOR,
	{N_("/_View/_Code set/Thai (TIS-620)"),
	 CODESET_ACTION(C_TIS_620)},
	{N_("/_View/_Code set/Thai (Windows-874)"),
	 CODESET_ACTION(C_WINDOWS_874)},
#endif

#undef CODESET_SEPARATOR
#undef CODESET_ACTION

	{N_("/_View/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_View/Open in new _window"),	"<control><alt>N", open_msg_cb, 0, NULL},
	{N_("/_View/Mess_age source"),		"<control>U", view_source_cb, 0, NULL},
	{N_("/_View/Show all _header"),		"<control>H", show_all_header_cb, 0, "<ToggleItem>"},
	{N_("/_View/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_View/_Update summary"),		"<control><alt>U", update_summary_cb,  0, NULL},

	{N_("/_Message"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_Message/Get new ma_il"),		"<control>I",	inc_mail_cb, 0, NULL},
	{N_("/_Message/Get from _all accounts"),
						"<shift><control>I", inc_all_account_mail_cb, 0, NULL},
	{N_("/_Message/Cancel receivin_g"),	NULL, inc_cancel_cb, 0, NULL},
	{N_("/_Message/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/_Send queued messages"), NULL, send_queue_cb, 0, NULL},
	{N_("/_Message/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/Compose a_n email message"),	"<control>M", compose_mail_cb, 0, NULL},
	{N_("/_Message/Compose a news message"),	NULL,	compose_news_cb, 0, NULL},
	{N_("/_Message/_Reply"),		"<control>R", 	reply_cb, COMPOSE_REPLY, NULL},
	{N_("/_Message/Repl_y to sender"),	"<control><alt>R", reply_cb, COMPOSE_REPLY_TO_SENDER, NULL},
	{N_("/_Message/Follow-up and reply to"), NULL, reply_cb, COMPOSE_FOLLOWUP_AND_REPLY_TO, NULL},
	{N_("/_Message/Reply to a_ll"),		"<shift><control>R", reply_cb, COMPOSE_REPLY_TO_ALL, NULL},
	{N_("/_Message/_Forward"),		"<control><alt>F", reply_cb, COMPOSE_FORWARD, NULL},
	{N_("/_Message/Redirect"),		NULL, reply_cb, COMPOSE_REDIRECT, NULL},
	{N_("/_Message/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/Re-_edit"),		NULL, reedit_cb, 0, NULL},
	{N_("/_Message/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/M_ove..."),		"<control>O", move_to_cb, 0, NULL},
	{N_("/_Message/_Copy..."),		"<shift><control>O", copy_to_cb, 0, NULL},
	{N_("/_Message/_Delete"),		"<control>D", delete_cb,  0, NULL},
	{N_("/_Message/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/_Mark"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_Message/_Mark/_Mark"),		"<shift>asterisk", mark_cb, 0, NULL},
	{N_("/_Message/_Mark/_Unmark"),		"U", unmark_cb, 0, NULL},
	{N_("/_Message/_Mark/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/_Mark/Mark as unr_ead"),	"<shift>exclam", mark_as_unread_cb, 0, NULL},
	{N_("/_Message/_Mark/Mark as rea_d"),
						NULL, mark_as_read_cb, 0, NULL},
	{N_("/_Message/_Mark/Mark all _read"),	NULL, mark_all_read_cb, 0, NULL},

	{N_("/_Tools"),				NULL, NULL, 0, "<Branch>"},
	{N_("/_Tools/_Selective download..."),	"<alt>S", sel_download_cb, 0, NULL},
	{N_("/_Tools/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Tools/_Address book..."),	"<shift><control>A", addressbook_open_cb, 0, NULL},
	{N_("/_Tools/Add sender to address boo_k"),
						NULL, add_address_cb, 0, NULL},
	{N_("/_Tools/_Harvest addresses"),	NULL, NULL, 0, "<Branch>"},
	{N_("/_Tools/_Harvest addresses/from _Folder..."),
						NULL, addr_harvest_cb, 0, NULL},
	{N_("/_Tools/_Harvest addresses/from _Messages..."),
						NULL, addr_harvest_msg_cb, 0, NULL},
	{N_("/_Tools/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Tools/_Filter messages"),		NULL, filter_cb, 0, NULL},
	{N_("/_Tools/_Create filter rule"),	NULL, NULL, 0, "<Branch>"},
	{N_("/_Tools/_Create filter rule/_Automatically"),
						NULL, create_filter_cb, FILTER_BY_AUTO, NULL},
	{N_("/_Tools/_Create filter rule/by _From"),
						NULL, create_filter_cb, FILTER_BY_FROM, NULL},
	{N_("/_Tools/_Create filter rule/by _To"),
						NULL, create_filter_cb, FILTER_BY_TO, NULL},
	{N_("/_Tools/_Create filter rule/by _Subject"),
						NULL, create_filter_cb, FILTER_BY_SUBJECT, NULL},
	{N_("/_Tools/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Tools/Actio_ns"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_Tools/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Tools/E_xecute"),			"X", execute_summary_cb, 0, NULL},
	{N_("/_Tools/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Tools/Delete du_plicated messages"),
						NULL, delete_duplicated_cb,   0, NULL},
	{N_("/_Tools/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Tools/_Log window"),		"<control>L", log_window_show_cb, 0, NULL},

	{N_("/_Configuration"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_Configuration/_Common preferences..."),
						NULL, prefs_common_open_cb, 0, NULL},
	{N_("/_Configuration/_Filter setting..."),
						NULL, prefs_filter_open_cb, 0, NULL},
	{N_("/_Configuration/_Scoring..."),
						NULL, prefs_scoring_open_cb, 0, NULL},
	{N_("/_Configuration/_Filtering..."),
						NULL, prefs_filtering_open_cb, 0, NULL},
	{N_("/_Configuration/_Template..."),	NULL, prefs_template_open_cb, 0, NULL},
	{N_("/_Configuration/_Actions..."),	NULL, prefs_actions_open_cb, 0, NULL},
	{N_("/_Configuration/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Configuration/_Preferences for current account..."),
						NULL, prefs_account_open_cb, 0, NULL},
	{N_("/_Configuration/Create _new account..."),
						NULL, new_account_cb, 0, NULL},
	{N_("/_Configuration/_Edit accounts..."),
						NULL, account_edit_open, 0, NULL},
	{N_("/_Configuration/C_hange current account"),
						NULL, NULL, 0, "<Branch>"},

	{N_("/_Help"),				NULL, NULL, 0, "<Branch>"},
	{N_("/_Help/_Manual"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_Help/_Manual/_English"),		NULL, manual_open_cb, MANUAL_LANG_EN, NULL},
	{N_("/_Help/_Manual/_German"),		NULL, manual_open_cb, MANUAL_LANG_DE, NULL},
	{N_("/_Help/_Manual/_Spanish"),		NULL, manual_open_cb, MANUAL_LANG_ES, NULL},
	{N_("/_Help/_Manual/_French"),		NULL, manual_open_cb, MANUAL_LANG_FR, NULL},
	{N_("/_Help/_Manual/_Japanese"),	NULL, manual_open_cb, MANUAL_LANG_JA, NULL},
	{N_("/_Help/_FAQ"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_Help/_FAQ/_English"),		NULL, faq_open_cb, MANUAL_LANG_EN, NULL},
	{N_("/_Help/_FAQ/_German"),		NULL, faq_open_cb, MANUAL_LANG_DE, NULL},
	{N_("/_Help/_FAQ/_Spanish"),		NULL, faq_open_cb, MANUAL_LANG_ES, NULL},
	{N_("/_Help/_FAQ/_French"),		NULL, faq_open_cb, MANUAL_LANG_FR, NULL},
	{N_("/_Help/_FAQ/_Italian"),		NULL, faq_open_cb, MANUAL_LANG_IT, NULL},
	{N_("/_Help/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Help/_About"),			NULL, about_show, 0, NULL}
};
static GtkItemFactoryEntry reply_popup_entries[] =
{
	{N_("/Reply with _quote"), NULL, reply_cb, COMPOSE_REPLY_WITH_QUOTE, NULL},
	{N_("/_Reply without quote"), NULL, reply_cb, COMPOSE_REPLY_WITHOUT_QUOTE, NULL}
};
static GtkItemFactoryEntry replyall_popup_entries[] =
{
	{N_("/Reply to all with _quote"), "<shift>A", reply_cb, COMPOSE_REPLY_TO_ALL_WITH_QUOTE, NULL},
	{N_("/_Reply to all without quote"), "a", reply_cb, COMPOSE_REPLY_TO_ALL_WITHOUT_QUOTE, NULL}
};
static GtkItemFactoryEntry replysender_popup_entries[] =
{
	{N_("/Reply to sender with _quote"), NULL, reply_cb, COMPOSE_REPLY_TO_SENDER_WITH_QUOTE, NULL},
	{N_("/_Reply to sender without quote"), NULL, reply_cb, COMPOSE_REPLY_TO_SENDER_WITHOUT_QUOTE, NULL}
};
static GtkItemFactoryEntry fwd_popup_entries[] =
{
	{N_("/_Forward message (inline style)"), "f", reply_cb, COMPOSE_FORWARD_INLINE, NULL},
	{N_("/Forward message as _attachment"), "<shift>F", reply_cb, COMPOSE_FORWARD_AS_ATTACH, NULL}
};

MainWindow *main_window_create(SeparateType type)
{
	MainWindow *mainwin;
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *menubar;
	GtkWidget *handlebox;
	GtkWidget *vbox_body;
	GtkWidget *hbox_stat;
	GtkWidget *statusbar;
	GtkWidget *progressbar;
	GtkWidget *statuslabel;
	GtkWidget *ac_button;
	GtkWidget *ac_label;
 	GtkWidget *online_status;
	GtkWidget *offline_status;
	GtkWidget *online_switch;
	GtkWidget *offline_switch;

	FolderView *folderview;
	SummaryView *summaryview;
	MessageView *messageview;
	GdkColormap *colormap;
	GdkColor color[4];
	gboolean success[4];
	guint n_menu_entries;
	GtkItemFactory *ifactory;
	GtkWidget *ac_menu;
	GtkWidget *menuitem;
	GtkWidget *compose_popup;
	GtkWidget *reply_popup;
	GtkWidget *replyall_popup;
	GtkWidget *replysender_popup;
	GtkWidget *fwd_popup;
	gint i;

	static GdkGeometry geometry;

	debug_print(_("Creating main window...\n"));
	mainwin = g_new0(MainWindow, 1);

	/* main window */
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), PROG_VERSION);
	gtk_window_set_policy(GTK_WINDOW(window), TRUE, TRUE, FALSE);
	gtk_window_set_wmclass(GTK_WINDOW(window), "main_window", "Sylpheed");

	if (!geometry.min_height) {
		geometry.min_width = 320;
		geometry.min_height = 200;
	}
	gtk_window_set_geometry_hints(GTK_WINDOW(window), NULL, &geometry,
				      GDK_HINT_MIN_SIZE);

	gtk_signal_connect(GTK_OBJECT(window), "delete_event",
			   GTK_SIGNAL_FUNC(main_window_close_cb), mainwin);
	MANAGE_WINDOW_SIGNALS_CONNECT(window);
	gtk_signal_connect(GTK_OBJECT(window), "key_press_event",
				GTK_SIGNAL_FUNC(key_pressed), mainwin);

	gtk_widget_realize(window);
	gtk_widget_add_events(window, GDK_KEY_PRESS_MASK|GDK_KEY_RELEASE_MASK);
	

	gtkut_widget_set_app_icon(window);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(vbox);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	/* menu bar */
	n_menu_entries = sizeof(mainwin_entries) / sizeof(mainwin_entries[0]);
	menubar = menubar_create(window, mainwin_entries, 
				 n_menu_entries, "<Main>", mainwin);
	gtk_widget_show(menubar);
	gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, TRUE, 0);
	ifactory = gtk_item_factory_from_widget(menubar);

	handlebox = gtk_handle_box_new();
	gtk_widget_show(handlebox);
	gtk_box_pack_start(GTK_BOX(vbox), handlebox, FALSE, FALSE, 0);

	/* create the popup menus for the reply buttons specials */
	n_menu_entries = sizeof(reply_popup_entries) /
			sizeof(reply_popup_entries[0]);
	reply_popup = popupmenu_create(window, reply_popup_entries, n_menu_entries,
				      "<ReplyPopup>", mainwin);
	gtk_signal_connect(GTK_OBJECT(reply_popup), "selection_done",
			   GTK_SIGNAL_FUNC(toolbar_reply_popup_closed_cb), mainwin);
	n_menu_entries = sizeof(replyall_popup_entries) /
			sizeof(replyall_popup_entries[0]);
	replyall_popup = popupmenu_create(window, replyall_popup_entries, n_menu_entries,
				      "<ReplyAllPopup>", mainwin);
	gtk_signal_connect(GTK_OBJECT(replyall_popup), "selection_done",
			   GTK_SIGNAL_FUNC(toolbar_reply_to_all_popup_closed_cb), mainwin);
	n_menu_entries = sizeof(replysender_popup_entries) /
			sizeof(replysender_popup_entries[0]);
	replysender_popup = popupmenu_create(window, replysender_popup_entries, n_menu_entries,
				      "<ReplySenderPopup>", mainwin);
	gtk_signal_connect(GTK_OBJECT(replysender_popup), "selection_done",
			   GTK_SIGNAL_FUNC(toolbar_reply_to_sender_popup_closed_cb), mainwin);
	/* create the popup menu for the forward button */
	n_menu_entries = sizeof(fwd_popup_entries) /
			sizeof(fwd_popup_entries[0]);
	fwd_popup = popupmenu_create(window, fwd_popup_entries, n_menu_entries,
				      "<ForwardPopup>", mainwin);
	gtk_signal_connect(GTK_OBJECT(fwd_popup), "selection_done",
			   GTK_SIGNAL_FUNC(toolbar_forward_popup_closed_cb), mainwin);
			   
	main_window_toolbar_create(mainwin, handlebox);

	/* vbox that contains body */
	vbox_body = gtk_vbox_new(FALSE, BORDER_WIDTH);
	gtk_widget_show(vbox_body);
	gtk_container_set_border_width(GTK_CONTAINER(vbox_body), BORDER_WIDTH);
	gtk_box_pack_start(GTK_BOX(vbox), vbox_body, TRUE, TRUE, 0);

	hbox_stat = gtk_hbox_new(FALSE, 2);
	gtk_box_pack_end(GTK_BOX(vbox_body), hbox_stat, FALSE, FALSE, 0);

	statusbar = statusbar_create();
	gtk_box_pack_start(GTK_BOX(hbox_stat), statusbar, TRUE, TRUE, 0);

	progressbar = gtk_progress_bar_new();
	gtk_widget_set_usize(progressbar, 120, 1);
	gtk_box_pack_start(GTK_BOX(hbox_stat), progressbar, FALSE, FALSE, 0);

	online_status = stock_pixmap_widget(hbox_stat, STOCK_PIXMAP_WORK_ONLINE);
	offline_status = stock_pixmap_widget(hbox_stat, STOCK_PIXMAP_WORK_OFFLINE);
	online_switch = gtk_button_new ();
	offline_switch = gtk_button_new ();
	gtk_container_add (GTK_CONTAINER(online_switch), online_status);
	gtk_button_set_relief (GTK_BUTTON(online_switch), GTK_RELIEF_NONE);
	gtk_signal_connect (GTK_OBJECT(online_switch), "clicked", (GtkSignalFunc)online_switch_clicked, mainwin);
	gtk_box_pack_start (GTK_BOX(hbox_stat), online_switch, FALSE, FALSE, 0);
	gtk_container_add (GTK_CONTAINER(offline_switch), offline_status);
	gtk_button_set_relief (GTK_BUTTON(offline_switch), GTK_RELIEF_NONE);
	gtk_signal_connect (GTK_OBJECT(offline_switch), "clicked", (GtkSignalFunc)online_switch_clicked, mainwin);
	gtk_box_pack_start (GTK_BOX(hbox_stat), offline_switch, FALSE, FALSE, 0);
	
	statuslabel = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(hbox_stat), statuslabel, FALSE, FALSE, 0);

	ac_button = gtk_button_new();
	gtk_button_set_relief(GTK_BUTTON(ac_button), GTK_RELIEF_NONE);
	GTK_WIDGET_UNSET_FLAGS(ac_button, GTK_CAN_FOCUS);
	gtk_widget_set_usize(ac_button, -1, 1);
	gtk_box_pack_end(GTK_BOX(hbox_stat), ac_button, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(ac_button), "button_press_event",
			   GTK_SIGNAL_FUNC(ac_label_button_pressed), mainwin);

	ac_label = gtk_label_new("");
	gtk_container_add(GTK_CONTAINER(ac_button), ac_label);

	gtk_widget_show_all(hbox_stat);

	gtk_widget_hide(offline_switch);
	/* create views */
	mainwin->folderview  = folderview  = folderview_create();
	mainwin->summaryview = summaryview = summary_create();
	mainwin->messageview = messageview = messageview_create();
	mainwin->logwin      = log_window_create();

	folderview->mainwin      = mainwin;
	folderview->summaryview  = summaryview;

	summaryview->mainwin     = mainwin;
	summaryview->folderview  = folderview;
	summaryview->messageview = messageview;
	summaryview->window      = window;

	messageview->mainwin     = mainwin;

	mainwin->window       = window;
	mainwin->vbox         = vbox;
	mainwin->menubar      = menubar;
	mainwin->menu_factory = ifactory;
	mainwin->handlebox    = handlebox;
	mainwin->vbox_body    = vbox_body;
	mainwin->hbox_stat    = hbox_stat;
	mainwin->statusbar    = statusbar;
	mainwin->progressbar  = progressbar;
	mainwin->statuslabel  = statuslabel;
	mainwin->ac_button    = ac_button;
	mainwin->ac_label     = ac_label;
	
	mainwin->reply_popup       = reply_popup;
	mainwin->replyall_popup    = replyall_popup;
	mainwin->replysender_popup = replysender_popup;
	mainwin->fwd_popup         = fwd_popup;
	mainwin->online_switch     = online_switch;
	mainwin->offline_switch    = offline_switch;
	
	/* set context IDs for status bar */
	mainwin->mainwin_cid = gtk_statusbar_get_context_id
		(GTK_STATUSBAR(statusbar), "Main Window");
	mainwin->folderview_cid = gtk_statusbar_get_context_id
		(GTK_STATUSBAR(statusbar), "Folder View");
	mainwin->summaryview_cid = gtk_statusbar_get_context_id
		(GTK_STATUSBAR(statusbar), "Summary View");

	/* allocate colors for summary view and folder view */
	summaryview->color_marked.red = summaryview->color_marked.green = 0;
	summaryview->color_marked.blue = (guint16)65535;

	summaryview->color_dim.red = summaryview->color_dim.green =
		summaryview->color_dim.blue = COLOR_DIM;

	folderview->color_new.red = (guint16)55000;
	folderview->color_new.green = folderview->color_new.blue = 15000;

	gtkut_convert_int_to_gdk_color(prefs_common.tgt_folder_col,
				       &folderview->color_op);

	summaryview->color_important.red = 0;
	summaryview->color_important.green = 0;
	summaryview->color_important.blue = (guint16)65535;

	color[0] = summaryview->color_marked;
	color[1] = summaryview->color_dim;
	color[2] = folderview->color_new;
	color[3] = folderview->color_op;

	colormap = gdk_window_get_colormap(window->window);
	gdk_colormap_alloc_colors(colormap, color, 4, FALSE, TRUE, success);
	for (i = 0; i < 4; i++) {
		if (success[i] == FALSE)
			g_warning(_("MainWindow: color allocation %d failed\n"), i);
	}

	debug_print(_("done.\n"));

	messageview->visible = TRUE;

	main_window_set_widgets(mainwin, type);

	/* set menu items */
	menuitem = gtk_item_factory_get_item
		(ifactory, "/View/Code set/Auto detect");
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), TRUE);

	switch (prefs_common.toolbar_style) {
	case TOOLBAR_NONE:
		menuitem = gtk_item_factory_get_item
			(ifactory, "/View/Show or hide/Toolbar/None");
		break;
	case TOOLBAR_ICON:
		menuitem = gtk_item_factory_get_item
			(ifactory, "/View/Show or hide/Toolbar/Icon");
		break;
	case TOOLBAR_TEXT:
		menuitem = gtk_item_factory_get_item
			(ifactory, "/View/Show or hide/Toolbar/Text");
		break;
	case TOOLBAR_BOTH:
		menuitem = gtk_item_factory_get_item
			(ifactory, "/View/Show or hide/Toolbar/Icon and text");
	}
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), TRUE);

	gtk_widget_hide(mainwin->hbox_stat);
	menuitem = gtk_item_factory_get_item
		(ifactory, "/View/Show or hide/Status bar");
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem),
				       prefs_common.show_statusbar);

	/* set account selection menu */
	ac_menu = gtk_item_factory_get_widget
		(ifactory, "/Configuration/Change current account");
	gtk_signal_connect(GTK_OBJECT(ac_menu), "selection_done",
			   GTK_SIGNAL_FUNC(ac_menu_popup_closed), mainwin);
	mainwin->ac_menu = ac_menu;

	main_window_set_toolbar_sensitive(mainwin);

	/* Create actions menu */
	update_mainwin_actions_menu(ifactory, mainwin);

	/* show main window */
	gtk_widget_set_uposition(mainwin->window,
				 prefs_common.mainwin_x,
				 prefs_common.mainwin_y);
	gtk_widget_set_usize(window, prefs_common.mainwin_width,
			     prefs_common.mainwin_height);
	gtk_widget_show(mainwin->window);

	/* initialize views */
	folderview_init(folderview);
	summary_init(summaryview);
	messageview_init(messageview);
	log_window_init(mainwin->logwin);

	mainwin->lock_count = 0;
	mainwin->menu_lock_count = 0;
	mainwin->cursor_count = 0;

	if (!watch_cursor)
		watch_cursor = gdk_cursor_new(GDK_WATCH);

	mainwin_list = g_list_append(mainwin_list, mainwin);

	/* init work_offline */
	if (prefs_common.work_offline)
		online_switch_clicked (GTK_BUTTON(online_switch), mainwin);

	return mainwin;
}

void main_window_cursor_wait(MainWindow *mainwin)
{

	if (mainwin->cursor_count == 0)
		gdk_window_set_cursor(mainwin->window->window, watch_cursor);

	mainwin->cursor_count++;

	gdk_flush();
}

void main_window_cursor_normal(MainWindow *mainwin)
{
	if (mainwin->cursor_count)
		mainwin->cursor_count--;

	if (mainwin->cursor_count == 0)
		gdk_window_set_cursor(mainwin->window->window, NULL);

	gdk_flush();
}

/* lock / unlock the user-interface */
void main_window_lock(MainWindow *mainwin)
{
	if (mainwin->lock_count == 0)
		gtk_widget_set_sensitive(mainwin->ac_button, FALSE);

	mainwin->lock_count++;

	main_window_set_menu_sensitive(mainwin);
	main_window_set_toolbar_sensitive(mainwin);
}

void main_window_unlock(MainWindow *mainwin)
{
	if (mainwin->lock_count)
		mainwin->lock_count--;

	main_window_set_menu_sensitive(mainwin);
	main_window_set_toolbar_sensitive(mainwin);

	if (mainwin->lock_count == 0)
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

void main_window_reflect_prefs_all(void)
{
	main_window_reflect_prefs_all_real(FALSE);
}

void main_window_reflect_prefs_all_real(gboolean pixmap_theme_changed)
{
	GList *cur;
	MainWindow *mainwin;

	for (cur = mainwin_list; cur != NULL; cur = cur->next) {
		mainwin = (MainWindow *)cur->data;

		main_window_show_cur_account(mainwin);
		main_window_set_menu_sensitive(mainwin);
		main_window_set_toolbar_sensitive(mainwin);

		/* pixmap themes */
		if (pixmap_theme_changed) {
			gtk_container_remove(GTK_CONTAINER(mainwin->handlebox), GTK_WIDGET(mainwin->toolbar));
			mainwin->toolbar = NULL;
			main_window_toolbar_create(mainwin, mainwin->handlebox);
			set_toolbar_style(mainwin);
			activate_compose_button(mainwin, prefs_common.toolbar_style, mainwin->compose_btn_type);
			folderview_reflect_prefs_pixmap_theme(mainwin->folderview);
			summary_reflect_prefs_pixmap_theme(mainwin->summaryview);
		}
		
		if (prefs_common.immediate_exec)
			gtk_widget_hide(mainwin->exec_btn);
		else
			gtk_widget_show(mainwin->exec_btn);

		summary_redisplay_msg(mainwin->summaryview);
		headerview_set_visibility(mainwin->messageview->headerview,
					  prefs_common.display_header_pane);
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

void main_window_set_account_menu(GList *account_list)
{
	GList *cur, *cur_ac, *cur_item;
	GtkWidget *menuitem;
	MainWindow *mainwin;
	PrefsAccount *ac_prefs;

	for (cur = mainwin_list; cur != NULL; cur = cur->next) {
		mainwin = (MainWindow *)cur->data;

		/* destroy all previous menu item */
		cur_item = GTK_MENU_SHELL(mainwin->ac_menu)->children;
		while (cur_item != NULL) {
			GList *next = cur_item->next;
			gtk_widget_destroy(GTK_WIDGET(cur_item->data));
			cur_item = next;
		}

		for (cur_ac = account_list; cur_ac != NULL;
		     cur_ac = cur_ac->next) {
			ac_prefs = (PrefsAccount *)cur_ac->data;

			menuitem = gtk_menu_item_new_with_label
				(ac_prefs->account_name
				 ? ac_prefs->account_name : _("Untitled"));
			gtk_widget_show(menuitem);
			gtk_menu_append(GTK_MENU(mainwin->ac_menu), menuitem);
			gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
					   GTK_SIGNAL_FUNC(account_menu_cb),
					   ac_prefs);
		}
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

	gtk_label_set_text(GTK_LABEL(mainwin->ac_label), ac_name);
	gtk_widget_queue_resize(mainwin->ac_button);

	g_free(ac_name);
}

void main_window_separation_change(MainWindow *mainwin, SeparateType type)
{
	GtkWidget *folder_wid  = GTK_WIDGET_PTR(mainwin->folderview);
	GtkWidget *summary_wid = GTK_WIDGET_PTR(mainwin->summaryview);
	GtkWidget *message_wid = GTK_WIDGET_PTR(mainwin->messageview);

	debug_print(_("Changing window separation type from %d to %d\n"),
		    mainwin->type, type);

	if (mainwin->type == type) return;

	/* remove widgets from those containers */
	gtk_widget_ref(folder_wid);
	gtk_widget_ref(summary_wid);
	gtk_widget_ref(message_wid);
	gtkut_container_remove
		(GTK_CONTAINER(folder_wid->parent), folder_wid);
	gtkut_container_remove
		(GTK_CONTAINER(summary_wid->parent), summary_wid);
	gtkut_container_remove
		(GTK_CONTAINER(message_wid->parent), message_wid);

	/* clean containers */
	switch (mainwin->type) {
	case SEPARATE_NONE:
		gtk_widget_destroy(mainwin->win.sep_none.hpaned);
		break;
	case SEPARATE_FOLDER:
		gtk_widget_destroy(mainwin->win.sep_folder.vpaned);
		gtk_widget_destroy(mainwin->win.sep_folder.folderwin);
		break;
	case SEPARATE_MESSAGE:
		gtk_widget_destroy(mainwin->win.sep_message.hpaned);
		gtk_widget_destroy(mainwin->win.sep_message.messagewin);
		break;
	case SEPARATE_BOTH:
		gtk_widget_destroy(mainwin->win.sep_both.messagewin);
		gtk_widget_destroy(mainwin->win.sep_both.folderwin);
		break;
	}

	gtk_widget_hide(mainwin->window);
	main_window_set_widgets(mainwin, type);
	gtk_widget_show(mainwin->window);

	gtk_widget_unref(folder_wid);
	gtk_widget_unref(summary_wid);
	gtk_widget_unref(message_wid);
}

void main_window_toggle_message_view(MainWindow *mainwin)
{
	SummaryView *summaryview = mainwin->summaryview;
	union CompositeWin *cwin = &mainwin->win;
	GtkWidget *vpaned = NULL;
	GtkWidget *container = NULL;
	GtkWidget *msgwin = NULL;

	switch (mainwin->type) {
	case SEPARATE_NONE:
		vpaned = cwin->sep_none.vpaned;
		container = cwin->sep_none.hpaned;
		break;
	case SEPARATE_FOLDER:
		vpaned = cwin->sep_folder.vpaned;
		container = mainwin->vbox_body;
		break;
	case SEPARATE_MESSAGE:
		msgwin = mainwin->win.sep_message.messagewin;
		break;
	case SEPARATE_BOTH:
		msgwin = mainwin->win.sep_both.messagewin;
		break;
	}

	if (msgwin) {
		if (GTK_WIDGET_VISIBLE(msgwin)) {
			gtk_widget_hide(msgwin);
			mainwin->messageview->visible = FALSE;
			summaryview->displayed = NULL;
		} else {
			gtk_widget_show(msgwin);
			mainwin->messageview->visible = TRUE;
		}
	} else if (vpaned->parent != NULL) {
		mainwin->messageview->visible = FALSE;
		summaryview->displayed = NULL;
		gtk_widget_ref(vpaned);
		gtkut_container_remove(GTK_CONTAINER(container), vpaned);
		gtk_widget_reparent(GTK_WIDGET_PTR(summaryview), container);
		gtk_arrow_set(GTK_ARROW(summaryview->toggle_arrow),
			      GTK_ARROW_UP, GTK_SHADOW_OUT);
	} else {
		mainwin->messageview->visible = TRUE;
		gtk_widget_reparent(GTK_WIDGET_PTR(summaryview), vpaned);
		gtk_container_add(GTK_CONTAINER(container), vpaned);
		gtk_widget_unref(vpaned);
		gtk_arrow_set(GTK_ARROW(summaryview->toggle_arrow),
			      GTK_ARROW_DOWN, GTK_SHADOW_OUT);
	}

	main_window_set_menu_sensitive(mainwin);

	gtk_widget_grab_focus(summaryview->ctree);
}

void main_window_get_size(MainWindow *mainwin)
{
	GtkAllocation *allocation;

	allocation = &(GTK_WIDGET_PTR(mainwin->summaryview)->allocation);

	prefs_common.summaryview_width  = allocation->width;

	if ((mainwin->type == SEPARATE_NONE ||
	     mainwin->type == SEPARATE_FOLDER) &&
	    messageview_is_visible(mainwin->messageview))
		prefs_common.summaryview_height = allocation->height;

	prefs_common.mainview_width     = allocation->width;

	allocation = &mainwin->window->allocation;

	prefs_common.mainview_height = allocation->height;
	prefs_common.mainwin_width   = allocation->width;
	prefs_common.mainwin_height  = allocation->height;

	allocation = &(GTK_WIDGET_PTR(mainwin->folderview)->allocation);

	prefs_common.folderview_width  = allocation->width;
	prefs_common.folderview_height = allocation->height;
}

void main_window_get_position(MainWindow *mainwin)
{
	gint x, y;

	gtkut_widget_get_uposition(mainwin->window, &x, &y);

	prefs_common.mainview_x = x;
	prefs_common.mainview_y = y;
	prefs_common.mainwin_x = x;
	prefs_common.mainwin_y = y;

	debug_print(_("window position: x = %d, y = %d\n"), x, y);
}

void main_window_empty_trash(MainWindow *mainwin, gboolean confirm)
{
	GList *list;
	guint has_trash;
	Folder *folder;

	for (has_trash = 0, list = folder_get_list(); list != NULL; list = list->next) {
		folder = FOLDER(list->data);
		if (folder && folder->trash && folder->trash->total > 0)
			has_trash++;
	}

	if (!has_trash) return;
	
	if (confirm) {
		if (alertpanel(_("Empty trash"),
			       _("Empty all messages in trash?"),
			       _("Yes"), _("No"), NULL) != G_ALERTDEFAULT)
			return;
		manage_window_focus_in(mainwin->window, NULL, NULL);
	}

	procmsg_empty_trash();

	for (list = folder_get_list(); list != NULL; list = list->next) {
		folder = list->data;
		if (folder && folder->trash && folder->trash->total != 0) {
			folderview_update_item(folder->trash, TRUE);
		}
	}

	if (mainwin->summaryview->folder_item &&
	    mainwin->summaryview->folder_item->stype == F_TRASH)
		gtk_widget_grab_focus(mainwin->folderview->ctree);
}

void main_window_add_mailbox(MainWindow *mainwin)
{
	gchar *path;
	Folder *folder;

	path = input_dialog(_("Add mailbox"),
			    _("Input the location of mailbox.\n"
			      "If the existing mailbox is specified, it will be\n"
			      "scanned automatically."),
			    "Mail");
	if (!path) return;
	if (folder_find_from_path(path)) {
		alertpanel_error(_("The mailbox `%s' already exists."), path);
		g_free(path);
		return;
	}
	if (!strcmp(path, "Mail"))
		folder = folder_new(F_MH, _("Mailbox"), path);
	else
		folder = folder_new(F_MH, g_basename(path), path);
	g_free(path);

	if (folder->create_tree(folder) < 0) {
		alertpanel_error(_("Creation of the mailbox failed.\n"
				   "Maybe some files already exist, or you don't have the permission to write there."));
		folder_destroy(folder);
		return;
	}

	folder_add(folder);
	folder_set_ui_func(folder, scan_tree_func, mainwin);
	folder->scan_tree(folder);
	folder_set_ui_func(folder, NULL, NULL);

	folderview_set(mainwin->folderview);
}

void main_window_add_mbox(MainWindow *mainwin)
{
	gchar *path;
	Folder *folder;
	FolderItem * item;

	path = input_dialog(_("Add mbox mailbox"),
			    _("Input the location of mailbox."),
			    "mail");

	if (!path) return;

	if (folder_find_from_path(path)) {
		alertpanel_error(_("The mailbox `%s' already exists."), path);
		g_free(path);
		return;
	}

	/*
	if (!strcmp(path, "Mail"))
		folder = folder_new(F_MBOX, _("Mailbox"), path);
		else
	*/

	folder = folder_new(F_MBOX, g_basename(path), path);
	g_free(path);

	if (folder->create_tree(folder) < 0) {
		alertpanel_error(_("Creation of the mailbox failed."));
		folder_destroy(folder);
		return;
	}

	folder_add(folder);

	item = folder_item_new(folder->name, NULL);
	item->folder = folder;
	folder->node = g_node_new(item);

	folder->create_folder(folder, item, "inbox");
	folder->create_folder(folder, item, "outbox");
	folder->create_folder(folder, item, "queue");
	folder->create_folder(folder, item, "draft");
	folder->create_folder(folder, item, "trash");

	folderview_set(mainwin->folderview);
}

typedef enum
{
	M_UNLOCKED            = 1 << 0,
	M_MSG_EXIST           = 1 << 1,
	M_TARGET_EXIST        = 1 << 2,
	M_SINGLE_TARGET_EXIST = 1 << 3,
	M_EXEC                = 1 << 4,
	M_ALLOW_REEDIT        = 1 << 5,
	M_HAVE_ACCOUNT        = 1 << 6,
	M_THREADED	      = 1 << 7,
	M_UNTHREADED	      = 1 << 8,
	M_ALLOW_DELETE	      = 1 << 9,
	M_INC_ACTIVE	      = 1 << 10,
	M_NEWS                = 1 << 11,
	M_HAVE_NEWS_ACCOUNT   = 1 << 12,
	M_HIDE_READ_MSG	      = 1 << 13
} SensitiveCond;

static SensitiveCond main_window_get_current_state(MainWindow *mainwin)
{
	SensitiveCond state = 0;
	SummarySelection selection;
	FolderItem *item = mainwin->summaryview->folder_item;
	GList *account_list = account_get_list();
	
	selection = summary_get_selection_type(mainwin->summaryview);

	if (mainwin->lock_count == 0)
		state |= M_UNLOCKED;
	if (selection != SUMMARY_NONE)
		state |= M_MSG_EXIST;
	if (item && item->path && item->parent && !item->no_select) {
		state |= M_EXEC;
		if (item->threaded)
			state |= M_THREADED;
		else
			state |= M_UNTHREADED;	
		/*		if (item->folder->type != F_NEWS) */
		state |= M_ALLOW_DELETE;

		if (selection == SUMMARY_NONE && item->hide_read_msgs
		    || selection != SUMMARY_NONE)
			state |= M_HIDE_READ_MSG;	
	}		
	if (selection == SUMMARY_SELECTED_SINGLE ||
	    selection == SUMMARY_SELECTED_MULTIPLE)
		state |= M_TARGET_EXIST;
	if (selection == SUMMARY_SELECTED_SINGLE)
		state |= M_SINGLE_TARGET_EXIST;
	if (mainwin->summaryview->folder_item &&
	    mainwin->summaryview->folder_item->folder->type == F_NEWS)
		state |= M_NEWS;
	if (selection == SUMMARY_SELECTED_SINGLE &&
	    (item &&
	     (item->stype == F_OUTBOX || item->stype == F_DRAFT ||
	      item->stype == F_QUEUE)))
		state |= M_ALLOW_REEDIT;
	if (cur_account)
		state |= M_HAVE_ACCOUNT;
	
	for ( ; account_list != NULL; account_list = account_list->next) {
		if (((PrefsAccount*)account_list->data)->protocol == A_NNTP) {
			state |= M_HAVE_NEWS_ACCOUNT;
			break;
		}
	}

	if (inc_is_active())
		state |= M_INC_ACTIVE;

	return state;
}

void main_window_set_toolbar_sensitive(MainWindow *mainwin)
{
	SensitiveCond state;
	gboolean sensitive;
	gint i = 0;

	struct {
		GtkWidget *widget;
		SensitiveCond cond;
	} entry[11];

#define SET_WIDGET_COND(w, c)	\
{				\
	entry[i].widget = w;	\
	entry[i].cond = c;	\
	i++;			\
}

	SET_WIDGET_COND(mainwin->get_btn, M_HAVE_ACCOUNT|M_UNLOCKED);
	SET_WIDGET_COND(mainwin->getall_btn, M_HAVE_ACCOUNT|M_UNLOCKED);
	SET_WIDGET_COND(mainwin->compose_news_btn, M_HAVE_ACCOUNT);
	SET_WIDGET_COND(mainwin->reply_btn,
			M_HAVE_ACCOUNT|M_SINGLE_TARGET_EXIST);
	SET_WIDGET_COND(mainwin->replyall_btn,
			M_HAVE_ACCOUNT|M_SINGLE_TARGET_EXIST);
	SET_WIDGET_COND(mainwin->replysender_btn,
			M_HAVE_ACCOUNT|M_SINGLE_TARGET_EXIST);
	SET_WIDGET_COND(mainwin->fwd_btn, M_HAVE_ACCOUNT|M_TARGET_EXIST);
#if 0
	SET_WIDGET_COND(mainwin->prefs_btn, M_UNLOCKED);
	SET_WIDGET_COND(mainwin->account_btn, M_UNLOCKED);
#endif
	SET_WIDGET_COND(mainwin->next_btn, M_MSG_EXIST);
	SET_WIDGET_COND(mainwin->delete_btn,
			M_TARGET_EXIST|M_ALLOW_DELETE|M_UNLOCKED);
	SET_WIDGET_COND(mainwin->exec_btn, M_MSG_EXIST|M_EXEC|M_UNLOCKED);
	SET_WIDGET_COND(NULL, 0);

#undef SET_WIDGET_COND

	state = main_window_get_current_state(mainwin);

	for (i = 0; entry[i].widget != NULL; i++) {
		sensitive = ((entry[i].cond & state) == entry[i].cond);
		gtk_widget_set_sensitive(entry[i].widget, sensitive);
	}

	activate_compose_button(mainwin, 
			prefs_common.toolbar_style,
			mainwin->compose_btn_type);
}

void main_window_set_menu_sensitive(MainWindow *mainwin)
{
	GtkItemFactory *ifactory = mainwin->menu_factory;
	SensitiveCond state;
	gboolean sensitive;
	GtkWidget *menuitem;
	FolderItem *item;
	gchar *menu_path;
	gint i;

	static const struct {
		gchar *const entry;
		SensitiveCond cond;
	} entry[] = {
		{"/File/Add mailbox..."        , M_UNLOCKED},
                {"/File/Add mbox mailbox..."   , M_UNLOCKED},
		{"/File/Rescan folder tree"    , M_UNLOCKED},
		{"/File/Folder"                , M_UNLOCKED},
		{"/File/Import mbox file..."   , M_UNLOCKED},
		{"/File/Export to mbox file...", M_UNLOCKED},
		{"/File/Empty trash"           , M_UNLOCKED},
		{"/File/Work offline"	       , M_UNLOCKED},
		{"/File/Save as...", M_SINGLE_TARGET_EXIST|M_UNLOCKED},
		{"/File/Print..."  , M_TARGET_EXIST|M_UNLOCKED},
		/* {"/File/Close", M_UNLOCKED}, */
		{"/File/Exit" , M_UNLOCKED},

		{"/Edit/Select thread"		   , M_SINGLE_TARGET_EXIST},
		{"/View/Sort"                      , M_EXEC},
		{"/View/Thread view"               , M_EXEC},
		{"/View/Expand all threads"        , M_MSG_EXIST},
		{"/View/Collapse all threads"      , M_MSG_EXIST},
		{"/View/Hide read messages"	   , M_HIDE_READ_MSG},
		{"/View/Go to/Prev message"        , M_MSG_EXIST},
		{"/View/Go to/Next message"        , M_MSG_EXIST},
		{"/View/Go to/Prev unread message" , M_MSG_EXIST},
		{"/View/Go to/Next unread message" , M_MSG_EXIST},
		{"/View/Go to/Prev new message"    , M_MSG_EXIST},
		{"/View/Go to/Next new message"    , M_MSG_EXIST},
		{"/View/Go to/Prev marked message" , M_MSG_EXIST},
		{"/View/Go to/Next marked message" , M_MSG_EXIST},
		{"/View/Go to/Prev labeled message", M_MSG_EXIST},
		{"/View/Go to/Next labeled message", M_MSG_EXIST},
		{"/View/Open in new window"        , M_SINGLE_TARGET_EXIST},
		{"/View/Show all header"           , M_SINGLE_TARGET_EXIST},
		{"/View/Message source"            , M_SINGLE_TARGET_EXIST},

		{"/Message/Get new mail"          , M_HAVE_ACCOUNT|M_UNLOCKED},
		{"/Message/Get from all accounts" , M_HAVE_ACCOUNT|M_UNLOCKED},
		{"/Message/Cancel receiving"      , M_INC_ACTIVE},
		{"/Message/Compose a news message", M_HAVE_NEWS_ACCOUNT},
		{"/Message/Reply"                 , M_HAVE_ACCOUNT|M_SINGLE_TARGET_EXIST},
		{"/Message/Reply to sender"       , M_HAVE_ACCOUNT|M_SINGLE_TARGET_EXIST},
		{"/Message/Reply to all"          , M_HAVE_ACCOUNT|M_SINGLE_TARGET_EXIST},
		{"/Message/Follow-up and reply to", M_HAVE_ACCOUNT|M_SINGLE_TARGET_EXIST|M_NEWS},
		{"/Message/Forward"               , M_HAVE_ACCOUNT|M_TARGET_EXIST},
        	{"/Message/Redirect"		  , M_HAVE_ACCOUNT|M_SINGLE_TARGET_EXIST},
		{"/Message/Re-edit"		  , M_HAVE_ACCOUNT|M_ALLOW_REEDIT},
		{"/Message/Move..."		  , M_TARGET_EXIST|M_ALLOW_DELETE|M_UNLOCKED},
		{"/Message/Copy..."		  , M_TARGET_EXIST|M_EXEC|M_UNLOCKED},
		{"/Message/Delete" 		  , M_TARGET_EXIST|M_ALLOW_DELETE|M_UNLOCKED},
		{"/Message/Mark"   		  , M_TARGET_EXIST},

		{"/Tools/Selective download..."	    , M_HAVE_ACCOUNT|M_UNLOCKED},
		{"/Tools/Add sender to address book", M_SINGLE_TARGET_EXIST},
		{"/Tools/Harvest addresses"	    , M_UNLOCKED},
		{"/Tools/Filter messages"           , M_MSG_EXIST|M_EXEC|M_UNLOCKED},
		{"/Tools/Create filter rule"        , M_SINGLE_TARGET_EXIST|M_UNLOCKED},
		{"/Tools/Execute"                   , M_MSG_EXIST|M_EXEC|M_UNLOCKED},
		{"/Tools/Actions"		    , M_MSG_EXIST},
		{"/Tools/Delete duplicated messages", M_MSG_EXIST|M_ALLOW_DELETE|M_UNLOCKED},

		{"/Configuration", M_UNLOCKED},

		{NULL, 0}
	};

	state = main_window_get_current_state(mainwin);

	for (i = 0; entry[i].entry != NULL; i++) {
		sensitive = ((entry[i].cond & state) == entry[i].cond);
		menu_set_sensitive(ifactory, entry[i].entry, sensitive);
	}

	main_window_menu_callback_block(mainwin);

#define SET_CHECK_MENU_ACTIVE(path, active) \
{ \
	menuitem = gtk_item_factory_get_widget(ifactory, path); \
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), active); \
}

	SET_CHECK_MENU_ACTIVE("/View/Show or hide/Message view",
			      messageview_is_visible(mainwin->messageview));

	item = mainwin->summaryview->folder_item;
	menu_path = "/View/Sort/Don't sort";
	if (item) {
		switch (item->sort_key) {
		case SORT_BY_NUMBER:
			menu_path = "/View/Sort/by number"; break;
		case SORT_BY_SIZE:
			menu_path = "/View/Sort/by size"; break;
		case SORT_BY_DATE:
			menu_path = "/View/Sort/by date"; break;
		case SORT_BY_FROM:
			menu_path = "/View/Sort/by from"; break;
		case SORT_BY_SUBJECT:
			menu_path = "/View/Sort/by subject"; break;
		case SORT_BY_LABEL:
			menu_path = "/View/Sort/by color label"; break;
		case SORT_BY_MARK:
			menu_path = "/View/Sort/by mark"; break;
		case SORT_BY_UNREAD:
			menu_path = "/View/Sort/by unread"; break;
		case SORT_BY_MIME:
			menu_path = "/View/Sort/by attachment"; break;
		case SORT_BY_SCORE:
			menu_path = "/View/Sort/by score"; break;
		case SORT_BY_LOCKED:
			menu_path = "/View/Sort/by locked"; break;
		case SORT_BY_NONE:
		default:
			menu_path = "/View/Sort/Don't sort"; break;
		}
	}
	SET_CHECK_MENU_ACTIVE(menu_path, TRUE);

	if (!item || item->sort_type == SORT_ASCENDING) {
		SET_CHECK_MENU_ACTIVE("/View/Sort/Ascending", TRUE);
	} else {
		SET_CHECK_MENU_ACTIVE("/View/Sort/Descending", TRUE);
	}

	if (item && item->sort_key != SORT_BY_NONE) {
		menu_set_sensitive(ifactory, "/View/Sort/Ascending", TRUE);
		menu_set_sensitive(ifactory, "/View/Sort/Descending", TRUE);
	} else {
		menu_set_sensitive(ifactory, "/View/Sort/Ascending", FALSE);
		menu_set_sensitive(ifactory, "/View/Sort/Descending", FALSE);
	}

	SET_CHECK_MENU_ACTIVE("/View/Show all header",
			      mainwin->messageview->textview->show_all_headers);
	SET_CHECK_MENU_ACTIVE("/View/Thread view", (state & M_THREADED) != 0);

#undef SET_CHECK_MENU_ACTIVE

	main_window_menu_callback_unblock(mainwin);
}

void main_window_popup(MainWindow *mainwin)
{
	gint x, y;
	gint sx, sy;
	GtkWidget *widget;

	gdk_window_get_origin(mainwin->window->window, &x, &y);
	sx = gdk_screen_width();
	sy = gdk_screen_height();
	x %= sx; if (x < 0) x = 0;
	y %= sy; if (y < 0) y = 0;
	gdk_window_move(mainwin->window->window, x, y);
	gdk_window_raise(mainwin->window->window);
	gdk_window_show(mainwin->window->window);

	debug_print("window position: x = %d, y = %d\n", x, y);

	switch (mainwin->type) {
	case SEPARATE_FOLDER:
		widget = mainwin->win.sep_folder.folderwin;
		gdk_window_get_origin(widget->window, &x, &y);
		x %= sx; if (x < 0) x = 0;
		y %= sy; if (y < 0) y = 0;
		gdk_window_move(widget->window, x, y);
		gdk_window_raise(widget->window);
		break;
	case SEPARATE_MESSAGE:
		widget = mainwin->win.sep_message.messagewin;
		gdk_window_get_origin(widget->window, &x, &y);
		x %= sx; if (x < 0) x = 0;
		y %= sy; if (y < 0) y = 0;
		gdk_window_move(widget->window, x, y);
		gdk_window_raise(widget->window);
		break;
	case SEPARATE_BOTH:
		widget = mainwin->win.sep_both.folderwin;
		gdk_window_get_origin(widget->window, &x, &y);
		x %= sx; if (x < 0) x = 0;
		y %= sy; if (y < 0) y = 0;
		gdk_window_move(widget->window, x, y);
		gdk_window_raise(widget->window);
		widget = mainwin->win.sep_both.messagewin;
		gdk_window_get_origin(widget->window, &x, &y);
		x %= sx; if (x < 0) x = 0;
		y %= sy; if (y < 0) y = 0;
		gdk_window_move(widget->window, x, y);
		gdk_window_raise(widget->window);
		break;
	default:
		break;
	}
}

static void main_window_set_widgets(MainWindow *mainwin, SeparateType type)
{
	GtkWidget *folderwin = NULL;
	GtkWidget *messagewin = NULL;
	GtkWidget *hpaned;
	GtkWidget *vpaned;
	GtkWidget *vbox_body = mainwin->vbox_body;
	GtkItemFactory *ifactory = mainwin->menu_factory;
	GtkWidget *menuitem;

	debug_print(_("Setting widgets..."));

	/* create separated window(s) if needed */
	if (type & SEPARATE_FOLDER) {
		folderwin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		gtk_window_set_title(GTK_WINDOW(folderwin),
				     _("Sylpheed - Folder View"));
		gtk_window_set_wmclass(GTK_WINDOW(folderwin),
				       "folder_view", "Sylpheed");
		gtk_window_set_policy(GTK_WINDOW(folderwin),
				      TRUE, TRUE, FALSE);
		gtk_widget_set_usize(folderwin, -1,
				     prefs_common.mainview_height);
		gtk_container_set_border_width(GTK_CONTAINER(folderwin),
					       BORDER_WIDTH);
		gtk_signal_connect(GTK_OBJECT(folderwin), "delete_event",
				   GTK_SIGNAL_FUNC(folder_window_close_cb),
				   mainwin);
	}
	if (type & SEPARATE_MESSAGE) {
		messagewin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		gtk_window_set_title(GTK_WINDOW(messagewin),
				     _("Sylpheed - Message View"));
		gtk_window_set_wmclass(GTK_WINDOW(messagewin),
				       "message_view", "Sylpheed");
		gtk_window_set_policy(GTK_WINDOW(messagewin),
				      TRUE, TRUE, FALSE);
		gtk_widget_set_usize
			(messagewin, prefs_common.mainview_width,
			 prefs_common.mainview_height
			 - prefs_common.summaryview_height
			 + DEFAULT_HEADERVIEW_HEIGHT);
		gtk_container_set_border_width(GTK_CONTAINER(messagewin),
					       BORDER_WIDTH);
		gtk_signal_connect(GTK_OBJECT(messagewin), "delete_event",
				   GTK_SIGNAL_FUNC(message_window_close_cb),
				   mainwin);
	}

	switch (type) {
	case SEPARATE_NONE:
		hpaned = gtk_hpaned_new();
		gtk_widget_show(hpaned);
		gtk_box_pack_start(GTK_BOX(vbox_body), hpaned, TRUE, TRUE, 0);
		gtk_paned_add1(GTK_PANED(hpaned),
			       GTK_WIDGET_PTR(mainwin->folderview));

		vpaned = gtk_vpaned_new();
		if (messageview_is_visible(mainwin->messageview)) {
			gtk_paned_add2(GTK_PANED(hpaned), vpaned);
			gtk_paned_add1(GTK_PANED(vpaned),
				       GTK_WIDGET_PTR(mainwin->summaryview));
		} else {
			gtk_paned_add2(GTK_PANED(hpaned),
				       GTK_WIDGET_PTR(mainwin->summaryview));
			gtk_widget_ref(vpaned);
		}
		gtk_widget_set_usize(GTK_WIDGET_PTR(mainwin->summaryview),
				     prefs_common.summaryview_width,
				     prefs_common.summaryview_height);
		gtk_paned_add2(GTK_PANED(vpaned),
			       GTK_WIDGET_PTR(mainwin->messageview));
		gtk_widget_set_usize(GTK_WIDGET_PTR(mainwin->messageview),
				     prefs_common.mainview_width, -1);
		gtk_widget_set_usize(mainwin->window,
				     prefs_common.folderview_width +
				     prefs_common.mainview_width,
				     prefs_common.mainwin_height);
		gtk_widget_show_all(vpaned);

		mainwin->win.sep_none.hpaned = hpaned;
		mainwin->win.sep_none.vpaned = vpaned;
		break;
	case SEPARATE_FOLDER:
		vpaned = gtk_vpaned_new();
		if (messageview_is_visible(mainwin->messageview)) {
			gtk_box_pack_start(GTK_BOX(vbox_body), vpaned,
					   TRUE, TRUE, 0);
			gtk_paned_add1(GTK_PANED(vpaned),
				       GTK_WIDGET_PTR(mainwin->summaryview));
		} else {
			gtk_box_pack_start(GTK_BOX(vbox_body),
					   GTK_WIDGET_PTR(mainwin->summaryview),
					   TRUE, TRUE, 0);
			gtk_widget_ref(vpaned);
		}
		gtk_paned_add2(GTK_PANED(vpaned),
			       GTK_WIDGET_PTR(mainwin->messageview));
		gtk_widget_show_all(vpaned);
		gtk_widget_set_usize(GTK_WIDGET_PTR(mainwin->summaryview),
				     prefs_common.summaryview_width,
				     prefs_common.summaryview_height);
		gtk_widget_set_usize(GTK_WIDGET_PTR(mainwin->messageview),
				     prefs_common.mainview_width, -1);
		gtk_widget_set_usize(mainwin->window,
				     prefs_common.mainview_width,
				     prefs_common.mainview_height);

		gtk_container_add(GTK_CONTAINER(folderwin),
				  GTK_WIDGET_PTR(mainwin->folderview));

		mainwin->win.sep_folder.folderwin = folderwin;
		mainwin->win.sep_folder.vpaned    = vpaned;

		gtk_widget_show_all(folderwin);
		break;
	case SEPARATE_MESSAGE:
		hpaned = gtk_hpaned_new();
		gtk_box_pack_start(GTK_BOX(vbox_body), hpaned, TRUE, TRUE, 0);

		gtk_paned_add1(GTK_PANED(hpaned),
			       GTK_WIDGET_PTR(mainwin->folderview));
		gtk_paned_add2(GTK_PANED(hpaned),
			       GTK_WIDGET_PTR(mainwin->summaryview));
		gtk_widget_set_usize(GTK_WIDGET_PTR(mainwin->summaryview),
				     prefs_common.summaryview_width,
				     prefs_common.summaryview_height);
		gtk_widget_set_usize(mainwin->window,
				     prefs_common.folderview_width +
				     prefs_common.mainview_width,
				     prefs_common.mainwin_height);
		gtk_widget_show_all(hpaned);
		gtk_container_add(GTK_CONTAINER(messagewin),
				  GTK_WIDGET_PTR(mainwin->messageview));

		mainwin->win.sep_message.messagewin = messagewin;
		mainwin->win.sep_message.hpaned     = hpaned;

		gtk_widget_show_all(messagewin);
		break;
	case SEPARATE_BOTH:
		gtk_box_pack_start(GTK_BOX(vbox_body),
				   GTK_WIDGET_PTR(mainwin->summaryview),
				   TRUE, TRUE, 0);
		gtk_widget_set_usize(GTK_WIDGET_PTR(mainwin->summaryview),
				     prefs_common.summaryview_width,
				     prefs_common.summaryview_height);
		gtk_widget_set_usize(mainwin->window,
				     prefs_common.mainview_width,
				     prefs_common.mainwin_height);
		gtk_container_add(GTK_CONTAINER(folderwin),
				  GTK_WIDGET_PTR(mainwin->folderview));
		gtk_container_add(GTK_CONTAINER(messagewin),
				  GTK_WIDGET_PTR(mainwin->messageview));

		mainwin->win.sep_both.folderwin = folderwin;
		mainwin->win.sep_both.messagewin = messagewin;

		gtk_widget_show_all(folderwin);
		gtk_widget_show_all(messagewin);
		break;
	}

	mainwin->type = type;

	mainwin->messageview->visible = TRUE;

	/* toggle menu state */
	menuitem = gtk_item_factory_get_item
		(ifactory, "/View/Show or hide/Folder tree");
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), TRUE);
	menuitem = gtk_item_factory_get_item
		(ifactory, "/View/Show or hide/Message view");
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), TRUE);

	menuitem = gtk_item_factory_get_item
		(ifactory, "/View/Separate folder tree");
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem),
				       ((type & SEPARATE_FOLDER) != 0));
	menuitem = gtk_item_factory_get_item
		(ifactory, "/View/Separate message view");
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem),
				       ((type & SEPARATE_MESSAGE) != 0));

	debug_print(_("done.\n"));
}

static void main_window_toolbar_create(MainWindow *mainwin,
				       GtkWidget *container)
{
	GtkWidget *toolbar;
	GtkWidget *icon_wid;
	GtkWidget *get_btn;
	GtkWidget *getall_btn;
	GtkWidget *compose_mail_btn;
	GtkWidget *compose_news_btn;
	GtkWidget *reply_btn;
	GtkWidget *replyall_btn;
	GtkWidget *replysender_btn;
	GtkWidget *fwd_btn;
	GtkWidget *send_btn;
#if 0
	GtkWidget *prefs_btn;
	GtkWidget *account_btn;
#endif
	GtkWidget *next_btn;
	GtkWidget *delete_btn;
	GtkWidget *exec_btn;
	
	GtkTooltips *tooltips;

	toolbar = gtk_toolbar_new(GTK_ORIENTATION_HORIZONTAL,
				  GTK_TOOLBAR_BOTH);
	gtk_container_add(GTK_CONTAINER(container), toolbar);
	gtk_container_set_border_width(GTK_CONTAINER(container), 2);
	gtk_toolbar_set_button_relief(GTK_TOOLBAR(toolbar), GTK_RELIEF_NONE);
	gtk_toolbar_set_space_style(GTK_TOOLBAR(toolbar),
				    GTK_TOOLBAR_SPACE_LINE);

	icon_wid = stock_pixmap_widget(container, STOCK_PIXMAP_MAIL_RECEIVE);
	get_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					  _("Get"),
					  _("Get new mail from current account"),
					  "Get",
					  icon_wid, toolbar_inc_cb, mainwin);
	icon_wid = stock_pixmap_widget(container, STOCK_PIXMAP_MAIL_RECEIVE_ALL);
	getall_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					     _("Get all"),
					     _("Get new mail from all accounts"),
					     "Get all",
					     icon_wid,
					     toolbar_inc_all_cb,
					     mainwin);

	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));

	icon_wid = stock_pixmap_widget(container, STOCK_PIXMAP_MAIL_SEND);
	send_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					   _("Send"),
					   _("Send queued message(s)"),
					   "Send",
					   icon_wid,
					   toolbar_send_cb,
					   mainwin);

	/* insert compose mail button widget */					   
	icon_wid = stock_pixmap_widget(container, STOCK_PIXMAP_MAIL_COMPOSE);
	compose_mail_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					      _("Email"),
					      _("Compose an email message"),
					      "New",
					      icon_wid,
					      toolbar_compose_mail_cb,
					      mainwin);

	/* insert compose news button widget */
	icon_wid = stock_pixmap_widget(container, STOCK_PIXMAP_NEWS_COMPOSE);
	compose_news_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					      _("News"),
					      _("Compose a news message"),
					      "New",
					      icon_wid,
					      toolbar_compose_news_cb,
					      mainwin);

	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));
	
	/* reply button */
	
	icon_wid = stock_pixmap_widget(container, STOCK_PIXMAP_MAIL_REPLY);
	reply_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					    _("Reply"),
					    _("Reply to the message - Right button: more options"),
					    "Reply",
					    icon_wid,
					    toolbar_reply_cb,
					    mainwin);

	/* replyall button */

	icon_wid = stock_pixmap_widget(container, STOCK_PIXMAP_MAIL_REPLY_TO_ALL);
	replyall_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					       _("All"),
					       _("Reply to all - Right button: more options"),
					       "Reply to all",
					       icon_wid,
					       toolbar_reply_to_all_cb,
					       mainwin);

	/* reply to sender button */
	icon_wid = stock_pixmap_widget(container, STOCK_PIXMAP_MAIL_REPLY_TO_AUTHOR);
	replysender_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
						  _("Sender"),
						  _("Reply to sender - Right button: more options"),
						  "Reply to sender",
						  icon_wid,
						  toolbar_reply_to_sender_cb,
						  mainwin);

	/* forward button */
	icon_wid = stock_pixmap_widget(container, STOCK_PIXMAP_MAIL_FORWARD);
	fwd_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					  _("Forward"),
					  _("Forward the message - Right button: more options"),
					  "Fwd",
					  icon_wid,
					  toolbar_forward_cb,
					  mainwin);

	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));

	icon_wid = stock_pixmap_widget(container, STOCK_PIXMAP_CLOSE);
	delete_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					  _("Delete"),
					  _("Delete the message"),
					  "Delete",
					  icon_wid,
					  toolbar_delete_cb,
					  mainwin);

	icon_wid = stock_pixmap_widget(container, STOCK_PIXMAP_EXEC);
	exec_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					   _("Execute"),
					   _("Execute marked process"),
					   "Execute",
					   icon_wid,
					   toolbar_exec_cb,
					   mainwin);

	icon_wid = stock_pixmap_widget(container, STOCK_PIXMAP_DOWN_ARROW);
	next_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					   _("Next"),
					   _("Next unread message"),
					   "Next unread",
					   icon_wid,
					   toolbar_next_unread_cb,
					   mainwin);

#if 0
	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));

	icon_wid = stock_pixmap_widget(container, STOCK_PIXMAP_PREFERENCES);
	prefs_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					    _("Prefs"),
					    _("Common preferences"),
					    "Prefs",
					    icon_wid,
					    toolbar_prefs_cb,
					    mainwin);
	icon_wid = stock_pixmap_widget(container, STOCK_PIXMAP_PROPERTIES);
	account_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					      _("Account"),
					      _("Account setting"),
					      "Account",
					      icon_wid,
					      toolbar_account_cb,
					      mainwin);
	gtk_signal_connect(GTK_OBJECT(account_btn), "button_press_event",
			   GTK_SIGNAL_FUNC(toolbar_account_button_pressed),
			   mainwin);
#endif

	gtk_signal_connect(GTK_OBJECT(reply_btn), "button_press_event",
		GTK_SIGNAL_FUNC(toolbar_reply_popup_cb),
		mainwin);
	
	gtk_signal_connect(GTK_OBJECT(replyall_btn), "button_press_event",
		GTK_SIGNAL_FUNC(toolbar_reply_to_all_popup_cb),
		mainwin);
	
	gtk_signal_connect(GTK_OBJECT(replysender_btn), "button_press_event",
		GTK_SIGNAL_FUNC(toolbar_reply_to_sender_popup_cb),
		mainwin);
	
	gtk_signal_connect(GTK_OBJECT(fwd_btn), "button_press_event",
		GTK_SIGNAL_FUNC(toolbar_forward_popup_cb),
		mainwin);
	

	mainwin->toolbar        	= toolbar;
	mainwin->get_btn        	= get_btn;
	mainwin->getall_btn     	= getall_btn;
	mainwin->compose_mail_btn	= compose_mail_btn;
	mainwin->compose_news_btn	= compose_news_btn;
	mainwin->reply_btn      	= reply_btn;
	mainwin->replyall_btn   	= replyall_btn;
	mainwin->replysender_btn	= replysender_btn;
	mainwin->fwd_btn          	= fwd_btn;
	mainwin->send_btn        	= send_btn;
	#if 0
	mainwin->prefs_btn       	= prefs_btn;
	mainwin->account_btn     	= account_btn;
	#endif
	mainwin->next_btn        	= next_btn;
	mainwin->delete_btn      	= delete_btn;
	mainwin->exec_btn        	= exec_btn;

	gtk_widget_show_all(toolbar);
}

/* callback functions */
static void toolbar_reply_popup_cb(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	MainWindow *mainwindow = (MainWindow *) data;
	
	if (!event) return;

	if (event->button == 3) {
		gtk_button_set_relief(GTK_BUTTON(widget), GTK_RELIEF_NORMAL);
		gtk_menu_popup(GTK_MENU(mainwindow->reply_popup), NULL, NULL,
		       menu_button_position, widget,
		       event->button, event->time);
	}
}

static void toolbar_reply_popup_closed_cb(GtkMenuShell *menu_shell, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	gtk_button_set_relief(GTK_BUTTON(mainwin->reply_btn), GTK_RELIEF_NONE);
	manage_window_focus_in(mainwin->window, NULL, NULL);
}

static void toolbar_reply_to_all_popup_cb(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	MainWindow *mainwindow = (MainWindow *) data;
	
	if (!event) return;

	if (event->button == 3) {
		gtk_button_set_relief(GTK_BUTTON(widget), GTK_RELIEF_NORMAL);
		gtk_menu_popup(GTK_MENU(mainwindow->replyall_popup), NULL, NULL,
		       menu_button_position, widget,
		       event->button, event->time);
	}
}

static void toolbar_reply_to_all_popup_closed_cb(GtkMenuShell *menu_shell, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	gtk_button_set_relief(GTK_BUTTON(mainwin->replyall_btn), GTK_RELIEF_NONE);
	manage_window_focus_in(mainwin->window, NULL, NULL);
}

static void toolbar_reply_to_sender_popup_cb(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	MainWindow *mainwindow = (MainWindow *) data;
	GtkWidget *replysender_menu, *replysender_item;

	if (!event) return;

	if (event->button == 3) {
		gtk_button_set_relief(GTK_BUTTON(widget), GTK_RELIEF_NORMAL);
		gtk_menu_popup(GTK_MENU(mainwindow->replysender_popup), NULL, NULL,
		       menu_button_position, widget,
		       event->button, event->time);
	}
}

static void toolbar_reply_to_sender_popup_closed_cb(GtkMenuShell *menu_shell, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	gtk_button_set_relief(GTK_BUTTON(mainwin->replysender_btn), GTK_RELIEF_NONE);
	manage_window_focus_in(mainwin->window, NULL, NULL);
}

static void toolbar_forward_popup_cb(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	MainWindow *mainwindow = (MainWindow *) data;
	
	if (!event) return;

	if (event->button == 3) {
		gtk_button_set_relief(GTK_BUTTON(widget), GTK_RELIEF_NORMAL);
		gtk_menu_popup(GTK_MENU(mainwindow->fwd_popup), NULL, NULL,
		       menu_button_position, widget,
		       event->button, event->time);
	}
}

static void toolbar_forward_popup_closed_cb(GtkMenuShell *menu_shell, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	gtk_button_set_relief(GTK_BUTTON(mainwin->fwd_btn), GTK_RELIEF_NONE);
	manage_window_focus_in(mainwin->window, NULL, NULL);
}

static void toolbar_inc_cb	(GtkWidget	*widget,
				 gpointer	 data)
{
	MainWindow *mainwin = (MainWindow *)data;

	inc_mail_cb(mainwin, 0, NULL);
}

static void toolbar_inc_all_cb	(GtkWidget	*widget,
				 gpointer	 data)
{
	MainWindow *mainwin = (MainWindow *)data;

	inc_all_account_mail_cb(mainwin, 0, NULL);
}

static void toolbar_send_cb	(GtkWidget	*widget,
				 gpointer	 data)
{
	MainWindow *mainwin = (MainWindow *)data;

	send_queue_cb(mainwin, 0, NULL);
}

static void toolbar_compose_cb	(GtkWidget	*widget,
				 gpointer	 data)
{
	MainWindow *mainwin = (MainWindow *)data;

	if (mainwin->compose_btn_type == COMPOSEBUTTON_MAIL)
		compose_cb(mainwin, 0, NULL);
	else
		compose_news_cb(mainwin, 0, NULL);
}

static void toolbar_compose_news_cb	(GtkWidget	*widget,
				 gpointer	 data)
{
	MainWindow *mainwin = (MainWindow *)data;

	compose_news_cb(mainwin, 0, NULL);
}

static void toolbar_compose_mail_cb	(GtkWidget	*widget,
				 gpointer	 data)
{
	MainWindow *mainwin = (MainWindow *)data;

	compose_mail_cb(mainwin, 0, NULL);
}

static void toolbar_reply_cb(GtkWidget *widget, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	reply_cb(mainwin, 
		 prefs_common.reply_with_quote ? COMPOSE_REPLY_WITH_QUOTE 
		 : COMPOSE_REPLY_WITHOUT_QUOTE,
		 NULL);
}

static void toolbar_reply_to_all_cb(GtkWidget *widget, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	reply_cb(mainwin, 
		 prefs_common.reply_with_quote ? COMPOSE_REPLY_TO_ALL_WITH_QUOTE 
		 : COMPOSE_REPLY_TO_ALL_WITHOUT_QUOTE, 
		 NULL);
}


static void toolbar_reply_to_sender_cb(GtkWidget *widget, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	reply_cb(mainwin, 
		 prefs_common.reply_with_quote ? COMPOSE_REPLY_TO_SENDER_WITH_QUOTE 
		 : COMPOSE_REPLY_TO_SENDER_WITHOUT_QUOTE, 
		 NULL);
}

static void toolbar_forward_cb	(GtkWidget	*widget,
				 gpointer	 data)
{
	MainWindow *mainwin = (MainWindow *)data;

	if (prefs_common.forward_as_attachment)
		reply_cb(mainwin, COMPOSE_FORWARD_AS_ATTACH, NULL);
	else
		reply_cb(mainwin, COMPOSE_FORWARD, NULL);
}

static void toolbar_delete_cb	(GtkWidget	*widget,
				 gpointer	 data)
{
	MainWindow *mainwin = (MainWindow *)data;

	summary_delete(mainwin->summaryview);
}

static void toolbar_exec_cb	(GtkWidget	*widget,
				 gpointer	 data)
{
	MainWindow *mainwin = (MainWindow *)data;

	summary_execute(mainwin->summaryview);
}

static void toolbar_next_unread_cb	(GtkWidget	*widget,
					 gpointer	 data)
{
	MainWindow *mainwin = (MainWindow *)data;

	next_unread_cb(mainwin, 0, NULL);
}

static void toolbar_prefs_cb	(GtkWidget	*widget,
				 gpointer	 data)
{
	prefs_common_open();
}

static void toolbar_account_cb	(GtkWidget	*widget,
				 gpointer	 data)
{
	MainWindow *mainwin = (MainWindow *)data;

	prefs_account_open_cb(mainwin, 0, NULL);
}

#if 0
static void toolbar_account_button_pressed(GtkWidget *widget,
					   GdkEventButton *event,
					   gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	if (!event) return;
	if (event->button != 3) return;

	gtk_button_set_relief(GTK_BUTTON(widget), GTK_RELIEF_NORMAL);
	gtk_object_set_data(GTK_OBJECT(mainwin->ac_menu), "menu_button",
			    widget);

	gtk_menu_popup(GTK_MENU(mainwin->ac_menu), NULL, NULL,
		       menu_button_position, widget,
		       event->button, event->time);
}
#endif

static void ac_label_button_pressed(GtkWidget *widget, GdkEventButton *event,
				    gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	if (!event) return;

	gtk_button_set_relief(GTK_BUTTON(widget), GTK_RELIEF_NORMAL);
	gtk_object_set_data(GTK_OBJECT(mainwin->ac_menu), "menu_button",
			    widget);

	gtk_menu_popup(GTK_MENU(mainwin->ac_menu), NULL, NULL,
		       menu_button_position, widget,
		       event->button, event->time);
}

static void ac_menu_popup_closed(GtkMenuShell *menu_shell, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	GtkWidget *button;

	button = gtk_object_get_data(GTK_OBJECT(menu_shell), "menu_button");
	if (!button) return;
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	gtk_object_remove_data(GTK_OBJECT(mainwin->ac_menu), "menu_button");
	manage_window_focus_in(mainwin->window, NULL, NULL);
}

static gint main_window_close_cb(GtkWidget *widget, GdkEventAny *event,
				 gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	if (mainwin->lock_count == 0)
		app_exit_cb(data, 0, widget);

	return TRUE;
}

static gint folder_window_close_cb(GtkWidget *widget, GdkEventAny *event,
				   gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	GtkWidget *menuitem;

	menuitem = gtk_item_factory_get_item
		(mainwin->menu_factory, "/View/Show or hide/Folder tree");
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), FALSE);

	return TRUE;
}

static gint message_window_close_cb(GtkWidget *widget, GdkEventAny *event,
				    gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	GtkWidget *menuitem;

	menuitem = gtk_item_factory_get_item
		(mainwin->menu_factory, "/View/Show or hide/Message view");
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), FALSE);

	return TRUE;
}

static void add_mailbox_cb(MainWindow *mainwin, guint action,
			   GtkWidget *widget)
{
	main_window_add_mailbox(mainwin);
}

static void add_mbox_cb(MainWindow *mainwin, guint action,
			GtkWidget *widget)
{
	main_window_add_mbox(mainwin);
}

static void update_folderview_cb(MainWindow *mainwin, guint action,
				 GtkWidget *widget)
{
	summary_show(mainwin->summaryview, NULL, FALSE);
	folderview_rescan_all();
}

static void new_folder_cb(MainWindow *mainwin, guint action,
			  GtkWidget *widget)
{
	folderview_new_folder(mainwin->folderview);
}

static void rename_folder_cb(MainWindow *mainwin, guint action,
			     GtkWidget *widget)
{
	folderview_rename_folder(mainwin->folderview);
}

static void delete_folder_cb(MainWindow *mainwin, guint action,
			     GtkWidget *widget)
{
	folderview_delete_folder(mainwin->folderview);
}

static void import_mbox_cb(MainWindow *mainwin, guint action,
			   GtkWidget *widget)
{
	import_mbox(mainwin->summaryview->folder_item);
}

static void export_mbox_cb(MainWindow *mainwin, guint action,
			   GtkWidget *widget)
{
	export_mbox(mainwin->summaryview->folder_item);
}

static void empty_trash_cb(MainWindow *mainwin, guint action,
			   GtkWidget *widget)
{
	main_window_empty_trash(mainwin, TRUE);
}

static void save_as_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	summary_save_as(mainwin->summaryview);
}

static void print_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	summary_print(mainwin->summaryview);
}

static void app_exit_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	if (prefs_common.confirm_on_exit) {
		if (alertpanel(_("Exit"), _("Exit this program?"),
			       _("OK"), _("Cancel"), NULL) != G_ALERTDEFAULT)
			return;
		manage_window_focus_in(mainwin->window, NULL, NULL);
	}

	app_will_exit(widget, mainwin);
}

static void search_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	if (action == 1)
		summary_search(mainwin->summaryview);
	else
		message_search(mainwin->messageview);
}

static void toggle_folder_cb(MainWindow *mainwin, guint action,
			     GtkWidget *widget)
{
	gboolean active;

	active = GTK_CHECK_MENU_ITEM(widget)->active;

	switch (mainwin->type) {
	case SEPARATE_NONE:
	case SEPARATE_MESSAGE:
#if 0
		if (active)
			gtk_widget_show(GTK_WIDGET_PTR(mainwin->folderview));
		else
			gtk_widget_hide(GTK_WIDGET_PTR(mainwin->folderview));
#endif
		break;
	case SEPARATE_FOLDER:
		debug_print("separate folder\n");
		if (active)
			gtk_widget_show(mainwin->win.sep_folder.folderwin);
		else
			gtk_widget_hide(mainwin->win.sep_folder.folderwin);
		break;
	case SEPARATE_BOTH:
		if (active)
			gtk_widget_show(mainwin->win.sep_both.folderwin);
		else
			gtk_widget_hide(mainwin->win.sep_both.folderwin);
		break;
	}
}

static void toggle_message_cb(MainWindow *mainwin, guint action,
			      GtkWidget *widget)
{
	gboolean active;

	active = GTK_CHECK_MENU_ITEM(widget)->active;

	if (active != messageview_is_visible(mainwin->messageview))
		summary_toggle_view(mainwin->summaryview);
}

static void toggle_toolbar_cb(MainWindow *mainwin, guint action,
			      GtkWidget *widget)
{
	switch ((ToolbarStyle)action) {
	case TOOLBAR_NONE:
		gtk_widget_hide(mainwin->handlebox);
	case TOOLBAR_ICON:
		gtk_toolbar_set_style(GTK_TOOLBAR(mainwin->toolbar),
				      GTK_TOOLBAR_ICONS);
		break;
	case TOOLBAR_TEXT:
		gtk_toolbar_set_style(GTK_TOOLBAR(mainwin->toolbar),
				      GTK_TOOLBAR_TEXT);
		break;
	case TOOLBAR_BOTH:
		gtk_toolbar_set_style(GTK_TOOLBAR(mainwin->toolbar),
				      GTK_TOOLBAR_BOTH);
		break;
	}

	if (action != TOOLBAR_NONE) {
		gtk_widget_show(mainwin->handlebox);
		gtk_widget_queue_resize(mainwin->handlebox);
	}

	mainwin->toolbar_style = (ToolbarStyle)action;
	prefs_common.toolbar_style = (ToolbarStyle)action;
}

static void toggle_statusbar_cb(MainWindow *mainwin, guint action,
				GtkWidget *widget)
{
	if (GTK_CHECK_MENU_ITEM(widget)->active) {
		gtk_widget_show(mainwin->hbox_stat);
		prefs_common.show_statusbar = TRUE;
	} else {
		gtk_widget_hide(mainwin->hbox_stat);
		prefs_common.show_statusbar = FALSE;
	}
}

static void separate_widget_cb(MainWindow *mainwin, guint action,
			       GtkWidget *widget)
{
	SeparateType type;

	if (GTK_CHECK_MENU_ITEM(widget)->active)
		type = mainwin->type | action;
	else
		type = mainwin->type & ~action;

	main_window_separation_change(mainwin, type);

	prefs_common.sep_folder = (type & SEPARATE_FOLDER)  != 0;
	prefs_common.sep_msg    = (type & SEPARATE_MESSAGE) != 0;
}

static void toggle_work_offline_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	if (GTK_CHECK_MENU_ITEM(widget)->active) {
		online_switch_clicked (GTK_BUTTON(mainwin->online_switch), mainwin);
	} else {
		online_switch_clicked (GTK_BUTTON(mainwin->offline_switch), mainwin);		
	}
}

static void online_switch_clicked (GtkButton *btn, gpointer data) 
{
	MainWindow *mainwin;
	GtkItemFactory *ifactory;
	GtkCheckMenuItem *menuitem;

	mainwin = (MainWindow *) data;
	
	ifactory = gtk_item_factory_from_widget(mainwin->menubar);
	menuitem = GTK_CHECK_MENU_ITEM (gtk_item_factory_get_widget(ifactory, "/File/Work offline"));
	
	g_return_if_fail(mainwin != NULL);
	g_return_if_fail(menuitem != NULL);
	
	if (btn == GTK_BUTTON(mainwin->online_switch)) {
		/* go offline */
		gtk_widget_hide (mainwin->online_switch);
		gtk_widget_show (mainwin->offline_switch);
		menuitem->active = TRUE;
		prefs_common.work_offline = TRUE;
		inc_autocheck_timer_remove();		
	} else {
		/*go online */
		gtk_widget_hide (mainwin->offline_switch);
		gtk_widget_show (mainwin->online_switch);
		menuitem->active = FALSE;
		prefs_common.work_offline = FALSE;
		inc_autocheck_timer_set();
	}
}

static void addressbook_open_cb(MainWindow *mainwin, guint action,
				GtkWidget *widget)
{
	addressbook_open(NULL);
}

static void log_window_show_cb(MainWindow *mainwin, guint action,
			       GtkWidget *widget)
{
	log_window_show(mainwin->logwin);
}

static void sel_download_cb(MainWindow *mainwin, guint action,
			       GtkWidget *widget)
{
	selective_download(mainwin);
}

static void inc_mail_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	inc_mail(mainwin, prefs_common.newmail_notify_manu);
}

static void inc_all_account_mail_cb(MainWindow *mainwin, guint action,
				    GtkWidget *widget)
{
	inc_all_account_mail(mainwin, prefs_common.newmail_notify_manu);
}

static void inc_cancel_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	inc_cancel_all();
}

static void send_queue_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	GList *list;

	if (prefs_common.work_offline)
		if (alertpanel(_("Offline warning"), 
			       _("You're working offline. Override?"),
			       _("Yes"), _("No"), NULL) != G_ALERTDEFAULT)
		return;

	for (list = folder_get_list(); list != NULL; list = list->next) {
		Folder *folder = list->data;

		if (folder->queue) {
			if (procmsg_send_queue
				(folder->queue, prefs_common.savemsg) < 0)
				alertpanel_error(_("Some errors occurred while sending queued messages."));
			statusbar_pop_all();
			folder_item_scan(folder->queue);
			folderview_update_item(folder->queue, TRUE);
		}
	}
}

static void compose_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	if (mainwin->summaryview->folder_item) {
		if (mainwin->summaryview->folder_item->folder->account != NULL
		    && mainwin->summaryview->folder_item->folder->account->protocol == A_NNTP)
			compose_new_with_recipient(mainwin->summaryview->folder_item->folder->account, mainwin->summaryview->folder_item->path);
		else
			compose_new_with_folderitem(mainwin->summaryview->folder_item->folder->account, mainwin->summaryview->folder_item);
	}
	else
		compose_new(NULL);
}

static void compose_mail_cb(MainWindow *mainwin, guint action,
			    GtkWidget *widget)
{
	PrefsAccount * ac;
	GList * list;
	GList * cur;

	if (mainwin->summaryview->folder_item) {
		ac = mainwin->summaryview->folder_item->folder->account;
		if (ac && ac->protocol != A_NNTP) {
			compose_new_with_folderitem(ac, mainwin->summaryview->folder_item);
			return;
		}
	}

	if(cur_account && (cur_account->protocol != A_NNTP)) {
		compose_new_with_folderitem(cur_account, mainwin->summaryview->folder_item);
		return;
	}

	list = account_get_list();
	for(cur = list ; cur != NULL ; cur = g_list_next(cur)) {
		ac = (PrefsAccount *) cur->data;
		if (ac->protocol != A_NNTP) {
			compose_new_with_folderitem(ac, mainwin->summaryview->folder_item);
			return;
		}
	}
}

static void compose_news_cb(MainWindow *mainwin, guint action,
			    GtkWidget *widget)
{
	PrefsAccount * ac = NULL;
	GList * list;
	GList * cur;

	if (mainwin->summaryview->folder_item) {
		ac = mainwin->summaryview->folder_item->folder->account;
		if (ac && ac->protocol == A_NNTP) {
			compose_new_with_recipient
				(ac, mainwin->summaryview->folder_item->path);
			return;
		}
	}

	list = account_get_list();
	for(cur = list ; cur != NULL ; cur = g_list_next(cur)) {
		ac = (PrefsAccount *) cur->data;
		if (ac->protocol == A_NNTP) {
			compose_new(ac);
			return;
		}
	}
}

static void reply_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	summary_reply(mainwin->summaryview, (ComposeMode)action);
}

static void move_to_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	summary_move_to(mainwin->summaryview);
}

static void copy_to_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	summary_copy_to(mainwin->summaryview);
}

static void delete_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	summary_delete(mainwin->summaryview);
}

static void open_msg_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	summary_open_msg(mainwin->summaryview);
}

static void view_source_cb(MainWindow *mainwin, guint action,
			   GtkWidget *widget)
{
	summary_view_source(mainwin->summaryview);
}

static void show_all_header_cb(MainWindow *mainwin, guint action,
			       GtkWidget *widget)
{
	if (mainwin->menu_lock_count) return;
	summary_display_msg_selected(mainwin->summaryview,
				     GTK_CHECK_MENU_ITEM(widget)->active);
}

static void reedit_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	summary_reedit(mainwin->summaryview);
}

static void mark_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	summary_mark(mainwin->summaryview);
}

static void unmark_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	summary_unmark(mainwin->summaryview);
}

static void mark_as_unread_cb(MainWindow *mainwin, guint action,
			      GtkWidget *widget)
{
	summary_mark_as_unread(mainwin->summaryview);
}

static void mark_as_read_cb(MainWindow *mainwin, guint action,
			    GtkWidget *widget)
{
	summary_mark_as_read(mainwin->summaryview);
}

static void mark_all_read_cb(MainWindow *mainwin, guint action,
			     GtkWidget *widget)
{
	summary_mark_all_read(mainwin->summaryview);
}

static void add_address_cb(MainWindow *mainwin, guint action,
			   GtkWidget *widget)
{
	summary_add_address(mainwin->summaryview);
}

static void set_charset_cb(MainWindow *mainwin, guint action,
			   GtkWidget *widget)
{
	const gchar *str;

	str = conv_get_charset_str((CharSet)action);
	g_free(prefs_common.force_charset);
	prefs_common.force_charset = str ? g_strdup(str) : NULL;

	summary_redisplay_msg(mainwin->summaryview);

	debug_print(_("forced charset: %s\n"), str ? str : "Auto-Detect");
}

static void hide_read_messages (MainWindow *mainwin, guint action,
				GtkWidget *widget)
{
	if (!mainwin->summaryview->folder_item
	    || gtk_object_get_data(GTK_OBJECT(widget), "dont_toggle"))
		return;
	summary_toggle_show_read_messages(mainwin->summaryview);
}

static void thread_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	if (mainwin->menu_lock_count) return;
	if (!mainwin->summaryview->folder_item) return;

	if (GTK_CHECK_MENU_ITEM(widget)->active) {
		summary_thread_build(mainwin->summaryview);
		mainwin->summaryview->folder_item->threaded = TRUE;
	} else {
		summary_unthread(mainwin->summaryview);
		mainwin->summaryview->folder_item->threaded = FALSE;
	}
}

static void expand_threads_cb(MainWindow *mainwin, guint action,
			      GtkWidget *widget)
{
	summary_expand_threads(mainwin->summaryview);
}

static void collapse_threads_cb(MainWindow *mainwin, guint action,
				GtkWidget *widget)
{
	summary_collapse_threads(mainwin->summaryview);
}

static void set_display_item_cb(MainWindow *mainwin, guint action,
				GtkWidget *widget)
{
	prefs_summary_column_open();
}

static void sort_summary_cb(MainWindow *mainwin, guint action,
			    GtkWidget *widget)
{
	FolderItem *item = mainwin->summaryview->folder_item;
	GtkWidget *menuitem;

	if (mainwin->menu_lock_count) return;
	if (item) {
		menuitem = gtk_item_factory_get_item
			(mainwin->menu_factory, "/View/Sort/Ascending");
		summary_sort(mainwin->summaryview, (FolderSortKey)action,
			     GTK_CHECK_MENU_ITEM(menuitem)->active
			     ? SORT_ASCENDING : SORT_DESCENDING);
	}
}

static void sort_summary_type_cb(MainWindow *mainwin, guint action,
				 GtkWidget *widget)
{
	FolderItem *item = mainwin->summaryview->folder_item;

	if (mainwin->menu_lock_count) return;
	if (item)
		summary_sort(mainwin->summaryview,
			     item->sort_key, (FolderSortType)action);
}

static void attract_by_subject_cb(MainWindow *mainwin, guint action,
				  GtkWidget *widget)
{
	summary_attract_by_subject(mainwin->summaryview);
}

static void delete_duplicated_cb(MainWindow *mainwin, guint action,
				 GtkWidget *widget)
{
	summary_delete_duplicated(mainwin->summaryview);
}

static void filter_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	summary_filter(mainwin->summaryview);
}

static void execute_summary_cb(MainWindow *mainwin, guint action,
			       GtkWidget *widget)
{
	summary_execute(mainwin->summaryview);
}

static void update_summary_cb(MainWindow *mainwin, guint action,
			      GtkWidget *widget)
{
	FolderItem *fitem;
	FolderView *folderview = mainwin->folderview;

	if (!mainwin->summaryview->folder_item) return;
	if (!folderview->opened) return;

	folder_update_op_count();

	fitem = gtk_ctree_node_get_row_data(GTK_CTREE(folderview->ctree),
					    folderview->opened);
	if (!fitem) return;

	folder_item_scan(fitem);
	summary_show(mainwin->summaryview, fitem, TRUE);
}

static void prev_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	summary_step(mainwin->summaryview, GTK_SCROLL_STEP_BACKWARD);
}

static void next_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	summary_step(mainwin->summaryview, GTK_SCROLL_STEP_FORWARD);
}

static void prev_unread_cb(MainWindow *mainwin, guint action,
			   GtkWidget *widget)
{
	summary_select_prev_unread(mainwin->summaryview);
}

static void next_unread_cb(MainWindow *mainwin, guint action,
			   GtkWidget *widget)
{
	summary_select_next_unread(mainwin->summaryview);
}

static void prev_new_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	summary_select_prev_new(mainwin->summaryview);
}

static void next_new_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	summary_select_next_new(mainwin->summaryview);
}

static void prev_marked_cb(MainWindow *mainwin, guint action,
			   GtkWidget *widget)
{
	summary_select_prev_marked(mainwin->summaryview);
}

static void next_marked_cb(MainWindow *mainwin, guint action,
			   GtkWidget *widget)
{
	summary_select_next_marked(mainwin->summaryview);
}

static void prev_labeled_cb(MainWindow *mainwin, guint action,
			    GtkWidget *widget)
{
	summary_select_prev_labeled(mainwin->summaryview);
}

static void next_labeled_cb(MainWindow *mainwin, guint action,
			    GtkWidget *widget)
{
	summary_select_next_labeled(mainwin->summaryview);
}

static void goto_folder_cb(MainWindow *mainwin, guint action,
			   GtkWidget *widget)
{
	FolderItem *to_folder;

	to_folder = foldersel_folder_sel(NULL, FOLDER_SEL_ALL, NULL);

	if (to_folder)
		folderview_select(mainwin->folderview, to_folder);
}

static void copy_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	messageview_copy_clipboard(mainwin->messageview);
}

static void allsel_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	MessageView *msgview = mainwin->messageview;

	if (GTK_WIDGET_HAS_FOCUS(mainwin->summaryview->ctree))
		summary_select_all(mainwin->summaryview);
	else if (messageview_is_visible(msgview) &&
		 (GTK_WIDGET_HAS_FOCUS(msgview->textview->text) ||
		  GTK_WIDGET_HAS_FOCUS(msgview->mimeview->textview->text)))
		messageview_select_all(mainwin->messageview);
}

static void selthread_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	if (messageview_is_visible(mainwin->summaryview->messageview))
		summary_select_thread(mainwin->summaryview);
}

static void create_filter_cb(MainWindow *mainwin, guint action,
			     GtkWidget *widget)
{
	summary_filter_open(mainwin->summaryview, (PrefsFilterType)action);
}

static void prefs_common_open_cb(MainWindow *mainwin, guint action,
				 GtkWidget *widget)
{
	prefs_common_open();
}

static void prefs_filter_open_cb(MainWindow *mainwin, guint action,
				 GtkWidget *widget)
{
	prefs_filter_open(NULL, NULL);
}

static void prefs_scoring_open_cb(MainWindow *mainwin, guint action,
				  GtkWidget *widget)
{
	prefs_scoring_open(NULL);
}

static void prefs_filtering_open_cb(MainWindow *mainwin, guint action,
				    GtkWidget *widget)
{
	prefs_filtering_open(NULL, NULL, NULL);
}

static void prefs_template_open_cb(MainWindow *mainwin, guint action,
				   GtkWidget *widget)
{
	prefs_template_open();
}

static void prefs_actions_open_cb(MainWindow *mainwin, guint action,
				  GtkWidget *widget)
{
	prefs_actions_open(mainwin);
}

static void prefs_account_open_cb(MainWindow *mainwin, guint action,
				  GtkWidget *widget)
{
	if (!cur_account) {
		new_account_cb(mainwin, 0, widget);
	} else {
		gboolean prev_default = cur_account->is_default;

		prefs_account_open(cur_account);
		if (!prev_default && cur_account->is_default)
			account_set_as_default(cur_account);
		account_save_config_all();
		account_set_menu();
		main_window_reflect_prefs_all();
	}
}

static void new_account_cb(MainWindow *mainwin, guint action,
			   GtkWidget *widget)
{
	account_edit_open();
	if (!compose_get_compose_list()) account_add();
}

static void account_menu_cb(GtkMenuItem	*menuitem, gpointer data)
{
	cur_account = (PrefsAccount *)data;
	main_window_reflect_prefs_all();
}

static void manual_open_cb(MainWindow *mainwin, guint action,
			   GtkWidget *widget)
{
	manual_open((ManualLang)action);
}

static void faq_open_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	faq_open((ManualLang)action);
}

static void scan_tree_func(Folder *folder, FolderItem *item, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	gchar *str;

	if (item->path)
		str = g_strdup_printf(_("Scanning folder %s%c%s ..."),
				      LOCAL_FOLDER(folder)->rootpath,
				      G_DIR_SEPARATOR,
				      item->path);
	else
		str = g_strdup_printf(_("Scanning folder %s ..."),
				      LOCAL_FOLDER(folder)->rootpath);

	STATUSBAR_PUSH(mainwin, str);
	STATUSBAR_POP(mainwin);
	g_free(str);
}

static void activate_compose_button (MainWindow *mainwin,
				ToolbarStyle style,
				ComposeButtonType type)
{
	SensitiveCond state = main_window_get_current_state(mainwin);

	if (style == TOOLBAR_NONE) 
		return;

	gtk_widget_hide(type == COMPOSEBUTTON_NEWS ? mainwin->compose_mail_btn 
		: mainwin->compose_news_btn);
	gtk_widget_show(type == COMPOSEBUTTON_NEWS ? mainwin->compose_news_btn
		: mainwin->compose_mail_btn);
	mainwin->compose_btn_type = type;	
}

void main_window_toolbar_set_compose_button(MainWindow *mainwin, ComposeButtonType compose_btn_type)
{
	if (mainwin->compose_btn_type != compose_btn_type)
		activate_compose_button(mainwin, 
					prefs_common.toolbar_style,
					compose_btn_type);
}

#define BREAK_ON_MODIFIER_KEY() \
	if ((event->state & (GDK_MOD1_MASK|GDK_CONTROL_MASK)) != 0) break

static void key_pressed (GtkWidget *widget, GdkEventKey *event,	gpointer data)
{
	MainWindow *mainwin = (MainWindow*) data;
	
	if (!mainwin || !event) return;
		
	switch (event->keyval) {
	case GDK_Q:             /* Quit */
		BREAK_ON_MODIFIER_KEY();

		app_exit_cb(mainwin, 0, NULL);
		return;
	default:
		break;
	}
}

#undef BREAK_ON_MODIFIER_KEY

static void set_toolbar_style(MainWindow *mainwin)
{
	switch (prefs_common.toolbar_style) {
	case TOOLBAR_NONE:
		gtk_widget_hide(mainwin->handlebox);
		break;
	case TOOLBAR_ICON:
		gtk_toolbar_set_style(GTK_TOOLBAR(mainwin->toolbar),
				      GTK_TOOLBAR_ICONS);
		break;
	case TOOLBAR_TEXT:
		gtk_toolbar_set_style(GTK_TOOLBAR(mainwin->toolbar),
				      GTK_TOOLBAR_TEXT);
		break;
	case TOOLBAR_BOTH:
		gtk_toolbar_set_style(GTK_TOOLBAR(mainwin->toolbar),
				      GTK_TOOLBAR_BOTH);
		break;
	}
	
	if (prefs_common.toolbar_style != TOOLBAR_NONE) {
		gtk_widget_show(mainwin->handlebox);
		gtk_widget_queue_resize(mainwin->handlebox);
	}
}

/*
 * Harvest addresses for selected folder.
 */
static void addr_harvest_cb( MainWindow *mainwin,
			    guint action,
			    GtkWidget *widget )
{
	addressbook_harvest( mainwin->summaryview->folder_item, FALSE, NULL );
}

/*
 * Harvest addresses for selected messages in summary view.
 */
static void addr_harvest_msg_cb( MainWindow *mainwin,
			    guint action,
			    GtkWidget *widget )
{
	summary_harvest_address( mainwin->summaryview );
}

/*
* End of Source.
*/

