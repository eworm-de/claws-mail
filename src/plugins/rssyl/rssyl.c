/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2005-2023 the Claws Mail Team and Andrej Kacian <andrej@kacian.sk>
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

/* Global includes */
#include <glib.h>
#include <glib/gi18n.h>
#include <curl/curl.h>

/* Claws Mail includes */
#include <folder.h>
#include <procmsg.h>
#include <main.h>
#include <mh.h>
#include <xml.h>
#include <toolbar.h>
#include <prefs_common.h>
#include <prefs_toolbar.h>
#include <utils.h>
#include <file-utils.h>

/* Local includes */
#include "libfeed/feeditem.h"
#include "rssyl.h"
#include "rssyl_deleted.h"
#include "rssyl_gtk.h"
#include "rssyl_feed.h"
#include "rssyl_prefs.h"
#include "rssyl_subscribe.h"
#include "rssyl_update_feed.h"
#include "rssyl_update_format.h"
#include "opml_import.h"
#include "opml_export.h"
#include "strutils.h"

FolderClass rssyl_class;

static gint rssyl_create_tree(Folder *folder);
static gint rssyl_scan_tree(Folder *folder);

static gboolean existing_tree_found = FALSE;

static void rssyl_init_read_func(FolderItem *item, gpointer data)
{
	RFolderItem *ritem = (RFolderItem *)item;
	RPrefs *rsprefs = NULL;

	if( !IS_RSSYL_FOLDER_ITEM(item) )
		return;

	existing_tree_found = TRUE;

	/* Don't do anything if we're on root of our folder tree or on
	 * a regular folder (no feed) */
	if( folder_item_parent(item) == NULL || ritem->url == NULL )
		return;

	ritem->refresh_id = 0;

	/* Start automatic refresh timer, if necessary */
	if( ritem->default_refresh_interval ) {
		rsprefs = rssyl_prefs_get();
		if( !rsprefs->refresh_enabled )
			return;

		ritem->refresh_interval = rsprefs->refresh;
	}

	/* Start the timer, if determined interval is >0 */
	if( ritem->refresh_interval > 0 )
		rssyl_feed_start_refresh_timeout(ritem);
}

static void rssyl_make_rc_dir(void)
{
	gchar *rssyl_dir = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, RSSYL_DIR,
			NULL);

	if( !is_dir_exist(rssyl_dir) ) {
		if( make_dir(rssyl_dir) < 0 ) {
			g_warning("couldn't create directory %s", rssyl_dir);
		}

		debug_print("RSSyl: created directory %s\n", rssyl_dir);
	}

	g_free(rssyl_dir);
}

static void rssyl_create_default_mailbox(void)
{
	Folder *root = NULL;

	rssyl_make_rc_dir();

	root = folder_new(rssyl_folder_get_class(), RSSYL_DEFAULT_MAILBOX, NULL);

	g_return_if_fail(root != NULL);
	folder_add(root);

	rssyl_scan_tree(root);

	/* FIXME: subscribe default feed */
//	rssyl_subscribe_new_feed(item, RSSYL_DEFAULT_FEED, TRUE);
}

static gboolean rssyl_update_all_feeds_deferred(gpointer data)
{
	rssyl_update_all_feeds();
	return FALSE;
}

static void rssyl_toolbar_cb_refresh_all_feeds(gpointer parent, const gchar *item_name, gpointer data)
{
	rssyl_update_all_feeds();
}

void rssyl_init(void)
{
	folder_register_class(rssyl_folder_get_class());

	rssyl_gtk_init();
	rssyl_make_rc_dir();

	rssyl_prefs_init();

	folder_func_to_all_folders((FolderItemFunc)rssyl_init_read_func, NULL);

	if( !existing_tree_found )
		rssyl_create_default_mailbox();
	else
		rssyl_update_format();

	prefs_toolbar_register_plugin_item(TOOLBAR_MAIN, PLUGIN_NAME, _("Refresh all feeds"), rssyl_toolbar_cb_refresh_all_feeds, NULL);

	if( rssyl_prefs_get()->refresh_on_startup &&
			!prefs_common_get_prefs()->work_offline &&
			claws_is_starting() )
		g_timeout_add(2000, rssyl_update_all_feeds_deferred, NULL);
}

