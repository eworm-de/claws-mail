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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <stdio.h>
#include <string.h>

#include "intl.h"
#include "folder.h"
#include "session.h"
#include "imap.h"
#include "news.h"
#include "mh.h"
#include "utils.h"
#include "xml.h"
#include "codeconv.h"
#include "prefs.h"
#include "account.h"
#include "prefs_account.h"

static GList *folder_list = NULL;

static void folder_init		(Folder		*folder,
				 FolderType	 type,
				 const gchar	*name);

static void local_folder_destroy	(LocalFolder	*lfolder);
static void remote_folder_destroy	(RemoteFolder	*rfolder);
static void mh_folder_destroy		(MHFolder	*folder);
static void imap_folder_destroy		(IMAPFolder	*folder);
static void news_folder_destroy		(NewsFolder	*folder);

static gboolean folder_read_folder_func	(GNode		*node,
					 gpointer	 data);
static gchar *folder_get_list_path	(void);
static void folder_write_list_recursive	(GNode		*node,
					 gpointer	 data);


Folder *folder_new(FolderType type, const gchar *name, const gchar *path)
{
	Folder *folder = NULL;

	name = name ? name : path;
	switch (type) {
	case F_MH:
		folder = mh_folder_new(name, path);
		break;
	case F_IMAP:
		folder = imap_folder_new(name, path);
		break;
	case F_NEWS:
		folder = news_folder_new(name, path);
		break;
	default:
		return NULL;
	}

	return folder;
}

Folder *mh_folder_new(const gchar *name, const gchar *path)
{
	Folder *folder;

	folder = (Folder *)g_new0(MHFolder, 1);
	folder_init(folder, F_MH, name);
	LOCAL_FOLDER(folder)->rootpath = g_strdup(path);

	return folder;
}

Folder *mbox_folder_new(const gchar *name, const gchar *path)
{
	/* not yet implemented */
	return NULL;
}

Folder *maildir_folder_new(const gchar *name, const gchar *path)
{
	/* not yet implemented */
	return NULL;
}

Folder *imap_folder_new(const gchar *name, const gchar *path)
{
	Folder *folder;

	folder = (Folder *)g_new0(IMAPFolder, 1);
	folder_init(folder, F_IMAP, name);

	return folder;
}

Folder *news_folder_new(const gchar *name, const gchar *path)
{
	Folder *folder;

	folder = (Folder *)g_new0(NewsFolder, 1);
	folder_init(folder, F_NEWS, name);

	return folder;
}

FolderItem *folder_item_new(const gchar *name, const gchar *path)
{
	FolderItem *item;

	item = g_new0(FolderItem, 1);

	item->stype = F_NORMAL;
	item->name = g_strdup(name);
	item->path = g_strdup(path);
	item->account = NULL;
	item->mtime = 0;
	item->new = 0;
	item->unread = 0;
	item->total = 0;
	item->last_num = -1;
	item->parent = NULL;
	item->folder = NULL;
	item->data = NULL;

	item->prefs = prefs_folder_item_new();

	return item;
}

void folder_item_append(FolderItem *parent, FolderItem *item)
{
	GNode *node;

	g_return_if_fail(parent != NULL);
	g_return_if_fail(parent->folder != NULL);
	g_return_if_fail(item != NULL);

	node = parent->folder->node;
	node = g_node_find(node, G_PRE_ORDER, G_TRAVERSE_ALL, parent);
	g_return_if_fail(node != NULL);

	item->parent = parent;
	item->folder = parent->folder;
	g_node_append_data(node, item);
}

void folder_item_remove(FolderItem *item)
{
	GNode *node;

	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);

	node = item->folder->node;
	node = g_node_find(node, G_PRE_ORDER, G_TRAVERSE_ALL, item);
	g_return_if_fail(node != NULL);

	/* TODO: free all FolderItem's first */
	if (item->folder->node == node)
		item->folder->node = NULL;
	g_node_destroy(node);
}

void folder_set_ui_func(Folder *folder, FolderUIFunc func, gpointer data)
{
	g_return_if_fail(folder != NULL);

	folder->ui_func = func;
	folder->ui_func_data = data;
}

void folder_set_name(Folder *folder, const gchar *name)
{
	g_return_if_fail(folder != NULL);

	g_free(folder->name);
	folder->name = name ? g_strdup(name) : NULL;
	if (folder->node && folder->node->data) {
		FolderItem *item = (FolderItem *)folder->node->data;

		g_free(item->name);
		item->name = name ? g_strdup(name) : NULL;
	}
}

