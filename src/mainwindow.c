/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2007 Hiroyuki Yamamoto and the Claws Mail team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
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
#include <gtk/gtktooltips.h>
#include <string.h>

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
#include "log.h"
#include "compose.h"
#include "procmsg.h"
#include "import.h"
#include "export.h"
#include "edittags.h"
#include "prefs_common.h"
#include "prefs_actions.h"
#include "prefs_filtering.h"
#include "prefs_account.h"
#include "prefs_summary_column.h"
#include "prefs_folder_column.h"
#include "prefs_template.h"
#include "action.h"
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
#include "socket.h"

#define AC_LABEL_WIDTH	240

/* list of all instantiated MainWindow */
static GList *mainwin_list = NULL;

static GdkCursor *watch_cursor = NULL;
static GdkCursor *hand_cursor = NULL;

static gint iconified_count = 0;

static void main_window_menu_callback_block	(MainWindow	*mainwin);
static void main_window_menu_callback_unblock	(MainWindow	*mainwin);

static void main_window_show_cur_account	(MainWindow	*mainwin);

static void main_window_separation_change	(MainWindow	*mainwin,
						 LayoutType	 layout_mode);

static void main_window_set_widgets		(MainWindow	*mainwin,
						 LayoutType	 layout_mode);

static void toolbar_child_attached		(GtkWidget	*widget,
						 GtkWidget	*child,
						 gpointer	 data);
static void toolbar_child_detached		(GtkWidget	*widget,
						 GtkWidget	*child,
						 gpointer	 data);

static gboolean ac_label_button_pressed		(GtkWidget	*widget,
						 GdkEventButton	*event,
						 gpointer	 data);

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

static void update_folderview_cb (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void add_mailbox_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void foldersort_cb	 (MainWindow 	*mainwin,
				  guint		 action,
                        	  GtkWidget 	*widget);
static void import_mbox_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void export_mbox_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void export_list_mbox_cb  (MainWindow 	*mainwin, 
				  guint 	 action,
				  GtkWidget 	*widget);
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

static void toggle_message_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void toggle_toolbar_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void toggle_statusbar_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void set_layout_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);

static void addressbook_open_cb	(MainWindow	*mainwin,
				 guint		 action,
				 GtkWidget	*widget);
static void log_window_show_cb	(MainWindow	*mainwin,
				 guint		 action,
				 GtkWidget	*widget);
static void filtering_debug_window_show_cb	(MainWindow	*mainwin,
				 guint		 action,
				 GtkWidget	*widget);