void rssyl_done(void)
{
	rssyl_opml_export();

	prefs_toolbar_unregister_plugin_item(TOOLBAR_MAIN, PLUGIN_NAME, _("Refresh all feeds"));

	rssyl_prefs_done();
	rssyl_gtk_done();

	if( !claws_is_exiting() )
		folder_unregister_class(rssyl_folder_get_class());

	debug_print("RSSyl is done\n");
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
	const char *f;
	GDir *dp;
	GError *error = NULL;
	gint max = 0;
	gint num;

	g_return_if_fail(item != NULL);

	debug_print("rssyl_get_last_num(): Scanning %s ...\n", item->path);
	path = folder_item_get_path(item);
	g_return_if_fail(path != NULL);

	if( (dp = g_dir_open(path, 0, &error)) == NULL ) {
		FILE_OP_ERROR(item->path, "g_dir_open");
		debug_print("g_dir_open() failed on \"%s\", error %d (%s).\n",
				path, error->code, error->message);
		g_error_free(error);
		g_free(path);
		return;
	}

	g_free(path);

	while( (f = g_dir_read_name(dp)) != NULL) {
		if ((num = to_number(f)) > 0 &&
				g_file_test(f, G_FILE_TEST_IS_REGULAR)) {
			if( max < num )
				max = num;
		}
	}
	g_dir_close(dp);

	debug_print("Last number in dir %s = %d\n", item->path, max);
	item->last_num = max;
}

static Folder *rssyl_new_folder(const gchar *name, const gchar *path)
{
	Folder *folder;

	debug_print("RSSyl: new_folder: %s (%s)\n", name, path);

	rssyl_make_rc_dir();

	folder = g_new0(Folder, 1);
	FOLDER(folder)->klass = &rssyl_class;
	folder_init(FOLDER(folder), name);

	return FOLDER(folder);
}

static void rssyl_destroy_folder(Folder *folder)
{
}

static void rssyl_item_set_xml(Folder *folder, FolderItem *item, XMLTag *tag)
{
	GList *cur;
	RFolderItem *ritem = (RFolderItem *)item;

	folder_item_set_xml(folder, item, tag);

	for( cur = tag->attr; cur != NULL; cur = g_list_next(cur)) {
		XMLAttr *attr = (XMLAttr *) cur->data;

		if( !attr || !attr->name || !attr->value)
			continue;

		/* (str) URL */
		if( !strcmp(attr->name, "uri")) {
			g_free(ritem->url);
			ritem->url = g_strdup(attr->value);
		}
		/* (int) URL auth */
		if (!strcmp(attr->name, "auth")) {
			ritem->auth->type = atoi(attr->value);
		}
		/* (str) Auth user */
		if (!strcmp(attr->name, "auth_user")) {
			g_free(ritem->auth->username);
			ritem->auth->username = g_strdup(attr->value);
		}
		/* (str) Auth pass - save directly to password store */
		if (!strcmp(attr->name, "auth_pass")) {
			gsize len = 0;
			guchar *pwd = g_base64_decode(attr->value, &len);
			memset(attr->value, 0, strlen(attr->value));
			rssyl_passwd_set(ritem, (gchar *)pwd);
			memset(pwd, 0, strlen(pwd));
			g_free(pwd);
		}
		/* (str) Official title */
		if( !strcmp(attr->name, "official_title")) {
			g_free(ritem->official_title);
			ritem->official_title = g_strdup(attr->value);
		}
		/* (bool) Keep old items */
		if( !strcmp(attr->name, "keep_old"))
			ritem->keep_old = (atoi(attr->value) == 0 ? FALSE : TRUE );
		/* (bool) Use default refresh_interval */
		if( !strcmp(attr->name, "default_refresh_interval"))
			ritem->default_refresh_interval = (atoi(attr->value) == 0 ? FALSE : TRUE );
		/* (int) Refresh interval */
		if( !strcmp(attr->name, "refresh_interval"))
			ritem->refresh_interval = atoi(attr->value);
		/* (bool) Fetch comments */
		if( !strcmp(attr->name, "fetch_comments"))
			ritem->fetch_comments = (atoi(attr->value) == 0 ? FALSE : TRUE );
		/* (int) Max age of posts to fetch comments for */
		if( !strcmp(attr->name, "fetch_comments_max_age"))
			ritem->fetch_comments_max_age = atoi(attr->value);
		/* (bool) Write heading */
		if( !strcmp(attr->name, "write_heading"))
			ritem->write_heading = (atoi(attr->value) == 0 ? FALSE : TRUE );
		/* (int) Silent update */
		if( !strcmp(attr->name, "silent_update"))
			ritem->silent_update = atoi(attr->value);
		/* (bool) Ignore title rename */
		if( !strcmp(attr->name, "ignore_title_rename"))
			ritem->ignore_title_rename = (atoi(attr->value) == 0 ? FALSE : TRUE );
		/* (bool) Verify SSL peer  */
		if( !strcmp(attr->name, "ssl_verify_peer"))
			ritem->ssl_verify_peer = (atoi(attr->value) == 0 ? FALSE : TRUE );
	}
}

