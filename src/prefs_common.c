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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "intl.h"
#include "main.h"
#include "prefs.h"
#include "prefs_common.h"
#include "mainwindow.h"
#include "summaryview.h"
#include "messageview.h"
#include "manage_window.h"
#include "menu.h"
#include "codeconv.h"
#include "utils.h"
#include "gtkutils.h"
#include "alertpanel.h"
#include "folder.h"
#include "prefs_display_headers.h"

PrefsCommon prefs_common;

static PrefsDialog dialog;

static struct Receive {
	GtkWidget *checkbtn_incext;
	GtkWidget *entry_incext;
	GtkWidget *button_incext;

	GtkWidget *checkbtn_local;
	GtkWidget *checkbtn_filter_on_inc;
	GtkWidget *entry_spool;

	GtkWidget *checkbtn_autochk;
	GtkWidget *spinbtn_autochk;
	GtkObject *spinbtn_autochk_adj;

	GtkWidget *checkbtn_chkonstartup;

	GtkWidget *spinbtn_maxarticle;
	GtkObject *spinbtn_maxarticle_adj;
} receive;

static struct Send {
	GtkWidget *checkbtn_sendext;
	GtkWidget *entry_sendext;
	GtkWidget *button_sendext;

	GtkWidget *checkbtn_savemsg;
	GtkWidget *checkbtn_queuemsg;

	GtkWidget *optmenu_charset;
} send;

static struct Compose {
	GtkWidget *checkbtn_quote;
	GtkWidget *entry_quotemark;
	GtkWidget *text_quotefmt;
	GtkWidget *checkbtn_autosig;
	GtkWidget *entry_sigsep;

	GtkWidget *spinbtn_linewrap;
	GtkObject *spinbtn_linewrap_adj;
	GtkWidget *checkbtn_wrapquote;
	GtkWidget *checkbtn_wrapatsend;
} compose;

static struct Display {
	GtkWidget *entry_textfont;
	GtkWidget *button_textfont;

	GtkWidget *chkbtn_folder_unread;

	GtkWidget *chkbtn_transhdr;

	GtkWidget *chkbtn_swapfrom;
	GtkWidget *chkbtn_hscrollbar;
} display;

static struct Message {
	GtkWidget *chkbtn_enablecol;
	GtkWidget *button_edit_col;
	GtkWidget *chkbtn_mbalnum;
	GtkWidget *chkbtn_disphdrpane;
	GtkWidget *chkbtn_disphdr;
	GtkWidget *spinbtn_linespc;
	GtkObject *spinbtn_linespc_adj;
	GtkWidget *chkbtn_headspc;

	GtkWidget *chkbtn_smoothscroll;
	GtkWidget *spinbtn_scrollstep;
	GtkObject *spinbtn_scrollstep_adj;
	GtkWidget *chkbtn_halfpage;
} message;

#if USE_GPGME
static struct Privacy {
	GtkWidget *checkbtn_gpgme_warning;
	GtkWidget *checkbtn_default_encrypt;
	GtkWidget *checkbtn_default_sign;
	GtkWidget *checkbtn_auto_check_signatures;
	GtkWidget *checkbtn_passphrase_grab;
	GtkWidget *optmenu_default_signkey;
} privacy;
#endif

static struct Interface {
	GtkWidget *checkbtn_emacs;
	GtkWidget *checkbtn_openunread;
	GtkWidget *checkbtn_openinbox;
	GtkWidget *checkbtn_immedexec;
	GtkWidget *checkbtn_confonexit;
	GtkWidget *checkbtn_cleanonexit;
	GtkWidget *checkbtn_askonclean;
	GtkWidget *checkbtn_warnqueued;
	GtkWidget *checkbtn_returnreceipt;
	GtkWidget *checkbtn_addaddrbyclick;
} interface;

static struct Other {
	GtkWidget *uri_combo;
	GtkWidget *uri_entry;
	GtkWidget *printcmd_entry;
	GtkWidget *exteditor_combo;
	GtkWidget *exteditor_entry;
} other;

static struct MessageColorButtons {
	GtkWidget *quote_level1_btn;
	GtkWidget *quote_level2_btn;
	GtkWidget *quote_level3_btn;
	GtkWidget *uri_btn;
} color_buttons;

static GtkWidget *quote_desc_win;
static GtkWidget *font_sel_win;
static GtkWidget *quote_color_win;
static GtkWidget *color_dialog;
static GtkWidget *entry_datefmt;
static GtkWidget *datefmt_sample;

static void prefs_common_charset_set_data_from_optmenu(PrefParam *pparam);
static void prefs_common_charset_set_optmenu	      (PrefParam *pparam);
#if USE_GPGME
static void prefs_common_default_signkey_set_data_from_optmenu
							(PrefParam *pparam);
