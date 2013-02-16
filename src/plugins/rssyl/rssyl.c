/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2004 Hiroyuki Yamamoto
 * This file (C) 2005 Andrej Kacian <andrej@kacian.sk>
 *
 * - s-c folderclass callback handler functions
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
#include "claws-features.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>

#ifdef G_OS_WIN32
#  include <w32lib.h>
#endif

#include <glib.h>
#include <curl/curl.h>

#include "folder.h"
#include "localfolder.h"

#include "procheader.h"
#include "common/utils.h"
#include "toolbar.h"
#include "prefs_toolbar.h"

#include "main.h"

#include "feed.h"
#include "feedprops.h"
#include "opml.h"
#include "rssyl.h"
#include "rssyl_gtk.h"
#include "rssyl_prefs.h"
#include "strreplace.h"

static gint rssyl_create_tree(Folder *folder);

static gboolean existing_tree_found = FALSE;

static void rssyl_init_read_func(FolderItem *item, gpointer data)
{
	if( !IS_RSSYL_FOLDER_ITEM(item) )
		return;

	existing_tree_found = TRUE;

	if( folder_item_parent(item) == NULL )
		return;

	rssyl_get_feed_props((RSSylFolderItem *)item);
}

static void rssyl_make_rc_dir(void)
{
	gchar *rssyl_dir = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, RSSYL_DIR,
			NULL);

	if( !is_dir_exist(rssyl_dir) ) {
		if( make_dir(rssyl_dir) < 0 ) {
			g_warning("couldn't create directory %s\n", rssyl_dir);
		}

		debug_print("created directorty %s\n", rssyl_dir);
	}

	g_free(rssyl_dir);
}

static void rssyl_create_default_mailbox(void)
{
	Folder *root = NULL;
	FolderItem *item;

	rssyl_make_rc_dir();

	root = folder_new(rssyl_folder_get_class(), RSSYL_DEFAULT_MAILBOX, NULL);

	g_return_if_fail(root != NULL);
	folder_add(root);

	item = FOLDER_ITEM(root->node->data);

	rssyl_subscribe_new_feed(item, RSSYL_DEFAULT_FEED, TRUE);
}

static gboolean rssyl_refresh_all_feeds_deferred(gpointer data)
{
	rssyl_refresh_all_feeds();
	return FALSE;
}

static void rssyl_toolbar_cb_refresh_all(gpointer parent, const gchar *item_name, gpointer data)
{
	rssyl_refresh_all_feeds();
}

void rssyl_init(void)
{
	folder_register_class(rssyl_folder_get_class());

	rssyl_gtk_init();

	rssyl_make_rc_dir();

	rssyl_prefs_init();

	folder_func_to_all_folders((FolderItemFunc)rssyl_init_read_func, NULL);

	if( existing_tree_found == FALSE )
		rssyl_create_default_mailbox();

	prefs_toolbar_register_plugin_item(TOOLBAR_MAIN, "RSSyl",
			_("Refresh all feeds"), rssyl_toolbar_cb_refresh_all, NULL);

	rssyl_opml_export();

	if( rssyl_prefs_get()->refresh_on_startup &&
			claws_is_starting() )
		g_timeout_add(2000, rssyl_refresh_all_feeds_deferred, NULL);
}

void rssyl_done(void)
{
	prefs_toolbar_unregister_plugin_item(TOOLBAR_MAIN, "RSSyl",
			_("Refresh all feeds"));
	rssyl_prefs_done();
	rssyl_gtk_done();
	if (!claws_is_exiting())
		folder_unregister_class(rssyl_folder_get_class());
}

static gchar *rssyl_get_new_msg_filename(FolderItem *dest)
{
	gchar *destfile;
	gchar *destpath;

	destpath = folder_item_get_path(dest);
	g_return_val_if_fail(destpath != NULL, NULL);

	if( !is_dir_exist(destpath) )
		make_dir_hier(destpath);

	for( ; ; ) {
		destfile = g_strdup_printf("%s%c%d", destpath, G_DIR_SEPARATOR,
				dest->last_num + 1);
		if( is_file_entry_exist(destfile) ) {
			dest->last_num++;
			g_free(destfile);
		} else
			break;
	}

	g_free(destpath);

	return destfile;
}

static void rssyl_get_last_num(Folder *folder, FolderItem *item)
{
	gchar *path;
	DIR *dp;
	struct dirent *d;
	gint max = 0;
	gint num;

	g_return_if_fail(item != NULL);

	debug_print("rssyl_get_last_num(): Scanning %s ...\n", item->path);
	path = folder_item_get_path(item);
	g_return_if_fail(path != NULL);
	if( change_dir(path) < 0 ) {
		g_free(path);
		return;
	}
	g_free(path);

	if( (dp = opendir(".")) == NULL ) {
		FILE_OP_ERROR(item->path, "opendir");
		return;
	}

	while( (d = readdir(dp)) != NULL ) {
		if( (num = to_number(d->d_name)) > 0 && dirent_is_regular_file(d) ) {
			if( max < num )
				max = num;
		}
	}
	closedir(dp);

	debug_print("Last number in dir %s = %d\n", item->path, max);
	item->last_num = max;
}

