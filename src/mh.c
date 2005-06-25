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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#undef MEASURE_TIME

#ifdef MEASURE_TIME
#  include <sys/time.h>
#endif

#include "folder.h"
#include "mh.h"
#include "procmsg.h"
#include "procheader.h"
#include "utils.h"
#include "codeconv.h"

static void	mh_folder_init		(Folder		*folder,
					 const gchar	*name,
					 const gchar	*path);

static Folder	*mh_folder_new		(const gchar	*name,
					 const gchar	*path);
static void     mh_folder_destroy	(Folder		*folder);
static gchar   *mh_fetch_msg		(Folder		*folder,
					 FolderItem	*item,
					 gint		 num);
static MsgInfo *mh_get_msginfo		(Folder		*folder,
					 FolderItem	*item,
					 gint		 num);
static gint     mh_add_msg		(Folder		*folder,
					 FolderItem	*dest,
					 const gchar	*file,
					 MsgFlags	*flags);
static gint     mh_add_msgs		(Folder		*folder,
					 FolderItem	*dest,
					 GSList		*file_list,
					 GRelation 	*relation);
static gint     mh_copy_msg		(Folder		*folder,
					 FolderItem	*dest,
					 MsgInfo	*msginfo);
static gint     mh_remove_msg		(Folder		*folder,
					 FolderItem	*item,
					 gint 		 num);
static gint     mh_remove_all_msg	(Folder		*folder,
					 FolderItem	*item);
static gboolean mh_is_msg_changed	(Folder		*folder,
					 FolderItem	*item,
					 MsgInfo	*msginfo);

static gint 	mh_get_num_list		(Folder 	*folder,
			    		 FolderItem 	*item, 
					 GSList 	**list, 
					 gboolean 	*old_uids_valid);
static gint 	mh_scan_tree		(Folder 	*folder);

static gint    mh_create_tree		(Folder		*folder);
static FolderItem *mh_create_folder	(Folder		*folder,
					 FolderItem	*parent,
					 const gchar	*name);
static gint    mh_rename_folder		(Folder		*folder,
					 FolderItem	*item,
					 const gchar	*name);
static gint    mh_remove_folder		(Folder		*folder,
					 FolderItem	*item);

static gchar   *mh_get_new_msg_filename		(FolderItem	*dest);

static MsgInfo *mh_parse_msg			(const gchar	*file,
						 FolderItem	*item);
static void	mh_remove_missing_folder_items	(Folder		*folder);
static gchar	*mh_filename_from_utf8		(const gchar	*path);
static gchar	*mh_filename_to_utf8		(const gchar	*path);
static void	mh_scan_tree_recursive		(FolderItem	*item);

static gboolean mh_rename_folder_func		(GNode		*node,
						 gpointer	 data);
static gchar   *mh_item_get_path		(Folder *folder, 
						 FolderItem *item);

static gboolean mh_scan_required	(Folder		*folder,
					 FolderItem	*item);

static FolderClass mh_class;

FolderClass *mh_get_class(void)
{
	if (mh_class.idstr == NULL) {
		mh_class.type = F_MH;
		mh_class.idstr = "mh";
		mh_class.uistr = "MH";
		
		/* Folder functions */
		mh_class.new_folder = mh_folder_new;
		mh_class.destroy_folder = mh_folder_destroy;
		mh_class.set_xml = folder_local_set_xml;
		mh_class.get_xml = folder_local_get_xml;
		mh_class.scan_tree = mh_scan_tree;
		mh_class.create_tree = mh_create_tree;

		/* FolderItem functions */
		mh_class.item_get_path = mh_item_get_path;
		mh_class.create_folder = mh_create_folder;
		mh_class.rename_folder = mh_rename_folder;
		mh_class.remove_folder = mh_remove_folder;
		mh_class.get_num_list = mh_get_num_list;
		mh_class.scan_required = mh_scan_required;
		
		/* Message functions */
		mh_class.get_msginfo = mh_get_msginfo;
		mh_class.fetch_msg = mh_fetch_msg;
		mh_class.add_msg = mh_add_msg;
		mh_class.add_msgs = mh_add_msgs;
		mh_class.copy_msg = mh_copy_msg;
		mh_class.remove_msg = mh_remove_msg;
		mh_class.remove_all_msg = mh_remove_all_msg;
		mh_class.is_msg_changed = mh_is_msg_changed;
	}

	return &mh_class;
}