static void prefs_common_default_signkey_set_optmenu	(PrefParam *pparam);
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
	 &receive.checkbtn_incext,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"ext_inc_path", DEFAULT_INC_PATH, &prefs_common.extinc_path, P_STRING,
	 &receive.entry_incext,
	 prefs_set_data_from_entry, prefs_set_entry},

	{"inc_local", "TRUE", &prefs_common.inc_local, P_BOOL,
	 &receive.checkbtn_local,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"filter_on_inc_local", "FALSE", &prefs_common.filter_on_inc, P_BOOL,
	 &receive.checkbtn_filter_on_inc,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"spool_path", DEFAULT_SPOOL_PATH, &prefs_common.spool_path, P_STRING,
	 &receive.entry_spool,
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

	{"max_news_articles", "300", &prefs_common.max_articles, P_INT,
	 &receive.spinbtn_maxarticle,
	 prefs_set_data_from_spinbtn, prefs_set_spinbtn},

	/* Send */
	{"use_ext_send", "FALSE", &prefs_common.use_extsend, P_BOOL,
	 &send.checkbtn_sendext,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"ext_send_path", NULL, &prefs_common.extsend_path, P_STRING,
	 &send.entry_sendext, prefs_set_data_from_entry, prefs_set_entry},
	{"save_message", "TRUE", &prefs_common.savemsg, P_BOOL,
	 &send.checkbtn_savemsg,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"queue_message", "FALSE", &prefs_common.queue_msg, P_BOOL,
	 &send.checkbtn_queuemsg,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"outgoing_charset", CS_AUTO, &prefs_common.outgoing_charset, P_STRING,
	 &send.optmenu_charset,
	 prefs_common_charset_set_data_from_optmenu,
	 prefs_common_charset_set_optmenu},

	/* Compose */
	{"reply_with_quote", "TRUE", &prefs_common.reply_with_quote, P_BOOL,
	 &compose.checkbtn_quote,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"quote_mark", "> ", &prefs_common.quotemark, P_STRING,
	 &compose.entry_quotemark, prefs_set_data_from_entry, prefs_set_entry},
	{"quote_format", "On %d\\n%f wrote:\\n\\n",
	 &prefs_common.quotefmt, P_STRING, &compose.text_quotefmt,
	 prefs_set_data_from_text, prefs_set_text},

	{"auto_signature", "TRUE", &prefs_common.auto_sig, P_BOOL,
	 &compose.checkbtn_autosig,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"signature_separator", "-- ", &prefs_common.sig_sep, P_STRING,
	 &compose.entry_sigsep, prefs_set_data_from_entry, prefs_set_entry},

	{"linewrap_length", "74", &prefs_common.linewrap_len, P_INT,
	 &compose.spinbtn_linewrap,
	 prefs_set_data_from_spinbtn, prefs_set_spinbtn},
	{"linewrap_quotation", "FALSE", &prefs_common.linewrap_quote, P_BOOL,
	 &compose.checkbtn_wrapquote,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"linewrap_before_sending", "FALSE",
	 &prefs_common.linewrap_at_send, P_BOOL,
	 &compose.checkbtn_wrapatsend,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"show_ruler", "TRUE", &prefs_common.show_ruler, P_BOOL,
	 NULL, NULL, NULL},

	/* Display */
	{"widget_font", NULL, &prefs_common.widgetfont, P_STRING,
	 NULL, NULL, NULL},
	{"message_font", "-misc-fixed-medium-r-normal--14-*-*-*-*-*-*-*",
	 &prefs_common.textfont, P_STRING,
	 &display.entry_textfont,
	 prefs_set_data_from_entry, prefs_set_entry},

	{"display_folder_unread_num", "TRUE",
	 &prefs_common.display_folder_unread, P_BOOL,
	 &display.chkbtn_folder_unread,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"translate_header", "TRUE", &prefs_common.trans_hdr, P_BOOL,
	 &display.chkbtn_transhdr,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	/* Display: Summary View */
	{"enable_swap_from", "TRUE", &prefs_common.swap_from, P_BOOL,
	 &display.chkbtn_swapfrom,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"enable_hscrollbar", "TRUE", &prefs_common.enable_hscrollbar, P_BOOL,
	 &display.chkbtn_hscrollbar,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"date_format", "%y/%m/%d(%a) %H:%M", &prefs_common.date_format,
	 P_STRING, &entry_datefmt,
	 prefs_set_data_from_entry, prefs_set_entry},

	{"enable_thread", "TRUE", &prefs_common.enable_thread, P_BOOL,
	 NULL, NULL, NULL},
	{"toolbar_style", "3", &prefs_common.toolbar_style, P_ENUM,
	 NULL, NULL, NULL},
	{"show_statusbar", "TRUE", &prefs_common.show_statusbar, P_BOOL,
	 NULL, NULL, NULL},

	{"show_mark", "TRUE", &prefs_common.show_mark, P_BOOL,
	 NULL, NULL, NULL},
	{"show_unread", "TRUE", &prefs_common.show_unread, P_BOOL,
	 NULL, NULL, NULL},
	{"show_mime", "TRUE", &prefs_common.show_mime, P_BOOL,
	 NULL, NULL, NULL},
	{"show_number", "TRUE", &prefs_common.show_number, P_BOOL,
	 NULL, NULL, NULL},
	{"show_score", "TRUE", &prefs_common.show_score, P_BOOL,
	 NULL, NULL, NULL},
	{"show_size", "FALSE", &prefs_common.show_size, P_BOOL,
	 NULL, NULL, NULL},
	{"show_date", "TRUE", &prefs_common.show_date, P_BOOL,
	 NULL, NULL, NULL},
	{"show_from", "TRUE", &prefs_common.show_from, P_BOOL,
	 NULL, NULL, NULL},
	{"show_subject", "TRUE", &prefs_common.show_subject, P_BOOL,
	 NULL, NULL, NULL},

	/* Widget size */
	{"folderview_width", "179", &prefs_common.folderview_width, P_INT,
	 NULL, NULL, NULL},
	{"folderview_height", "600", &prefs_common.folderview_height, P_INT,
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
	{"summaryview_height", "173", &prefs_common.summaryview_height, P_INT,
	 NULL, NULL, NULL},
	{"summary_col_mark", "10", &prefs_common.summary_col_mark, P_INT,
	 NULL, NULL, NULL},
	{"summary_col_unread", "13", &prefs_common.summary_col_unread, P_INT,
	 NULL, NULL, NULL},
	{"summary_col_mime", "10", &prefs_common.summary_col_mime, P_INT,
	 NULL, NULL, NULL},
	{"summary_col_number", "40", &prefs_common.summary_col_number, P_INT,
	 NULL, NULL, NULL},
	{"summary_col_score", "40", &prefs_common.summary_col_score,
	 P_INT, NULL, NULL, NULL},
	{"summary_col_size", "48", &prefs_common.summary_col_size, P_INT,
	 NULL, NULL, NULL},
	{"summary_col_date", "120", &prefs_common.summary_col_date, P_INT,
	 NULL, NULL, NULL},
	{"summary_col_from", "140", &prefs_common.summary_col_from, P_INT,
	 NULL, NULL, NULL},
	{"summary_col_subject", "200", &prefs_common.summary_col_subject,
	 P_INT, NULL, NULL, NULL},
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
	{"compose_width", "600", &prefs_common.compose_width, P_INT,
	 NULL, NULL, NULL},
	{"compose_height", "560", &prefs_common.compose_height, P_INT,
	 NULL, NULL, NULL},

	/* Message */
	{"enable_color", "TRUE", &prefs_common.enable_color, P_BOOL,
	 &message.chkbtn_enablecol,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"quote_level1_color", "179", &prefs_common.quote_level1_col, P_INT,
	 NULL, NULL, NULL},
	{"quote_level2_color", "179", &prefs_common.quote_level2_col, P_INT,
	 NULL, NULL, NULL},
	{"quote_level3_color", "179", &prefs_common.quote_level3_col, P_INT,
	 NULL, NULL, NULL},
	{"uri_color", "32512", &prefs_common.uri_col, P_INT,
	 NULL, NULL, NULL},
	{"signature_color", "0", &prefs_common.sig_col, P_USHORT,
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
	{"line_space", "2", &prefs_common.line_space, P_INT,
	 &message.spinbtn_linespc,
	 prefs_set_data_from_spinbtn, prefs_set_spinbtn},
	{"enable_head_space", "FALSE", &prefs_common.head_space, P_BOOL,
	 &message.chkbtn_headspc,
	 prefs_set_data_from_toggle, prefs_set_toggle},

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

	/* MIME viewer */
	{"mime_image_viewer", "display '%s'",
	 &prefs_common.mime_image_viewer, P_STRING, NULL, NULL, NULL},
	{"mime_audio_player", "play '%s'",
	 &prefs_common.mime_audio_player, P_STRING, NULL, NULL, NULL},

#if USE_GPGME
	/* Privacy */
	{"gpgme_warning", "TRUE", &prefs_common.gpgme_warning, P_BOOL,
	 &privacy.checkbtn_gpgme_warning,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"default_encrypt", "FALSE", &prefs_common.default_encrypt, P_BOOL,
	 &privacy.checkbtn_default_encrypt,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"default_sign", "FALSE", &prefs_common.default_sign, P_BOOL,
	 &privacy.checkbtn_default_sign,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"auto_check_signatures", "TRUE",
	 &prefs_common.auto_check_signatures, P_BOOL,
	 &privacy.checkbtn_auto_check_signatures,
	 prefs_set_data_from_toggle, prefs_set_toggle},
#ifndef __MINGW32__
	{"passphrase_grab", "FALSE", &prefs_common.passphrase_grab, P_BOOL,
	 &privacy.checkbtn_passphrase_grab,
	 prefs_set_data_from_toggle, prefs_set_toggle},
#endif /* __MINGW32__ */
	{"default_signkey", CS_AUTO, &prefs_common.default_signkey, P_STRING,
	 &privacy.optmenu_default_signkey,
	 prefs_common_default_signkey_set_data_from_optmenu,
	 prefs_common_default_signkey_set_optmenu},
#endif /* USE_GPGME */

	/* Interface */
	{"separate_folder", "FALSE", &prefs_common.sep_folder, P_BOOL,
	 NULL, NULL, NULL},
	{"separate_message", "FALSE", &prefs_common.sep_msg, P_BOOL,
	 NULL, NULL, NULL},

	{"emulate_emacs", "FALSE", &prefs_common.emulate_emacs, P_BOOL,
	 &interface.checkbtn_emacs,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"open_unread_on_enter", "FALSE", &prefs_common.open_unread_on_enter,
	 P_BOOL, &interface.checkbtn_openunread,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"open_inbox_on_inc", "TRUE", &prefs_common.open_inbox_on_inc,
	 P_BOOL, &interface.checkbtn_openinbox,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"immediate_execution", "TRUE", &prefs_common.immediate_exec, P_BOOL,
	 &interface.checkbtn_immedexec,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"add_address_by_click", "FALSE", &prefs_common.add_address_by_click,
	 P_BOOL, &interface.checkbtn_addaddrbyclick,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"confirm_on_exit", "TRUE", &prefs_common.confirm_on_exit, P_BOOL,
	 &interface.checkbtn_confonexit,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"send_return_receipt", "TRUE", &prefs_common.return_receipt, P_BOOL,
	 &interface.checkbtn_returnreceipt,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"clean_trash_on_exit", "FALSE", &prefs_common.clean_on_exit, P_BOOL,
	 &interface.checkbtn_cleanonexit,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"ask_on_cleaning", "TRUE", &prefs_common.ask_on_clean, P_BOOL,
	 &interface.checkbtn_askonclean,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"warn_queued_on_exit", "TRUE", &prefs_common.warn_queued_on_exit,
	 P_BOOL, &interface.checkbtn_warnqueued,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	/* Other */
	{"uri_open_command", "netscape -remote 'openURL(%s,raise)'",
	 &prefs_common.uri_cmd, P_STRING,
	 &other.uri_entry, prefs_set_data_from_entry, prefs_set_entry},
	{"print_command", "lpr %s", &prefs_common.print_cmd, P_STRING,
	 &other.printcmd_entry, prefs_set_data_from_entry, prefs_set_entry},
	{"ext_editor_command", "gedit %s",
	 &prefs_common.ext_editor_cmd, P_STRING,
	 &other.exteditor_entry, prefs_set_data_from_entry, prefs_set_entry},

	{NULL, NULL, NULL, P_OTHER, NULL, NULL, NULL}
};

#define VSPACING		12
#define VSPACING_NARROW		4
#define VBOX_BORDER		16
#define DEFAULT_ENTRY_WIDTH	80
#define PREFSBUFSIZE		1024

/* widget creating functions */
static void prefs_common_create		(void);
static void prefs_receive_create	(void);
static void prefs_send_create		(void);
static void prefs_compose_create	(void);
static void prefs_display_create	(void);
static void prefs_message_create	(void);
#if USE_GPGME
static void prefs_privacy_create	(void);
#endif
static void prefs_interface_create	(void);
static void prefs_other_create		(void);

static void prefs_quote_description		(void);
static void prefs_quote_description_create	(void);
static void prefs_quote_colors_dialog		(void);
static void prefs_quote_colors_dialog_create	(void);
static void quote_color_set_dialog		(GtkWidget	*widget,
						 gpointer	 data);
static void quote_colors_set_dialog_ok		(GtkWidget	*widget,
						 gpointer	 data);
static void quote_colors_set_dialog_cancel	(GtkWidget	*widget,
						 gpointer	 data);
static void set_button_bg_color			(GtkWidget	*widget,
						 gint		 color);
static void prefs_enable_message_color_toggled	(void);
static void prefs_recycle_colors_toggled	(GtkWidget	*widget);

/* functions for setting items of SummaryView */
static void prefs_summary_display_item_dialog_create
					(gboolean	*cancelled);
static void display_item_ok		(GtkWidget	*widget,
					 gboolean	*cancelled);
static void display_item_cancel		(GtkWidget	*widget,
					 gboolean	*cancelled);
static gint display_item_delete_event	(GtkWidget	*widget,
					 GdkEventAny	*event,
					 gboolean	*cancelled);
static void display_item_key_pressed	(GtkWidget	*widget,
					 GdkEventKey	*event,
					 gboolean	*cancelled);

static void prefs_font_select			(GtkButton	*button);
static void prefs_font_selection_key_pressed	(GtkWidget	*widget,
						 GdkEventKey	*event,
						 gpointer	 data);
static void prefs_font_selection_ok		(GtkButton	*button);

static void prefs_common_key_pressed	(GtkWidget	*widget,
					 GdkEventKey	*event,
					 gpointer	 data);
static void prefs_common_ok		(GtkButton	*button);
static void prefs_common_apply		(GtkButton	*button);

void prefs_common_read_config(void)
{
	prefs_read_config(param, "Common", COMMON_RC);
}

void prefs_common_save_config(void)
{
	prefs_save_config(param, "Common", COMMON_RC);
}

void prefs_common_open(void)
{
	if (!dialog.window) {
		prefs_common_create();
	}

	manage_window_set_transient(GTK_WINDOW(dialog.window));
	gtk_notebook_set_page(GTK_NOTEBOOK(dialog.notebook), 0);
	gtk_widget_grab_focus(dialog.ok_btn);

	prefs_set_dialog(param);

	gtk_widget_show(dialog.window);
}

static void prefs_common_create(void)
{
	gint page = 0;

	debug_print(_("Creating common preferences window...\n"));

	prefs_dialog_create(&dialog);
	gtk_window_set_title (GTK_WINDOW(dialog.window),
			      _("Common Preferences"));
	gtk_signal_connect (GTK_OBJECT(dialog.window), "delete_event",
			    GTK_SIGNAL_FUNC(gtk_widget_hide_on_delete), NULL);
	gtk_signal_connect (GTK_OBJECT(dialog.window), "key_press_event",
			    GTK_SIGNAL_FUNC(prefs_common_key_pressed), NULL);
	gtk_signal_connect (GTK_OBJECT(dialog.window), "focus_in_event",
			    GTK_SIGNAL_FUNC(manage_window_focus_in), NULL);
	gtk_signal_connect (GTK_OBJECT(dialog.window), "focus_out_event",
			    GTK_SIGNAL_FUNC(manage_window_focus_out), NULL);
	gtk_signal_connect (GTK_OBJECT(dialog.ok_btn), "clicked",
			    GTK_SIGNAL_FUNC(prefs_common_ok), NULL);
	gtk_signal_connect (GTK_OBJECT(dialog.apply_btn), "clicked",
			    GTK_SIGNAL_FUNC(prefs_common_apply), NULL);
	gtk_signal_connect_object (GTK_OBJECT(dialog.cancel_btn), "clicked",
				   GTK_SIGNAL_FUNC(gtk_widget_hide_on_delete),
				   GTK_OBJECT(dialog.window));

	/* create all widgets on notebook */
	prefs_receive_create();
	SET_NOTEBOOK_LABEL(dialog.notebook, _("Receive"),   page++);
	prefs_send_create();
	SET_NOTEBOOK_LABEL(dialog.notebook, _("Send"),      page++);
	prefs_compose_create();
	SET_NOTEBOOK_LABEL(dialog.notebook, _("Compose"),   page++);
	prefs_display_create();
	SET_NOTEBOOK_LABEL(dialog.notebook, _("Display"),   page++);
	prefs_message_create();
	SET_NOTEBOOK_LABEL(dialog.notebook, _("Message"),   page++);
#if USE_GPGME
	prefs_privacy_create();
	SET_NOTEBOOK_LABEL(dialog.notebook, _("Privacy"),   page++);
#endif
	prefs_interface_create();
	SET_NOTEBOOK_LABEL(dialog.notebook, _("Interface"), page++);
	prefs_other_create();
	SET_NOTEBOOK_LABEL(dialog.notebook, _("Other"),     page++);

	gtk_widget_show_all(dialog.window);
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
	GtkWidget *button_incext;

	GtkWidget *frame_spool;
	GtkWidget *checkbtn_local;
	GtkWidget *checkbtn_filter_on_inc;
	GtkWidget *label_spool;
	GtkWidget *entry_spool;

	GtkWidget *hbox_autochk;
	GtkWidget *checkbtn_autochk;
	GtkWidget *label_autochk1;
	GtkObject *spinbtn_autochk_adj;
	GtkWidget *spinbtn_autochk;
	GtkWidget *label_autochk2;
	GtkWidget *checkbtn_chkonstartup;

	GtkWidget *frame_news;
	GtkWidget *label_maxarticle;
	GtkWidget *spinbtn_maxarticle;
	GtkObject *spinbtn_maxarticle_adj;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (dialog.notebook), vbox1);
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

	label_incext = gtk_label_new (_("Program path"));
	gtk_widget_show (label_incext);
	gtk_box_pack_start (GTK_BOX (hbox), label_incext, FALSE, FALSE, 0);

	entry_incext = gtk_entry_new ();
	gtk_widget_show (entry_incext);
	gtk_box_pack_start (GTK_BOX (hbox), entry_incext, TRUE, TRUE, 0);

	button_incext = gtk_button_new_with_label ("... ");
	gtk_widget_show (button_incext);
	gtk_box_pack_start (GTK_BOX (hbox), button_incext, FALSE, FALSE, 0);

	PACK_FRAME(vbox1, frame_spool, _("Local spool"));

	vbox2 = gtk_vbox_new (FALSE, VSPACING_NARROW);
	gtk_widget_show (vbox2);
	gtk_container_add (GTK_CONTAINER (frame_spool), vbox2);
	gtk_container_set_border_width (GTK_CONTAINER (vbox2), 8);

	hbox = gtk_hbox_new (FALSE, 32);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON (hbox, checkbtn_local, _("Incorporate from spool"));
	PACK_CHECK_BUTTON (hbox, checkbtn_filter_on_inc,
			   _("Filter on incorporation"));
	SET_TOGGLE_SENSITIVITY (checkbtn_local, checkbtn_filter_on_inc);

	hbox = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox, FALSE, FALSE, 0);
	SET_TOGGLE_SENSITIVITY (checkbtn_local, hbox);

	label_spool = gtk_label_new (_("Spool directory"));
	gtk_widget_show (label_spool);
	gtk_box_pack_start (GTK_BOX (hbox), label_spool, FALSE, FALSE, 0);

	entry_spool = gtk_entry_new ();
	gtk_widget_show (entry_spool);
	gtk_box_pack_start (GTK_BOX (hbox), entry_spool, TRUE, TRUE, 0);

	vbox2 = gtk_vbox_new (FALSE, VSPACING_NARROW);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, FALSE, 0);

	/* Auto-checking */
	hbox_autochk = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox_autochk);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox_autochk, FALSE, FALSE, 0);

	gtk_widget_set_sensitive(hbox_autochk, FALSE);

	PACK_CHECK_BUTTON (hbox_autochk, checkbtn_autochk,
			   _("Auto-check new mail"));

	label_autochk1 = gtk_label_new (_("each"));
	gtk_widget_show (label_autochk1);
	gtk_box_pack_start (GTK_BOX (hbox_autochk), label_autochk1, FALSE, FALSE, 0);

	spinbtn_autochk_adj = gtk_adjustment_new (5, 1, 100, 1, 10, 10);
	spinbtn_autochk = gtk_spin_button_new
		(GTK_ADJUSTMENT (spinbtn_autochk_adj), 1, 0);
	gtk_widget_show (spinbtn_autochk);
	gtk_box_pack_start (GTK_BOX (hbox_autochk), spinbtn_autochk, FALSE, FALSE, 0);
	gtk_widget_set_usize (spinbtn_autochk, 64, -1);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbtn_autochk), TRUE);

	label_autochk2 = gtk_label_new (_("minute(s)"));
	gtk_widget_show (label_autochk2);
	gtk_box_pack_start (GTK_BOX (hbox_autochk), label_autochk2, FALSE, FALSE, 0);

	SET_TOGGLE_SENSITIVITY(checkbtn_autochk, label_autochk1);
	SET_TOGGLE_SENSITIVITY(checkbtn_autochk, spinbtn_autochk);
	SET_TOGGLE_SENSITIVITY(checkbtn_autochk, label_autochk2);

	PACK_CHECK_BUTTON (vbox2, checkbtn_chkonstartup,
			   _("Check new mail on startup"));

	PACK_FRAME(vbox1, frame_news, _("News"));

	hbox = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox);
	gtk_container_add (GTK_CONTAINER (frame_news), hbox);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 8);

	label_maxarticle = gtk_label_new
		(_("Maximum article number to download\n"
		   "(unlimited if 0 is specified)"));
	gtk_widget_show (label_maxarticle);
	gtk_box_pack_start (GTK_BOX (hbox), label_maxarticle, FALSE, FALSE, 0);
	gtk_label_set_justify (GTK_LABEL (label_maxarticle), GTK_JUSTIFY_LEFT);

	spinbtn_maxarticle_adj =
		gtk_adjustment_new (300, 0, 10000, 10, 100, 100);
	spinbtn_maxarticle = gtk_spin_button_new
		(GTK_ADJUSTMENT (spinbtn_maxarticle_adj), 10, 0);
	gtk_widget_show (spinbtn_maxarticle);
	gtk_box_pack_start (GTK_BOX (hbox), spinbtn_maxarticle,
			    FALSE, FALSE, 0);
	gtk_widget_set_usize (spinbtn_maxarticle, 64, -1);
	gtk_spin_button_set_numeric
		(GTK_SPIN_BUTTON (spinbtn_maxarticle), TRUE);

	receive.checkbtn_incext = checkbtn_incext;
	receive.entry_incext    = entry_incext;
	receive.button_incext   = button_incext;

	receive.checkbtn_local         = checkbtn_local;
	receive.checkbtn_filter_on_inc = checkbtn_filter_on_inc;
	receive.entry_spool            = entry_spool;

	receive.checkbtn_autochk    = checkbtn_autochk;
	receive.spinbtn_autochk     = spinbtn_autochk;
	receive.spinbtn_autochk_adj = spinbtn_autochk_adj;

	receive.checkbtn_chkonstartup = checkbtn_chkonstartup;

	receive.spinbtn_maxarticle     = spinbtn_maxarticle;
	receive.spinbtn_maxarticle_adj = spinbtn_maxarticle_adj;
}

