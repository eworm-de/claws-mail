/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2002 Hiroyuki Yamamoto
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
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#undef MEASURE_TIME

#ifdef MEASURE_TIME
#  include <sys/time.h>
#endif

#include "intl.h"
#include "folder.h"
#include "mh.h"
#include "procmsg.h"
#include "procheader.h"
#include "utils.h"

static void	mh_folder_init			(Folder		*folder,
						 const gchar	*name,
						 const gchar	*path);

GSList  *mh_get_msg_list	(Folder		*folder,
				 FolderItem	*item,
				 gboolean	 use_cache);
gchar   *mh_fetch_msg		(Folder		*folder,
				 FolderItem	*item,
				 gint		 num);
MsgInfo   *mh_fetch_msginfo	(Folder		*folder,
				 FolderItem	*item,
				 gint		 num);
gint     mh_add_msg		(Folder		*folder,
				 FolderItem	*dest,
				 const gchar	*file,
				 gboolean	 remove_source);
gint     mh_move_msg		(Folder		*folder,
				 FolderItem	*dest,
				 MsgInfo	*msginfo);
gint     mh_move_msgs_with_dest	(Folder		*folder,
				 FolderItem	*dest,
				 GSList		*msglist);
gint     mh_copy_msg		(Folder		*folder,
				 FolderItem	*dest,
				 MsgInfo	*msginfo);
gint     mh_copy_msgs_with_dest	(Folder		*folder,
				 FolderItem	*dest,
				 GSList		*msglist);
gint     mh_remove_msg		(Folder		*folder,
				 FolderItem	*item,
				 gint		 num);
gint     mh_remove_all_msg	(Folder		*folder,
				 FolderItem	*item);
gboolean mh_is_msg_changed	(Folder		*folder,
				 FolderItem	*item,
				 MsgInfo	*msginfo);

gint    mh_scan_folder		(Folder		*folder,
				 FolderItem	*item);
GSList *mh_get_num_list		(Folder		*folder,
				 FolderItem	*item);
void    mh_scan_tree		(Folder		*folder);

gint    mh_create_tree		(Folder		*folder);
FolderItem *mh_create_folder	(Folder		*folder,
				 FolderItem	*parent,
				 const gchar	*name);
gint    mh_rename_folder	(Folder		*folder,
				 FolderItem	*item,
				 const gchar	*name);
gint    mh_remove_folder	(Folder		*folder,
				 FolderItem	*item);

gchar   *mh_get_new_msg_filename		(FolderItem	*dest);

static GSList  *mh_get_uncached_msgs		(GHashTable	*msg_table,
						 FolderItem	*item);
static MsgInfo *mh_parse_msg			(const gchar	*file,
						 FolderItem	*item);
static void	mh_scan_tree_recursive		(FolderItem	*item, 
						 GHashTable *pptable);

static gboolean mh_rename_folder_func		(GNode		*node,
						 gpointer	 data);


Folder *mh_folder_new(const gchar *name, const gchar *path)
{
	Folder *folder;

	folder = (Folder *)g_new0(MHFolder, 1);
	mh_folder_init(folder, name, path);

	return folder;
}

void mh_folder_destroy(MHFolder *folder)
{
	folder_local_folder_destroy(LOCAL_FOLDER(folder));
}

static void mh_folder_init(Folder *folder, const gchar *name, const gchar *path)
{
	folder_local_folder_init(folder, name, path);

	folder->type = F_MH;

/*
	folder->get_msg_list        = mh_get_msg_list;
*/
	folder->fetch_msg           = mh_fetch_msg;
	folder->fetch_msginfo       = mh_fetch_msginfo;
	folder->add_msg             = mh_add_msg;
	folder->move_msg            = mh_move_msg;
	folder->move_msgs_with_dest = mh_move_msgs_with_dest;
	folder->copy_msg            = mh_copy_msg;
	folder->copy_msgs_with_dest = mh_copy_msgs_with_dest;
	folder->remove_msg          = mh_remove_msg;
	folder->remove_all_msg      = mh_remove_all_msg;
	folder->is_msg_changed      = mh_is_msg_changed;
/*
	folder->scan                = mh_scan_folder;
*/
	folder->get_num_list	    = mh_get_num_list;
	folder->scan_tree           = mh_scan_tree;
	folder->create_tree         = mh_create_tree;
	folder->create_folder       = mh_create_folder;
	folder->rename_folder       = mh_rename_folder;
	folder->remove_folder       = mh_remove_folder;
}