void folder_destroy(Folder *folder)
{
	g_return_if_fail(folder != NULL);

	folder_list = g_list_remove(folder_list, folder);

	switch (folder->type) {
	case F_MH:
		mh_folder_destroy(MH_FOLDER(folder));
		break;
	case F_IMAP:
		imap_folder_destroy(IMAP_FOLDER(folder));
		break;
	case F_NEWS:
		news_folder_destroy(NEWS_FOLDER(folder));
		break;
	default:
	}

	folder_tree_destroy(folder);
	g_free(folder->name);
	g_free(folder);
}

void folder_tree_destroy(Folder *folder)
{
	/* TODO: destroy all FolderItem before */
	g_node_destroy(folder->node);
}

void folder_add(Folder *folder)
{
	Folder *cur_folder;
	GList *cur;
	gint i;

	g_return_if_fail(folder != NULL);

	for (i = 0, cur = folder_list; cur != NULL; cur = cur->next, i++) {
		cur_folder = FOLDER(cur->data);
		if (folder->type == F_MH) {
			if (cur_folder->type != F_MH) break;
		} else if (folder->type == F_IMAP) {
			if (cur_folder->type != F_MH &&
			    cur_folder->type != F_IMAP) break;
		} else if (folder->type == F_NEWS) {
			if (cur_folder->type != F_MH &&
			    cur_folder->type != F_IMAP &&
			    cur_folder->type != F_NEWS) break;
		}
	}

	folder_list = g_list_insert(folder_list, folder, i);
}

GList *folder_get_list(void)
{
	return folder_list;
}

gint folder_read_list(void)
{
	GNode *node;
	XMLNode *xmlnode;

	node = xml_parse_file(folder_get_list_path());
	if (!node) return -1;

	xmlnode = node->data;
	if (strcmp2(xmlnode->tag->tag, "folderlist") != 0) {
		g_warning("wrong folder list\n");
		xml_free_tree(node);
		return -1;
	}

	g_node_traverse(node, G_PRE_ORDER, G_TRAVERSE_ALL, 2,
			folder_read_folder_func, NULL);

	xml_free_tree(node);
	if (folder_list)
		return 0;
	else
		return -1;
}

void folder_write_list(void)
{
	GList *list;
	Folder *folder;
	gchar *path;
	PrefFile *pfile;

	path = folder_get_list_path();
	if ((pfile = prefs_write_open(path)) == NULL) return;

	fprintf(pfile->fp, "<?xml version=\"1.0\" encoding=\"%s\"?>\n",
		conv_get_current_charset_str());
	fputs("\n<folderlist>\n", pfile->fp);

	for (list = folder_list; list != NULL; list = list->next) {
		folder = list->data;
		folder_write_list_recursive(folder->node, pfile->fp);
	}

	fputs("</folderlist>\n", pfile->fp);

	if (prefs_write_close(pfile) < 0)
		g_warning("failed to write folder list.\n");
}

Folder *folder_find_from_path(const gchar *path)
{
	GList *list;
	Folder *folder;

	for (list = folder_list; list != NULL; list = list->next) {
		folder = list->data;
		if (folder->type == F_MH &&
		    !path_cmp(LOCAL_FOLDER(folder)->rootpath, path))
			return folder;
	}

	return NULL;
}

static gboolean folder_item_find_func(GNode *node, gpointer data)
{
	FolderItem *item = node->data;
	gpointer *d = data;
	const gchar *path = d[0];

	if (path_cmp(path, item->path) != 0)
		return FALSE;

	d[1] = item;

	return TRUE;
}

FolderItem *folder_find_item_from_path(const gchar *path)
{
	Folder *folder;
	gpointer d[2];

	folder = folder_get_default_folder();
	g_return_val_if_fail(folder != NULL, NULL);

	d[0] = (gpointer)path;
	d[1] = NULL;
	g_node_traverse(folder->node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
			folder_item_find_func, d);
	return d[1];
}

Folder *folder_get_default_folder(void)
{
	return FOLDER(folder_list->data);
}

FolderItem *folder_get_default_inbox(void)
{
	Folder *folder;

	folder = FOLDER(folder_list->data);
	g_return_val_if_fail(folder != NULL, NULL);
	return folder->inbox;
}

FolderItem *folder_get_default_outbox(void)
{
	Folder *folder;

	folder = FOLDER(folder_list->data);
	g_return_val_if_fail(folder != NULL, NULL);
	return folder->outbox;
}