static void prefs_send_create(void)
{
	GtkWidget *vbox1;
	GtkWidget *vbox2;
	GtkWidget *frame_sendext;
	GtkWidget *vbox_sendext;
	GtkWidget *checkbtn_sendext;
	GtkWidget *hbox1;
	GtkWidget *label_sendext;
	GtkWidget *entry_sendext;
	GtkWidget *button_sendext;
	GtkWidget *checkbtn_savemsg;
	GtkWidget *checkbtn_queuemsg;
	GtkWidget *label_outcharset;
	GtkWidget *optmenu;
	GtkWidget *optmenu_menu;
	GtkWidget *menuitem;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (dialog.notebook), vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	PACK_FRAME(vbox1, frame_sendext, _("External program"));

	gtk_widget_set_sensitive(frame_sendext, FALSE);

	vbox_sendext = gtk_vbox_new (FALSE, VSPACING_NARROW);
	gtk_widget_show (vbox_sendext);
	gtk_container_add (GTK_CONTAINER (frame_sendext), vbox_sendext);
	gtk_container_set_border_width (GTK_CONTAINER (vbox_sendext), 8);

	PACK_CHECK_BUTTON (vbox_sendext, checkbtn_sendext,
			   _("Use external program for sending"));

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox_sendext), hbox1, FALSE, FALSE, 0);
	SET_TOGGLE_SENSITIVITY(checkbtn_sendext, hbox1);

	label_sendext = gtk_label_new (_("Program path"));
	gtk_widget_show (label_sendext);
	gtk_box_pack_start (GTK_BOX (hbox1), label_sendext, FALSE, FALSE, 0);

	entry_sendext = gtk_entry_new ();
	gtk_widget_show (entry_sendext);
	gtk_box_pack_start (GTK_BOX (hbox1), entry_sendext, TRUE, TRUE, 0);

	button_sendext = gtk_button_new_with_label ("... ");
	gtk_widget_show (button_sendext);
	gtk_box_pack_start (GTK_BOX (hbox1), button_sendext, FALSE, FALSE, 0);

	vbox2 = gtk_vbox_new (FALSE, VSPACING_NARROW);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON (vbox2, checkbtn_savemsg,
			   _("Save sent message to outbox"));
	PACK_CHECK_BUTTON (vbox2, checkbtn_queuemsg,
			   _("Queue message that failed to send"));

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);

	label_outcharset = gtk_label_new (_("Outgoing codeset"));
	gtk_widget_show (label_outcharset);
	gtk_box_pack_start (GTK_BOX (hbox1), label_outcharset, FALSE, FALSE, 0);

	optmenu = gtk_option_menu_new ();
	gtk_widget_show (optmenu);
	gtk_box_pack_start(GTK_BOX (hbox1), optmenu, FALSE, FALSE, 0);

	optmenu_menu = gtk_menu_new ();

#define SET_MENUITEM(str, charset) \
{ \
	MENUITEM_ADD(optmenu_menu, menuitem, str, charset); \
}

	SET_MENUITEM(_("Automatic"),			 CS_AUTO);
	SET_MENUITEM(_("7bit ascii (US-ASCII)"),	 CS_US_ASCII);
#if HAVE_LIBJCONV
	SET_MENUITEM(_("Unicode (UTF-8)"),		 CS_UTF_8);
#endif
	SET_MENUITEM(_("Western European (ISO-8859-1)"), CS_ISO_8859_1);
#if HAVE_LIBJCONV
	SET_MENUITEM(_("Central European (ISO-8859-2)"), CS_ISO_8859_2);
	SET_MENUITEM(_("Baltic (ISO-8859-13)"),		 CS_ISO_8859_13);
	SET_MENUITEM(_("Baltic (ISO-8859-4)"),		 CS_ISO_8859_4);
	SET_MENUITEM(_("Greek (ISO-8859-7)"),		 CS_ISO_8859_7);
	SET_MENUITEM(_("Turkish (ISO-8859-9)"),		 CS_ISO_8859_9);
	SET_MENUITEM(_("Cyrillic (ISO-8859-5)"),	 CS_ISO_8859_5);
	SET_MENUITEM(_("Cyrillic (KOI8-R)"),		 CS_KOI8_R);
	SET_MENUITEM(_("Cyrillic (Windows-1251)"),	 CS_CP1251);
	SET_MENUITEM(_("Cyrillic (KOI8-U)"),		 CS_KOI8_U);
#endif /* HAVE_LIBJCONV */
	SET_MENUITEM(_("Japanese (ISO-2022-JP)"),	 CS_ISO_2022_JP);
#if 0
	SET_MENUITEM(_("Japanese (EUC-JP)"),		 CS_EUC_JP);
	SET_MENUITEM(_("Japanese (Shift_JIS)"),		 CS_SHIFT_JIS);
#endif /* 0 */
#if HAVE_LIBJCONV
	SET_MENUITEM(_("Simplified Chinese (GB2312)"),	 CS_GB2312);
	SET_MENUITEM(_("Traditional Chinese (Big5)"),	 CS_BIG5);
#if 0
	SET_MENUITEM(_("Traditional Chinese (EUC-TW)"),  CS_EUC_TW);
	SET_MENUITEM(_("Chinese (ISO-2022-CN)"),	 CS_ISO_2022_CN);
#endif /* 0 */
	SET_MENUITEM(_("Korean (EUC-KR)"),		 CS_EUC_KR);
#endif /* HAVE_LIBJCONV */

	gtk_option_menu_set_menu (GTK_OPTION_MENU (optmenu), optmenu_menu);

	send.checkbtn_sendext = checkbtn_sendext;
	send.entry_sendext    = entry_sendext;
	send.button_sendext   = button_sendext;

	send.checkbtn_savemsg  = checkbtn_savemsg;
	send.checkbtn_queuemsg = checkbtn_queuemsg;

	send.optmenu_charset = optmenu;
}