static void inc_cancel_cb		(MainWindow	*mainwin,
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

static void hide_quotes_cb(MainWindow	*mainwin,
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
static void delete_trash_cb			(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);

static void cancel_cb                   (MainWindow     *mainwin,
					 guint           action,
					 GtkWidget      *widget);

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
static void mark_as_spam_cb		(MainWindow 	*mainwin, 
					 guint 		 action,
			     		 GtkWidget 	*widget);

static void ignore_thread_cb		(MainWindow 	*mainwin, 
					 guint 		 action,
			     		 GtkWidget 	*widget);
static void unignore_thread_cb		(MainWindow 	*mainwin, 
					 guint 		 action,
			     		 GtkWidget 	*widget);
static void lock_msgs_cb		(MainWindow 	*mainwin, 
					 guint 		 action,
			     		 GtkWidget 	*widget);
static void unlock_msgs_cb		(MainWindow 	*mainwin, 
					 guint 		 action,
			     		 GtkWidget 	*widget);

static void reedit_cb			(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);

static void add_address_cb		(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);

static void set_charset_cb		(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);

static void set_decode_cb		(MainWindow	*mainwin,
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

static void set_summary_display_item_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void set_folder_display_item_cb	 (MainWindow	*mainwin,
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
static void delete_duplicated_all_cb (MainWindow	*mainwin,
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
static void next_unread_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void prev_unread_cb	 (MainWindow	*mainwin,
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
static void last_read_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void parent_cb		 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);

static void goto_folder_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void goto_unread_folder_cb(MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);

static void copy_cb		 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void allsel_cb		 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void select_thread_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void delete_thread_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);

static void create_filter_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void create_processing_cb (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void open_urls_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);

static void prefs_template_open_cb	(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);
static void prefs_actions_open_cb	(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);
static void prefs_tags_open_cb		(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);
static void prefs_account_open_cb	(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);

static void prefs_pre_processing_open_cb  (MainWindow	*mainwin,
				  	   guint	 action,
				  	   GtkWidget	*widget);

static void prefs_post_processing_open_cb (MainWindow	*mainwin,
				  	   guint	 action,
				  	   GtkWidget	*widget);

static void prefs_filtering_open_cb 	(MainWindow	*mainwin,
				  	 guint		 action,
				  	 GtkWidget	*widget);
#ifdef USE_OPENSSL
static void ssl_manager_open_cb 	(MainWindow	*mainwin,
				  	 guint		 action,
				  	 GtkWidget	*widget);
#endif
static void new_account_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);

static void account_selector_menu_cb	 (GtkMenuItem	*menuitem,
					  gpointer	 data);
static void account_receive_menu_cb	 (GtkMenuItem	*menuitem,
					  gpointer	 data);
static void account_compose_menu_cb	 (GtkMenuItem	*menuitem,
					  gpointer	 data);

static void prefs_open_cb	(GtkMenuItem	*menuitem,
				 gpointer 	 data);
static void plugins_open_cb	(GtkMenuItem	*menuitem,
				 gpointer 	 data);

static void online_switch_clicked(GtkButton     *btn, 
				  gpointer data);

static void manual_open_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);

static void legend_open_cb	 (GtkMenuItem	*menuitem,
				  gpointer 	 data);

static void scan_tree_func	 (Folder	*folder,
				  FolderItem	*item,
				  gpointer	 data);
				  
static void toggle_work_offline_cb(MainWindow *mainwin, guint action, GtkWidget *widget);

static void addr_harvest_cb	 ( MainWindow  *mainwin,
				   guint       action,
				   GtkWidget   *widget );

static void addr_harvest_msg_cb	 ( MainWindow  *mainwin,
				   guint       action,
				   GtkWidget   *widget );
static void sync_cb		 ( MainWindow *mainwin, 
				   guint action, 
				   GtkWidget *widget );

static gboolean mainwindow_focus_in_event	(GtkWidget	*widget, 
						 GdkEventFocus	*focus,
						 gpointer	 data);
static gboolean mainwindow_visibility_event_cb	(GtkWidget	*widget, 
						 GdkEventVisibility	*state,
						 gpointer	 data);
static gboolean mainwindow_state_event_cb	(GtkWidget	*widget, 
						 GdkEventWindowState	*state,
						 gpointer	 data);
static void main_window_reply_cb			(MainWindow 	*mainwin, 
						 guint 		 action,
						 GtkWidget 	*widget);
static gboolean mainwindow_progressindicator_hook	(gpointer 	 source,
						 gpointer 	 userdata);

static gint mailing_list_create_submenu(GtkItemFactory *ifactory,
				       MsgInfo *msginfo);

static gint mailing_list_populate_submenu(GtkWidget *menu, const gchar * list_header);
	
static void get_url_part(const gchar **buf, gchar *url_decoded, gint maxlen);

static void mailing_list_compose(GtkWidget *w, gpointer *data);
 
static void mailing_list_open_uri(GtkWidget *w, gpointer *data);
#define  SEPARATE_ACTION 500 
static void mainwindow_quicksearch		(MainWindow 	*mainwin, 
						 guint 		 action, 
						 GtkWidget 	*widget);
static gboolean any_folder_want_synchronise(void);

static GtkItemFactoryEntry mainwin_entries[] =
{
	{N_("/_File"),				NULL, NULL, 0, "<Branch>"},
	{N_("/_File/_Add mailbox"),		NULL, NULL, 0, "<Branch>"},
	{N_("/_File/_Add mailbox/MH..."),	NULL, add_mailbox_cb, 0, NULL},
	{N_("/_File/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_File/Change folder order..."),	NULL, foldersort_cb,  0, NULL},
	{N_("/_File/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_File/_Import mbox file..."),	NULL, import_mbox_cb, 0, NULL},
	{N_("/_File/_Export to mbox file..."),	NULL, export_mbox_cb, 0, NULL},
	{N_("/_File/Exp_ort selected to mbox file..."),	
						NULL, export_list_mbox_cb, 0, NULL},
	{N_("/_File/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_File/Empty all _Trash folders"),	"<shift>D", empty_trash_cb, 0, NULL},
	{N_("/_File/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_File/_Save as..."),		"<control>S", save_as_cb, 0, NULL},
	{N_("/_File/_Print..."),		"<control>P", print_cb, 0, NULL},
	{N_("/_File/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_File/_Work offline"),		"<control>W", toggle_work_offline_cb, 0, "<ToggleItem>"},
	{N_("/_File/Synchronise folders"),   	"<control><shift>S", sync_cb, 0, NULL},
	{N_("/_File/---"),			NULL, NULL, 0, "<Separator>"},
	/* {N_("/_File/_Close"),		"<alt>W", app_exit_cb, 0, NULL}, */
	{N_("/_File/E_xit"),			"<control>Q", app_exit_cb, 0, NULL},

	{N_("/_Edit"),				NULL, NULL, 0, "<Branch>"},
	{N_("/_Edit/_Copy"),			"<control>C", copy_cb, 0, NULL},
	{N_("/_Edit/Select _all"),		"<control>A", allsel_cb, 0, NULL},
	{N_("/_Edit/Select _thread"),		NULL, select_thread_cb, 0, NULL},
	{N_("/_Edit/_Delete thread"),		NULL, delete_thread_cb, 0, NULL},
	{N_("/_Edit/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Edit/_Find in current message..."),
						"<control>F", search_cb, 0, NULL},
	{N_("/_Edit/_Search folder..."),	"<shift><control>F", search_cb, 1, NULL},
	{N_("/_Edit/_Quick search"),		"slash", mainwindow_quicksearch, 0, NULL},
	{N_("/_View"),				NULL, NULL, 0, "<Branch>"},
	{N_("/_View/Show or hi_de"),		NULL, NULL, 0, "<Branch>"},
	{N_("/_View/Show or hi_de/_Message view"),
						"V", toggle_message_cb, 0, "<ToggleItem>"},
	{N_("/_View/Show or hi_de/_Toolbar"),
						NULL, NULL, 0, "<Branch>"},
	{N_("/_View/Show or hi_de/_Toolbar/Text _below icons"),
						NULL, toggle_toolbar_cb, TOOLBAR_BOTH, "<RadioItem>"},
	{N_("/_View/Show or hi_de/_Toolbar/Text be_side icons"),
						NULL, toggle_toolbar_cb, TOOLBAR_BOTH_HORIZ, "/View/Show or hide/Toolbar/Text below icons"},
	{N_("/_View/Show or hi_de/_Toolbar/_Icons only"),
						NULL, toggle_toolbar_cb, TOOLBAR_ICON, "/View/Show or hide/Toolbar/Text below icons"},
	{N_("/_View/Show or hi_de/_Toolbar/_Text only"),
						NULL, toggle_toolbar_cb, TOOLBAR_TEXT, "/View/Show or hide/Toolbar/Text below icons"},
	{N_("/_View/Show or hi_de/_Toolbar/_Hide"),
						NULL, toggle_toolbar_cb, TOOLBAR_NONE, "/View/Show or hide/Toolbar/Text below icons"},
	{N_("/_View/Show or hi_de/Status _bar"),
						NULL, toggle_statusbar_cb, 0, "<ToggleItem>"},
	{N_("/_View/Set displayed _columns"),	NULL, NULL, 0, "<Branch>"},
	{N_("/_View/Set displayed _columns/in _Folder list..."),	NULL, set_folder_display_item_cb, 0, NULL},
	{N_("/_View/Set displayed _columns/in _Message list..."),NULL, set_summary_display_item_cb, 0, NULL},

	{N_("/_View/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_View/La_yout"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_View/Layout/_Standard"),		NULL, set_layout_cb, NORMAL_LAYOUT, "<RadioItem>"},
	{N_("/_View/Layout/_Three columns"),	NULL, set_layout_cb, VERTICAL_LAYOUT, "/View/Layout/Standard"},
	{N_("/_View/Layout/_Wide message"),	NULL, set_layout_cb, WIDE_LAYOUT, "/View/Layout/Standard"},
	{N_("/_View/Layout/W_ide message list"),NULL, set_layout_cb, WIDE_MSGLIST_LAYOUT, "/View/Layout/Standard"},
	{N_("/_View/Layout/S_mall screen"),	NULL, set_layout_cb, SMALL_LAYOUT, "/View/Layout/Standard"},
	{N_("/_View/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_View/_Sort"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_View/_Sort/by _number"),		NULL, sort_summary_cb, SORT_BY_NUMBER, "<RadioItem>"},
	{N_("/_View/_Sort/by S_ize"),		NULL, sort_summary_cb, SORT_BY_SIZE, "/View/Sort/by number"},
	{N_("/_View/_Sort/by _Date"),		NULL, sort_summary_cb, SORT_BY_DATE, "/View/Sort/by number"},
	{N_("/_View/_Sort/by Thread date"),	NULL, sort_summary_cb, SORT_BY_THREAD_DATE, "/View/Sort/by number"},
	{N_("/_View/_Sort/by _From"),		NULL, sort_summary_cb, SORT_BY_FROM, "/View/Sort/by number"},
	{N_("/_View/_Sort/by _To"),		NULL, sort_summary_cb, SORT_BY_TO, "/View/Sort/by number"},
	{N_("/_View/_Sort/by S_ubject"),	NULL, sort_summary_cb, SORT_BY_SUBJECT, "/View/Sort/by number"},
	{N_("/_View/_Sort/by _color label"),	NULL, sort_summary_cb, SORT_BY_LABEL, "/View/Sort/by number"},
	{N_("/_View/_Sort/by tag"),		NULL, sort_summary_cb, SORT_BY_TAGS, "/View/Sort/by number"},
	{N_("/_View/_Sort/by _mark"),		NULL, sort_summary_cb, SORT_BY_MARK, "/View/Sort/by number"},
	{N_("/_View/_Sort/by _status"),		NULL, sort_summary_cb, SORT_BY_STATUS, "/View/Sort/by number"},
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

	{N_("/_View/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_View/_Go to"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_View/_Go to/_Previous message"),	"P", prev_cb, 0, NULL},
	{N_("/_View/_Go to/_Next message"),	"N", next_cb, 0, NULL},
	{N_("/_View/_Go to/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_View/_Go to/P_revious unread message"),
						"<shift>P", prev_unread_cb, 0, NULL},
	{N_("/_View/_Go to/N_ext unread message"),
						"<shift>N", next_unread_cb, 0, NULL},
	{N_("/_View/_Go to/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_View/_Go to/Previous ne_w message"),	NULL, prev_new_cb, 0, NULL},
	{N_("/_View/_Go to/Ne_xt new message"),	NULL, next_new_cb, 0, NULL},
	{N_("/_View/_Go to/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_View/_Go to/Previous _marked message"),
						NULL, prev_marked_cb, 0, NULL},
	{N_("/_View/_Go to/Next m_arked message"),
						NULL, next_marked_cb, 0, NULL},
	{N_("/_View/_Go to/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_View/_Go to/Previous _labeled message"),
						NULL, prev_labeled_cb, 0, NULL},
	{N_("/_View/_Go to/Next la_beled message"),
						NULL, next_labeled_cb, 0, NULL},
	{N_("/_View/_Go to/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_View/_Go to/Last read message"),
						NULL, last_read_cb, 0, NULL},
	{N_("/_View/_Go to/Parent message"),
						"<control>Up", parent_cb, 0, NULL},
	{N_("/_View/_Go to/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_View/_Go to/Next unread _folder"),	"<shift>G", goto_unread_folder_cb, 0, NULL},
	{N_("/_View/_Go to/_Other folder..."),	"G", goto_folder_cb, 0, NULL},
	{N_("/_View/---"),			NULL, NULL, 0, "<Separator>"},

#define ENC_SEPARATOR \
	{N_("/_View/Character _encoding/---"),		NULL, NULL, 0, "<Separator>"}
#define ENC_ACTION(action) \
	 NULL, set_charset_cb, action, "/View/Character encoding/Auto detect"

	{N_("/_View/Character _encoding"),		NULL, NULL, 0, "<Branch>"},
	{N_("/_View/Character _encoding/_Auto detect"),
	 NULL, set_charset_cb, C_AUTO, "<RadioItem>"},
	{N_("/_View/Character _encoding/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_View/Character _encoding/7bit ascii (US-ASC_II)"),
	 ENC_ACTION(C_US_ASCII)},
	{N_("/_View/Character _encoding/Unicode (_UTF-8)"),
	 ENC_ACTION(C_UTF_8)},
	ENC_SEPARATOR,

	{N_("/_View/Character _encoding/Western European (ISO-8859-_1)"),
	 ENC_ACTION(C_ISO_8859_1)},
	{N_("/_View/Character _encoding/Western European (ISO-8859-15)"),
	 ENC_ACTION(C_ISO_8859_15)},
	{N_("/_View/Character _encoding/Western European (Windows-1252)"),
	 ENC_ACTION(C_WINDOWS_1252)},
	ENC_SEPARATOR,

	{N_("/_View/Character _encoding/Central European (ISO-8859-_2)"),
	 ENC_ACTION(C_ISO_8859_2)},
	ENC_SEPARATOR,

	{N_("/_View/Character _encoding/_Baltic (ISO-8859-13)"),
	 ENC_ACTION(C_ISO_8859_13)},
	{N_("/_View/Character _encoding/Baltic (ISO-8859-_4)"),
	 ENC_ACTION(C_ISO_8859_4)},
	ENC_SEPARATOR,

	{N_("/_View/Character _encoding/Greek (ISO-8859-_7)"),
	 ENC_ACTION(C_ISO_8859_7)},
	ENC_SEPARATOR,

	{N_("/_View/Character _encoding/Hebrew (ISO-8859-_8)"),
	 ENC_ACTION(C_ISO_8859_8)},
	{N_("/_View/Character _encoding/Hebrew (Windows-1255)"),
	 ENC_ACTION(C_CP1255)},
	ENC_SEPARATOR,

	{N_("/_View/Character _encoding/Arabic (ISO-8859-_6)"),
	 ENC_ACTION(C_ISO_8859_6)},
	{N_("/_View/Character _encoding/Arabic (Windows-1256)"),
	 ENC_ACTION(C_CP1256)},
	ENC_SEPARATOR,

	{N_("/_View/Character _encoding/Turkish (ISO-8859-_9)"),
	 ENC_ACTION(C_ISO_8859_9)},
	ENC_SEPARATOR,

	{N_("/_View/Character _encoding/Cyrillic (ISO-8859-_5)"),
	 ENC_ACTION(C_ISO_8859_5)},
	{N_("/_View/Character _encoding/Cyrillic (KOI8-_R)"),
	 ENC_ACTION(C_KOI8_R)},
	{N_("/_View/Character _encoding/Cyrillic (KOI8-U)"),
	 ENC_ACTION(C_KOI8_U)},
	{N_("/_View/Character _encoding/Cyrillic (Windows-1251)"),
	 ENC_ACTION(C_CP1251)},
	ENC_SEPARATOR,

	{N_("/_View/Character _encoding/Japanese (ISO-2022-_JP)"),
	 ENC_ACTION(C_ISO_2022_JP)},
	{N_("/_View/Character _encoding/Japanese (ISO-2022-JP-2)"),
	 ENC_ACTION(C_ISO_2022_JP_2)},
	{N_("/_View/Character _encoding/Japanese (_EUC-JP)"),
	 ENC_ACTION(C_EUC_JP)},
	{N_("/_View/Character _encoding/Japanese (_Shift__JIS)"),
	 ENC_ACTION(C_SHIFT_JIS)},
	ENC_SEPARATOR,

	{N_("/_View/Character _encoding/Simplified Chinese (_GB2312)"),
	 ENC_ACTION(C_GB2312)},
	{N_("/_View/Character _encoding/Simplified Chinese (GBK)"),
	 ENC_ACTION(C_GBK)},
	{N_("/_View/Character _encoding/Traditional Chinese (_Big5)"),
	 ENC_ACTION(C_BIG5)},
	{N_("/_View/Character _encoding/Traditional Chinese (EUC-_TW)"),
	 ENC_ACTION(C_EUC_TW)},
	{N_("/_View/Character _encoding/Chinese (ISO-2022-_CN)"),
	 ENC_ACTION(C_ISO_2022_CN)},
	ENC_SEPARATOR,

	{N_("/_View/Character _encoding/Korean (EUC-_KR)"),
	 ENC_ACTION(C_EUC_KR)},
	{N_("/_View/Character _encoding/Korean (ISO-2022-KR)"),
	 ENC_ACTION(C_ISO_2022_KR)},
	ENC_SEPARATOR,

	{N_("/_View/Character _encoding/Thai (TIS-620)"),
	 ENC_ACTION(C_TIS_620)},
	{N_("/_View/Character _encoding/Thai (Windows-874)"),
	 ENC_ACTION(C_WINDOWS_874)},

#undef ENC_SEPARATOR
#undef ENC_ACTION

#define DEC_SEPARATOR \
	{N_("/_View/Decode/---"),		NULL, NULL, 0, "<Separator>"}
#define DEC_ACTION(action) \
	 NULL, set_decode_cb, action, "/View/Decode/Auto detect"
	{N_("/_View/Decode"),		NULL, NULL, 0, "<Branch>"},
	{N_("/_View/Decode/_Auto detect"),
	 NULL, set_decode_cb, 0, "<RadioItem>"},
	{N_("/_View/Decode/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_View/Decode/_8bit"), 		DEC_ACTION(ENC_8BIT)},
	{N_("/_View/Decode/_Quoted printable"),	DEC_ACTION(ENC_QUOTED_PRINTABLE)},
	{N_("/_View/Decode/_Base64"), 		DEC_ACTION(ENC_BASE64)},
	{N_("/_View/Decode/_Uuencode"),		DEC_ACTION(ENC_X_UUENCODE)},

#undef DEC_SEPARATOR
#undef DEC_ACTION

	{N_("/_View/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_View/Open in new _window"),	"<control><alt>N", open_msg_cb, 0, NULL},
	{N_("/_View/Mess_age source"),		"<control>U", view_source_cb, 0, NULL},
	{N_("/_View/All headers"),		"<control>H", show_all_header_cb, 0, "<ToggleItem>"},
	{N_("/_View/Quotes"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_View/Quotes/_Fold all"),		"<control><shift>Q", hide_quotes_cb, 1, "<ToggleItem>"},
	{N_("/_View/Quotes/Fold from level _2"),NULL, hide_quotes_cb, 2, "<ToggleItem>"},
	{N_("/_View/Quotes/Fold from level _3"),NULL, hide_quotes_cb, 3, "<ToggleItem>"},
	{N_("/_View/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_View/_Update summary"),		"<control><alt>U", update_summary_cb,  0, NULL},

	{N_("/_Message"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_Message/Recei_ve"),		NULL, NULL, 0, "<Branch>"},
	{N_("/_Message/Recei_ve/Get from _current account"),
						"<control>I",	inc_mail_cb, 0, NULL},
	{N_("/_Message/Recei_ve/Get from _all accounts"),
						"<shift><control>I", inc_all_account_mail_cb, 0, NULL},
	{N_("/_Message/Recei_ve/Cancel receivin_g"),
						NULL, inc_cancel_cb, 0, NULL},
	{N_("/_Message/Recei_ve/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/_Send queued messages"), NULL, send_queue_cb, 0, NULL},
	{N_("/_Message/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/Compose a_n email message"),	"<control>M", compose_mail_cb, 0, NULL},
	{N_("/_Message/Compose a news message"),	NULL,	compose_news_cb, 0, NULL},
	{N_("/_Message/_Reply"),		"<control>R", 	main_window_reply_cb, COMPOSE_REPLY, NULL},
	{N_("/_Message/Repl_y to"),		NULL, NULL, 0, "<Branch>"},
	{N_("/_Message/Repl_y to/_all"),	"<shift><control>R", main_window_reply_cb, COMPOSE_REPLY_TO_ALL, NULL},
	{N_("/_Message/Repl_y to/_sender"),	NULL, main_window_reply_cb, COMPOSE_REPLY_TO_SENDER, NULL},
	{N_("/_Message/Repl_y to/mailing _list"),
						"<control>L", main_window_reply_cb, COMPOSE_REPLY_TO_LIST, NULL},
	{N_("/_Message/Follow-up and reply to"),NULL, main_window_reply_cb, COMPOSE_FOLLOWUP_AND_REPLY_TO, NULL},
	{N_("/_Message/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/_Forward"),		"<control><alt>F", main_window_reply_cb, COMPOSE_FORWARD_INLINE, NULL},
	{N_("/_Message/For_ward as attachment"),	NULL, main_window_reply_cb, COMPOSE_FORWARD_AS_ATTACH, NULL},
	{N_("/_Message/Redirect"),		NULL, main_window_reply_cb, COMPOSE_REDIRECT, NULL},

	{N_("/_Message/Mailing-_List"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_Message/Mailing-_List/Post"),		NULL, NULL, 0, "<Branch>"},
	{N_("/_Message/Mailing-_List/Help"),		NULL, NULL, 0, "<Branch>"},
 	{N_("/_Message/Mailing-_List/Subscribe"),	NULL, NULL, 0, "<Branch>"},
 	{N_("/_Message/Mailing-_List/Unsubscribe"),	NULL, NULL, 0, "<Branch>"},
	{N_("/_Message/Mailing-_List/View archive"),	NULL, NULL, 0, "<Branch>"},
 	{N_("/_Message/Mailing-_List/Contact owner"),	NULL, NULL, 0, "<Branch>"},
 	
	{N_("/_Message/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/M_ove..."),		"<control>O", move_to_cb, 0, NULL},
	{N_("/_Message/_Copy..."),		"<shift><control>O", copy_to_cb, 0, NULL},
	{N_("/_Message/Move to _trash"),	"<control>D", delete_trash_cb,  0, NULL},
	{N_("/_Message/_Delete..."),		NULL, delete_cb,  0, NULL},
	{N_("/_Message/Cancel a news message"), "", cancel_cb,  0, NULL},
	{N_("/_Message/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/_Mark"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_Message/_Mark/_Mark"),		"<shift>asterisk", mark_cb, 0, NULL},
	{N_("/_Message/_Mark/_Unmark"),		"U", unmark_cb, 0, NULL},
	{N_("/_Message/_Mark/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/_Mark/Mark as unr_ead"),	"<shift>exclam", mark_as_unread_cb, 0, NULL},
	{N_("/_Message/_Mark/Mark as rea_d"),	NULL, mark_as_read_cb, 0, NULL},
	{N_("/_Message/_Mark/Mark all _read"),	NULL, mark_all_read_cb, 0, NULL},
	{N_("/_Message/_Mark/Ignore thread"),	NULL, ignore_thread_cb, 0, NULL},
	{N_("/_Message/_Mark/Unignore thread"),	NULL, unignore_thread_cb, 0, NULL},
	{N_("/_Message/_Mark/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/_Mark/Mark as _spam"),	NULL, mark_as_spam_cb, 1, NULL},
	{N_("/_Message/_Mark/Mark as _ham"),	NULL, mark_as_spam_cb, 0, NULL},
	{N_("/_Message/_Mark/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/_Mark/Lock"),		NULL, lock_msgs_cb, 0, NULL},
	{N_("/_Message/_Mark/Unlock"),		NULL, unlock_msgs_cb, 0, NULL},
	{N_("/_Message/Color la_bel"),		NULL, NULL, 	       0, NULL},
	{N_("/_Message/T_ags"),			NULL, NULL, 	       0, NULL},
	{N_("/_Message/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/Re-_edit"),		NULL, reedit_cb, 0, NULL},

	{N_("/_Tools"),				NULL, NULL, 0, "<Branch>"},
	{N_("/_Tools/_Address book..."),	"<shift><control>A", addressbook_open_cb, 0, NULL},
	{N_("/_Tools/Add sender to address boo_k"),
						NULL, add_address_cb, 0, NULL},
	{N_("/_Tools/_Harvest addresses"),	NULL, NULL, 0, "<Branch>"},
	{N_("/_Tools/_Harvest addresses/from _Folder..."),
						NULL, addr_harvest_cb, 0, NULL},
	{N_("/_Tools/_Harvest addresses/from _Messages..."),
						NULL, addr_harvest_msg_cb, 0, NULL},
	{N_("/_Tools/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Tools/_Filter all messages in folder"),
						NULL, filter_cb, 0, NULL},
	{N_("/_Tools/Filter _selected messages"),
						NULL, filter_cb, 1, NULL},
	{N_("/_Tools/_Create filter rule"),	NULL, NULL, 0, "<Branch>"},
	{N_("/_Tools/_Create filter rule/_Automatically"),
						NULL, create_filter_cb, FILTER_BY_AUTO, NULL},
	{N_("/_Tools/_Create filter rule/by _From"),
						NULL, create_filter_cb, FILTER_BY_FROM, NULL},
	{N_("/_Tools/_Create filter rule/by _To"),
						NULL, create_filter_cb, FILTER_BY_TO, NULL},
	{N_("/_Tools/_Create filter rule/by _Subject"),
						NULL, create_filter_cb, FILTER_BY_SUBJECT, NULL},
	{N_("/_Tools/C_reate processing rule"),	NULL, NULL, 0, "<Branch>"},
	{N_("/_Tools/C_reate processing rule/_Automatically"),
						NULL, create_processing_cb, FILTER_BY_AUTO, NULL},
	{N_("/_Tools/C_reate processing rule/by _From"),
						NULL, create_processing_cb, FILTER_BY_FROM, NULL},
	{N_("/_Tools/C_reate processing rule/by _To"),
						NULL, create_processing_cb, FILTER_BY_TO, NULL},
	{N_("/_Tools/C_reate processing rule/by _Subject"),
						NULL, create_processing_cb, FILTER_BY_SUBJECT, NULL},
	{N_("/_Tools/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Tools/List _URLs..."),		"<shift><control>U", open_urls_cb, 0, NULL},
	{N_("/_Tools/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Tools/Actio_ns"),		NULL, NULL, 0, "<Branch>"},
	{N_("/_Tools/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Tools/Ch_eck for new messages in all folders"),
						NULL, update_folderview_cb, 0, NULL},
	{N_("/_Tools/Delete du_plicated messages"),
						NULL, NULL, 0, "<Branch>"},
	{N_("/_Tools/Delete du_plicated messages/In selected folder"),
						NULL, delete_duplicated_cb,   0, NULL},
	{N_("/_Tools/Delete du_plicated messages/In all folders"),
						NULL, delete_duplicated_all_cb,   0, NULL},
	{N_("/_Tools/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Tools/E_xecute"),		"X", execute_summary_cb, 0, NULL},
#ifdef USE_OPENSSL
	{N_("/_Tools/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Tools/SSL cer_tificates..."),	
						NULL, ssl_manager_open_cb, 0, NULL},
#endif
	{N_("/_Tools/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Tools/Filtering Log"),		NULL, filtering_debug_window_show_cb, 0, NULL},
	{N_("/_Tools/Network _Log"),		"<shift><control>L", log_window_show_cb, 0, NULL},

	{N_("/_Configuration"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_Configuration/C_hange current account"),
						NULL, NULL, 0, "<Branch>"},
	{N_("/_Configuration/_Preferences for current account..."),
						NULL, prefs_account_open_cb, 0, NULL},
	{N_("/_Configuration/Create _new account..."),
						NULL, new_account_cb, 0, NULL},
	{N_("/_Configuration/_Edit accounts..."),
						NULL, account_edit_open, 0, NULL},
	{N_("/_Configuration/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Configuration/P_references..."),
						NULL, prefs_open_cb, 0, NULL},
	{N_("/_Configuration/Pre-pr_ocessing..."),
						NULL, prefs_pre_processing_open_cb, 0, NULL},
	{N_("/_Configuration/Post-pro_cessing..."),
						NULL, prefs_post_processing_open_cb, 0, NULL},
	{N_("/_Configuration/_Filtering..."),
						NULL, prefs_filtering_open_cb, 0, NULL},
	{N_("/_Configuration/_Templates..."),	NULL, prefs_template_open_cb, 0, NULL},
	{N_("/_Configuration/_Actions..."),	NULL, prefs_actions_open_cb, 0, NULL},
	{N_("/_Configuration/Tag_s..."),	NULL, prefs_tags_open_cb, 0, NULL},
	{N_("/_Configuration/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Configuration/Plu_gins..."),  	NULL, plugins_open_cb, 0, NULL},

	{N_("/_Help"),				NULL, NULL, 0, "<Branch>"},
	{N_("/_Help/_Manual"),			NULL, manual_open_cb, MANUAL_MANUAL_LOCAL, NULL},
	{N_("/_Help/_Online User-contributed FAQ"),	
						NULL, manual_open_cb, MANUAL_FAQ_CLAWS, NULL},
	{N_("/_Help/Icon _Legend"),		NULL, legend_open_cb, 0, NULL},
	{N_("/_Help/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Help/_About"),			NULL, about_show, 0, NULL}
};

static gboolean offline_ask_sync = TRUE;
static guint lastkey;
static gboolean is_obscured = FALSE;

static gboolean main_window_accel_activate (GtkAccelGroup *accelgroup,
                                            GObject *arg1,
                                            guint value,
                                            GdkModifierType mod,
                                            gpointer user_data) 
{
	MainWindow *mainwin = (MainWindow *)user_data;

	if (mainwin->summaryview &&
	    mainwin->summaryview->quicksearch &&
	    quicksearch_has_focus(mainwin->summaryview->quicksearch) &&
	    (mod == 0 || mod == GDK_SHIFT_MASK)) {
		quicksearch_pass_key(mainwin->summaryview->quicksearch, lastkey, mod);
		return TRUE;
	}
	return FALSE;
}

#define N_COLOR_LABELS colorlabel_get_color_count()

static void mainwindow_colorlabel_menu_item_activate_item_cb(GtkMenuItem *menu_item,
							  gpointer data)
{
	MainWindow *mainwin;
	GtkMenuShell *menu;
	GtkCheckMenuItem **items;
	gint n;
	GList *cur;
	GSList *sel;

	mainwin = (MainWindow *)data;
	g_return_if_fail(mainwin != NULL);

	sel = summary_get_selection(mainwin->summaryview);
	if (!sel) return;

	menu = GTK_MENU_SHELL(mainwin->colorlabel_menu);
	g_return_if_fail(menu != NULL);

	Xalloca(items, (N_COLOR_LABELS + 1) * sizeof(GtkWidget *), return);

	/* NOTE: don't return prematurely because we set the "dont_toggle"
	 * state for check menu items */
	g_object_set_data(G_OBJECT(menu), "dont_toggle",
			  GINT_TO_POINTER(1));

	/* clear items. get item pointers. */
	for (n = 0, cur = menu->children; cur != NULL && cur->data != NULL; cur = cur->next) {
		if (GTK_IS_CHECK_MENU_ITEM(cur->data)) {
			gtk_check_menu_item_set_active
				(GTK_CHECK_MENU_ITEM(cur->data), FALSE);
			items[n] = GTK_CHECK_MENU_ITEM(cur->data);
			n++;
		}
	}

	if (n == (N_COLOR_LABELS + 1)) {
		/* iterate all messages and set the state of the appropriate
		 * items */
		for (; sel != NULL; sel = sel->next) {
			MsgInfo *msginfo;
			gint clabel;

			msginfo = (MsgInfo *)sel->data;
			if (msginfo) {
				clabel = MSG_GET_COLORLABEL_VALUE(msginfo->flags);
				if (!items[clabel]->active)
					gtk_check_menu_item_set_active
						(items[clabel], TRUE);
			}
		}
	} else
		g_warning("invalid number of color elements (%d)\n", n);

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
	g_return_if_fail(mainwin != NULL);

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
	GList *cur;
	GSList *sel;
	GHashTable *menu_table = g_hash_table_new_full(
					g_direct_hash,
					g_direct_equal,
					NULL, NULL);

	mainwin = (MainWindow *)data;
	g_return_if_fail(mainwin != NULL);

	sel = summary_get_selection(mainwin->summaryview);
	if (!sel) return;

	menu = GTK_MENU_SHELL(mainwin->tags_menu);
	g_return_if_fail(menu != NULL);

	/* NOTE: don't return prematurely because we set the "dont_toggle"
	 * state for check menu items */
	g_object_set_data(G_OBJECT(menu), "dont_toggle",
			  GINT_TO_POINTER(1));

	/* clear items. get item pointers. */
	for (cur = menu->children; cur != NULL && cur->data != NULL; cur = cur->next) {
		if (GTK_IS_CHECK_MENU_ITEM(cur->data)) {
			gint id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(cur->data),
				"tag_id"));
			gtk_check_menu_item_set_active
				(GTK_CHECK_MENU_ITEM(cur->data), FALSE);
				
			g_hash_table_insert(menu_table, GINT_TO_POINTER(id), GTK_CHECK_MENU_ITEM(cur->data));
		}
	}

	/* iterate all messages and set the state of the appropriate
	 * items */
	for (; sel != NULL; sel = sel->next) {
		MsgInfo *msginfo;
		GSList *tags = NULL;
		gint id;
		GtkCheckMenuItem *item;
		msginfo = (MsgInfo *)sel->data;
		if (msginfo) {
			tags =  msginfo->tags;
			if (!tags)
				continue;

			for (; tags; tags = tags->next) {
				id = GPOINTER_TO_INT(tags->data);
				item = g_hash_table_lookup(menu_table, GINT_TO_POINTER(tags->data));
				if (item && !item->active)
					gtk_check_menu_item_set_active
						(item, TRUE);
			}
		}
	}

	g_slist_free(sel);
	g_hash_table_destroy(menu_table);
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
	g_return_if_fail(mainwin != NULL);

	/* "dont_toggle" state set? */
	if (g_object_get_data(G_OBJECT(mainwin->tags_menu),
				"dont_toggle"))
		return;

	if (!set)
		id = -id;
	summary_set_tag(mainwin->summaryview, id, NULL);
}

static void mainwindow_colorlabel_menu_create(MainWindow *mainwin, gboolean refresh)
{
	GtkWidget *label_menuitem;
	GtkWidget *menu;
	GtkWidget *item;
	gint i;

	label_menuitem = gtk_item_factory_get_item(mainwin->menu_factory,
						   "/Message/Color label");
	g_signal_connect(G_OBJECT(label_menuitem), "activate",
			 G_CALLBACK(mainwindow_colorlabel_menu_item_activate_item_cb),
			   mainwin);
	gtk_widget_show(label_menuitem);

	menu = gtk_menu_new();

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

	gtk_widget_add_accelerator(item, "activate", 
				   mainwin->menu_factory->accel_group, 
				   GDK_0, GDK_CONTROL_MASK,
				   GTK_ACCEL_LOCKED | GTK_ACCEL_VISIBLE);

	item = gtk_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_widget_show(item);

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
		if (i < 9)
			gtk_widget_add_accelerator(item, "activate", 
				   mainwin->menu_factory->accel_group, 
				   GDK_1+i, GDK_CONTROL_MASK,
				   GTK_ACCEL_LOCKED | GTK_ACCEL_VISIBLE);
	}
	gtk_widget_show(menu);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(label_menuitem), menu);
	mainwin->colorlabel_menu = menu;
}

static void mainwindow_tags_menu_item_new_tag_activate_cb(GtkWidget *widget,
						     gpointer data)
{
	MainWindow *mainwin;
	gint id;
	mainwin = g_object_get_data(G_OBJECT(widget), "mainwin");
	g_return_if_fail(mainwin != NULL);

	/* "dont_toggle" state set? */
	if (g_object_get_data(G_OBJECT(mainwin->tags_menu),
				"dont_toggle"))
		return;
	
	id = prefs_tags_create_new(mainwin);
	if (id != -1) {
		summary_set_tag(mainwin->summaryview, id, NULL);
		main_window_reflect_tags_changes(mainwindow_get_mainwindow());
	}
}

static void mainwindow_tags_menu_create(MainWindow *mainwin, gboolean refresh)
{
	GtkWidget *label_menuitem;
	GtkWidget *menu;
	GtkWidget *item;
	GSList *cur = tags_get_list();
	GSList *orig = cur;
	gboolean existing_tags = FALSE;

	label_menuitem = gtk_item_factory_get_item(mainwin->menu_factory,
						   "/Message/Tags");
	g_signal_connect(G_OBJECT(label_menuitem), "activate",
			 G_CALLBACK(mainwindow_tags_menu_item_activate_item_cb),
			   mainwin);

	gtk_widget_show(label_menuitem);

	menu = gtk_menu_new();

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
		existing_tags = TRUE;
	}
	if (existing_tags) {
		/* separator */
		item = gtk_menu_item_new();
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		gtk_widget_show(item);
	}
	item = gtk_menu_item_new_with_label(_("New tag..."));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	g_signal_connect(G_OBJECT(item), "activate",
			 G_CALLBACK(mainwindow_tags_menu_item_new_tag_activate_cb),
			 NULL);
	g_object_set_data(G_OBJECT(item), "mainwin",
			  mainwin);
	gtk_widget_show(item);
	g_slist_free(orig);
	gtk_widget_show(menu);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(label_menuitem), menu);
	mainwin->tags_menu = menu;
}

static gboolean warning_icon_pressed(GtkWidget *widget, GdkEventButton *evt,
				    MainWindow *mainwindow)
{
	if (evt && evt->button == 1) {
		log_window_show_error(mainwindow->logwin);
		gtk_widget_hide(mainwindow->warning_btn);
	}
	return FALSE;
}

static gboolean warning_visi_notify(GtkWidget *widget,
				       GdkEventVisibility *event,
				       MainWindow *mainwindow)
{
	gdk_window_set_cursor(mainwindow->warning_btn->window, hand_cursor);
	return FALSE;
}

static gboolean warning_leave_notify(GtkWidget *widget,
				      GdkEventCrossing *event,
				      MainWindow *mainwindow)
{
	gdk_window_set_cursor(mainwindow->warning_btn->window, NULL);
	return FALSE;
}

static gboolean warning_enter_notify(GtkWidget *widget,
				      GdkEventCrossing *event,
				      MainWindow *mainwindow)
{
	gdk_window_set_cursor(mainwindow->warning_btn->window, hand_cursor);
	return FALSE;
}

void mainwindow_show_error(void)
{
	MainWindow *mainwin = mainwindow_get_mainwindow();
	gtk_widget_show(mainwin->warning_btn);
}

void mainwindow_clear_error(MainWindow *mainwin)
{
	gtk_widget_hide(mainwin->warning_btn);
}

MainWindow *main_window_create()
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
 	GtkWidget *online_pixmap;
	GtkWidget *offline_pixmap;
	GtkWidget *online_switch;
	GtkWidget *offline_switch;
	GtkTooltips *tips;
	GtkWidget *warning_icon;
	GtkWidget *warning_btn;

	FolderView *folderview;
	SummaryView *summaryview;
	MessageView *messageview;
	GdkColormap *colormap;
	GdkColor color[4];
	gboolean success[4];
	GtkItemFactory *ifactory;
	GtkWidget *ac_menu;
	GtkWidget *menuitem;
	gint i;
	guint n_menu_entries;

	static GdkGeometry geometry;

	debug_print("Creating main window...\n");
	mainwin = g_new0(MainWindow, 1);

	/* main window */
	window = gtkut_window_new(GTK_WINDOW_TOPLEVEL, "mainwindow");
	gtk_window_set_title(GTK_WINDOW(window), PROG_VERSION);
	gtk_window_set_resizable(GTK_WINDOW(window), TRUE);

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

/*	gtk_widget_show(gtk_item_factory_get_item(ifactory,"/Message/Mailing-List"));
	main_create_mailing_list_menu (mainwin, NULL); */

	menu_set_sensitive(ifactory, "/Help/Manual", manual_available(MANUAL_MANUAL_LOCAL));

	if (prefs_common.toolbar_detachable) {
		handlebox = gtk_handle_box_new();
		gtk_widget_show(handlebox);
		gtk_box_pack_start(GTK_BOX(vbox), handlebox, FALSE, FALSE, 0);
		g_signal_connect(G_OBJECT(handlebox), "child_attached",
				 G_CALLBACK(toolbar_child_attached), mainwin);
		g_signal_connect(G_OBJECT(handlebox), "child_detached",
				 G_CALLBACK(toolbar_child_detached), mainwin);
	} else {
		handlebox = gtk_hbox_new(FALSE, 0);
		gtk_widget_show(handlebox);
		gtk_box_pack_start(GTK_BOX(vbox), handlebox, FALSE, FALSE, 0);
	}
	/* link window to mainwin->window to avoid gdk warnings */
	mainwin->window       = window;
	
	/* create toolbar */
	mainwin->toolbar = toolbar_create(TOOLBAR_MAIN, 
					  handlebox, 
					  (gpointer)mainwin);
	toolbar_set_learn_button
		(mainwin->toolbar,
		 LEARN_SPAM);

	/* vbox that contains body */
	vbox_body = gtk_vbox_new(FALSE, BORDER_WIDTH);
	gtk_widget_show(vbox_body);
	gtk_container_set_border_width(GTK_CONTAINER(vbox_body), BORDER_WIDTH);
	gtk_box_pack_start(GTK_BOX(vbox), vbox_body, TRUE, TRUE, 0);

	hbox_stat = gtk_hbox_new(FALSE, 2);
	gtk_box_pack_end(GTK_BOX(vbox_body), hbox_stat, FALSE, FALSE, 0);

	tips = gtk_tooltips_new();

	warning_icon = gtk_image_new_from_stock
                        (GTK_STOCK_DIALOG_WARNING, GTK_ICON_SIZE_SMALL_TOOLBAR);
	warning_btn = gtk_event_box_new();
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(warning_btn), FALSE);
	
	mainwin->warning_btn      = warning_btn;
	
	g_signal_connect(G_OBJECT(warning_btn), "button-press-event", 
			 G_CALLBACK(warning_icon_pressed),
			 (gpointer) mainwin);
	g_signal_connect(G_OBJECT(warning_btn), "visibility-notify-event",
			 G_CALLBACK(warning_visi_notify), mainwin);
	g_signal_connect(G_OBJECT(warning_btn), "motion-notify-event",
			 G_CALLBACK(warning_visi_notify), mainwin);
	g_signal_connect(G_OBJECT(warning_btn), "leave-notify-event",
			 G_CALLBACK(warning_leave_notify), mainwin);
	g_signal_connect(G_OBJECT(warning_btn), "enter-notify-event",
			 G_CALLBACK(warning_enter_notify), mainwin);

	gtk_container_add (GTK_CONTAINER(warning_btn), warning_icon);

	gtk_tooltips_set_tip(GTK_TOOLTIPS(tips),warning_btn, 
			     _("Some error(s) happened. Click here to view log."), NULL);
	gtk_box_pack_start(GTK_BOX(hbox_stat), warning_btn, FALSE, FALSE, 0);

	statusbar = statusbar_create();
	gtk_box_pack_start(GTK_BOX(hbox_stat), statusbar, TRUE, TRUE, 0);

	progressbar = gtk_progress_bar_new();
	gtk_widget_set_size_request(progressbar, 120, 1);
	gtk_box_pack_start(GTK_BOX(hbox_stat), progressbar, FALSE, FALSE, 0);

	online_pixmap = stock_pixmap_widget(hbox_stat, STOCK_PIXMAP_ONLINE);
	offline_pixmap = stock_pixmap_widget(hbox_stat, STOCK_PIXMAP_OFFLINE);
	online_switch = gtk_button_new ();
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tips),online_switch, 
			     _("You are online. Click the icon to go offline"), NULL);
	offline_switch = gtk_button_new ();
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tips),offline_switch, 
			     _("You are offline. Click the icon to go online"),
			     NULL);
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
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tips),
			     ac_button, _("Select account"), NULL);
	GTK_WIDGET_UNSET_FLAGS(ac_button, GTK_CAN_FOCUS);
	gtk_widget_set_size_request(ac_button, -1, 0);
	gtk_box_pack_end(GTK_BOX(hbox_stat), ac_button, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(ac_button), "button_press_event",
			 G_CALLBACK(ac_label_button_pressed), mainwin);

	ac_label = gtk_label_new("");
	gtk_container_add(GTK_CONTAINER(ac_button), ac_label);

	gtk_widget_show_all(hbox_stat);

	gtk_widget_hide(offline_switch);
	gtk_widget_hide(warning_btn);

	/* create views */
	mainwin->folderview  = folderview  = folderview_create();
	mainwin->summaryview = summaryview = summary_create();
	mainwin->messageview = messageview = messageview_create(mainwin);

	/* init log instances data before creating log views */
	set_log_title(LOG_PROTOCOL, _("Network log"));
	set_log_prefs(LOG_PROTOCOL,
			&prefs_common.logwin_width,
			&prefs_common.logwin_height);
	set_log_title(LOG_DEBUG_FILTERING, _("Filtering/processing debug log"));
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

	messageview->statusbar   = statusbar;
	mainwin->vbox           = vbox;
	mainwin->menubar        = menubar;
	mainwin->menu_factory   = ifactory;
	mainwin->handlebox      = handlebox;
	mainwin->vbox_body      = vbox_body;
	mainwin->hbox_stat      = hbox_stat;
	mainwin->statusbar      = statusbar;
	mainwin->progressbar    = progressbar;
	mainwin->statuslabel    = statuslabel;
	mainwin->online_switch  = online_switch;
	mainwin->online_pixmap  = online_pixmap;
	mainwin->offline_pixmap = offline_pixmap;
	mainwin->ac_button      = ac_button;
	mainwin->ac_label       = ac_label;
	mainwin->offline_switch    = offline_switch;
	
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

	/* allocate colors for summary view and folder view */
	summaryview->color_marked.red = summaryview->color_marked.green = 0;
	summaryview->color_marked.blue = (guint16)65535;

	summaryview->color_dim.red = summaryview->color_dim.green =
		summaryview->color_dim.blue = COLOR_DIM;

	gtkut_convert_int_to_gdk_color(prefs_common.color_new,
				       &folderview->color_new);

	gtkut_convert_int_to_gdk_color(prefs_common.tgt_folder_col,
				       &folderview->color_op);

	summaryview->color_important.red = 0;
	summaryview->color_important.green = 0;
	summaryview->color_important.blue = (guint16)65535;

	color[0] = summaryview->color_marked;
	color[1] = summaryview->color_dim;
	color[2] = folderview->color_new;
	color[3] = folderview->color_op;

	colormap = gdk_drawable_get_colormap(window->window);
	gdk_colormap_alloc_colors(colormap, color, 4, FALSE, TRUE, success);
	for (i = 0; i < 4; i++) {
		if (success[i] == FALSE)
			g_warning("MainWindow: color allocation %d failed\n", i);
	}

	debug_print("done.\n");

	messageview->visible = prefs_common.msgview_visible;

	main_window_set_widgets(mainwin, prefs_common.layout_mode);

	g_signal_connect(G_OBJECT(window), "size_allocate",
			 G_CALLBACK(main_window_size_allocate_cb),
			 mainwin);

	/* set menu items */
	menuitem = gtk_item_factory_get_item
		(ifactory, "/View/Character encoding/Auto detect");
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), TRUE);

	switch (prefs_common.toolbar_style) {
	case TOOLBAR_NONE:
		menuitem = gtk_item_factory_get_item
			(ifactory, "/View/Show or hide/Toolbar/Hide");
		break;
	case TOOLBAR_ICON:
		menuitem = gtk_item_factory_get_item
			(ifactory, "/View/Show or hide/Toolbar/Icons only");
		break;
	case TOOLBAR_TEXT:
		menuitem = gtk_item_factory_get_item
			(ifactory, "/View/Show or hide/Toolbar/Text only");
		break;
	case TOOLBAR_BOTH:
		menuitem = gtk_item_factory_get_item
			(ifactory, "/View/Show or hide/Toolbar/Text below icons");
		break;
	case TOOLBAR_BOTH_HORIZ:
		menuitem = gtk_item_factory_get_item
			(ifactory,
			 "/View/Show or hide/Toolbar/Text beside icons");
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
	mainwin->ac_menu = ac_menu;

	toolbar_main_set_sensitive(mainwin);

	/* create actions menu */
	main_window_update_actions_menu(mainwin);

	main_create_mailing_list_menu (mainwin, NULL);

	/* attach accel groups to main window */
#define	ADD_MENU_ACCEL_GROUP_TO_WINDOW(menu,win)			\
	gtk_window_add_accel_group					\
		(GTK_WINDOW(win), 					\
		 gtk_item_factory_from_widget(menu)->accel_group); 	\
	g_signal_connect(G_OBJECT(gtk_item_factory_from_widget(menu)->accel_group), \
			"accel_activate", 				\
		       	G_CALLBACK(main_window_accel_activate), mainwin);
			 
	
	ADD_MENU_ACCEL_GROUP_TO_WINDOW(summaryview->popupmenu, mainwin->window);
	
	/* connect the accelerators for equivalent 
	   menu items in different menus             */
	menu_connect_identical_items();

	gtk_window_iconify(GTK_WINDOW(mainwin->window));

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
	
	gtk_widget_show(mainwin->window);

	/* initialize views */
	folderview_init(folderview);
	summary_init(summaryview);
	messageview_init(messageview);
#ifdef USE_OPENSSL
	sslcertwindow_register_hook();
#endif
	mainwin->lock_count = 0;
	mainwin->menu_lock_count = 0;
	mainwin->cursor_count = 0;

	mainwin->progressindicator_hook =
		hooks_register_hook(PROGRESSINDICATOR_HOOKLIST, mainwindow_progressindicator_hook, mainwin);

	if (!watch_cursor)
		watch_cursor = gdk_cursor_new(GDK_WATCH);
	if (!hand_cursor)
		hand_cursor = gdk_cursor_new(GDK_HAND2);

	mainwin_list = g_list_append(mainwin_list, mainwin);

	/* init work_offline */
	if (prefs_common.work_offline)
		online_switch_clicked (GTK_BUTTON(online_switch), mainwin);

	mainwindow_colorlabel_menu_create(mainwin, FALSE);
	mainwindow_tags_menu_create(mainwin, FALSE);
	
	return mainwin;
}

void main_window_destroy(MainWindow *mainwin)
{
	/* TODO : destroy other component */
	messageview_destroy(mainwin->messageview);
	mainwin->messageview = NULL;	
}

void main_window_update_actions_menu(MainWindow *mainwin)
{
	GtkItemFactory *ifactory;

	ifactory = gtk_item_factory_from_widget(mainwin->menubar);
	action_update_mainwin_menu(ifactory, "/Tools/Actions", mainwin);
}

void main_window_cursor_wait(MainWindow *mainwin)
{

	if (mainwin->cursor_count == 0) {
		gdk_window_set_cursor(mainwin->window->window, watch_cursor);
		textview_cursor_wait(mainwin->messageview->mimeview->textview);
	}
	
	mainwin->cursor_count++;

	gdk_flush();
}

void main_window_cursor_normal(MainWindow *mainwin)
{
	if (mainwin->cursor_count)
		mainwin->cursor_count--;

	if (mainwin->cursor_count == 0) {
		gdk_window_set_cursor(mainwin->window->window, NULL);
		textview_cursor_normal(mainwin->messageview->mimeview->textview);
	}
	gdk_flush();
}

/* lock / unlock the user-interface */
void main_window_lock(MainWindow *mainwin)
{
	if (mainwin->lock_count == 0)
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
	GtkWidget *pixmap;

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
			folderview_reflect_prefs_pixmap_theme(mainwin->folderview);
			summary_reflect_prefs_pixmap_theme(mainwin->summaryview);

			pixmap = stock_pixmap_widget(mainwin->hbox_stat, STOCK_PIXMAP_ONLINE);
			gtk_container_remove(GTK_CONTAINER(mainwin->online_switch), 
					     mainwin->online_pixmap);
			gtk_container_add (GTK_CONTAINER(mainwin->online_switch), pixmap);
			gtk_widget_show(pixmap);
			mainwin->online_pixmap = pixmap;
			pixmap = stock_pixmap_widget(mainwin->hbox_stat, STOCK_PIXMAP_OFFLINE);
			gtk_container_remove(GTK_CONTAINER(mainwin->offline_switch), 
					     mainwin->offline_pixmap);
			gtk_container_add (GTK_CONTAINER(mainwin->offline_switch), pixmap);
			gtk_widget_show(pixmap);
			mainwin->offline_pixmap = pixmap;
		}
		
		headerview_set_font(mainwin->messageview->headerview);
		headerview_set_visibility(mainwin->messageview->headerview,
					  prefs_common.display_header_pane);
		textview_reflect_prefs(mainwin->messageview->mimeview->textview);
		folderview_reflect_prefs();
		summary_reflect_prefs();
		summary_redisplay_msg(mainwin->summaryview);
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
	GList *cur;

	/* re-create colorlabel submenu */
	menu = GTK_MENU_SHELL(mainwin->colorlabel_menu);
	g_return_if_fail(menu != NULL);

	/* clear items. get item pointers. */
	for (cur = menu->children; cur != NULL && cur->data != NULL; cur = cur->next) {
		gtk_menu_item_remove_submenu(GTK_MENU_ITEM(cur->data));
	}
	mainwindow_colorlabel_menu_create(mainwin, TRUE);
	summary_reflect_prefs_custom_colors(mainwin->summaryview);

}

void main_window_reflect_tags_changes(MainWindow *mainwin)
{
	GtkMenuShell *menu;
	GList *cur;

	/* re-create tags submenu */
	menu = GTK_MENU_SHELL(mainwin->tags_menu);
	g_return_if_fail(menu != NULL);

	/* clear items. get item pointers. */
	for (cur = menu->children; cur != NULL && cur->data != NULL; cur = cur->next) {
		gtk_menu_item_remove_submenu(GTK_MENU_ITEM(cur->data));
	}
	mainwindow_tags_menu_create(mainwin, TRUE);
	summary_reflect_tags_changes(mainwin->summaryview);

}

void main_window_reflect_prefs_all_real(gboolean pixmap_theme_changed)
{
	if (prefs_tag == 0 || pixmap_theme_changed) {
		prefs_tag = g_timeout_add(500, reflect_prefs_timeout_cb, 
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
	GList *cur_ac, *cur_item;
	GtkWidget *menuitem;
	PrefsAccount *ac_prefs;

	/* destroy all previous menu item */
	cur_item = GTK_MENU_SHELL(mainwin->ac_menu)->children;
	while (cur_item != NULL) {
		GList *next = cur_item->next;
		gtk_widget_destroy(GTK_WIDGET(cur_item->data));
		cur_item = next;
	}

	for (cur_ac = account_list; cur_ac != NULL; cur_ac = cur_ac->next) {
		ac_prefs = (PrefsAccount *)cur_ac->data;

		menuitem = gtk_menu_item_new_with_label
			(ac_prefs->account_name
			 ? ac_prefs->account_name : _("Untitled"));
		gtk_widget_show(menuitem);
		gtk_menu_append(GTK_MENU(mainwin->ac_menu), menuitem);
		g_signal_connect(G_OBJECT(menuitem), "activate",
				 G_CALLBACK(account_selector_menu_cb),
				 ac_prefs);
	}
}

static void main_window_set_account_receive_menu(MainWindow *mainwin,
						 GList *account_list)
{
	GList *cur_ac, *cur_item;
	GtkWidget *menu;
	GtkWidget *menuitem;
	PrefsAccount *ac_prefs;

	menu = gtk_item_factory_get_widget(mainwin->menu_factory,
					   "/Message/Receive");

	/* search for separator */
	for (cur_item = GTK_MENU_SHELL(menu)->children; cur_item != NULL;
	     cur_item = cur_item->next) {
		if (GTK_BIN(cur_item->data)->child == NULL) {
			cur_item = cur_item->next;
			break;
		}
	}

	/* destroy all previous menu item */
	while (cur_item != NULL) {
		GList *next = cur_item->next;
		gtk_widget_destroy(GTK_WIDGET(cur_item->data));
		cur_item = next;
	}

	for (cur_ac = account_list; cur_ac != NULL; cur_ac = cur_ac->next) {
		ac_prefs = (PrefsAccount *)cur_ac->data;

		menuitem = gtk_menu_item_new_with_label
			(ac_prefs->account_name ? ac_prefs->account_name
			 : _("Untitled"));
		gtk_widget_show(menuitem);
		gtk_menu_append(GTK_MENU(menu), menuitem);
		g_signal_connect(G_OBJECT(menuitem), "activate",
				 G_CALLBACK(account_receive_menu_cb),
				 ac_prefs);
	}
}

static void main_window_set_toolbar_combo_receive_menu(MainWindow *mainwin,
						       GList *account_list)
{
	GList *cur_ac, *cur_item;
	GtkWidget *menuitem;
	PrefsAccount *ac_prefs;
	GtkWidget *menu = NULL;

	if (mainwin->toolbar->getall_btn == NULL
	||  mainwin->toolbar->getall_combo == NULL) /* button doesn't exist */
		return;

	menu = mainwin->toolbar->getall_combo->menu;

	/* destroy all previous menu item */
	cur_item = GTK_MENU_SHELL(menu)->children;
	while (cur_item != NULL) {
		GList *next = cur_item->next;
		gtk_widget_destroy(GTK_WIDGET(cur_item->data));
		cur_item = next;
	}

	for (cur_ac = account_list; cur_ac != NULL; cur_ac = cur_ac->next) {
		ac_prefs = (PrefsAccount *)cur_ac->data;

		menuitem = gtk_menu_item_new_with_label
			(ac_prefs->account_name
			 ? ac_prefs->account_name : _("Untitled"));
		gtk_widget_show(menuitem);
		gtk_menu_append(GTK_MENU(menu), menuitem);
		g_signal_connect(G_OBJECT(menuitem), "activate",
				 G_CALLBACK(account_receive_menu_cb),
				 ac_prefs);
	}
}

static void main_window_set_toolbar_combo_compose_menu(MainWindow *mainwin,
						       GList *account_list)
{
	GList *cur_ac, *cur_item;
	GtkWidget *menuitem;
	PrefsAccount *ac_prefs;
	GtkWidget *menu = NULL;

	if (mainwin->toolbar->compose_mail_btn == NULL
	||  mainwin->toolbar->compose_combo == NULL) /* button doesn't exist */
		return;

	menu = mainwin->toolbar->compose_combo->menu;

	/* destroy all previous menu item */
	cur_item = GTK_MENU_SHELL(menu)->children;
	while (cur_item != NULL) {
		GList *next = cur_item->next;
		gtk_widget_destroy(GTK_WIDGET(cur_item->data));
		cur_item = next;
	}

	for (cur_ac = account_list; cur_ac != NULL; cur_ac = cur_ac->next) {
		ac_prefs = (PrefsAccount *)cur_ac->data;

		menuitem = gtk_menu_item_new_with_label
			(ac_prefs->account_name
			 ? ac_prefs->account_name : _("Untitled"));
		gtk_widget_show(menuitem);
		gtk_menu_append(GTK_MENU(menu), menuitem);
		g_signal_connect(G_OBJECT(menuitem), "activate",
				 G_CALLBACK(account_compose_menu_cb),
				 ac_prefs);
	}
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

	gtk_label_set_text(GTK_LABEL(mainwin->ac_label), ac_name);
	gtk_widget_queue_resize(mainwin->ac_button);

	g_free(ac_name);
}

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
	gtk_widget_ref(folder_wid);
	gtk_widget_ref(summary_wid);
	gtk_widget_ref(message_wid);
	gtkut_container_remove
		(GTK_CONTAINER(folder_wid->parent), folder_wid);
	gtkut_container_remove
		(GTK_CONTAINER(summary_wid->parent), summary_wid);
	gtkut_container_remove
		(GTK_CONTAINER(message_wid->parent), message_wid);

	gtk_widget_hide(mainwin->window);
	main_window_set_widgets(mainwin, layout_mode);
	gtk_widget_show(mainwin->window);

	gtk_widget_unref(folder_wid);
	gtk_widget_unref(summary_wid);
	gtk_widget_unref(message_wid);
}

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
		if (ppaned->parent != NULL) {
			mainwin->messageview->visible = FALSE;
			summaryview->displayed = NULL;
			gtk_widget_ref(ppaned);
			gtkut_container_remove(GTK_CONTAINER(container), ppaned);
			gtk_widget_reparent(GTK_WIDGET_PTR(summaryview), container);
		} else {
			mainwin->messageview->visible = TRUE;
			gtk_widget_reparent(GTK_WIDGET_PTR(summaryview), ppaned);
			gtk_container_add(GTK_CONTAINER(container), ppaned);
			gtk_widget_unref(ppaned);
		}
		break;
	case WIDE_LAYOUT:
		ppaned = mainwin->hpaned;
		container = mainwin->vpaned;
		if (mainwin->messageview->vbox->parent != NULL) {
			mainwin->messageview->visible = FALSE;
			summaryview->displayed = NULL;
			gtk_widget_ref(mainwin->messageview->vbox);
			gtkut_container_remove(GTK_CONTAINER(container), mainwin->messageview->vbox);
		} else {
			mainwin->messageview->visible = TRUE;
			gtk_container_add(GTK_CONTAINER(container), mainwin->messageview->vbox);
			gtk_widget_unref(mainwin->messageview->vbox);
		}
		break;
	case WIDE_MSGLIST_LAYOUT:
		g_warning("can't hide messageview in this wide msglist layout");
		break;
	}

	if (messageview_is_visible(mainwin->messageview))
		gtk_arrow_set(GTK_ARROW(mainwin->summaryview->toggle_arrow),
			      GTK_ARROW_DOWN, GTK_SHADOW_OUT);
	else
		gtk_arrow_set(GTK_ARROW(mainwin->summaryview->toggle_arrow),
			      GTK_ARROW_UP, GTK_SHADOW_OUT);

	if (mainwin->messageview->visible == FALSE)
		messageview_clear(mainwin->messageview);

	main_window_set_menu_sensitive(mainwin);

	prefs_common.msgview_visible = mainwin->messageview->visible;

	if (messageview_is_visible(mainwin->messageview)) {
		gtk_widget_queue_resize(mainwin->hpaned);
		gtk_widget_queue_resize(mainwin->vpaned);
	}
	summary_grab_focus(summaryview);
}

