/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2005 Hiroyuki Yamamoto
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include "main.h"
#include "prefs_gtk.h"
#include "prefs_common.h"
#include "prefs_display_header.h"
#include "prefs_summary_column.h"
#include "mainwindow.h"
#include "summaryview.h"
#include "messageview.h"
#include "manage_window.h"
#include "inc.h"
#include "menu.h"
#include "codeconv.h"
#include "utils.h"
#include "gtkutils.h"
#include "alertpanel.h"
#include "folder.h"
#include "socket.h"
#include "filesel.h"
#include "folderview.h"
#include "stock_pixmap.h"
#include "prefswindow.h"

enum {
	DATEFMT_FMT,
	DATEFMT_TXT,
	N_DATEFMT_COLUMNS
};

PrefsCommon prefs_common;

GtkWidget *notebook;

static struct Receive {
	GtkWidget *checkbtn_incext;
	GtkWidget *entry_incext;
	GtkWidget *button_incext;

	GtkWidget *checkbtn_autochk;
	GtkWidget *spinbtn_autochk;
	GtkObject *spinbtn_autochk_adj;

	GtkWidget *checkbtn_chkonstartup;
	GtkWidget *checkbtn_scan_after_inc;


	GtkWidget *checkbtn_newmail_auto;
	GtkWidget *checkbtn_newmail_manu;
	GtkWidget *entry_newmail_notify_cmd;
	GtkWidget *hbox_newmail_notify;
	GtkWidget *optmenu_recvdialog;
	GtkWidget *checkbtn_no_recv_err_panel;
	GtkWidget *checkbtn_close_recv_dialog;
} receive;

static struct Send {
	GtkWidget *checkbtn_savemsg;
	GtkWidget *optmenu_senddialog;

	GtkWidget *optmenu_charset;
	GtkWidget *optmenu_encoding_method;
} p_send;

static struct Display {
	GtkWidget *chkbtn_folder_unread;
	GtkWidget *entry_ng_abbrev_len;
	GtkWidget *spinbtn_ng_abbrev_len;
	GtkObject *spinbtn_ng_abbrev_len_adj;

	GtkWidget *chkbtn_transhdr;

	GtkWidget *chkbtn_swapfrom;
	GtkWidget *chkbtn_useaddrbook;
	GtkWidget *chkbtn_threadsubj;
	GtkWidget *entry_datefmt;
} display;

static struct Message {
	GtkWidget *chkbtn_mbalnum;
	GtkWidget *chkbtn_disphdrpane;
	GtkWidget *chkbtn_disphdr;
	GtkWidget *chkbtn_html;
	GtkWidget *chkbtn_cursor;
	GtkWidget *spinbtn_linespc;
	GtkObject *spinbtn_linespc_adj;

	GtkWidget *chkbtn_smoothscroll;
	GtkWidget *spinbtn_scrollstep;
	GtkObject *spinbtn_scrollstep_adj;
	GtkWidget *chkbtn_halfpage;

	GtkWidget *chkbtn_attach_desc;
} message;

static struct Interface {
	/* GtkWidget *checkbtn_emacs; */
	GtkWidget *checkbtn_always_show_msg;
	GtkWidget *checkbtn_openunread;
	GtkWidget *checkbtn_mark_as_read_on_newwin;
	GtkWidget *checkbtn_openinbox;
	GtkWidget *checkbtn_immedexec;
 	GtkWidget *optmenu_nextunreadmsgdialog;
} interface;

static struct Other {
	GtkWidget *checkbtn_addaddrbyclick;
	GtkWidget *checkbtn_confonexit;
	GtkWidget *checkbtn_cleanonexit;
	GtkWidget *checkbtn_askonclean;
	GtkWidget *checkbtn_warnqueued;
        GtkWidget *checkbtn_cliplog;
        GtkWidget *loglength_entry;
#if 0
#ifdef USE_OPENSSL
	GtkWidget *checkbtn_ssl_ask_unknown_valid;
#endif
#endif

	GtkWidget *spinbtn_iotimeout;
	GtkObject *spinbtn_iotimeout_adj;
} other;

static struct KeybindDialog {
	GtkWidget *window;
	GtkWidget *combo;
} keybind;

static void prefs_common_charset_set_data_from_optmenu	   (PrefParam *pparam);
static void prefs_common_charset_set_optmenu		   (PrefParam *pparam);
static void prefs_common_encoding_set_data_from_optmenu    (PrefParam *pparam);
static void prefs_common_encoding_set_optmenu		   (PrefParam *pparam);
static void prefs_common_recv_dialog_set_data_from_optmenu (PrefParam *pparam);
static void prefs_common_recv_dialog_set_optmenu	   (PrefParam *pparam);
static void prefs_common_recv_dialog_newmail_notify_toggle_cb	(GtkWidget *w,
								 gpointer data);
static void prefs_common_recv_dialog_set_data_from_optmenu(PrefParam *pparam);
static void prefs_common_recv_dialog_set_optmenu(PrefParam *pparam);
static void prefs_common_send_dialog_set_data_from_optmenu(PrefParam *pparam);
static void prefs_common_send_dialog_set_optmenu(PrefParam *pparam);
static void prefs_nextunreadmsgdialog_set_data_from_optmenu(PrefParam *pparam);
static void prefs_nextunreadmsgdialog_set_optmenu(PrefParam *pparam);

/*
   parameter name, default value, pointer to the prefs variable, data type,
   pointer to the widget pointer,
   pointer to the function for data setting,
   pointer to the function for widget setting
 */