static void prefs_compose_create(void)
{
	GtkWidget *vbox1;
	GtkWidget *frame_quote;
	GtkWidget *vbox_quote;
	GtkWidget *checkbtn_quote;
	GtkWidget *hbox1;
	GtkWidget *label_quotemark;
	GtkWidget *entry_quotemark;
	GtkWidget *hbox2;
	GtkWidget *label_quotefmt;
	GtkWidget *btn_quotedesc;
	GtkWidget *scrolledwin_quotefmt;
	GtkWidget *text_quotefmt;

	GtkWidget *frame_sig;
	GtkWidget *vbox_sig;
	GtkWidget *checkbtn_autosig;
	GtkWidget *label_sigsep;
	GtkWidget *entry_sigsep;

	GtkWidget *vbox_linewrap;
	GtkWidget *hbox3;
	GtkWidget *hbox4;
	GtkWidget *label_linewrap;
	GtkObject *spinbtn_linewrap_adj;
	GtkWidget *spinbtn_linewrap;
	GtkWidget *checkbtn_wrapquote;
	GtkWidget *checkbtn_wrapatsend;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (dialog.notebook), vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	PACK_FRAME(vbox1, frame_quote, _("Quotation"));

	vbox_quote = gtk_vbox_new (FALSE, VSPACING_NARROW);
	gtk_widget_show (vbox_quote);
	gtk_container_add (GTK_CONTAINER (frame_quote), vbox_quote);
	gtk_container_set_border_width (GTK_CONTAINER (vbox_quote), 8);

	PACK_CHECK_BUTTON (vbox_quote, checkbtn_quote,
			   _("Quote message when replying"));

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox_quote), hbox1, TRUE, TRUE, 0);

	label_quotemark = gtk_label_new (_("Quotation mark"));
	gtk_widget_show (label_quotemark);
	gtk_box_pack_start (GTK_BOX (hbox1), label_quotemark, FALSE, FALSE, 0);

	entry_quotemark = gtk_entry_new ();
	gtk_widget_show (entry_quotemark);
	gtk_box_pack_start (GTK_BOX (hbox1), entry_quotemark, FALSE, FALSE, 0);
	gtk_widget_set_usize (entry_quotemark, 64, -1);

	hbox2 = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox2);
	gtk_box_pack_start (GTK_BOX (vbox_quote), hbox2, TRUE, TRUE, 0);

	label_quotefmt = gtk_label_new (_("Quotation format:"));
	gtk_widget_show (label_quotefmt);
	gtk_box_pack_start (GTK_BOX (hbox2), label_quotefmt, FALSE, FALSE, 0);

	btn_quotedesc =
		gtk_button_new_with_label (_(" Description of symbols "));
	gtk_widget_show (btn_quotedesc);
	gtk_box_pack_end (GTK_BOX (hbox2), btn_quotedesc, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(btn_quotedesc), "clicked",
			   GTK_SIGNAL_FUNC(prefs_quote_description), NULL);

	scrolledwin_quotefmt = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (scrolledwin_quotefmt);
	gtk_box_pack_start (GTK_BOX (vbox_quote), scrolledwin_quotefmt, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy
		(GTK_SCROLLED_WINDOW (scrolledwin_quotefmt),
		 GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);

	text_quotefmt = gtk_text_new (NULL, NULL);
	gtk_widget_show (text_quotefmt);
	gtk_container_add(GTK_CONTAINER(scrolledwin_quotefmt), text_quotefmt);
	gtk_text_set_editable (GTK_TEXT (text_quotefmt), TRUE);
	gtk_widget_set_usize(text_quotefmt, -1, 60);

	PACK_FRAME(vbox1, frame_sig, _("Signature"));

	vbox_sig = gtk_vbox_new (FALSE, VSPACING_NARROW);
	gtk_widget_show (vbox_sig);
	gtk_container_add (GTK_CONTAINER (frame_sig), vbox_sig);
	gtk_container_set_border_width (GTK_CONTAINER (vbox_sig), 8);

	PACK_CHECK_BUTTON (vbox_sig, checkbtn_autosig,
			   _("Insert signature automatically"));

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox_sig), hbox1, TRUE, TRUE, 0);

	label_sigsep = gtk_label_new (_("Signature separator"));
	gtk_widget_show (label_sigsep);
	gtk_box_pack_start (GTK_BOX (hbox1), label_sigsep, FALSE, FALSE, 0);

	entry_sigsep = gtk_entry_new ();
	gtk_widget_show (entry_sigsep);
	gtk_box_pack_start (GTK_BOX (hbox1), entry_sigsep, FALSE, FALSE, 0);
	gtk_widget_set_usize (entry_sigsep, 64, -1);

	/* line-wrapping */
	vbox_linewrap = gtk_vbox_new (FALSE, VSPACING_NARROW);
	gtk_widget_show (vbox_linewrap);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox_linewrap, FALSE, FALSE, 0);

	hbox3 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox3);
	gtk_box_pack_start (GTK_BOX (vbox_linewrap), hbox3, FALSE, FALSE, 0);

	label_linewrap = gtk_label_new (_("Wrap messages at"));
	gtk_widget_show (label_linewrap);
	gtk_box_pack_start (GTK_BOX (hbox3), label_linewrap, FALSE, FALSE, 0);

	spinbtn_linewrap_adj = gtk_adjustment_new (72, 20, 1024, 1, 10, 10);
	spinbtn_linewrap = gtk_spin_button_new
		(GTK_ADJUSTMENT (spinbtn_linewrap_adj), 1, 0);
	gtk_widget_show (spinbtn_linewrap);
	gtk_box_pack_start (GTK_BOX (hbox3), spinbtn_linewrap, FALSE, FALSE, 0);
	gtk_widget_set_usize (spinbtn_linewrap, 64, -1);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbtn_linewrap), TRUE);

	label_linewrap = gtk_label_new (_("characters"));
	gtk_widget_show (label_linewrap);
	gtk_box_pack_start (GTK_BOX (hbox3), label_linewrap, FALSE, FALSE, 0);

	hbox4 = gtk_hbox_new (FALSE, 32);
	gtk_widget_show (hbox4);
	gtk_box_pack_start (GTK_BOX (vbox_linewrap), hbox4, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON (hbox4, checkbtn_wrapquote, _("Wrap quotation"));
	PACK_CHECK_BUTTON
		(hbox4, checkbtn_wrapatsend, _("Wrap before sending"));

	compose.checkbtn_quote   = checkbtn_quote;
	compose.entry_quotemark  = entry_quotemark;
	compose.text_quotefmt    = text_quotefmt;
	compose.checkbtn_autosig = checkbtn_autosig;
	compose.entry_sigsep     = entry_sigsep;

	compose.spinbtn_linewrap     = spinbtn_linewrap;
	compose.spinbtn_linewrap_adj = spinbtn_linewrap_adj;
	compose.checkbtn_wrapquote   = checkbtn_wrapquote;
	compose.checkbtn_wrapatsend  = checkbtn_wrapatsend;
}


/* alfons - nice ui for darko */

static void date_format_close_btn_clicked(GtkButton *button, GtkWidget **widget)
{
	gchar *text;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(*widget != NULL);
	g_return_if_fail(entry_datefmt != NULL);
	g_return_if_fail(datefmt_sample != NULL);

	text = gtk_editable_get_chars(GTK_EDITABLE(datefmt_sample), 0, -1);
	g_free(prefs_common.date_format);
	prefs_common.date_format = text;
	gtk_entry_set_text(GTK_ENTRY(entry_datefmt), text);

	gtk_widget_destroy(*widget);
	*widget = NULL;
}

static gboolean date_format_on_delete(GtkWidget *dialogwidget, gpointer d1, GtkWidget **widget)
{
	g_return_if_fail(widget != NULL);
	g_return_if_fail(*widget != NULL);
	*widget = NULL;
	return FALSE;
}

static void date_format_entry_on_change(GtkEditable *editable, GtkLabel *example)
{
	time_t cur_time;
	struct tm *cal_time;
	char buffer[100];
	char *text;
	cur_time = time(NULL);
	cal_time = localtime(&cur_time);
	buffer[0] = 0;
	text = gtk_editable_get_chars(editable, 0, -1);
	if (text) {
		strftime(buffer, sizeof buffer, text, cal_time); 
	}
	gtk_label_set_text(example, buffer);
}

static GtkWidget *create_date_format(GtkButton *button, void *data)
{
	static GtkWidget *date_format = NULL;
	GtkWidget      *vbox1;
	GtkWidget      *scrolledwindow1;
	GtkWidget      *date_format_list;
	GtkWidget      *label3;
	GtkWidget      *label4;
	GtkWidget      *table2;
	GtkWidget      *vbox2;
	GtkWidget      *vbox3;
	GtkWidget      *hbox2;
	GtkWidget      *label5;
	GtkWidget      *hbox1;
	GtkWidget      *label6;
	GtkWidget      *label7;
	GtkWidget      *hbox3;
	GtkWidget      *button1;

	const struct  {
		gchar *fmt;
		gchar *txt;
	} time_format[] = {
		{ "%a", _("the full abbreviated weekday name") },
		{ "%A", _("the full weekday name") },
		{ "%b", _("the abbreviated month name") },
		{ "%B", _("the full month name") },
		{ "%c", _("the preferred date and time for the current locale") },
		{ "%C", _("the century number (year/100)") },
		{ "%d", _("the day of the month as a decimal number") },
		{ "%H", _("the hour as a decimal number using a 24-hour clock") },
		{ "%I", _("the hour as a decimal number using a 12-hour clock") },
		{ "%j", _("the day of the year as a decimal number") },
		{ "%m", _("the month as a decimal number") },
		{ "%M", _("the minute as a decimal number") },
		{ "%p", _("either AM or PM") },
		{ "%S", _("the second as a decimal number") },
		{ "%w", _("the day of the week as a decimal number") },
		{ "%x", _("the preferred date for the current locale") },
		{ "%y", _("the last two digits of a year") },
		{ "%Y", _("the year as a decimal number") },
		{ "%Z", _("the time zone or name or abbreviation") }
	};
	int tmp;
	const int TIME_FORMAT_ELEMS = sizeof time_format / sizeof time_format[0];

	if (date_format) return date_format;

	date_format = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_window_set_title(GTK_WINDOW(date_format), _("Date format"));
	gtk_window_set_position(GTK_WINDOW(date_format), GTK_WIN_POS_CENTER);
	gtk_window_set_default_size(GTK_WINDOW(date_format), 440, 200);

	vbox1 = gtk_vbox_new(FALSE, 10);
	gtk_widget_show(vbox1);
	gtk_container_add(GTK_CONTAINER(date_format), vbox1);

	scrolledwindow1 = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(scrolledwindow1);
	gtk_box_pack_start(GTK_BOX(vbox1), scrolledwindow1, TRUE, TRUE, 0);

	date_format_list = gtk_clist_new(2);
	gtk_widget_show(date_format_list);
	gtk_container_add(GTK_CONTAINER(scrolledwindow1), date_format_list);
	gtk_clist_set_column_width(GTK_CLIST(date_format_list), 0, 80);
	gtk_clist_set_column_width(GTK_CLIST(date_format_list), 1, 80);
	gtk_clist_column_titles_show(GTK_CLIST(date_format_list));

	label3 = gtk_label_new(_("Date Format"));
	gtk_widget_show(label3);
	gtk_clist_set_column_widget(GTK_CLIST(date_format_list), 0, label3);

	label4 = gtk_label_new(_("Date Format Description"));
	gtk_widget_show(label4);
	gtk_clist_set_column_widget(GTK_CLIST(date_format_list), 1, label4);

	for (tmp = 0; tmp < TIME_FORMAT_ELEMS; tmp++) {
		gchar *text[3];
		/* phoney casting necessary because of gtk... */
		text[0] = (gchar *) time_format[tmp].fmt;
		text[1] = (gchar *) time_format[tmp].txt;
		text[2] = NULL;
		gtk_clist_append(GTK_CLIST(date_format_list), text);
	}

	table2 = gtk_table_new(1, 1, TRUE);
	gtk_widget_show(table2);
	gtk_box_pack_start(GTK_BOX(vbox1), table2, FALSE, TRUE, 0);

	vbox2 = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(vbox2);
	gtk_table_attach(GTK_TABLE(table2), vbox2, 0, 1, 0, 1,
					 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);

	vbox3 = gtk_vbox_new(TRUE, 4);
	gtk_widget_show(vbox3);
	gtk_box_pack_end(GTK_BOX(vbox2), vbox3, FALSE, FALSE, 10);

	hbox2 = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(hbox2);
	gtk_box_pack_start(GTK_BOX(vbox3), hbox2, TRUE, TRUE, 0);

	label5 = gtk_label_new(_("Date format"));
	gtk_widget_show(label5);
	gtk_box_pack_start(GTK_BOX(hbox2), label5, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label5), 8, 0);

	datefmt_sample = gtk_entry_new_with_max_length(300);
	gtk_widget_show(datefmt_sample);
	gtk_box_pack_start(GTK_BOX(hbox2), datefmt_sample, TRUE, TRUE, 40);

	hbox1 = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(hbox1);
	gtk_box_pack_start(GTK_BOX(vbox3), hbox1, TRUE, TRUE, 0);

	label6 = gtk_label_new(_("Example"));
	gtk_widget_show(label6);
	gtk_box_pack_start(GTK_BOX(hbox1), label6, FALSE, TRUE, 0);
	gtk_misc_set_padding(GTK_MISC(label6), 8, 0);

	label7 = gtk_label_new(_("label7"));
	gtk_widget_show(label7);
	gtk_box_pack_start(GTK_BOX(hbox1), label7, TRUE, TRUE, 60);
	gtk_label_set_justify(GTK_LABEL(label7), GTK_JUSTIFY_LEFT);

	hbox3 = gtk_hbox_new(TRUE, 0);
	gtk_widget_show(hbox3);
	gtk_box_pack_end(GTK_BOX(vbox3), hbox3, FALSE, FALSE, 0);

	button1 = gtk_button_new_with_label(_("Close"));
	gtk_widget_show(button1);
	gtk_box_pack_start(GTK_BOX(hbox3), button1, FALSE, TRUE, 144);

	/* set the current format */
	gtk_entry_set_text(GTK_ENTRY(datefmt_sample), prefs_common.date_format);
	date_format_entry_on_change(GTK_EDITABLE(datefmt_sample),
				    GTK_LABEL(label7));

	gtk_signal_connect(GTK_OBJECT(button1), "clicked",
				  GTK_SIGNAL_FUNC(date_format_close_btn_clicked), &date_format);
				  
	gtk_signal_connect(GTK_OBJECT(date_format), "delete_event",
			    GTK_SIGNAL_FUNC(date_format_on_delete), &date_format);

	gtk_signal_connect(GTK_OBJECT(datefmt_sample), "changed",
				GTK_SIGNAL_FUNC(date_format_entry_on_change), label7);
				  
	gtk_window_set_position(GTK_WINDOW(date_format), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(date_format), TRUE);

	gtk_widget_show(date_format);					
	return date_format;
}

