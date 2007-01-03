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

/* alfons - all folder item specific settings should migrate into 
 * folderlist.xml!!! the old folderitemrc file will only serve for a few 
 * versions (for compatibility) */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>

#include "defs.h"
#include "folder.h"
#include "utils.h"
#include "prefs_gtk.h"
#include "filtering.h"
#include "folder_item_prefs.h"

FolderItemPrefs tmp_prefs;

static PrefParam param[] = {
	{"sort_by_number", "FALSE", &tmp_prefs.sort_by_number, P_BOOL,
	 NULL, NULL, NULL},
	{"sort_by_size", "FALSE", &tmp_prefs.sort_by_size, P_BOOL,
	 NULL, NULL, NULL},
	{"sort_by_date", "FALSE", &tmp_prefs.sort_by_date, P_BOOL,
	 NULL, NULL, NULL},
	{"sort_by_from", "FALSE", &tmp_prefs.sort_by_from, P_BOOL,
	 NULL, NULL, NULL},
	{"sort_by_subject", "FALSE", &tmp_prefs.sort_by_subject, P_BOOL,
	 NULL, NULL, NULL},
	{"sort_by_score", "FALSE", &tmp_prefs.sort_by_score, P_BOOL,
	 NULL, NULL, NULL},
	{"sort_descending", "FALSE", &tmp_prefs.sort_descending, P_BOOL,
	 NULL, NULL, NULL},
	/* MIGRATION */	 
	{"request_return_receipt", "", &tmp_prefs.request_return_receipt, P_BOOL,
	 NULL, NULL, NULL},
	{"enable_default_to", "", &tmp_prefs.enable_default_to, P_BOOL,
	 NULL, NULL, NULL},
	{"default_to", "", &tmp_prefs.default_to, P_STRING,
	 NULL, NULL, NULL},
	{"enable_default_reply_to", "", &tmp_prefs.enable_default_reply_to, P_BOOL,
	 NULL, NULL, NULL},
	{"default_reply_to", "", &tmp_prefs.default_reply_to, P_STRING,
	 NULL, NULL, NULL},
	{"enable_simplify_subject", "", &tmp_prefs.enable_simplify_subject, P_BOOL,
	 NULL, NULL, NULL},
	{"simplify_subject_regexp", "", &tmp_prefs.simplify_subject_regexp, P_STRING,
	 NULL, NULL, NULL},
	{"enable_folder_chmod", "", &tmp_prefs.enable_folder_chmod, P_BOOL,
	 NULL, NULL, NULL},
	{"folder_chmod", "", &tmp_prefs.folder_chmod, P_INT,
	 NULL, NULL, NULL},
	{"enable_default_account", "", &tmp_prefs.enable_default_account, P_BOOL,
	 NULL, NULL, NULL},
	{"default_account", NULL, &tmp_prefs.default_account, P_INT,
	 NULL, NULL, NULL},
#if USE_ASPELL
	{"enable_default_dictionary", "", &tmp_prefs.enable_default_dictionary, P_BOOL,
	 NULL, NULL, NULL},
	{"default_dictionary", NULL, &tmp_prefs.default_dictionary, P_STRING,
	 NULL, NULL, NULL},
	{"enable_default_alt_dictionary", "", &tmp_prefs.enable_default_alt_dictionary, P_BOOL,
	 NULL, NULL, NULL},
	{"default_alt_dictionary", NULL, &tmp_prefs.default_alt_dictionary, P_STRING,
	 NULL, NULL, NULL},
#endif	 
	{"save_copy_to_folder", NULL, &tmp_prefs.save_copy_to_folder, P_BOOL,
	 NULL, NULL, NULL},
	{"folder_color", "", &tmp_prefs.color, P_INT,
	 NULL, NULL, NULL},
	{"enable_processing", "FALSE", &tmp_prefs.enable_processing, P_BOOL,
	 NULL, NULL, NULL},
	{"newmailcheck", "TRUE", &tmp_prefs.newmailcheck, P_BOOL,
	 NULL, NULL, NULL},
	{"offlinesync", "FALSE", &tmp_prefs.offlinesync, P_BOOL,
	 NULL, NULL, NULL},
	{NULL, NULL, NULL, P_OTHER, NULL, NULL, NULL}
};