FolderItem *folder_get_default_draft(void)
{
	Folder *folder;

	folder = FOLDER(folder_list->data);
	g_return_val_if_fail(folder != NULL, NULL);
	return folder->draft;
}

FolderItem *folder_get_default_queue(void)
{
	Folder *folder;

	folder = FOLDER(folder_list->data);
	g_return_val_if_fail(folder != NULL, NULL);
	return folder->queue;
}

FolderItem *folder_get_default_trash(void)
{
	Folder *folder;

	folder = FOLDER(folder_list->data);
	g_return_val_if_fail(folder != NULL, NULL);
	return folder->trash;
}

gchar *folder_item_get_path(FolderItem *item)
{
	gchar *folder_path;
	gchar *path;

	g_return_val_if_fail(item != NULL, NULL);

	if (FOLDER_TYPE(item->folder) == F_MH)
		folder_path = g_strdup(LOCAL_FOLDER(item->folder)->rootpath);
	else if (FOLDER_TYPE(item->folder) == F_IMAP) {
		g_return_val_if_fail(item->folder->account != NULL, NULL);
		folder_path = g_strconcat(get_imap_cache_dir(),
					  G_DIR_SEPARATOR_S,
					  item->folder->account->recv_server,
					  G_DIR_SEPARATOR_S,
					  item->folder->account->userid,
					  NULL);
	} else if (FOLDER_TYPE(item->folder) == F_NEWS) {
		g_return_val_if_fail(item->folder->account != NULL, NULL);
		folder_path = g_strconcat(get_news_cache_dir(),
					  G_DIR_SEPARATOR_S,
					  item->folder->account->nntp_server,
					  NULL);
	} else
		return NULL;

	g_return_val_if_fail(folder_path != NULL, NULL);

	if (folder_path[0] == G_DIR_SEPARATOR) {
		if (item->path)
			path = g_strconcat(folder_path, G_DIR_SEPARATOR_S,
					   item->path, NULL);
		else
			path = g_strdup(folder_path);
	} else {
		if (item->path)
			path = g_strconcat(get_home_dir(), G_DIR_SEPARATOR_S,
					   folder_path, G_DIR_SEPARATOR_S,
					   item->path, NULL);
		else
			path = g_strconcat(get_home_dir(), G_DIR_SEPARATOR_S,
					   folder_path, NULL);
	}

	g_free(folder_path);
	return path;
}

void folder_item_scan(FolderItem *item)
{
	Folder *folder;

	g_return_if_fail(item != NULL);

	folder = item->folder;
	folder->scan(folder, item);
}

static void folder_item_scan_foreach_func(gpointer key, gpointer val,
					  gpointer data)
{
	folder_item_scan(FOLDER_ITEM(key));
}

void folder_item_scan_foreach(GHashTable *table)
{
	g_hash_table_foreach(table, folder_item_scan_foreach_func, NULL);
}

gchar *folder_item_fetch_msg(FolderItem *item, gint num)
{
	Folder *folder;

	g_return_val_if_fail(item != NULL, NULL);

	folder = item->folder;
	if (item->last_num < 0) folder->scan(folder, item);

	return folder->fetch_msg(folder, item, num);
}

gint folder_item_add_msg(FolderItem *dest, const gchar *file,
			 gboolean remove_source)
{
	Folder *folder;
	gint num;

	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(file != NULL, -1);
	g_return_val_if_fail(dest->folder->add_msg != NULL, -1);

	folder = dest->folder;
	if (dest->last_num < 0) folder->scan(folder, dest);

	num = folder->add_msg(folder, dest, file, remove_source);
	if (num > 0) dest->last_num = num;

	return num;
}

gint folder_item_move_msg(FolderItem *dest, MsgInfo *msginfo)
{
	Folder *folder;
	gint num;

	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(msginfo != NULL, -1);

	folder = dest->folder;
	if (dest->last_num < 0) folder->scan(folder, dest);

	num = folder->move_msg(folder, dest, msginfo);
	if (num > 0) dest->last_num = num;

	return num;
}

gint folder_item_move_msgs_with_dest(FolderItem *dest, GSList *msglist)
{
	Folder *folder;
	gint num;

	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(msglist != NULL, -1);

	folder = dest->folder;
	if (dest->last_num < 0) folder->scan(folder, dest);

	num = folder->move_msgs_with_dest(folder, dest, msglist);
	if (num > 0) dest->last_num = num;

	return num;
}