static XMLTag *rssyl_item_get_xml(Folder *folder, FolderItem *item)
{
	XMLTag *tag;
	RFolderItem *ri = (RFolderItem *)item;
	gchar *tmp = NULL;

	tag = folder_item_get_xml(folder, item);

	/* (str) URL */
	if( ri->url != NULL )
		xml_tag_add_attr(tag, xml_attr_new("uri", ri->url));
	/* (int) Auth */
	tmp = g_strdup_printf("%d", ri->auth->type);
	xml_tag_add_attr(tag, xml_attr_new("auth", tmp));
	g_free(tmp);
	/* (str) Auth user */
	if (ri->auth->username != NULL)
		xml_tag_add_attr(tag, xml_attr_new("auth_user", ri->auth->username));
	/* (str) Official title */
	if( ri->official_title != NULL )
		xml_tag_add_attr(tag, xml_attr_new("official_title", ri->official_title));
	/* (bool) Keep old items */
	xml_tag_add_attr(tag, xml_attr_new("keep_old",
				(ri->keep_old ? "1" : "0")) );
	/* (bool) Use default refresh interval */
	xml_tag_add_attr(tag, xml_attr_new("default_refresh_interval",
				(ri->default_refresh_interval ? "1" : "0")) );
	/* (int) Refresh interval */
	tmp = g_strdup_printf("%d", ri->refresh_interval);
	xml_tag_add_attr(tag, xml_attr_new("refresh_interval", tmp));
	g_free(tmp);
	/* (bool) Fetch comments */
	xml_tag_add_attr(tag, xml_attr_new("fetch_comments",
				(ri->fetch_comments ? "1" : "0")) );
	/* (int) Max age of posts to fetch comments for */
	tmp = g_strdup_printf("%d", ri->fetch_comments_max_age);
	xml_tag_add_attr(tag, xml_attr_new("fetch_comments_max_age", tmp));
	g_free(tmp);
	/* (bool) Write heading */
	xml_tag_add_attr(tag, xml_attr_new("write_heading",
				(ri->write_heading ? "1" : "0")) );
	/* (int) Silent update */
	tmp = g_strdup_printf("%d", ri->silent_update);
	xml_tag_add_attr(tag, xml_attr_new("silent_update", tmp));
	g_free(tmp);
	/* (bool) Ignore title rename */
	xml_tag_add_attr(tag, xml_attr_new("ignore_title_rename",
				(ri->ignore_title_rename ? "1" : "0")) );
	/* (bool) Verify SSL peer */
	xml_tag_add_attr(tag, xml_attr_new("ssl_verify_peer",
				(ri->ssl_verify_peer ? "1" : "0")) );

	return tag;
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

	g_return_val_if_fail(folder != NULL, -1);

	rssyl_make_rc_dir();

	if( !folder->node ) {
		rootitem = folder_item_new(folder, folder->name, NULL);
		rootitem->folder = folder;
		rootnode = g_node_new(rootitem);
		folder->node = rootnode;
		rootitem->node = rootnode;
	}

	debug_print("RSSyl: created new rssyl tree\n");
	return 0;
}