static FolderItemPrefs *folder_item_prefs_clear(FolderItemPrefs *prefs);

void folder_item_prefs_read_config(FolderItem * item)
{
	gchar * id;
	gchar *rcpath;

	id = folder_item_get_identifier(item);
	folder_item_prefs_clear(&tmp_prefs);
	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, FOLDERITEM_RC, NULL);
	prefs_read_config(param, id, rcpath, NULL);
	g_free(id);
	g_free(rcpath);

	*item->prefs = tmp_prefs;

	/*
	 * MIGRATION: next lines are migration code. the idea is that
	 *            if used regularly, claws folder config ends up
	 *            in the same file as sylpheed-main
	 */

	item->ret_rcpt = tmp_prefs.request_return_receipt ? TRUE : FALSE;

	/* MIGRATION: 0.7.8main+ has persistent sort order. claws had the sort
	 *	      order in different members, which is ofcourse a little
	 *	      bit phoney. */
	if (item->sort_key == SORT_BY_NONE) {
		item->sort_key  = (tmp_prefs.sort_by_number  ? SORT_BY_NUMBER  :
				   tmp_prefs.sort_by_size    ? SORT_BY_SIZE    :
				   tmp_prefs.sort_by_date    ? SORT_BY_DATE    :
				   tmp_prefs.sort_by_from    ? SORT_BY_FROM    :
				   tmp_prefs.sort_by_subject ? SORT_BY_SUBJECT :
				   tmp_prefs.sort_by_score   ? SORT_BY_SCORE   :
								 SORT_BY_NONE);
		item->sort_type = tmp_prefs.sort_descending ? SORT_DESCENDING : SORT_ASCENDING;
	}								
}

void folder_item_prefs_save_config(FolderItem * item)
{	
	gchar * id;

	tmp_prefs = * item->prefs;

	id = folder_item_get_identifier(item);

	prefs_write_config(param, id, FOLDERITEM_RC);
	g_free(id);

	/* MIGRATION: make sure migrated items are not saved
	 */
}

void folder_item_prefs_set_config(FolderItem * item,
				  int sort_type, gint sort_mode)
{
	g_assert(item);
	g_warning("folder_item_prefs_set_config() should never be called\n");
	item->sort_key  = sort_type;
	item->sort_type = sort_mode;
}

static FolderItemPrefs *folder_item_prefs_clear(FolderItemPrefs *prefs)
{
	prefs->sort_by_number = FALSE;
	prefs->sort_by_size = FALSE;
	prefs->sort_by_date = FALSE;
	prefs->sort_by_from = FALSE;
	prefs->sort_by_subject = FALSE;
	prefs->sort_by_score = FALSE;
	prefs->sort_descending = FALSE;

	prefs->request_return_receipt = FALSE;
	prefs->enable_default_to = FALSE;
	prefs->default_to = NULL;
	prefs->enable_default_reply_to = FALSE;
	prefs->default_reply_to = NULL;
	prefs->enable_simplify_subject = FALSE;
	prefs->simplify_subject_regexp = NULL;
	prefs->enable_folder_chmod = FALSE;
	prefs->folder_chmod = 0;
	prefs->enable_default_account = FALSE;
	prefs->default_account = 0;
#if USE_ASPELL
	prefs->enable_default_dictionary = FALSE;
	prefs->default_dictionary = NULL;
	prefs->enable_default_alt_dictionary = FALSE;
	prefs->default_alt_dictionary = NULL;
#endif
	prefs->save_copy_to_folder = FALSE;
	prefs->color = 0;

        prefs->enable_processing = TRUE;
	prefs->processing = NULL;

	prefs->newmailcheck = TRUE;
	prefs->offlinesync = FALSE;

	return prefs;
}