gint folder_item_copy_msg(FolderItem *dest, MsgInfo *msginfo)
{
	Folder *folder;
	gint num;

	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(msginfo != NULL, -1);

	folder = dest->folder;
	if (dest->last_num < 0) folder->scan(folder, dest);

	num = folder->copy_msg(folder, dest, msginfo);
	if (num > 0) dest->last_num = num;

	return num;
}

gint folder_item_copy_msgs_with_dest(FolderItem *dest, GSList *msglist)
{
	Folder *folder;
	gint num;

	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(msglist != NULL, -1);

	folder = dest->folder;
	if (dest->last_num < 0) folder->scan(folder, dest);

	num = folder->copy_msgs_with_dest(folder, dest, msglist);
	if (num > 0) dest->last_num = num;

	return num;
}

gint folder_item_remove_msg(FolderItem *item, gint num)
{
	Folder *folder;

	g_return_val_if_fail(item != NULL, -1);

	folder = item->folder;
	if (item->last_num < 0) folder->scan(folder, item);

	return folder->remove_msg(folder, item, num);
}

gint folder_item_remove_all_msg(FolderItem *item)
{
	Folder *folder;

	g_return_val_if_fail(item != NULL, -1);

	folder = item->folder;
	if (item->last_num < 0) folder->scan(folder, item);

	return folder->remove_all_msg(folder, item);
}

gboolean folder_item_is_msg_changed(FolderItem *item, MsgInfo *msginfo)
{
	Folder *folder;

	g_return_val_if_fail(item != NULL, FALSE);

	folder = item->folder;
	return folder->is_msg_changed(folder, item, msginfo);
}

gchar *folder_item_get_cache_file(FolderItem *item)
{
	gchar *path;
	gchar *file;

	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(item->path != NULL, NULL);

	path = folder_item_get_path(item);
	g_return_val_if_fail(path != NULL, NULL);
	if (!is_dir_exist(path))
		make_dir_hier(path);
	file = g_strconcat(path, G_DIR_SEPARATOR_S, CACHE_FILE, NULL);
	g_free(path);

	return file;
}

gchar *folder_item_get_mark_file(FolderItem *item)
{
	gchar *path;
	gchar *file;

	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(item->path != NULL, NULL);

	path = folder_item_get_path(item);
	g_return_val_if_fail(path != NULL, NULL);
	if (!is_dir_exist(path))
		make_dir_hier(path);
	file = g_strconcat(path, G_DIR_SEPARATOR_S, MARK_FILE, NULL);
	g_free(path);

	return file;
}


static void folder_init(Folder *folder, FolderType type, const gchar *name)
{
	FolderItem *item;

	g_return_if_fail(folder != NULL);

	folder->type = type;
	folder_set_name(folder, name);
	folder->account = NULL;
	folder->inbox = NULL;
	folder->outbox = NULL;
	folder->draft = NULL;
	folder->queue = NULL;
	folder->trash = NULL;
	folder->ui_func = NULL;
	folder->ui_func_data = NULL;
	item = folder_item_new(name, NULL);
	item->folder = folder;
	folder->node = g_node_new(item);
	folder->data = NULL;

	switch (type) {
	case F_MH:
		folder->get_msg_list        = mh_get_msg_list;
		folder->fetch_msg           = mh_fetch_msg;
		folder->add_msg             = mh_add_msg;
		folder->move_msg            = mh_move_msg;
		folder->move_msgs_with_dest = mh_move_msgs_with_dest;
		folder->copy_msg            = mh_copy_msg;
		folder->copy_msgs_with_dest = mh_copy_msgs_with_dest;
		folder->remove_msg          = mh_remove_msg;
		folder->remove_all_msg      = mh_remove_all_msg;
		folder->is_msg_changed      = mh_is_msg_changed;
		folder->scan                = mh_scan_folder;
		folder->scan_tree           = mh_scan_tree;
		folder->create_tree         = mh_create_tree;
		folder->create_folder       = mh_create_folder;
		folder->rename_folder       = mh_rename_folder;
		folder->remove_folder       = mh_remove_folder;
		break;
	case F_IMAP:
		folder->get_msg_list        = imap_get_msg_list;
		folder->fetch_msg           = imap_fetch_msg;
		folder->move_msg            = imap_move_msg;
		folder->move_msgs_with_dest = imap_move_msgs_with_dest;
		folder->remove_msg          = imap_remove_msg;
		folder->remove_all_msg      = imap_remove_all_msg;
		folder->scan                = imap_scan_folder;
		folder->create_folder       = imap_create_folder;
		folder->remove_folder       = imap_remove_folder;		
		break;
	case F_NEWS:
		folder->get_msg_list = news_get_article_list;
		folder->fetch_msg    = news_fetch_msg;
		folder->scan         = news_scan_group;
		break;
	default:
	}

	switch (type) {
	case F_MH:
	case F_MBOX:
	case F_MAILDIR:
		LOCAL_FOLDER(folder)->rootpath = NULL;
		break;
	case F_IMAP:
	case F_NEWS:
		REMOTE_FOLDER(folder)->session   = NULL;
		break;
	default:
	}
}

