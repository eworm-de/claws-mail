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
#include <gtk/gtkwidget.h>
#include <gtk/gtkpixmap.h>
#include <string.h>
#include <dirent.h>

#include "stock_pixmap.h"
#include "gtkutils.h"
#include "utils.h"
#include "prefs_common.h"

#include "pixmaps/address.xpm"
#include "pixmaps/book.xpm"
#include "pixmaps/category.xpm"
#include "pixmaps/checkbox_off.xpm"
#include "pixmaps/checkbox_on.xpm"
#include "pixmaps/clip.xpm"
#include "pixmaps/clipkey.xpm"
#include "pixmaps/complete.xpm"
#include "pixmaps/continue.xpm"
#include "pixmaps/deleted.xpm"
#include "pixmaps/dir_close.xpm"
#include "pixmaps/dir_open.xpm"
#include "pixmaps/dir_open_hrm.xpm"
#include "pixmaps/error.xpm"
#include "pixmaps/forwarded.xpm"
#include "pixmaps/group.xpm"
#include "pixmaps/inbox.xpm"
#include "pixmaps/inbox_hrm.xpm"
#include "pixmaps/interface.xpm"
#include "pixmaps/jpilot.xpm"
#include "pixmaps/key.xpm"
#include "pixmaps/ldap.xpm"
#include "pixmaps/linewrap.xpm"
#include "pixmaps/mark.xpm"
#include "pixmaps/locked.xpm"
#include "pixmaps/new.xpm"
#include "pixmaps/outbox.xpm"
#include "pixmaps/outbox_hrm.xpm"
#include "pixmaps/replied.xpm"
#include "pixmaps/close.xpm"
#include "pixmaps/down_arrow.xpm"
#include "pixmaps/up_arrow.xpm"
#include "pixmaps/exec.xpm"
#include "pixmaps/mail.xpm"
#include "pixmaps/mail_attach.xpm"
#include "pixmaps/mail_compose.xpm"
#include "pixmaps/mail_forward.xpm"
#include "pixmaps/mail_receive.xpm"
#include "pixmaps/mail_receive_all.xpm"
#include "pixmaps/mail_reply.xpm"
#include "pixmaps/mail_reply_to_all.xpm"
#include "pixmaps/mail_reply_to_author.xpm"
#include "pixmaps/mail_send.xpm"
#include "pixmaps/mail_send_queue.xpm"
#include "pixmaps/news_compose.xpm"
#include "pixmaps/paste.xpm"
#include "pixmaps/preferences.xpm"
#include "pixmaps/properties.xpm"
#include "pixmaps/sylpheed_logo.xpm"
#include "pixmaps/address_book.xpm"
#include "pixmaps/trash.xpm"
#include "pixmaps/trash_hrm.xpm"
#include "pixmaps/unread.xpm"
#include "pixmaps/vcard.xpm"
#include "pixmaps/ignorethread.xpm"
#include "pixmaps/work_online.xpm"
#include "pixmaps/work_offline.xpm"
#include "pixmaps/notice_warn.xpm"
#include "pixmaps/notice_error.xpm"
#include "pixmaps/notice_note.xpm"
#include "pixmaps/quicksearch.xpm"

typedef struct _StockPixmapData	StockPixmapData;

struct _StockPixmapData
{
	gchar **data;
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	gchar *file;
	gchar *icon_path;
};

static void stock_pixmap_find_themes_in_dir(GList **list, const gchar *dirname);

