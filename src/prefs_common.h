/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2023 the Claws Mail team and Hiroyuki Yamamoto
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

#ifndef __PREFS_COMMON_H__
#define __PREFS_COMMON_H__

#ifdef HAVE_CONFIG_H
#include "claws-features.h"
#endif

#include <glib.h>

#include "mainwindow.h"
#include "summaryview.h"
#include "folderview.h"
#include "codeconv.h"
#include "textview.h"
#include "procmime.h"
#include "prefs_msg_colors.h"
#include "prefs_summary_open.h"

#define CLAWS_CONFIG_VERSION 4

typedef struct _PrefsCommon	PrefsCommon;

typedef enum {
	RECV_DIALOG_ALWAYS,
	RECV_DIALOG_MANUAL,
	RECV_DIALOG_NEVER
} RecvDialogMode;

typedef enum {
	COMPOSE_DND_ASK,
	COMPOSE_DND_INSERT,
	COMPOSE_DND_ATTACH
} ComposeDndInsertOrAttach;

typedef enum {
	CTE_AUTO,
	CTE_BASE64,
	CTE_QUOTED_PRINTABLE,
	CTE_8BIT
} TransferEncodingMethod;

typedef enum
{
/* U = unread, N = new, M = marked */
	SELECTONENTRY_NOTHING,
	SELECTONENTRY_UNM,
	SELECTONENTRY_UMN,
	SELECTONENTRY_NUM,
	SELECTONENTRY_NMU,
	SELECTONENTRY_MNU,
	SELECTONENTRY_MUN,
	SELECTONENTRY_LAST
} SelectOnEntry;

typedef enum
{
	ACTION_UNSET = 0, /* for backward compatibility */
	ACTION_OLDEST_MARKED,
	ACTION_OLDEST_NEW,
	ACTION_OLDEST_UNREAD,
	ACTION_LAST_OPENED,
	ACTION_NEWEST_LIST,
	ACTION_NOTHING,
	ACTION_OLDEST_LIST,
	ACTION_NEWEST_MARKED,
	ACTION_NEWEST_NEW,
	ACTION_NEWEST_UNREAD
} EntryAction;

typedef enum
{
	NEXTUNREADMSGDIALOG_ALWAYS,
	NEXTUNREADMSGDIALOG_ASSUME_YES,
	NEXTUNREADMSGDIALOG_ASSUME_NO
} NextUnreadMsgDialogShow;

typedef enum
{
	SHOW_NAME,
	SHOW_ADDR,
	SHOW_BOTH
} SummaryFromShow;

typedef enum
{
	AVATARS_DISABLE = 0,
	AVATARS_ENABLE_CAPTURE = 1,
	AVATARS_ENABLE_RENDER = 2,
	AVATARS_ENABLE_BOTH = 3
} EnableAvatars;

typedef enum
{
	COL_MISSPELLED,
	COL_QUOTE_LEVEL1,
	COL_QUOTE_LEVEL2,
	COL_QUOTE_LEVEL3,
	COL_QUOTE_LEVEL1_BG,
	COL_QUOTE_LEVEL2_BG,
	COL_QUOTE_LEVEL3_BG,
	COL_URI,
	COL_TGT_FOLDER,
	COL_SIGNATURE,
	COL_EMPHASIS,
	COL_DEFAULT_HEADER,
	COL_DEFAULT_HEADER_BG,
	COL_TAGS,
	COL_TAGS_BG,
	COL_QS_ACTIVE,
	COL_QS_ACTIVE_BG,
	COL_QS_ERROR,
	COL_QS_ERROR_BG,
	COL_LOG_MSG,
	COL_LOG_WARN,
	COL_LOG_ERROR,
	COL_LOG_IN,
	COL_LOG_OUT,
	COL_LOG_STATUS_OK,
	COL_LOG_STATUS_NOK,
	COL_LOG_STATUS_SKIP,
	COL_NEW,
	COL_DIFF_ADDED,
	COL_DIFF_DELETED,
	COL_DIFF_HUNK,
	COL_LAST_COLOR_INDEX
} ColorIndex;

struct _PrefsCommon
{
	gint config_version;

	/* Receive */
	gboolean use_extinc;
	gchar *extinc_cmd;
	gboolean scan_all_after_inc;
	gboolean autochk_newmail;
	gint autochk_itv;
	gboolean chk_on_startup;
	gboolean open_inbox_on_inc;
 	gboolean newmail_notify_auto;
 	gboolean newmail_notify_manu;
 	gchar   *newmail_notify_cmd;
	RecvDialogMode recv_dialog_mode;
	gint receivewin_width;
	gint receivewin_height;
	gboolean close_recv_dialog;
	gboolean no_recv_err_panel;

