/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2021 the Claws Mail team and Hiroyuki Yamamoto
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
 * 
 */

#ifndef FOLDER_ITEM_PREFS_H
#define FOLDER_ITEM_PREFS_H

#include <glib.h>
#include <sys/types.h>

typedef struct _FolderItemPrefs FolderItemPrefs;

#include "folder.h"
typedef enum {
	HTML_RENDER_DEFAULT=0,
	HTML_RENDER_NEVER,
	HTML_RENDER_ALWAYS
} HTMLRenderType;
typedef enum {
	INVOKE_PLUGIN_ON_HTML_DEFAULT=0,
	INVOKE_PLUGIN_ON_HTML_NEVER,
	INVOKE_PLUGIN_ON_HTML_ALWAYS
} InvokePluginOnHTMLType;
typedef enum {
	HTML_PROMOTE_DEFAULT=0,
	HTML_PROMOTE_NEVER,
	HTML_PROMOTE_ALWAYS
} HTMLPromoteType;
typedef enum {
	SIGN_OR_ENCRYPT_DEFAULT=0,
	SIGN_OR_ENCRYPT_NEVER,
	SIGN_OR_ENCRYPT_ALWAYS
} SignOrEncryptType;

struct _FolderItemPrefs {
	gchar * directory;

	gint config_version;

	int enable_processing; /* at start-up */
	int enable_processing_when_opening;
	GSList * processing;

	int newmailcheck;
	int offlinesync;
	int offlinesync_days;
	int remove_old_bodies;
	HTMLRenderType render_html;
	InvokePluginOnHTMLType invoke_plugin_on_html;
	HTMLPromoteType promote_html_part;
	gboolean skip_on_goto_unread_or_new;

	gboolean request_return_receipt;
	gboolean enable_default_from;
	gchar *default_from;
	gboolean enable_default_to;
	gchar *default_to;
	gboolean enable_default_reply_to;
	gchar *default_reply_to;
	gboolean enable_default_cc;
	gchar *default_cc;
	gboolean enable_default_bcc;
	gchar *default_bcc;
	gboolean enable_default_replyto;
	gchar *default_replyto;
	gboolean enable_simplify_subject;
	gchar *simplify_subject_regexp;
	gboolean enable_folder_chmod;
	gint folder_chmod;
	gboolean enable_default_account;
	gint default_account;

	gboolean enable_default_dictionary;
	gchar *default_dictionary;
	gboolean enable_default_alt_dictionary;
	gchar *default_alt_dictionary;
	SignOrEncryptType always_sign;
	SignOrEncryptType always_encrypt;
	gboolean save_copy_to_folder;
	GdkRGBA color;

	gboolean compose_with_format;
	gchar *compose_override_from_format;
	gchar *compose_subject_format;
	gchar *compose_body_format;
	gboolean reply_with_format;
	gchar *reply_quotemark;
	gchar *reply_override_from_format;
	gchar *reply_body_format;
	gboolean forward_with_format;
	gchar *forward_quotemark;
	gchar *forward_override_from_format;
	gchar *forward_body_format;
};

void folder_item_prefs_read_config(FolderItem * item);
void folder_item_prefs_save_config(FolderItem * item);
void folder_item_prefs_save_config_recursive(FolderItem * item);
void folder_prefs_save_config_recursive(Folder *folder);
FolderItemPrefs *folder_item_prefs_new(void);
void folder_item_prefs_free(FolderItemPrefs * prefs);
void folder_item_prefs_copy_prefs(FolderItem * src, FolderItem * dest);

#endif /* FOLDER_ITEM_PREFS_H */