struct _RSSylFolder {
	LocalFolder folder;
};

typedef struct _RSSylFolder RSSylFolder;

FolderClass rssyl_class;

static Folder *rssyl_new_folder(const gchar *name, const gchar *path)
{
	RSSylFolder *folder;

	debug_print("RSSyl: new_folder\n");

	rssyl_make_rc_dir();

	folder = g_new0(RSSylFolder, 1);
	FOLDER(folder)->klass = &rssyl_class;
	folder_init(FOLDER(folder), name);

	return FOLDER(folder);
}

static void rssyl_destroy_folder(Folder *_folder)
{
	RSSylFolder *folder = (RSSylFolder *)_folder;

	folder_local_folder_destroy(LOCAL_FOLDER(folder));
}

static gint rssyl_scan_tree(Folder *folder)
{
	g_return_val_if_fail(folder != NULL, -1);

	folder->outbox = NULL;
	folder->draft = NULL;
	folder->queue = NULL;
	folder->trash = NULL;

	debug_print("RSSyl: scanning tree\n");
	rssyl_create_tree(folder);

	return 0;
}

static gint rssyl_create_tree(Folder *folder)
{
	FolderItem *rootitem;
	GNode *rootnode;

	rssyl_make_rc_dir();

	if( !folder->node ) {
		rootitem = folder_item_new(folder, folder->name, NULL);
		rootitem->folder = folder;
		rootnode = g_node_new(rootitem);
		folder->node = rootnode;
		rootitem->node = rootnode;
	} else {
		rootitem = FOLDER_ITEM(folder->node->data);
		rootnode = folder->node;
	}

	debug_print("RSSyl: created new rssyl tree\n");
	return 0;
}

static FolderItem *rssyl_item_new(Folder *folder)
{
	RSSylFolderItem *ritem;

	debug_print("RSSyl: item_new\n");

	ritem = g_new0(RSSylFolderItem, 1);

	ritem->url = NULL;
	ritem->default_refresh_interval = TRUE;
	ritem->default_expired_num = TRUE;
	ritem->fetch_comments = FALSE;
	ritem->fetch_comments_for = -1;
	ritem->silent_update = 0;
	ritem->refresh_interval = rssyl_prefs_get()->refresh;
	ritem->refresh_id = 0;
	ritem->expired_num = rssyl_prefs_get()->expired;
	ritem->last_count = 0;

	ritem->contents = NULL;
	ritem->feedprop = NULL;

	return (FolderItem *)ritem;
}

static void rssyl_item_destroy(Folder *folder, FolderItem *item)
{
	RSSylFolderItem *ritem = (RSSylFolderItem *)item;

	g_return_if_fail(ritem != NULL);

	/* Silently remove feed refresh timeouts */
	if( ritem->refresh_id != 0 )
		g_source_remove(ritem->refresh_id);

	g_free(ritem->url);
	g_free(ritem->official_name);
	g_slist_free(ritem->contents);

	g_free(item);
}

static FolderItem *rssyl_create_folder(Folder *folder,
								FolderItem *parent, const gchar *name)
{
	gchar *path = NULL, *tmp;
	FolderItem *newitem = NULL;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(parent != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);
	tmp = rssyl_feed_title_to_dir((gchar *)name);
	path = g_strconcat((parent->path != NULL) ? parent->path : "", ".",
				tmp, NULL);
	g_free(tmp);
	newitem = folder_item_new(folder, name, path);
	folder_item_append(parent, newitem);
	g_free(path);

	return newitem;
}

static gchar *rssyl_item_get_path(Folder *folder, FolderItem *item)
{
	gchar *result, *tmp;
	tmp = rssyl_feed_title_to_dir(item->name);
	result = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, RSSYL_DIR,
			G_DIR_SEPARATOR_S, tmp, NULL);
	g_free(tmp);
	return result;
}