void mh_get_last_num(Folder *folder, FolderItem *item)
{
	gchar *path;
	DIR *dp;
	struct dirent *d;
	struct stat s;
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
		if ((num = to_number(d->d_name)) >= 0 &&
		    stat(d->d_name, &s) == 0 &&
		    S_ISREG(s.st_mode)) {
			if (max < num)
				max = num;
		}
	}
	closedir(dp);

	debug_print(_("Last number in dir %s = %d\n"), item->path, max);
	item->last_num = max;
}

GSList *mh_get_num_list(Folder *folder, FolderItem *item)
{

	gchar *path;
	DIR *dp;
	struct dirent *d;
	struct stat s;
	gint num;
	GSList *list = NULL;

	g_return_val_if_fail(item != NULL, NULL);

	debug_print("mh_get_last_num(): Scanning %s ...\n", item->path);

	path = folder_item_get_path(item);
	g_return_val_if_fail(path != NULL, NULL);
	if (change_dir(path) < 0) {
		g_free(path);
		return NULL;
	}
	g_free(path);

	if ((dp = opendir(".")) == NULL) {
		FILE_OP_ERROR(item->path, "opendir");
		return NULL;
	}

	while ((d = readdir(dp)) != NULL) {
		if ((num = to_number(d->d_name)) >= 0 &&
		    stat(d->d_name, &s) == 0 &&
		    S_ISREG(s.st_mode)) {
			list = g_slist_prepend(list, GINT_TO_POINTER(num));
		}
	}
	closedir(dp);

	return list;
}

GSList *mh_get_msg_list(Folder *folder, FolderItem *item, gboolean use_cache)
{
	GSList *mlist;
	GHashTable *msg_table;
	gchar *path;
	struct stat s;
	gboolean scan_new = TRUE;
#ifdef MEASURE_TIME
	struct timeval tv_before, tv_after, tv_result;

	gettimeofday(&tv_before, NULL);
#endif

	g_return_val_if_fail(item != NULL, NULL);

	path = folder_item_get_path(item);
	if (stat(path, &s) < 0) {
		FILE_OP_ERROR(path, "stat");
	} else {
		time_t mtime;

		mtime = MAX(s.st_mtime, s.st_ctime);
		if (item->mtime == mtime) {
			debug_print("Folder is not modified.\n");
			scan_new = FALSE;
		} else
			item->mtime = mtime;
	}
	g_free(path);

	if (use_cache && !scan_new) {
		mlist = procmsg_read_cache(item, FALSE);
		if (!mlist)
			mlist = mh_get_uncached_msgs(NULL, item);
	} else if (use_cache) {
		GSList *newlist;

		mlist = procmsg_read_cache(item, TRUE);
		msg_table = procmsg_msg_hash_table_create(mlist);

		newlist = mh_get_uncached_msgs(msg_table, item);
		if (msg_table)
			g_hash_table_destroy(msg_table);

		mlist = g_slist_concat(mlist, newlist);
	} else
		mlist = mh_get_uncached_msgs(NULL, item);

	procmsg_set_flags(mlist, item);

#ifdef MEASURE_TIME
	gettimeofday(&tv_after, NULL);

	timersub(&tv_after, &tv_before, &tv_result);
	g_print("mh_get_msg_list: %s: elapsed time: %ld.%06ld sec\n",
		item->path, tv_result.tv_sec, tv_result.tv_usec);
#endif

	return mlist;
}

gchar *mh_fetch_msg(Folder *folder, FolderItem *item, gint num)
{
	gchar *path;
	gchar *file;

	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(num > 0, NULL);

	path = folder_item_get_path(item);
	file = g_strconcat(path, G_DIR_SEPARATOR_S, itos(num), NULL);
	g_free(path);
	if (!is_file_exist(file)) {
		g_free(file);
		return NULL;
	}

	return file;
}

MsgInfo *mh_fetch_msginfo(Folder *folder, FolderItem *item, gint num)
{
	gchar *path;
	gchar *file;
	MsgFlags flags;
	MsgInfo *msginfo;
	struct stat s;

	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(num > 0, NULL);

	path = folder_item_get_path(item);
	file = g_strconcat(path, G_DIR_SEPARATOR_S, itos(num), NULL);
	g_free(path);
	if (!is_file_exist(file)) {
		g_free(file);
		return NULL;
	}

	folder_item_set_default_flags(item, &flags);
	msginfo = procheader_parse_file(file, flags, TRUE, FALSE);
	msginfo->msgnum = num;
	msginfo->folder = item;

	if (stat(file, &s) < 0) {
		FILE_OP_ERROR(file, "stat");
		msginfo->size = 0;
		msginfo->mtime = 0;
	} else {
		msginfo->size = s.st_size;
		msginfo->mtime = s.st_mtime;
	}

	return msginfo;
}