static Folder *mh_folder_new(const gchar *name, const gchar *path)
{
	Folder *folder;

	folder = (Folder *)g_new0(MHFolder, 1);
	folder->klass = &mh_class;
	mh_folder_init(folder, name, path);

	return folder;
}

static void mh_folder_destroy(Folder *folder)
{
	folder_local_folder_destroy(LOCAL_FOLDER(folder));
}

static void mh_folder_init(Folder *folder, const gchar *name, const gchar *path)
{
	folder_local_folder_init(folder, name, path);

}

gboolean mh_scan_required(Folder *folder, FolderItem *item)
{
	gchar *path;
	struct stat s;

	path = folder_item_get_path(item);
	g_return_val_if_fail(path != NULL, FALSE);

	if (stat(path, &s) < 0) {
		FILE_OP_ERROR(path, "stat");
		g_free(path);
		return FALSE;
	}

	if ((s.st_mtime > item->mtime) &&
		(s.st_mtime - 3600 != item->mtime)) {
		debug_print("MH scan required, folder updated: %s (%ld > %ld)\n",
			    path,
			    s.st_mtime,
			    item->mtime);
		g_free(path);
		return TRUE;
	}

	debug_print("MH scan not required: %s (%ld <= %ld)\n",
		    path,
		    s.st_mtime,
		    item->mtime);
	g_free(path);
	return FALSE;
}

void mh_get_last_num(Folder *folder, FolderItem *item)
{
	gchar *path;
	DIR *dp;
	struct dirent *d;
	gint max = 0;
	gint num;

	g_return_if_fail(item != NULL);

	debug_print("mh_get_last_num(): Scanning %s ...\n", item->path);

	path = folder_item_get_path(item);
	g_return_if_fail(path != NULL);
	if (change_dir(path) < 0) {
		g_free(path);
		return;
	}
	g_free(path);

	if ((dp = opendir(".")) == NULL) {
		FILE_OP_ERROR(item->path, "opendir");
		return;
	}

	while ((d = readdir(dp)) != NULL) {
		if ((num = to_number(d->d_name)) > 0 &&
		    dirent_is_regular_file(d)) {
			if (max < num)
				max = num;
		}
	}
	closedir(dp);

	debug_print("Last number in dir %s = %d\n", item->path, max);
	item->last_num = max;
}

gint mh_get_num_list(Folder *folder, FolderItem *item, GSList **list, gboolean *old_uids_valid)
{

	gchar *path;
	DIR *dp;
	struct dirent *d;
	gint num, nummsgs = 0;

	g_return_val_if_fail(item != NULL, -1);

	debug_print("mh_get_num_list(): Scanning %s ...\n", item->path);

	*old_uids_valid = TRUE;

	path = folder_item_get_path(item);
	g_return_val_if_fail(path != NULL, -1);
	if (change_dir(path) < 0) {
		g_free(path);
		return -1;
	}
	g_free(path);

	if ((dp = opendir(".")) == NULL) {
		FILE_OP_ERROR(item->path, "opendir");
		return -1;
	}

	while ((d = readdir(dp)) != NULL) {
		if ((num = to_number(d->d_name)) > 0) {
			*list = g_slist_prepend(*list, GINT_TO_POINTER(num));
		   	nummsgs++;
		}
	}
	closedir(dp);

	item->mtime = time(NULL);
	return nummsgs;
}