static gint rssyl_rename_folder(Folder *folder, FolderItem *item,
				const gchar *name)
{
	gchar *oldname = NULL, *oldpath = NULL, *newpath = NULL;
	RSSylFolderItem *ritem = NULL;
	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(item != NULL, -1);
	g_return_val_if_fail(item->path != NULL, -1);
	g_return_val_if_fail(name != NULL, -1);

	debug_print("RSSyl: renaming folder '%s' to '%s'\n", item->path, name);

	oldpath = rssyl_item_get_path(folder, item);
	
	/* now get the new path using the new name */
	oldname = item->name;
	item->name = g_strdup(name);
	newpath = rssyl_item_get_path(folder, item);
	
	/* put back the old name in case the rename fails */
	g_free(item->name);
	item->name = oldname;
	
	if (g_rename(oldpath, newpath) < 0) {
		FILE_OP_ERROR(oldpath, "rename");
		g_free(oldpath);
		g_free(newpath);
		return -1;
	}
	
	g_free(item->path);
	item->path = g_strdup_printf(".%s", name);
	
	ritem = (RSSylFolderItem *)item;
	
	if (ritem->url)
		rssyl_props_update_name(ritem, (gchar *)name);
	
	g_free(item->name);
	item->name = g_strdup(name);
	
	folder_write_list();

	return 0;
}

static gint rssyl_remove_folder(Folder *folder, FolderItem *item)
{
	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(item != NULL, -1);
	g_return_val_if_fail(item->path != NULL, -1);
	g_return_val_if_fail(item->stype == F_NORMAL, -1);

	debug_print("RSSyl: removing folder item %s\n", item->path);

	folder_item_remove(item);

	return 0;
}

static gint rssyl_get_num_list(Folder *folder, FolderItem *item,
		MsgNumberList **list, gboolean *old_uids_valid)
{
	gchar *path;
	DIR *dp;
	struct dirent *d;
	gint num, nummsgs = 0;
	RSSylFolderItem *ritem = (RSSylFolderItem *)item;

	g_return_val_if_fail(item != NULL, -1);

	debug_print("RSSyl: scanning '%s'...\n", item->path);

	rssyl_get_feed_props(ritem);

	if (ritem->url == NULL)
		return -1;

	*old_uids_valid = TRUE;

	path = folder_item_get_path(item);
	g_return_val_if_fail(path != NULL, -1);
	if( change_dir(path) < 0 ) {
		g_free(path);
		return -1;
	}
	g_free(path);

	if( (dp = opendir(".")) == NULL ) {
		FILE_OP_ERROR(item->path, "opendir");
		return -1;
	}

	while( (d = readdir(dp)) != NULL ) {
		if( (num = to_number(d->d_name)) > 0 ) {
			*list = g_slist_prepend(*list, GINT_TO_POINTER(num));
			nummsgs++;
		}
	}
	closedir(dp);

	return nummsgs;
}

static gboolean rssyl_scan_required(Folder *folder, FolderItem *item)
{
	return TRUE;
}

static gchar *rssyl_fetch_msg(Folder *folder, FolderItem *item, gint num)
{
	gchar *snum = g_strdup_printf("%d", num);
	gchar *tmp = rssyl_feed_title_to_dir(item->name);
	gchar *file = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, RSSYL_DIR,
			G_DIR_SEPARATOR_S, tmp,
			G_DIR_SEPARATOR_S, snum, NULL);
	g_free(tmp);
	debug_print("RSSyl: fetch_msg: '%s'\n", file);

	g_free(snum);

	return file;
}

static MsgInfo *rssyl_get_msginfo(Folder *folder, FolderItem *item, gint num)
{
	MsgInfo *msginfo = NULL;
	gchar *file;
	MsgFlags flags;

	debug_print("RSSyl: get_msginfo: %d\n", num);

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(num > 0, NULL);

	file = rssyl_fetch_msg(folder, item, num);
	g_return_val_if_fail(file != NULL, NULL);

	flags.perm_flags = MSG_NEW | MSG_UNREAD;
	flags.tmp_flags = 0;

	msginfo = rssyl_parse_feed_item_to_msginfo(file, flags, TRUE, TRUE, item);

	if( msginfo )
		msginfo->msgnum = num;

	g_free(file);

	return msginfo;
}

static gint rssyl_add_msgs(Folder *folder, FolderItem *dest, GSList *file_list,
		GHashTable *relation)
{
	gchar *destfile;
	GSList *cur;
	MsgFileInfo *fileinfo;

	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(file_list != NULL, -1);

	if( dest->last_num < 0 ) {
		rssyl_get_last_num(folder, dest);
		if( dest->last_num < 0 ) return -1;
	}

	for( cur = file_list; cur != NULL; cur = cur->next ) {
		fileinfo = (MsgFileInfo *)cur->data;

		destfile = rssyl_get_new_msg_filename(dest);
		g_return_val_if_fail(destfile != NULL, -1);

#ifdef G_OS_UNIX
		if( link(fileinfo->file, destfile) < 0 )
#endif
			if( copy_file(fileinfo->file, destfile, TRUE) < 0 ) {
				g_warning("can't copy message %s to %s\n", fileinfo->file, destfile);
				g_free(destfile);
				return -1;
			}

		if( relation != NULL )
			g_hash_table_insert(relation, fileinfo,
					GINT_TO_POINTER(dest->last_num + 1) );
		g_free(destfile);
		dest->last_num++;
	}

	return dest->last_num;
}