static PrefParam param[] = {
	/* Receive */
	{"use_ext_inc", "FALSE", &prefs_common.use_extinc, P_BOOL,
	 &receive.checkbtn_incext,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"ext_inc_path", DEFAULT_INC_PATH, &prefs_common.extinc_cmd, P_STRING,
	 &receive.entry_incext,
	 prefs_set_data_from_entry, prefs_set_entry},

	{"autochk_newmail", "FALSE", &prefs_common.autochk_newmail, P_BOOL,
	 &receive.checkbtn_autochk,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"autochk_interval", "10", &prefs_common.autochk_itv, P_INT,
	 &receive.spinbtn_autochk,
	 prefs_set_data_from_spinbtn, prefs_set_spinbtn},
	{"check_on_startup", "FALSE", &prefs_common.chk_on_startup, P_BOOL,
	 &receive.checkbtn_chkonstartup,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"scan_all_after_inc", "FALSE", &prefs_common.scan_all_after_inc,
	 P_BOOL, &receive.checkbtn_scan_after_inc,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"newmail_notify_manu", "FALSE", &prefs_common.newmail_notify_manu,
	 P_BOOL, &receive.checkbtn_newmail_manu,
 	 prefs_set_data_from_toggle, prefs_set_toggle},
 	{"newmail_notify_auto", "FALSE", &prefs_common.newmail_notify_auto,
	P_BOOL, &receive.checkbtn_newmail_auto,
 	 prefs_set_data_from_toggle, prefs_set_toggle},
 	{"newmail_notify_cmd", "", &prefs_common.newmail_notify_cmd, P_STRING,
 	 &receive.entry_newmail_notify_cmd,
 	 prefs_set_data_from_entry, prefs_set_entry},
	{"receive_dialog_mode", "1", &prefs_common.recv_dialog_mode, P_ENUM,
	 &receive.optmenu_recvdialog,
	 prefs_common_recv_dialog_set_data_from_optmenu,
	 prefs_common_recv_dialog_set_optmenu},
	{"no_receive_error_panel", "FALSE", &prefs_common.no_recv_err_panel,
	 P_BOOL, &receive.checkbtn_no_recv_err_panel,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"close_receive_dialog", "TRUE", &prefs_common.close_recv_dialog,
	 P_BOOL, &receive.checkbtn_close_recv_dialog,
	 prefs_set_data_from_toggle, prefs_set_toggle},
 
	/* Send */
	{"save_message", "TRUE", &prefs_common.savemsg, P_BOOL,
	 &p_send.checkbtn_savemsg,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"send_dialog_mode", "0", &prefs_common.send_dialog_mode, P_ENUM,
	 &p_send.optmenu_senddialog,
	 prefs_common_send_dialog_set_data_from_optmenu,
	 prefs_common_send_dialog_set_optmenu},

	{"outgoing_charset", CS_AUTO, &prefs_common.outgoing_charset, P_STRING,
	 &p_send.optmenu_charset,
	 prefs_common_charset_set_data_from_optmenu,
	 prefs_common_charset_set_optmenu},
	{"encoding_method", "0", &prefs_common.encoding_method, P_ENUM,
	 &p_send.optmenu_encoding_method,
	 prefs_common_encoding_set_data_from_optmenu,
	 prefs_common_encoding_set_optmenu},

	{"allow_jisx0201_kana", "FALSE", &prefs_common.allow_jisx0201_kana,
	 P_BOOL, NULL, NULL, NULL},

	/* Compose */
	{"auto_ext_editor", "FALSE", &prefs_common.auto_exteditor, P_BOOL,
	 NULL, NULL, NULL},
	{"forward_as_attachment", "FALSE", &prefs_common.forward_as_attachment,
	 P_BOOL, NULL, NULL, NULL},
	{"redirect_keep_from", "FALSE",
	 &prefs_common.redirect_keep_from, P_BOOL,
	 NULL, NULL, NULL},
	{"undo_level", "50", &prefs_common.undolevels, P_INT,
	 NULL, NULL, NULL},

	{"linewrap_length", "72", &prefs_common.linewrap_len, P_INT,
	 NULL, NULL, NULL},
	{"linewrap_quotation", "TRUE", &prefs_common.linewrap_quote, P_BOOL,
	 NULL, NULL, NULL},
	{"linewrap_auto", "TRUE", &prefs_common.autowrap, P_BOOL,
	 NULL, NULL, NULL},
	{"linewrap_before_sending", "FALSE", &prefs_common.linewrap_at_send, P_BOOL, 
	 NULL, NULL, NULL},
        {"autosave", "FALSE", &prefs_common.autosave,
	 P_BOOL, NULL, NULL, NULL},
        {"autosave_length", "50", &prefs_common.autosave_length, P_INT,
	 NULL, NULL, NULL},
#if USE_ASPELL
	{"enable_aspell", "TRUE", &prefs_common.enable_aspell, P_BOOL,
	 NULL, NULL, NULL},
	{"aspell_path", ASPELL_PATH, &prefs_common.aspell_path, P_STRING,
	 NULL, NULL, NULL},
	{"dictionary",  "", &prefs_common.dictionary, P_STRING,
	 NULL, NULL, NULL},
	{"aspell_sugmode", "1", &prefs_common.aspell_sugmode, P_INT,
	 NULL, NULL, NULL},
	{"use_alternate_dict", "FALSE", &prefs_common.use_alternate, P_BOOL,
	 NULL, NULL, NULL},
	{"check_while_typing", "TRUE", &prefs_common.check_while_typing, P_BOOL,
	 NULL, NULL, NULL},
	{"misspelled_color", "16711680", &prefs_common.misspelled_col, P_COLOR,
	 NULL, NULL, NULL},
#endif
	{"reply_with_quote", "TRUE", &prefs_common.reply_with_quote, P_BOOL,
	 NULL, NULL, NULL},

	/* Account autoselection */
	{"reply_account_autoselect", "TRUE",
	 &prefs_common.reply_account_autosel, P_BOOL,
	 NULL, NULL, NULL},
	{"forward_account_autoselect", "TRUE",
	 &prefs_common.forward_account_autosel, P_BOOL,
	 NULL, NULL, NULL},
	{"reedit_account_autoselect", "TRUE",
	 &prefs_common.reedit_account_autosel, P_BOOL,
	 NULL, NULL, NULL},

	{"default_reply_list", "TRUE", &prefs_common.default_reply_list, P_BOOL,
	 NULL, NULL, NULL},

	{"show_ruler", "TRUE", &prefs_common.show_ruler, P_BOOL,
	 NULL, NULL, NULL},

	/* Quote */
	{"reply_quote_mark", "> ", &prefs_common.quotemark, P_STRING,
	 NULL, NULL, NULL},
	{"reply_quote_format", "On %d\\n%f wrote:\\n\\n%q",
	 &prefs_common.quotefmt, P_STRING, NULL, NULL, NULL},

	{"forward_quote_mark", "> ", &prefs_common.fw_quotemark, P_STRING,
	 NULL, NULL, NULL},
	{"forward_quote_format",
	 "\\n\\nBegin forwarded message:\\n\\n"
	 "?d{Date: %d\\n}?f{From: %f\\n}?t{To: %t\\n}?c{Cc: %c\\n}"
	 "?n{Newsgroups: %n\\n}?s{Subject: %s\\n}\\n\\n%M",
	 &prefs_common.fw_quotefmt, P_STRING, NULL, NULL, NULL},
	{"quote_chars", ">", &prefs_common.quote_chars, P_STRING,
	 NULL, NULL, NULL},

	/* Display */
	/* Obsolete fonts. For coexisting with Gtk+-1.2 version */
	{"widget_font",		NULL,
	  &prefs_common.widgetfont_gtk1,	P_STRING, NULL, NULL, NULL},
	{"message_font",	"-misc-fixed-medium-r-normal--14-*-*-*-*-*-*-*",
	 &prefs_common.textfont_gtk1,		P_STRING, NULL, NULL, NULL},
	{"small_font",		"-*-helvetica-medium-r-normal--10-*-*-*-*-*-*-*",
	  &prefs_common.smallfont_gtk1,		P_STRING, NULL, NULL, NULL},
	{"bold_font",		"-*-helvetica-bold-r-normal--12-*-*-*-*-*-*-*",
	  &prefs_common.boldfont_gtk1,		P_STRING, NULL, NULL, NULL},
	{"normal_font",		"-*-helvetica-medium-r-normal--12-*-*-*-*-*-*-*",
	  &prefs_common.normalfont_gtk1,	P_STRING, NULL, NULL, NULL},

	/* new fonts */
	{"widget_font_gtk2",	NULL,
	  &prefs_common.widgetfont,		P_STRING, NULL, NULL, NULL},
	{"message_font_gtk2",	"Monospace 9",
	 &prefs_common.textfont,		P_STRING, NULL, NULL, NULL},
	{"small_font_gtk2",	"Sans 9",
	  &prefs_common.smallfont,		P_STRING, NULL, NULL, NULL},
	{"bold_font_gtk2",	"Sans Bold 9",
	  &prefs_common.boldfont,		P_STRING, NULL, NULL, NULL},
	{"normal_font_gtk2",	"Sans 9", 
	  &prefs_common.normalfont,		P_STRING, NULL, NULL, NULL},

	/* image viewer */
	{"display_image", "TRUE", &prefs_common.display_img, P_BOOL,
	 NULL, NULL, NULL},
	{"resize_image", "TRUE", &prefs_common.resize_img, P_BOOL,
	 NULL, NULL, NULL},
	{"inline_image", "TRUE", &prefs_common.inline_img, P_BOOL,
	 NULL, NULL, NULL},

	{"display_folder_unread_num", "TRUE",
	 &prefs_common.display_folder_unread, P_BOOL,
	 &display.chkbtn_folder_unread,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"newsgroup_abbrev_len", "16",
	 &prefs_common.ng_abbrev_len, P_INT,
	 &display.spinbtn_ng_abbrev_len,
	 prefs_set_data_from_spinbtn, prefs_set_spinbtn},

	{"translate_header", "TRUE", &prefs_common.trans_hdr, P_BOOL,
	 &display.chkbtn_transhdr,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	/* Display: Summary View */
	{"enable_swap_from", "FALSE", &prefs_common.swap_from, P_BOOL,
	 &display.chkbtn_swapfrom,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"use_address_book", "TRUE", &prefs_common.use_addr_book, P_BOOL,
	 &display.chkbtn_useaddrbook,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"thread_by_subject", "TRUE", &prefs_common.thread_by_subject, P_BOOL,
	 &display.chkbtn_threadsubj,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"date_format", "%y/%m/%d(%a) %H:%M", &prefs_common.date_format,
	 P_STRING, &display.entry_datefmt,
	 prefs_set_data_from_entry, prefs_set_entry},

	{"enable_hscrollbar", "TRUE", &prefs_common.enable_hscrollbar, P_BOOL,
	 NULL, NULL, NULL},
	{"bold_unread", "TRUE", &prefs_common.bold_unread, P_BOOL,
	 NULL, NULL, NULL},
	{"thread_by_subject_max_age", "10", &prefs_common.thread_by_subject_max_age,
	P_INT, NULL, NULL, NULL },

	{"enable_thread", "TRUE", &prefs_common.enable_thread, P_BOOL,
	 NULL, NULL, NULL},
	{"toolbar_style", "3", &prefs_common.toolbar_style, P_ENUM,
	 NULL, NULL, NULL},
	{"show_statusbar", "TRUE", &prefs_common.show_statusbar, P_BOOL,
	 NULL, NULL, NULL},
	{"show_searchbar", "TRUE", &prefs_common.show_searchbar, P_BOOL,
	 NULL, NULL, NULL},

	{"folderview_vscrollbar_policy", "0",
	 &prefs_common.folderview_vscrollbar_policy, P_ENUM,
	 NULL, NULL, NULL},

	{"summary_col_show_mark", "TRUE",
	 &prefs_common.summary_col_visible[S_COL_MARK], P_BOOL, NULL, NULL, NULL},
	{"summary_col_show_unread", "TRUE",
	 &prefs_common.summary_col_visible[S_COL_STATUS], P_BOOL, NULL, NULL, NULL},
	{"summary_col_show_mime", "TRUE",
	 &prefs_common.summary_col_visible[S_COL_MIME], P_BOOL, NULL, NULL, NULL},
	{"summary_col_show_subject", "TRUE",
	 &prefs_common.summary_col_visible[S_COL_SUBJECT], P_BOOL, NULL, NULL, NULL},
	{"summary_col_show_from", "TRUE",
	 &prefs_common.summary_col_visible[S_COL_FROM], P_BOOL, NULL, NULL, NULL},
	{"summary_col_show_date", "TRUE",
	 &prefs_common.summary_col_visible[S_COL_DATE], P_BOOL, NULL, NULL, NULL},
	{"summary_col_show_size", "TRUE",
	 &prefs_common.summary_col_visible[S_COL_SIZE], P_BOOL, NULL, NULL, NULL},
	{"summary_col_show_number", "FALSE",
	 &prefs_common.summary_col_visible[S_COL_NUMBER], P_BOOL, NULL, NULL, NULL},
	{"summary_col_show_score", "FALSE",
	 &prefs_common.summary_col_visible[S_COL_SCORE], P_BOOL, NULL, NULL, NULL},
	{"summary_col_show_locked", "FALSE",
	 &prefs_common.summary_col_visible[S_COL_LOCKED], P_BOOL, NULL, NULL, NULL},

	{"summary_col_pos_mark", "0",
	  &prefs_common.summary_col_pos[S_COL_MARK], P_INT, NULL, NULL, NULL},
	{"summary_col_pos_unread", "1",
	  &prefs_common.summary_col_pos[S_COL_STATUS], P_INT, NULL, NULL, NULL},
	{"summary_col_pos_mime", "2",
	  &prefs_common.summary_col_pos[S_COL_MIME], P_INT, NULL, NULL, NULL},
	{"summary_col_pos_subject", "3",
	  &prefs_common.summary_col_pos[S_COL_SUBJECT], P_INT, NULL, NULL, NULL},
	{"summary_col_pos_from", "4",
	  &prefs_common.summary_col_pos[S_COL_FROM], P_INT, NULL, NULL, NULL},
	{"summary_col_pos_date", "5",
	  &prefs_common.summary_col_pos[S_COL_DATE], P_INT, NULL, NULL, NULL},
	{"summary_col_pos_size", "6",
	  &prefs_common.summary_col_pos[S_COL_SIZE], P_INT, NULL, NULL, NULL},
	{"summary_col_pos_number", "7",
	  &prefs_common.summary_col_pos[S_COL_NUMBER], P_INT, NULL, NULL, NULL},
	{"summary_col_pos_score", "8",
	 &prefs_common.summary_col_pos[S_COL_SCORE], P_INT, NULL, NULL, NULL},
	{"summary_col_pos_locked", "9",
	 &prefs_common.summary_col_pos[S_COL_LOCKED], P_INT, NULL, NULL, NULL},

	{"summary_col_size_mark", "10",
	 &prefs_common.summary_col_size[S_COL_MARK], P_INT, NULL, NULL, NULL},
	{"summary_col_size_unread", "13",
	 &prefs_common.summary_col_size[S_COL_STATUS], P_INT, NULL, NULL, NULL},
	{"summary_col_size_mime", "10",
	 &prefs_common.summary_col_size[S_COL_MIME], P_INT, NULL, NULL, NULL},
	{"summary_col_size_subject", "200",
	 &prefs_common.summary_col_size[S_COL_SUBJECT], P_INT, NULL, NULL, NULL},
	{"summary_col_size_from", "120",
	 &prefs_common.summary_col_size[S_COL_FROM], P_INT, NULL, NULL, NULL},
	{"summary_col_size_date", "118",
	 &prefs_common.summary_col_size[S_COL_DATE], P_INT, NULL, NULL, NULL},
	{"summary_col_size_size", "45",
	 &prefs_common.summary_col_size[S_COL_SIZE], P_INT, NULL, NULL, NULL},
	{"summary_col_size_number", "40",
	 &prefs_common.summary_col_size[S_COL_NUMBER], P_INT, NULL, NULL, NULL},
	{"summary_col_size_score", "40",
	 &prefs_common.summary_col_size[S_COL_SCORE], P_INT, NULL, NULL, NULL},
	{"summary_col_size_locked", "13",
	 &prefs_common.summary_col_size[S_COL_LOCKED], P_INT, NULL, NULL, NULL},

	/* Widget size */
	{"folderwin_x", "16", &prefs_common.folderwin_x, P_INT,
	 NULL, NULL, NULL},
	{"folderwin_y", "16", &prefs_common.folderwin_y, P_INT,
	 NULL, NULL, NULL},
	{"folderview_width", "179", &prefs_common.folderview_width, P_INT,
	 NULL, NULL, NULL},
	{"folderview_height", "450", &prefs_common.folderview_height, P_INT,
	 NULL, NULL, NULL},
	{"folderview_visible", "TRUE", &prefs_common.folderview_visible, P_BOOL,
	 NULL, NULL, NULL},

	{"folder_col_folder", "150", &prefs_common.folder_col_folder, P_INT,
	 NULL, NULL, NULL},
	{"folder_col_new", "32", &prefs_common.folder_col_new, P_INT,
	 NULL, NULL, NULL},
	{"folder_col_unread", "32", &prefs_common.folder_col_unread, P_INT,
	 NULL, NULL, NULL},
	{"folder_col_total", "32", &prefs_common.folder_col_total, P_INT,
	 NULL, NULL, NULL},

	{"summaryview_width", "600", &prefs_common.summaryview_width, P_INT,
	 NULL, NULL, NULL},
	{"summaryview_height", "157", &prefs_common.summaryview_height, P_INT,
	 NULL, NULL, NULL},

	{"main_messagewin_x", "256", &prefs_common.main_msgwin_x, P_INT,
	 NULL, NULL, NULL},
	{"main_messagewin_y", "210", &prefs_common.main_msgwin_y, P_INT,
	 NULL, NULL, NULL},
	{"messageview_width", "600", &prefs_common.msgview_width, P_INT,
	 NULL, NULL, NULL},
	{"messageview_height", "300", &prefs_common.msgview_height, P_INT,
	 NULL, NULL, NULL},
	{"messageview_visible", "TRUE", &prefs_common.msgview_visible, P_BOOL,
	 NULL, NULL, NULL},

	{"mainview_x", "64", &prefs_common.mainview_x, P_INT,
	 NULL, NULL, NULL},
	{"mainview_y", "64", &prefs_common.mainview_y, P_INT,
	 NULL, NULL, NULL},
	{"mainview_width", "600", &prefs_common.mainview_width, P_INT,
	 NULL, NULL, NULL},
	{"mainview_height", "600", &prefs_common.mainview_height, P_INT,
	 NULL, NULL, NULL},
	{"mainwin_x", "64", &prefs_common.mainwin_x, P_INT,
	 NULL, NULL, NULL},
	{"mainwin_y", "64", &prefs_common.mainwin_y, P_INT,
	 NULL, NULL, NULL},
	{"mainwin_width", "800", &prefs_common.mainwin_width, P_INT,
	 NULL, NULL, NULL},
	{"mainwin_height", "600", &prefs_common.mainwin_height, P_INT,
	 NULL, NULL, NULL},
	{"messagewin_width", "600", &prefs_common.msgwin_width, P_INT,
	 NULL, NULL, NULL},
	{"messagewin_height", "540", &prefs_common.msgwin_height, P_INT,
	 NULL, NULL, NULL},
	{"sourcewin_width", "600", &prefs_common.sourcewin_width, P_INT,
	 NULL, NULL, NULL},
	{"sourcewin_height", "500", &prefs_common.sourcewin_height, P_INT,
	 NULL, NULL, NULL},
	{"compose_width", "600", &prefs_common.compose_width, P_INT,
	 NULL, NULL, NULL},
	{"compose_height", "560", &prefs_common.compose_height, P_INT,
	 NULL, NULL, NULL},
	{"compose_x", "0", &prefs_common.compose_x, P_INT,
	 NULL, NULL, NULL},
	{"compose_y", "0", &prefs_common.compose_y, P_INT,
	 NULL, NULL, NULL},
	/* Message */
	{"enable_color", "TRUE", &prefs_common.enable_color, P_BOOL,
	 NULL, NULL, NULL},

	{"quote_level1_color", "179", &prefs_common.quote_level1_col, P_COLOR,
	 NULL, NULL, NULL},
	{"quote_level2_color", "179", &prefs_common.quote_level2_col, P_COLOR,
	 NULL, NULL, NULL},
	{"quote_level3_color", "179", &prefs_common.quote_level3_col, P_COLOR,
	 NULL, NULL, NULL},
	{"uri_color", "32512", &prefs_common.uri_col, P_COLOR,
	 NULL, NULL, NULL},
	{"target_folder_color", "14294218", &prefs_common.tgt_folder_col, P_COLOR,
	 NULL, NULL, NULL},
	{"signature_color", "7960953", &prefs_common.signature_col, P_COLOR,
	 NULL, NULL, NULL},
	{"recycle_quote_colors", "FALSE", &prefs_common.recycle_quote_colors,
	 P_BOOL, NULL, NULL, NULL},

	{"convert_mb_alnum", "FALSE", &prefs_common.conv_mb_alnum, P_BOOL,
	 &message.chkbtn_mbalnum,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"display_header_pane", "TRUE", &prefs_common.display_header_pane,
	 P_BOOL, &message.chkbtn_disphdrpane,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"display_header", "TRUE", &prefs_common.display_header, P_BOOL,
	 &message.chkbtn_disphdr,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"render_html", "TRUE", &prefs_common.render_html, P_BOOL,
	 &message.chkbtn_html,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"textview_cursor_visible", "FALSE",
	 &prefs_common.textview_cursor_visible, P_BOOL,
	 &message.chkbtn_cursor, prefs_set_data_from_toggle, prefs_set_toggle},
	{"line_space", "2", &prefs_common.line_space, P_INT,
	 &message.spinbtn_linespc,
	 prefs_set_data_from_spinbtn, prefs_set_spinbtn},

	{"enable_smooth_scroll", "FALSE",
	 &prefs_common.enable_smooth_scroll, P_BOOL,
	 &message.chkbtn_smoothscroll,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"scroll_step", "1", &prefs_common.scroll_step, P_INT,
	 &message.spinbtn_scrollstep,
	 prefs_set_data_from_spinbtn, prefs_set_spinbtn},
	{"scroll_half_page", "FALSE", &prefs_common.scroll_halfpage, P_BOOL,
	 &message.chkbtn_halfpage,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"show_other_header", "FALSE", &prefs_common.show_other_header, P_BOOL,
	 NULL, NULL, NULL},

	{"attach_desc", "TRUE", &prefs_common.attach_desc, P_BOOL,
	 &message.chkbtn_attach_desc,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"attach_save_directory", NULL,
	 &prefs_common.attach_save_dir, P_STRING, NULL, NULL, NULL},

	/* MIME viewer */
	{"mime_image_viewer", "display '%s'",
	 &prefs_common.mime_image_viewer, P_STRING, NULL, NULL, NULL},
	{"mime_audio_player", "play '%s'",
	 &prefs_common.mime_audio_player, P_STRING, NULL, NULL, NULL},
	{"mime_open_command", "gedit '%s'",
	 &prefs_common.mime_open_cmd, P_STRING, NULL, NULL, NULL},

	/* Interface */
	{"separate_folder", "FALSE", &prefs_common.sep_folder, P_BOOL,
	 NULL, NULL, NULL},
	{"separate_message", "FALSE", &prefs_common.sep_msg, P_BOOL,
	 NULL, NULL, NULL},

	/* {"emulate_emacs", "FALSE", &prefs_common.emulate_emacs, P_BOOL,
	 NULL, NULL, NULL}, */
	{"always_show_message_when_selected", "FALSE",
	 &prefs_common.always_show_msg,
	 P_BOOL, &interface.checkbtn_always_show_msg,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"open_unread_on_enter", "FALSE", &prefs_common.open_unread_on_enter,
	 P_BOOL, &interface.checkbtn_openunread,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"mark_as_read_on_new_window", "FALSE",
	 &prefs_common.mark_as_read_on_new_window,
	 P_BOOL, &interface.checkbtn_mark_as_read_on_newwin,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"mark_as_read_delay", "0",
	 &prefs_common.mark_as_read_delay, P_INT, 
	 NULL, NULL, NULL},
	{"open_inbox_on_inc", "FALSE", &prefs_common.open_inbox_on_inc,
	 P_BOOL, &interface.checkbtn_openinbox,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"immediate_execution", "TRUE", &prefs_common.immediate_exec, P_BOOL,
	 &interface.checkbtn_immedexec,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"nextunreadmsg_dialog", NULL, &prefs_common.next_unread_msg_dialog, P_ENUM,
	 &interface.optmenu_nextunreadmsgdialog,
	 prefs_nextunreadmsgdialog_set_data_from_optmenu,
	 prefs_nextunreadmsgdialog_set_optmenu},

	{"pixmap_theme_path", DEFAULT_PIXMAP_THEME, 
	 &prefs_common.pixmap_theme_path, P_STRING,
	 NULL, NULL, NULL},

	{"hover_timeout", "500", &prefs_common.hover_timeout, P_INT,
	 NULL, NULL, NULL},
	
	/* Other */
	{"uri_open_command", DEFAULT_BROWSER_CMD,
	 &prefs_common.uri_cmd, P_STRING, NULL, NULL, NULL},
	{"print_command", "lpr %s", &prefs_common.print_cmd, P_STRING,
	 NULL, NULL, NULL},
	{"ext_editor_command", "gedit %s",
	 &prefs_common.ext_editor_cmd, P_STRING, NULL, NULL, NULL},

	{"add_address_by_click", "FALSE", &prefs_common.add_address_by_click,
	 P_BOOL, &other.checkbtn_addaddrbyclick,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"confirm_on_exit", "FALSE", &prefs_common.confirm_on_exit, P_BOOL,
	 &other.checkbtn_confonexit,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"clean_trash_on_exit", "FALSE", &prefs_common.clean_on_exit, P_BOOL,
	 &other.checkbtn_cleanonexit,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"ask_on_cleaning", "TRUE", &prefs_common.ask_on_clean, P_BOOL,
	 &other.checkbtn_askonclean,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"warn_queued_on_exit", "TRUE", &prefs_common.warn_queued_on_exit,
	 P_BOOL, &other.checkbtn_warnqueued,
	 prefs_set_data_from_toggle, prefs_set_toggle},
#if 0
#ifdef USE_OPENSSL
	{"ssl_ask_unknown_valid", "TRUE", &prefs_common.ssl_ask_unknown_valid,
	 P_BOOL, &other.checkbtn_ssl_ask_unknown_valid,
	 prefs_set_data_from_toggle, prefs_set_toggle},
#endif
#endif
	{"work_offline", "FALSE", &prefs_common.work_offline, P_BOOL,
	 NULL, NULL, NULL},
	{"summary_quicksearch_type", "0", &prefs_common.summary_quicksearch_type, P_INT,
	 NULL, NULL, NULL},
	{"summary_quicksearch_sticky", "1", &prefs_common.summary_quicksearch_sticky, P_INT,
	 NULL, NULL, NULL},
	{"summary_quicksearch_recurse", "1", &prefs_common.summary_quicksearch_recurse, P_INT,
	 NULL, NULL, NULL},

	{"io_timeout_secs", "60", &prefs_common.io_timeout_secs,
	 P_INT, &other.spinbtn_iotimeout,
	 prefs_set_data_from_spinbtn, prefs_set_spinbtn},
	{"hide_score", "-9999", &prefs_common.kill_score, P_INT,
	 NULL, NULL, NULL},
	{"important_score", "1", &prefs_common.important_score, P_INT,
	 NULL, NULL, NULL},
        {"clip_log", "TRUE", &prefs_common.cliplog, P_BOOL,
	 &other.checkbtn_cliplog,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"log_length", "500", &prefs_common.loglength, P_INT,
	 &other.loglength_entry,
	 prefs_set_data_from_entry, prefs_set_entry},

	{"cache_max_mem_usage", "4096", &prefs_common.cache_max_mem_usage, P_INT,
	 NULL, NULL, NULL},
	{"cache_min_keep_time", "15", &prefs_common.cache_min_keep_time, P_INT,
	 NULL, NULL, NULL},

	{"color_new", "179", &prefs_common.color_new, P_COLOR,
	 NULL, NULL, NULL},

	{"filteringwin_width", "500", &prefs_common.filteringwin_width, P_INT,
	 NULL, NULL, NULL},
	{"filteringwin_height", "-1", &prefs_common.filteringwin_height, P_INT,
	 NULL, NULL, NULL},
	{"warn_dnd", "1", &prefs_common.warn_dnd, P_INT,
	 NULL, NULL, NULL},

	{NULL, NULL, NULL, P_OTHER, NULL, NULL, NULL}
};

/* widget creating functions */
static void prefs_common_create		(void);
static void prefs_receive_create	(void);
static void prefs_send_create		(void);
static void prefs_display_create	(void);
static void prefs_message_create	(void);
static void prefs_interface_create	(void);
static void prefs_other_create		(void);

static void date_format_ok_btn_clicked		(GtkButton	*button,
						 GtkWidget     **widget);
static void date_format_cancel_btn_clicked	(GtkButton	*button,
						 GtkWidget     **widget);
static gboolean date_format_key_pressed		(GtkWidget	*keywidget,
						 GdkEventKey	*event,
						 GtkWidget     **widget);
static gboolean date_format_on_delete		(GtkWidget	*dialogwidget,
						 GdkEventAny	*event,
						 GtkWidget     **widget);
static void date_format_entry_on_change		(GtkEditable	*editable,
						 GtkLabel	*example);
static void date_format_select_row	        (GtkTreeView *list_view,
						 GtkTreePath *path,
						 GtkTreeViewColumn *column,
						 GtkWidget *date_format);
static GtkWidget *date_format_create            (GtkButton      *button,
                                                 void           *data);

static void prefs_keybind_select		(void);
static gint prefs_keybind_deleted		(GtkWidget	*widget,
						 GdkEventAny	*event,
						 gpointer	 data);
static gboolean prefs_keybind_key_pressed	(GtkWidget	*widget,
						 GdkEventKey	*event,
						 gpointer	 data);
static void prefs_keybind_cancel		(void);
static void prefs_keybind_apply_clicked		(GtkWidget	*widget);

static void prefs_common_apply		(void);

typedef struct CommonPage
{
        PrefsPage page;
 
        GtkWidget *vbox;
} CommonPage;

static CommonPage common_page;

static void create_widget_func(PrefsPage * _page,
                                           GtkWindow * window,
                                           gpointer data)
{
	CommonPage *page = (CommonPage *) _page;
	GtkWidget *vbox;

	vbox = gtk_vbox_new(FALSE, 6);
	gtk_widget_show(vbox);

	if (notebook == NULL)
		prefs_common_create();
	gtk_box_pack_start (GTK_BOX (vbox), notebook, TRUE, TRUE, 0);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), 0);

	prefs_set_dialog(param);

	page->vbox = vbox;

	page->page.widget = vbox;
}