static gchar *mh_fetch_msg(Folder *folder, FolderItem *item, gint num)
{
	gchar *path;
	gchar *file;

	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(num > 0, NULL);

	path = folder_item_get_path(item);
	file = g_strconcat(path, G_DIR_SEPARATOR_S, itos(num), NULL);

	if (!is_file_exist(file)) {
		g_free(file);
		g_free(path);
		return NULL;
	}
	g_free(path);
	return file;
}

static MsgInfo *mh_get_msginfo(Folder *folder, FolderItem *item, gint num)
{
	MsgInfo *msginfo;
	gchar *file;

	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(num > 0, NULL);

	file = mh_fetch_msg(folder, item, num);
	if (!file) return NULL;

	msginfo = mh_parse_msg(file, item);
	if (msginfo)
		msginfo->msgnum = num;

	g_free(file);

	return msginfo;
}

static gchar *mh_get_new_msg_filename(FolderItem *dest)
{
	gchar *destfile;
	gchar *destpath;

	destpath = folder_item_get_path(dest);
	g_return_val_if_fail(destpath != NULL, NULL);

	if (!is_dir_exist(destpath))
		make_dir_hier(destpath);

	for (;;) {
		destfile = g_strdup_printf("%s%c%d", destpath, G_DIR_SEPARATOR,
					   dest->last_num + 1);
		if (is_file_entry_exist(destfile)) {
			dest->last_num++;
			g_free(destfile);
		} else
			break;
	}

	g_free(destpath);

	return destfile;
}

static gint mh_add_msg(Folder *folder, FolderItem *dest, const gchar *file, MsgFlags *flags)
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

        ret = mh_add_msgs(folder, dest, &file_list, NULL);
	return ret;
} 
 
static gint mh_add_msgs(Folder *folder, FolderItem *dest, GSList *file_list, 
                 GRelation *relation)
{ 
	gchar *destfile;
	GSList *cur;
	MsgFileInfo *fileinfo;

	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(file_list != NULL, -1);

	if (dest->last_num < 0) {
		mh_get_last_num(folder, dest);
		if (dest->last_num < 0) return -1;
	}

	for (cur = file_list; cur != NULL; cur = cur->next) {
		fileinfo = (MsgFileInfo *)cur->data;

		destfile = mh_get_new_msg_filename(dest);
		if (destfile == NULL) return -1;

		if (link(fileinfo->file, destfile) < 0) {
			if (copy_file(fileinfo->file, destfile, TRUE) < 0) {
				g_warning(_("can't copy message %s to %s\n"),
					  fileinfo->file, destfile);
				g_free(destfile);
				return -1;
			}
		}
		if (relation != NULL)
			g_relation_insert(relation, fileinfo, GINT_TO_POINTER(dest->last_num + 1));
		g_free(destfile);
		dest->last_num++;
	}

	return dest->last_num;
}