static StockPixmapData pixmaps[] =
{
	{address_xpm			, NULL, NULL, "address", "  "},
	{address_book_xpm		, NULL, NULL, "address_book", "  "},
	{book_xpm				, NULL, NULL, "book", "  "},
	{category_xpm			, NULL, NULL, "category", "  "},
	{checkbox_off_xpm		, NULL, NULL, "checkbox_off", "  "},
	{checkbox_on_xpm		, NULL, NULL, "checkbox_on", "  "},
	{clip_xpm				, NULL, NULL, "clip", "  "},
	{clipkey_xpm			, NULL, NULL, "clipkey", "  "},
	{close_xpm				, NULL, NULL, "close", "  "},
	{complete_xpm			, NULL, NULL, "complete", "  "},
	{continue_xpm			, NULL, NULL, "continue", "  "},
	{deleted_xpm			, NULL, NULL, "deleted", "  "},
	{dir_close_xpm			, NULL, NULL, "dir_close", "  "},
	{dir_close_xpm			, NULL, NULL, "dir_close_hrm", " "},
	{dir_open_xpm			, NULL, NULL, "dir_open", "  "},
	{dir_open_hrm_xpm		, NULL, NULL, "dir_open_hrm", "  "},
	{down_arrow_xpm			, NULL, NULL, "down_arrow", "  "},
	{up_arrow_xpm			, NULL, NULL, "up_arrow", "  "},
	{mail_compose_xpm		, NULL, NULL, "edit_extern", "  "},
	{error_xpm				, NULL, NULL, "error", "  "},
	{exec_xpm				, NULL, NULL, "exec", "  "},
	{forwarded_xpm			, NULL, NULL, "forwarded", "  "},
	{group_xpm				, NULL, NULL, "group", "  "},
	{ignorethread_xpm		, NULL, NULL, "ignorethread", "  "},
	{inbox_xpm				, NULL, NULL, "inbox_close", "  "},
	{inbox_hrm_xpm			, NULL, NULL, "inbox_close_hrm", "  "},
	{inbox_xpm				, NULL, NULL, "inbox_open", "  "},
	{inbox_hrm_xpm			, NULL, NULL, "inbox_open_hrm", "  "},
	{paste_xpm				, NULL, NULL, "insert_file", "  "},
	{interface_xpm			, NULL, NULL, "interface", "  "},
	{jpilot_xpm				, NULL, NULL, "jpilot", "  "},
	{key_xpm				, NULL, NULL, "key", "  "},
	{ldap_xpm				, NULL, NULL, "ldap", "  "},
	{linewrap_xpm			, NULL, NULL, "linewrap", "  "},
	{locked_xpm				, NULL, NULL, "locked", "  "},
	{mail_xpm				, NULL, NULL, "mail", "  "},
	{mail_attach_xpm		, NULL, NULL, "mail_attach", "  "},
	{mail_compose_xpm		, NULL, NULL, "mail_compose", "  "},
	{mail_forward_xpm		, NULL, NULL, "mail_forward", "  "},
	{mail_receive_xpm		, NULL, NULL, "mail_receive", "  "},
	{mail_receive_all_xpm	, NULL, NULL, "mail_receive_all", "  "},
	{mail_reply_xpm			, NULL, NULL, "mail_reply", "  "},
	{mail_reply_to_all_xpm	, NULL, NULL, "mail_reply_to_all", "  "},
	{mail_reply_to_author_xpm
							, NULL, NULL, "mail_reply_to_author", "  "},
	{mail_send_xpm			, NULL, NULL, "mail_send", "  "},
	{mail_send_queue_xpm	, NULL, NULL, "mail_send_queue", "  "},
	{mail_xpm				, NULL, NULL, "mail_sign", "  "},
	{mark_xpm				, NULL, NULL, "mark", "  "},
	{new_xpm				, NULL, NULL, "new", "  "},
	{news_compose_xpm		, NULL, NULL, "news_compose", "  "},
	{outbox_xpm				, NULL, NULL, "outbox_close", "  "},
	{outbox_hrm_xpm			, NULL, NULL, "outbox_close_hrm", "  "},
	{outbox_xpm				, NULL, NULL, "outbox_open", "  "},
	{outbox_hrm_xpm			, NULL, NULL, "outbox_open_hrm", "  "},
	{replied_xpm			, NULL, NULL, "replied", "  "},
	{paste_xpm				, NULL, NULL, "paste", "  "},
	{preferences_xpm		, NULL, NULL, "preferences", "  "},
	{properties_xpm			, NULL, NULL, "properties", "  "},
	{outbox_xpm				, NULL, NULL, "queue_close", "  "},
	{outbox_hrm_xpm			, NULL, NULL, "queue_close_hrm", "  "},
	{outbox_xpm				, NULL, NULL, "queue_open", "  "},
	{outbox_hrm_xpm			, NULL, NULL, "queue_open_hrm", "  "},
	{sylpheed_logo_xpm		, NULL, NULL, "sylpheed_logo", "  "},
	{trash_xpm				, NULL, NULL, "trash_open", "  "},
	{trash_hrm_xpm			, NULL, NULL, "trash_open_hrm", "  "},
	{trash_xpm				, NULL, NULL, "trash_close", "  "},
	{trash_hrm_xpm			, NULL, NULL, "trash_close_hrm", "  "},
	{unread_xpm				, NULL, NULL, "unread", "  "},
	{vcard_xpm				, NULL, NULL, "vcard", "  "},
	{work_online_xpm			, NULL, NULL, "work_online", "  "},
	{work_offline_xpm			, NULL, NULL, "work_offline", "  "},
	{notice_warn_xpm			, NULL, NULL, "notice_warn",  "  "},
	{notice_error_xpm			, NULL, NULL, "notice_error",  "  "},
	{notice_note_xpm			, NULL, NULL, "notice_note",  "  "},
	{quicksearch_xpm			, NULL, NULL, "quicksearch",  "  "},
};

/* return newly constructed GtkPixmap from GdkPixmap */
GtkWidget *stock_pixmap_widget(GtkWidget *window, StockPixmap icon)
{
	GdkPixmap *pixmap;
	GdkBitmap *mask;

	g_return_val_if_fail(window != NULL, NULL);
	g_return_val_if_fail(icon >= 0 && icon < N_STOCK_PIXMAPS, NULL);

	if (stock_pixmap_gdk(window, icon, &pixmap, &mask) != -1)
		return gtk_pixmap_new(pixmap, mask);
	
	return NULL;
}