FolderItemPrefs * folder_item_prefs_new(void)
{
	FolderItemPrefs * prefs;

	prefs = g_new0(FolderItemPrefs, 1);

	return folder_item_prefs_clear(prefs);
}

void folder_item_prefs_free(FolderItemPrefs * prefs)
{
	g_free(prefs->default_to);
	g_free(prefs->default_reply_to);
	g_free(prefs);
}

gint folder_item_prefs_get_sort_mode(FolderItem * item)
{
	g_assert(item != NULL);
	g_warning("folder_item_prefs_get_sort_mode() should never be called\n");
	return item->sort_key;
}

gint folder_item_prefs_get_sort_type(FolderItem * item)
{
	g_assert(item != NULL);
	g_warning("folder_item_prefs_get_sort_type() should never be called\n");
	return item->sort_type;
}

#define SAFE_STRING(str) \
	(str) ? (str) : ""

void folder_item_prefs_copy_prefs(FolderItem * src, FolderItem * dest)
{
	GSList *tmp_prop_list = NULL, *tmp;
	folder_item_prefs_read_config(src);

	tmp_prefs.directory			= g_strdup(src->prefs->directory);
	tmp_prefs.sort_by_number		= src->prefs->sort_by_number;
	tmp_prefs.sort_by_size			= src->prefs->sort_by_size;
	tmp_prefs.sort_by_date			= src->prefs->sort_by_date;
	tmp_prefs.sort_by_from			= src->prefs->sort_by_from;
	tmp_prefs.sort_by_subject		= src->prefs->sort_by_subject;
	tmp_prefs.sort_by_score			= src->prefs->sort_by_score;
	tmp_prefs.sort_descending		= src->prefs->sort_descending;
	tmp_prefs.enable_thread			= src->prefs->enable_thread;
        tmp_prefs.enable_processing             = src->prefs->enable_processing;
	tmp_prefs.newmailcheck                  = src->prefs->newmailcheck;
	tmp_prefs.offlinesync                  = src->prefs->offlinesync;

	prefs_matcher_read_config();

	for (tmp = src->prefs->processing; tmp != NULL && tmp->data != NULL;) {
		FilteringProp *prop = (FilteringProp *)tmp->data;
		
		tmp_prop_list = g_slist_append(tmp_prop_list,
					   filteringprop_copy(prop));
		tmp = tmp->next;
	}
	tmp_prefs.processing			= tmp_prop_list;
	
	tmp_prefs.request_return_receipt	= src->prefs->request_return_receipt;
	tmp_prefs.enable_default_to		= src->prefs->enable_default_to;
	tmp_prefs.default_to			= g_strdup(src->prefs->default_to);
	tmp_prefs.enable_default_reply_to	= src->prefs->enable_default_reply_to;
	tmp_prefs.default_reply_to		= g_strdup(src->prefs->default_reply_to);
	tmp_prefs.enable_simplify_subject	= src->prefs->enable_simplify_subject;
	tmp_prefs.simplify_subject_regexp	= g_strdup(src->prefs->simplify_subject_regexp);
	tmp_prefs.enable_folder_chmod		= src->prefs->enable_folder_chmod;
	tmp_prefs.folder_chmod			= src->prefs->folder_chmod;
	tmp_prefs.enable_default_account	= src->prefs->enable_default_account;
	tmp_prefs.default_account		= src->prefs->default_account;
#if USE_ASPELL
	tmp_prefs.enable_default_dictionary	= src->prefs->enable_default_dictionary;
	tmp_prefs.default_dictionary		= g_strdup(src->prefs->default_dictionary);
	tmp_prefs.enable_default_alt_dictionary	= src->prefs->enable_default_alt_dictionary;
	tmp_prefs.default_alt_dictionary	= g_strdup(src->prefs->default_alt_dictionary);
#endif
	tmp_prefs.save_copy_to_folder		= src->prefs->save_copy_to_folder;
	tmp_prefs.color				= src->prefs->color;

	*dest->prefs = tmp_prefs;
	folder_item_prefs_save_config(dest);
}