static void destroy_widget_func(PrefsPage *_page)
{
	CommonPage *page = (CommonPage *) _page;

	gtk_container_remove(GTK_CONTAINER (page->vbox), notebook);
}

static void save_func(PrefsPage * _page)
{
	prefs_common_apply();
}

void prefs_common_init(void) 
{
        static gchar *path[2];

	prefs_common.disphdr_list = NULL;
            
        path[0] = _("Common");
        path[2] = NULL;
        
        common_page.page.path = path;
	common_page.page.weight = 1000.0;
        common_page.page.create_widget = create_widget_func;
        common_page.page.destroy_widget = destroy_widget_func;
        common_page.page.save_page = save_func;

        prefs_gtk_register_page((PrefsPage *) &common_page);
}

PrefsCommon *prefs_common_get(void)
{
	return &prefs_common;
}

/*
 * Read history list from the specified history file
 */
GList *prefs_common_read_history(const gchar *history) 
{
	FILE *fp;
	gchar *path;
	gchar buf[PREFSBUFSIZE];
	GList *tmp = NULL;

	path = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, history,
			   NULL);
	if ((fp = fopen(path, "rb")) == NULL) {
		if (ENOENT != errno) FILE_OP_ERROR(path, "fopen");
		g_free(path);
		return NULL;
	}
	g_free(path);
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		g_strstrip(buf);
		if (buf[0] == '\0') continue;
		tmp = add_history(tmp, buf);
	}
	fclose(fp);

	tmp = g_list_reverse(tmp);

	return tmp;
}

void prefs_common_read_config(void)
{
	gchar *rcpath;
	
	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMON_RC, NULL);
	prefs_read_config(param, "Common", rcpath, NULL);
	g_free(rcpath);
	
	prefs_common.mime_open_cmd_history =
		prefs_common_read_history(COMMAND_HISTORY);
	prefs_common.summary_quicksearch_history =
		prefs_common_read_history(QUICKSEARCH_HISTORY);
}

/*
 * Save history list to the specified history file
 */
void prefs_common_save_history(const gchar *history, GList *list)
{
	GList *cur;
	FILE *fp;
	gchar *path;

	path = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, history,
			   NULL);
	if ((fp = fopen(path, "wb")) == NULL) {
		FILE_OP_ERROR(path, "fopen");
		g_free(path);
		return;
	}

	for (cur = list; cur != NULL; cur = cur->next) {
		fputs((gchar *)cur->data, fp);
		fputc('\n', fp);
	}

	fclose(fp);
	g_free(path);
}

void prefs_common_write_config(void)
{
	prefs_write_config(param, "Common", COMMON_RC);

	prefs_common_save_history(COMMAND_HISTORY, 
		prefs_common.mime_open_cmd_history);
	prefs_common_save_history(QUICKSEARCH_HISTORY, 
		prefs_common.summary_quicksearch_history);
}

static void prefs_common_create(void)
{
	gint page = 0;

	debug_print("Creating common preferences window...\n");

	notebook = gtk_notebook_new ();
	gtk_widget_show(notebook);
	gtk_container_set_border_width (GTK_CONTAINER (notebook), 2);
	/* GTK_WIDGET_UNSET_FLAGS (notebook, GTK_CAN_FOCUS); */
	gtk_notebook_set_scrollable (GTK_NOTEBOOK (notebook), TRUE);
	
	gtk_notebook_popup_enable (GTK_NOTEBOOK (notebook));

	gtk_widget_ref(notebook);

	/* create all widgets on notebook */
	prefs_receive_create();
	SET_NOTEBOOK_LABEL(notebook, _("Receive"),   page++);
	prefs_send_create();
	SET_NOTEBOOK_LABEL(notebook, _("Send"),      page++);
	prefs_display_create();
	SET_NOTEBOOK_LABEL(notebook, _("Display"),   page++);
	prefs_message_create();
	SET_NOTEBOOK_LABEL(notebook, _("Message"),   page++);
	prefs_interface_create();
	SET_NOTEBOOK_LABEL(notebook, _("Interface"), page++);
	prefs_other_create();
	SET_NOTEBOOK_LABEL(notebook, _("Other"),     page++);

	gtk_widget_show_all(notebook);
}

static void prefs_receive_create(void)
{
	GtkWidget *vbox1;
	GtkWidget *vbox2;
	GtkWidget *frame_incext;
	GtkWidget *checkbtn_incext;
	GtkWidget *hbox;
	GtkWidget *label_incext;
	GtkWidget *entry_incext;
	/* GtkWidget *button_incext; */

	GtkWidget *hbox_autochk;
	GtkWidget *checkbtn_autochk;
	GtkWidget *label_autochk1;
	GtkObject *spinbtn_autochk_adj;
	GtkWidget *spinbtn_autochk;
	GtkWidget *label_autochk2;
	GtkWidget *checkbtn_chkonstartup;
	GtkWidget *checkbtn_scan_after_inc;


	GtkWidget *frame_newmail;
	GtkWidget *hbox_newmail_notify;
	GtkWidget *checkbtn_newmail_auto;
	GtkWidget *checkbtn_newmail_manu;
	GtkWidget *entry_newmail_notify_cmd;
	GtkWidget *label_newmail_notify_cmd;

	GtkWidget *hbox_recvdialog;
	GtkWidget *label_recvdialog;
	GtkWidget *menu;
	GtkWidget *menuitem;
	GtkWidget *optmenu_recvdialog;
	GtkWidget *checkbtn_no_recv_err_panel;
	GtkWidget *checkbtn_close_recv_dialog;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (notebook), vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	PACK_FRAME(vbox1, frame_incext, _("External program"));

	vbox2 = gtk_vbox_new (FALSE, VSPACING_NARROW);
	gtk_widget_show (vbox2);
	gtk_container_add (GTK_CONTAINER (frame_incext), vbox2);
	gtk_container_set_border_width (GTK_CONTAINER (vbox2), 8);

	/* Use of external incorporation program */
	PACK_CHECK_BUTTON (vbox2, checkbtn_incext,
			   _("Use external program for incorporation"));

	hbox = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox, FALSE, FALSE, 0);
	SET_TOGGLE_SENSITIVITY (checkbtn_incext, hbox);

	label_incext = gtk_label_new (_("Command"));
	gtk_widget_show (label_incext);
	gtk_box_pack_start (GTK_BOX (hbox), label_incext, FALSE, FALSE, 0);

	entry_incext = gtk_entry_new ();
	gtk_widget_show (entry_incext);
	gtk_box_pack_start (GTK_BOX (hbox), entry_incext, TRUE, TRUE, 0);

#if 0
	button_incext = gtk_button_new_with_label ("... ");
	gtk_widget_show (button_incext);
	gtk_box_pack_start (GTK_BOX (hbox), button_incext, FALSE, FALSE, 0);