void main_window_get_size(MainWindow *mainwin)
{
	GtkAllocation *allocation;

	if (mainwin_list == NULL || mainwin->messageview == NULL) {
		debug_print("called after messageview "
			    "has been deallocated!\n");
		return;
	}

	allocation = &(GTK_WIDGET_PTR(mainwin->summaryview)->allocation);

	if (allocation->width > 1 && allocation->height > 1) {
		prefs_common.summaryview_width = allocation->width;

		if (messageview_is_visible(mainwin->messageview))
			prefs_common.summaryview_height = allocation->height;

		prefs_common.mainview_width = allocation->width;
	}

	allocation = &mainwin->window->allocation;
	if (allocation->width > 1 && allocation->height > 1) {
		prefs_common.mainview_height = allocation->height;
		prefs_common.mainwin_width   = allocation->width;
		prefs_common.mainwin_height  = allocation->height;
	}

	allocation = &(GTK_WIDGET_PTR(mainwin->folderview)->allocation);
	if (allocation->width > 1 && allocation->height > 1) {
		prefs_common.folderview_width  = allocation->width;
		prefs_common.folderview_height = allocation->height;
	}

	allocation = &(GTK_WIDGET_PTR(mainwin->messageview)->allocation);
	if (allocation->width > 1 && allocation->height > 1) {
		prefs_common.msgview_width = allocation->width;
		prefs_common.msgview_height = allocation->height;
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

	gtkut_widget_get_uposition(mainwin->window, &x, &y);

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

void main_window_progress_set(MainWindow *mainwin, gint cur, gint total)
{
	gchar buf[32];

	g_snprintf(buf, sizeof(buf), "%d / %d", cur, total);
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(mainwin->progressbar), buf);
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(mainwin->progressbar),
				(total == 0) ? 0 : (gfloat)cur / (gfloat)total);
}