static gint mh_copy_msg(Folder *folder, FolderItem *dest, MsgInfo *msginfo)
{
	gboolean dest_need_scan = FALSE;
	gchar *srcfile;
	gchar *destfile;
	gint filemode = 0;
	FolderItemPrefs *prefs;

	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(msginfo != NULL, -1);

	if (msginfo->folder == dest) {
		g_warning("the src folder is identical to the dest.\n");
		return -1;
	}

	if (dest->last_num < 0) {
		mh_get_last_num(folder, dest);
		if (dest->last_num < 0) return -1;
	}

	prefs = dest->prefs;

	srcfile = procmsg_get_message_file(msginfo);
	destfile = mh_get_new_msg_filename(dest);
	if (!destfile) {
		g_free(srcfile);
		return -1;
	}
	
	debug_print("Copying message %s%c%d to %s ...\n",
		    msginfo->folder->path, G_DIR_SEPARATOR,
		    msginfo->msgnum, dest->path);
	

	if ((MSG_IS_QUEUED(msginfo->flags) || MSG_IS_DRAFT(msginfo->flags))
	&&  dest->stype != F_QUEUE && dest->stype != F_DRAFT) {
		if (procmsg_remove_special_headers(srcfile, destfile) !=0) {
			g_free(srcfile);
			g_free(destfile);
			return -1;
		}
	} else if (!(MSG_IS_QUEUED(msginfo->flags) || MSG_IS_DRAFT(msginfo->flags))
	&& (dest->stype == F_QUEUE || dest->stype == F_DRAFT)) {
		g_free(srcfile);
		g_free(destfile);
		return -1;
	} else if (copy_file(srcfile, destfile, TRUE) < 0) {
		FILE_OP_ERROR(srcfile, "copy");
		g_free(srcfile);
		g_free(destfile);
		return -1;
	}

	dest_need_scan = mh_scan_required(dest->folder, dest);
	if (!dest_need_scan)
		dest->mtime = time(NULL);


	if (prefs && prefs->enable_folder_chmod && prefs->folder_chmod) {
		if (chmod(destfile, prefs->folder_chmod) < 0)
			FILE_OP_ERROR(destfile, "chmod");

		/* for mark file */
		filemode = prefs->folder_chmod;
		if (filemode & S_IRGRP) filemode |= S_IWGRP;
		if (filemode & S_IROTH) filemode |= S_IWOTH;
	}

	g_free(srcfile);
	g_free(destfile);
	dest->last_num++;

	return dest->last_num;
}

static gint mh_remove_msg(Folder *folder, FolderItem *item, gint num)
{
	gboolean need_scan = FALSE;
	gchar *file;

	g_return_val_if_fail(item != NULL, -1);

	file = mh_fetch_msg(folder, item, num);
	g_return_val_if_fail(file != NULL, -1);

	need_scan = mh_scan_required(folder, item);

	if (unlink(file) < 0) {
		FILE_OP_ERROR(file, "unlink");
		g_free(file);
		return -1;
	}

	if (!need_scan)
		item->mtime = time(NULL);

	g_free(file);
	return 0;
}

static gint mh_remove_all_msg(Folder *folder, FolderItem *item)
{
	gchar *path;
	gint val;

	g_return_val_if_fail(item != NULL, -1);

	path = folder_item_get_path(item);
	g_return_val_if_fail(path != NULL, -1);
	val = remove_all_numbered_files(path);
	g_free(path);

	return val;
}

static gboolean mh_is_msg_changed(Folder *folder, FolderItem *item,
				  MsgInfo *msginfo)
{
	struct stat s;

	if (stat(itos(msginfo->msgnum), &s) < 0 ||
	    msginfo->size  != s.st_size || (
		(msginfo->mtime - s.st_mtime != 0) &&
		(msginfo->mtime - s.st_mtime != 3600) &&
		(msginfo->mtime - s.st_mtime != -3600)))
		return TRUE;

	return FALSE;
}

static gint mh_scan_tree(Folder *folder)
{
	FolderItem *item;
	gchar *rootpath;

	g_return_val_if_fail(folder != NULL, -1);

	if (!folder->node) {
		item = folder_item_new(folder, folder->name, NULL);
		item->folder = folder;
		folder->node = item->node = g_node_new(item);
	} else
		item = FOLDER_ITEM(folder->node->data);

	rootpath = folder_item_get_path(item);
	if (change_dir(rootpath) < 0) {
		g_free(rootpath);
		return -1;
	}
	g_free(rootpath);

	mh_create_tree(folder);
	mh_remove_missing_folder_items(folder);
	mh_scan_tree_recursive(item);

	return 0;
}

#define MAKE_DIR_IF_NOT_EXIST(dir) \
{ \
	if (!is_dir_exist(dir)) { \
		if (is_file_exist(dir)) { \
			g_warning("File `%s' already exists.\n" \
				    "Can't create folder.", dir); \
			return -1; \
		} \
		if (make_dir(dir) < 0) \
			return -1; \
	} \
}