static void prefs_display_create(void)
{
	GtkWidget *vbox1;
	GtkWidget *frame_font;
	GtkWidget *table1;
	GtkWidget *label_textfont;
	GtkWidget *entry_textfont;
	GtkWidget *button_textfont;
	GtkWidget *chkbtn_folder_unread;
	GtkWidget *chkbtn_transhdr;
	GtkWidget *frame_summary;
	GtkWidget *vbox2;
	GtkWidget *chkbtn_swapfrom;
	GtkWidget *chkbtn_hscrollbar;
	GtkWidget *hbox1;
	GtkWidget *label_datefmt;
	GtkWidget *button_dispitem;
	GtkWidget *button_headers_display;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (dialog.notebook), vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	PACK_FRAME(vbox1, frame_font, _("Font"));

	table1 = gtk_table_new (1, 3, FALSE);
	gtk_widget_show (table1);
	gtk_container_add (GTK_CONTAINER (frame_font), table1);
	gtk_container_set_border_width (GTK_CONTAINER (table1), 8);
	gtk_table_set_row_spacings (GTK_TABLE (table1), 8);
	gtk_table_set_col_spacings (GTK_TABLE (table1), 8);

	label_textfont = gtk_label_new (_("Text"));
	gtk_widget_show (label_textfont);
	gtk_table_attach (GTK_TABLE (table1), label_textfont, 0, 1, 0, 1,
			  GTK_FILL, (GTK_EXPAND | GTK_FILL), 0, 0);

	entry_textfont = gtk_entry_new ();
	gtk_widget_show (entry_textfont);
	gtk_table_attach (GTK_TABLE (table1), entry_textfont, 1, 2, 0, 1,
			  (GTK_EXPAND | GTK_FILL), 0, 0, 0);

	button_textfont = gtk_button_new_with_label ("... ");
	gtk_widget_show (button_textfont);
	gtk_table_attach (GTK_TABLE (table1), button_textfont, 2, 3, 0, 1,
			  0, 0, 0, 0);
	gtk_signal_connect (GTK_OBJECT (button_textfont), "clicked",
			    GTK_SIGNAL_FUNC (prefs_font_select), NULL);

	vbox2 = gtk_vbox_new (FALSE, VSPACING_NARROW);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, TRUE, 0);

	PACK_CHECK_BUTTON
		(vbox2, chkbtn_transhdr,
		 _("Translate header name (such as `From:', `Subject:')"));

	PACK_CHECK_BUTTON (vbox2, chkbtn_folder_unread,
			   _("Display unread number next to folder name"));

	/* ---- Summary ---- */

	PACK_FRAME(vbox1, frame_summary, _("Summary View"));

	vbox2 = gtk_vbox_new (FALSE, VSPACING_NARROW);
	gtk_widget_show (vbox2);
	gtk_container_add (GTK_CONTAINER (frame_summary), vbox2);
	gtk_container_set_border_width (GTK_CONTAINER (vbox2), 8);

	PACK_CHECK_BUTTON
		(vbox2, chkbtn_swapfrom,
		 _("Display recipient on `From' column if sender is yourself"));
	PACK_CHECK_BUTTON
		(vbox2, chkbtn_hscrollbar, _("Enable horizontal scroll bar"));

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, TRUE, 0);

	label_datefmt = gtk_button_new_with_label (_("Date format"));
	gtk_widget_show (label_datefmt);
	gtk_box_pack_start (GTK_BOX (hbox1), label_datefmt, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(label_datefmt), "clicked",
				  GTK_SIGNAL_FUNC(create_date_format), NULL);

	entry_datefmt = gtk_entry_new ();
	gtk_widget_show (entry_datefmt);
	gtk_box_pack_start (GTK_BOX (hbox1), entry_datefmt, TRUE, TRUE, 0);
	
	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, TRUE, 0);

	button_dispitem = gtk_button_new_with_label
		(_(" Set display item of summary... "));
	gtk_widget_show (button_dispitem);
	gtk_box_pack_start (GTK_BOX (hbox1), button_dispitem, FALSE, TRUE, 0);
	gtk_signal_connect (GTK_OBJECT (button_dispitem), "clicked",
			    GTK_SIGNAL_FUNC (prefs_summary_display_item_set),
			    NULL);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, TRUE, 0);

	button_headers_display = gtk_button_new_with_label
		(_(" Set displaying of headers... "));
	gtk_widget_show (button_headers_display);
	gtk_box_pack_start (GTK_BOX (hbox1), button_headers_display, FALSE,
			    TRUE, 0);
	gtk_signal_connect (GTK_OBJECT (button_headers_display), "clicked",
			    GTK_SIGNAL_FUNC (prefs_display_headers_open),
			    NULL);

	display.entry_textfont	= entry_textfont;
	display.button_textfont	= button_textfont;

	display.chkbtn_folder_unread = chkbtn_folder_unread;
	display.chkbtn_transhdr   = chkbtn_transhdr;

	display.chkbtn_swapfrom   = chkbtn_swapfrom;
	display.chkbtn_hscrollbar = chkbtn_hscrollbar;
}

static void prefs_message_create(void)
{
	GtkWidget *vbox1;
	GtkWidget *vbox2;
	GtkWidget *hbox1;
	GtkWidget *chkbtn_enablecol;
	GtkWidget *button_edit_col;
	GtkWidget *chkbtn_mbalnum;
	GtkWidget *chkbtn_disphdrpane;
	GtkWidget *chkbtn_disphdr;
	GtkWidget *hbox_linespc;
	GtkWidget *label_linespc;
	GtkObject *spinbtn_linespc_adj;
	GtkWidget *spinbtn_linespc;
	GtkWidget *chkbtn_headspc;

	GtkWidget *frame_scr;
	GtkWidget *vbox_scr;
	GtkWidget *chkbtn_smoothscroll;
	GtkWidget *hbox_scr;
	GtkWidget *label_scr;
	GtkObject *spinbtn_scrollstep_adj;
	GtkWidget *spinbtn_scrollstep;
	GtkWidget *chkbtn_halfpage;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (dialog.notebook), vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, TRUE, 0);

	PACK_CHECK_BUTTON (hbox1, chkbtn_enablecol,
			   _("Enable coloration of message"));
	gtk_signal_connect (GTK_OBJECT (chkbtn_enablecol), "toggled",
						GTK_SIGNAL_FUNC (prefs_enable_message_color_toggled),
						NULL);

	button_edit_col = gtk_button_new_with_label (_(" Edit... "));
	gtk_widget_show (button_edit_col);
	gtk_box_pack_end (GTK_BOX (hbox1), button_edit_col, FALSE, TRUE, 0);
	gtk_signal_connect (GTK_OBJECT (button_edit_col), "clicked",
			    GTK_SIGNAL_FUNC (prefs_quote_colors_dialog), NULL);

	SET_TOGGLE_SENSITIVITY(chkbtn_enablecol, button_edit_col);

	vbox2 = gtk_vbox_new (FALSE, VSPACING_NARROW);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON
		(vbox2, chkbtn_mbalnum,
		 _("Display 2-byte alphabet and numeric with 1-byte character"));
	PACK_CHECK_BUTTON(vbox2, chkbtn_disphdrpane,
			  _("Display header pane above message view"));
	PACK_CHECK_BUTTON(vbox2, chkbtn_disphdr,
			  _("Display short headers on message view"));

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
	gtk_widget_set_usize (spinbtn_linespc, 64, -1);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbtn_linespc), TRUE);

	label_linespc = gtk_label_new (_("pixel(s)"));
	gtk_widget_show (label_linespc);
	gtk_box_pack_start (GTK_BOX (hbox_linespc), label_linespc,
			    FALSE, FALSE, 0);

	PACK_CHECK_BUTTON(hbox1, chkbtn_headspc, _("Leave space on head"));

	PACK_FRAME(vbox1, frame_scr, _("Scroll"));

	vbox_scr = gtk_vbox_new (FALSE, VSPACING_NARROW);
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
	gtk_widget_set_usize (spinbtn_scrollstep, 64, -1);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbtn_scrollstep),
				     TRUE);

	label_scr = gtk_label_new (_("pixel(s)"));
	gtk_widget_show (label_scr);
	gtk_box_pack_start (GTK_BOX (hbox_scr), label_scr, FALSE, FALSE, 0);

	SET_TOGGLE_SENSITIVITY (chkbtn_smoothscroll, hbox_scr)

	message.chkbtn_enablecol   = chkbtn_enablecol;
	message.button_edit_col    = button_edit_col;
	message.chkbtn_mbalnum     = chkbtn_mbalnum;
	message.chkbtn_disphdrpane = chkbtn_disphdrpane;
	message.chkbtn_disphdr     = chkbtn_disphdr;
	message.spinbtn_linespc    = spinbtn_linespc;
	message.chkbtn_headspc     = chkbtn_headspc;

	message.chkbtn_smoothscroll    = chkbtn_smoothscroll;
	message.spinbtn_scrollstep     = spinbtn_scrollstep;
	message.spinbtn_scrollstep_adj = spinbtn_scrollstep_adj;
	message.chkbtn_halfpage        = chkbtn_halfpage;
}