void main_window_empty_trash(MainWindow *mainwin, gboolean confirm)
{
	if (confirm && procmsg_have_trashed_mails_fast()) {
		if (alertpanel(_("Empty trash"),
			       _("Delete all messages in trash folders?"),
			       GTK_STOCK_NO, "+" GTK_STOCK_YES, NULL)
		    != G_ALERTALTERNATE)
			return;
		manage_window_focus_in(mainwin->window, NULL, NULL);
	}

	procmsg_empty_all_trash();

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
			      "If an existing mailbox is specified, it will be\n"
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

SensitiveCond main_window_get_current_state(MainWindow *mainwin)
{
	SensitiveCond state = 0;
	SummarySelection selection;
	FolderItem *item = mainwin->summaryview->folder_item;
	GList *account_list = account_get_list();
	GSList *tmp;
	
	selection = summary_get_selection_type(mainwin->summaryview);

	if (mainwin->lock_count == 0)
		state |= M_UNLOCKED;
	if (selection != SUMMARY_NONE)
		state |= M_MSG_EXIST;
	if (item && item->path && folder_item_parent(item) && !item->no_select) {
		state |= M_EXEC;
		/*		if (item->folder->type != F_NEWS) */
		state |= M_ALLOW_DELETE;

		if (prefs_common.immediate_exec == 0
		    && mainwin->lock_count == 0)
			state |= M_DELAY_EXEC;

		if ((selection == SUMMARY_NONE && item->hide_read_msgs)
		    || selection != SUMMARY_NONE)
			state |= M_HIDE_READ_MSG;
	}		
	if (mainwin->summaryview->threaded)
		state |= M_THREADED;
	else
		state |= M_UNTHREADED;	
	if (selection == SUMMARY_SELECTED_SINGLE ||
	    selection == SUMMARY_SELECTED_MULTIPLE)
		state |= M_TARGET_EXIST;
	if (selection == SUMMARY_SELECTED_SINGLE)
		state |= M_SINGLE_TARGET_EXIST;
	if (mainwin->summaryview->folder_item &&
	    mainwin->summaryview->folder_item->folder->klass->type == F_NEWS)
		state |= M_NEWS;
	else
		state |= M_NOT_NEWS;
	if (prefs_common.actions_list && g_slist_length(prefs_common.actions_list))
		state |= M_ACTIONS_EXIST;

	tmp = tags_get_list();
	if (tmp && g_slist_length(tmp))
		state |= M_TAGS_EXIST;
	g_slist_free(tmp);

	if (procmsg_have_queued_mails_fast() && !procmsg_is_sending())
		state |= M_HAVE_QUEUED_MAILS;

	if (selection == SUMMARY_SELECTED_SINGLE &&
	    (item &&
	     (folder_has_parent_of_type(item, F_DRAFT) ||
	      folder_has_parent_of_type(item, F_OUTBOX) ||
	      folder_has_parent_of_type(item, F_QUEUE))))
		state |= M_ALLOW_REEDIT;
	if (cur_account)
		state |= M_HAVE_ACCOUNT;
	
	if (any_folder_want_synchronise())
		state |= M_WANT_SYNC;

	for ( ; account_list != NULL; account_list = account_list->next) {
		if (((PrefsAccount*)account_list->data)->protocol == A_NNTP) {
			state |= M_HAVE_NEWS_ACCOUNT;
			break;
		}
	}
	
	if (procmsg_spam_can_learn() &&
	    (mainwin->summaryview->folder_item &&
	     mainwin->summaryview->folder_item->folder->klass->type != F_UNKNOWN &&
	     mainwin->summaryview->folder_item->folder->klass->type != F_NEWS)) {
		state |= M_CAN_LEARN_SPAM;
	}

	if (inc_is_active())
		state |= M_INC_ACTIVE;
	if (imap_cancel_all_enabled())
		state |= M_INC_ACTIVE;

	if (mainwin->summaryview->deleted > 0 ||
	    mainwin->summaryview->moved > 0 ||
	    mainwin->summaryview->copied > 0)
		state |= M_DELAY_EXEC;

	return state;
}



void main_window_set_menu_sensitive(MainWindow *mainwin)
{
	GtkItemFactory *ifactory = mainwin->menu_factory;
	SensitiveCond state;
	gboolean sensitive;
	GtkWidget *menu;
	GtkWidget *menuitem;
	SummaryView *summaryview;
	gchar *menu_path;
	gint i;
	GList *cur_item;

	static const struct {
		gchar *const entry;
		SensitiveCond cond;
	} entry[] = {
		{"/File/Save as...", M_TARGET_EXIST},
		{"/File/Print..."  , M_TARGET_EXIST},
		{"/File/Synchronise folders", M_WANT_SYNC},
		{"/File/Exit"      , M_UNLOCKED},

		{"/Edit/Select thread"		   , M_TARGET_EXIST},
		{"/Edit/Delete thread"		   , M_TARGET_EXIST},
		{"/Edit/Find in current message...", M_SINGLE_TARGET_EXIST},

		{"/View/Set displayed columns/in Folder list..."
						   , M_UNLOCKED}, 
		{"/View/Sort"                      , M_EXEC},
		{"/View/Thread view"               , M_EXEC},
		{"/View/Expand all threads"        , M_MSG_EXIST},
		{"/View/Collapse all threads"      , M_MSG_EXIST},
		{"/View/Hide read messages"	   , M_HIDE_READ_MSG},
		{"/View/Go to/Previous message"        , M_MSG_EXIST},
		{"/View/Go to/Next message"        , M_MSG_EXIST},
		{"/View/Go to/Previous unread message" , M_MSG_EXIST},
		{"/View/Go to/Previous new message"    , M_MSG_EXIST},
		{"/View/Go to/Previous marked message" , M_MSG_EXIST},
		{"/View/Go to/Previous labeled message", M_MSG_EXIST},
		{"/View/Go to/Next labeled message", M_MSG_EXIST},
		{"/View/Go to/Last read message"   , M_SINGLE_TARGET_EXIST},
		{"/View/Go to/Parent message"      , M_SINGLE_TARGET_EXIST},
		{"/View/Open in new window"        , M_SINGLE_TARGET_EXIST},
		{"/View/Message source"            , M_SINGLE_TARGET_EXIST},
		{"/View/All headers"          	   , M_SINGLE_TARGET_EXIST},
		{"/View/Quotes"                    , M_SINGLE_TARGET_EXIST},

		{"/Message/Receive/Get from current account"
						 , M_HAVE_ACCOUNT|M_UNLOCKED},
		{"/Message/Receive/Get from all accounts"
						 , M_HAVE_ACCOUNT|M_UNLOCKED},
		{"/Message/Receive/Cancel receiving"
						 , M_INC_ACTIVE},
		{"/Message/Send queued messages"  , M_HAVE_ACCOUNT|M_HAVE_QUEUED_MAILS},
		{"/Message/Compose a news message", M_HAVE_NEWS_ACCOUNT},
		{"/Message/Reply"                 , M_HAVE_ACCOUNT|M_TARGET_EXIST},
		{"/Message/Reply to"              , M_HAVE_ACCOUNT|M_TARGET_EXIST},
		{"/Message/Follow-up and reply to", M_HAVE_ACCOUNT|M_TARGET_EXIST|M_NEWS},
		{"/Message/Forward"               , M_HAVE_ACCOUNT|M_TARGET_EXIST},
		{"/Message/Forward as attachment" , M_HAVE_ACCOUNT|M_TARGET_EXIST},
        	{"/Message/Redirect"		  , M_HAVE_ACCOUNT|M_TARGET_EXIST},
		{"/Message/Move..."		  , M_TARGET_EXIST|M_ALLOW_DELETE},
		{"/Message/Copy..."		  , M_TARGET_EXIST|M_EXEC},
		{"/Message/Move to trash"	  , M_TARGET_EXIST|M_ALLOW_DELETE|M_NOT_NEWS},
		{"/Message/Delete..." 		  , M_TARGET_EXIST|M_ALLOW_DELETE},
		{"/Message/Cancel a news message" , M_TARGET_EXIST|M_ALLOW_DELETE|M_NEWS},
		{"/Message/Mark"   		  , M_TARGET_EXIST},
		{"/Message/Mark/Mark as spam"	  , M_TARGET_EXIST|M_CAN_LEARN_SPAM},
		{"/Message/Mark/Mark as ham" 	  , M_TARGET_EXIST|M_CAN_LEARN_SPAM},
		{"/Message/Mark/Ignore thread"    , M_TARGET_EXIST},
		{"/Message/Mark/Unignore thread"  , M_TARGET_EXIST},
		{"/Message/Mark/Lock"   	  , M_TARGET_EXIST},
		{"/Message/Mark/Unlock"   	  , M_TARGET_EXIST},
		{"/Message/Color label"		  , M_TARGET_EXIST},
		{"/Message/Tags"		  , M_TARGET_EXIST},
		{"/Message/Re-edit"               , M_HAVE_ACCOUNT|M_ALLOW_REEDIT},

		{"/Tools/Add sender to address book"   , M_SINGLE_TARGET_EXIST},
		{"/Tools/Harvest addresses"            , M_MSG_EXIST},
		{"/Tools/Harvest addresses/from Messages..."
						       , M_TARGET_EXIST},
		{"/Tools/Filter all messages in folder", M_MSG_EXIST|M_EXEC},
		{"/Tools/Filter selected messages"     , M_TARGET_EXIST|M_EXEC},
		{"/Tools/Create filter rule"           , M_SINGLE_TARGET_EXIST|M_UNLOCKED},
		{"/Tools/Create processing rule"       , M_SINGLE_TARGET_EXIST|M_UNLOCKED},
		{"/Tools/List URLs..."                 , M_TARGET_EXIST},
		{"/Tools/Actions"                      , M_TARGET_EXIST|M_ACTIONS_EXIST},
		{"/Tools/Execute"                      , M_DELAY_EXEC},
		{"/Tools/Delete duplicated messages/In selected folder"   , M_MSG_EXIST|M_ALLOW_DELETE},

		{"/Configuration", M_UNLOCKED},
		{"/Configuration/Preferences for current account...", M_UNLOCKED},
		{"/Configuration/Create new account...", M_UNLOCKED},
		{"/Configuration/Edit accounts...", M_UNLOCKED},

		{NULL, 0}
	};

	state = main_window_get_current_state(mainwin);

	for (i = 0; entry[i].entry != NULL; i++) {
		sensitive = ((entry[i].cond & state) == entry[i].cond);
		menu_set_sensitive(ifactory, entry[i].entry, sensitive);
	}

	menu = gtk_item_factory_get_widget(ifactory, "/Message/Receive");

	/* search for separator */
	for (cur_item = GTK_MENU_SHELL(menu)->children; cur_item != NULL;
	     cur_item = cur_item->next) {
		if (GTK_BIN(cur_item->data)->child == NULL) {
			cur_item = cur_item->next;
			break;
		}
	}

	for (; cur_item != NULL; cur_item = cur_item->next) {
		gtk_widget_set_sensitive(GTK_WIDGET(cur_item->data),
					 (M_UNLOCKED & state) != 0);
	}

	main_window_menu_callback_block(mainwin);

#define SET_CHECK_MENU_ACTIVE(path, active) \
{ \
	menuitem = gtk_item_factory_get_widget(ifactory, path); \
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), active); \
}

	SET_CHECK_MENU_ACTIVE("/View/Show or hide/Message view",
			      messageview_is_visible(mainwin->messageview));

	summaryview = mainwin->summaryview;
	menu_path = "/View/Sort/Don't sort";

	switch (summaryview->sort_key) {
	case SORT_BY_NUMBER:
		menu_path = "/View/Sort/by number"; break;
	case SORT_BY_SIZE:
		menu_path = "/View/Sort/by Size"; break;
	case SORT_BY_DATE:
		menu_path = "/View/Sort/by Date"; break;
	case SORT_BY_THREAD_DATE:
		menu_path = "/View/Sort/by Thread date"; break;
	case SORT_BY_FROM:
		menu_path = "/View/Sort/by From"; break;
	case SORT_BY_TO:
		menu_path = "/View/Sort/by To"; break;
	case SORT_BY_SUBJECT:
		menu_path = "/View/Sort/by Subject"; break;
	case SORT_BY_LABEL:
		menu_path = "/View/Sort/by color label"; break;
	case SORT_BY_MARK:
		menu_path = "/View/Sort/by mark"; break;
	case SORT_BY_STATUS:
		menu_path = "/View/Sort/by status"; break;
	case SORT_BY_MIME:
		menu_path = "/View/Sort/by attachment"; break;
	case SORT_BY_SCORE:
		menu_path = "/View/Sort/by score"; break;
	case SORT_BY_LOCKED:
		menu_path = "/View/Sort/by locked"; break;
	case SORT_BY_TAGS:
		menu_path = "/View/Sort/by tag"; break;
	case SORT_BY_NONE:
	default:
		menu_path = "/View/Sort/Don't sort"; break;
	}
	SET_CHECK_MENU_ACTIVE(menu_path, TRUE);

	if (summaryview->sort_type == SORT_ASCENDING) {
		SET_CHECK_MENU_ACTIVE("/View/Sort/Ascending", TRUE);
	} else {
		SET_CHECK_MENU_ACTIVE("/View/Sort/Descending", TRUE);
	}

	if (summaryview->sort_key != SORT_BY_NONE) {
		menu_set_sensitive(ifactory, "/View/Sort/Ascending", TRUE);
		menu_set_sensitive(ifactory, "/View/Sort/Descending", TRUE);
	} else {
		menu_set_sensitive(ifactory, "/View/Sort/Ascending", FALSE);
		menu_set_sensitive(ifactory, "/View/Sort/Descending", FALSE);
	}

	if (mainwin->messageview 
	&&  mainwin->messageview->mimeview
	&&  mainwin->messageview->mimeview->textview)
		SET_CHECK_MENU_ACTIVE("/View/All headers",
			      mainwin->messageview->mimeview->textview->show_all_headers);
	SET_CHECK_MENU_ACTIVE("/View/Thread view", (state & M_THREADED) != 0);
	SET_CHECK_MENU_ACTIVE("/View/Quotes/Fold all", FALSE);
	SET_CHECK_MENU_ACTIVE("/View/Quotes/Fold from level 2", FALSE);
	SET_CHECK_MENU_ACTIVE("/View/Quotes/Fold from level 3", FALSE);
	if (prefs_common.hide_quotes == 1)
		SET_CHECK_MENU_ACTIVE("/View/Quotes/Fold all", TRUE);
	if (prefs_common.hide_quotes == 2)
		SET_CHECK_MENU_ACTIVE("/View/Quotes/Fold from level 2", TRUE);
	if (prefs_common.hide_quotes == 3)
		SET_CHECK_MENU_ACTIVE("/View/Quotes/Fold from level 3", TRUE);