	/* Send */
	gboolean savemsg;
	gboolean confirm_send_queued_messages;
	gboolean send_dialog_invisible;
	gint sendwin_width;
	gint sendwin_height;
	gchar *outgoing_charset;
	TransferEncodingMethod encoding_method;
	gboolean outgoing_fallback_to_ascii;
	gboolean rewrite_first_from;
	gboolean warn_empty_subj;
	gint warn_sending_many_recipients_num;
	gboolean hide_timezone;
	gboolean allow_jisx0201_kana;

	/* Compose */
	gint undolevels;
	gint linewrap_len;
	gboolean linewrap_quote;
	gboolean linewrap_pastes;
	gboolean primary_paste_unselects;
	gboolean autowrap;
	gboolean auto_indent;
	gboolean auto_exteditor;
	gboolean reply_account_autosel;
	gboolean default_reply_list;
	gboolean forward_account_autosel;
	gboolean reedit_account_autosel;
	gboolean show_ruler;
	gboolean autosave;
	gint autosave_length;
	gboolean autosave_encrypted;
	gboolean warn_large_insert;
	gint warn_large_insert_size;
	gboolean compose_no_markup;
	ComposeDndInsertOrAttach compose_dnd_mode;
	gboolean compose_with_format;
	gchar *compose_subject_format;
	gchar *compose_body_format;
	gboolean show_compose_margin;
	gboolean type_any_header;

	/* Quote */
	gboolean reply_with_quote;
	gchar *quotemark;
	gchar *quotefmt;
	gchar *fw_quotemark;
	gchar *fw_quotefmt;
	gboolean forward_as_attachment;
	gboolean redirect_keep_from;
	gchar *quote_chars;
	
	gboolean enable_aspell;
	gchar *dictionary;
	gchar *alt_dictionary;
	gboolean check_while_typing;
	gboolean recheck_when_changing_dict;
	gboolean use_alternate;
	gboolean use_both_dicts;

	/* Display */
	/* fonts */
	gchar *textfont;
	gchar *printfont;
	gchar *boldfont;
	gchar *normalfont;
	gchar *smallfont;
	gchar *titlefont;
	gboolean use_different_print_font;
	gboolean derive_from_normal_font;

	/* custom colors */
	ColorlabelPrefs custom_colorlabel[COLORLABELS];

	/* program colors */
	GdkRGBA color[COL_LAST_COLOR_INDEX];

	/* image viewer */
	gboolean display_img;
	gboolean resize_img;
	gboolean inline_img;
	gboolean fit_img_height;

	gboolean trans_hdr;
	gint display_folder_unread;
	gint ng_abbrev_len;

	gboolean show_searchbar;
	gboolean swap_from;
	gboolean use_addr_book;
	gchar *date_format;
	gboolean *msgview_date_format;

	gboolean use_stripes_everywhere;
	gboolean use_stripes_in_summaries; /* overrides if use_stripes_everywhere is set to TRUE */
	gint stripes_color_offset;
	gboolean enable_hscrollbar;
	gboolean bold_unread;
	gboolean bold_marked;
	gboolean next_on_delete;
	gboolean thread_by_subject;
	gint thread_by_subject_max_age; /*!< Max. age of a thread which was threaded
					 *   by subject (days) */
	FolderSortKey default_sort_key;
	FolderSortType default_sort_type;
	gboolean folder_default_thread;
	gboolean folder_default_thread_collapsed;
	gboolean folder_default_hide_read_threads;
	gboolean folder_default_hide_read_msgs;
	gboolean folder_default_hide_del_msgs;

	gchar *last_opened_folder;
	gboolean goto_last_folder_on_startup;
	gchar *startup_folder;
	gboolean goto_folder_on_startup;

	ToolbarStyle toolbar_style;
	gboolean show_statusbar;
	gboolean show_col_headers;

	gint folderview_vscrollbar_policy;

	/* Filtering */
	GSList *fltlist;

	gint kill_score;
	gint important_score;

	/* Actions */
	GSList *actions_list;

	/* Summary lock column headers */
	gboolean summary_col_lock;

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
	gint mainwin_maximised;
	gint mainwin_fullscreen;
	gint mainwin_menubar;

	gint msgwin_width;
	gint msgwin_height;

	gint mimeview_tree_height;
	
	gint sourcewin_width;
	gint sourcewin_height;

