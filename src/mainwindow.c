/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2001 Hiroyuki Yamamoto
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
#include <gtk/gtkmain.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkcontainer.h>
#include <gtk/gtkstatusbar.h>
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
#include "headerview.h"
#include "menu.h"
#include "folder.h"
#include "inc.h"
#include "compose.h"
#include "procmsg.h"
#include "import.h"
#include "export.h"
#include "prefs_common.h"
#include "prefs_filter.h"
#include "prefs_filtering.h"
#include "prefs_scoring.h"
#include "prefs_account.h"
#include "prefs_folder_item.h"
#include "account.h"
#include "addressbook.h"
#include "headerwindow.h"
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
static void toolbar_reply_cb		(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_reply_to_all_cb	(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_reply_to_sender_cb	(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_forward_cb		(GtkWidget	*widget,
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

static void toolbar_account_button_pressed	(GtkWidget	*widget,
						 GdkEventButton	*event,
						 gpointer	 data);
static void ac_label_button_pressed		(GtkWidget	*widget,
						 GdkEventButton	*event,
						 gpointer	 data);

static gint main_window_close_cb (GtkWidget	*widget,
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
static void separate_widget_cb(GtkCheckMenuItem *checkitem, guint action);

static void addressbook_open_cb	(MainWindow	*mainwin,
				 guint		 action,
				 GtkWidget	*widget);
static void log_window_show_cb	(MainWindow	*mainwin,
				 guint		 action,
				 GtkWidget	*widget);

static void inc_mail_cb			(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);
static void inc_all_account_mail_cb	(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);

static void send_queue_cb		(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);

static void compose_cb			(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);
static void reply_cb			(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);

static void open_msg_cb			(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);

static void view_source_cb		(MainWindow	*mainwin,
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

static void set_charset_cb		(MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);

static void thread_cb		 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void set_display_item_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);
static void sort_summary_cb	 (MainWindow	*mainwin,
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

static void next_unread_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);

static void next_marked_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);

static void prev_marked_cb	 (MainWindow	*mainwin,
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
static void new_account_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);

static void account_menu_cb	 (GtkMenuItem	*menuitem,
				  gpointer	 data);

static void manual_open_cb	 (MainWindow	*mainwin,
				  guint		 action,
				  GtkWidget	*widget);

static void scan_tree_func	 (Folder	*folder,
				  FolderItem	*item,
				  gpointer	 data);

#define  SEPARATE_ACTION  667

static GtkItemFactoryEntry mainwin_entries[] =
{
	{N_("/_File"),				NULL, NULL, 0, "<Branch>"},
	{N_("/_File/_Add mailbox..."),		NULL, add_mailbox_cb, 0, NULL},
	{N_("/_File/_Add mbox mailbox..."),     NULL, add_mbox_cb, 0, NULL},
	{N_("/_File/_Update folder tree"),	NULL, update_folderview_cb, 0, NULL},
	{N_("/_File/_Folder"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_File/_Folder/Create _new folder..."),
						NULL, new_folder_cb, 0, NULL},
	{N_("/_File/_Folder/_Rename folder..."),NULL, rename_folder_cb, 0, NULL},
	{N_("/_File/_Folder/_Delete folder"),	NULL, delete_folder_cb, 0, NULL},
	{N_("/_File/_Import mbox file..."),	NULL, import_mbox_cb, 0, NULL},
	{N_("/_File/_Export to mbox file..."),	NULL, export_mbox_cb, 0, NULL},
	{N_("/_File/Empty _trash"),		NULL, empty_trash_cb, 0, NULL},
	{N_("/_File/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_File/_Save as..."),		NULL, save_as_cb, 0, NULL},
	{N_("/_File/_Print..."),		"<alt>P", print_cb, 0, NULL},
	{N_("/_File/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_File/_Close"),			"<alt>W", app_exit_cb, 0, NULL},
	{N_("/_File/E_xit"),			"<alt>Q", app_exit_cb, 0, NULL},

	{N_("/_Edit"),				NULL, NULL, 0, "<Branch>"},
	{N_("/_Edit/_Copy"),			"<control>C", copy_cb, 0, NULL},
	{N_("/_Edit/Select _all"),		"<control>A", allsel_cb, 0, NULL},
	{N_("/_Edit/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Edit/_Search"),			"<control>S", summary_search_cb, 0, NULL},

	{N_("/_View"),				NULL, NULL, 0, "<Branch>"},
	{N_("/_View/_Folder tree"),		NULL, toggle_folder_cb, 0, "<ToggleItem>"},
	{N_("/_View/_Message view"),		NULL, toggle_message_cb, 0, "<ToggleItem>"},
	{N_("/_View/_Toolbar"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_View/_Toolbar/Icon _and text"),	NULL, toggle_toolbar_cb, TOOLBAR_BOTH, "<RadioItem>"},
	{N_("/_View/_Toolbar/_Icon"),		NULL, toggle_toolbar_cb, TOOLBAR_ICON, "/View/Toolbar/Icon and text"},
	{N_("/_View/_Toolbar/_Text"),		NULL, toggle_toolbar_cb, TOOLBAR_TEXT, "/View/Toolbar/Icon and text"},
	{N_("/_View/_Toolbar/_Non-display"),	NULL, toggle_toolbar_cb, TOOLBAR_NONE, "/View/Toolbar/Icon and text"},
	{N_("/_View/_Status bar"),		NULL, toggle_statusbar_cb, 0, "<ToggleItem>"},
	{N_("/_View/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_View/Separate f_older tree"),	NULL, NULL, SEPARATE_ACTION + SEPARATE_FOLDER, "<ToggleItem>"},
	{N_("/_View/Separate m_essage view"),	NULL, NULL, SEPARATE_ACTION + SEPARATE_MESSAGE, "<ToggleItem>"},
	{N_("/_View/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_View/_Code set"),		NULL, NULL, 0, "<Branch>"},
	{N_("/_View/_Code set/_Auto detect"),
	 NULL, set_charset_cb, C_AUTO, "<RadioItem>"},

#define CODESET_SEPARATOR \
	{N_("/_View/_Code set/---"),		NULL, NULL, 0, "<Separator>"}
#define CODESET_ACTION(action) \
	 NULL, set_charset_cb, action, "/View/Code set/Auto detect"

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
	 CODESET_ACTION(C_CP1251)},
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
#endif

#undef CODESET_SEPARATOR
#undef CODESET_ACTION

	{N_("/_Message"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_Message/Rece_ive new mail"),	"<alt>I",	inc_mail_cb, 0, NULL},
	{N_("/_Message/Receive from _all accounts"),
						"<shift><alt>I", inc_all_account_mail_cb, 0, NULL},
	{N_("/_Message/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/Send queued messa_ges"),
						NULL, send_queue_cb, 0, NULL},
	{N_("/_Message/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/Compose _new message"),	"<alt>N",	compose_cb, 0, NULL},
	{N_("/_Message/_Reply"),		"<alt>R", 	reply_cb, COMPOSE_REPLY, NULL},
	{N_("/_Message/Repl_y to sender"),	"<control><alt>R", reply_cb, COMPOSE_REPLY_TO_SENDER, NULL},
	{N_("/_Message/Reply to a_ll"),		"<shift><alt>R", reply_cb, COMPOSE_REPLY_TO_ALL, NULL},
	{N_("/_Message/_Forward"),		"<control>F", reply_cb, COMPOSE_FORWARD, NULL},
	{N_("/_Message/Forward as an a_ttachment"),
						"<shift><control>F", reply_cb, COMPOSE_FORWARD_AS_ATTACH, NULL},
	{N_("/_Message/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/M_ove..."),		"<alt>O", move_to_cb, 0, NULL},
	{N_("/_Message/_Copy..."),		NULL, copy_to_cb, 0, NULL},
	{N_("/_Message/_Delete"),		"<alt>D", delete_cb,  0, NULL},
	{N_("/_Message/_Mark"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_Message/_Mark/_Mark"),		NULL, mark_cb,   0, NULL},
	{N_("/_Message/_Mark/_Unmark"),		NULL, unmark_cb, 0, NULL},
	{N_("/_Message/_Mark/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/_Mark/Mark as unr_ead"),	NULL, mark_as_unread_cb, 0, NULL},
	{N_("/_Message/_Mark/Mark it as _being read"),
						NULL, mark_as_read_cb, 0, NULL},
	{N_("/_Message/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/Open in new _window"),	"<shift><control>N", open_msg_cb, 0, NULL},
	{N_("/_Message/View _source"),		"<control>U", view_source_cb, 0, NULL},
	{N_("/_Message/Show all _header"),	"<control>H", header_window_show_cb, 0, NULL},
	{N_("/_Message/Re_edit"),		NULL, reedit_cb, 0, NULL},

	{N_("/_Summary"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_Summary/_Delete duplicated messages"),
						NULL, delete_duplicated_cb,   0, NULL},
	{N_("/_Summary/_Filter messages"),	NULL, filter_cb, 0, NULL},
	{N_("/_Summary/E_xecute"),		"<alt>X", execute_summary_cb, 0, NULL},
	{N_("/_Summary/_Update"),		"<alt>U", update_summary_cb,  0, NULL},
	{N_("/_Summary/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Summary/_Prev message"),		NULL, prev_cb, 0, NULL},
	{N_("/_Summary/_Next message"),		NULL, next_cb, 0, NULL},
	{N_("/_Summary/N_ext unread message"),	NULL, next_unread_cb, 0, NULL},
	{N_("/_Summary/Prev marked message"),	NULL, prev_marked_cb, 0, NULL},
	{N_("/_Summary/Next marked message"),	NULL, next_marked_cb, 0, NULL},
	{N_("/_Summary/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Summary/_Go to other folder"),	"<alt>G", goto_folder_cb, 0, NULL},
	{N_("/_Summary/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Summary/_Sort"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_Summary/_Sort/Sort by _number"),	NULL, sort_summary_cb, SORT_BY_NUMBER, NULL},
	{N_("/_Summary/_Sort/Sort by s_ize"),	NULL, sort_summary_cb, SORT_BY_SIZE, NULL},
	{N_("/_Summary/_Sort/Sort by _date"),	NULL, sort_summary_cb, SORT_BY_DATE, NULL},
	{N_("/_Summary/_Sort/Sort by _from"),	NULL, sort_summary_cb, SORT_BY_FROM, NULL},
	{N_("/_Summary/_Sort/Sort by _subject"),NULL, sort_summary_cb, SORT_BY_SUBJECT, NULL},
	{N_("/_Summary/_Sort/Sort by sco_re"),  NULL, sort_summary_cb, SORT_BY_SCORE, NULL},
	{N_("/_Summary/_Sort/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Summary/_Sort/_Attract by subject"),
						NULL, attract_by_subject_cb, 0, NULL},
	{N_("/_Summary/_Thread view"),		"<control>T",	     thread_cb, 0, NULL},
	{N_("/_Summary/Unt_hread view"),	"<shift><control>T", thread_cb, 1, NULL},
	{N_("/_Summary/Set display _item..."),	NULL, set_display_item_cb, 0, NULL},

	{N_("/_Tool"),				NULL, NULL, 0, "<Branch>"},
	{N_("/_Tool/_Address book"),		"<alt>A", addressbook_open_cb, 0, NULL},
	{N_("/_Tool/_Log window"),		"<alt>L", log_window_show_cb, 0, NULL},

	{N_("/_Configuration"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_Configuration/_Common preferences..."),
						NULL, prefs_common_open_cb, 0, NULL},
	{N_("/_Configuration/_Filter setting..."),
						NULL, prefs_filter_open_cb, 0, NULL},
	{N_("/_Configuration/_Scoring ..."),
						NULL, prefs_scoring_open_cb, 0, NULL},
	{N_("/_Configuration/_Filtering ..."),
						NULL, prefs_filtering_open_cb, 0, NULL},
	{N_("/_Configuration/_Preferences per account..."),
						NULL, prefs_account_open_cb, 0, NULL},
	{N_("/_Configuration/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Configuration/Create _new account..."),
						NULL, new_account_cb, 0, NULL},
	{N_("/_Configuration/_Edit accounts..."),
						NULL, account_edit_open, 0, NULL},
	{N_("/_Configuration/C_hange current account"),
						NULL, NULL, 0, "<Branch>"},

	{N_("/_Help"),				NULL, NULL, 0, "<LastBranch>"},
	{N_("/_Help/_Manual"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_Help/_Manual/_English"),		NULL, NULL, MANUAL_LANG_EN, NULL},
	{N_("/_Help/_Manual/_Japanese"),	NULL, manual_open_cb, MANUAL_LANG_JA, NULL},
	{N_("/_Help/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Help/_About"),			NULL, about_show, 0, NULL}
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
	GtkWidget *ac_button;
	GtkWidget *ac_label;

	FolderView *folderview;
	SummaryView *summaryview;
	MessageView *messageview;
	GdkColormap *colormap;
	GdkColor color[5];
	gboolean success[5];
	guint n_menu_entries;
	GtkItemFactory *ifactory;
	GtkWidget *ac_menu;
	GtkWidget *menuitem;
	gint i;

	debug_print(_("Creating main window...\n"));
	mainwin = g_new0(MainWindow, 1);

	/* main window */
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), PROG_VERSION);
	gtk_window_set_policy(GTK_WINDOW(window), TRUE, TRUE, FALSE);
	gtk_signal_connect(GTK_OBJECT(window), "delete_event",
			   GTK_SIGNAL_FUNC(main_window_close_cb), mainwin);
	gtk_signal_connect(GTK_OBJECT(window), "focus_in_event",
			   GTK_SIGNAL_FUNC(manage_window_focus_in), NULL);
	gtk_signal_connect(GTK_OBJECT(window), "focus_out_event",
			   GTK_SIGNAL_FUNC(manage_window_focus_out), NULL);
	gtk_widget_realize(window);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(vbox);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	/* menu bar */
	n_menu_entries = sizeof(mainwin_entries) / sizeof(mainwin_entries[0]);
	menubar = menubar_create(window, mainwin_entries, 
				 n_menu_entries, "<Main>", mainwin);
	gtk_widget_show(menubar);
	gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, TRUE, 0);

	handlebox = gtk_handle_box_new();
	gtk_widget_show(handlebox);
	gtk_box_pack_start(GTK_BOX(vbox), handlebox, FALSE, FALSE, 0);

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

	/* create views */
	mainwin->folderview  = folderview  = folderview_create();
	mainwin->summaryview = summaryview = summary_create();
	mainwin->messageview = messageview = messageview_create();
	mainwin->headerwin   = header_window_create();
	mainwin->logwin      = log_window_create();

	folderview->mainwin      = mainwin;
	folderview->summaryview  = summaryview;

	summaryview->mainwin     = mainwin;
	summaryview->folderview  = folderview;
	summaryview->messageview = messageview;
	summaryview->headerwin   = mainwin->headerwin;
	summaryview->window      = window;

	messageview->mainwin     = mainwin;

	mainwin->window    = window;
	mainwin->vbox      = vbox;
	mainwin->menubar   = menubar;
	mainwin->handlebox = handlebox;
	mainwin->vbox_body = vbox_body;
	mainwin->hbox_stat = hbox_stat;
	mainwin->statusbar = statusbar;
	mainwin->ac_button = ac_button;
	mainwin->ac_label  = ac_label;

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

	summaryview->color_normal.red = summaryview->color_normal.green =
		summaryview->color_normal.blue = 0;

	folderview->color_new.red = (guint16)55000;
	folderview->color_new.green = folderview->color_new.blue = 15000;

	folderview->color_normal.red = folderview->color_normal.green =
		folderview->color_normal.blue = 0;

	summaryview->color_important.red = 0;
	summaryview->color_marked.green = 0;
	summaryview->color_important.blue = (guint16)65535;

	color[0] = summaryview->color_marked;
	color[1] = summaryview->color_dim;
	color[2] = summaryview->color_normal;
	color[3] = folderview->color_new;
	color[4] = folderview->color_normal;

	colormap = gdk_window_get_colormap(window->window);
	gdk_colormap_alloc_colors(colormap, color, 5, FALSE, TRUE, success);
	for (i = 0; i < 5; i++) {
		if (success[i] == FALSE)
			g_warning(_("MainWindow: color allocation %d failed\n"), i);
	}

	debug_print(_("done.\n"));

	main_window_set_widgets(mainwin, type);

	/* set menu items */
	ifactory = gtk_item_factory_from_widget(menubar);
	menuitem = gtk_item_factory_get_item
		(ifactory, "/View/Code set/Auto detect");
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), TRUE);

	switch (prefs_common.toolbar_style) {
	case TOOLBAR_NONE:
		menuitem = gtk_item_factory_get_item
			(ifactory, "/View/Toolbar/Non-display");
		break;
	case TOOLBAR_ICON:
		menuitem = gtk_item_factory_get_item
			(ifactory, "/View/Toolbar/Icon");
		break;
	case TOOLBAR_TEXT:
		menuitem = gtk_item_factory_get_item
			(ifactory, "/View/Toolbar/Text");
		break;
	case TOOLBAR_BOTH:
		menuitem = gtk_item_factory_get_item
			(ifactory, "/View/Toolbar/Icon and text");
	}
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), TRUE);

	gtk_widget_hide(mainwin->hbox_stat);
	menuitem = gtk_item_factory_get_item(ifactory, "/View/Status bar");
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem),
				       prefs_common.show_statusbar);

	/* set the check of the SEPARATE_xxx menu items. we also need the main window
	 * as a property and pass the action type to the callback */
	menuitem = gtk_item_factory_get_widget_by_action(ifactory, SEPARATE_ACTION + SEPARATE_FOLDER);
	g_assert(menuitem);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), type & SEPARATE_FOLDER);
	gtk_object_set_data(GTK_OBJECT(menuitem), "mainwindow", mainwin);
	gtk_signal_connect(GTK_OBJECT(menuitem), "toggled", GTK_SIGNAL_FUNC(separate_widget_cb), 
					   GUINT_TO_POINTER(SEPARATE_FOLDER));

	menuitem = gtk_item_factory_get_widget_by_action(ifactory, SEPARATE_ACTION + SEPARATE_MESSAGE);
	g_assert(menuitem);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), type & SEPARATE_MESSAGE);
	gtk_object_set_data(GTK_OBJECT(menuitem), "mainwindow", mainwin);
	gtk_signal_connect(GTK_OBJECT(menuitem), "toggled", GTK_SIGNAL_FUNC(separate_widget_cb), 
					   GUINT_TO_POINTER(SEPARATE_MESSAGE));

	/*
	menu_set_sensitive(ifactory, "/Summary/Thread view",
			   prefs_common.enable_thread ? FALSE : TRUE);
	menu_set_sensitive(ifactory, "/Summary/Unthread view",
			   prefs_common.enable_thread ? TRUE : FALSE);
	*/
	main_window_set_thread_option(mainwin);

	menu_set_sensitive(ifactory, "/Help/Manual/English", FALSE);

	/* set account selection menu */
	ac_menu = gtk_item_factory_get_widget
		(ifactory, "/Configuration/Change current account");
	mainwin->ac_menu = ac_menu;

	main_window_set_toolbar_sensitive(mainwin, FALSE);

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
	header_window_init(mainwin->headerwin);
	log_window_init(mainwin->logwin);

	mainwin->cursor_count = 0;

	if (!watch_cursor)
		watch_cursor = gdk_cursor_new(GDK_WATCH);

	mainwin_list = g_list_append(mainwin_list, mainwin);

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

void main_window_reflect_prefs_all(void)
{
	GList *cur;
	MainWindow *mainwin;

	for (cur = mainwin_list; cur != NULL; cur = cur->next) {
		mainwin = (MainWindow *)cur->data;

		main_window_show_cur_account(mainwin);
		if (cur_account) {
			gtk_widget_set_sensitive(mainwin->get_btn,    TRUE);
			gtk_widget_set_sensitive(mainwin->getall_btn, TRUE);
		} else {
			gtk_widget_set_sensitive(mainwin->get_btn,    FALSE);
			gtk_widget_set_sensitive(mainwin->getall_btn, FALSE);
		}

		if (prefs_common.immediate_exec)
			gtk_widget_hide(mainwin->exec_btn);
		else
			gtk_widget_show(mainwin->exec_btn);

		summary_change_display_item(mainwin->summaryview);
		summary_redisplay_msg(mainwin->summaryview);
		headerview_set_visibility(mainwin->messageview->headerview,
					  prefs_common.display_header_pane);
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

	buf = g_strdup_printf(_("Current account: %s"), ac_name);
	gtk_label_set_text(GTK_LABEL(mainwin->ac_label), buf);
	gtk_widget_queue_resize(mainwin->ac_button);
	g_free(buf);

	g_free(ac_name);
}

void main_window_separation_change(MainWindow *mainwin, SeparateType type)
{
	GtkWidget *folder_wid  = GTK_WIDGET_PTR(mainwin->folderview);
	GtkWidget *summary_wid = GTK_WIDGET_PTR(mainwin->summaryview);
	GtkWidget *message_wid = GTK_WIDGET_PTR(mainwin->messageview);

	if (mainwin->type == type) return;

	/* remove widgets from those containers */
	gtk_widget_ref(folder_wid);
	gtk_widget_ref(summary_wid);
	gtk_widget_ref(message_wid);
	gtk_container_remove
		(GTK_CONTAINER(folder_wid->parent), folder_wid);
	gtk_container_remove
		(GTK_CONTAINER(summary_wid->parent), summary_wid);
	gtk_container_remove
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

void main_window_get_size(MainWindow *mainwin)
{
	GtkAllocation *allocation;

	allocation = &(GTK_WIDGET_PTR(mainwin->summaryview)->allocation);

	prefs_common.summaryview_width  = allocation->width;

	if (mainwin->summaryview->msg_is_toggled_on)
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

	if (confirm) {
		if (alertpanel(_("Empty trash"),
			       _("Empty all messages in trash?"),
			       _("Yes"), _("No"), NULL) != G_ALERTDEFAULT)
			return;
		manage_window_focus_in(mainwin->window, NULL, NULL);
	}

	procmsg_empty_trash();

	for (list = folder_get_list(); list != NULL; list = list->next) {
		Folder *folder;

		folder = list->data;
		if (folder->trash) {
			folder_item_scan(folder->trash);
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

void main_window_set_toolbar_sensitive(MainWindow *mainwin, gboolean sensitive)
{
	gtk_widget_set_sensitive(mainwin->reply_btn,       sensitive);
	gtk_widget_set_sensitive(mainwin->replyall_btn,    sensitive);
	gtk_widget_set_sensitive(mainwin->replysender_btn, sensitive);
	gtk_widget_set_sensitive(mainwin->fwd_btn,         sensitive);
	gtk_widget_set_sensitive(mainwin->exec_btn,        sensitive);
	gtk_widget_set_sensitive(mainwin->next_btn,        sensitive);

	if (!mainwin->summaryview->folder_item ||
	    mainwin->summaryview->folder_item->folder->type == F_NEWS)
		gtk_widget_set_sensitive(mainwin->delete_btn, FALSE);
	else
		gtk_widget_set_sensitive(mainwin->delete_btn, sensitive);
}

void main_window_set_menu_sensitive(MainWindow *mainwin, gint selection)
{
	GtkItemFactory *ifactory;
	gboolean sens;
	gboolean exec;

	ifactory = gtk_item_factory_from_widget(mainwin->menubar);

	if (selection == SUMMARY_SELECTED_SINGLE)
		sens = TRUE;
	else
		sens = FALSE;
	if (!mainwin->summaryview->folder_item ||
	    mainwin->summaryview->folder_item->folder->type == F_NEWS)
		exec = FALSE;
	else
		exec = TRUE;

	menu_set_sensitive(ifactory, "/File/Save as...", sens);
	menu_set_sensitive(ifactory, "/Message/Reply", sens);
	menu_set_sensitive(ifactory, "/Message/Reply to sender", sens);
	menu_set_sensitive(ifactory, "/Message/Reply to all", sens);
	menu_set_sensitive(ifactory, "/Message/Forward", sens);
	menu_set_sensitive(ifactory, "/Message/Forward as an attachment", sens);
	menu_set_sensitive(ifactory, "/Message/Open in new window", sens);
	menu_set_sensitive(ifactory, "/Message/Show all header", sens);
	menu_set_sensitive(ifactory, "/Message/View source", sens);
	if (sens && (!mainwin->summaryview->folder_item ||
		     mainwin->summaryview->folder_item->stype != F_DRAFT))
		sens = FALSE;
	menu_set_sensitive(ifactory, "/Message/Reedit", sens);

	if (selection == SUMMARY_SELECTED_SINGLE ||
	    selection == SUMMARY_SELECTED_MULTIPLE)
		sens = TRUE;
	else
		sens = FALSE;

	menu_set_sensitive(ifactory, "/File/Print..."  , sens);
	menu_set_sensitive(ifactory, "/Message/Move...", sens && exec);
	menu_set_sensitive(ifactory, "/Message/Copy...", sens && exec);
	menu_set_sensitive(ifactory, "/Message/Delete" , sens && exec);
	menu_set_sensitive(ifactory, "/Message/Mark"   , sens);

	if (selection != SUMMARY_NONE)
		sens = TRUE;
	else
		sens = FALSE;

	menu_set_sensitive(ifactory, "/Summary/Delete duplicated messages", sens && exec);
	menu_set_sensitive(ifactory, "/Summary/Filter messages", sens && exec);
	menu_set_sensitive(ifactory, "/Summary/Execute", sens);
	menu_set_sensitive(ifactory, "/Summary/Prev message", sens);
	menu_set_sensitive(ifactory, "/Summary/Next message", sens);
	menu_set_sensitive(ifactory, "/Summary/Next unread message", sens);
	menu_set_sensitive(ifactory, "/Summary/Sort", sens);
}

void main_window_popup(MainWindow *mainwin)
{
	gint x, y;
	gint sx, sy;
	GtkWidget *widget;

	gdk_window_get_origin(mainwin->window->window, &x, &y);
	sx = gdk_screen_width();
	sy = gdk_screen_height();
	x %= sx; if (x < 0) x += sx;
	y %= sy; if (y < 0) y += sy;
	gdk_window_move(mainwin->window->window, x, y);
	gdk_window_raise(mainwin->window->window);

	debug_print("window position: x = %d, y = %d\n", x, y);

	switch (mainwin->type) {
	case SEPARATE_FOLDER:
		widget = mainwin->win.sep_folder.folderwin;
		gdk_window_get_origin(widget->window, &x, &y);
		x %= sx; if (x < 0) x += sx;
		y %= sy; if (y < 0) y += sy;
		gdk_window_move(widget->window, x, y);
		gdk_window_raise(widget->window);
		break;
	case SEPARATE_MESSAGE:
		widget = mainwin->win.sep_message.messagewin;
		gdk_window_get_origin(widget->window, &x, &y);
		x %= sx; if (x < 0) x += sx;
		y %= sy; if (y < 0) y += sy;
		gdk_window_move(widget->window, x, y);
		gdk_window_raise(widget->window);
		break;
	case SEPARATE_BOTH:
		widget = mainwin->win.sep_both.folderwin;
		gdk_window_get_origin(widget->window, &x, &y);
		x %= sx; if (x < 0) x += sx;
		y %= sy; if (y < 0) y += sy;
		gdk_window_move(widget->window, x, y);
		gdk_window_raise(widget->window);
		widget = mainwin->win.sep_both.messagewin;
		gdk_window_get_origin(widget->window, &x, &y);
		x %= sx; if (x < 0) x += sx;
		y %= sy; if (y < 0) y += sy;
		gdk_window_move(widget->window, x, y);
		gdk_window_raise(widget->window);
		break;
	default:
	}
}

static void main_window_set_widgets(MainWindow *mainwin, SeparateType type)
{
	GtkWidget *folderwin = NULL;
	GtkWidget *messagewin = NULL;
	GtkWidget *hpaned;
	GtkWidget *vpaned;
	GtkWidget *vbox_body = mainwin->vbox_body;

	debug_print(_("Setting widgets..."));

	/* create separated window(s) if needed */
	if (type & SEPARATE_FOLDER) {
		folderwin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		gtk_window_set_policy(GTK_WINDOW(folderwin),
				      TRUE, TRUE, FALSE);
		gtk_widget_set_usize(folderwin, -1,
				     prefs_common.mainview_height);
		gtk_container_set_border_width(GTK_CONTAINER(folderwin),
					       BORDER_WIDTH);
		gtk_signal_connect(GTK_OBJECT(folderwin), "delete_event",
				   GTK_SIGNAL_FUNC(gtk_widget_hide_on_delete),
				   NULL);
	}
	if (type & SEPARATE_MESSAGE) {
		messagewin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
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
				   GTK_SIGNAL_FUNC(gtk_widget_hide_on_delete),
				   NULL);
	}

	switch (type) {
	case SEPARATE_NONE:
		hpaned = gtk_hpaned_new();
		gtk_widget_show(hpaned);
		gtk_box_pack_start(GTK_BOX(vbox_body), hpaned, TRUE, TRUE, 0);
		gtk_paned_add1(GTK_PANED(hpaned),
			       GTK_WIDGET_PTR(mainwin->folderview));

		vpaned = gtk_vpaned_new();
		if (mainwin->summaryview->msg_is_toggled_on) {
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
		if (mainwin->summaryview->msg_is_toggled_on) {
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

	debug_print(_("done.\n"));
}

#include "pixmaps/stock_mail_receive.xpm"
#include "pixmaps/stock_mail_receive_all.xpm"
#include "pixmaps/stock_mail_compose.xpm"
#include "pixmaps/stock_mail_reply.xpm"
#include "pixmaps/stock_mail_reply_to_all.xpm"
#include "pixmaps/stock_mail_reply_to_author.xpm"
#include "pixmaps/stock_mail_forward.xpm"
#include "pixmaps/stock_mail_send.xpm"
#include "pixmaps/stock_preferences.xpm"
#include "pixmaps/stock_properties.xpm"
#include "pixmaps/stock_down_arrow.xpm"
#include "pixmaps/stock_close.xpm"
#include "pixmaps/stock_exec.xpm"

#define CREATE_TOOLBAR_ICON(xpm_d) \
{ \
	icon = gdk_pixmap_create_from_xpm_d(container->window, &mask, \
					    &container->style->white, \
					    xpm_d); \
	icon_wid = gtk_pixmap_new(icon, mask); \
}

static void main_window_toolbar_create(MainWindow *mainwin,
				       GtkWidget *container)
{
	GtkWidget *toolbar;
	GdkPixmap *icon;
	GdkBitmap *mask;
	GtkWidget *icon_wid;
	GtkWidget *get_btn;
	GtkWidget *getall_btn;
	GtkWidget *compose_btn;
	GtkWidget *reply_btn;
	GtkWidget *replyall_btn;
	GtkWidget *replysender_btn;
	GtkWidget *fwd_btn;
	GtkWidget *send_btn;
	/*
	GtkWidget *prefs_btn;
	GtkWidget *account_btn;
	*/
	GtkWidget *next_btn;
	GtkWidget *delete_btn;
	GtkWidget *exec_btn;

	toolbar = gtk_toolbar_new(GTK_ORIENTATION_HORIZONTAL,
				  GTK_TOOLBAR_BOTH);
	gtk_container_add(GTK_CONTAINER(container), toolbar);
	gtk_container_set_border_width(GTK_CONTAINER(container), 2);
	gtk_toolbar_set_button_relief(GTK_TOOLBAR(toolbar), GTK_RELIEF_NONE);
	gtk_toolbar_set_space_style(GTK_TOOLBAR(toolbar),
				    GTK_TOOLBAR_SPACE_LINE);

	CREATE_TOOLBAR_ICON(stock_mail_receive_xpm);
	get_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					  _("Get"),
					  _("Incorporate new mail"),
					  "Get",
					  icon_wid, toolbar_inc_cb, mainwin);
	CREATE_TOOLBAR_ICON(stock_mail_receive_all_xpm);
	getall_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					     _("Get all"),
					     _("Incorporate new mail of all accounts"),
					     "Get all",
					     icon_wid,
					     toolbar_inc_all_cb,
					     mainwin);

	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));

	CREATE_TOOLBAR_ICON(stock_mail_send_xpm);
	send_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					   _("Send"),
					   _("Send queued message(s)"),
					   "Send",
					   icon_wid,
					   toolbar_send_cb,
					   mainwin);

	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));

	CREATE_TOOLBAR_ICON(stock_mail_compose_xpm);
	compose_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					      _("Compose"),
					      _("Compose new message"),
					      "New",
					      icon_wid,
					      toolbar_compose_cb,
					      mainwin);
	CREATE_TOOLBAR_ICON(stock_mail_reply_xpm);
	reply_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					    _("Reply"),
					    _("Reply to the message"),
					    "Reply",
					    icon_wid,
					    toolbar_reply_cb,
					    mainwin);
	CREATE_TOOLBAR_ICON(stock_mail_reply_to_all_xpm);
	replyall_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					       _("Reply all"),
					       _("Reply to all"),
					       "Reply to all",
					       icon_wid,
					       toolbar_reply_to_all_cb,
					       mainwin);
	CREATE_TOOLBAR_ICON(stock_mail_reply_to_author_xpm);
	replysender_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
						  _("Reply sender"),
						  _("Reply to sender"),
						  "Reply to sender",
						  icon_wid,
						  toolbar_reply_to_sender_cb,
						  mainwin);
	CREATE_TOOLBAR_ICON(stock_mail_forward_xpm);
	fwd_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					  _("Forward"),
					  _("Forward the message"),
					  "Fwd",
					  icon_wid,
					  toolbar_forward_cb,
					  mainwin);

	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));

	CREATE_TOOLBAR_ICON(stock_close_xpm);
	delete_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					  _("Delete"),
					  _("Delete the message"),
					  "Delete",
					  icon_wid,
					  toolbar_delete_cb,
					  mainwin);

	CREATE_TOOLBAR_ICON(stock_exec_xpm);
	exec_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					   _("Execute"),
					   _("Execute marked process"),
					   "Execute",
					   icon_wid,
					   toolbar_exec_cb,
					   mainwin);

	CREATE_TOOLBAR_ICON(stock_down_arrow_xpm);
	next_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					   _("Next"),
					   _("Next unread message"),
					   "Next unread",
					   icon_wid,
					   toolbar_next_unread_cb,
					   mainwin);

	/*
	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));

	CREATE_TOOLBAR_ICON(stock_preferences_xpm);
	prefs_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					    _("Prefs"),
					    _("Common preference"),
					    "Prefs",
					    icon_wid,
					    toolbar_prefs_cb,
					    mainwin);
	CREATE_TOOLBAR_ICON(stock_properties_xpm);
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
	*/

	mainwin->toolbar         = toolbar;
	mainwin->get_btn         = get_btn;
	mainwin->getall_btn      = getall_btn;
	mainwin->compose_btn     = compose_btn;
	mainwin->reply_btn       = reply_btn;
	mainwin->replyall_btn    = replyall_btn;
	mainwin->replysender_btn = replysender_btn;
	mainwin->fwd_btn         = fwd_btn;
	mainwin->send_btn        = send_btn;
	/*
	mainwin->prefs_btn       = prefs_btn;
	mainwin->account_btn     = account_btn;
	*/
	mainwin->next_btn        = next_btn;
	mainwin->delete_btn      = delete_btn;
	mainwin->exec_btn        = exec_btn;

	gtk_widget_show_all(toolbar);
}