static void local_folder_destroy(LocalFolder *lfolder)
{
	g_return_if_fail(lfolder != NULL);

	g_free(lfolder->rootpath);
}

static void remote_folder_destroy(RemoteFolder *rfolder)
{
	g_return_if_fail(rfolder != NULL);

	if (rfolder->session)
		session_destroy(rfolder->session);
}

static void mh_folder_destroy(MHFolder *folder)
{
	local_folder_destroy(LOCAL_FOLDER(folder));
}

static void imap_folder_destroy(IMAPFolder *folder)
{
	remote_folder_destroy(REMOTE_FOLDER(folder));
}

static void news_folder_destroy(NewsFolder *folder)
{
	remote_folder_destroy(REMOTE_FOLDER(folder));
}

static gboolean folder_build_tree(GNode *node, gpointer data)
{
	Folder *folder = FOLDER(data);
	FolderItem *item;
	XMLNode *xmlnode;
	GList *list;
	SpecialFolderItemType stype = F_NORMAL;
	const gchar *name = NULL;
	const gchar *path = NULL;
	PrefsAccount *account = NULL;
	gint mtime = 0, new = 0, unread = 0, total = 0;

	g_return_val_if_fail(node->data != NULL, FALSE);
	if (!node->parent) return FALSE;

	xmlnode = node->data;
	if (strcmp2(xmlnode->tag->tag, "folderitem") != 0) {
		g_warning("tag name != \"folderitem\"\n");
		return FALSE;
	}

	list = xmlnode->tag->attr;
	for (; list != NULL; list = list->next) {
		XMLAttr *attr = list->data;

		if (!attr || !attr->name || !attr->value) continue;
		if (!strcmp(attr->name, "type")) {
			if (!strcasecmp(attr->value, "normal"))
				stype = F_NORMAL;
			else if (!strcasecmp(attr->value, "inbox"))
				stype = F_INBOX;
			else if (!strcasecmp(attr->value, "outbox"))
				stype = F_OUTBOX;
			else if (!strcasecmp(attr->value, "draft"))
				stype = F_DRAFT;
			else if (!strcasecmp(attr->value, "queue"))
				stype = F_QUEUE;
			else if (!strcasecmp(attr->value, "trash"))
				stype = F_TRASH;
		} else if (!strcmp(attr->name, "name"))
			name = attr->value;
		else if (!strcmp(attr->name, "path"))
			path = attr->value;
		else if (!strcmp(attr->name, "account_id")) {
			account = account_find_from_id(atoi(attr->value));
			if (!account) g_warning("account_id: %s not found\n",
						attr->value);
		}
		else if (!strcmp(attr->name, "mtime"))
			mtime = atoi(attr->value);
		else if (!strcmp(attr->name, "new"))
			new = atoi(attr->value);
		else if (!strcmp(attr->name, "unread"))
			unread = atoi(attr->value);
		else if (!strcmp(attr->name, "total"))
			total = atoi(attr->value);
	}

	item = folder_item_new(name, path);
	item->stype = stype;
	item->account = account;
	item->mtime = mtime;
	item->new = new;
	item->unread = unread;
	item->total = total;
	item->parent = FOLDER_ITEM(node->parent->data);
	item->folder = folder;
	switch (stype) {
	case F_INBOX:  folder->inbox  = item; break;
	case F_OUTBOX: folder->outbox = item; break;
	case F_DRAFT:  folder->draft  = item; break;
	case F_QUEUE:  folder->queue  = item; break;
	case F_TRASH:  folder->trash  = item; break;
	default:
	}

	prefs_folder_item_read_config(item);

	node->data = item;
	xml_free_node(xmlnode);

	return FALSE;
}