	gint compose_width;
	gint compose_height;
	gint compose_notebook_height;
	gint compose_x;
	gint compose_y;

	/* Message */
	gboolean enable_color;
	gboolean enable_bgcolor;
	gboolean recycle_quote_colors;
	gboolean display_header_pane;
	gboolean display_header;
	gboolean display_xface;
	gboolean save_xface;
	gint line_space;
	gboolean render_html;
	gboolean invoke_plugin_on_html;
	gboolean promote_html_part;
	gboolean textview_cursor_visible;
	gboolean enable_smooth_scroll;
	gint scroll_step;
	gboolean scroll_halfpage;
	gboolean hide_quoted;
	gboolean respect_flowed_format;
	gboolean show_all_headers;

	gboolean show_other_header;
	GSList *disphdr_list;

	gboolean attach_desc;

	/* MIME viewer */
	gchar *mime_textviewer;
	gchar *mime_open_cmd;
	gchar *attach_save_dir;
	gchar *attach_load_dir;

	GList *mime_open_cmd_history;
	gboolean show_inline_attachments;

	/* Addressbook */
	gboolean addressbook_use_editaddress_dialog;
	gint addressbook_hpaned_pos;
	gint addressbook_vpaned_pos;
	GList *addressbook_custom_attributes;

	/* Interface */
	LayoutType layout_mode;

	gint statusbar_update_step;
	gboolean emulate_emacs;

	gboolean open_selected_on_folder_open;
	gboolean open_selected_on_search_results;
	gboolean open_selected_on_prevnext;
	gboolean open_selected_on_deletemove;
	gboolean open_selected_on_directional;
	gboolean always_show_msg;

	gboolean mark_as_read_on_new_window;
	gboolean mark_as_read_delay;
	gboolean immediate_exec;
	SelectOnEntry select_on_entry;
	gboolean show_tooltips;

	EntryAction summary_select_prio[SUMMARY_OPEN_ACTIONS-1];

	NextUnreadMsgDialogShow next_unread_msg_dialog;
	SummaryFromShow summary_from_show;
	gboolean add_address_by_click;
	gchar *pixmap_theme_path;
#ifdef HAVE_SVG
	gboolean enable_alpha_svg;
	gboolean enable_pixmap_scaling;
	gboolean pixmap_scaling_auto;
	gint pixmap_scaling_ppi;
#endif
	int hover_timeout; /* msecs mouse hover timeout */
	gboolean ask_mark_all_read;
	gboolean run_processingrules_before_mark_all;
	gboolean ask_override_colorlabel;
	gboolean ask_apply_per_account_filtering_rules;
	gint apply_per_account_filtering_rules;

	/* Other */
#ifndef G_OS_WIN32
	gchar *uri_cmd;
#else
	gchar *gtk_theme;
#endif
	gchar *ext_editor_cmd;
	gboolean cmds_use_system_default;

    	gboolean cliplog;
    	guint loglength;
	gboolean enable_log_standard;
	gboolean enable_log_warning;
	gboolean enable_log_error;
	gboolean enable_log_status;

	gboolean enable_filtering_debug;
	gint filtering_debug_level;
	gboolean enable_filtering_debug_inc;
	gboolean enable_filtering_debug_manual;
	gboolean enable_filtering_debug_folder_proc;
	gboolean enable_filtering_debug_pre_proc;
	gboolean enable_filtering_debug_post_proc;
	gboolean filtering_debug_cliplog;
	guint filtering_debug_loglength;

	gboolean confirm_on_exit;
	gboolean session_passwords;
	gboolean clean_on_exit;
	gboolean ask_on_clean;
	gboolean warn_queued_on_exit;

	gint io_timeout_secs;

	gboolean gtk_enable_accels;
	
	/* Memory cache*/
	gint cache_max_mem_usage;
	gint cache_min_keep_time;
	
	/* boolean for work offline 
	   stored here for use in inc.c */
	gboolean work_offline;
	
	gint summary_quicksearch_type;
	gint summary_quicksearch_sticky;
	gint summary_quicksearch_recurse;
	gint summary_quicksearch_dynamic;
	gint summary_quicksearch_autorun;