static FolderItem *rssyl_item_new(Folder *folder)
{
	RFolderItem *ritem = g_new0(RFolderItem, 1);

	ritem->url = NULL;
	ritem->auth = g_new0(FeedAuth, 1);
	ritem->auth->type = FEED_AUTH_NONE;
	ritem->auth->username = NULL;
	ritem->auth->password = NULL;
	ritem->official_title = NULL;
	ritem->source_id = NULL;
	ritem->items = NULL;
	ritem->deleted_items = NULL;
	ritem->keep_old = TRUE;
	ritem->default_refresh_interval = TRUE;
	ritem->refresh_interval = atoi(PREF_DEFAULT_REFRESH);
	ritem->fetch_comments = FALSE;
	ritem->fetch_comments_max_age = -1;
	ritem->write_heading = TRUE;
	ritem->fetching_comments = FALSE;
	ritem->silent_update = 0;
	ritem->last_update = 0;
	ritem->ignore_title_rename = FALSE;
	ritem->ssl_verify_peer = TRUE;
	ritem->feedprop = NULL;
	ritem->refresh_id = 0;

	return (FolderItem *)ritem;
}

static void rssyl_item_destroy(Folder *folder, FolderItem *item)
{
	RFolderItem *ritem = (RFolderItem *)item;

	g_return_if_fail(ritem != NULL);

	g_free(ritem->url);
	if (ritem->auth->username)
		g_free(ritem->auth->username);
	if (ritem->auth->password)
		g_free(ritem->auth->password);
	g_free(ritem->auth);
	g_free(ritem->official_title);
	g_slist_free(ritem->items);

	/* Remove a scheduled refresh, if any */
	if( ritem->refresh_id != 0)
		g_source_remove(ritem->refresh_id);

	g_free(ritem);
}

static FolderItem *rssyl_create_folder(Folder *folder,
								FolderItem *parent, const gchar *name)
{
	gchar *path = NULL, *basepath = NULL, *itempath = NULL;
	FolderItem *newitem = NULL;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(parent != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);

	path = folder_item_get_path(parent);
	if( !is_dir_exist(path) ) {
		if( (make_dir_hier(path) != 0) ) {
			debug_print("RSSyl: Couldn't create directory (rec) '%s'\n", path);
			return NULL;
		}
	}

	basepath = g_strdelimit(g_strdup(name), G_DIR_SEPARATOR_S, '_');
	path = g_strconcat(path, G_DIR_SEPARATOR_S, basepath, NULL);

	if( make_dir(path) < 0 ) {
		debug_print("RSSyl: Couldn't create directory '%s'\n", path);
		g_free(path);
		g_free(basepath);
		return NULL;
	}
	g_free(path);

	itempath = g_strconcat((parent->path ? parent->path : ""),
			G_DIR_SEPARATOR_S, basepath, NULL);
	newitem = folder_item_new(folder, name, itempath);
	g_free(itempath);
	g_free(basepath);

	folder_item_append(parent, newitem);

	return newitem;
}