static gint rssyl_add_msg(Folder *folder, FolderItem *dest, const gchar *file,
		MsgFlags *flags)
{
	gint ret;
	GSList file_list;
	MsgFileInfo fileinfo;

	g_return_val_if_fail(file != NULL, -1);

	fileinfo.msginfo = NULL;
	fileinfo.file = (gchar *)file;
	fileinfo.flags = flags;
	file_list.data = &fileinfo;
	file_list.next = NULL;

	ret = rssyl_add_msgs(folder, dest, &file_list, NULL);
	return ret;
}

static gint rssyl_dummy_copy_msg(Folder *folder, FolderItem *dest, MsgInfo *info)
{
	if (info->folder == NULL || info->folder->folder != dest->folder) {
		return -1;
	}
	if (info->folder && info->folder->name && dest->name
	&&  !strcmp(info->folder->name, dest->name)) {
		/* this is a folder move */
		gchar *file = procmsg_get_message_file(info);
		gchar *tmp = g_strdup_printf("%s.tmp", file);
		copy_file(file, tmp, TRUE);
		g_free(file);
		g_free(tmp);
		return info->msgnum;
	} else {
		return -1;
	}
}
static gint rssyl_remove_msg(Folder *folder, FolderItem *item, gint num)
{
	gboolean need_scan = FALSE;
	gchar *file, *tmp;

	g_return_val_if_fail(item != NULL, -1);

	file = rssyl_fetch_msg(folder, item, num);
	g_return_val_if_fail(file != NULL, -1);

	need_scan = rssyl_scan_required(folder, item);

	/* are we doing a folder move ? */
	tmp = g_strdup_printf("%s.tmp", file);
	if (is_file_exist(tmp)) {
		claws_unlink(tmp);
		g_free(tmp);
		g_free(file);
		return 0;
	}
	g_free(tmp);
	if( claws_unlink(file) < 0 ) {
		FILE_OP_ERROR(file, "unlink");
		g_free(file);
		return -1;
	}

	if( !need_scan )
		item->mtime = time(NULL);

	g_free(file);
	return 0;
}

static gboolean rssyl_subscribe_uri(Folder *folder, const gchar *uri)
{
	if (folder->klass != rssyl_folder_get_class())
		return FALSE;
	
	if( rssyl_subscribe_new_feed(
				FOLDER_ITEM(folder->node->data), uri, FALSE) != NULL )
		return TRUE;
	return FALSE;
}

/************************************************************************/

FolderClass *rssyl_folder_get_class()
{
	if( rssyl_class.idstr == NULL ) {
		rssyl_class.type = F_UNKNOWN;
		rssyl_class.idstr = "rssyl";
		rssyl_class.uistr = "RSSyl";

		/* Folder functions */
		rssyl_class.new_folder = rssyl_new_folder;
		rssyl_class.destroy_folder = rssyl_destroy_folder;
		rssyl_class.set_xml = folder_set_xml;
		rssyl_class.get_xml = folder_get_xml;
		rssyl_class.scan_tree = rssyl_scan_tree;
		rssyl_class.create_tree = rssyl_create_tree;

		/* FolderItem functions */
		rssyl_class.item_new = rssyl_item_new;
		rssyl_class.item_destroy = rssyl_item_destroy;
		rssyl_class.item_get_path = rssyl_item_get_path;
		rssyl_class.create_folder = rssyl_create_folder;
		rssyl_class.rename_folder = rssyl_rename_folder;
		rssyl_class.remove_folder = rssyl_remove_folder;
		rssyl_class.get_num_list = rssyl_get_num_list;
		rssyl_class.scan_required = rssyl_scan_required;

		/* Message functions */
		rssyl_class.get_msginfo = rssyl_get_msginfo;
		rssyl_class.fetch_msg = rssyl_fetch_msg;
		rssyl_class.copy_msg = rssyl_dummy_copy_msg;
		rssyl_class.add_msg = rssyl_add_msg;
		rssyl_class.add_msgs = rssyl_add_msgs;
		rssyl_class.remove_msg = rssyl_remove_msg;
		rssyl_class.remove_msgs = NULL;
//		rssyl_class.change_flags = rssyl_change_flags;
		rssyl_class.change_flags = NULL;
		rssyl_class.subscribe = rssyl_subscribe_uri;
		debug_print("RSSyl: registered folderclass\n");
	}

	return &rssyl_class;
}