#if USE_GPGME
static void prefs_privacy_create(void)
{
	GtkWidget *vbox1;
	GtkWidget *vbox2;
	GtkWidget *hbox1;
	GtkWidget *checkbtn_gpgme_warning;
	GtkWidget *checkbtn_default_encrypt;
	GtkWidget *checkbtn_default_sign;
	GtkWidget *checkbtn_auto_check_signatures;
	GtkWidget *checkbtn_passphrase_grab;
	GtkWidget *label;
	GtkWidget *menuitem;
	GtkWidget *optmenu;
	GtkWidget *optmenu_menu;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (dialog.notebook), vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	vbox2 = gtk_vbox_new (FALSE, VSPACING_NARROW);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON
		(vbox2, checkbtn_gpgme_warning,
		 _("Display warning on startup if GnuPG does not work"));

	PACK_CHECK_BUTTON (vbox2, checkbtn_default_encrypt,
			   _("Encrypt message by default"));

	PACK_CHECK_BUTTON (vbox2, checkbtn_default_sign,
			   _("Sign message by default"));

	PACK_CHECK_BUTTON (vbox2, checkbtn_auto_check_signatures,
			   _("Automatically check signatures"));

#ifndef __MINGW32__
	PACK_CHECK_BUTTON (vbox2, checkbtn_passphrase_grab,
			   _("Grab input while entering a passphrase"));
#endif

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);

	/* create default signkey box */
	label = gtk_label_new (_("Default Sign Key"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox1), label, FALSE, FALSE, 0);
	optmenu = gtk_option_menu_new ();
	gtk_widget_show (optmenu);
	gtk_box_pack_start(GTK_BOX (hbox1), optmenu, FALSE, FALSE, 0);
	optmenu_menu = gtk_menu_new ();

	MENUITEM_ADD(optmenu_menu, menuitem, "Default Key", "def_key");
	MENUITEM_ADD(optmenu_menu, menuitem, "Second Key", "2nd_key");
	gtk_option_menu_set_menu (GTK_OPTION_MENU (optmenu), optmenu_menu);
	/* FIXME: disabled because not implemented */
	gtk_widget_set_sensitive(optmenu, FALSE);

	privacy.checkbtn_gpgme_warning   = checkbtn_gpgme_warning;
	privacy.checkbtn_default_encrypt = checkbtn_default_encrypt;
	privacy.checkbtn_default_sign    = checkbtn_default_sign;
	privacy.checkbtn_auto_check_signatures
					 = checkbtn_auto_check_signatures;
	privacy.checkbtn_passphrase_grab = checkbtn_passphrase_grab;
	privacy.optmenu_default_signkey  = optmenu;
}

static void
prefs_common_default_signkey_set_data_from_optmenu(PrefParam *pparam)
{
#if 0
	GtkWidget *menu;
	GtkWidget *menuitem;
	gchar *charset;

	menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(*pparam->widget));
	menuitem = gtk_menu_get_active(GTK_MENU(menu));
	charset = gtk_object_get_user_data(GTK_OBJECT(menuitem));
	g_free(*((gchar **)pparam->data));
	*((gchar **)pparam->data) = g_strdup(charset);
#endif
}

static void prefs_common_default_signkey_set_optmenu(PrefParam *pparam)
{
#if 0
	GList *cur;
	GtkOptionMenu *optmenu = GTK_OPTION_MENU(*pparam->widget);
	GtkWidget *menu;
	GtkWidget *menuitem;
	gchar *charset;
	gint n = 0;

	g_return_if_fail(optmenu != NULL);
	g_return_if_fail(*((gchar **)pparam->data) != NULL);

	menu = gtk_option_menu_get_menu(optmenu);
	for (cur = GTK_MENU_SHELL(menu)->children;
	     cur != NULL; cur = cur->next) {
		menuitem = GTK_WIDGET(cur->data);
		charset = gtk_object_get_user_data(GTK_OBJECT(menuitem));
		if (!strcmp(charset, *((gchar **)pparam->data))) {
			gtk_option_menu_set_history(optmenu, n);
			return;
		}
		n++;
	}

	gtk_option_menu_set_history(optmenu, 0);
	prefs_common_charset_set_data_from_optmenu(pparam);
#endif
}
#endif /* USE_GPGME */

static void prefs_interface_create(void)
{
	GtkWidget *vbox1;
	GtkWidget *vbox2;
	GtkWidget *vbox3;
	GtkWidget *checkbtn_emacs;
	GtkWidget *checkbtn_openunread;
	GtkWidget *checkbtn_openinbox;
	GtkWidget *checkbtn_immedexec;
	GtkWidget *checkbtn_addaddrbyclick;
	GtkWidget *label;

	GtkWidget *frame_exit;
	GtkWidget *vbox_exit;
	GtkWidget *hbox1;
	GtkWidget *checkbtn_confonexit;
	GtkWidget *checkbtn_cleanonexit;
	GtkWidget *checkbtn_askonclean;
	GtkWidget *checkbtn_warnqueued;
	GtkWidget *checkbtn_returnreceipt;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (dialog.notebook), vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	vbox2 = gtk_vbox_new (FALSE, VSPACING_NARROW);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON (vbox2, checkbtn_emacs,
			   _("Emulate the behavior of mouse operation of\n"
			     "Emacs-based mailer"));
	gtk_label_set_justify (GTK_LABEL (GTK_BIN (checkbtn_emacs)->child),
			       GTK_JUSTIFY_LEFT);

	PACK_CHECK_BUTTON
		(vbox2, checkbtn_openunread,
		 _("Open first unread message when entering a folder"));

	PACK_CHECK_BUTTON
		(vbox2, checkbtn_openinbox,
		 _("Go to inbox after receiving new mail"));

	vbox3 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox3);
	gtk_box_pack_start (GTK_BOX (vbox2), vbox3, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON
		(vbox3, checkbtn_immedexec,
		 _("Execute immediately when moving or deleting messages"));

	hbox1 = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox3), hbox1, FALSE, FALSE, 0);

	label = gtk_label_new
		(_("(Messages will be just marked till execution\n"
		   " if this is turned off)"));
	gtk_box_pack_start (GTK_BOX (hbox1), label, FALSE, FALSE, 8);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);

	PACK_CHECK_BUTTON
		(vbox2, checkbtn_addaddrbyclick,
		 _("Add address to destination when double-clicked"));

	PACK_FRAME (vbox1, frame_exit, _("On exit"));

	vbox_exit = gtk_vbox_new (FALSE, VSPACING_NARROW);
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

	PACK_CHECK_BUTTON (vbox_exit, checkbtn_returnreceipt,
			   _("Send return receipt on request"));

	interface.checkbtn_emacs          = checkbtn_emacs;
	interface.checkbtn_openunread     = checkbtn_openunread;
	interface.checkbtn_openinbox      = checkbtn_openinbox;
	interface.checkbtn_immedexec      = checkbtn_immedexec;
	interface.checkbtn_addaddrbyclick = checkbtn_addaddrbyclick;
	interface.checkbtn_confonexit     = checkbtn_confonexit;
	interface.checkbtn_cleanonexit    = checkbtn_cleanonexit;
	interface.checkbtn_askonclean     = checkbtn_askonclean;
	interface.checkbtn_warnqueued     = checkbtn_warnqueued;
	interface.checkbtn_returnreceipt  = checkbtn_returnreceipt;
}

static void prefs_other_create(void)
{
	GtkWidget *vbox1;
	GtkWidget *hbox1;

	GtkWidget *uri_frame;
	GtkWidget *uri_label;
	GtkWidget *uri_combo;
	GtkWidget *uri_entry;

	GtkWidget *print_frame;
	GtkWidget *printcmd_label;
	GtkWidget *printcmd_entry;

	GtkWidget *exteditor_frame;
	GtkWidget *exteditor_label;
	GtkWidget *exteditor_combo;
	GtkWidget *exteditor_entry;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (dialog.notebook), vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	PACK_FRAME(vbox1, uri_frame,
		   _("External Web browser (%s will be replaced with URI)"));

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_container_add (GTK_CONTAINER (uri_frame), hbox1);
	gtk_container_set_border_width (GTK_CONTAINER (hbox1), 8);

	uri_label = gtk_label_new (_("Command"));
	gtk_widget_show(uri_label);
	gtk_box_pack_start (GTK_BOX (hbox1), uri_label, FALSE, TRUE, 0);

	uri_combo = gtk_combo_new ();
	gtk_widget_show (uri_combo);
	gtk_box_pack_start (GTK_BOX (hbox1), uri_combo, TRUE, TRUE, 0);
	gtkut_combo_set_items (GTK_COMBO (uri_combo),
			       "netscape -remote 'openURL(%s,raise)'",
			       "gnome-moz-remote --raise --newwin '%s'",
			       "kterm -e w3m '%s'",
			       "kterm -e lynx '%s'",
			       NULL);

	uri_entry = GTK_COMBO (uri_combo)->entry;

	PACK_FRAME(vbox1, print_frame,
		   _("Printing (%s will be replaced with file name)"));

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_container_add (GTK_CONTAINER (print_frame), hbox1);
	gtk_container_set_border_width (GTK_CONTAINER (hbox1), 8);

	printcmd_label = gtk_label_new (_("Command"));
	gtk_widget_show (printcmd_label);
	gtk_box_pack_start (GTK_BOX (hbox1), printcmd_label, FALSE, FALSE, 0);

	printcmd_entry = gtk_entry_new ();
	gtk_widget_show (printcmd_entry);
	gtk_box_pack_start (GTK_BOX (hbox1), printcmd_entry, TRUE, TRUE, 0);

	PACK_FRAME(vbox1, exteditor_frame,
		   _("External editor (%s will be replaced with file name)"));

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_container_add (GTK_CONTAINER (exteditor_frame), hbox1);
	gtk_container_set_border_width (GTK_CONTAINER (hbox1), 8);

	exteditor_label = gtk_label_new (_("Command"));
	gtk_widget_show (exteditor_label);
	gtk_box_pack_start (GTK_BOX (hbox1), exteditor_label, FALSE, FALSE, 0);

	exteditor_combo = gtk_combo_new ();
	gtk_widget_show (exteditor_combo);
	gtk_box_pack_start (GTK_BOX (hbox1), exteditor_combo, TRUE, TRUE, 0);
	gtkut_combo_set_items (GTK_COMBO (exteditor_combo),
			       "gedit %s",
			       "mgedit --no-fork %s",
			       "emacs %s",
			       "xemacs %s",
			       "kterm -e jed %s",
			       "kterm -e vi %s",
			       NULL);
	exteditor_entry = GTK_COMBO (exteditor_combo)->entry;

	other.uri_combo = uri_combo;
	other.uri_entry = uri_entry;
	other.printcmd_entry = printcmd_entry;

	other.exteditor_combo = exteditor_combo;
	other.exteditor_entry = exteditor_entry;
}

void prefs_quote_colors_dialog(void)
{
	if (!quote_color_win)
		prefs_quote_colors_dialog_create();
	gtk_widget_show(quote_color_win);

	gtk_main();
	gtk_widget_hide(quote_color_win);

	textview_update_message_colors();
	main_window_reflect_prefs_all();
}

static void prefs_quote_colors_dialog_create(void)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *table;
	GtkWidget *quotelevel1_label;
	GtkWidget *quotelevel2_label;
	GtkWidget *quotelevel3_label;
	GtkWidget *uri_label;
	GtkWidget *hbbox;
	GtkWidget *ok_btn;
	//GtkWidget *cancel_btn;
	GtkWidget *recycle_colors_btn;
	GtkWidget *frame_colors;

	window = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_container_set_border_width(GTK_CONTAINER(window), 2);
	gtk_window_set_title(GTK_WINDOW(window), _("Set message colors"));
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	gtk_window_set_policy(GTK_WINDOW(window), FALSE, TRUE, FALSE);

	vbox = gtk_vbox_new (FALSE, VSPACING);
	gtk_container_add (GTK_CONTAINER (window), vbox);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 8);
	PACK_FRAME(vbox, frame_colors, _("Colors"));

	table = gtk_table_new (4, 2, FALSE);
	gtk_container_add (GTK_CONTAINER (frame_colors), table);
	gtk_container_set_border_width (GTK_CONTAINER (table), 8);
	gtk_table_set_row_spacings (GTK_TABLE (table), 2);
	gtk_table_set_col_spacings (GTK_TABLE (table), 4);