#undef SET_CHECK_MENU_ACTIVE

	main_window_menu_callback_unblock(mainwin);
}

void main_create_mailing_list_menu (MainWindow *mainwin, MsgInfo *msginfo)
{
	GtkItemFactory *ifactory;
	gint is_menu = 0;
	ifactory = gtk_item_factory_from_widget(mainwin->menubar);
	
	if (msginfo) 
		is_menu = mailing_list_create_submenu (ifactory, msginfo);
	if (is_menu)
		gtk_widget_set_sensitive (gtk_item_factory_get_item
				(ifactory,"/Message/Mailing-List"), TRUE);
	else
		gtk_widget_set_sensitive (gtk_item_factory_get_item
				(ifactory,"/Message/Mailing-List"), FALSE);
}

static gint mailing_list_create_submenu (GtkItemFactory *ifactory, MsgInfo *msginfo)
{
	gint menu_nb = 0;
	GtkWidget *menuitem;
	
	if (!msginfo || !msginfo->extradata) {
		menu_set_sensitive(ifactory, "/Message/Mailing-List/Post", FALSE);
		menu_set_sensitive(ifactory, "/Message/Mailing-List/Help", FALSE);
		menu_set_sensitive(ifactory, "/Message/Mailing-List/Subscribe", FALSE);
		menu_set_sensitive(ifactory, "/Message/Mailing-List/Unsubscribe", FALSE);
		menu_set_sensitive(ifactory, "/Message/Mailing-List/View archive", FALSE);
		menu_set_sensitive(ifactory, "/Message/Mailing-List/Contact owner", FALSE);
		return 0;
	}
		
	/* Mailing list post */
	if (!strcmp2 (msginfo->extradata->list_post, "NO")) {
		g_free(msginfo->extradata->list_post);
		msginfo->extradata->list_post = g_strdup (_("No posting allowed"));
 	}
 	menuitem = gtk_item_factory_get_item (ifactory, "/Message/Mailing-List/Post");
 		
 	menu_nb += mailing_list_populate_submenu (menuitem, msginfo->extradata->list_post);
 
 	/* Mailing list help */
	menuitem = gtk_item_factory_get_item (ifactory, "/Message/Mailing-List/Help");
	
	menu_nb += mailing_list_populate_submenu (menuitem, msginfo->extradata->list_help);

	/* Mailing list subscribe */
	menuitem = gtk_item_factory_get_item (ifactory, "/Message/Mailing-List/Subscribe");
	
	menu_nb += mailing_list_populate_submenu (menuitem, msginfo->extradata->list_subscribe);
		
	/* Mailing list unsubscribe */
	menuitem = gtk_item_factory_get_item (ifactory, "/Message/Mailing-List/Unsubscribe");
	
	menu_nb += mailing_list_populate_submenu (menuitem, msginfo->extradata->list_unsubscribe);
	
	/* Mailing list view archive */
	menuitem = gtk_item_factory_get_item (ifactory, "/Message/Mailing-List/View archive");
	
	menu_nb += mailing_list_populate_submenu (menuitem, msginfo->extradata->list_archive);
	
	/* Mailing list contact owner */
	menuitem = gtk_item_factory_get_item (ifactory, "/Message/Mailing-List/Contact owner");
	
	menu_nb += mailing_list_populate_submenu (menuitem, msginfo->extradata->list_owner);
	
	return menu_nb;
}