gchar *mh_get_new_msg_filename(FolderItem *dest)
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

#define SET_DEST_MSG_FLAGS(fp, dest, msginfo) \
{ \
	MsgInfo newmsginfo; \
 \
	newmsginfo.msgnum = dest->last_num; \
	newmsginfo.flags = msginfo->flags; \
	if (dest->stype == F_OUTBOX || \
	    dest->stype == F_QUEUE  || \
	    dest->stype == F_DRAFT  || \
	    dest->stype == F_TRASH) \
		MSG_UNSET_PERM_FLAGS(newmsginfo.flags, \
				     MSG_NEW|MSG_UNREAD|MSG_DELETED); \
 \
	procmsg_write_flags(&newmsginfo, fp); \
}

gint mh_add_msg(Folder *folder, FolderItem *dest, const gchar *file,
		gboolean remove_source)
{
	gchar *destfile;

	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(file != NULL, -1);

	if (dest->last_num < 0) {
		mh_get_last_num(folder, dest);
		if (dest->last_num < 0) return -1;
	}

	destfile = mh_get_new_msg_filename(dest);
	g_return_val_if_fail(destfile != NULL, -1);

	if (link(file, destfile) < 0) {
		if (copy_file(file, destfile) < 0) {
			g_warning(_("can't copy message %s to %s\n"),
				  file, destfile);
			g_free(destfile);
			return -1;
		}
	}

	if (remove_source) {
		if (unlink(file) < 0)
			FILE_OP_ERROR(file, "unlink");
	}

	g_free(destfile);
	dest->last_num++;
	return dest->last_num;
}

static gint mh_do_move(Folder *folder, FolderItem *dest, MsgInfo *msginfo)
{
	gchar *destdir;
	gchar *srcfile;
	gchar *destfile;
	FILE *fp;
	gint filemode = 0;
	PrefsFolderItem *prefs;

	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(msginfo != NULL, -1);

	if (msginfo->folder == dest) {
		g_warning(_("the src folder is identical to the dest.\n"));
		return -1;
	}

	if (dest->last_num < 0) {
		mh_get_last_num(folder, dest);
		if (dest->last_num < 0) return -1;
	}

	prefs = dest->prefs;

	destfile = mh_get_new_msg_filename(dest);
	g_return_val_if_fail(destfile != NULL, -1);

	debug_print(_("Moving message %s%c%d to %s ...\n"),
		    msginfo->folder->path, G_DIR_SEPARATOR,
		    msginfo->msgnum, dest->path);
	srcfile = procmsg_get_message_file(msginfo);

	destfile = mh_get_new_msg_filename(dest);
	if(!destfile) return -1;

	srcfile = procmsg_get_message_file(msginfo);

	if (move_file(srcfile, destfile) < 0) {
		g_free(srcfile);
		g_free(destfile);
		return -1;
	}

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

	g_free(destdir);

	return dest->last_num;
}

gint mh_move_msg(Folder *folder, FolderItem *dest, MsgInfo *msginfo)
{
	gchar *srcfile;
 	gint ret = 0;
 
 	g_return_val_if_fail(folder != NULL, -1);
 	g_return_val_if_fail(dest != NULL, -1);
 	g_return_val_if_fail(msginfo != NULL, -1);
 	g_return_val_if_fail(msginfo->folder != NULL, -1);
 
 	if (folder == msginfo->folder->folder)
 		return mh_do_move(folder, dest, msginfo);
 
 	srcfile = procmsg_get_message_file(msginfo);
 	if (!srcfile) return -1;
 
 	ret = mh_add_msg(folder, dest, srcfile, FALSE);
 	g_free(srcfile);
 
 	if (ret != -1) {
 		gchar *destdir;
 		FILE *fp;
 
 		destdir = folder_item_get_path(dest);
 		if ((fp = procmsg_open_mark_file(destdir, TRUE)) == NULL)
 			g_warning(_("Can't open mark file.\n"));
 		else {
 			SET_DEST_MSG_FLAGS(fp, dest, msginfo);
 			fclose(fp);
 		}
 		g_free(destdir);
 
 		ret = folder_item_remove_msg(msginfo->folder, msginfo->msgnum);
 	}
 
 	return ret;
}