static gint mh_create_tree(Folder *folder)
{
	gchar *rootpath;

	g_return_val_if_fail(folder != NULL, -1);

	CHDIR_RETURN_VAL_IF_FAIL(get_home_dir(), -1);
	rootpath = LOCAL_FOLDER(folder)->rootpath;
	MAKE_DIR_IF_NOT_EXIST(rootpath);
	CHDIR_RETURN_VAL_IF_FAIL(rootpath, -1);
	MAKE_DIR_IF_NOT_EXIST(INBOX_DIR);
	MAKE_DIR_IF_NOT_EXIST(OUTBOX_DIR);
	MAKE_DIR_IF_NOT_EXIST(QUEUE_DIR);
	MAKE_DIR_IF_NOT_EXIST(DRAFT_DIR);
	MAKE_DIR_IF_NOT_EXIST(TRASH_DIR);

	return 0;
}

#undef MAKE_DIR_IF_NOT_EXIST

static gchar *mh_item_get_path(Folder *folder, FolderItem *item)
{
	gchar *folder_path, *path;
	gchar *real_path;
	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(item != NULL, NULL);

	folder_path = g_strdup(LOCAL_FOLDER(folder)->rootpath);
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
	real_path = mh_filename_from_utf8(path);
	g_free(path);
	return real_path;
}

static FolderItem *mh_create_folder(Folder *folder, FolderItem *parent,
				    const gchar *name)
{
	gchar *path, *real_name;
	gchar *fullpath;
	FolderItem *new_item;
	gchar *mh_sequences_filename;
	FILE *mh_sequences_file;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(parent != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);

	path = folder_item_get_path(parent);
	if (!is_dir_exist(path)) 
		if (make_dir_hier(path) != 0)
			return NULL;
		
	real_name = mh_filename_from_utf8(name);
	fullpath = g_strconcat(path, G_DIR_SEPARATOR_S, real_name, NULL);
	g_free(real_name);
	g_free(path);

	if (make_dir(fullpath) < 0) {
		g_free(fullpath);
		return NULL;
	}

	g_free(fullpath);

	if (parent->path)
		path = g_strconcat(parent->path, G_DIR_SEPARATOR_S, name,
				   NULL);
	else
		path = g_strdup(name);
	new_item = folder_item_new(folder, name, path);
	folder_item_append(parent, new_item);

	g_free(path);

	path = folder_item_get_path(new_item);
	mh_sequences_filename = g_strconcat(path, G_DIR_SEPARATOR_S,
					    ".mh_sequences", NULL);
	if ((mh_sequences_file = fopen(mh_sequences_filename, "a+b")) != NULL) {
		fclose(mh_sequences_file);
	}
	g_free(mh_sequences_filename);
	g_free(path);

	return new_item;
}

static gint mh_rename_folder(Folder *folder, FolderItem *item,
			     const gchar *name)
{
 	gchar *real_name;
	gchar *oldpath;
	gchar *dirname;
	gchar *newpath, *utf8newpath;
	gchar *paths[2];

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(item != NULL, -1);
	g_return_val_if_fail(item->path != NULL, -1);
	g_return_val_if_fail(name != NULL, -1);

	oldpath = folder_item_get_path(item);
	if (!is_dir_exist(oldpath))
		make_dir_hier(oldpath);

	dirname = g_path_get_dirname(oldpath);
	real_name = mh_filename_from_utf8(name);
	newpath = g_strconcat(dirname, G_DIR_SEPARATOR_S, real_name, NULL);
	g_free(real_name);

	if (rename(oldpath, newpath) < 0) {
		FILE_OP_ERROR(oldpath, "rename");
		g_free(oldpath);
		g_free(newpath);
		return -1;
	}

	g_free(oldpath);
	g_free(newpath);

	if (strchr(item->path, G_DIR_SEPARATOR) != NULL) {
		dirname = g_path_get_dirname(item->path);
		utf8newpath = g_strconcat(dirname, G_DIR_SEPARATOR_S,
					  name, NULL);
		g_free(dirname);
	} else
		utf8newpath = g_strdup(name);

	g_free(item->name);
	item->name = g_strdup(name);

	paths[0] = g_strdup(item->path);
	paths[1] = utf8newpath;
	g_node_traverse(item->node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
			mh_rename_folder_func, paths);

	g_free(paths[0]);
	g_free(paths[1]);
	return 0;
}