static gint mailing_list_populate_submenu (GtkWidget *menuitem, const gchar * list_header)
{
	GtkWidget *item, *menu;
	const gchar *url_pt ;
	gchar url_decoded[BUFFSIZE];
	GList *amenu, *alist;
	gint menu_nb = 0;
	
	menu = GTK_WIDGET(GTK_MENU_ITEM(menuitem)->submenu);
	
	/* First delete old submenu */
	/* FIXME: we can optimize this, and only change/add/delete necessary items */
	for (amenu = (GTK_MENU_SHELL(menu)->children) ; amenu; ) {
		alist = amenu->next;
		item = GTK_WIDGET (amenu->data);
		gtk_widget_destroy (item);
		amenu = alist;
	}
	if (list_header) {
		for (url_pt = list_header; url_pt && *url_pt;) {
			get_url_part (&url_pt, url_decoded, BUFFSIZE);
			item = NULL;
			if (!g_strncasecmp(url_decoded, "mailto:", 7)) {
 				item = gtk_menu_item_new_with_label ((url_decoded));
				g_signal_connect(G_OBJECT(item), "activate",
						 G_CALLBACK(mailing_list_compose),
						 NULL);
			}
 			else if (!g_strncasecmp (url_decoded, "http:", 5) ||
				 !g_strncasecmp (url_decoded, "https:",6)) {

				item = gtk_menu_item_new_with_label ((url_decoded));
				g_signal_connect(G_OBJECT(item), "activate",
						 G_CALLBACK(mailing_list_open_uri),
						 NULL);
			} 
			if (item) {
				gtk_menu_append (GTK_MENU(menu), item);
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

static void get_url_part (const gchar **buffer, gchar *url_decoded, gint maxlen)
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
		for (i = 0; *buf != '>' && *buf != 0x00 && i<maxlen; tmp[i++] = *(buf++));
		buf++;
	}
	else  {
		*buffer = NULL;
		*url_decoded = '\0';
		return;
	}
	
	tmp[i]       = 0x00;
	*url_decoded = '\0';
	*buffer = NULL;
	
	if (i == maxlen) {
		return;
	}
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
	gchar *mailto;

	gtk_label_get (GTK_LABEL (GTK_BIN (w)->child), (gchar **) &mailto);
	compose_new(NULL, mailto+7, NULL);
}
 
 static void mailing_list_open_uri (GtkWidget *w, gpointer *data)
{
 
 	gchar *mailto;
 
 	gtk_label_get (GTK_LABEL (GTK_BIN (w)->child), (gchar **) &mailto);
 	open_uri (mailto, prefs_common.uri_cmd);
} 
	
void main_window_popup(MainWindow *mainwin)
{
	if (!GTK_WIDGET_VISIBLE(GTK_WIDGET(mainwin->window)))
		main_window_show(mainwin);

	gtkut_window_popup(mainwin->window);
	if (prefs_common.layout_mode == SMALL_LAYOUT) {
		if (mainwin->in_folder) {
			mainwindow_enter_folder(mainwin);
		} else {
			mainwindow_exit_folder(mainwin);
		}
	}
}

void main_window_show(MainWindow *mainwin)
{
	gtk_widget_show(mainwin->window);
	gtk_widget_show(mainwin->vbox_body);

        gtk_widget_set_uposition(mainwin->window,
                                 prefs_common.mainwin_x,
                                 prefs_common.mainwin_y);

	gtk_widget_set_size_request(GTK_WIDGET_PTR(mainwin->folderview),
			     prefs_common.folderview_width,
			     prefs_common.folderview_height);
	gtk_widget_set_size_request(GTK_WIDGET_PTR(mainwin->summaryview),
			     prefs_common.summaryview_width,
			     prefs_common.summaryview_height);
	gtk_widget_set_size_request(GTK_WIDGET_PTR(mainwin->messageview),
			     prefs_common.msgview_width,
			     prefs_common.msgview_height);
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
	GtkItemFactory *ifactory = mainwin->menu_factory;
	GtkWidget *menuitem;
	gboolean first_set = (mainwin->hpaned == NULL);
	debug_print("Setting widgets... ");

	if (layout_mode == SMALL_LAYOUT && first_set) {
		gtk_widget_set_size_request(GTK_WIDGET_PTR(mainwin->folderview),
				    prefs_common.folderview_width,
				    prefs_common.folderview_height);
		gtk_widget_set_size_request(GTK_WIDGET_PTR(mainwin->summaryview),
				    0,0);
		gtk_widget_set_size_request(GTK_WIDGET_PTR(mainwin->messageview),
				    0,0);
	} else {
		gtk_widget_set_size_request(GTK_WIDGET_PTR(mainwin->folderview),
				    prefs_common.folderview_width,
				    prefs_common.folderview_height);
		gtk_widget_set_size_request(GTK_WIDGET_PTR(mainwin->summaryview),
				    prefs_common.summaryview_width,
				    prefs_common.summaryview_height);
		gtk_widget_set_size_request(GTK_WIDGET_PTR(mainwin->messageview),
				    prefs_common.msgview_width,
				    prefs_common.msgview_height);
	}

	mainwin->messageview->statusbar = mainwin->statusbar;
	mainwin->messageview->statusbar_cid = mainwin->messageview_cid;

	/* clean top-most container */
	if (mainwin->hpaned) {
		if (mainwin->hpaned->parent == mainwin->vpaned)
			gtk_widget_destroy(mainwin->vpaned);
		else
			gtk_widget_destroy(mainwin->hpaned);
	}

	menu_set_sensitive(ifactory, "/View/Show or hide/Message view", 
		(layout_mode != WIDE_MSGLIST_LAYOUT && layout_mode != SMALL_LAYOUT));
	switch (layout_mode) {
	case VERTICAL_LAYOUT:
	case NORMAL_LAYOUT:
	case SMALL_LAYOUT:
		hpaned = gtk_hpaned_new();
		if (layout_mode == VERTICAL_LAYOUT)
			vpaned = gtk_hpaned_new();
		else
			vpaned = gtk_vpaned_new();
		gtk_box_pack_start(GTK_BOX(vbox_body), hpaned, TRUE, TRUE, 0);
		gtk_paned_add1(GTK_PANED(hpaned),
			       GTK_WIDGET_PTR(mainwin->folderview));
		gtk_widget_show(hpaned);
		gtk_widget_queue_resize(hpaned);

		if (messageview_is_visible(mainwin->messageview)) {
			gtk_paned_add2(GTK_PANED(hpaned), vpaned);
			gtk_paned_add1(GTK_PANED(vpaned),
				       GTK_WIDGET_PTR(mainwin->summaryview));
		} else {
			gtk_paned_add2(GTK_PANED(hpaned),
				       GTK_WIDGET_PTR(mainwin->summaryview));
			gtk_widget_ref(vpaned);
		}
		gtk_paned_add2(GTK_PANED(vpaned),
			       GTK_WIDGET_PTR(mainwin->messageview));
		gtk_widget_show(vpaned);
		if (layout_mode == SMALL_LAYOUT && first_set) {
			mainwin_paned_show_first(GTK_PANED(hpaned));
		}
		gtk_widget_queue_resize(vpaned);
		break;
	case WIDE_LAYOUT:
		vpaned = gtk_vpaned_new();
		hpaned = gtk_hpaned_new();
		gtk_box_pack_start(GTK_BOX(vbox_body), vpaned, TRUE, TRUE, 0);
		gtk_paned_add1(GTK_PANED(vpaned), hpaned);

		gtk_paned_add1(GTK_PANED(hpaned),
			       GTK_WIDGET_PTR(mainwin->folderview));
		gtk_paned_add2(GTK_PANED(hpaned),
			       GTK_WIDGET_PTR(mainwin->summaryview));

		gtk_widget_show(hpaned);
		gtk_widget_queue_resize(hpaned);

		if (messageview_is_visible(mainwin->messageview)) {
			gtk_paned_add2(GTK_PANED(vpaned),
			       GTK_WIDGET_PTR(mainwin->messageview));	
		} else {
			gtk_widget_ref(GTK_WIDGET_PTR(mainwin->messageview));
		}
		gtk_widget_show(vpaned);
		gtk_widget_queue_resize(vpaned);
		break;
	case WIDE_MSGLIST_LAYOUT:
		vpaned = gtk_vpaned_new();
		hpaned = gtk_hpaned_new();
		gtk_box_pack_start(GTK_BOX(vbox_body), vpaned, TRUE, TRUE, 0);

		gtk_paned_add1(GTK_PANED(vpaned),
			       GTK_WIDGET_PTR(mainwin->summaryview));
		gtk_paned_add1(GTK_PANED(hpaned),
			       GTK_WIDGET_PTR(mainwin->folderview));

		gtk_widget_show(hpaned);
		gtk_widget_queue_resize(hpaned);

		if (messageview_is_visible(mainwin->messageview)) {
			gtk_paned_add2(GTK_PANED(hpaned),
			       GTK_WIDGET_PTR(mainwin->messageview));	
		} else {
			gtk_widget_ref(GTK_WIDGET_PTR(mainwin->messageview));
		}
		gtk_paned_add2(GTK_PANED(vpaned), hpaned);

		gtk_widget_show(vpaned);
		gtk_widget_queue_resize(vpaned);
		break;
	default:
		g_warning("Unknown layout");
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
		gtk_widget_set_size_request(GTK_WIDGET_PTR(mainwin->folderview),
				    prefs_common.folderview_width,
				    prefs_common.folderview_height);
		gtk_widget_set_size_request(GTK_WIDGET_PTR(mainwin->summaryview),
				    0,0);
		gtk_widget_set_size_request(GTK_WIDGET_PTR(mainwin->messageview),
				    0,0);
		gtk_widget_set_size_request(GTK_WIDGET(mainwin->window),
				prefs_common.mainwin_width,
				prefs_common.mainwin_height);
		gtk_paned_set_position(GTK_PANED(mainwin->hpaned), 800);
	} 
	/* remove headerview if not in prefs */
	headerview_set_visibility(mainwin->messageview->headerview,
				  prefs_common.display_header_pane);

	if (messageview_is_visible(mainwin->messageview))
		gtk_arrow_set(GTK_ARROW(mainwin->summaryview->toggle_arrow),
			      GTK_ARROW_DOWN, GTK_SHADOW_OUT);
	else
		gtk_arrow_set(GTK_ARROW(mainwin->summaryview->toggle_arrow),
			      GTK_ARROW_UP, GTK_SHADOW_OUT);

	gtk_window_move(GTK_WINDOW(mainwin->window),
			prefs_common.mainwin_x,
			prefs_common.mainwin_y);

	gtk_widget_queue_resize(vbox_body);
	gtk_widget_queue_resize(mainwin->vbox);
	gtk_widget_queue_resize(mainwin->window);
	/* CLAWS: previous "gtk_widget_show_all" makes noticeview
	 * and mimeview icon list/ctree lose track of their visibility states */
	if (!noticeview_is_visible(mainwin->messageview->noticeview)) 
		gtk_widget_hide(GTK_WIDGET_PTR(mainwin->messageview->noticeview));
	if (!noticeview_is_visible(mainwin->messageview->mimeview->siginfoview)) 
		gtk_widget_hide(GTK_WIDGET_PTR(mainwin->messageview->mimeview->siginfoview));
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mainwin->messageview->mimeview->mime_toggle)))
		gtk_widget_hide(mainwin->messageview->mimeview->icon_mainbox);
	else 
		gtk_widget_hide(mainwin->messageview->mimeview->ctree_mainbox);

	prefs_common.layout_mode = layout_mode;

	menuitem = gtk_item_factory_get_item
		(ifactory, "/View/Show or hide/Message view");
	gtk_check_menu_item_set_active
		(GTK_CHECK_MENU_ITEM(menuitem),
		 messageview_is_visible(mainwin->messageview));