static gboolean folder_read_folder_func(GNode *node, gpointer data)
{
	Folder *folder;
	XMLNode *xmlnode;
	GList *list;
	FolderType type = F_UNKNOWN;
	const gchar *name = NULL;
	const gchar *path = NULL;
	PrefsAccount *account = NULL;

	if (g_node_depth(node) != 2) return FALSE;
	g_return_val_if_fail(node->data != NULL, FALSE);

	xmlnode = node->data;
	if (strcmp2(xmlnode->tag->tag, "folder") != 0) {
		g_warning("tag name != \"folder\"\n");
		return TRUE;
	}
	g_node_unlink(node);
	list = xmlnode->tag->attr;
	for (; list != NULL; list = list->next) {
		XMLAttr *attr = list->data;

		if (!attr || !attr->name || !attr->value) continue;
		if (!strcmp(attr->name, "type")) {
			if (!strcasecmp(attr->value, "mh"))
				type = F_MH;
			else if (!strcasecmp(attr->value, "mbox"))
				type = F_MBOX;
			else if (!strcasecmp(attr->value, "maildir"))
				type = F_MAILDIR;
			else if (!strcasecmp(attr->value, "imap"))
				type = F_IMAP;
			else if (!strcasecmp(attr->value, "news"))
				type = F_NEWS;
		} else if (!strcmp(attr->name, "name"))
			name = attr->value;
		else if (!strcmp(attr->name, "path"))
			path = attr->value;
		else if (!strcmp(attr->name, "account_id")) {
			account = account_find_from_id(atoi(attr->value));
			if (!account) g_warning("account_id: %s not found\n",
						attr->value);
		}
	}

	folder = folder_new(type, name, path);
	g_return_val_if_fail(folder != NULL, FALSE);
	folder->account = account;
	if (account && (type == F_IMAP || type == F_NEWS))
		account->folder = REMOTE_FOLDER(folder);
	node->data = folder->node->data;
	g_node_destroy(folder->node);
	folder->node = node;
	folder_add(folder);

	g_node_traverse(node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
			folder_build_tree, folder);

	return FALSE;
}

static gchar *folder_get_list_path(void)
{
	static gchar *filename = NULL;

	if (!filename)
		filename =  g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
					FOLDER_LIST, NULL);

	return filename;
}

static void folder_write_list_recursive(GNode *node, gpointer data)
{
	FILE *fp = (FILE *)data;
	FolderItem *item = FOLDER_ITEM(node->data);
	gint i, depth;
	static gchar *folder_type_str[] = {"mh", "mbox", "maildir", "imap",
					   "news", "unknown"};
	static gchar *folder_item_stype_str[] = {"normal", "inbox", "outbox",
						 "draft", "queue", "trash"};

	g_return_if_fail(item != NULL);

	depth = g_node_depth(node);
	for (i = 0; i < depth; i++)
		fputs("    ", fp);
	if (depth == 1) {
		Folder *folder = item->folder;

		fprintf(fp, "<folder type=\"%s\"", folder_type_str[folder->type]);
		if (folder->name) {
			fputs(" name=\"", fp);
			xml_file_put_escape_str(fp, folder->name);
			fputs("\"", fp);
		}
		if (folder->type == F_MH) {
			fputs(" path=\"", fp);
			xml_file_put_escape_str
				(fp, LOCAL_FOLDER(folder)->rootpath);
			fputs("\"", fp);
		}
		if (folder->account)
			fprintf(fp, " account_id=\"%d\"",
				folder->account->account_id);
	} else {
		fprintf(fp, "<folderitem type=\"%s\"",
			folder_item_stype_str[item->stype]);
		if (item->name) {
			fputs(" name=\"", fp);
			xml_file_put_escape_str(fp, item->name);
			fputs("\"", fp);
		}
		if (item->path) {
			fputs(" path=\"", fp);
			xml_file_put_escape_str(fp, item->path);
			fputs("\"", fp);
		}
		if (item->account)
			fprintf(fp, " account_id = \"%d\"",
				item->account->account_id);
		fprintf(fp,
			" mtime=\"%ld\" new=\"%d\" unread=\"%d\" total=\"%d\"",
			item->mtime, item->new, item->unread, item->total);
	}

	if (node->children) {
		GNode *child;
		fputs(">\n", fp);

		child = node->children;
		while (child) {
			GNode *cur;

			cur = child;
			child = cur->next;
			folder_write_list_recursive(cur, data);
		}

		for (i = 0; i < depth; i++)
			fputs("    ", fp);
		fprintf(fp, "</%s>\n", depth == 1 ? "folder" : "folderitem");
	} else
		fputs(" />\n", fp);
}