#endif

	vbox2 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, FALSE, 0);

	/* Auto-checking */
	hbox_autochk = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox_autochk);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox_autochk, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON (hbox_autochk, checkbtn_autochk,
			   _("Auto-check new mail"));

	label_autochk1 = gtk_label_new (_("every"));
	gtk_widget_show (label_autochk1);
	gtk_box_pack_start (GTK_BOX (hbox_autochk), label_autochk1, FALSE, FALSE, 0);

	spinbtn_autochk_adj = gtk_adjustment_new (5, 1, 100, 1, 10, 10);
	spinbtn_autochk = gtk_spin_button_new
		(GTK_ADJUSTMENT (spinbtn_autochk_adj), 1, 0);
	gtk_widget_show (spinbtn_autochk);
	gtk_box_pack_start (GTK_BOX (hbox_autochk), spinbtn_autochk, FALSE, FALSE, 0);
	gtk_widget_set_size_request (spinbtn_autochk, 64, -1);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbtn_autochk), TRUE);

	label_autochk2 = gtk_label_new (_("minute(s)"));
	gtk_widget_show (label_autochk2);
	gtk_box_pack_start (GTK_BOX (hbox_autochk), label_autochk2, FALSE, FALSE, 0);

	SET_TOGGLE_SENSITIVITY(checkbtn_autochk, label_autochk1);
	SET_TOGGLE_SENSITIVITY(checkbtn_autochk, spinbtn_autochk);
	SET_TOGGLE_SENSITIVITY(checkbtn_autochk, label_autochk2);

	PACK_CHECK_BUTTON (vbox2, checkbtn_chkonstartup,
			   _("Check new mail on startup"));
	PACK_CHECK_BUTTON (vbox2, checkbtn_scan_after_inc,
			   _("Update all local folders after incorporation"));


	/* receive dialog */
	hbox_recvdialog = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox_recvdialog);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox_recvdialog, FALSE, FALSE, 0);

	label_recvdialog = gtk_label_new (_("Show receive dialog"));
	gtk_misc_set_alignment(GTK_MISC(label_recvdialog), 0, 0.5);
	gtk_widget_show (label_recvdialog);
	gtk_box_pack_start (GTK_BOX (hbox_recvdialog), label_recvdialog, FALSE, FALSE, 0);

	optmenu_recvdialog = gtk_option_menu_new ();
	gtk_widget_show (optmenu_recvdialog);
	gtk_box_pack_start (GTK_BOX (hbox_recvdialog), optmenu_recvdialog, FALSE, FALSE, 0);

	menu = gtk_menu_new ();
	MENUITEM_ADD (menu, menuitem, _("Always"), RECV_DIALOG_ALWAYS);
	MENUITEM_ADD (menu, menuitem, _("Only on manual receiving"),
		      RECV_DIALOG_MANUAL);
	MENUITEM_ADD (menu, menuitem, _("Never"), RECV_DIALOG_NEVER);

	gtk_option_menu_set_menu (GTK_OPTION_MENU (optmenu_recvdialog), menu);

	PACK_CHECK_BUTTON (vbox2, checkbtn_no_recv_err_panel,
			   _("Don't popup error dialog on receive error"));

	PACK_CHECK_BUTTON (vbox2, checkbtn_close_recv_dialog,
			   _("Close receive dialog when finished"));
	
	PACK_FRAME(vbox1, frame_newmail, _("Run command when new mail "
					   "arrives"));
	vbox2 = gtk_vbox_new (FALSE, VSPACING_NARROW);
	gtk_widget_show (vbox2);
	gtk_container_add (GTK_CONTAINER (frame_newmail), vbox2);
	gtk_container_set_border_width (GTK_CONTAINER (vbox2), 8);

	hbox = gtk_hbox_new (TRUE, 8);
	gtk_widget_show (hbox);
	PACK_CHECK_BUTTON (hbox, checkbtn_newmail_auto,
			   _("after autochecking"));
	PACK_CHECK_BUTTON (hbox, checkbtn_newmail_manu,
			   _("after manual checking"));
	gtk_box_pack_start (GTK_BOX(vbox2), hbox, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(checkbtn_newmail_auto), "toggled",
			 G_CALLBACK(prefs_common_recv_dialog_newmail_notify_toggle_cb),
			 NULL);
	g_signal_connect(G_OBJECT(checkbtn_newmail_manu), "toggled",
			 G_CALLBACK(prefs_common_recv_dialog_newmail_notify_toggle_cb),
			 NULL);

	hbox_newmail_notify = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox_newmail_notify, FALSE, 
			    FALSE, 0);

	label_newmail_notify_cmd = gtk_label_new (_("Command to execute:\n"
						    "(use %d as number of new "
						    "mails)"));
	gtk_label_set_justify(GTK_LABEL(label_newmail_notify_cmd), 
			      GTK_JUSTIFY_RIGHT);
	gtk_widget_show (label_newmail_notify_cmd);
	gtk_box_pack_start (GTK_BOX (hbox_newmail_notify), 
			    label_newmail_notify_cmd, FALSE, FALSE, 0);

	entry_newmail_notify_cmd = gtk_entry_new ();
	gtk_widget_show (entry_newmail_notify_cmd);
	gtk_box_pack_start (GTK_BOX (hbox_newmail_notify), 
			    entry_newmail_notify_cmd, TRUE, TRUE, 0);

	gtk_widget_set_sensitive(hbox_newmail_notify, 
				 prefs_common.newmail_notify_auto || 
				 prefs_common.newmail_notify_manu);

	receive.checkbtn_incext = checkbtn_incext;
	receive.entry_incext    = entry_incext;
	/* receive.button_incext   = button_incext; */

	receive.checkbtn_autochk    = checkbtn_autochk;
	receive.spinbtn_autochk     = spinbtn_autochk;
	receive.spinbtn_autochk_adj = spinbtn_autochk_adj;

	receive.checkbtn_chkonstartup = checkbtn_chkonstartup;
	receive.checkbtn_scan_after_inc = checkbtn_scan_after_inc;


	receive.checkbtn_newmail_auto  = checkbtn_newmail_auto;
	receive.checkbtn_newmail_manu  = checkbtn_newmail_manu;
	receive.hbox_newmail_notify    = hbox_newmail_notify;
	receive.entry_newmail_notify_cmd = entry_newmail_notify_cmd;
	receive.optmenu_recvdialog	    = optmenu_recvdialog;
	receive.checkbtn_no_recv_err_panel  = checkbtn_no_recv_err_panel;
	receive.checkbtn_close_recv_dialog  = checkbtn_close_recv_dialog;
}

static void prefs_send_create(void)
{
	GtkWidget *vbox1;
	GtkWidget *vbox2;
	GtkWidget *hbox1;
	GtkWidget *checkbtn_savemsg;
	GtkWidget *label_outcharset;
	GtkWidget *optmenu_charset;
	GtkWidget *optmenu_menu;
	GtkWidget *menuitem;
	GtkTooltips *charset_tooltip;
	GtkWidget *optmenu_encoding;
	GtkWidget *label_encoding;
	GtkTooltips *encoding_tooltip;
	GtkWidget *label_senddialog;
	GtkWidget *menu;
	GtkWidget *optmenu_senddialog;
	GtkWidget *hbox_senddialog;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (notebook), vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	vbox2 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON (vbox2, checkbtn_savemsg,
			   _("Save sent messages to Sent folder"));

	hbox_senddialog = gtk_hbox_new (FALSE, 8);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox_senddialog, FALSE, FALSE, 0);

	label_senddialog = gtk_label_new (_("Show send dialog"));
	gtk_widget_show (label_senddialog);
	gtk_box_pack_start (GTK_BOX (hbox_senddialog), label_senddialog, FALSE, FALSE, 0);

	optmenu_senddialog = gtk_option_menu_new ();
	gtk_widget_show (optmenu_senddialog);
	gtk_box_pack_start (GTK_BOX (hbox_senddialog), optmenu_senddialog, FALSE, FALSE, 0);
	
	menu = gtk_menu_new ();
	MENUITEM_ADD (menu, menuitem, _("Always"), SEND_DIALOG_ALWAYS);
	MENUITEM_ADD (menu, menuitem, _("Never"), SEND_DIALOG_NEVER);

	gtk_option_menu_set_menu (GTK_OPTION_MENU (optmenu_senddialog), menu);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);

	label_outcharset = gtk_label_new (_("Outgoing encoding"));
	gtk_widget_show (label_outcharset);
	gtk_box_pack_start (GTK_BOX (hbox1), label_outcharset, FALSE, FALSE, 0);

	charset_tooltip = gtk_tooltips_new();

	optmenu_charset = gtk_option_menu_new ();
	gtk_widget_show (optmenu_charset);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(charset_tooltip), optmenu_charset,
			     _("If `Automatic' is selected, the optimal encoding"
		   	       " for the current locale will be used"),
			     NULL);
 	gtk_box_pack_start (GTK_BOX (hbox1), optmenu_charset, FALSE, FALSE, 0);

	optmenu_menu = gtk_menu_new ();

#define SET_MENUITEM(str, data) \
{ \
	MENUITEM_ADD(optmenu_menu, menuitem, str, data); \
}

	SET_MENUITEM(_("Automatic (Recommended)"),	 CS_AUTO);
	SET_MENUITEM(NULL, NULL);
	SET_MENUITEM(_("7bit ascii (US-ASCII)"),	 CS_US_ASCII);
	SET_MENUITEM(NULL, NULL);
	SET_MENUITEM(_("Unicode (UTF-8)"),		 CS_UTF_8);
	SET_MENUITEM(NULL, NULL);
	SET_MENUITEM(_("Western European (ISO-8859-1)"),  CS_ISO_8859_1);
	SET_MENUITEM(_("Western European (ISO-8859-15)"), CS_ISO_8859_15);
	SET_MENUITEM(NULL, NULL);
	SET_MENUITEM(_("Central European (ISO-8859-2)"),  CS_ISO_8859_2);
	SET_MENUITEM(NULL, NULL);
	SET_MENUITEM(_("Baltic (ISO-8859-13)"),		  CS_ISO_8859_13);
	SET_MENUITEM(_("Baltic (ISO-8859-4)"),		  CS_ISO_8859_4);
	SET_MENUITEM(NULL, NULL);
	SET_MENUITEM(_("Greek (ISO-8859-7)"),		  CS_ISO_8859_7);
	SET_MENUITEM(NULL, NULL);
	SET_MENUITEM(_("Turkish (ISO-8859-9)"),		  CS_ISO_8859_9);
	SET_MENUITEM(NULL, NULL);
	SET_MENUITEM(_("Cyrillic (ISO-8859-5)"),	  CS_ISO_8859_5);
	SET_MENUITEM(_("Cyrillic (KOI8-R)"),		 CS_KOI8_R);
	SET_MENUITEM(_("Cyrillic (KOI8-U)"),		 CS_KOI8_U);
	SET_MENUITEM(_("Cyrillic (Windows-1251)"),	 CS_WINDOWS_1251);
	SET_MENUITEM(NULL, NULL);
	SET_MENUITEM(_("Japanese (ISO-2022-JP)"),	 CS_ISO_2022_JP);
#if 0
	SET_MENUITEM(_("Japanese (EUC-JP)"),		 CS_EUC_JP);
	SET_MENUITEM(_("Japanese (Shift_JIS)"),		 CS_SHIFT_JIS);
#endif /* 0 */
	SET_MENUITEM(NULL, NULL);
	SET_MENUITEM(_("Simplified Chinese (GB2312)"),	 CS_GB2312);
	SET_MENUITEM(_("Simplified Chinese (GBK)"),	 CS_GBK);
	SET_MENUITEM(_("Traditional Chinese (Big5)"),	 CS_BIG5);
#if 0
	SET_MENUITEM(_("Traditional Chinese (EUC-TW)"),  CS_EUC_TW);
	SET_MENUITEM(_("Chinese (ISO-2022-CN)"),	 CS_ISO_2022_CN);
#endif /* 0 */
	SET_MENUITEM(NULL, NULL);
	SET_MENUITEM(_("Korean (EUC-KR)"),		 CS_EUC_KR);
	SET_MENUITEM(NULL, NULL);
	SET_MENUITEM(_("Thai (TIS-620)"),		 CS_TIS_620);
	SET_MENUITEM(_("Thai (Windows-874)"),		 CS_WINDOWS_874);

	gtk_option_menu_set_menu (GTK_OPTION_MENU (optmenu_charset),
				  optmenu_menu);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);

	label_encoding = gtk_label_new (_("Transfer encoding"));
	gtk_widget_show (label_encoding);
	gtk_box_pack_start (GTK_BOX (hbox1), label_encoding, FALSE, FALSE, 0);

	encoding_tooltip = gtk_tooltips_new();

	optmenu_encoding = gtk_option_menu_new ();
	gtk_widget_show (optmenu_encoding);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(encoding_tooltip), optmenu_encoding,
			     _("Specify Content-Transfer-Encoding used when"
		   	       " message body contains non-ASCII characters"),
			     NULL);
 	gtk_box_pack_start (GTK_BOX (hbox1), optmenu_encoding, FALSE, FALSE, 0);

	optmenu_menu = gtk_menu_new ();

	SET_MENUITEM(_("Automatic"),	 CTE_AUTO);
	SET_MENUITEM("base64",		 CTE_BASE64);
	SET_MENUITEM("quoted-printable", CTE_QUOTED_PRINTABLE);
	SET_MENUITEM("8bit",		 CTE_8BIT);

	gtk_option_menu_set_menu (GTK_OPTION_MENU (optmenu_encoding),
				  optmenu_menu);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);

	p_send.checkbtn_savemsg  = checkbtn_savemsg;
	p_send.optmenu_senddialog = optmenu_senddialog;

	p_send.optmenu_charset = optmenu_charset;
	p_send.optmenu_encoding_method = optmenu_encoding;
}

static void prefs_common_recv_dialog_newmail_notify_toggle_cb(GtkWidget *w, gpointer data)
{
	gboolean toggled;

	toggled = gtk_toggle_button_get_active
			(GTK_TOGGLE_BUTTON(receive.checkbtn_newmail_manu)) ||
		  gtk_toggle_button_get_active
			(GTK_TOGGLE_BUTTON(receive.checkbtn_newmail_auto));
	gtk_widget_set_sensitive(receive.hbox_newmail_notify, toggled);
}