#define SET_CHECK_MENU_ACTIVE(path, active) \
{ \
	menuitem = gtk_item_factory_get_widget(ifactory, path); \
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), active); \
}

	switch (prefs_common.layout_mode) {
	case NORMAL_LAYOUT:
		SET_CHECK_MENU_ACTIVE("/View/Layout/Standard", TRUE);
		break;
	case VERTICAL_LAYOUT:
		SET_CHECK_MENU_ACTIVE("/View/Layout/Three columns", TRUE);
		break;
	case WIDE_LAYOUT:
		SET_CHECK_MENU_ACTIVE("/View/Layout/Wide message", TRUE);
		break;
	case WIDE_MSGLIST_LAYOUT:
		SET_CHECK_MENU_ACTIVE("/View/Layout/Wide message list", TRUE);
		break;
	case SMALL_LAYOUT:
		SET_CHECK_MENU_ACTIVE("/View/Layout/Small screen", TRUE);
		break;
	}
#undef SET_CHECK_MENU_ACTIVE

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

	debug_print("done.\n");
}

void main_window_destroy_all(void)
{
	while (mainwin_list != NULL) {
		MainWindow *mainwin = (MainWindow*)mainwin_list->data;
		
		/* free toolbar stuff */
		toolbar_clear_list(TOOLBAR_MAIN);
		TOOLBAR_DESTROY_ACTIONS(mainwin->toolbar->action_list);
		TOOLBAR_DESTROY_ITEMS(mainwin->toolbar->item_list);

		mainwin->folderview->mainwin = NULL;
		mainwin->summaryview->mainwin = NULL;
		mainwin->messageview->mainwin = NULL;

		g_free(mainwin->toolbar);
		g_free(mainwin);
		
		mainwin_list = g_list_remove(mainwin_list, mainwin);
	}
	g_list_free(mainwin_list);
	mainwin_list = NULL;
}

static void toolbar_child_attached(GtkWidget *widget, GtkWidget *child,
				   gpointer data)
{
	gtk_widget_set_size_request(child, 1, -1);
}

static void toolbar_child_detached(GtkWidget *widget, GtkWidget *child,
				   gpointer data)
{
	gtk_widget_set_size_request(child, -1, -1);
}

static gboolean ac_label_button_pressed(GtkWidget *widget, GdkEventButton *event,
				    gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	if (!event) return FALSE;

	gtk_button_set_relief(GTK_BUTTON(widget), GTK_RELIEF_NORMAL);
	g_object_set_data(G_OBJECT(mainwin->ac_menu), "menu_button",
			  widget);

	gtk_menu_popup(GTK_MENU(mainwin->ac_menu), NULL, NULL,
		       menu_button_position, widget,
		       event->button, event->time);

	return TRUE;
}

static gint main_window_close_cb(GtkWidget *widget, GdkEventAny *event,
				 gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	gboolean close_allowed = TRUE;

	hooks_invoke(MAIN_WINDOW_CLOSE, &close_allowed);

	if (close_allowed && mainwin->lock_count == 0)
		app_exit_cb(data, 0, widget);

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

static void add_mailbox_cb(MainWindow *mainwin, guint action,
			   GtkWidget *widget)
{
	main_window_add_mailbox(mainwin);
}

static void update_folderview_cb(MainWindow *mainwin, guint action,
				 GtkWidget *widget)
{
	summary_show(mainwin->summaryview, NULL);
	folderview_check_new_all();
}

static void foldersort_cb(MainWindow *mainwin, guint action,
                           GtkWidget *widget)
{
	foldersort_open();
}

static void import_mbox_cb(MainWindow *mainwin, guint action,
			   GtkWidget *widget)
{
	/* only notify if import has failed */
	if (import_mbox(mainwin->summaryview->folder_item) == -1) {
		alertpanel_error(_("Mbox import has failed."));
	}
}

static void export_mbox_cb(MainWindow *mainwin, guint action,
			   GtkWidget *widget)
{
	/* only notify if export has failed */
	if (export_mbox(mainwin->summaryview->folder_item) == -1) {
		alertpanel_error(_("Export to mbox has failed."));
	}
}

static void export_list_mbox_cb(MainWindow *mainwin, guint action,
				GtkWidget *widget)
{
	/* only notify if export has failed */
	if (summaryview_export_mbox_list(mainwin->summaryview) == -1) {
		alertpanel_error(_("Export to mbox has failed."));
	}
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
		if (alertpanel(_("Exit"), _("Exit Claws Mail?"),
			       GTK_STOCK_CANCEL, GTK_STOCK_QUIT,  NULL)
		    != G_ALERTALTERNATE)
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

static void mainwindow_quicksearch(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	summaryview_activate_quicksearch(mainwin->summaryview, TRUE);
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
	toolbar_toggle(action, mainwin);
}

static void main_window_reply_cb(MainWindow *mainwin, guint action,
			  GtkWidget *widget)
{
	MessageView *msgview = (MessageView*)mainwin->messageview;
	GSList *msginfo_list = NULL;

	g_return_if_fail(msgview != NULL);

	msginfo_list = summary_get_selection(mainwin->summaryview);
	g_return_if_fail(msginfo_list != NULL);
	compose_reply_from_messageview(msgview, msginfo_list, action);
	g_slist_free(msginfo_list);
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

static void set_layout_cb(MainWindow *mainwin, guint action,
			       GtkWidget *widget)
{
	LayoutType layout_mode = action;
	LayoutType old_layout_mode = prefs_common.layout_mode;
	if (mainwin->menu_lock_count) {
		return;
	}
	if (!GTK_CHECK_MENU_ITEM(widget)->active) {
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
	}
	if (old_layout_mode != SMALL_LAYOUT && layout_mode == SMALL_LAYOUT) {
		mainwin_paned_show_first(GTK_PANED(mainwin->hpaned));
		mainwindow_exit_folder(mainwin);
	}
	summary_relayout(mainwin->summaryview);	
	summary_update_unread(mainwin->summaryview, NULL);
}

void main_window_toggle_work_offline (MainWindow *mainwin, gboolean offline,
					gboolean ask_sync)
{
	offline_ask_sync = ask_sync;
	if (offline)
		online_switch_clicked (GTK_BUTTON(mainwin->online_switch), mainwin);
	else
		online_switch_clicked (GTK_BUTTON(mainwin->offline_switch), mainwin);
	offline_ask_sync = TRUE;
}

static void toggle_work_offline_cb (MainWindow *mainwin, guint action, GtkWidget *widget)
{
	main_window_toggle_work_offline(mainwin, GTK_CHECK_MENU_ITEM(widget)->active, TRUE);
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
			GTK_STOCK_CANCEL, _("+_Synchronise"), NULL) != G_ALERTALTERNATE)
		return;
	
	if (offline_ask_sync)
		folder_synchronise(NULL);
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
		gtk_widget_hide (mainwin->online_switch);
		gtk_widget_show (mainwin->offline_switch);
		menuitem->active = TRUE;
		inc_autocheck_timer_remove();
			
		/* go offline */
		if (prefs_common.work_offline)
			return;
		mainwindow_check_synchronise(mainwin, TRUE);
		prefs_common.work_offline = TRUE;
		imap_disconnect_all();
		hooks_invoke(OFFLINE_SWITCH_HOOKLIST, NULL);
	} else {
		/*go online */
		if (!prefs_common.work_offline)
			return;
		gtk_widget_hide (mainwin->offline_switch);
		gtk_widget_show (mainwin->online_switch);
		menuitem->active = FALSE;
		prefs_common.work_offline = FALSE;
		inc_autocheck_timer_set();
		refresh_resolvers();
		hooks_invoke(OFFLINE_SWITCH_HOOKLIST, NULL);
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

static void filtering_debug_window_show_cb(MainWindow *mainwin, guint action,
			       GtkWidget *widget)
{
	log_window_show(mainwin->filtering_debugwin);
}

static void inc_cancel_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	inc_cancel_all();
	imap_cancel_all();
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

static void delete_trash_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	summary_delete_trash(mainwin->summaryview);
}

static void cancel_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	summary_cancel(mainwin->summaryview);
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
	mainwin->summaryview->messageview->all_headers = 
			GTK_CHECK_MENU_ITEM(widget)->active;
	summary_display_msg_selected(mainwin->summaryview,
				     GTK_CHECK_MENU_ITEM(widget)->active);
}

#define SET_CHECK_MENU_ACTIVE(path, active) \
{ \
	menuitem = gtk_item_factory_get_widget(ifactory, path); \
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), active); \
}

static void hide_quotes_cb(MainWindow *mainwin, guint action,
			       GtkWidget *widget)
{
	GtkWidget *menuitem;
	GtkItemFactory *ifactory = mainwin->menu_factory;

	if (mainwin->menu_lock_count) return;

	prefs_common.hide_quotes = 
			GTK_CHECK_MENU_ITEM(widget)->active ? action : 0;

	mainwin->menu_lock_count++;
	SET_CHECK_MENU_ACTIVE("/View/Quotes/Fold all", FALSE);
	SET_CHECK_MENU_ACTIVE("/View/Quotes/Fold from level 2", FALSE);
	SET_CHECK_MENU_ACTIVE("/View/Quotes/Fold from level 3", FALSE);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(widget), prefs_common.hide_quotes > 0);
	mainwin->menu_lock_count--;

	summary_redisplay_msg(mainwin->summaryview);
}

