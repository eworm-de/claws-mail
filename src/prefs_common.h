/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999,2000 Hiroyuki Yamamoto
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

#ifndef __PREFS_COMMON_H__
#define __PREFS_COMMON_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>

#include "mainwindow.h"
#include "codeconv.h"
#include "textview.h"

typedef struct _PrefsCommon	PrefsCommon;

struct _PrefsCommon
{
	/* Receive */
	gboolean use_extinc;
	gchar *extinc_path;
	gboolean inc_local;
	gboolean filter_on_inc;
	gchar *spool_path;
	gboolean autochk_newmail;
	gint autochk_itv;
	gboolean chk_on_startup;
	gint max_articles;

	/* Send */
	gboolean use_extsend;
	gchar *extsend_path;
	gboolean savemsg;
	gboolean queue_msg;
	gchar *outgoing_charset;

	/* Compose */
	gboolean reply_with_quote;
	gchar *quotemark;
	gchar *quotefmt;
	gboolean auto_sig;
	gchar *sig_sep;
	gint linewrap_len;
	gboolean linewrap_quote;
	gboolean linewrap_at_send;
	gboolean show_ruler;

	/* Display */
	gchar *widgetfont;
	gchar *textfont;
	gboolean display_folder_unread;
	ToolbarStyle toolbar_style;
	gboolean show_statusbar;
	gboolean trans_hdr;
	gboolean enable_thread;
	gboolean enable_hscrollbar;
	gboolean swap_from;
	gchar *date_format;

	/* Filtering */
	GSList *fltlist;

	gboolean show_mark;
	gboolean show_unread;
	gboolean show_mime;
	gboolean show_number;
	gboolean show_size;
	gboolean show_date;
	gboolean show_from;
	gboolean show_subject;

	/* Widget size */
	gint folderview_x;
	gint folderview_y;
	gint folderview_width;
	gint folderview_height;
	gint folder_col_folder;
	gint folder_col_new;
	gint folder_col_unread;
	gint folder_col_total;

	gint summaryview_width;
	gint summaryview_height;
	gint summary_col_mark;
	gint summary_col_unread;
	gint summary_col_mime;
	gint summary_col_number;
	gint summary_col_size;
	gint summary_col_date;
	gint summary_col_from;
	gint summary_col_subject;

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

	gint compose_width;
	gint compose_height;

	/* Message */
	gboolean enable_color;
	gint quote_level1_col;
	gint quote_level2_col;
	gint quote_level3_col;
	gint uri_col;
	gushort sig_col;
	gboolean recycle_quote_colors;
	gboolean conv_mb_alnum;
	gboolean display_header_pane;
	gboolean display_header;
	gboolean head_space;
	gint line_space;
	gboolean enable_smooth_scroll;
	gint scroll_step;
	gboolean scroll_halfpage;

	gchar *force_charset;

	/* MIME viewer */
	gchar *mime_image_viewer;
	gchar *mime_audio_player;

	/* Privacy */
	gboolean gpgme_warning;
	gboolean default_encrypt;
	gboolean default_sign;
        gboolean auto_check_signatures;
	gboolean passphrase_grab;
	gchar *default_signkey;

	/* Interface */
	gboolean sep_folder;
	gboolean sep_msg;
	gboolean emulate_emacs;
	gboolean open_unread_on_enter;
	gboolean open_inbox_on_inc;
	gboolean immediate_exec;
	gboolean add_address_by_click;

	gboolean confirm_on_exit;
	gboolean clean_on_exit;
	gboolean ask_on_clean;
	gboolean warn_queued_on_exit;

	/* Other */
	gchar *uri_cmd;
	gchar *print_cmd;
	gchar *ext_editor_cmd;
};

extern PrefsCommon prefs_common;

void prefs_common_read_config	(void);
void prefs_common_save_config	(void);
void prefs_common_open		(void);

void prefs_summary_display_item_set	(void);

#endif /* __PREFS_COMMON_H__ */