static void prefs_display_create(void)
{
	GtkWidget *vbox1;
	GtkWidget *chkbtn_transhdr;
	GtkWidget *chkbtn_folder_unread;
	GtkWidget *hbox1;
	GtkWidget *label_ng_abbrev;
	GtkWidget *spinbtn_ng_abbrev_len;
	GtkObject *spinbtn_ng_abbrev_len_adj;
	GtkWidget *frame_summary;
	GtkWidget *vbox2;
	GtkWidget *chkbtn_swapfrom;
	GtkWidget *chkbtn_useaddrbook;
	GtkWidget *chkbtn_threadsubj;
	GtkWidget *vbox3;
	GtkWidget *label_datefmt;
	GtkWidget *button_datefmt;
	GtkWidget *entry_datefmt;
	GtkWidget *button_dispitem;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (notebook), vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	vbox2 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, TRUE, 0);

	PACK_CHECK_BUTTON
		(vbox2, chkbtn_transhdr,
		 _("Translate header name (such as `From:', `Subject:')"));

	PACK_CHECK_BUTTON (vbox2, chkbtn_folder_unread,
			   _("Display unread number next to folder name"));

	PACK_VSPACER(vbox2, vbox3, VSPACING_NARROW_2);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, TRUE, 0);

	label_ng_abbrev = gtk_label_new
		(_("Abbreviate newsgroup names longer than"));
	gtk_widget_show (label_ng_abbrev);
	gtk_box_pack_start (GTK_BOX (hbox1), label_ng_abbrev, FALSE, FALSE, 0);

	spinbtn_ng_abbrev_len_adj = gtk_adjustment_new (16, 0, 999, 1, 10, 10);
	spinbtn_ng_abbrev_len = gtk_spin_button_new
		(GTK_ADJUSTMENT (spinbtn_ng_abbrev_len_adj), 1, 0);
	gtk_widget_show (spinbtn_ng_abbrev_len);
	gtk_box_pack_start (GTK_BOX (hbox1), spinbtn_ng_abbrev_len,
			    FALSE, FALSE, 0);
	gtk_widget_set_size_request (spinbtn_ng_abbrev_len, 56, -1);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbtn_ng_abbrev_len),
				     TRUE);

	label_ng_abbrev = gtk_label_new
		(_("letters"));
	gtk_widget_show (label_ng_abbrev);
	gtk_box_pack_start (GTK_BOX (hbox1), label_ng_abbrev, FALSE, FALSE, 0);

	/* ---- Summary ---- */

	PACK_FRAME(vbox1, frame_summary, _("Summary View"));

	vbox2 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox2);
	gtk_container_add (GTK_CONTAINER (frame_summary), vbox2);
	gtk_container_set_border_width (GTK_CONTAINER (vbox2), 8);

	PACK_CHECK_BUTTON
		(vbox2, chkbtn_swapfrom,
		 _("Display recipient in `From' column if sender is yourself"));
	PACK_CHECK_BUTTON
		(vbox2, chkbtn_useaddrbook,
		 _("Display sender using address book"));
	PACK_CHECK_BUTTON
		(vbox2, chkbtn_threadsubj,
		 _("Thread using subject in addition to standard headers"));

	PACK_VSPACER(vbox2, vbox3, VSPACING_NARROW_2);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, TRUE, 0);

	label_datefmt = gtk_label_new (_("Date format"));
	gtk_widget_show (label_datefmt);
	gtk_box_pack_start (GTK_BOX (hbox1), label_datefmt, FALSE, FALSE, 0);

	entry_datefmt = gtk_entry_new ();
	gtk_widget_show (entry_datefmt);
	gtk_box_pack_start (GTK_BOX (hbox1), entry_datefmt, TRUE, TRUE, 0);

	button_datefmt = gtk_button_new_with_label (" ... ");

	gtk_widget_show (button_datefmt);
	gtk_box_pack_start (GTK_BOX (hbox1), button_datefmt, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (button_datefmt), "clicked",
			  G_CALLBACK (date_format_create), NULL);

	PACK_VSPACER(vbox2, vbox3, VSPACING_NARROW);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, TRUE, 0);

	button_dispitem = gtk_button_new_with_label
		(_(" Set displayed items in summary... "));
	gtk_widget_show (button_dispitem);
	gtk_box_pack_start (GTK_BOX (hbox1), button_dispitem, FALSE, TRUE, 0);
	g_signal_connect (G_OBJECT (button_dispitem), "clicked",
			  G_CALLBACK (prefs_summary_column_open),
			  NULL);

	display.chkbtn_transhdr           = chkbtn_transhdr;
	display.chkbtn_folder_unread      = chkbtn_folder_unread;
	display.spinbtn_ng_abbrev_len     = spinbtn_ng_abbrev_len;
	display.spinbtn_ng_abbrev_len_adj = spinbtn_ng_abbrev_len_adj;

	display.chkbtn_swapfrom      = chkbtn_swapfrom;
	display.chkbtn_useaddrbook   = chkbtn_useaddrbook;
	display.chkbtn_threadsubj    = chkbtn_threadsubj;
	display.entry_datefmt        = entry_datefmt;
}

static void prefs_message_create(void)
{
	GtkWidget *vbox1;
	GtkWidget *vbox2;
	GtkWidget *vbox3;
	GtkWidget *hbox1;
	GtkWidget *chkbtn_mbalnum;
	GtkWidget *chkbtn_disphdrpane;
	GtkWidget *chkbtn_disphdr;
	GtkWidget *button_edit_disphdr;
	GtkWidget *chkbtn_html;
	GtkWidget *chkbtn_cursor;
	GtkWidget *hbox_linespc;
	GtkWidget *label_linespc;
	GtkObject *spinbtn_linespc_adj;
	GtkWidget *spinbtn_linespc;

	GtkWidget *frame_scr;
	GtkWidget *vbox_scr;
	GtkWidget *chkbtn_smoothscroll;
	GtkWidget *hbox_scr;
	GtkWidget *label_scr;
	GtkObject *spinbtn_scrollstep_adj;
	GtkWidget *spinbtn_scrollstep;
	GtkWidget *chkbtn_halfpage;

	GtkWidget *chkbtn_attach_desc;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (notebook), vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	vbox2 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON
		(vbox2, chkbtn_mbalnum,
		 _("Display multi-byte alphanumeric as\n"
		   "ASCII character (Japanese only)"));
	gtk_label_set_justify (GTK_LABEL (GTK_BIN(chkbtn_mbalnum)->child),
			       GTK_JUSTIFY_LEFT);

	PACK_CHECK_BUTTON(vbox2, chkbtn_disphdrpane,
			  _("Display header pane above message view"));

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, TRUE, 0);

	PACK_CHECK_BUTTON(hbox1, chkbtn_disphdr,
			  _("Display short headers on message view"));

	button_edit_disphdr = gtk_button_new_with_label (_(" Edit... "));
	gtk_widget_show (button_edit_disphdr);
	gtk_box_pack_end (GTK_BOX (hbox1), button_edit_disphdr,
			  FALSE, TRUE, 0);
	g_signal_connect (G_OBJECT (button_edit_disphdr), "clicked",
			  G_CALLBACK (prefs_display_header_open),
			  NULL);

	SET_TOGGLE_SENSITIVITY(chkbtn_disphdr, button_edit_disphdr);

	PACK_CHECK_BUTTON(vbox2, chkbtn_html,
			  _("Render HTML messages as text"));

	PACK_CHECK_BUTTON(vbox2, chkbtn_cursor,
			  _("Display cursor in message view"));

	PACK_VSPACER(vbox2, vbox3, VSPACING_NARROW_2);

	hbox1 = gtk_hbox_new (FALSE, 32);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, TRUE, 0);

	hbox_linespc = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (hbox1), hbox_linespc, FALSE, TRUE, 0);

	label_linespc = gtk_label_new (_("Line space"));
	gtk_widget_show (label_linespc);
	gtk_box_pack_start (GTK_BOX (hbox_linespc), label_linespc,
			    FALSE, FALSE, 0);

	spinbtn_linespc_adj = gtk_adjustment_new (2, 0, 16, 1, 1, 16);
	spinbtn_linespc = gtk_spin_button_new
		(GTK_ADJUSTMENT (spinbtn_linespc_adj), 1, 0);
	gtk_widget_show (spinbtn_linespc);
	gtk_box_pack_start (GTK_BOX (hbox_linespc), spinbtn_linespc,
			    FALSE, FALSE, 0);
	gtk_widget_set_size_request (spinbtn_linespc, 64, -1);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbtn_linespc), TRUE);

	label_linespc = gtk_label_new (_("pixel(s)"));
	gtk_widget_show (label_linespc);
	gtk_box_pack_start (GTK_BOX (hbox_linespc), label_linespc,
			    FALSE, FALSE, 0);

	PACK_FRAME(vbox1, frame_scr, _("Scroll"));

	vbox_scr = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox_scr);
	gtk_container_add (GTK_CONTAINER (frame_scr), vbox_scr);
	gtk_container_set_border_width (GTK_CONTAINER (vbox_scr), 8);

	PACK_CHECK_BUTTON(vbox_scr, chkbtn_halfpage, _("Half page"));

	hbox1 = gtk_hbox_new (FALSE, 32);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox_scr), hbox1, FALSE, TRUE, 0);

	PACK_CHECK_BUTTON(hbox1, chkbtn_smoothscroll, _("Smooth scroll"));

	hbox_scr = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox_scr);
	gtk_box_pack_start (GTK_BOX (hbox1), hbox_scr, FALSE, FALSE, 0);

	label_scr = gtk_label_new (_("Step"));
	gtk_widget_show (label_scr);
	gtk_box_pack_start (GTK_BOX (hbox_scr), label_scr, FALSE, FALSE, 0);

	spinbtn_scrollstep_adj = gtk_adjustment_new (1, 1, 100, 1, 10, 10);
	spinbtn_scrollstep = gtk_spin_button_new
		(GTK_ADJUSTMENT (spinbtn_scrollstep_adj), 1, 0);
	gtk_widget_show (spinbtn_scrollstep);
	gtk_box_pack_start (GTK_BOX (hbox_scr), spinbtn_scrollstep,
			    FALSE, FALSE, 0);
	gtk_widget_set_size_request (spinbtn_scrollstep, 64, -1);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbtn_scrollstep),
				     TRUE);

	label_scr = gtk_label_new (_("pixel(s)"));
	gtk_widget_show (label_scr);
	gtk_box_pack_start (GTK_BOX (hbox_scr), label_scr, FALSE, FALSE, 0);

	SET_TOGGLE_SENSITIVITY (chkbtn_smoothscroll, hbox_scr)

	vbox3 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox3);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox3, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON(vbox3, chkbtn_attach_desc,
			  _("Show attachment descriptions (rather than names)"));

	message.chkbtn_mbalnum     = chkbtn_mbalnum;
	message.chkbtn_disphdrpane = chkbtn_disphdrpane;
	message.chkbtn_disphdr     = chkbtn_disphdr;
	message.chkbtn_html        = chkbtn_html;
	message.chkbtn_cursor      = chkbtn_cursor;
	message.spinbtn_linespc    = spinbtn_linespc;

	message.chkbtn_smoothscroll    = chkbtn_smoothscroll;
	message.spinbtn_scrollstep     = spinbtn_scrollstep;
	message.spinbtn_scrollstep_adj = spinbtn_scrollstep_adj;
	message.chkbtn_halfpage        = chkbtn_halfpage;

	message.chkbtn_attach_desc  = chkbtn_attach_desc;
}

static void prefs_interface_create(void)
{
	GtkWidget *vbox1;
	GtkWidget *vbox2;
	GtkWidget *vbox3;
	/* GtkWidget *checkbtn_emacs; */
	GtkWidget *checkbtn_always_show_msg;
	GtkWidget *checkbtn_openunread;
	GtkWidget *checkbtn_mark_as_read_on_newwin;
	GtkWidget *checkbtn_openinbox;
	GtkWidget *checkbtn_immedexec;
	GtkTooltips *immedexec_tooltip;
	GtkWidget *hbox1;
	GtkWidget *label;
	GtkWidget *menu;
	GtkWidget *menuitem;

	GtkWidget *button_keybind;

	GtkWidget *hbox_nextunreadmsgdialog;
 	GtkWidget *optmenu_nextunreadmsgdialog;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (notebook), vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	vbox2 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, FALSE, 0);

	/* PACK_CHECK_BUTTON (vbox2, checkbtn_emacs,
			   _("Emulate the behavior of mouse operation of\n"
			     "Emacs-based mailer"));
	gtk_label_set_justify (GTK_LABEL (GTK_BIN (checkbtn_emacs)->child),
			       GTK_JUSTIFY_LEFT);   */

	PACK_CHECK_BUTTON
		(vbox2, checkbtn_always_show_msg,
		 _("Always open messages in summary when selected"));

	PACK_CHECK_BUTTON
		(vbox2, checkbtn_openunread,
		 _("Open first unread message when entering a folder"));

	PACK_CHECK_BUTTON
		(vbox2, checkbtn_mark_as_read_on_newwin,
		 _("Only mark message as read when opened in new window"));

	PACK_CHECK_BUTTON
		(vbox2, checkbtn_openinbox,
		 _("Go to inbox after receiving new mail"));

	vbox3 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox3);
	gtk_box_pack_start (GTK_BOX (vbox2), vbox3, FALSE, FALSE, 0);

	immedexec_tooltip = gtk_tooltips_new();

	PACK_CHECK_BUTTON
		(vbox3, checkbtn_immedexec,
		 _("Execute immediately when moving or deleting messages"));
	gtk_tooltips_set_tip(GTK_TOOLTIPS(immedexec_tooltip), checkbtn_immedexec,
			     _("Messages will be marked until execution"
		   	       " if this is turned off"),
			     NULL);

	hbox1 = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox3), hbox1, FALSE, FALSE, 0);

 	/* Next Unread Message Dialog */
	hbox_nextunreadmsgdialog = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox_nextunreadmsgdialog, FALSE, FALSE, 0);

	label = gtk_label_new (_("Show no-unread-message dialog"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox_nextunreadmsgdialog), label, FALSE, FALSE, 8);

 	optmenu_nextunreadmsgdialog = gtk_option_menu_new ();
 	gtk_widget_show (optmenu_nextunreadmsgdialog);
	gtk_box_pack_start (GTK_BOX (hbox_nextunreadmsgdialog), optmenu_nextunreadmsgdialog, FALSE, FALSE, 8);
	
	menu = gtk_menu_new ();
	MENUITEM_ADD (menu, menuitem, _("Always"), NEXTUNREADMSGDIALOG_ALWAYS);
	MENUITEM_ADD (menu, menuitem, _("Assume 'Yes'"), 
		      NEXTUNREADMSGDIALOG_ASSUME_YES);
	MENUITEM_ADD (menu, menuitem, _("Assume 'No'"), 
		      NEXTUNREADMSGDIALOG_ASSUME_NO);

	gtk_option_menu_set_menu (GTK_OPTION_MENU (optmenu_nextunreadmsgdialog), menu);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);

	button_keybind = gtk_button_new_with_label (_(" Set key bindings... "));
	gtk_widget_show (button_keybind);
	gtk_box_pack_start (GTK_BOX (hbox1), button_keybind, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (button_keybind), "clicked",
			  G_CALLBACK (prefs_keybind_select), NULL);

	/* interface.checkbtn_emacs          = checkbtn_emacs; */
	interface.checkbtn_always_show_msg    = checkbtn_always_show_msg;
	interface.checkbtn_openunread         = checkbtn_openunread;
	interface.checkbtn_mark_as_read_on_newwin
					      = checkbtn_mark_as_read_on_newwin;
	interface.checkbtn_openinbox          = checkbtn_openinbox;
	interface.checkbtn_immedexec          = checkbtn_immedexec;
	interface.optmenu_nextunreadmsgdialog = optmenu_nextunreadmsgdialog;
}