/* create GdkPixmap if it has not created yet */
gint stock_pixmap_gdk(GtkWidget *window, StockPixmap icon,
		      GdkPixmap **pixmap, GdkBitmap **mask)
{
	StockPixmapData *pix_d;

	if (pixmap) *pixmap = NULL;
	if (mask)   *mask   = NULL;

	g_return_val_if_fail(window != NULL, -1);
	g_return_val_if_fail(icon >= 0 && icon < N_STOCK_PIXMAPS, -1);

	pix_d = &pixmaps[icon];

	if (!pix_d->pixmap || (strcmp(pix_d->icon_path, prefs_common.pixmap_theme_path) != 0)) {
		GdkPixmap *pix = NULL;
	
		if (strcmp(prefs_common.pixmap_theme_path, DEFAULT_PIXMAP_THEME) != 0) {
			if ( is_dir_exist(prefs_common.pixmap_theme_path) ) {
				char *icon_file_name; 
				
				icon_file_name = g_strconcat(prefs_common.pixmap_theme_path,
							     G_DIR_SEPARATOR_S,
							     pix_d->file,
							     ".xpm",
							     NULL);
				if (is_file_exist(icon_file_name))
					PIXMAP_CREATE_FROM_FILE(window, pix, pix_d->mask, icon_file_name);
				if (pix) 
					pix_d->icon_path = prefs_common.pixmap_theme_path;
				g_free(icon_file_name);
			} else {
				/* even the path does not exist (deleted between two sessions), so
				set the preferences to the internal theme */
				prefs_common.pixmap_theme_path = DEFAULT_PIXMAP_THEME;
			}
		}
		pix_d->pixmap = pix;
	}

	if (!pix_d->pixmap) {
		PIXMAP_CREATE(window, pix_d->pixmap, pix_d->mask, pix_d->data);
		if (pix_d->pixmap) 
			pix_d->icon_path = DEFAULT_PIXMAP_THEME;	
	}

	g_return_val_if_fail(pix_d->pixmap != NULL, -1);
	
	if (pixmap) 
		*pixmap = pix_d->pixmap;
	if (mask)   
		*mask   = pix_d->mask;

	return 0;
}

static void stock_pixmap_find_themes_in_dir(GList **list, const gchar *dirname)
{
	struct dirent *d;
	DIR *dp;
	
	if ((dp = opendir(dirname)) == NULL) {
		FILE_OP_ERROR(dirname, "opendir");
		return;
	}
	
	while ((d = readdir(dp)) != NULL) {
		gchar *entry;
		gchar *fullentry;

		entry     = d->d_name;
		fullentry = g_strconcat(dirname, G_DIR_SEPARATOR_S, entry, NULL);
		
		if (strcmp(entry, ".") != 0 && strcmp(entry, "..") != 0 && is_dir_exist(fullentry)) {
			gchar *filetoexist;
			int i;
			
			for (i = 0; i < N_STOCK_PIXMAPS; i++) {
				filetoexist = g_strconcat(fullentry, G_DIR_SEPARATOR_S, pixmaps[i].file, ".xpm", NULL);
				if (is_file_exist(filetoexist)) {
					*list = g_list_append(*list, fullentry);
					break;
				}
			}
			g_free(filetoexist);
			if (i == N_STOCK_PIXMAPS) 
				g_free(fullentry);
		} else 
			g_free(fullentry);
	}
	closedir(dp);
}

GList *stock_pixmap_themes_list_new(void)
{
	gchar *defaulttheme;
	gchar *userthemes;
	gchar *systemthemes;
	GList *list = NULL;
	
	defaulttheme = g_strdup(DEFAULT_PIXMAP_THEME);
	userthemes   = g_strconcat(get_home_dir(), G_DIR_SEPARATOR_S,
				   RC_DIR, G_DIR_SEPARATOR_S, 
				   PIXMAP_THEME_DIR, NULL);
	systemthemes = g_strconcat(PACKAGE_DATA_DIR, G_DIR_SEPARATOR_S,
				   PIXMAP_THEME_DIR, NULL);

	list = g_list_append(list, defaulttheme);
	
	stock_pixmap_find_themes_in_dir(&list, userthemes);
	stock_pixmap_find_themes_in_dir(&list, systemthemes);

	g_free(userthemes);
	g_free(systemthemes);
	return list;
}

void stock_pixmap_themes_list_free(GList *list)
{
	GList *ptr;

	for (ptr = g_list_first(list); ptr != NULL; ptr = g_list_next(ptr)) 
		if (ptr->data)
			g_free(ptr->data);
	g_list_free(list);		
}

gchar *stock_pixmap_get_name (StockPixmap icon)
{
	g_return_val_if_fail(icon >= 0 && icon < N_STOCK_PIXMAPS, NULL);
	
	return pixmaps[icon].file;

}

StockPixmap stock_pixmap_get_icon (gchar *file)
{
	gint i;
	
	for (i = 0; i < N_STOCK_PIXMAPS; i++) {
		if (strcmp (pixmaps[i].file, file) == 0)
			return i;
	}
	return -1;
}
	
