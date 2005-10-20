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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __PREFS_COMMON_H__
#define __PREFS_COMMON_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>

#include "mainwindow.h"
#include "summaryview.h"
#include "folderview.h"
#include "codeconv.h"
#include "textview.h"
#include "procmime.h"

typedef struct _PrefsCommon	PrefsCommon;

typedef enum {
	RECV_DIALOG_ALWAYS,
	RECV_DIALOG_MANUAL,
	RECV_DIALOG_NEVER
} RecvDialogMode;

typedef enum {
	CTE_AUTO,
	CTE_BASE64,
	CTE_QUOTED_PRINTABLE,
	CTE_8BIT
} TransferEncodingMethod;

typedef enum {
	SEND_DIALOG_ALWAYS,
	/* SEND_DIALOG_ACTIVE would be irrelevant */
	SEND_DIALOG_NEVER
} SendDialogMode;

typedef enum
{
	SELECTONENTRY_UNREAD = 1 << 0,
	SELECTONENTRY_NEW    = 1 << 1
} SelectOnEntry;

typedef enum
{
	NEXTUNREADMSGDIALOG_ALWAYS,
	NEXTUNREADMSGDIALOG_ASSUME_YES,
	NEXTUNREADMSGDIALOG_ASSUME_NO
} NextUnreadMsgDialogShow;

struct _PrefsCommon
{
	/* Receive */
	gboolean use_extinc;
	gchar *extinc_cmd;
	gboolean scan_all_after_inc;
	gboolean autochk_newmail;
	gint autochk_itv;
	gboolean chk_on_startup;
 	gboolean newmail_notify_auto;
 	gboolean newmail_notify_manu;
 	gchar   *newmail_notify_cmd;
	RecvDialogMode recv_dialog_mode;
	gboolean close_recv_dialog;
	gboolean no_recv_err_panel;

	/* Send */
	gboolean savemsg;
	gboolean confirm_send_queued_messages;
	SendDialogMode send_dialog_mode;
	gchar *outgoing_charset;
	TransferEncodingMethod encoding_method;

	gboolean allow_jisx0201_kana;

	/* Compose */
	gint undolevels;
	gint linewrap_len;
	gboolean linewrap_quote;
	gboolean linewrap_pastes;
	gboolean autowrap;
	gboolean linewrap_at_send;
	gboolean auto_exteditor;
	gboolean reply_account_autosel;
	gboolean default_reply_list;
	gboolean forward_account_autosel;
	gboolean reedit_account_autosel;
	gboolean show_ruler;
	gboolean autosave;
	gint autosave_length;
	
	/* Quote */
	gboolean reply_with_quote;
	gchar *quotemark;
	gchar *quotefmt;
	gchar *fw_quotemark;
	gchar *fw_quotefmt;
	gboolean forward_as_attachment;
	gboolean redirect_keep_from;
	gboolean block_cursor;
	gchar *quote_chars;
	
#if USE_ASPELL
	gboolean enable_aspell;
	gchar *aspell_path;
	gchar *dictionary;
	gulong misspelled_col;
	gint aspell_sugmode;
	gboolean check_while_typing;
	gboolean use_alternate;
#endif
        
	/* Display */
	/* obsolete fonts */
	gchar *widgetfont_gtk1;
	gchar *textfont_gtk1;
	gchar *normalfont_gtk1;
	gchar *boldfont_gtk1;
	gchar *smallfont_gtk1;

	/* new fonts */
	gchar *widgetfont;
	gchar *textfont;
	gchar *normalfont;
	gchar *smallfont;
	gchar *titlefont;

	/* image viewer */
	gboolean display_img;
	gboolean resize_img;
	gboolean inline_img;

	gboolean trans_hdr;
	gboolean display_folder_unread;
	gint ng_abbrev_len;

	gboolean show_searchbar;
	gboolean expand_thread;
	gboolean swap_from;
	gboolean use_addr_book;
	gchar *date_format;

	gboolean enable_rules_hint;
	gboolean enable_hscrollbar;
	gboolean bold_unread;
	gboolean enable_thread;
	gboolean thread_by_subject;
	gint thread_by_subject_max_age; /*!< Max. age of a thread which was threaded
					 *   by subject (days) */

	ToolbarStyle toolbar_style;
	gboolean toolbar_detachable;
	gboolean show_statusbar;

	gint folderview_vscrollbar_policy;

	/* Filtering */
	GSList *fltlist;

	gint kill_score;
	gint important_score;

	/* Actions */
	GSList *actions_list;

	/* Summary columns visibility, position and size */
	gboolean summary_col_visible[N_SUMMARY_COLS];
	gint summary_col_pos[N_SUMMARY_COLS];
	gint summary_col_size[N_SUMMARY_COLS];

	gboolean folder_col_visible[N_FOLDER_COLS];
	gint folder_col_pos[N_FOLDER_COLS];
	gint folder_col_size[N_FOLDER_COLS];