/* callback functions */

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

	compose_cb(mainwin, 0, NULL);
}

static void toolbar_reply_cb	(GtkWidget	*widget,
				 gpointer	 data)
{
	MainWindow *mainwin = (MainWindow *)data;

	reply_cb(mainwin, COMPOSE_REPLY, NULL);
}

static void toolbar_reply_to_all_cb	(GtkWidget	*widget,
					 gpointer	 data)
{
	MainWindow *mainwin = (MainWindow *)data;

	reply_cb(mainwin, COMPOSE_REPLY_TO_ALL, NULL);
}

static void toolbar_reply_to_sender_cb	(GtkWidget	*widget,
					 gpointer	 data)
{
	MainWindow *mainwin = (MainWindow *)data;

	reply_cb(mainwin, COMPOSE_REPLY_TO_SENDER, NULL);
}

static void toolbar_forward_cb	(GtkWidget	*widget,
				 gpointer	 data)
{
	MainWindow *mainwin = (MainWindow *)data;

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

static void toolbar_account_button_pressed(GtkWidget *widget,
					   GdkEventButton *event,
					   gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	if (!event) return;
	if (event->button != 3) return;

	gtk_menu_popup(GTK_MENU(mainwin->ac_menu), NULL, NULL, NULL, NULL,
		       event->button, event->time);
}

static void ac_label_button_pressed(GtkWidget *widget, GdkEventButton *event,
				    gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	if (!event) return;

	gtk_menu_popup(GTK_MENU(mainwin->ac_menu), NULL, NULL, NULL, NULL,
		       event->button, event->time);
}

static gint main_window_close_cb(GtkWidget *widget, GdkEventAny *event,
				 gpointer data)
{
	app_exit_cb(data, 0, widget);

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
	folderview_update_all();
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

static void toggle_folder_cb(MainWindow *mainwin, guint action,
			     GtkWidget *widget)
{
	switch (mainwin->type) {
	case SEPARATE_NONE:
	case SEPARATE_MESSAGE:
		break;
	case SEPARATE_FOLDER:
		if (GTK_CHECK_MENU_ITEM(widget)->active)
			gtk_widget_show(mainwin->win.sep_folder.folderwin);
		else
			gtk_widget_hide(mainwin->win.sep_folder.folderwin);
		break;
	case SEPARATE_BOTH:
		if (GTK_CHECK_MENU_ITEM(widget)->active)
			gtk_widget_show(mainwin->win.sep_both.folderwin);
		else
			gtk_widget_hide(mainwin->win.sep_both.folderwin);
		break;
	}
}

static void toggle_message_cb(MainWindow *mainwin, guint action,
			      GtkWidget *widget)
{
	switch (mainwin->type) {
	case SEPARATE_NONE:
	case SEPARATE_FOLDER:
		break;
	case SEPARATE_MESSAGE:
		if (GTK_CHECK_MENU_ITEM(widget)->active)
			gtk_widget_show(mainwin->win.sep_message.messagewin);
		else
			gtk_widget_hide(mainwin->win.sep_message.messagewin);
		break;
	case SEPARATE_BOTH:
		if (GTK_CHECK_MENU_ITEM(widget)->active)
			gtk_widget_show(mainwin->win.sep_both.messagewin);
		else
			gtk_widget_hide(mainwin->win.sep_both.messagewin);
		break;
	}
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

static void separate_widget_cb(GtkCheckMenuItem *checkitem, guint action)
{
	MainWindow *mainwin;
	SeparateType type;

	mainwin = (MainWindow *) gtk_object_get_data(GTK_OBJECT(checkitem), "mainwindow");
	g_return_if_fail(mainwin != NULL);

	type = mainwin->type ^ action;
	main_window_separation_change(mainwin, type);

	prefs_common.sep_folder = (type & SEPARATE_FOLDER)  != 0;
	prefs_common.sep_msg    = (type & SEPARATE_MESSAGE) != 0;
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

static void inc_mail_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	inc_mail(mainwin);
}

static void inc_all_account_mail_cb(MainWindow *mainwin, guint action,
				    GtkWidget *widget)
{
	inc_all_account_mail(mainwin);
}

static void send_queue_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	GList *list;

	if (procmsg_send_queue() < 0)
		alertpanel_error(_("Sending queued message failed."));

	statusbar_pop_all();

	for (list = folder_get_list(); list != NULL; list = list->next) {
		Folder *folder;

		folder = list->data;
		if (folder->queue) {
			folder_item_scan(folder->queue);
			folderview_update_item(folder->queue, TRUE);
		}
	}
}

static void compose_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	if (mainwin->summaryview->folder_item)
		compose_new(mainwin->summaryview->folder_item->folder->account);
	else
		compose_new(NULL);
}

static void reply_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	MsgInfo *msginfo;

	msginfo = gtk_ctree_node_get_row_data
		(GTK_CTREE(mainwin->summaryview->ctree),
		 mainwin->summaryview->selected);

	if (!msginfo) return;

	switch (action) {
	case COMPOSE_REPLY:
		compose_reply(msginfo, prefs_common.reply_with_quote,
			      FALSE, FALSE);
		break;
	case COMPOSE_REPLY_TO_SENDER:
		compose_reply(msginfo, prefs_common.reply_with_quote,
			      FALSE, TRUE);
		break;
	case COMPOSE_REPLY_TO_ALL:
		compose_reply(msginfo, prefs_common.reply_with_quote,
			      TRUE, FALSE);
		break;
	case COMPOSE_FORWARD:
		compose_forward(NULL, msginfo, FALSE);
		break;
	case COMPOSE_FORWARD_AS_ATTACH:
		compose_forward(NULL, msginfo, TRUE);
		break;
	default:
		g_warning("reply_cb(): invalid action type: %d\n", action);
	}

	summary_set_marks_selected(mainwin->summaryview);
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

void main_window_set_thread_option(MainWindow *mainwin)
{
	GtkItemFactory *ifactory;
	gboolean no_item = FALSE;

	ifactory = gtk_item_factory_from_widget(mainwin->menubar);

	if (mainwin->summaryview == NULL)
		no_item = TRUE;
	else if (mainwin->summaryview->folder_item == NULL)
		no_item = TRUE;

	if (no_item) {
		menu_set_sensitive(ifactory, "/Summary/Thread view",   FALSE);
		menu_set_sensitive(ifactory, "/Summary/Unthread view", FALSE);
	}
	else {
		if (mainwin->summaryview->folder_item->prefs->enable_thread) {
			menu_set_sensitive(ifactory,
					   "/Summary/Thread view",   FALSE);
			menu_set_sensitive(ifactory,
					   "/Summary/Unthread view", TRUE);
			summary_thread_build(mainwin->summaryview);
		}
		else {
			menu_set_sensitive(ifactory,
					   "/Summary/Thread view",   TRUE);
			menu_set_sensitive(ifactory,
					   "/Summary/Unthread view", FALSE);
			summary_unthread(mainwin->summaryview);
		}
		prefs_folder_item_save_config(mainwin->summaryview->folder_item);
	}
}

static void thread_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	mainwin->summaryview->folder_item->prefs->enable_thread =
		!mainwin->summaryview->folder_item->prefs->enable_thread;
	main_window_set_thread_option(mainwin);

	/*
	if (0 == action) {
		summary_thread_build(mainwin->summaryview);
		mainwin->summaryview->folder_item->prefs->enable_thread =
			TRUE;
		menu_set_sensitive(ifactory, "/Summary/Thread view",   FALSE);
		menu_set_sensitive(ifactory, "/Summary/Unthread view", TRUE);
	} else {
		summary_unthread(mainwin->summaryview);
		mainwin->summaryview->folder_item->prefs->enable_thread =
			FALSE;
		menu_set_sensitive(ifactory, "/Summary/Thread view",   TRUE);
		menu_set_sensitive(ifactory, "/Summary/Unthread view", FALSE);
	}
	*/
}