static gint mh_remove_folder(Folder *folder, FolderItem *item)
{
	gchar *path;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(item != NULL, -1);
	g_return_val_if_fail(item->path != NULL, -1);

	path = folder_item_get_path(item);
	if (remove_dir_recursive(path) < 0) {
		g_warning("can't remove directory `%s'\n", path);
		g_free(path);
		return -1;
	}

	g_free(path);
	folder_item_remove(item);
	return 0;
}

static MsgInfo *mh_parse_msg(const gchar *file, FolderItem *item)
{
	MsgInfo *msginfo;
	MsgFlags flags;

	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(file != NULL, NULL);

	flags.perm_flags = MSG_NEW|MSG_UNREAD;
	flags.tmp_flags = 0;

	if (item->stype == F_QUEUE) {
		MSG_SET_TMP_FLAGS(flags, MSG_QUEUED);
	} else if (item->stype == F_DRAFT) {
		MSG_SET_TMP_FLAGS(flags, MSG_DRAFT);
	}

	msginfo = procheader_parse_file(file, flags, FALSE, FALSE);
	if (!msginfo) return NULL;

	msginfo->msgnum = atoi(file);
	msginfo->folder = item;

	return msginfo;
}

#if 0
static gboolean mh_is_maildir_one(const gchar *path, const gchar *dir)
{
	gchar *entry;
	gboolean result;

	entry = g_strconcat(path, G_DIR_SEPARATOR_S, dir, NULL);
	result = is_dir_exist(entry);
	g_free(entry);

	return result;
}

/*
 * check whether PATH is a Maildir style mailbox.
 * This is the case if the 3 subdir: new, cur, tmp are existing.
 * This functon assumes that entry is an directory
 */
static gboolean mh_is_maildir(const gchar *path)
{
	return mh_is_maildir_one(path, "new") &&
	       mh_is_maildir_one(path, "cur") &&
	       mh_is_maildir_one(path, "tmp");
}
#endif

static gboolean mh_remove_missing_folder_items_func(GNode *node, gpointer data)
{
	FolderItem *item;
	gchar *path;

	g_return_val_if_fail(node->data != NULL, FALSE);

	if (G_NODE_IS_ROOT(node))
		return FALSE;

	item = FOLDER_ITEM(node->data);

	path = folder_item_get_path(item);
	if (!is_dir_exist(path)) {
		debug_print("folder '%s' not found. removing...\n", path);
		folder_item_remove(item);
	}
	g_free(path);

	return FALSE;
}

static void mh_remove_missing_folder_items(Folder *folder)
{
	g_return_if_fail(folder != NULL);

	debug_print("searching missing folders...\n");

	g_node_traverse(folder->node, G_POST_ORDER, G_TRAVERSE_ALL, -1,
			mh_remove_missing_folder_items_func, folder);
}