static void prefs_other_create(void)
{
	GtkWidget *vbox1;
	GtkWidget *hbox1;

	GtkWidget *frame_addr;
	GtkWidget *vbox_addr;
	GtkWidget *checkbtn_addaddrbyclick;
	
	GtkWidget *frame_cliplog;
	GtkWidget *vbox_cliplog;
	GtkWidget *hbox_cliplog;
	GtkWidget *checkbtn_cliplog;
	GtkWidget *loglength_label;
	GtkWidget *loglength_entry;
	GtkTooltips *loglength_tooltip;

	GtkWidget *frame_exit;
	GtkWidget *vbox_exit;
	GtkWidget *checkbtn_confonexit;
	GtkWidget *checkbtn_cleanonexit;
	GtkWidget *checkbtn_askonclean;
	GtkWidget *checkbtn_warnqueued;

	GtkWidget *label_iotimeout;
	GtkWidget *spinbtn_iotimeout;
	GtkObject *spinbtn_iotimeout_adj;

#if 0
#ifdef USE_OPENSSL
	GtkWidget *frame_ssl;
	GtkWidget *vbox_ssl;
	GtkWidget *hbox_ssl;
	GtkWidget *checkbtn_ssl_ask_unknown_valid;
#endif
#endif
	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (notebook), vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	PACK_FRAME (vbox1, frame_addr, _("Address book"));

	vbox_addr = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox_addr);
	gtk_container_add (GTK_CONTAINER (frame_addr), vbox_addr);
	gtk_container_set_border_width (GTK_CONTAINER (vbox_addr), 8);

	PACK_CHECK_BUTTON
		(vbox_addr, checkbtn_addaddrbyclick,
		 _("Add address to destination when double-clicked"));

	/* Clip Log */
	PACK_FRAME (vbox1, frame_cliplog, _("Log Size"));

	vbox_cliplog = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox_cliplog);
	gtk_container_add (GTK_CONTAINER (frame_cliplog), vbox_cliplog);
	gtk_container_set_border_width (GTK_CONTAINER (vbox_cliplog), 8);
	PACK_CHECK_BUTTON (vbox_cliplog, checkbtn_cliplog,
			   _("Clip the log size"));
	hbox_cliplog = gtk_hbox_new (FALSE, 3);
	gtk_container_add (GTK_CONTAINER (vbox_cliplog), hbox_cliplog);
	gtk_widget_show (hbox_cliplog);
	
	loglength_label = gtk_label_new (_("Log window length"));
	gtk_box_pack_start (GTK_BOX (hbox_cliplog), loglength_label,
			    FALSE, TRUE, 0);
	gtk_widget_show (GTK_WIDGET (loglength_label));
	
	loglength_tooltip = gtk_tooltips_new();
	
	loglength_entry = gtk_entry_new ();
	gtk_widget_set_size_request (GTK_WIDGET (loglength_entry), 64, -1);
	gtk_box_pack_start (GTK_BOX (hbox_cliplog), loglength_entry,
			    FALSE, TRUE, 0);
	gtk_widget_show (GTK_WIDGET (loglength_entry));
	gtk_tooltips_set_tip(GTK_TOOLTIPS(loglength_tooltip), loglength_entry,
			     _("0 to stop logging in the log window"),
			     NULL);
	SET_TOGGLE_SENSITIVITY(checkbtn_cliplog, loglength_entry);

#if 0
#ifdef USE_OPENSSL
	/* SSL */
	PACK_FRAME (vbox1, frame_ssl, _("Security"));

	vbox_ssl = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox_ssl);
	gtk_container_add (GTK_CONTAINER (frame_ssl), vbox_ssl);
	gtk_container_set_border_width (GTK_CONTAINER (vbox_ssl), 8);
	PACK_CHECK_BUTTON (vbox_ssl, checkbtn_ssl_ask_unknown_valid, 
			   _("Ask before accepting SSL certificates"));
	hbox_ssl = gtk_hbox_new (FALSE, 3);
	gtk_container_add (GTK_CONTAINER (vbox_ssl), hbox_ssl);
	gtk_widget_show (hbox_ssl);
#endif
#endif
	
	/* On Exit */
	PACK_FRAME (vbox1, frame_exit, _("On exit"));

	vbox_exit = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox_exit);
	gtk_container_add (GTK_CONTAINER (frame_exit), vbox_exit);
	gtk_container_set_border_width (GTK_CONTAINER (vbox_exit), 8);

	PACK_CHECK_BUTTON (vbox_exit, checkbtn_confonexit,
			   _("Confirm on exit"));

	hbox1 = gtk_hbox_new (FALSE, 32);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox_exit), hbox1, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON (hbox1, checkbtn_cleanonexit,
			   _("Empty trash on exit"));
	PACK_CHECK_BUTTON (hbox1, checkbtn_askonclean,
			   _("Ask before emptying"));
	SET_TOGGLE_SENSITIVITY (checkbtn_cleanonexit, checkbtn_askonclean);

	PACK_CHECK_BUTTON (vbox_exit, checkbtn_warnqueued,
			   _("Warn if there are queued messages"));

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);

	label_iotimeout = gtk_label_new (_("Socket I/O timeout:"));
	gtk_widget_show (label_iotimeout);
	gtk_box_pack_start (GTK_BOX (hbox1), label_iotimeout, FALSE, FALSE, 0);

	spinbtn_iotimeout_adj = gtk_adjustment_new (60, 0, 1000, 1, 10, 10);
	spinbtn_iotimeout = gtk_spin_button_new
		(GTK_ADJUSTMENT (spinbtn_iotimeout_adj), 1, 0);
	gtk_widget_show (spinbtn_iotimeout);
	gtk_box_pack_start (GTK_BOX (hbox1), spinbtn_iotimeout,
			    FALSE, FALSE, 0);
	gtk_widget_set_size_request (spinbtn_iotimeout, 64, -1);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbtn_iotimeout), TRUE);

	label_iotimeout = gtk_label_new (_("seconds"));
	gtk_widget_show (label_iotimeout);
	gtk_box_pack_start (GTK_BOX (hbox1), label_iotimeout, FALSE, FALSE, 0);

	other.checkbtn_addaddrbyclick = checkbtn_addaddrbyclick;
	
	other.checkbtn_cliplog     = checkbtn_cliplog;
	other.loglength_entry      = loglength_entry;

	other.checkbtn_confonexit  = checkbtn_confonexit;
	other.checkbtn_cleanonexit = checkbtn_cleanonexit;
	other.checkbtn_askonclean  = checkbtn_askonclean;
	other.checkbtn_warnqueued  = checkbtn_warnqueued;

	other.spinbtn_iotimeout     = spinbtn_iotimeout;
	other.spinbtn_iotimeout_adj = spinbtn_iotimeout_adj;
	
#if 0
#ifdef USE_OPENSSL
	other.checkbtn_ssl_ask_unknown_valid = checkbtn_ssl_ask_unknown_valid;
#endif
#endif
}

static void date_format_ok_btn_clicked(GtkButton *button, GtkWidget **widget)
{
	GtkWidget *datefmt_sample = NULL;
	gchar *text;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(*widget != NULL);
	g_return_if_fail(display.entry_datefmt != NULL);

	datefmt_sample = GTK_WIDGET(g_object_get_data
				    (G_OBJECT(*widget), "datefmt_sample"));
	g_return_if_fail(datefmt_sample != NULL);

	text = gtk_editable_get_chars(GTK_EDITABLE(datefmt_sample), 0, -1);
	g_free(prefs_common.date_format);
	prefs_common.date_format = text;
	gtk_entry_set_text(GTK_ENTRY(display.entry_datefmt), text);

	gtk_widget_destroy(*widget);
	*widget = NULL;
}

static void date_format_cancel_btn_clicked(GtkButton *button,
					   GtkWidget **widget)
{
	g_return_if_fail(widget != NULL);
	g_return_if_fail(*widget != NULL);

	gtk_widget_destroy(*widget);
	*widget = NULL;
}

static gboolean date_format_key_pressed(GtkWidget *keywidget, GdkEventKey *event,
					GtkWidget **widget)
{
	if (event && event->keyval == GDK_Escape)
		date_format_cancel_btn_clicked(NULL, widget);
	return FALSE;
}

static gboolean date_format_on_delete(GtkWidget *dialogwidget,
				      GdkEventAny *event, GtkWidget **widget)
{
	g_return_val_if_fail(widget != NULL, FALSE);
	g_return_val_if_fail(*widget != NULL, FALSE);

	*widget = NULL;
	return FALSE;
}

static void date_format_entry_on_change(GtkEditable *editable,
					GtkLabel *example)
{
	time_t cur_time;
	struct tm *cal_time;
	gchar buffer[100];
	gchar *text;

	cur_time = time(NULL);
	cal_time = localtime(&cur_time);
	buffer[0] = 0;
	text = gtk_editable_get_chars(editable, 0, -1);
	if (text)
		strftime(buffer, sizeof buffer, text, cal_time); 
	g_free(text);

	text = conv_codeset_strdup(buffer,
				   conv_get_locale_charset_str(),
				   CS_UTF_8);
	if (!text)
		text = g_strdup(buffer);

	gtk_label_set_text(example, text);

	g_free(text);
}

static void date_format_select_row(GtkTreeView *list_view,
				   GtkTreePath *path,
				   GtkTreeViewColumn *column,
				   GtkWidget *date_format)
{
	gint cur_pos;
	gchar *format;
	const gchar *old_format;
	gchar *new_format;
	GtkWidget *datefmt_sample;
	GtkTreeIter iter;
	GtkTreeModel *model;
	
	g_return_if_fail(date_format != NULL);

	/* only on double click */
	datefmt_sample = GTK_WIDGET(g_object_get_data(G_OBJECT(date_format), 
						      "datefmt_sample"));

	g_return_if_fail(datefmt_sample != NULL);

	model = gtk_tree_view_get_model(list_view);

	/* get format from list */
	if (!gtk_tree_model_get_iter(model, &iter, path))
		return;

	gtk_tree_model_get(model, &iter, DATEFMT_FMT, &format, -1);		
	
	cur_pos = gtk_editable_get_position(GTK_EDITABLE(datefmt_sample));
	old_format = gtk_entry_get_text(GTK_ENTRY(datefmt_sample));

	/* insert the format into the text entry */
	new_format = g_malloc(strlen(old_format) + 3);

	strncpy(new_format, old_format, cur_pos);
	new_format[cur_pos] = '\0';
	strcat(new_format, format);
	strcat(new_format, &old_format[cur_pos]);

	gtk_entry_set_text(GTK_ENTRY(datefmt_sample), new_format);
	gtk_editable_set_position(GTK_EDITABLE(datefmt_sample), cur_pos + 2);

	g_free(new_format);
}

