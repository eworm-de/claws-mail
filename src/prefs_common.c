/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2006 Hiroyuki Yamamoto and the Sylpheed-Claws team
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
#include "prefs_folder_column.h"
#include "mainwindow.h"
#include "summaryview.h"
#include "folderview.h"
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
#include "colorlabel.h"

enum {
	DATEFMT_FMT,
	DATEFMT_TXT,
	N_DATEFMT_COLUMNS
};

PrefsCommon prefs_common;

GtkWidget *notebook;

#ifdef G_OS_WIN32
/*
 * In the Windows version prefs_common contains
 *   - the non-OS-specific settings of the "Common" section and
 *   - the OS-specific settings of the "CommonWin32" section
 * The OS-specific settings of the "Common" section are not used
 * but saved in prefs_unix.
 */

#  define SPECIFIC_PREFS prefs_unix

static PrefsCommon prefs_unix;

static PrefParam param_os_specific[] = {
	/* Receive */
	{"ext_inc_path", "",
	 &prefs_common.extinc_cmd, P_STRING, NULL, NULL, NULL},
	{"newmail_notify_cmd", "",
	 &prefs_common.newmail_notify_cmd, P_STRING, NULL, NULL, NULL},

	/* new fonts */
	{"widget_font_gtk2",	NULL,
	  &prefs_common.widgetfont,		P_STRING, NULL, NULL, NULL},
	{"message_font_gtk2",	"Monospace 9",
	 &prefs_common.textfont,		P_STRING, NULL, NULL, NULL},
        {"print_font_gtk2",     "Monospace 9",
         &prefs_common.printfont,		P_STRING, NULL, NULL, NULL},
	{"small_font_gtk2",	"Sans 9",
	  &prefs_common.smallfont,		P_STRING, NULL, NULL, NULL},
	{"normal_font_gtk2",	"Sans 9",
	  &prefs_common.normalfont,		P_STRING, NULL, NULL, NULL},

	/* Message */
	{"attach_save_directory", NULL,
	 &prefs_common.attach_save_dir, P_STRING, NULL, NULL, NULL},
	{"attach_load_directory", NULL,
	 &prefs_common.attach_load_dir, P_STRING, NULL, NULL, NULL},

	/* MIME viewer */
	{"mime_textviewer", NULL,
	 &prefs_common.mime_textviewer,   P_STRING, NULL, NULL, NULL},
	{"mime_open_command", "notepad '%s'",
	 &prefs_common.mime_open_cmd,     P_STRING, NULL, NULL, NULL},

	/* Interface */
	{"pixmap_theme_path", DEFAULT_PIXMAP_THEME, 
	 &prefs_common.pixmap_theme_path, P_STRING, NULL, NULL, NULL},

	/* Other */
	{"uri_open_command", NULL,
	 &prefs_common.uri_cmd, P_STRING, NULL, NULL, NULL},
	{"print_command", "notepad /p %s",
	 &prefs_common.print_cmd, P_STRING, NULL, NULL, NULL},
	{"ext_editor_command", "notepad %s",
	 &prefs_common.ext_editor_cmd, P_STRING, NULL, NULL, NULL},

	{NULL, NULL, NULL, P_OTHER, NULL, NULL, NULL}
};
#else
#  define SPECIFIC_PREFS prefs_common
#endif

/*
   parameter name, default value, pointer to the prefs variable, data type,
   pointer to the widget pointer,
   pointer to the function for data setting,
   pointer to the function for widget setting
 */