/*	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0); */

	color_buttons.quote_level1_btn = gtk_button_new();
	gtk_table_attach(GTK_TABLE (table), color_buttons.quote_level1_btn, 0, 1, 0, 1,
					(GtkAttachOptions) (0),
					(GtkAttachOptions) (0), 0, 0);
	gtk_widget_set_usize (color_buttons.quote_level1_btn, 40, 30);
	gtk_container_set_border_width (GTK_CONTAINER (color_buttons.quote_level1_btn), 5);

	color_buttons.quote_level2_btn = gtk_button_new();
	gtk_table_attach(GTK_TABLE (table), color_buttons.quote_level2_btn, 0, 1, 1, 2,
					(GtkAttachOptions) (0),
					(GtkAttachOptions) (0), 0, 0);
	gtk_widget_set_usize (color_buttons.quote_level2_btn, 40, 30);
	gtk_container_set_border_width (GTK_CONTAINER (color_buttons.quote_level2_btn), 5);

	color_buttons.quote_level3_btn = gtk_button_new_with_label ("");
	gtk_table_attach(GTK_TABLE (table), color_buttons.quote_level3_btn, 0, 1, 2, 3,
					(GtkAttachOptions) (0),
					(GtkAttachOptions) (0), 0, 0);
	gtk_widget_set_usize (color_buttons.quote_level3_btn, 40, 30);
	gtk_container_set_border_width (GTK_CONTAINER (color_buttons.quote_level3_btn), 5);

	color_buttons.uri_btn = gtk_button_new_with_label ("");
	gtk_table_attach(GTK_TABLE (table), color_buttons.uri_btn, 0, 1, 3, 4,
					(GtkAttachOptions) (0),
					(GtkAttachOptions) (0), 0, 0);
	gtk_widget_set_usize (color_buttons.uri_btn, 40, 30);
	gtk_container_set_border_width (GTK_CONTAINER (color_buttons.uri_btn), 5);

	quotelevel1_label = gtk_label_new (_("Quoted Text - First Level"));
	gtk_table_attach(GTK_TABLE (table), quotelevel1_label, 1, 2, 0, 1,
					(GTK_EXPAND | GTK_FILL), 0, 0, 0);
	gtk_label_set_justify (GTK_LABEL (quotelevel1_label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (quotelevel1_label), 0, 0.5);

	quotelevel2_label = gtk_label_new (_("Quoted Text - Second Level"));
	gtk_table_attach(GTK_TABLE (table), quotelevel2_label, 1, 2, 1, 2,
					(GTK_EXPAND | GTK_FILL), 0, 0, 0);
	gtk_label_set_justify (GTK_LABEL (quotelevel2_label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (quotelevel2_label), 0, 0.5);

	quotelevel3_label = gtk_label_new (_("Quoted Text - Third Level"));
	gtk_table_attach(GTK_TABLE (table), quotelevel3_label, 1, 2, 2, 3,
					(GTK_EXPAND | GTK_FILL), 0, 0, 0);
	gtk_label_set_justify (GTK_LABEL (quotelevel3_label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (quotelevel3_label), 0, 0.5);

	uri_label = gtk_label_new (_("URI link"));
	gtk_table_attach(GTK_TABLE (table), uri_label, 1, 2, 3, 4,
					(GTK_EXPAND | GTK_FILL), 0, 0, 0);
	gtk_label_set_justify (GTK_LABEL (uri_label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (uri_label), 0, 0.5);

	PACK_CHECK_BUTTON (vbox, recycle_colors_btn,
			   _("Recycle quote colors"));

	gtkut_button_set_create(&hbbox, &ok_btn, _("OK"),
				NULL, NULL, NULL, NULL);
	gtk_box_pack_end(GTK_BOX(vbox), hbbox, FALSE, FALSE, 0);

	gtk_widget_grab_default(ok_btn);
	gtk_signal_connect(GTK_OBJECT(ok_btn), "clicked",
					GTK_SIGNAL_FUNC(gtk_main_quit), NULL);
	gtk_signal_connect(GTK_OBJECT(window), "delete_event",
					GTK_SIGNAL_FUNC(gtk_main_quit), NULL);

	gtk_signal_connect(GTK_OBJECT(color_buttons.quote_level1_btn), "clicked",
			   GTK_SIGNAL_FUNC(quote_color_set_dialog), "LEVEL1");
	gtk_signal_connect(GTK_OBJECT(color_buttons.quote_level2_btn), "clicked",
			   GTK_SIGNAL_FUNC(quote_color_set_dialog), "LEVEL2");
	gtk_signal_connect(GTK_OBJECT(color_buttons.quote_level3_btn), "clicked",
			   GTK_SIGNAL_FUNC(quote_color_set_dialog), "LEVEL3");
	gtk_signal_connect(GTK_OBJECT(color_buttons.uri_btn), "clicked",
			   GTK_SIGNAL_FUNC(quote_color_set_dialog), "URI");
	gtk_signal_connect(GTK_OBJECT(recycle_colors_btn), "toggled",
			   GTK_SIGNAL_FUNC(prefs_recycle_colors_toggled), NULL);

	/* show message button colors and recycle options */
	set_button_bg_color(color_buttons.quote_level1_btn,
			    prefs_common.quote_level1_col);
	set_button_bg_color(color_buttons.quote_level2_btn,
			    prefs_common.quote_level2_col);
	set_button_bg_color(color_buttons.quote_level3_btn,
			    prefs_common.quote_level3_col);
	set_button_bg_color(color_buttons.uri_btn,
			    prefs_common.uri_col);
	gtk_toggle_button_set_active((GtkToggleButton *)recycle_colors_btn,
				     prefs_common.recycle_quote_colors);

	gtk_widget_show_all(vbox);
	quote_color_win = window;
}

static void quote_color_set_dialog(GtkWidget *widget, gpointer data)
{
	gchar *type = (gchar *)data;
	gchar *title = NULL;
	gdouble color[4] = {0.0, 0.0, 0.0, 0.0};
	gint rgbvalue = 0;
	GtkColorSelectionDialog *dialog;

	if(g_strcasecmp(type, "LEVEL1") == 0) {
		title = _("Pick color for quotation level 1");
		rgbvalue = prefs_common.quote_level1_col;
	} else if(g_strcasecmp(type, "LEVEL2") == 0) {
		title = _("Pick color for quotation level 2");
		rgbvalue = prefs_common.quote_level2_col;
	} else if(g_strcasecmp(type, "LEVEL3") == 0) {
		title = _("Pick color for quotation level 3");
		rgbvalue = prefs_common.quote_level3_col;
	} else if(g_strcasecmp(type, "URI") == 0) {
		title = _("Pick color for URI");
		rgbvalue = prefs_common.uri_col;
	} else {   /* Should never be called */
		fprintf(stderr, "Unrecognized datatype '%s' in quote_color_set_dialog\n", type);
		return;
	}

	color_dialog = gtk_color_selection_dialog_new(title);
	gtk_window_set_position(GTK_WINDOW(color_dialog), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(color_dialog), TRUE);
	gtk_window_set_policy(GTK_WINDOW(color_dialog), FALSE, FALSE, FALSE);

	gtk_signal_connect(GTK_OBJECT(((GtkColorSelectionDialog *)color_dialog)->ok_button),
			   "clicked", GTK_SIGNAL_FUNC(quote_colors_set_dialog_ok), data);
	gtk_signal_connect(GTK_OBJECT(((GtkColorSelectionDialog *)color_dialog)->cancel_button),
			   "clicked", GTK_SIGNAL_FUNC(quote_colors_set_dialog_cancel), data);

	/* preselect the previous color in the color selection dialog */
	color[0] = (gdouble) ((rgbvalue & 0xff0000) >> 16) / 255.0;
	color[1] = (gdouble) ((rgbvalue & 0x00ff00) >>  8) / 255.0;
	color[2] = (gdouble)  (rgbvalue & 0x0000ff)        / 255.0;
	dialog = (GtkColorSelectionDialog *)color_dialog;
	gtk_color_selection_set_color
		((GtkColorSelection *)dialog->colorsel, color);

	gtk_widget_show(color_dialog);
}

static void quote_colors_set_dialog_ok(GtkWidget *widget, gpointer data)
{
	GtkColorSelection *colorsel = (GtkColorSelection *)
						((GtkColorSelectionDialog *)color_dialog)->colorsel;
	gdouble color[4];
	gint red, green, blue, rgbvalue;
	gchar *type = (gchar *)data;

	gtk_color_selection_get_color(colorsel, color);

	red      = (gint) (color[0] * 255.0);
	green    = (gint) (color[1] * 255.0);
	blue     = (gint) (color[2] * 255.0);
	rgbvalue = (gint) ((red * 0x10000) | (green * 0x100) | blue);

#if 0
	fprintf(stderr, "redc = %f, greenc = %f, bluec = %f\n", color[0], color[1], color[2]);
	fprintf(stderr, "red = %d, green = %d, blue = %d\n", red, green, blue);
	fprintf(stderr, "Color is %x\n", rgbvalue);
#endif

	if (g_strcasecmp(type, "LEVEL1") == 0) {
		prefs_common.quote_level1_col = rgbvalue;
		set_button_bg_color(color_buttons.quote_level1_btn, rgbvalue);
	} else if (g_strcasecmp(type, "LEVEL2") == 0) {
		prefs_common.quote_level2_col = rgbvalue;
		set_button_bg_color(color_buttons.quote_level2_btn, rgbvalue);
	} else if (g_strcasecmp(type, "LEVEL3") == 0) {
		prefs_common.quote_level3_col = rgbvalue;
		set_button_bg_color(color_buttons.quote_level3_btn, rgbvalue);
	} else if (g_strcasecmp(type, "URI") == 0) {
		prefs_common.uri_col = rgbvalue;
		set_button_bg_color(color_buttons.uri_btn, rgbvalue);
	} else
		fprintf( stderr, "Unrecognized datatype '%s' in quote_color_set_dialog_ok\n", type );

	gtk_widget_hide(color_dialog);
}

static void quote_colors_set_dialog_cancel(GtkWidget *widget, gpointer data)
{
	gtk_widget_hide(color_dialog);
}

static void set_button_bg_color(GtkWidget *widget, gint rgbvalue)
{
	GtkStyle *newstyle;
	GdkColor color;

	gtkut_convert_int_to_gdk_color(rgbvalue, &color);
	newstyle = gtk_style_copy(gtk_widget_get_default_style());
	newstyle->bg[GTK_STATE_NORMAL]   = color;
	newstyle->bg[GTK_STATE_PRELIGHT] = color;
	newstyle->bg[GTK_STATE_ACTIVE]   = color;

	gtk_widget_set_style(GTK_WIDGET(widget), newstyle);
}

static void prefs_enable_message_color_toggled(void)
{
	gboolean is_active;

	is_active = gtk_toggle_button_get_active
		(GTK_TOGGLE_BUTTON(message.chkbtn_enablecol));
	gtk_widget_set_sensitive(message.button_edit_col, is_active);
	prefs_common.enable_color = is_active;
}

static void prefs_recycle_colors_toggled(GtkWidget *widget)
{
	gboolean is_active;

	is_active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
	prefs_common.recycle_quote_colors = is_active;
}

static void prefs_quote_description(void)
{
	if (!quote_desc_win)
		prefs_quote_description_create();

	gtk_widget_show(quote_desc_win);
	gtk_main();
	gtk_widget_hide(quote_desc_win);
}

static void prefs_quote_description_create(void)
{
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *hbbox;
	GtkWidget *label;
	GtkWidget *ok_btn;

	quote_desc_win = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_window_set_title(GTK_WINDOW(quote_desc_win),
			     _("Description of symbols"));
	gtk_container_set_border_width(GTK_CONTAINER(quote_desc_win), 8);
	gtk_window_set_position(GTK_WINDOW(quote_desc_win), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(quote_desc_win), TRUE);
	gtk_window_set_policy(GTK_WINDOW(quote_desc_win), FALSE, TRUE, FALSE);

	vbox = gtk_vbox_new(FALSE, 8);
	gtk_container_add(GTK_CONTAINER(quote_desc_win), vbox);

	hbox = gtk_hbox_new(FALSE, 4);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

	label = gtk_label_new
		("%d:\n"
		 "%f:\n"
		 "%n:\n"
		 "%N:\n"
		 "%I:\n"
		 "%s:\n"
		 "%t:\n"
		 "%i:\n"
		 "%%:");

	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);

	label = gtk_label_new
		(_("Date\n"
		   "From\n"
		   "Full Name of Sender\n"
		   "First Name of Sender\n"
		   "Initial of Sender\n"
		   "Subject\n"
		   "To\n"
		   "Message-ID\n"
		   "%"));

	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);

	gtkut_button_set_create(&hbbox, &ok_btn, _("OK"),
				NULL, NULL, NULL, NULL);
	gtk_box_pack_end(GTK_BOX(vbox), hbbox, FALSE, FALSE, 0);

	gtk_widget_grab_default(ok_btn);
	gtk_signal_connect(GTK_OBJECT(ok_btn), "clicked",
				  GTK_SIGNAL_FUNC(gtk_main_quit), NULL);

	gtk_signal_connect(GTK_OBJECT(quote_desc_win), "delete_event",
					  GTK_SIGNAL_FUNC(gtk_main_quit), NULL);

	gtk_widget_show_all(vbox);
}

/* functions for setting items of SummaryView */

static struct _SummaryDisplayItem
{
	GtkWidget *window;

	GtkWidget *chkbtn[N_SUMMARY_COLS];

	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;
} summarydispitem;

#define SET_ACTIVE(column, var) \
	gtk_toggle_button_set_active \
		(GTK_TOGGLE_BUTTON(summarydispitem.chkbtn[column]), \
		 prefs_common.var)
#define GET_ACTIVE(column, var) \
	prefs_common.var = gtk_toggle_button_get_active \
		(GTK_TOGGLE_BUTTON(summarydispitem.chkbtn[column]))

void prefs_summary_display_item_set(void)
{
	static gboolean cancelled;

	if (!summarydispitem.window)
		prefs_summary_display_item_dialog_create(&cancelled);
	gtk_widget_grab_focus(summarydispitem.ok_btn);
	gtk_widget_show(summarydispitem.window);
	manage_window_set_transient(GTK_WINDOW(summarydispitem.window));

	SET_ACTIVE(S_COL_MARK, show_mark);
	SET_ACTIVE(S_COL_UNREAD, show_unread);
	SET_ACTIVE(S_COL_MIME, show_mime);
	SET_ACTIVE(S_COL_NUMBER, show_number);
	SET_ACTIVE(S_COL_SCORE, show_score);
	SET_ACTIVE(S_COL_SIZE, show_size);
	SET_ACTIVE(S_COL_DATE, show_date);
	SET_ACTIVE(S_COL_FROM, show_from);
	SET_ACTIVE(S_COL_SUBJECT, show_subject);

	gtk_main();
	gtk_widget_hide(summarydispitem.window);

	if (cancelled != TRUE) {
		GET_ACTIVE(S_COL_MARK, show_mark);
		GET_ACTIVE(S_COL_UNREAD, show_unread);
		GET_ACTIVE(S_COL_MIME, show_mime);
		GET_ACTIVE(S_COL_NUMBER, show_number);
		GET_ACTIVE(S_COL_SCORE, show_score);
		GET_ACTIVE(S_COL_SIZE, show_size);
		GET_ACTIVE(S_COL_DATE, show_date);
		GET_ACTIVE(S_COL_FROM, show_from);
		GET_ACTIVE(S_COL_SUBJECT, show_subject);

		main_window_reflect_prefs_all();
	}
}

#define SET_CHECK_BUTTON(column, label) \
{ \
	summarydispitem.chkbtn[column] = \
		gtk_check_button_new_with_label(label); \
	gtk_box_pack_start(GTK_BOX(chkbtn_vbox), \
			   summarydispitem.chkbtn[column], \
			   FALSE, FALSE, 0); \
}

static void prefs_summary_display_item_dialog_create(gboolean *cancelled)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *chkbtn_vbox;
	GtkWidget *hbbox;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;

	window = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_container_set_border_width(GTK_CONTAINER(window), 8);
	gtk_window_set_title(GTK_WINDOW(window), _("Set display item"));
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	gtk_window_set_policy(GTK_WINDOW(window), FALSE, FALSE, FALSE);
	gtk_signal_connect(GTK_OBJECT(window), "delete_event",
			   GTK_SIGNAL_FUNC(display_item_delete_event),
			   cancelled);
	gtk_signal_connect(GTK_OBJECT(window), "key_press_event",
			   GTK_SIGNAL_FUNC(display_item_key_pressed),
			   cancelled);

	vbox = gtk_vbox_new(FALSE, 8);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	chkbtn_vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), chkbtn_vbox, FALSE, FALSE, 0);

	SET_CHECK_BUTTON(S_COL_MARK, _("Mark"));
	SET_CHECK_BUTTON(S_COL_UNREAD, _("Unread"));
	SET_CHECK_BUTTON(S_COL_MIME, _("MIME"));
	SET_CHECK_BUTTON(S_COL_NUMBER, _("Number"));
	SET_CHECK_BUTTON(S_COL_SCORE, _("Score"));
	SET_CHECK_BUTTON(S_COL_SIZE, _("Size"));
	SET_CHECK_BUTTON(S_COL_DATE, _("Date"));
	SET_CHECK_BUTTON(S_COL_FROM, _("From"));
	SET_CHECK_BUTTON(S_COL_SUBJECT, _("Subject"));

	gtkut_button_set_create(&hbbox, &ok_btn, _("OK"),
				&cancel_btn, _("Cancel"), NULL, NULL);
	gtk_box_pack_end(GTK_BOX(vbox), hbbox, FALSE, FALSE, 0);
	gtk_widget_grab_default(ok_btn);

	gtk_signal_connect(GTK_OBJECT(ok_btn), "clicked",
			   GTK_SIGNAL_FUNC(display_item_ok), cancelled);
	gtk_signal_connect(GTK_OBJECT(cancel_btn), "clicked",
			   GTK_SIGNAL_FUNC(display_item_cancel), cancelled);

	gtk_widget_show_all(vbox);

	summarydispitem.window = window;
	summarydispitem.ok_btn = ok_btn;
	summarydispitem.cancel_btn = cancel_btn;
}