static GtkWidget *date_format_create(GtkButton *button, void *data)
{
	static GtkWidget *datefmt_win = NULL;

	GtkWidget *vbox1;
	GtkWidget *scrolledwindow1;
	GtkWidget *datefmt_list_view;
	GtkWidget *table;
	GtkWidget *label1;
	GtkWidget *label2;
	GtkWidget *label3;
	GtkWidget *confirm_area;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;
	GtkWidget *datefmt_entry;
	GtkListStore *store;

	struct {
		gchar *fmt;
		gchar *txt;
	} time_format[] = {
		{ "%a", NULL },
		{ "%A", NULL },
		{ "%b", NULL },
		{ "%B", NULL },
		{ "%c", NULL },
		{ "%C", NULL },
		{ "%d", NULL },
		{ "%H", NULL },
		{ "%I", NULL },
		{ "%j", NULL },
		{ "%m", NULL },
		{ "%M", NULL },
		{ "%p", NULL },
		{ "%S", NULL },
		{ "%w", NULL },
		{ "%x", NULL },
		{ "%y", NULL },
		{ "%Y", NULL },
		{ "%Z", NULL }
	};

	gchar *titles[2];
	gint i;
	const gint TIME_FORMAT_ELEMS =
		sizeof time_format / sizeof time_format[0];

	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;

	time_format[0].txt  = _("the full abbreviated weekday name");
	time_format[1].txt  = _("the full weekday name");
	time_format[2].txt  = _("the abbreviated month name");
	time_format[3].txt  = _("the full month name");
	time_format[4].txt  = _("the preferred date and time for the current locale");
	time_format[5].txt  = _("the century number (year/100)");
	time_format[6].txt  = _("the day of the month as a decimal number");
	time_format[7].txt  = _("the hour as a decimal number using a 24-hour clock");
	time_format[8].txt  = _("the hour as a decimal number using a 12-hour clock");
	time_format[9].txt  = _("the day of the year as a decimal number");
	time_format[10].txt = _("the month as a decimal number");
	time_format[11].txt = _("the minute as a decimal number");
	time_format[12].txt = _("either AM or PM");
	time_format[13].txt = _("the second as a decimal number");
	time_format[14].txt = _("the day of the week as a decimal number");
	time_format[15].txt = _("the preferred date for the current locale");
	time_format[16].txt = _("the last two digits of a year");
	time_format[17].txt = _("the year as a decimal number");
	time_format[18].txt = _("the time zone or name or abbreviation");

	if (datefmt_win) return datefmt_win;

	store = gtk_list_store_new(N_DATEFMT_COLUMNS,
				   G_TYPE_STRING,
				   G_TYPE_STRING,
				   -1);

	for (i = 0; i < TIME_FORMAT_ELEMS; i++) {
		GtkTreeIter iter;

		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
				   DATEFMT_FMT, time_format[i].fmt,
				   DATEFMT_TXT, time_format[i].txt,
				   -1);
	}

	datefmt_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_container_set_border_width(GTK_CONTAINER(datefmt_win), 8);
	gtk_window_set_title(GTK_WINDOW(datefmt_win), _("Date format"));
	gtk_window_set_position(GTK_WINDOW(datefmt_win), GTK_WIN_POS_CENTER);
	gtk_widget_set_size_request(datefmt_win, 440, 280);

	vbox1 = gtk_vbox_new(FALSE, 10);
	gtk_widget_show(vbox1);
	gtk_container_add(GTK_CONTAINER(datefmt_win), vbox1);

	scrolledwindow1 = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy
		(GTK_SCROLLED_WINDOW(scrolledwindow1),
		 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_show(scrolledwindow1);
	gtk_box_pack_start(GTK_BOX(vbox1), scrolledwindow1, TRUE, TRUE, 0);

	datefmt_list_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	g_object_unref(G_OBJECT(store));
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(datefmt_list_view), TRUE);
	gtk_widget_show(datefmt_list_view);
	gtk_container_add(GTK_CONTAINER(scrolledwindow1), datefmt_list_view);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes
			(_("Specifier"), renderer, "text", DATEFMT_FMT,
			 NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(datefmt_list_view), column);
	
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes
			(_("Description"), renderer, "text", DATEFMT_TXT,
			 NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(datefmt_list_view), column);
	
	/* gtk_clist_set_column_width(GTK_CLIST(datefmt_clist), 0, 80); */
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(datefmt_list_view));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);

	g_signal_connect(G_OBJECT(datefmt_list_view), "row_activated", 
			 G_CALLBACK(date_format_select_row),
			 datefmt_win);
	
	table = gtk_table_new(2, 2, FALSE);
	gtk_widget_show(table);
	gtk_box_pack_start(GTK_BOX(vbox1), table, FALSE, FALSE, 0);
	gtk_table_set_row_spacings(GTK_TABLE(table), 4);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8);

	label1 = gtk_label_new(_("Date format"));
	gtk_widget_show(label1);
	gtk_table_attach(GTK_TABLE(table), label1, 0, 1, 0, 1,
			 GTK_FILL, 0, 0, 0);
	gtk_label_set_justify(GTK_LABEL(label1), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(label1), 0, 0.5);

	datefmt_entry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(datefmt_entry), 256);
	gtk_widget_show(datefmt_entry);
	gtk_table_attach(GTK_TABLE(table), datefmt_entry, 1, 2, 0, 1,
			 (GTK_EXPAND | GTK_FILL), 0, 0, 0);

	/* we need the "sample" entry box; add it as data so callbacks can
	 * get the entry box */
	g_object_set_data(G_OBJECT(datefmt_win), "datefmt_sample",
			  datefmt_entry);

	label2 = gtk_label_new(_("Example"));
	gtk_widget_show(label2);
	gtk_table_attach(GTK_TABLE(table), label2, 0, 1, 1, 2,
			 GTK_FILL, 0, 0, 0);
	gtk_label_set_justify(GTK_LABEL(label2), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(label2), 0, 0.5);

	label3 = gtk_label_new("");
	gtk_widget_show(label3);
	gtk_table_attach(GTK_TABLE(table), label3, 1, 2, 1, 2,
			 (GTK_EXPAND | GTK_FILL), 0, 0, 0);
	gtk_label_set_justify(GTK_LABEL(label3), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(label3), 0, 0.5);

	gtkut_stock_button_set_create(&confirm_area, &ok_btn, GTK_STOCK_OK,
				&cancel_btn, GTK_STOCK_CANCEL, NULL, NULL);

	gtk_box_pack_start(GTK_BOX(vbox1), confirm_area, FALSE, FALSE, 0);
	gtk_widget_show(confirm_area);
	gtk_widget_grab_default(ok_btn);

	/* set the current format */
	gtk_entry_set_text(GTK_ENTRY(datefmt_entry), prefs_common.date_format);
	date_format_entry_on_change(GTK_EDITABLE(datefmt_entry),
				    GTK_LABEL(label3));

	g_signal_connect(G_OBJECT(ok_btn), "clicked",
			 G_CALLBACK(date_format_ok_btn_clicked),
			 &datefmt_win);
	g_signal_connect(G_OBJECT(cancel_btn), "clicked",
			 G_CALLBACK(date_format_cancel_btn_clicked),
			 &datefmt_win);
	g_signal_connect(G_OBJECT(datefmt_win), "key_press_event",
			 G_CALLBACK(date_format_key_pressed),
			 &datefmt_win);
	g_signal_connect(G_OBJECT(datefmt_win), "delete_event",
			 G_CALLBACK(date_format_on_delete),
			 &datefmt_win);
	g_signal_connect(G_OBJECT(datefmt_entry), "changed",
			 G_CALLBACK(date_format_entry_on_change),
			 label3);

	gtk_window_set_position(GTK_WINDOW(datefmt_win), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(datefmt_win), TRUE);

	gtk_widget_show(datefmt_win);
	manage_window_set_transient(GTK_WINDOW(datefmt_win));

	gtk_widget_grab_focus(ok_btn);

	return datefmt_win;
}

static void prefs_keybind_select(void)
{
	GtkWidget *window;
	GtkWidget *vbox1;
	GtkWidget *hbox1;
	GtkWidget *label;
	GtkWidget *combo;
	GtkWidget *confirm_area;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_container_set_border_width (GTK_CONTAINER (window), 8);
	gtk_window_set_title (GTK_WINDOW (window), _("Key bindings"));
	gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal (GTK_WINDOW (window), TRUE);
	gtk_window_set_resizable(GTK_WINDOW (window), FALSE);
	manage_window_set_transient (GTK_WINDOW (window));

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_container_add (GTK_CONTAINER (window), vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), 2);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);

	label = gtk_label_new
		(_("Select preset:"));
	gtk_box_pack_start (GTK_BOX (hbox1), label, FALSE, FALSE, 0);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);

	combo = gtk_combo_new ();
	gtk_box_pack_start (GTK_BOX (hbox1), combo, TRUE, TRUE, 0);
	gtkut_combo_set_items (GTK_COMBO (combo),
			       _("Default"),
			       "Mew / Wanderlust",
			       "Mutt",
			       _("Old Sylpheed"),
			       NULL);
	gtk_editable_set_editable(GTK_EDITABLE(GTK_COMBO (combo)->entry), FALSE);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);

	label = gtk_label_new
		(_("You can also modify each menu shortcut by pressing\n"
		   "any key(s) when placing the mouse pointer on the item."));
	gtk_box_pack_start (GTK_BOX (hbox1), label, FALSE, FALSE, 0);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);

	gtkut_stock_button_set_create (&confirm_area, &ok_btn, GTK_STOCK_OK,
				       &cancel_btn, GTK_STOCK_CANCEL,
				       NULL, NULL);
	gtk_box_pack_end (GTK_BOX (hbox1), confirm_area, FALSE, FALSE, 0);
	gtk_widget_grab_default (ok_btn);

	MANAGE_WINDOW_SIGNALS_CONNECT(window);
	g_signal_connect (G_OBJECT (window), "delete_event",
			  G_CALLBACK (prefs_keybind_deleted), NULL);
	g_signal_connect (G_OBJECT (window), "key_press_event",
			  G_CALLBACK (prefs_keybind_key_pressed), NULL);
	g_signal_connect (G_OBJECT (ok_btn), "clicked",
			  G_CALLBACK (prefs_keybind_apply_clicked),
			  NULL);
	g_signal_connect (G_OBJECT (cancel_btn), "clicked",
			  G_CALLBACK (prefs_keybind_cancel),
			  NULL);

	gtk_widget_show_all(window);

	keybind.window = window;
	keybind.combo = combo;
}

static gboolean prefs_keybind_key_pressed(GtkWidget *widget, GdkEventKey *event,
					  gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		prefs_keybind_cancel();
	return FALSE;
}

static gint prefs_keybind_deleted(GtkWidget *widget, GdkEventAny *event,
				  gpointer data)
{
	prefs_keybind_cancel();
	return TRUE;
}

static void prefs_keybind_cancel(void)
{
	gtk_widget_destroy(keybind.window);
	keybind.window = NULL;
	keybind.combo = NULL;
}
  
struct KeyBind {
	const gchar *accel_path;
	const gchar *accel_key;
};

static void prefs_keybind_apply(struct KeyBind keybind[], gint num)
{
	gint i;
	guint key;
	GdkModifierType mods;

	for (i = 0; i < num; i++) {
		const gchar *accel_key
			= keybind[i].accel_key ? keybind[i].accel_key : "";
		gtk_accelerator_parse(accel_key, &key, &mods);
		gtk_accel_map_change_entry(keybind[i].accel_path,
					   key, mods, TRUE);
	}
}

static void prefs_keybind_apply_clicked(GtkWidget *widget)
{
	GtkEntry *entry = GTK_ENTRY(GTK_COMBO(keybind.combo)->entry);
	const gchar *text;
	struct KeyBind *menurc;
	gint n_menurc;

	static struct KeyBind default_menurc[] = {
		{"<Main>/File/Empty all Trash folders",		""},
		{"<Main>/File/Save as...",			"<control>S"},
		{"<Main>/File/Print...",			""},
		{"<Main>/File/Exit",				"<control>Q"},

		{"<Main>/Edit/Copy",				"<control>C"},
		{"<Main>/Edit/Select all",			"<control>A"},
		{"<Main>/Edit/Find in current message...",	"<control>F"},
		{"<Main>/Edit/Search folder...",		"<shift><control>F"},

		{"<Main>/View/Show or hide/Message View",	"V"},
		{"<Main>/View/Thread view",			"<control>T"},
		{"<Main>/View/Go to/Prev message",		"P"},
		{"<Main>/View/Go to/Next message",		"N"},
		{"<Main>/View/Go to/Prev unread message",	"<shift>P"},
		{"<Main>/View/Go to/Next unread message",	"<shift>N"},
		{"<Main>/View/Go to/Other folder...",		"G"},
		{"<Main>/View/Open in new window",		"<control><alt>N"},
		{"<Main>/View/Message source",			"<control>U"},
		{"<Main>/View/Show all headers",		"<control>H"},
		{"<Main>/View/Update summary",			"<control><alt>U"},

		{"<Main>/Message/Receive/Get from current account",
								"<control>I"},
		{"<Main>/Message/Receive/Get from all accounts","<shift><control>I"},
		{"<Main>/Message/Compose an email message",	"<control>M"},
		{"<Main>/Message/Reply",			"<control>R"},
		{"<Main>/Message/Reply to/all",			"<shift><control>R"},
		{"<Main>/Message/Reply to/sender",		""},
		{"<Main>/Message/Reply to/mailing list",	"<control>L"},
		{"<Main>/Message/Forward",			"<control><alt>F"},
		/* {"<Main>/Message/Forward as attachment",	 ""}, */
		{"<Main>/Message/Move...",			"<control>O"},
		{"<Main>/Message/Copy...",			"<shift><control>O"},
		{"<Main>/Message/Delete",			"<control>D"},
		{"<Main>/Message/Mark/Mark",			"<shift>asterisk"},
		{"<Main>/Message/Mark/Unmark",			"U"},
		{"<Main>/Message/Mark/Mark as unread",		"<shift>exclam"},
		{"<Main>/Message/Mark/Mark as read",		""},

		{"<Main>/Tools/Address book",			"<shift><control>A"},
		{"<Main>/Tools/Execute",			"X"},
		{"<Main>/Tools/Log window",			"<shift><control>L"},

		{"<Compose>/Message/Close",				"<control>W"},
		{"<Compose>/Edit/Select all",				"<control>A"},
		{"<Compose>/Edit/Advanced/Move a word backward",	""},
		{"<Compose>/Edit/Advanced/Move a word forward",		""},
		{"<Compose>/Edit/Advanced/Move to beginning of line",	""},
		{"<Compose>/Edit/Advanced/Delete a word backward",	""},
		{"<Compose>/Edit/Advanced/Delete a word forward",	""},
	};

	static struct KeyBind mew_wl_menurc[] = {
		{"<Main>/File/Empty all Trash folders",			"<shift>D"},
		{"<Main>/File/Save as...",			"Y"},
		{"<Main>/File/Print...",			"<shift>numbersign"},
		{"<Main>/File/Exit",				"<shift>Q"},

		{"<Main>/Edit/Copy",				"<control>C"},
		{"<Main>/Edit/Select all",			"<control>A"},
		{"<Main>/Edit/Find in current message...",	"<control>F"},
		{"<Main>/Edit/Search folder...",		"<control>S"},

		{"<Main>/View/Show or hide/Message View",	""},
		{"<Main>/View/Thread view",			"<shift>T"},
		{"<Main>/View/Go to/Prev message",		"P"},
		{"<Main>/View/Go to/Next message",		"N"},
		{"<Main>/View/Go to/Prev unread message",	"<shift>P"},
		{"<Main>/View/Go to/Next unread message",	"<shift>N"},
		{"<Main>/View/Go to/Other folder...",		"G"},
		{"<Main>/View/Open in new window",		"<control><alt>N"},
		{"<Main>/View/Message source",			"<control>U"},
		{"<Main>/View/Show all headers",		"<shift>H"},
		{"<Main>/View/Update summary",			"<shift>S"},

		{"<Main>/Message/Receive/Get from current account",
								"<control>I"},
		{"<Main>/Message/Receive/Get from all accounts","<shift><control>I"},
		{"<Main>/Message/Compose an email message",	"W"},
		{"<Main>/Message/Reply",			"<control>R"},
		{"<Main>/Message/Reply to/all",			"<shift>A"},
		{"<Main>/Message/Reply to/sender",		""},
		{"<Main>/Message/Reply to/mailing list",	"<control>L"},
		{"<Main>/Message/Forward",			"F"},
		/* {"<Main>/Message/Forward as attachment", "<shift>F"}, */
		{"<Main>/Message/Move...",			"O"},
		{"<Main>/Message/Copy...",			"<shift>O"},
		{"<Main>/Message/Delete",			"D"},
		{"<Main>/Message/Mark/Mark",			"<shift>asterisk"},
		{"<Main>/Message/Mark/Unmark",			"U"},
		{"<Main>/Message/Mark/Mark as unread",		"<shift>exclam"},
		{"<Main>/Message/Mark/Mark as read",		"<shift>R"},

		{"<Main>/Tools/Address book",			"<shift><control>A"},
		{"<Main>/Tools/Execute",			"X"},
		{"<Main>/Tools/Log window",			"<shift><control>L"},

		{"<Compose>/Message/Close",				"<alt>W"},
		{"<Compose>/Edit/Select all",				""},
		{"<Compose>/Edit/Advanced/Move a word backward,"	"<alt>B"},
		{"<Compose>/Edit/Advanced/Move a word forward",		"<alt>F"},
		{"<Compose>/Edit/Advanced/Move to beginning of line",	"<control>A"},
		{"<Compose>/Edit/Advanced/Delete a word backward",	"<control>W"},
		{"<Compose>/Edit/Advanced/Delete a word forward",	"<alt>D"},
	};

	static struct KeyBind mutt_menurc[] = {
		{"<Main>/File/Empty all Trash folders",		""},
		{"<Main>/File/Save as...",			"S"},
		{"<Main>/File/Print...",			"P"},
		{"<Main>/File/Exit",				"Q"},

		{"<Main>/Edit/Copy",				"<control>C"},
		{"<Main>/Edit/Select all",			"<control>A"},
		{"<Main>/Edit/Find in current message...",	"<control>F"},
		{"<Main>/Edit/Search messages...",		"slash"},

		{"<Main>/View/Show or hide/Message view",	"V"},
		{"<Main>/View/Thread view",			"<control>T"},
		{"<Main>/View/Go to/Prev message",		""},
		{"<Main>/View/Go to/Next message",		""},
		{"<Main>/View/Go to/Prev unread message",	""},
		{"<Main>/View/Go to/Next unread message",	""},
		{"<Main>/View/Go to/Other folder...",		"C"},
		{"<Main>/View/Open in new window",		"<control><alt>N"},
		{"<Main>/View/Message source",			"<control>U"},
		{"<Main>/View/Show all headers",		"<control>H"},
		{"<Main>/View/Update summary",				"<control><alt>U"},

		{"<Main>/Message/Receive/Get from current account",
								"<control>I"},
		{"<Main>/Message/Receive/Get from all accounts","<shift><control>I"},
		{"<Main>/Message/Compose an email message",		"M"},
		{"<Main>/Message/Reply",			"R"},
		{"<Main>/Message/Reply to/all",			"G"},
		{"<Main>/Message/Reply to/sender",		""},
		{"<Main>/Message/Reply to/mailing list",	"<control>L"},
		{"<Main>/Message/Forward",			"F"},
		{"<Main>/Message/Forward as attachment",	""},
		{"<Main>/Message/Move...",			"<control>O"},
		{"<Main>/Message/Copy...",			"<shift>C"},
		{"<Main>/Message/Delete",			"D"},
		{"<Main>/Message/Mark/Mark",			"<shift>F"},
		{"<Main>/Message/Mark/Unmark",			"U"},
		{"<Main>/Message/Mark/Mark as unread",		"<shift>N"},
		{"<Main>/Message/Mark/Mark as read",		""},

		{"<Main>/Tools/Address book",			"<shift><control>A"},
		{"<Main>/Tools/Execute",			"X"},
		{"<Main>/Tools/Log window",			"<shift><control>L"},

		{"<Compose>/Message/Close",				"<alt>W"},
		{"<Compose>/Edit/Select all",				""},
		{"<Compose>/Edit/Advanced/Move a word backward",	"<alt>B"},
		{"<Compose>/Edit/Advanced/Move a word forward",		"<alt>F"},
		{"<Compose>/Edit/Advanced/Move to beginning of line",	"<control>A"},
		{"<Compose>/Edit/Advanced/Delete a word backward",	"<control>W"},
		{"<Compose>/Edit/Advanced/Delete a word forward",	"<alt>D"},
	};

	static struct KeyBind old_sylpheed_menurc[] = {
		{"<Main>/File/Empty all Trash folders",		""},
		{"<Main>/File/Save as...",			""},
		{"<Main>/File/Print...",			"<alt>P"},
		{"<Main>/File/Exit",				"<alt>Q"},

		{"<Main>/Edit/Copy",				"<control>C"},
		{"<Main>/Edit/Select all",			"<control>A"},
		{"<Main>/Edit/Find in current message...",	"<control>F"},
		{"<Main>/Edit/Search folder...",		"<control>S"},

		{"<Main>/View/Show or hide/Message View",	""},
		{"<Main>/View/Thread view",			"<control>T"},
		{"<Main>/View/Go to/Prev message",		"P"},
		{"<Main>/View/Go to/Next message",		"N"},
		{"<Main>/View/Go to/Prev unread message",	"<shift>P"},
		{"<Main>/View/Go to/Next unread message",	"<shift>N"},
		{"<Main>/View/Go to/Other folder...",		"<alt>G"},
		{"<Main>/View/Open in new window",		"<shift><control>N"},
		{"<Main>/View/Message source",			"<control>U"},
		{"<Main>/View/Show all headers",		"<control>H"},
		{"<Main>/View/Update summary",			"<alt>U"},

		{"<Main>/Message/Receive/Get from current account",
								"<alt>I"},
		{"<Main>/Message/Receive/Get from all accounts","<shift><alt>I"},
		{"<Main>/Message/Compose an email message",	"<alt>N"},
		{"<Main>/Message/Reply",			"<alt>R"},
		{"<Main>/Message/Reply to/all",			"<shift><alt>R"},
		{"<Main>/Message/Reply to/sender",		"<control><alt>R"},
		{"<Main>/Message/Reply to/mailing list",	"<control>L"},
		{"<Main>/Message/Forward",			 "<shift><alt>F"},
		/* "(menu-path \"<Main>/Message/Forward as attachment", "<shift><control>F"}, */
		{"<Main>/Message/Move...",			"<alt>O"},
		{"<Main>/Message/Copy...",			""},
		{"<Main>/Message/Delete",			"<alt>D"},
		{"<Main>/Message/Mark/Mark",			"<shift>asterisk"},
		{"<Main>/Message/Mark/Unmark",			"U"},
		{"<Main>/Message/Mark/Mark as unread",		"<shift>exclam"},
		{"<Main>/Message/Mark/Mark as read",		""},

		{"<Main>/Tools/Address book",			"<alt>A"},
		{"<Main>/Tools/Execute",			"<alt>X"},
		{"<Main>/Tools/Log window",			"<alt>L"},

		{"<Compose>/Message/Close",				"<alt>W"},
		{"<Compose>/Edit/Select all",				""},
		{"<Compose>/Edit/Advanced/Move a word backward",	"<alt>B"},
		{"<Compose>/Edit/Advanced/Move a word forward",		"<alt>F"},
		{"<Compose>/Edit/Advanced/Move to beginning of line",	"<control>A"},
		{"<Compose>/Edit/Advanced/Delete a word backward",	"<control>W"},
		{"<Compose>/Edit/Advanced/Delete a word forward",	"<alt>D"},
	};
  
	text = gtk_entry_get_text(entry);
  
	if (!strcmp(text, _("Default"))) {
		menurc = default_menurc;
		n_menurc = G_N_ELEMENTS(default_menurc);
	} else if (!strcmp(text, "Mew / Wanderlust")) {
		menurc = mew_wl_menurc;
		n_menurc = G_N_ELEMENTS(mew_wl_menurc);
	} else if (!strcmp(text, "Mutt")) {
		menurc = mutt_menurc;
		n_menurc = G_N_ELEMENTS(mutt_menurc);
	} else if (!strcmp(text, _("Old Sylpheed"))) {
	        menurc = old_sylpheed_menurc;
		n_menurc = G_N_ELEMENTS(old_sylpheed_menurc);
	} else {
		return;
	}

	/* prefs_keybind_apply(empty_menurc, G_N_ELEMENTS(empty_menurc)); */
	prefs_keybind_apply(menurc, n_menurc);

	gtk_widget_destroy(keybind.window);
	keybind.window = NULL;
	keybind.combo = NULL;
}