static PrefParam param[] = {
	/* Receive */
	{"use_ext_inc", "FALSE", &prefs_common.use_extinc, P_BOOL,
	 NULL, NULL, NULL},
	{"ext_inc_path", DEFAULT_INC_PATH, &SPECIFIC_PREFS.extinc_cmd, P_STRING,
	 NULL, NULL, NULL},

	{"autochk_newmail", "FALSE", &prefs_common.autochk_newmail, P_BOOL,
	 NULL, NULL, NULL},
	{"autochk_interval", "10", &prefs_common.autochk_itv, P_INT,
	 NULL, NULL, NULL},
	{"check_on_startup", "FALSE", &prefs_common.chk_on_startup, P_BOOL,
	 NULL, NULL, NULL},
	{"open_inbox_on_inc", "FALSE", &prefs_common.open_inbox_on_inc,
	 P_BOOL, NULL, NULL, NULL},
	{"scan_all_after_inc", "FALSE", &prefs_common.scan_all_after_inc,
	 P_BOOL, NULL, NULL, NULL},
	{"newmail_notify_manu", "FALSE", &prefs_common.newmail_notify_manu,
	 P_BOOL, NULL, NULL, NULL},
 	{"newmail_notify_auto", "FALSE", &prefs_common.newmail_notify_auto,
	P_BOOL, NULL, NULL, NULL},
 	{"newmail_notify_cmd", "", &SPECIFIC_PREFS.newmail_notify_cmd, P_STRING,
 	 NULL, NULL, NULL},
	{"receive_dialog_mode", "1", &prefs_common.recv_dialog_mode, P_ENUM,
	 NULL, NULL, NULL},
	{"receivewin_width", "460", &prefs_common.receivewin_width, P_INT,
	 NULL, NULL, NULL},
	{"receivewin_height", "-1", &prefs_common.receivewin_height, P_INT,
	 NULL, NULL, NULL},
	{"no_receive_error_panel", "FALSE", &prefs_common.no_recv_err_panel,
	 P_BOOL, NULL, NULL, NULL},
	{"close_receive_dialog", "TRUE", &prefs_common.close_recv_dialog,
	 P_BOOL, NULL, NULL, NULL},
 
	/* Send */
	{"save_message", "TRUE", &prefs_common.savemsg, P_BOOL,
	 NULL, NULL, NULL},
	{"confirm_send_queued_messages", "FALSE", &prefs_common.confirm_send_queued_messages,
	 P_BOOL, NULL, NULL, NULL},
	{"send_dialog_mode", "0", &prefs_common.send_dialog_mode, P_ENUM,
	 NULL, NULL, NULL},
	{"sendwin_width", "460", &prefs_common.sendwin_width, P_INT,
	 NULL, NULL, NULL},
	{"sendwin_height", "-1", &prefs_common.sendwin_height, P_INT,
	 NULL, NULL, NULL},

	{"outgoing_charset", CS_AUTO, &prefs_common.outgoing_charset, P_STRING,
	 NULL, NULL, NULL},
	{"encoding_method", "0", &prefs_common.encoding_method, P_ENUM,
	 NULL, NULL, NULL},

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
	{"linewrap_pastes", "TRUE", &prefs_common.linewrap_pastes, P_BOOL,
	 NULL, NULL, NULL},
	{"linewrap_auto", "TRUE", &prefs_common.autowrap, P_BOOL,
	 NULL, NULL, NULL},
        {"autosave", "TRUE", &prefs_common.autosave,
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
	{"recheck_when_changing_dict", "TRUE", &prefs_common.recheck_when_changing_dict,
	 P_BOOL, NULL, NULL, NULL},
	{"misspelled_color", "16711680", &prefs_common.misspelled_col, P_COLOR,
	 NULL, NULL, NULL},
#endif
	{"reply_with_quote", "TRUE", &prefs_common.reply_with_quote, P_BOOL,
	 NULL, NULL, NULL},
	{"compose_dnd_insert_or_attach", "0", &prefs_common.compose_dnd_mode, P_ENUM,
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
	{"reply_quote_format", N_("On %d\\n%f wrote:\\n\\n%q"),
	 &prefs_common.quotefmt, P_STRING, NULL, NULL, NULL},

	{"forward_quote_mark", "> ", &prefs_common.fw_quotemark, P_STRING,
	 NULL, NULL, NULL},
	{"forward_quote_format",
	 N_("\\n\\nBegin forwarded message:\\n\\n"
	 "?d{Date: %d\\n}?f{From: %f\\n}?t{To: %t\\n}?c{Cc: %c\\n}"
	 "?n{Newsgroups: %n\\n}?s{Subject: %s\\n}\\n\\n%M"),
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
	  &SPECIFIC_PREFS.widgetfont,		P_STRING, NULL, NULL, NULL},
	{"message_font_gtk2",	"Monospace 9",
	 &SPECIFIC_PREFS.textfont,			P_STRING, NULL, NULL, NULL},
        {"print_font_gtk2",     "Monospace 9",
         &SPECIFIC_PREFS.printfont,             P_STRING, NULL, NULL, NULL},
	{"small_font_gtk2",	"Sans 9",
	  &SPECIFIC_PREFS.smallfont,		P_STRING, NULL, NULL, NULL},
	{"normal_font_gtk2",	"Sans 9",
	  &SPECIFIC_PREFS.normalfont,		P_STRING, NULL, NULL, NULL},

	/* custom colors */
	{"custom_color1", "#ff9900", &prefs_common.custom_colorlabel[0].color, P_COLOR,
	 NULL, NULL, NULL},
	{"custom_colorlabel1", N_("Orange"), &prefs_common.custom_colorlabel[0].label, P_STRING,
	 NULL, NULL, NULL},
	{"custom_color2", "#ff0000", &prefs_common.custom_colorlabel[1].color, P_COLOR,
	 NULL, NULL, NULL},
	{"custom_colorlabel2", N_("Red"), &prefs_common.custom_colorlabel[1].label, P_STRING,
	 NULL, NULL, NULL},
	{"custom_color3", "#ff66ff", &prefs_common.custom_colorlabel[2].color, P_COLOR,
	 NULL, NULL, NULL},
	{"custom_colorlabel3", N_("Pink"), &prefs_common.custom_colorlabel[2].label, P_STRING,
	 NULL, NULL, NULL},
	{"custom_color4", "#00ccff", &prefs_common.custom_colorlabel[3].color, P_COLOR,
	 NULL, NULL, NULL},
	{"custom_colorlabel4", N_("Sky blue"), &prefs_common.custom_colorlabel[3].label, P_STRING,
	 NULL, NULL, NULL},
	{"custom_color5", "#0000ff", &prefs_common.custom_colorlabel[4].color, P_COLOR,
	 NULL, NULL, NULL},
	{"custom_colorlabel5", N_("Blue"), &prefs_common.custom_colorlabel[4].label, P_STRING,
	 NULL, NULL, NULL},
	{"custom_color6", "#009900", &prefs_common.custom_colorlabel[5].color, P_COLOR,
	 NULL, NULL, NULL},
	{"custom_colorlabel6", N_("Green"), &prefs_common.custom_colorlabel[5].label, P_STRING,
	 NULL, NULL, NULL},
	{"custom_color7", "#663366", &prefs_common.custom_colorlabel[6].color, P_COLOR,
	 NULL, NULL, NULL},
	{"custom_colorlabel7", N_("Brown"), &prefs_common.custom_colorlabel[6].label, P_STRING,
	 NULL, NULL, NULL},

	/* image viewer */
	{"display_image", "TRUE", &prefs_common.display_img, P_BOOL,
	 NULL, NULL, NULL},
	{"resize_image", "TRUE", &prefs_common.resize_img, P_BOOL,
	 NULL, NULL, NULL},
	{"inline_image", "TRUE", &prefs_common.inline_img, P_BOOL,
	 NULL, NULL, NULL},

	{"display_folder_unread_num", "FALSE",
	 &prefs_common.display_folder_unread, P_BOOL,
	 NULL, NULL, NULL},
	{"newsgroup_abbrev_len", "16",
	 &prefs_common.ng_abbrev_len, P_INT,
	 NULL, NULL, NULL},

	{"translate_header", "FALSE", &prefs_common.trans_hdr, P_BOOL,
	 NULL, NULL, NULL},

	/* Display: Summary View */
	{"use_address_book", "FALSE", &prefs_common.use_addr_book, P_BOOL,
	 NULL, NULL, NULL},
	{"thread_by_subject", "TRUE", &prefs_common.thread_by_subject, P_BOOL,
	 NULL, NULL, NULL},
	{"date_format", N_("%y/%m/%d(%a) %H:%M"), &prefs_common.date_format,
	 P_STRING, NULL, NULL, NULL},

	{"bold_unread", "TRUE", &prefs_common.bold_unread, P_BOOL,
	 NULL, NULL, NULL},

	{"enable_thread", "TRUE", &prefs_common.enable_thread, P_BOOL,
	 NULL, NULL, NULL},
	{"toolbar_style", "3", &prefs_common.toolbar_style, P_ENUM,
	 NULL, NULL, NULL},
	{"toolbar_detachable", "FALSE", &prefs_common.toolbar_detachable, P_BOOL,
	 NULL, NULL, NULL},
	{"show_statusbar", "TRUE", &prefs_common.show_statusbar, P_BOOL,
	 NULL, NULL, NULL},
	{"show_searchbar", "TRUE", &prefs_common.show_searchbar, P_BOOL,
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
	{"summary_col_show_to", "FALSE",
	 &prefs_common.summary_col_visible[S_COL_TO], P_BOOL, NULL, NULL, NULL},
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
	{"summary_col_pos_to", "10",
	  &prefs_common.summary_col_pos[S_COL_TO], P_INT, NULL, NULL, NULL},

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
	{"summary_col_size_to", "120",
	 &prefs_common.summary_col_size[S_COL_TO], P_INT, NULL, NULL, NULL},
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
	{"folderview_width", "270", &prefs_common.folderview_width, P_INT,
	 NULL, NULL, NULL},
	{"folderview_height", "450", &prefs_common.folderview_height, P_INT,
	 NULL, NULL, NULL},
	{"folderview_visible", "TRUE", &prefs_common.folderview_visible, P_BOOL,
	 NULL, NULL, NULL},

	{"folder_col_show_folder", "TRUE",
	 &prefs_common.folder_col_visible[F_COL_FOLDER], P_BOOL, NULL, NULL, NULL},
	{"folder_col_show_new", "TRUE",
	 &prefs_common.folder_col_visible[F_COL_NEW], P_BOOL, NULL, NULL, NULL},
	{"folder_col_show_unread", "TRUE",
	 &prefs_common.folder_col_visible[F_COL_UNREAD], P_BOOL, NULL, NULL, NULL},
	{"folder_col_show_total", "TRUE",
	 &prefs_common.folder_col_visible[F_COL_TOTAL], P_BOOL, NULL, NULL, NULL},

	{"folder_col_pos_folder", "0",
	 &prefs_common.folder_col_pos[F_COL_FOLDER], P_INT, NULL, NULL, NULL},
	{"folder_col_pos_new", "1",
	 &prefs_common.folder_col_pos[F_COL_NEW], P_INT, NULL, NULL, NULL},
	{"folder_col_pos_unread", "2",
	 &prefs_common.folder_col_pos[F_COL_UNREAD], P_INT, NULL, NULL, NULL},
	{"folder_col_pos_total", "3",
	 &prefs_common.folder_col_pos[F_COL_TOTAL], P_INT, NULL, NULL, NULL},

	{"folder_col_size_folder", "120",
	 &prefs_common.folder_col_size[F_COL_FOLDER], P_INT, NULL, NULL, NULL},
	{"folder_col_size_new", "32",
	 &prefs_common.folder_col_size[F_COL_NEW], P_INT, NULL, NULL, NULL},
	{"folder_col_size_unread", "32",
	 &prefs_common.folder_col_size[F_COL_UNREAD], P_INT, NULL, NULL, NULL},
	{"folder_col_size_total", "32",
	 &prefs_common.folder_col_size[F_COL_TOTAL], P_INT, NULL, NULL, NULL},

	{"summaryview_width", "500", &prefs_common.summaryview_width, P_INT,
	 NULL, NULL, NULL},
	{"summaryview_height", "244", &prefs_common.summaryview_height, P_INT,
	 NULL, NULL, NULL},

	{"main_messagewin_x", "256", &prefs_common.main_msgwin_x, P_INT,
	 NULL, NULL, NULL},
	{"main_messagewin_y", "210", &prefs_common.main_msgwin_y, P_INT,
	 NULL, NULL, NULL},
	{"messageview_width", "500", &prefs_common.msgview_width, P_INT,
	 NULL, NULL, NULL},
	{"messageview_height", "213", &prefs_common.msgview_height, P_INT,
	 NULL, NULL, NULL},
	{"messageview_visible", "TRUE", &prefs_common.msgview_visible, P_BOOL,
	 NULL, NULL, NULL},

	{"mainview_x", "64", &prefs_common.mainview_x, P_INT,
	 NULL, NULL, NULL},
	{"mainview_y", "64", &prefs_common.mainview_y, P_INT,
	 NULL, NULL, NULL},
	{"mainview_width", "500", &prefs_common.mainview_width, P_INT,
	 NULL, NULL, NULL},
	{"mainview_height", "400", &prefs_common.mainview_height, P_INT,
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
	{"enable_bgcolor", "FALSE", &prefs_common.enable_bgcolor, P_BOOL,
	 NULL, NULL, NULL},
	{"quote_level1_bgcolor", "13421772", &prefs_common.quote_level1_bgcol, P_COLOR,
	 NULL, NULL, NULL},
	{"quote_level2_bgcolor", "13948116", &prefs_common.quote_level2_bgcol, P_COLOR,
	 NULL, NULL, NULL},
	{"quote_level3_bgcolor", "14540253", &prefs_common.quote_level3_bgcol, P_COLOR,
	 NULL, NULL, NULL},
	{"uri_color", "32512", &prefs_common.uri_col, P_COLOR,
	 NULL, NULL, NULL},
	{"target_folder_color", "14294218", &prefs_common.tgt_folder_col, P_COLOR,
	 NULL, NULL, NULL},
	{"signature_color", "7960953", &prefs_common.signature_col, P_COLOR,
	 NULL, NULL, NULL},
	{"recycle_quote_colors", "FALSE", &prefs_common.recycle_quote_colors,
	 P_BOOL, NULL, NULL, NULL},

	{"display_header_pane", "FALSE", &prefs_common.display_header_pane,
	 P_BOOL, NULL, NULL, NULL},
	{"display_header", "TRUE", &prefs_common.display_header, P_BOOL,
	 NULL, NULL, NULL},
	{"display_xface", "TRUE", &prefs_common.display_xface,
	 P_BOOL, NULL, NULL, NULL},
	{"render_html", "TRUE", &prefs_common.render_html, P_BOOL,
	 NULL, NULL, NULL},
	{"invoke_plugin_on_html", "FALSE", &prefs_common.invoke_plugin_on_html, P_BOOL,
	 NULL, NULL, NULL},
	{"line_space", "2", &prefs_common.line_space, P_INT,
	 NULL, NULL, NULL},
	{"never_send_retrcpt", "FALSE", &prefs_common.never_send_retrcpt, P_BOOL,
	 NULL, NULL, NULL},

	{"enable_smooth_scroll", "FALSE",
	 &prefs_common.enable_smooth_scroll, P_BOOL,
	 NULL, NULL, NULL},
	{"scroll_step", "1", &prefs_common.scroll_step, P_INT,
	 NULL, NULL, NULL},
	{"scroll_half_page", "FALSE", &prefs_common.scroll_halfpage, P_BOOL,
	 NULL, NULL, NULL},
	{"respect_flowed_format", "FALSE", &prefs_common.respect_flowed_format, P_BOOL,
	 NULL, NULL, NULL},

	{"show_other_header", "FALSE", &prefs_common.show_other_header, P_BOOL,
	 NULL, NULL, NULL},

	{"use_different_print_font", "FALSE", &prefs_common.use_different_print_font, P_BOOL,
	 NULL, NULL, NULL},

	{"attach_desc", "TRUE", &prefs_common.attach_desc, P_BOOL,
	 NULL, NULL, NULL},
	{"attach_save_directory", NULL,
	 &SPECIFIC_PREFS.attach_save_dir, P_STRING, NULL, NULL, NULL},
	{"attach_load_directory", NULL,
	 &SPECIFIC_PREFS.attach_load_dir, P_STRING, NULL, NULL, NULL},

	/* MIME viewer */
	{"mime_textviewer",   NULL,
	 &SPECIFIC_PREFS.mime_textviewer,   P_STRING, NULL, NULL, NULL},
	{"mime_open_command", "gedit '%s'",
	 &SPECIFIC_PREFS.mime_open_cmd,     P_STRING, NULL, NULL, NULL},

	/* Interface */
	{"separate_folder", "FALSE", &prefs_common.sep_folder, P_BOOL,
	 NULL, NULL, NULL},
	{"separate_message", "FALSE", &prefs_common.sep_msg, P_BOOL,
	 NULL, NULL, NULL},

	/* {"emulate_emacs", "FALSE", &prefs_common.emulate_emacs, P_BOOL,
	 NULL, NULL, NULL}, */
	{"always_show_message_when_selected", "FALSE",
	 &prefs_common.always_show_msg,
	 P_BOOL, NULL, NULL, NULL},
	{"select_on_entry", "2", &prefs_common.select_on_entry,
	 P_ENUM, NULL, NULL, NULL},
	{"mark_as_read_on_new_window", "FALSE",
	 &prefs_common.mark_as_read_on_new_window,
	 P_BOOL, NULL, NULL, NULL},
	{"mark_as_read_delay", "0",
	 &prefs_common.mark_as_read_delay, P_INT, 
	 NULL, NULL, NULL},
	{"immediate_execution", "TRUE", &prefs_common.immediate_exec, P_BOOL,
	 NULL, NULL, NULL},
	{"nextunreadmsg_dialog", "1", &prefs_common.next_unread_msg_dialog, P_ENUM,
	 NULL, NULL, NULL},

	{"pixmap_theme_path", DEFAULT_PIXMAP_THEME, 
	 &SPECIFIC_PREFS.pixmap_theme_path, P_STRING,
	 NULL, NULL, NULL},

	{"ask_mark_all_read", "TRUE", &prefs_common.ask_mark_all_read, P_BOOL,
	 NULL, NULL, NULL},

	{"ask_apply_per_account_filtering_rules", "TRUE", &prefs_common.ask_apply_per_account_filtering_rules, P_BOOL,
	 NULL, NULL, NULL},
	{"apply_per_account_filtering_rules", "0", &prefs_common.apply_per_account_filtering_rules, P_ENUM,
	 NULL, NULL, NULL},

	/* Other */
	{"uri_open_command", DEFAULT_BROWSER_CMD,
	 &SPECIFIC_PREFS.uri_cmd, P_STRING, NULL, NULL, NULL},
	{"print_command", "lpr %s",
	 &SPECIFIC_PREFS.print_cmd, P_STRING, NULL, NULL, NULL},
	{"ext_editor_command", DEFAULT_EDITOR_CMD,
	 &SPECIFIC_PREFS.ext_editor_cmd, P_STRING, NULL, NULL, NULL},

	{"add_address_by_click", "FALSE", &prefs_common.add_address_by_click,
	 P_BOOL, NULL, NULL, NULL},
	{"confirm_on_exit", "FALSE", &prefs_common.confirm_on_exit, P_BOOL,
	 NULL, NULL, NULL},
	{"clean_trash_on_exit", "FALSE", &prefs_common.clean_on_exit, P_BOOL,
	 NULL, NULL, NULL},
	{"ask_on_cleaning", "TRUE", &prefs_common.ask_on_clean, P_BOOL,
	 NULL, NULL, NULL},
	{"warn_queued_on_exit", "TRUE", &prefs_common.warn_queued_on_exit,
	 P_BOOL, NULL, NULL, NULL},
	{"work_offline", "FALSE", &prefs_common.work_offline, P_BOOL,
	 NULL, NULL, NULL},
	{"summary_quicksearch_type", "0", &prefs_common.summary_quicksearch_type, P_INT,
	 NULL, NULL, NULL},
	{"summary_quicksearch_recurse", "1", &prefs_common.summary_quicksearch_recurse, P_INT,
	 NULL, NULL, NULL},

	{"io_timeout_secs", "60", &prefs_common.io_timeout_secs,
	 P_INT, NULL, NULL, NULL},
	{"hide_score", "-9999", &prefs_common.kill_score, P_INT,
	 NULL, NULL, NULL},
	{"important_score", "1", &prefs_common.important_score, P_INT,
	 NULL, NULL, NULL},
        {"clip_log", "TRUE", &prefs_common.cliplog, P_BOOL,
	 NULL, NULL, NULL},
	{"log_length", "500", &prefs_common.loglength, P_INT,
	 NULL, NULL, NULL},
	{"log_msg_color", "#00af00", &prefs_common.log_msg_color, P_COLOR,
	 NULL, NULL, NULL},
	{"log_warn_color", "#af0000", &prefs_common.log_warn_color, P_COLOR,
	 NULL, NULL, NULL},
	{"log_error_color", "#af0000", &prefs_common.log_error_color, P_COLOR,
	 NULL, NULL, NULL},
	{"log_in_color", "#000000", &prefs_common.log_in_color, P_COLOR,
	 NULL, NULL, NULL},
	{"log_out_color", "#0000ef", &prefs_common.log_out_color, P_COLOR,
	 NULL, NULL, NULL},

	{"color_new", "179", &prefs_common.color_new, P_COLOR,
	 NULL, NULL, NULL},

	/* Some windows' sizes */
	{"filteringwin_width", "500", &prefs_common.filteringwin_width, P_INT,
	 NULL, NULL, NULL},
	{"filteringwin_height", "-1", &prefs_common.filteringwin_height, P_INT,
	 NULL, NULL, NULL},

	{"filteringactionwin_width", "490", &prefs_common.filteringactionwin_width, P_INT,
	 NULL, NULL, NULL},
	{"filteringactionwin_height", "-1", &prefs_common.filteringactionwin_height, P_INT,
	 NULL, NULL, NULL},

	{"matcherwin_width", "520", &prefs_common.matcherwin_width, P_INT,
	 NULL, NULL, NULL},
	{"matcherwin_height", "-1", &prefs_common.matcherwin_height, P_INT,
	 NULL, NULL, NULL},

	{"templateswin_width", "440", &prefs_common.templateswin_width, P_INT,
	 NULL, NULL, NULL},
	{"templateswin_height", "-1", &prefs_common.templateswin_height, P_INT,
	 NULL, NULL, NULL},

	{"actionswin_width", "486", &prefs_common.actionswin_width, P_INT,
	 NULL, NULL, NULL},
	{"actionswin_height", "-1", &prefs_common.actionswin_height, P_INT,
	 NULL, NULL, NULL},

	{"addressbookwin_width", "520", &prefs_common.addressbookwin_width, P_INT,
	 NULL, NULL, NULL},
	{"addressbookwin_height", "-1", &prefs_common.addressbookwin_height, P_INT,
	 NULL, NULL, NULL},

	{"addressbookeditpersonwin_width", "640", &prefs_common.addressbookeditpersonwin_width, P_INT,
	 NULL, NULL, NULL},
	{"addressbookeditpersonwin_height", "-1", &prefs_common.addressbookeditpersonwin_height, P_INT,
	 NULL, NULL, NULL},

	{"addressbookeditgroupwin_width", "580", &prefs_common.addressbookeditgroupwin_width, P_INT,
	 NULL, NULL, NULL},
	{"addressbookeditgroupwin_height", "340", &prefs_common.addressbookeditgroupwin_height, P_INT,
	 NULL, NULL, NULL},

	{"pluginswin_width", "480", &prefs_common.pluginswin_width, P_INT,
	 NULL, NULL, NULL},
	{"pluginswin_height", "-1", &prefs_common.pluginswin_height, P_INT,
	 NULL, NULL, NULL},

	{"prefswin_width", "600", &prefs_common.prefswin_width, P_INT,
	 NULL, NULL, NULL},
	{"prefswin_height", "-1", &prefs_common.prefswin_height, P_INT,
	 NULL, NULL, NULL},

	{"folderitemwin_width", "500", &prefs_common.folderitemwin_width, P_INT,
	 NULL, NULL, NULL},
	{"folderitemwin_height", "-1", &prefs_common.folderitemwin_height, P_INT,
	 NULL, NULL, NULL},

	{"editaccountwin_width", "500", &prefs_common.editaccountwin_width, P_INT,
	 NULL, NULL, NULL},
	{"editaccountwin_height", "-1", &prefs_common.editaccountwin_height, P_INT,
	 NULL, NULL, NULL},

	{"accountswin_width", "500", &prefs_common.accountswin_width, P_INT,
	 NULL, NULL, NULL},
	{"accountswin_height", "-1", &prefs_common.accountswin_height, P_INT,
	 NULL, NULL, NULL},

	{"logwin_width", "520", &prefs_common.logwin_width, P_INT,
	 NULL, NULL, NULL},
	{"logwin_height", "-1", &prefs_common.logwin_height, P_INT,
	 NULL, NULL, NULL},

	{"folderselwin_width", "300", &prefs_common.folderselwin_width, P_INT,
	 NULL, NULL, NULL},
	{"folderselwin_height", "-1", &prefs_common.folderselwin_height, P_INT,
	 NULL, NULL, NULL},

	{"addressaddwin_width", "300", &prefs_common.addressaddwin_width, P_INT,
	 NULL, NULL, NULL},
	{"addressaddwin_height", "-1", &prefs_common.addressaddwin_height, P_INT,
	 NULL, NULL, NULL},

	{"addressbook_folderselwin_width", "300", &prefs_common.addressbook_folderselwin_width, P_INT,
	 NULL, NULL, NULL},
	{"addressbook_folderselwin_height", "-1", &prefs_common.addressbook_folderselwin_height, P_INT,
	 NULL, NULL, NULL},

	/* Hidden */
	{"warn_dnd", "1", &prefs_common.warn_dnd, P_INT,
	 NULL, NULL, NULL},
	{"utf8_instead_of_locale_for_broken_mail", "0", 
	 &prefs_common.broken_are_utf8, P_INT,
	 NULL, NULL, NULL},
	{"enable_swap_from", "FALSE", &prefs_common.swap_from, P_BOOL,
	 NULL, NULL, NULL},
	{"use_stripes_everywhere", "TRUE", &prefs_common.use_stripes_everywhere, P_BOOL,
	 NULL, NULL, NULL},
	{"use_stripes_in_summaries", "TRUE", &prefs_common.use_stripes_in_summaries, P_BOOL,
	 NULL, NULL, NULL},
	{"stripes_color_offset", "4000", &prefs_common.stripes_color_offset, P_INT,
	 NULL, NULL, NULL},
	{"enable_dotted_lines", "FALSE", &prefs_common.enable_dotted_lines, P_BOOL,
	 NULL, NULL, NULL},
	{"enable_hscrollbar", "TRUE", &prefs_common.enable_hscrollbar, P_BOOL,
	 NULL, NULL, NULL},
	{"folderview_vscrollbar_policy", "0",
	 &prefs_common.folderview_vscrollbar_policy, P_ENUM,
	 NULL, NULL, NULL},
	{"textview_cursor_visible", "FALSE",
	 &prefs_common.textview_cursor_visible, P_BOOL,
	 NULL, NULL, NULL},
	{"hover_timeout", "500", &prefs_common.hover_timeout, P_INT,
	 NULL, NULL, NULL},
	{"cache_max_mem_usage", "4096", &prefs_common.cache_max_mem_usage, P_INT,
	 NULL, NULL, NULL},
	{"cache_min_keep_time", "15", &prefs_common.cache_min_keep_time, P_INT,
	 NULL, NULL, NULL},
	{"thread_by_subject_max_age", "10", &prefs_common.thread_by_subject_max_age,
	P_INT, NULL, NULL, NULL },
	{"summary_quicksearch_sticky", "1", &prefs_common.summary_quicksearch_sticky, P_INT,
	 NULL, NULL, NULL},
	{"statusbar_update_step", "10", &prefs_common.statusbar_update_step, P_INT,
	 NULL, NULL, NULL},
	{"compose_no_markup", "FALSE", &prefs_common.compose_no_markup, P_BOOL,
	 NULL, NULL, NULL},
	{"skip_ssl_cert_check", "FALSE", &prefs_common.skip_ssl_cert_check, P_BOOL,
	 NULL, NULL, NULL},
	{"live_dangerously", "FALSE", &prefs_common.live_dangerously, P_BOOL,
	 NULL, NULL, NULL},
	{"hide_quotes", "0", &prefs_common.hide_quotes, P_INT,
	 NULL, NULL, NULL},

	{NULL, NULL, NULL, P_OTHER, NULL, NULL, NULL}
};

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
	if ((fp = g_fopen(path, "rb")) == NULL) {
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
	gchar *tmp;

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMON_RC, NULL);
	prefs_read_config(param, "Common", rcpath, NULL);
#ifdef G_OS_WIN32
	prefs_read_config(param_os_specific, "CommonWin32", rcpath, NULL);
#endif

	g_free(rcpath);

	tmp = g_strdup(gettext(prefs_common.quotefmt));
	g_free(prefs_common.quotefmt);
	prefs_common.quotefmt = tmp;

	tmp = g_strdup(gettext(prefs_common.fw_quotefmt));
	g_free(prefs_common.fw_quotefmt);
	prefs_common.fw_quotefmt = tmp;
	
	tmp = g_strdup(gettext(prefs_common.date_format));
	g_free(prefs_common.date_format);
	prefs_common.date_format = tmp;

	prefs_common.mime_open_cmd_history =
		prefs_common_read_history(COMMAND_HISTORY);
	prefs_common.summary_quicksearch_history =
		prefs_common_read_history(QUICKSEARCH_HISTORY);

	colorlabel_update_colortable_from_prefs();
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
	if ((fp = g_fopen(path, "wb")) == NULL) {
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
#ifdef G_OS_WIN32
	prefs_write_config(param_os_specific, "CommonWin32", COMMON_RC);
#endif

	prefs_common_save_history(COMMAND_HISTORY, 
		prefs_common.mime_open_cmd_history);
	prefs_common_save_history(QUICKSEARCH_HISTORY, 
		prefs_common.summary_quicksearch_history);
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