FolderItem *rssyl_get_root_folderitem(FolderItem *item)
{
	FolderItem *i;

	for( i = item; folder_item_parent(i) != NULL; i = folder_item_parent(i) ) { }
	return i;
}

static gchar *rssyl_item_get_path(Folder *folder, FolderItem *item)
{
	gchar *path, *name;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(item != NULL, NULL);

	name = folder_item_get_name(rssyl_get_root_folderitem(item));
	path = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, RSSYL_DIR,
			G_DIR_SEPARATOR_S, name, item->path, NULL);
	g_free(name);

	return path;
}

static gboolean rssyl_rename_folder_func(GNode *node, gpointer data)
{
	FolderItem *item = node->data;
	gchar **paths = data;
	const gchar *oldpath = paths[0];
	const gchar *newpath = paths[1];
	gchar *base;
	gchar *new_itempath;
	gint oldpathlen;

	oldpathlen = strlen(oldpath);
	if (strncmp(oldpath, item->path, oldpathlen) != 0) {
		g_warning("path doesn't match: %s, %s", oldpath, item->path);
		return TRUE;
	}

	base = item->path + oldpathlen;
	while (*base == G_DIR_SEPARATOR) base++;
	if (*base == '\0')
		new_itempath = g_strdup(newpath);
	else
		new_itempath = g_strconcat(newpath, G_DIR_SEPARATOR_S, base,
				NULL);
	g_free(item->path);
	item->path = new_itempath;

	return FALSE;
}

static gint rssyl_rename_folder(Folder *folder, FolderItem *item,
				const gchar *name)
{
	gchar *oldpath;
	gchar *dirname;
	gchar *newpath, *utf8newpath;
	gchar *basenewpath;
	gchar *paths[2];

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(item != NULL, -1);
	g_return_val_if_fail(item->path != NULL, -1);
	g_return_val_if_fail(name != NULL, -1);

	debug_print("RSSyl: rssyl_rename_folder '%s' -> '%s'\n",
			item->name, name);

	if (!strcmp(item->name, name))
			return 0;

	oldpath = folder_item_get_path(item);
	if( !is_dir_exist(oldpath) )
		make_dir_hier(oldpath);

	dirname = g_path_get_dirname(oldpath);
	basenewpath = g_strdelimit(g_strdup(name), G_DIR_SEPARATOR_S, '_');
	newpath = g_strconcat(dirname, G_DIR_SEPARATOR_S, basenewpath, NULL);
	g_free(dirname);
	g_free(basenewpath);

	if( g_rename(oldpath, newpath) < 0 ) {
		FILE_OP_ERROR(oldpath, "rename");
		g_free(oldpath);
		g_free(newpath);
		return -1;
	}

	g_free(oldpath);
	g_free(newpath);

	if( strchr(item->path, G_DIR_SEPARATOR) != NULL ) {
		dirname = g_path_get_dirname(item->path);
		utf8newpath = g_strconcat(dirname, G_DIR_SEPARATOR_S, name, NULL);
		g_free(dirname);
	} else
		utf8newpath = g_strdup(name);

	g_free(item->name);
	item->name = g_strdup(name);

	paths[0] = g_strdup(item->path);
	paths[1] = utf8newpath;
	g_node_traverse(item->node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
			rssyl_rename_folder_func, paths);

	g_free(paths[0]);
	g_free(paths[1]);

	return 0;
}

static gint rssyl_remove_folder(Folder *folder, FolderItem *item)
{
	gchar *path = NULL;
	RFolderItem *ritem = (RFolderItem *)item;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(item != NULL, -1);
	g_return_val_if_fail(item->path != NULL, -1);
	g_return_val_if_fail(item->stype == F_NORMAL, -1);

	debug_print("RSSyl: removing folder item %s\n", item->path);

	path = folder_item_get_path(item);
	if( remove_dir_recursive(path) < 0 ) {
		g_warning("can't remove directory '%s'", path);
		g_free(path);
		return -1;
	}
	g_free(path);

	if (ritem->url != NULL)
		rssyl_passwd_set(ritem, NULL);

	folder_item_remove(item);

	return 0;
}