static void prefs_common_charset_set_data_from_optmenu(PrefParam *pparam)
{
	GtkWidget *menu;
	GtkWidget *menuitem;
	gchar *charset;

	menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(*pparam->widget));
	menuitem = gtk_menu_get_active(GTK_MENU(menu));
	charset = g_object_get_data(G_OBJECT(menuitem), MENU_VAL_ID);
	g_free(*((gchar **)pparam->data));
	*((gchar **)pparam->data) = g_strdup(charset);
}

static void prefs_common_charset_set_optmenu(PrefParam *pparam)
{
	GtkOptionMenu *optmenu = GTK_OPTION_MENU(*pparam->widget);
	gint index;

	g_return_if_fail(optmenu != NULL);
	g_return_if_fail(*((gchar **)pparam->data) != NULL);

	index = menu_find_option_menu_index(optmenu, *((gchar **)pparam->data),
					    (GCompareFunc)strcmp2);
	if (index >= 0)
		gtk_option_menu_set_history(optmenu, index);
	else {
		gtk_option_menu_set_history(optmenu, 0);
		prefs_common_charset_set_data_from_optmenu(pparam);
	}
}

static void prefs_common_encoding_set_data_from_optmenu(PrefParam *pparam)
{
	GtkWidget *menu;
	GtkWidget *menuitem;

	menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(*pparam->widget));
	menuitem = gtk_menu_get_active(GTK_MENU(menu));
	*((TransferEncodingMethod *)pparam->data) = GPOINTER_TO_INT
		(g_object_get_data(G_OBJECT(menuitem), MENU_VAL_ID));
}

static void prefs_common_encoding_set_optmenu(PrefParam *pparam)
{
	TransferEncodingMethod method =
		*((TransferEncodingMethod *)pparam->data);
	GtkOptionMenu *optmenu = GTK_OPTION_MENU(*pparam->widget);
	gint index;

	g_return_if_fail(optmenu != NULL);

	index = menu_find_option_menu_index(optmenu, GINT_TO_POINTER(method),
					    NULL);
	if (index >= 0)
		gtk_option_menu_set_history(optmenu, index);
	else {
		gtk_option_menu_set_history(optmenu, 0);
		prefs_common_encoding_set_data_from_optmenu(pparam);
	}
}

static void prefs_common_recv_dialog_set_data_from_optmenu(PrefParam *pparam)
{
	GtkWidget *menu;
	GtkWidget *menuitem;

	menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(*pparam->widget));
	menuitem = gtk_menu_get_active(GTK_MENU(menu));
	*((RecvDialogMode *)pparam->data) = GPOINTER_TO_INT
		(g_object_get_data(G_OBJECT(menuitem), MENU_VAL_ID));
}

static void prefs_common_recv_dialog_set_optmenu(PrefParam *pparam)
{
	RecvDialogMode mode = *((RecvDialogMode *)pparam->data);
	GtkOptionMenu *optmenu = GTK_OPTION_MENU(*pparam->widget);
	GtkWidget *menu;
	GtkWidget *menuitem;
	gint index;

	index = menu_find_option_menu_index(optmenu, GINT_TO_POINTER(mode),
					    NULL);
	if (index >= 0)
		gtk_option_menu_set_history(optmenu, index);
	else {
		gtk_option_menu_set_history(optmenu, 0);
		prefs_common_recv_dialog_set_data_from_optmenu(pparam);
	}

	menu = gtk_option_menu_get_menu(optmenu);
	menuitem = gtk_menu_get_active(GTK_MENU(menu));
	gtk_menu_item_activate(GTK_MENU_ITEM(menuitem));
}

static void prefs_common_send_dialog_set_data_from_optmenu(PrefParam *pparam)
{
	GtkWidget *menu;
	GtkWidget *menuitem;

	menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(*pparam->widget));
	menuitem = gtk_menu_get_active(GTK_MENU(menu));
	*((SendDialogMode *)pparam->data) = GPOINTER_TO_INT
		(g_object_get_data(G_OBJECT(menuitem), MENU_VAL_ID));
}

static void prefs_common_send_dialog_set_optmenu(PrefParam *pparam)
{
	SendDialogMode mode = *((SendDialogMode *)pparam->data);
	GtkOptionMenu *optmenu = GTK_OPTION_MENU(*pparam->widget);
	GtkWidget *menu;
	GtkWidget *menuitem;

	switch (mode) {
	case SEND_DIALOG_ALWAYS:
		gtk_option_menu_set_history(optmenu, 0);
		break;
	case SEND_DIALOG_NEVER:
		gtk_option_menu_set_history(optmenu, 1);
		break;
	default:
		break;
	}

	menu = gtk_option_menu_get_menu(optmenu);
	menuitem = gtk_menu_get_active(GTK_MENU(menu));
	gtk_menu_item_activate(GTK_MENU_ITEM(menuitem));
}

static void prefs_common_apply(void)
{
	MainWindow *mainwindow;

	prefs_set_data_from_dialog(param);
	sock_set_io_timeout(prefs_common.io_timeout_secs);
	main_window_reflect_prefs_all_real(FALSE);
	prefs_common_write_config();

	mainwindow = mainwindow_get_mainwindow();
	log_window_set_clipping(mainwindow->logwin, prefs_common.cliplog,
				prefs_common.loglength);
	
	inc_autocheck_timer_remove();
	inc_autocheck_timer_set();
}

static void prefs_nextunreadmsgdialog_set_data_from_optmenu(PrefParam *pparam)
{
	GtkWidget *menu;
	GtkWidget *menuitem;

	menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(*pparam->widget));
	menuitem = gtk_menu_get_active(GTK_MENU(menu));
	*((NextUnreadMsgDialogShow *)pparam->data) = GPOINTER_TO_INT
		(g_object_get_data(G_OBJECT(menuitem), MENU_VAL_ID));
}

static void prefs_nextunreadmsgdialog_set_optmenu(PrefParam *pparam)
{
	NextUnreadMsgDialogShow dialog_show;
	GtkOptionMenu *optmenu = GTK_OPTION_MENU(*pparam->widget);
	GtkWidget *menu;
	GtkWidget *menuitem;

	dialog_show = *((NextUnreadMsgDialogShow *)pparam->data);

	switch (dialog_show) {
	case NEXTUNREADMSGDIALOG_ALWAYS:
		gtk_option_menu_set_history(optmenu, 0);
		break;
	case NEXTUNREADMSGDIALOG_ASSUME_YES:
		gtk_option_menu_set_history(optmenu, 1);
		break;
	case NEXTUNREADMSGDIALOG_ASSUME_NO:
		gtk_option_menu_set_history(optmenu, 2);
		break;
	}

	menu = gtk_option_menu_get_menu(optmenu);
	menuitem = gtk_menu_get_active(GTK_MENU(menu));
	gtk_menu_item_activate(GTK_MENU_ITEM(menuitem));
}

void pref_set_textview_from_pref(GtkTextView *textview, gchar *txt)
{
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(textview);
	gchar *o_out, *out = malloc(txt?(strlen(txt)+1):1);
	gchar *t = txt;
	memset(out, 0, strlen(txt)+1);
	o_out = out;
	while (*t != '\0') {
		if (*t == '\\' && *(t+1) == 'n') {
			*out++ = '\n';
			t++;
		} else if (*t == '\\') {
			t++;
		} else {
			*out++ = *t;
		}
		t++;
	}
	*out='\0';

	gtk_text_buffer_set_text(buffer, o_out?o_out:"", -1);
	g_free(o_out);
}

gchar *pref_get_pref_from_textview(GtkTextView *textview) 
{
	GtkTextBuffer *buffer;
	GtkTextIter start, end;
	gchar *o_out, *out, *tmp, *t;
	
	buffer = gtk_text_view_get_buffer(textview);
	gtk_text_buffer_get_start_iter(buffer, &start);
	gtk_text_buffer_get_iter_at_offset(buffer, &end, -1);
	tmp = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
	t = tmp;
	o_out = out = malloc(2*strlen(tmp)+1);
	
	while (*t != '\0') {
		if (*t == '\n') {
			*out++ = '\\';
			*out++ = 'n';
		} else if (*t == '\\') {
			*out++ = '\\';
			*out++ = '\\';
		} else {
			*out++ = *t;
		}
		t++;
	}
	*out = '\0';
	g_free(tmp);

	return o_out;
}