static void set_display_item_cb(MainWindow *mainwin, guint action,
				GtkWidget *widget)
{
	prefs_summary_display_item_set();
}

static void sort_summary_cb(MainWindow *mainwin, guint action,
			    GtkWidget *widget)
{
	summary_sort(mainwin->summaryview, (SummarySortType)action);
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

	fitem = gtk_ctree_node_get_row_data(GTK_CTREE(folderview->ctree),
					    folderview->opened);
	if (!fitem) return;

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

static void next_unread_cb(MainWindow *mainwin, guint action,
			   GtkWidget *widget)
{
	summary_select_next_unread(mainwin->summaryview);
}

static void next_marked_cb(MainWindow *mainwin, guint action,
			   GtkWidget *widget)
{
	summary_select_next_marked(mainwin->summaryview);
}

static void prev_marked_cb(MainWindow *mainwin, guint action,
			   GtkWidget *widget)
{
	summary_select_prev_marked(mainwin->summaryview);
}

static void goto_folder_cb(MainWindow *mainwin, guint action,
			   GtkWidget *widget)
{
	FolderItem *to_folder;

	to_folder = foldersel_folder_sel(NULL);

	if (to_folder)
		folderview_select(mainwin->folderview, to_folder);
}

static void copy_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	messageview_copy_clipboard(mainwin->messageview);
}

static void allsel_cb(MainWindow *mainwin, guint action, GtkWidget *widget)
{
	if (GTK_WIDGET_HAS_FOCUS(mainwin->summaryview->ctree))
		summary_select_all(mainwin->summaryview);
	else if (mainwin->summaryview->msg_is_toggled_on &&
		 GTK_WIDGET_HAS_FOCUS(mainwin->messageview->textview->text))
		messageview_select_all(mainwin->messageview);
}

static void prefs_common_open_cb(MainWindow *mainwin, guint action,
				 GtkWidget *widget)
{
	prefs_common_open();
}

static void prefs_filter_open_cb(MainWindow *mainwin, guint action,
				 GtkWidget *widget)
{
	prefs_filter_open();
}

static void prefs_scoring_open_cb(MainWindow *mainwin, guint action,
				  GtkWidget *widget)
{
	prefs_scoring_open(NULL);
}

static void prefs_filtering_open_cb(MainWindow *mainwin, guint action,
				    GtkWidget *widget)
{
	prefs_filtering_open();
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