static gint rssyl_get_num_list(Folder *folder, FolderItem *item,
		MsgNumberList **list, gboolean *old_uids_valid)
{
	gchar *path;
	GDir *dp;
	const gchar *d;
	GError *error = NULL;
	gint num, nummsgs = 0;

	g_return_val_if_fail(item != NULL, -1);

	debug_print("RSSyl: get_num_list: scanning '%s'\n", item->path);

	*old_uids_valid = TRUE;
	
	path = folder_item_get_path(item);
	g_return_val_if_fail(path != NULL, -1);

	if( (dp = g_dir_open(path, 0, &error)) == NULL ) {
		debug_print("g_dir_open() failed on \"%s\", error %d (%s).\n",
				path, error->code, error->message);
		g_error_free(error);
		g_free(path);
		return -1;
	}

	g_free(path);

	while( (d = g_dir_read_name(dp)) != NULL ) {
		if( (num = to_number(d)) > 0 ) {
			*list = g_slist_prepend(*list, GINT_TO_POINTER(num));
			nummsgs++;
		}
	}
	g_dir_close(dp);

	debug_print("RSSyl: get_num_list: returning %d\n", nummsgs);

	return nummsgs;
}

static gboolean rssyl_is_msg_changed(Folder *folder, FolderItem *item,
		MsgInfo *msginfo)
{
	GStatBuf s;
	gchar *path = NULL;
	gchar *itempath = NULL;

	g_return_val_if_fail(folder != NULL, FALSE);
	g_return_val_if_fail(item != NULL, FALSE);
	g_return_val_if_fail(msginfo != NULL, FALSE);

	itempath = folder_item_get_path(item);
	path = g_strconcat(itempath, G_DIR_SEPARATOR_S, itos(msginfo->msgnum), NULL);
	g_free(itempath);

	if (g_stat(path, &s) < 0 ||
		msginfo->size != s.st_size || (
				(msginfo->mtime - s.st_mtime != 0) &&
				(msginfo->mtime - s.st_mtime != 3600) &&
				(msginfo->mtime - s.st_mtime != -3600))) {
		g_free(path);
		return TRUE;
	}

	g_free(path);
	return FALSE;
}

static gchar *rssyl_fetch_msg(Folder *folder, FolderItem *item, gint num)
{
	gchar *path;
	gchar *file;

	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(num > 0, NULL);

	path = folder_item_get_path(item);
	file = g_strconcat(path, G_DIR_SEPARATOR_S, itos(num), NULL);
	g_free(path);

	debug_print("RSSyl: fetch_msg '%s'\n", file);

	if( !is_file_exist(file)) {
		g_free(file);
		return NULL;
	}

	return file;
}

static MsgInfo *rssyl_get_msginfo(Folder *folder, FolderItem *item, gint num)
{
	MsgInfo *msginfo = NULL;
	gchar *file;
	MsgFlags flags;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(num > 0, NULL);

	debug_print("RSSyl: get_msginfo: %d\n", num);

	file = rssyl_fetch_msg(folder, item, num);
	g_return_val_if_fail(file != NULL, NULL);

	flags.perm_flags = 0;
	flags.tmp_flags = 0;

	msginfo = rssyl_feed_parse_item_to_msginfo(file, flags, TRUE, TRUE, item);
	g_free(file);

	if( msginfo )
		msginfo->msgnum = num;

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
		debug_print("RSSyl: add_msgs: new filename is '%s'\n", destfile);

		if( copy_file(fileinfo->file, destfile, TRUE) < 0 ) {
			g_warning("can't copy message %s to %s", fileinfo->file, destfile);
			g_free(destfile);
			return -1;
		}

		if( relation != NULL )
			g_hash_table_insert(relation, fileinfo->msginfo != NULL ?
					(gpointer) fileinfo->msginfo : (gpointer) fileinfo,
					GINT_TO_POINTER(dest->last_num + 1));
		g_free(destfile);
		dest->last_num++;
	}


	return dest->last_num;
}