	GList *summary_quicksearch_history;
	GList *summary_search_from_history;
	GList *summary_search_to_history;
	GList *summary_search_subject_history;
	GList *summary_search_body_history;
	GList *summary_search_adv_condition_history;
	GList *message_search_history;
	GList *compose_save_to_history;

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
	gint actionsiodialog_width;
	gint actionsiodialog_height;
	gint tagswin_width;
	gint tagswin_height;
	gint sslmanwin_width;
	gint sslmanwin_height;
	gint uriopenerwin_width;
	gint uriopenerwin_height;
	gint foldersortwin_width;
	gint foldersortwin_height;
	gint addressbookwin_width;
	gint addressbookwin_height;
	gint addressbookeditpersonwin_width;
	gint addressbookeditpersonwin_height;
	gint addressbookeditgroupwin_width;
	gint addressbookeditgroupwin_height;
	gint pluginswin_width;
	gint pluginswin_height;
	gint prefswin_width;
	gint prefswin_height;
	gint folderitemwin_width;
	gint folderitemwin_height;
	gchar *zero_replacement;
	gint editaccountwin_width;
	gint editaccountwin_height;
	gint accountswin_width;
	gint accountswin_height;
	gint logwin_width;
	gint logwin_height;
	gint filtering_debugwin_width;
	gint filtering_debugwin_height;
	gint folderselwin_width;
	gint folderselwin_height;
	gint addressaddwin_width;
	gint addressaddwin_height;
	gint addressbook_folderselwin_width;
	gint addressbook_folderselwin_height;
	gint aboutwin_width;
	gint aboutwin_height;
	gint addrgather_width;
	gint addrgather_height;
	gint news_subscribe_width;
	gint news_subscribe_height;

	gint imap_scan_tree_recurs_limit;
	gint warn_dnd;
	gint broken_are_utf8;
	gint skip_ssl_cert_check;
	gint live_dangerously;
	gint save_parts_readwrite;
	gint never_send_retrcpt;
	gint hide_quotes;
	gboolean unsafe_ssl_certs;
	gboolean real_time_sync;
	gboolean show_save_all_success;
	gboolean show_save_all_failure;

	gchar *print_paper_type;
	gint print_paper_orientation;
	gint print_margin_top;
	gint print_margin_bottom;
	gint print_margin_left;
	gint print_margin_right;
  
	gint print_use_color;
	gint print_use_collate;
	gint print_use_reverse;
	gint print_use_duplex;
	gint print_imgs;
	gint print_previewwin_width;
	gint print_previewwin_height;
	
	gboolean use_networkmanager;
	gboolean use_shred;
	gboolean two_line_vert;
	gboolean inherit_folder_props;
	gboolean flush_metadata;

	gint nav_history_length;

	gboolean folder_search_wildcard;
	gboolean address_search_wildcard;

	guint enable_avatars;

#ifndef PASSWORD_CRYPTO_OLD
	gboolean use_primary_passphrase;
	gchar *primary_passphrase;
	gchar *primary_passphrase_salt;
	guint primary_passphrase_pbkdf2_rounds;
#endif

	/* Proxy */
	gboolean use_proxy;
	ProxyInfo proxy_info;

    /* Quicksearch */
    guint qs_press_timeout;
};

extern PrefsCommon prefs_common;

PrefsCommon *prefs_common_get_prefs(void);

GList *prefs_common_read_history_from_dir_with_defaults(const gchar *dirname, const gchar *history,
															  GList *default_list);
void prefs_common_read_config	(void);
void prefs_common_write_config	(void);
void prefs_common_open		(void);
void pref_get_unescaped_pref(gchar *out, const gchar *in);
void pref_get_escaped_pref(gchar *out, const gchar *in);
void pref_set_textview_from_pref(GtkTextView *textview, const gchar *txt);
void pref_set_entry_from_pref(GtkEntry *entry, const gchar *txt);
gchar *pref_get_pref_from_textview(GtkTextView *textview);
gchar *pref_get_pref_from_entry(GtkEntry *entry);
const gchar *prefs_common_translated_header_name(const gchar *header_name);
const gchar *prefs_common_get_uri_cmd(void);
const gchar *prefs_common_get_ext_editor_cmd(void);

#define OPEN_SELECTED(when) (prefs_common.always_show_msg || prefs_common.when)

#define OPEN_SELECTED_ON_FOLDER_OPEN OPEN_SELECTED(open_selected_on_folder_open)
#define OPEN_SELECTED_ON_SEARCH_RESULTS OPEN_SELECTED(open_selected_on_search_results)
#define OPEN_SELECTED_ON_PREVNEXT OPEN_SELECTED(open_selected_on_prevnext)
#define OPEN_SELECTED_ON_DELETEMOVE OPEN_SELECTED(open_selected_on_deletemove)
#define OPEN_SELECTED_ON_DIRECTIONAL OPEN_SELECTED(open_selected_on_directional)

#endif /* __PREFS_COMMON_H__ */