static void display_item_ok(GtkWidget *widget, gboolean *cancelled)
{
	*cancelled = FALSE;
	gtk_main_quit();
}

static void display_item_cancel(GtkWidget *widget, gboolean *cancelled)
{
	*cancelled = TRUE;
	gtk_main_quit();
}

static gint display_item_delete_event(GtkWidget *widget, GdkEventAny *event,
				      gboolean *cancelled)
{
	*cancelled = TRUE;
	gtk_main_quit();

	return TRUE;
}

static void display_item_key_pressed(GtkWidget *widget, GdkEventKey *event,
				     gboolean *cancelled)
{
	if (event && event->keyval == GDK_Escape) {
		*cancelled = TRUE;
		gtk_main_quit();
	}
}

static void prefs_font_select(GtkButton *button)
{
	if (!font_sel_win) {
		font_sel_win = gtk_font_selection_dialog_new
			(_("Font selection"));
		gtk_window_position(GTK_WINDOW(font_sel_win),
				    GTK_WIN_POS_CENTER);
		gtk_signal_connect(GTK_OBJECT(font_sel_win), "delete_event",
				   GTK_SIGNAL_FUNC(gtk_widget_hide_on_delete),
				   NULL);
		gtk_signal_connect
			(GTK_OBJECT(font_sel_win), "key_press_event",
			 GTK_SIGNAL_FUNC(prefs_font_selection_key_pressed),
			 NULL);
		gtk_signal_connect
			(GTK_OBJECT(GTK_FONT_SELECTION_DIALOG(font_sel_win)->ok_button),
			 "clicked",
			 GTK_SIGNAL_FUNC(prefs_font_selection_ok),
			 NULL);
		gtk_signal_connect_object
			(GTK_OBJECT(GTK_FONT_SELECTION_DIALOG(font_sel_win)->cancel_button),
			 "clicked",
			 GTK_SIGNAL_FUNC(gtk_widget_hide_on_delete),
			 GTK_OBJECT(font_sel_win));
	}

	manage_window_set_transient(GTK_WINDOW(font_sel_win));
	gtk_window_set_modal(GTK_WINDOW(font_sel_win), TRUE);
	gtk_widget_grab_focus
		(GTK_FONT_SELECTION_DIALOG(font_sel_win)->ok_button);
	gtk_widget_show(font_sel_win);
}

static void prefs_font_selection_key_pressed(GtkWidget *widget,
					     GdkEventKey *event,
					     gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		gtk_widget_hide(font_sel_win);
}

static void prefs_font_selection_ok(GtkButton *button)
{
	gchar *fontname;

	fontname = gtk_font_selection_dialog_get_font_name
		(GTK_FONT_SELECTION_DIALOG(font_sel_win));

	if (fontname) {
		gtk_entry_set_text(GTK_ENTRY(display.entry_textfont), fontname);
		g_free(fontname);
	}

	gtk_widget_hide(font_sel_win);
}

static void prefs_common_charset_set_data_from_optmenu(PrefParam *pparam)
{
	GtkWidget *menu;
	GtkWidget *menuitem;
	gchar *charset;

	menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(*pparam->widget));
	menuitem = gtk_menu_get_active(GTK_MENU(menu));
	charset = gtk_object_get_user_data(GTK_OBJECT(menuitem));
	g_free(*((gchar **)pparam->data));
	*((gchar **)pparam->data) = g_strdup(charset);
}

static void prefs_common_charset_set_optmenu(PrefParam *pparam)
{
	GList *cur;
	GtkOptionMenu *optmenu = GTK_OPTION_MENU(*pparam->widget);
	GtkWidget *menu;
	GtkWidget *menuitem;
	gchar *charset;
	gint n = 0;

	g_return_if_fail(optmenu != NULL);
	g_return_if_fail(*((gchar **)pparam->data) != NULL);

	menu = gtk_option_menu_get_menu(optmenu);
	for (cur = GTK_MENU_SHELL(menu)->children;
	     cur != NULL; cur = cur->next) {
		menuitem = GTK_WIDGET(cur->data);
		charset = gtk_object_get_user_data(GTK_OBJECT(menuitem));
		if (!strcmp(charset, *((gchar **)pparam->data))) {
			gtk_option_menu_set_history(optmenu, n);
			return;
		}
		n++;
	}

	gtk_option_menu_set_history(optmenu, 0);
	prefs_common_charset_set_data_from_optmenu(pparam);
}

static void prefs_common_key_pressed(GtkWidget *widget, GdkEventKey *event,
				     gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		gtk_widget_hide(dialog.window);
}

static void prefs_common_ok(GtkButton *button)
{
	prefs_common_apply(button);
	gtk_widget_hide(dialog.window);
	if (quote_desc_win && GTK_WIDGET_VISIBLE(quote_desc_win))
		gtk_widget_hide(quote_desc_win);
}

static void prefs_common_apply(GtkButton *button)
{
	prefs_set_data_from_dialog(param);
	main_window_reflect_prefs_all();
	prefs_common_save_config();
}