static gint mh_do_move_msgs_with_dest(Folder *folder, FolderItem *dest,
				      GSList *msglist)
{
	gchar *srcfile;
	gchar *destfile;
	FILE *fp;
	GSList *cur;
	MsgInfo *msginfo;
	PrefsFolderItem *prefs;

	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(msglist != NULL, -1);

	if (dest->last_num < 0) {
		mh_get_last_num(folder, dest);
		if (dest->last_num < 0) return -1;
	}

	prefs = dest->prefs;

	for (cur = msglist; cur != NULL; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;

		if (msginfo->folder == dest) {
			g_warning(_("the src folder is identical to the dest.\n"));
			continue;
		}
		debug_print(_("Moving message %s%c%d to %s ...\n"),
			    msginfo->folder->path, G_DIR_SEPARATOR,
			    msginfo->msgnum, dest->path);

		destfile = mh_get_new_msg_filename(dest);
		if (!destfile) break;
		srcfile = procmsg_get_message_file(msginfo);
		destfile = mh_get_new_msg_filename(dest);
		if(!destfile) return -1;

		if (move_file(srcfile, destfile) < 0) {
			g_free(srcfile);
			g_free(destfile);
			break;
		}

		g_free(srcfile);
		g_free(destfile);
		dest->last_num++;
	}

	return dest->last_num;
}

gint mh_move_msgs_with_dest(Folder *folder, FolderItem *dest, GSList *msglist)
{
	MsgInfo *msginfo;
	GSList *cur;
	gint ret = 0;

	msginfo = (MsgInfo *)msglist->data;
	if (folder == msginfo->folder->folder)
		return mh_do_move_msgs_with_dest(folder, dest, msglist);

	for (cur = msglist; cur != NULL; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;
		ret = mh_move_msg(folder, dest, msginfo);
		if (ret == -1) break;
	}

	return ret;
}