	/* Widget visibility, position and size */
	gint folderwin_x;
	gint folderwin_y;
	gint folderview_width;
	gint folderview_height;
	gboolean folderview_visible;

	gint summaryview_width;
	gint summaryview_height;

	gint main_msgwin_x;
	gint main_msgwin_y;
	gint msgview_width;
	gint msgview_height;
	gboolean msgview_visible;

	gint mainview_x;
	gint mainview_y;
	gint mainview_width;
	gint mainview_height;
	gint mainwin_x;
	gint mainwin_y;
	gint mainwin_width;
	gint mainwin_height;

	gint msgwin_width;
	gint msgwin_height;

	gint sourcewin_width;
	gint sourcewin_height;

	gint compose_width;
	gint compose_height;
	gint compose_x;
	gint compose_y;

	/* Message */
	gboolean enable_color;
	gulong quote_level1_col;
	gulong quote_level2_col;
	gulong quote_level3_col;
	gulong uri_col;
	gulong tgt_folder_col;
	gulong signature_col;
	gboolean recycle_quote_colors;
	gboolean conv_mb_alnum;
	gboolean display_header_pane;
	gboolean display_header;
	gboolean display_xface;
	gint line_space;
	gboolean render_html;
	gboolean textview_cursor_visible;
	gboolean enable_smooth_scroll;
	gint scroll_step;
	gboolean scroll_halfpage;

	gboolean show_other_header;
	GSList *disphdr_list;

	gboolean attach_desc;

	/* MIME viewer */
	gchar *mime_image_viewer;
	gchar *mime_audio_player;
	gchar *mime_open_cmd;
	gchar *attach_save_dir;

	GList *mime_open_cmd_history;

#if USE_GPGME
	/* Privacy */
	gboolean auto_check_signatures;
	gboolean gpg_signature_popup;
	gboolean store_passphrase;
	gint store_passphrase_timeout;
	gboolean passphrase_grab;
	gboolean gpg_warning;
#endif /* USE_GPGME */

	/* Interface */
	gboolean sep_folder;
	gboolean sep_msg;
	gint statusbar_update_step;
	gboolean emulate_emacs;
	gboolean always_show_msg;
	gboolean mark_as_read_on_new_window;
	gboolean mark_as_read_delay;
	gboolean open_inbox_on_inc;
	gboolean immediate_exec;
	SelectOnEntry select_on_entry;
	NextUnreadMsgDialogShow next_unread_msg_dialog;
	gboolean add_address_by_click;
	gchar *pixmap_theme_path;
	int hover_timeout; /* msecs mouse hover timeout */
	gboolean ask_mark_all_read;

	/* Other */
	gchar *uri_cmd;
	gchar *print_cmd;
	gchar *ext_editor_cmd;

        gboolean cliplog;
        guint loglength;

        gboolean confirm_on_exit;
	gboolean clean_on_exit;
	gboolean ask_on_clean;
	gboolean warn_queued_on_exit;

	gint io_timeout_secs;
	
#if 0
#ifdef USE_OPENSSL
	gboolean ssl_ask_unknown_valid;
#endif
#endif

	/* Memory cache*/
	gint cache_max_mem_usage;
	gint cache_min_keep_time;
	
	/* boolean for work offline 
	   stored here for use in inc.c */
	gboolean work_offline;
	
	gint summary_quicksearch_type;
	gint summary_quicksearch_sticky;
	gint summary_quicksearch_recurse;
	gulong color_new;
	
	GList *summary_quicksearch_history;

	gint filteringwin_width;
	gint filteringwin_height;
	gint filteringactionwin_width;
	gint filteringactionwin_height;
	gint matcherwin_width;
	gint matcherwin_height;
	gint templateswin_width;
	gint templateswin_height;
	gint actionswin_width;
	gint actionswin_height;
	gint addressbookwin_width;
	gint addressbookwin_height;
	gint addressbookeditpersonwin_width;
	gint addressbookeditpersonwin_height;
	gint pluginswin_width;
	gint pluginswin_height;
	gint prefswin_width;
	gint prefswin_height;
	gint folderitemwin_width;
	gint folderitemwin_height;
	gint editaccountwin_width;
	gint editaccountwin_height;
	gint accountswin_width;
	gint accountswin_height;
	gint logwin_width;
	gint logwin_height;

	gint warn_dnd;
	gint broken_are_utf8;
};

extern PrefsCommon prefs_common;

void prefs_common_read_config	(void);
void prefs_common_write_config	(void);
void prefs_common_open		(void);
PrefsCommon *prefs_common_get	(void);
void pref_set_textview_from_pref(GtkTextView *textview, gchar *txt);
gchar *pref_get_pref_from_textview(GtkTextView *textview);

#endif /* __PREFS_COMMON_H__ */