#undef SET_CHECK_MENU_ACTIVE
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

static void mark_as_spam_cb(MainWindow *mainwin, guint action,
			     GtkWidget *widget)
{
	summary_mark_as_spam(mainwin->summaryview, action, NULL);
}

static void ignore_thread_cb(MainWindow *mainwin, guint action,
			    GtkWidget *widget)
{
	summary_ignore_thread(mainwin->summaryview);
}

static void unignore_thread_cb(MainWindow *mainwin, guint action,
			    GtkWidget *widget)
{
	summary_unignore_thread(mainwin->summaryview);
}

static void lock_msgs_cb(MainWindow *mainwin, guint action,
			    GtkWidget *widget)
{
	summary_msgs_lock(mainwin->summaryview);
}

static void unlock_msgs_cb(MainWindow *mainwin, guint action,
			    GtkWidget *widget)
{
	summary_msgs_unlock(mainwin->summaryview);
}


static void reedit_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	summary_reedit(mainwin->summaryview);
}

static void open_urls_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	if (!mainwin->summaryview->displayed && mainwin->summaryview->selected) {
		summary_display_msg_selected(mainwin->summaryview, 
			mainwin->messageview->mimeview->textview->show_all_headers);
	}
	messageview_list_urls(mainwin->messageview);
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

	if (GTK_CHECK_MENU_ITEM(widget)->active) {
		str = conv_get_charset_str((CharSet)action);
		
		g_free(mainwin->messageview->forced_charset);
		mainwin->messageview->forced_charset = str ? g_strdup(str) : NULL;
		procmime_force_charset(str);
		
		summary_redisplay_msg(mainwin->summaryview);
		
		debug_print("forced charset: %s\n", str ? str : "Auto-Detect");
	}
}

static void set_decode_cb(MainWindow *mainwin, guint action,
			   GtkWidget *widget)
{
	if (GTK_CHECK_MENU_ITEM(widget)->active) {
		mainwin->messageview->forced_encoding = (EncodingType)action;
		
		summary_redisplay_msg(mainwin->summaryview);
		
		debug_print("forced encoding: %d\n", action);
	}
}

static void hide_read_messages (MainWindow *mainwin, guint action,
				GtkWidget *widget)
{
	if (!mainwin->summaryview->folder_item
	    || g_object_get_data(G_OBJECT(widget), "dont_toggle"))
		return;
	summary_toggle_show_read_messages(mainwin->summaryview);
}

static void thread_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	gboolean threaded = FALSE;
	if (mainwin->menu_lock_count) return;
	if (!mainwin->summaryview->folder_item) return;

	threaded = GTK_CHECK_MENU_ITEM(widget)->active;

	mainwin->summaryview->folder_item->threaded = threaded; 

	mainwin->summaryview->threaded = threaded;

	summary_show(mainwin->summaryview, 
			mainwin->summaryview->folder_item);
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

static void set_summary_display_item_cb(MainWindow *mainwin, guint action,
				GtkWidget *widget)
{
	prefs_summary_column_open();
}

static void set_folder_display_item_cb(MainWindow *mainwin, guint action,
				GtkWidget *widget)
{
	prefs_folder_column_open();
}

static void sort_summary_cb(MainWindow *mainwin, guint action,
			    GtkWidget *widget)
{
	FolderItem *item = mainwin->summaryview->folder_item;
	GtkWidget *menuitem;

	if (mainwin->menu_lock_count) return;

	if (GTK_CHECK_MENU_ITEM(widget)->active && item) {
		menuitem = gtk_item_factory_get_item
			(mainwin->menu_factory, "/View/Sort/Ascending");
		summary_sort(mainwin->summaryview, (FolderSortKey)action,
			     GTK_CHECK_MENU_ITEM(menuitem)->active
			     ? SORT_ASCENDING : SORT_DESCENDING);
		item->sort_key = action;
	}
}

static void sort_summary_type_cb(MainWindow *mainwin, guint action,
				 GtkWidget *widget)
{
	FolderItem *item = mainwin->summaryview->folder_item;

	if (mainwin->menu_lock_count) return;

	if (GTK_CHECK_MENU_ITEM(widget)->active && item)
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
	FolderItem *item;

	item = folderview_get_selected_item(mainwin->folderview);
	if (item) {
		main_window_cursor_wait(mainwin);
		STATUSBAR_PUSH(mainwin, _("Deleting duplicated messages..."));

		folderutils_delete_duplicates(item, prefs_common.immediate_exec ?
					      DELETE_DUPLICATES_REMOVE : DELETE_DUPLICATES_SETFLAG);

		STATUSBAR_POP(mainwin);
		main_window_cursor_normal(mainwin);
	}
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

static void delete_duplicated_all_cb(MainWindow *mainwin, guint action,
				 GtkWidget *widget)
{
	struct DelDupsData data = {0, 0};

	main_window_cursor_wait(mainwin);
	folder_func_to_all_folders(deldup_all, &data);
	main_window_cursor_normal(mainwin);
	
	alertpanel_notice(ngettext("Deleted %d duplicate message in %d folders.\n",
				   "Deleted %d duplicate messages in %d folders.\n",
				   data.dups),
			  data.dups, data.folders);
}

static void filter_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	summary_filter(mainwin->summaryview, (gboolean)action);
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
	summary_show(mainwin->summaryview, fitem);
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

static void last_read_cb(MainWindow *mainwin, guint action,
			    GtkWidget *widget)
{
	summary_select_last_read(mainwin->summaryview);
}

static void parent_cb(MainWindow *mainwin, guint action,
			    GtkWidget *widget)
{
	summary_select_parent(mainwin->summaryview);
}

static void goto_folder_cb(MainWindow *mainwin, guint action,
			   GtkWidget *widget)
{
	FolderItem *to_folder;

	to_folder = foldersel_folder_sel(NULL, FOLDER_SEL_ALL, NULL);

	if (to_folder)
		folderview_select(mainwin->folderview, to_folder);
}

static void goto_unread_folder_cb(MainWindow *mainwin, guint action,
			   GtkWidget *widget)
{
	folderview_select_next_unread(mainwin->folderview, FALSE);
}

static void copy_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	messageview_copy_clipboard(mainwin->messageview);
}

static void allsel_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	MessageView *msgview = mainwin->messageview;

	if (messageview_is_visible(msgview) &&
		 (GTK_WIDGET_HAS_FOCUS(msgview->mimeview->textview->text)))
		messageview_select_all(mainwin->messageview);
	else
		summary_select_all(mainwin->summaryview);
}

static void select_thread_cb(MainWindow *mainwin, guint action,
			     GtkWidget *widget)
{
	summary_select_thread(mainwin->summaryview, FALSE);
}

static void delete_thread_cb(MainWindow *mainwin, guint action,
			     GtkWidget *widget)
{
	summary_select_thread(mainwin->summaryview, TRUE);
}

static void create_filter_cb(MainWindow *mainwin, guint action,
			     GtkWidget *widget)
{
	summary_filter_open(mainwin->summaryview, (PrefsFilterType)action, 0);
}

static void create_processing_cb(MainWindow *mainwin, guint action,
			     GtkWidget *widget)
{
	summary_filter_open(mainwin->summaryview, (PrefsFilterType)action, 1);
}

static void prefs_pre_processing_open_cb(MainWindow *mainwin, guint action,
				         GtkWidget *widget)
{
	prefs_filtering_open(&pre_global_processing,
			     _("Processing rules to apply before folder rules"),
			     MANUAL_ANCHOR_PROCESSING,
			     NULL, NULL, FALSE);
}

static void prefs_post_processing_open_cb(MainWindow *mainwin, guint action,
				          GtkWidget *widget)
{
	prefs_filtering_open(&post_global_processing,
			     _("Processing rules to apply after folder rules"),
			     MANUAL_ANCHOR_PROCESSING,
			     NULL, NULL, FALSE);
}

static void prefs_filtering_open_cb(MainWindow *mainwin, guint action,
				    GtkWidget *widget)
{
	prefs_filtering_open(&filtering_rules,
			     _("Filtering configuration"),
			     MANUAL_ANCHOR_FILTERING,
			     NULL, NULL, TRUE);
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

static void prefs_tags_open_cb(MainWindow *mainwin, guint action,
				  GtkWidget *widget)
{
	prefs_tags_open(mainwin);
}
#ifdef USE_OPENSSL
static void ssl_manager_open_cb(MainWindow *mainwin, guint action,
				  GtkWidget *widget)
{
	ssl_manager_open(mainwin);
}
#endif
static void prefs_account_open_cb(MainWindow *mainwin, guint action,
				  GtkWidget *widget)
{
	if (!cur_account) {
		new_account_cb(mainwin, 0, widget);
	} else {
		account_open(cur_account);
	}
}

static void new_account_cb(MainWindow *mainwin, guint action,
			   GtkWidget *widget)
{
	account_edit_open();
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

static void account_compose_menu_cb(GtkMenuItem *menuitem, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)mainwin_list->data;
	PrefsAccount *account = (PrefsAccount *)data;
	FolderItem *item = mainwin->summaryview->folder_item;	

	compose_new_with_folderitem(account, item, NULL);
}

static void prefs_open_cb(GtkMenuItem *menuitem, gpointer data)
{
	prefs_gtk_open();
}

static void plugins_open_cb(GtkMenuItem *menuitem, gpointer data)
{
	pluginwindow_create();
}

static void manual_open_cb(MainWindow *mainwin, guint action,
			   GtkWidget *widget)
{
	manual_open((ManualType)action, NULL);
}

static void legend_open_cb(GtkMenuItem *menuitem, gpointer data)
{
	legend_show();
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

static gboolean mainwindow_focus_in_event(GtkWidget *widget, GdkEventFocus *focus,
					  gpointer data)
{
	SummaryView *summary;

	g_return_val_if_fail(data, FALSE);
	if (!g_list_find(mainwin_list, data))
		return TRUE;
	summary = ((MainWindow *)data)->summaryview;
	g_return_val_if_fail(summary, FALSE);

	if (GTK_CLIST(summary->ctree)->selection && 
	    g_list_length(GTK_CLIST(summary->ctree)->selection) > 1)
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
	}
	if (state->new_window_state == 0)
		gtk_window_set_skip_taskbar_hint(GTK_WINDOW(widget), FALSE);
	return FALSE;
}

gboolean mainwindow_is_obscured(void)
{
	return is_obscured;
}

#define BREAK_ON_MODIFIER_KEY() \
	if ((event->state & (GDK_MOD1_MASK|GDK_CONTROL_MASK)) != 0) break

gboolean mainwindow_key_pressed (GtkWidget *widget, GdkEventKey *event,
				    gpointer data)
{
	MainWindow *mainwin = (MainWindow*) data;
	
	if (!mainwin || !event) 
		return FALSE;

	if (quicksearch_has_focus(mainwin->summaryview->quicksearch))
	{
		lastkey = event->keyval;
		return FALSE;
	}

	switch (event->keyval) {
	case GDK_Q:             /* Quit */
		BREAK_ON_MODIFIER_KEY();

		app_exit_cb(mainwin, 0, NULL);
		return FALSE;
	case GDK_space:
		if (mainwin->folderview && mainwin->summaryview
		    && ((!mainwin->summaryview->displayed
		        && !mainwin->summaryview->selected) 
			|| (mainwin->summaryview->folder_item
			    && mainwin->summaryview->folder_item->total_msgs == 0))) {
			g_signal_stop_emission_by_name(G_OBJECT(widget), 
                                       "key_press_event");
			folderview_select_next_unread(mainwin->folderview, TRUE);
		}
		break;
#ifdef MAEMO
	case GDK_F6:
		if (maemo_mainwindow_is_fullscreen(widget)) {
                	gtk_window_unfullscreen(GTK_WINDOW(widget));
                } else {
                	gtk_window_fullscreen(GTK_WINDOW(widget));
                }
		break;
#endif
	default:
		break;
	}
	return FALSE;
}

#undef BREAK_ON_MODIFIER_KEY

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

static void sync_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	mainwindow_check_synchronise(mainwin, FALSE);
}

void mainwindow_learn (MainWindow *mainwin, gboolean is_spam)
{
	summary_mark_as_spam(mainwin->summaryview, is_spam, NULL);
}

void mainwindow_jump_to(const gchar *target, gboolean popup)
{
	gchar *tmp = NULL;
	gchar *p = NULL;
	FolderItem *item = NULL;
	gchar *msg = NULL;
	MainWindow *mainwin = mainwindow_get_mainwindow();
	
	if (!target)
		return;
		
	if (!mainwin) {
		printf("not initialized\n");
		return;
	}

	tmp = g_strdup(target);
	
	if ((p = strstr(tmp, "\r")) != NULL)
		*p = '\0';
	if ((p = strstr(tmp, "\n")) != NULL)
		*p = '\0';

	if ((item = folder_find_item_from_identifier(tmp))) {
		printf("selecting folder '%s'\n", tmp);
		folderview_select(mainwin->folderview, item);
		if (popup)
			main_window_popup(mainwin);
		g_free(tmp);
		return;
	}
	
	msg = strrchr(tmp, G_DIR_SEPARATOR);
	if (msg) {
		*msg++ = '\0';
		if ((item = folder_find_item_from_identifier(tmp))) {
			printf("selecting folder '%s'\n", tmp);
			folderview_select(mainwin->folderview, item);
		} else {
			printf("'%s' not found\n", tmp);
		}
		if (item && msg && atoi(msg)) {
			printf("selecting message %d\n", atoi(msg));
			summary_select_by_msgnum(mainwin->summaryview, atoi(msg));
			summary_display_msg_selected(mainwin->summaryview, FALSE);
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
				printf("selecting message %s\n", msg);
				summary_select_by_msgnum(mainwin->summaryview, msginfo->msgnum);
				summary_display_msg_selected(mainwin->summaryview, FALSE);
				if (popup)
					main_window_popup(mainwin);
				g_free(tmp);
				procmsg_msginfo_free(msginfo);
				return;
			} else {
				printf("'%s' not found\n", msg);
			}
		} else {
			printf("'%s' not found\n", msg);
		}
	} else {
		printf("'%s' not found\n", tmp);
	}
	
	g_free(tmp);
}

void mainwindow_exit_folder(MainWindow *mainwin) {
	if (prefs_common.layout_mode == SMALL_LAYOUT) {
		folderview_close_opened(mainwin->folderview);
		mainwin_paned_show_first(GTK_PANED(mainwin->hpaned));
	}
	mainwin->in_folder = FALSE;
}

void mainwindow_enter_folder(MainWindow *mainwin) {
	if (prefs_common.layout_mode == SMALL_LAYOUT) {
		mainwin_paned_show_last(GTK_PANED(mainwin->hpaned));
	}
	mainwin->in_folder = TRUE;
}

#ifdef MAEMO
gboolean maemo_mainwindow_is_fullscreen(GtkWidget *widget)
{
	gint w, h;
	gtk_window_get_size(GTK_WINDOW(widget), &w, &h); 
	return (w == 800);
}

void maemo_window_full_screen_if_needed (GtkWindow *window)
{
	if (maemo_mainwindow_is_fullscreen(mainwindow_get_mainwindow()->window)) {
		gtk_window_fullscreen(GTK_WINDOW(window));
	}
}

void maemo_connect_key_press_to_mainwindow (GtkWindow *window)
{
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(mainwindow_key_pressed), mainwindow_get_mainwindow());
}
#endif