gint mh_copy_msg(Folder *folder, FolderItem *dest, MsgInfo *msginfo)
{
	gchar *srcfile;
	gchar *destfile;
	gchar *destdir;
	gint filemode = 0;
	PrefsFolderItem *prefs;

	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(msginfo != NULL, -1);

	if (msginfo->folder == dest) {
		g_warning(_("the src folder is identical to the dest.\n"));
		return -1;
	}

	if (dest->last_num < 0) {
		mh_get_last_num(folder, dest);
		if (dest->last_num < 0) return -1;
	}

	prefs = dest->prefs;

	destfile = mh_get_new_msg_filename(dest);
	g_return_val_if_fail(destfile != NULL, -1);

	debug_print(_("Copying message %s%c%d to %s ...\n"),
		    msginfo->folder->path, G_DIR_SEPARATOR,
		    msginfo->msgnum, dest->path);

	srcfile = procmsg_get_message_file(msginfo);
	destfile = mh_get_new_msg_filename(dest);
	if(!destfile) {
		g_free(srcfile);
		return -1;
	}
	
	dest->op_count--;

	if (copy_file(srcfile, destfile) < 0) {
		FILE_OP_ERROR(srcfile, "copy");
		g_free(srcfile);
		g_free(destfile);
		return -1;
	}

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

/*
gint mh_copy_msg(Folder *folder, FolderItem *dest, MsgInfo *msginfo)
{
	Folder * src_folder;
	gchar * filename;
	gint num;
	gchar * destdir;
	FILE * fp;

	src_folder = msginfo->folder->folder;
	
	g_return_val_if_fail(src_folder->fetch_msg != NULL, -1);
	
	filename = src_folder->fetch_msg(src_folder,
					 msginfo->folder,
					 msginfo->msgnum);
	if (filename == NULL)
		return -1;

	num = folder->add_msg(folder, dest, filename, FALSE);

	destdir = folder_item_get_path(dest);

	if (fp) {
		MsgInfo newmsginfo;

		newmsginfo.msgnum = dest->last_num;
		newmsginfo.flags = msginfo->flags;
		if (dest->stype == F_OUTBOX ||
		    dest->stype == F_QUEUE  ||
		    dest->stype == F_DRAFT  ||
		    dest->stype == F_TRASH)
			MSG_UNSET_FLAGS(newmsginfo.flags,
					MSG_NEW|MSG_UNREAD|MSG_DELETED);

		procmsg_write_flags(&newmsginfo, fp);
		fclose(fp);
	}
	
	return num;
}
*/

gint mh_copy_msgs_with_dest(Folder *folder, FolderItem *dest, GSList *msglist)
{
	gchar *srcfile;
	gchar *destfile;
	GSList *cur;
	MsgInfo *msginfo;

	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(msglist != NULL, -1);

	if (dest->last_num < 0) {
		mh_get_last_num(folder, dest);
		if (dest->last_num < 0) return -1;
	}

	for (cur = msglist; cur != NULL; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;

		if (msginfo->folder == dest) {
			g_warning(_("the src folder is identical to the dest.\n"));
			continue;
		}
		debug_print(_("Copying message %s%c%d to %s ...\n"),
			    msginfo->folder->path, G_DIR_SEPARATOR,
			    msginfo->msgnum, dest->path);

		destfile = mh_get_new_msg_filename(dest);
		if (!destfile) break;
		srcfile = procmsg_get_message_file(msginfo);

		if (copy_file(srcfile, destfile) < 0) {
			FILE_OP_ERROR(srcfile, "copy");
			g_free(srcfile);
			g_free(destfile);
			break;
		}

		g_free(srcfile);
		g_free(destfile);
		dest->last_num++;
	}

	return dest->last_num;
}

gint mh_remove_msg(Folder *folder, FolderItem *item, gint num)
{
	gchar *file;
	MsgInfo *msginfo;

	g_return_val_if_fail(item != NULL, -1);

	file = mh_fetch_msg(folder, item, num);
	g_return_val_if_fail(file != NULL, -1);

	if (unlink(file) < 0) {
		FILE_OP_ERROR(file, "unlink");
		g_free(file);
		return -1;
	}

	g_free(file);
	return 0;
}

gint mh_remove_all_msg(Folder *folder, FolderItem *item)
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

gboolean mh_is_msg_changed(Folder *folder, FolderItem *item, MsgInfo *msginfo)
{
	struct stat s;

	if (stat(itos(msginfo->msgnum), &s) < 0 ||
	    msginfo->size  != s.st_size ||
	    msginfo->mtime != s.st_mtime)
		return TRUE;

	return FALSE;
}

gint mh_scan_folder(Folder *folder, FolderItem *item)
{
	gchar *path;
	DIR *dp;
	struct dirent *d;
	struct stat s;
	gint max = 0;
	gint num;
	gint n_msg = 0;

	g_return_val_if_fail(item != NULL, -1);

	debug_print("mh_scan_folder(): Scanning %s ...\n", item->path);

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

	if (folder->ui_func)
		folder->ui_func(folder, item, folder->ui_func_data);

	while ((d = readdir(dp)) != NULL) {
		if ((num = to_number(d->d_name)) >= 0 &&
		    stat(d->d_name, &s) == 0 &&
		    S_ISREG(s.st_mode)) {
/*
			n_msg++;
*/
			if (max < num)
				max = num;
		}
	}
	closedir(dp);

/*
	if (n_msg == 0)
		item->new = item->unread = item->total = 0;
	else {
		gint new, unread, total, min, max;

		procmsg_get_mark_sum(".", &new, &unread, &total, &min, &max, 0);
		if (n_msg > total) {
			new += n_msg - total;
			unread += n_msg - total;
		}
		item->new = new;
		item->unread = unread;
		item->total = n_msg;
	}
*/
	debug_print(_("Last number in dir %s = %d\n"), item->path, max);
	item->last_num = max;

	return 0;
}

void mh_scan_tree(Folder *folder)
{
	FolderItem *item;
	gchar *rootpath;
	GHashTable *pptable;

	g_return_if_fail(folder != NULL);

	pptable = folder_persist_prefs_new(folder);

	prefs_scoring_clear();
	prefs_filtering_clear();
	folder_tree_destroy(folder);
	item = folder_item_new(folder->name, NULL);
	item->folder = folder;
	folder->node = g_node_new(item);

	rootpath = folder_item_get_path(item);
	if (change_dir(rootpath) < 0) {
		g_free(rootpath);
		return;
	}
	g_free(rootpath);

	mh_create_tree(folder);
	mh_scan_tree_recursive(item, pptable);
	
	folder_persist_prefs_free(pptable);

	prefs_matcher_read_config();
}

#define MAKE_DIR_IF_NOT_EXIST(dir) \
{ \
	if (!is_dir_exist(dir)) { \
		if (is_file_exist(dir)) { \
			g_warning(_("File `%s' already exists.\n" \
				    "Can't create folder."), dir); \
			return -1; \
		} \
		if (make_dir(dir) < 0) \
			return -1; \
	} \
}

gint mh_create_tree(Folder *folder)
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

FolderItem *mh_create_folder(Folder *folder, FolderItem *parent,
			     const gchar *name)
{
	gchar *path;
	gchar *fullpath;
	FolderItem *new_item;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(parent != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);

	path = folder_item_get_path(parent);
	if (!is_dir_exist(path))
		make_dir_hier(path);

	fullpath = g_strconcat(path, G_DIR_SEPARATOR_S, name, NULL);
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
	new_item = folder_item_new(name, path);
	folder_item_append(parent, new_item);
	g_free(path);

	return new_item;
}

gint mh_rename_folder(Folder *folder, FolderItem *item, const gchar *name)
{
	gchar *oldpath;
	gchar *dirname;
	gchar *newpath;
	GNode *node;
	gchar *paths[2];

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(item != NULL, -1);
	g_return_val_if_fail(item->path != NULL, -1);
	g_return_val_if_fail(name != NULL, -1);

	oldpath = folder_item_get_path(item);
	if (!is_dir_exist(oldpath))
		make_dir_hier(oldpath);

	dirname = g_dirname(oldpath);
	newpath = g_strconcat(dirname, G_DIR_SEPARATOR_S, name, NULL);
	g_free(dirname);

	if (rename(oldpath, newpath) < 0) {
		FILE_OP_ERROR(oldpath, "rename");
		g_free(oldpath);
		g_free(newpath);
		return -1;
	}

	g_free(oldpath);
	g_free(newpath);

	if (strchr(item->path, G_DIR_SEPARATOR) != NULL) {
		dirname = g_dirname(item->path);
		newpath = g_strconcat(dirname, G_DIR_SEPARATOR_S, name, NULL);
		g_free(dirname);
	} else
		newpath = g_strdup(name);

	g_free(item->name);
	item->name = g_strdup(name);

	node = g_node_find(item->folder->node, G_PRE_ORDER, G_TRAVERSE_ALL,
			   item);
	paths[0] = g_strdup(item->path);
	paths[1] = newpath;
	g_node_traverse(node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
			mh_rename_folder_func, paths);

	g_free(paths[0]);
	g_free(paths[1]);
	return 0;
}

gint mh_remove_folder(Folder *folder, FolderItem *item)
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


static GSList *mh_get_uncached_msgs(GHashTable *msg_table, FolderItem *item)
{
	gchar *path;
	DIR *dp;
	struct dirent *d;
	struct stat s;
	GSList *newlist = NULL;
	GSList *last = NULL;
	MsgInfo *msginfo;
	gint n_newmsg = 0;
	gint num;

	g_return_val_if_fail(item != NULL, NULL);

	path = folder_item_get_path(item);
	g_return_val_if_fail(path != NULL, NULL);
	if (change_dir(path) < 0) {
		g_free(path);
		return NULL;
	}
	g_free(path);

	if ((dp = opendir(".")) == NULL) {
		FILE_OP_ERROR(item->path, "opendir");
		return NULL;
	}

	debug_print(_("\tSearching uncached messages... "));

	if (msg_table) {
		while ((d = readdir(dp)) != NULL) {
			if ((num = to_number(d->d_name)) < 0) continue;
			if (stat(d->d_name, &s) < 0) {
				FILE_OP_ERROR(d->d_name, "stat");
				continue;
			}
			if (!S_ISREG(s.st_mode)) continue;

			msginfo = g_hash_table_lookup
				(msg_table, GUINT_TO_POINTER(num));

			if (!msginfo) {
				/* not found in the cache (uncached message) */
				msginfo = mh_parse_msg(d->d_name, item);
				if (!msginfo) continue;

				if (!newlist)
					last = newlist =
						g_slist_append(NULL, msginfo);
				else {
					last = g_slist_append(last, msginfo);
					last = last->next;
				}
				n_newmsg++;
			}
		}
	} else {
		/* discard all previous cache */
		while ((d = readdir(dp)) != NULL) {
			if (to_number(d->d_name) < 0) continue;
			if (stat(d->d_name, &s) < 0) {
				FILE_OP_ERROR(d->d_name, "stat");
				continue;
			}
			if (!S_ISREG(s.st_mode)) continue;

			msginfo = mh_parse_msg(d->d_name, item);
			if (!msginfo) continue;

			if (!newlist)
				last = newlist = g_slist_append(NULL, msginfo);
			else {
				last = g_slist_append(last, msginfo);
				last = last->next;
			}
			n_newmsg++;
		}
	}

	closedir(dp);

	if (n_newmsg)
		debug_print(_("%d uncached message(s) found.\n"), n_newmsg);
	else
		debug_print(_("done.\n"));

	/* sort new messages in numerical order */
	if (newlist) {
		debug_print(_("\tSorting uncached messages in numerical order... "));
		newlist = g_slist_sort
			(newlist, (GCompareFunc)procmsg_cmp_msgnum_for_sort);
		debug_print(_("done.\n"));
	}

	return newlist;
}

static MsgInfo *mh_parse_msg(const gchar *file, FolderItem *item)
{
	struct stat s;
	MsgInfo *msginfo;
	MsgFlags flags;

	flags.perm_flags = MSG_NEW|MSG_UNREAD;
	flags.tmp_flags = 0;

	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(file != NULL, NULL);

	if (item->stype == F_QUEUE) {
		MSG_SET_TMP_FLAGS(flags, MSG_QUEUED);
	} else if (item->stype == F_DRAFT) {
		MSG_SET_TMP_FLAGS(flags, MSG_DRAFT);
	}

	msginfo = procheader_parse_file(file, flags, FALSE, FALSE);
	if (!msginfo) return NULL;

	msginfo->msgnum = atoi(file);
	msginfo->folder = item;

	if (stat(file, &s) < 0) {
		FILE_OP_ERROR(file, "stat");
		msginfo->size = 0;
		msginfo->mtime = 0;
	} else {
		msginfo->size = s.st_size;
		msginfo->mtime = s.st_mtime;
	}

	return msginfo;
}

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

static void mh_scan_tree_recursive(FolderItem *item, GHashTable *pptable)
{
	DIR *dp;
	struct dirent *d;
	struct stat s;
	gchar *entry;
	gint n_msg = 0;

	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);

	dp = opendir(item->path ? item->path : ".");
	if (!dp) {
		FILE_OP_ERROR(item->path ? item->path : ".", "opendir");
		return;
	}

	debug_print("scanning %s ...\n",
		    item->path ? item->path
		    : LOCAL_FOLDER(item->folder)->rootpath);
	if (item->folder->ui_func)
		item->folder->ui_func(item->folder, item,
				      item->folder->ui_func_data);

	while ((d = readdir(dp)) != NULL) {
		if (d->d_name[0] == '.') continue;

		if (item->path)
			entry = g_strconcat(item->path, G_DIR_SEPARATOR_S,
					    d->d_name, NULL);
		else
			entry = g_strdup(d->d_name);

		if (stat(entry, &s) < 0) {
			FILE_OP_ERROR(entry, "stat");
			g_free(entry);
			continue;
		}

		if (S_ISDIR(s.st_mode)) {
			FolderItem *new_item;

			if (mh_is_maildir(entry)) {
				g_free(entry);
				continue;
			}

			new_item = folder_item_new(d->d_name, entry);
			folder_item_append(item, new_item);
			if (!item->path) {
				if (!strcmp(d->d_name, INBOX_DIR)) {
					new_item->stype = F_INBOX;
					item->folder->inbox = new_item;
				} else if (!strcmp(d->d_name, OUTBOX_DIR)) {
					new_item->stype = F_OUTBOX;
					item->folder->outbox = new_item;
				} else if (!strcmp(d->d_name, DRAFT_DIR)) {
					new_item->stype = F_DRAFT;
					item->folder->draft = new_item;
				} else if (!strcmp(d->d_name, QUEUE_DIR)) {
					new_item->stype = F_QUEUE;
					item->folder->queue = new_item;
				} else if (!strcmp(d->d_name, TRASH_DIR)) {
					new_item->stype = F_TRASH;
					item->folder->trash = new_item;
				}
			}
			folder_item_restore_persist_prefs(new_item, pptable);
			mh_scan_tree_recursive(new_item, pptable);
		} else if (to_number(d->d_name) != -1) n_msg++;

		g_free(entry);
	}

	closedir(dp);

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