gint rssyl_add_msg(Folder *folder, FolderItem *dest, const gchar *file,
		MsgFlags *flags)
{
	GSList file_list;
	MsgFileInfo fileinfo;

	g_return_val_if_fail(file != NULL, -1);

	fileinfo.msginfo = NULL;
	fileinfo.file = (gchar *)file;
	fileinfo.flags = flags;
	file_list.data = &fileinfo;
	file_list.next = NULL;

	return rssyl_add_msgs(folder, dest, &file_list, NULL);
}

static gint rssyl_remove_msg(Folder *folder, FolderItem *item, gint num)
{
	gboolean need_scan = FALSE;
	gchar *file, *tmp;
	RFolderItem *ritem = (RFolderItem *)item;

	g_return_val_if_fail(item != NULL, -1);

	file = rssyl_fetch_msg(folder, item, num);
	g_return_val_if_fail(file != NULL, -1);

	need_scan = mh_get_class()->scan_required(folder, item);

	/* are we doing a folder move ? */
	tmp = g_strdup_printf("%s.tmp", file);
	if (is_file_exist(tmp)) {
		g_unlink(tmp);
		g_free(tmp);
		g_free(file);
		return 0;
	}
	g_free(tmp);

	rssyl_deleted_update(ritem);
	rssyl_deleted_add(ritem, file);
	rssyl_deleted_store(ritem);
	rssyl_deleted_free(ritem);

	if( g_unlink(file) < 0 ) {
		FILE_OP_ERROR(file, "unlink");
		g_free(file);
		return -1;
	}

	if( !need_scan )
		item->mtime = time(NULL);

	g_free(file);
	return 0;
}

static gint rssyl_remove_msgs(Folder *folder, FolderItem *item,
		MsgInfoList *msglist, GHashTable *relation)
{
	gboolean need_scan = FALSE;
	MsgInfoList *cur;
	gint processed = 0;

	RFolderItem *ritem = (RFolderItem *)item;

	g_return_val_if_fail(item != NULL, -1);

	need_scan = mh_get_class()->scan_required(folder, item);

	rssyl_deleted_update(ritem);

	for (cur = msglist; cur != NULL; cur = cur->next) {
		gchar *file;
		MsgInfo *msginfo = (MsgInfo *)cur->data;

		if (msginfo == NULL)
			continue;

		file = rssyl_fetch_msg(folder, item, msginfo->msgnum);
		if (file == NULL)
			continue;

		rssyl_deleted_add(ritem, file);

		if (claws_unlink(file) < 0) {
			FILE_OP_ERROR(file, "unlink");
		}

		g_free(file);
		processed++;
	}

	if (processed > 0)
		rssyl_deleted_store(ritem);
	rssyl_deleted_free(ritem);

	if (!need_scan)
		item->mtime = time(NULL);

	return processed;
}

static gboolean rssyl_subscribe_uri(Folder *folder, const gchar *uri)
{
	if (folder->klass != rssyl_folder_get_class())
		return FALSE;
	return (rssyl_subscribe(FOLDER_ITEM(folder->node->data), uri, 0) ?
			TRUE : FALSE);
}