static void mh_scan_tree_recursive(FolderItem *item)
{
	Folder *folder;
	DIR *dp;
	struct dirent *d;
	struct stat s;
 	gchar *real_path, *entry, *utf8entry, *utf8name;
	gint n_msg = 0;

	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);

	folder = item->folder;

	real_path = item->path ? mh_filename_from_utf8(item->path) : g_strdup(".");
	dp = opendir(real_path);
	if (!dp) {
		FILE_OP_ERROR(real_path, "opendir");
		return;
	}
	g_free(real_path);

	debug_print("scanning %s ...\n",
		    item->path ? item->path
		    : LOCAL_FOLDER(item->folder)->rootpath);
	if (folder->ui_func)
		folder->ui_func(folder, item, folder->ui_func_data);

	while ((d = readdir(dp)) != NULL) {
		if (d->d_name[0] == '.') continue;

		utf8name = mh_filename_to_utf8(d->d_name);
		if (item->path)
			utf8entry = g_strconcat(item->path, G_DIR_SEPARATOR_S,
						utf8name, NULL);
		else
			utf8entry = g_strdup(utf8name);
		entry = mh_filename_from_utf8(utf8entry);

		if (
#ifdef HAVE_DIRENT_D_TYPE
			d->d_type == DT_DIR ||
			(d->d_type == DT_UNKNOWN &&
#endif
			stat(entry, &s) == 0 && S_ISDIR(s.st_mode)
#ifdef HAVE_DIRENT_D_TYPE
			)
#endif
		   ) {
			FolderItem *new_item = NULL;
			GNode *node;

#if 0
			if (mh_is_maildir(entry)) {
				g_free(entry);
				g_free(utf8entry);
				g_free(utf8name);
				continue;
			}
#endif

			node = item->node;
			for (node = node->children; node != NULL; node = node->next) {
				FolderItem *cur_item = FOLDER_ITEM(node->data);
				if (!strcmp2(cur_item->path, entry)) {
					new_item = cur_item;
					break;
				}
			}
			if (!new_item) {
				debug_print("new folder '%s' found.\n", entry);
				new_item = folder_item_new(folder, utf8name, utf8entry);
				folder_item_append(item, new_item);
			}

			if (!item->path) {
				if (!folder->inbox &&
				    !strcmp(d->d_name, INBOX_DIR)) {
					new_item->stype = F_INBOX;
					folder->inbox = new_item;
				} else if (!folder->outbox &&
					   !strcmp(d->d_name, OUTBOX_DIR)) {
					new_item->stype = F_OUTBOX;
					folder->outbox = new_item;
				} else if (!folder->draft &&
					   !strcmp(d->d_name, DRAFT_DIR)) {
					new_item->stype = F_DRAFT;
					folder->draft = new_item;
				} else if (!folder->queue &&
					   !strcmp(d->d_name, QUEUE_DIR)) {
					new_item->stype = F_QUEUE;
					folder->queue = new_item;
				} else if (!folder->trash &&
					   !strcmp(d->d_name, TRASH_DIR)) {
					new_item->stype = F_TRASH;
					folder->trash = new_item;
				}
			}

			mh_scan_tree_recursive(new_item);
		} else if (to_number(d->d_name) != -1) n_msg++;

		g_free(entry);
		g_free(utf8entry);
		g_free(utf8name);
	}

	closedir(dp);

	item->mtime = time(NULL);

/*
	if (item->path) {
		gint new, unread, total, min, max;

		procmsg_get_mark_sum(item->path, &new, &unread, &total,
				     &min, &max, 0);
		if (n_msg > total) {
			new += n_msg - total;
			unread += n_msg - total;
		}
		item->new = new;
		item->unread = unread;
		item->total = n_msg;
	}
*/
}

static gboolean mh_rename_folder_func(GNode *node, gpointer data)
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
		g_warning("path doesn't match: %s, %s\n", oldpath, item->path);
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

static gchar *mh_filename_from_utf8(const gchar *path)
{
	gchar *real_path = g_filename_from_utf8(path, -1, NULL, NULL, NULL);

	if (!real_path) {
		g_warning("mh_filename_from_utf8: faild to convert character set\n");
		real_path = g_strdup(path);
	}

	return real_path;
}

static gchar *mh_filename_to_utf8(const gchar *path)
{
	gchar *utf8path = g_filename_to_utf8(path, -1, NULL, NULL, NULL);
	if (!utf8path) {
		g_warning("mh_filename_to_utf8: faild to convert character set\n");
		utf8path = g_strdup(path);
	}

	return utf8path;
}