static void rssyl_copy_private_data(Folder *folder, FolderItem *oldi,
		FolderItem *newi)
{
	gchar *dpathold, *dpathnew, *pathold, *pathnew;
	RFolderItem *olditem = (RFolderItem *)oldi,
									*newitem = (RFolderItem *)newi;

	g_return_if_fail(folder != NULL);
	g_return_if_fail(olditem != NULL);
	g_return_if_fail(newitem != NULL);

	if (olditem->url != NULL) {
		g_free(newitem->url);
		newitem->url = g_strdup(olditem->url);
	}

	if (olditem->auth != NULL) {
		if (newitem->auth != NULL) {
			if (newitem->auth->username != NULL) {
				g_free(newitem->auth->username);
				newitem->auth->username = NULL;
			}
			if (newitem->auth->password != NULL) {
				g_free(newitem->auth->password);
				newitem->auth->password = NULL;
			}
			g_free(newitem->auth);
		}
		newitem->auth = g_new0(FeedAuth, 1);
		newitem->auth->type = olditem->auth->type;
		if (olditem->auth->username != NULL)
			newitem->auth->username = g_strdup(olditem->auth->username);
		if (olditem->auth->password != NULL)
			newitem->auth->password = g_strdup(olditem->auth->password);
	}

	if (olditem->official_title != NULL) {
		g_free(newitem->official_title);
		newitem->official_title = g_strdup(olditem->official_title);
	}

	if (olditem->source_id != NULL) {
		g_free(newitem->source_id);
		newitem->source_id = g_strdup(olditem->source_id);
	}

	newitem->keep_old = olditem->keep_old;
	newitem->default_refresh_interval = olditem->default_refresh_interval;
	newitem->refresh_interval = olditem->refresh_interval;
	newitem->fetch_comments = olditem->fetch_comments;
	newitem->fetch_comments_max_age = olditem->fetch_comments_max_age;
	newitem->silent_update = olditem->silent_update;
	newitem->write_heading = olditem->write_heading;
	newitem->ignore_title_rename = olditem->ignore_title_rename;
	newitem->ssl_verify_peer = olditem->ssl_verify_peer;
	newitem->refresh_id = olditem->refresh_id;
	newitem->fetching_comments = olditem->fetching_comments;
	newitem->last_update = olditem->last_update;

	pathold = rssyl_item_get_path(oldi->folder, oldi);
	dpathold = g_strconcat(pathold, G_DIR_SEPARATOR_S, RSSYL_DELETED_FILE, NULL);
	pathnew = rssyl_item_get_path(newi->folder, newi);
	dpathnew = g_strconcat(pathnew, G_DIR_SEPARATOR_S, RSSYL_DELETED_FILE, NULL);
	move_file(dpathold, dpathnew, TRUE);
	g_free(pathold);
	g_free(pathnew);
	g_free(dpathold);
	g_free(dpathnew);

}

/************************************************************************/

FolderClass *rssyl_folder_get_class()
{
	if( rssyl_class.idstr == NULL ) {
		rssyl_class.type = F_UNKNOWN;
		rssyl_class.idstr = "rssyl";
		rssyl_class.uistr = PLUGIN_NAME;

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
		rssyl_class.scan_required = mh_get_class()->scan_required;
		rssyl_class.item_set_xml = rssyl_item_set_xml;
		rssyl_class.item_get_xml = rssyl_item_get_xml;

		/* Message functions */
		rssyl_class.get_msginfo = rssyl_get_msginfo;
		rssyl_class.fetch_msg = rssyl_fetch_msg;
		rssyl_class.copy_msg = mh_get_class()->copy_msg;
		rssyl_class.copy_msgs = mh_get_class()->copy_msgs;
		rssyl_class.add_msg = rssyl_add_msg;
		rssyl_class.add_msgs = rssyl_add_msgs;
		rssyl_class.remove_msg = rssyl_remove_msg;
		rssyl_class.remove_msgs = rssyl_remove_msgs;
		rssyl_class.is_msg_changed = rssyl_is_msg_changed;
//		rssyl_class.change_flags = rssyl_change_flags;
		rssyl_class.change_flags = NULL;
		rssyl_class.subscribe = rssyl_subscribe_uri;
		rssyl_class.copy_private_data = rssyl_copy_private_data;
		rssyl_class.search_msgs = folder_item_search_msgs_local;
	}

	return &rssyl_class;
}
