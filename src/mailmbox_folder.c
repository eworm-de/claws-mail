/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2003 Hiroyuki Yamamoto
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
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#undef MEASURE_TIME

#ifdef MEASURE_TIME
#  include <sys/time.h>
#endif

#include "intl.h"
#include "folder.h"
#include "procmsg.h"
#include "procheader.h"
#include "utils.h"
#include "mailmbox.h"
#include "mailmbox_folder.h"
#include "mailmbox_parse.h"

static Folder *s_mailmbox_folder_new(const gchar *name, const gchar *path);

static void mailmbox_folder_destroy(Folder *folder);

static FolderItem *mailmbox_folder_item_new(Folder *folder);

static void mailmbox_folder_item_destroy(Folder *folder, FolderItem *_item);

static gchar *mailmbox_item_get_path(Folder *folder, FolderItem *item);

static gint mailmbox_get_num_list(Folder *folder, FolderItem *item,
    GSList **list, gboolean *old_uids_valid);

static MsgInfo *mailmbox_get_msginfo(Folder *folder,
    FolderItem *item, gint num);

static GSList *mailmbox_get_msginfos(Folder *folder, FolderItem *item,
    GSList *msgnum_list);

static gchar *s_mailmbox_fetch_msg(Folder *folder, FolderItem *item, gint num);

static gint mailmbox_add_msg(Folder *folder, FolderItem *dest,
    const gchar *file, MsgFlags *flags);

static gint mailmbox_add_msgs(Folder *folder, FolderItem *dest,
    GSList *file_list, 
    GRelation *relation);

static gint s_mailmbox_copy_msg(Folder *folder,
    FolderItem *dest, MsgInfo *msginfo);

static gint mailmbox_copy_msgs(Folder *folder, FolderItem *dest, 
    MsgInfoList *msglist, GRelation *relation);

static gint mailmbox_remove_msg(Folder *folder, FolderItem *item, gint num);

static gint mailmbox_remove_all_msg(Folder *folder, FolderItem *item);

static FolderItem *mailmbox_create_folder(Folder *folder, FolderItem *parent,
    const gchar *name);

static gboolean mailmbox_scan_required(Folder *folder, FolderItem *_item);

static gint mailmbox_rename_folder(Folder *folder,
    FolderItem *item, const gchar *name);

static gint mailmbox_remove_folder(Folder *folder, FolderItem *item);

static gint mailmbox_create_tree(Folder *folder);

static FolderClass mailmbox_class =
{
	F_MBOX,
	"mbox",
	"MBOX",

	/* Folder functions */
	s_mailmbox_folder_new,
	mailmbox_folder_destroy,
	NULL, /* mailmbox_scan_tree, */
	mailmbox_create_tree,

	/* FolderItem functions */
	mailmbox_folder_item_new,
	mailmbox_folder_item_destroy,
	mailmbox_item_get_path,
	mailmbox_create_folder,
	mailmbox_rename_folder,
	mailmbox_remove_folder,
	NULL,
	mailmbox_get_num_list,
	NULL,
	NULL,
	NULL,
	mailmbox_scan_required,
	
	/* Message functions */
	mailmbox_get_msginfo,
	mailmbox_get_msginfos,
	s_mailmbox_fetch_msg,
	mailmbox_add_msg,
	mailmbox_add_msgs,
	s_mailmbox_copy_msg,
	mailmbox_copy_msgs,
	mailmbox_remove_msg,
	mailmbox_remove_all_msg,
	NULL,
	NULL,
};

FolderClass *mailmbox_get_class(void)
{
	return &mailmbox_class;
}


static void mailmbox_folder_init(Folder *folder,
    const gchar *name, const gchar *path)
{
	folder_local_folder_init(folder, name, path);
}

static Folder *s_mailmbox_folder_new(const gchar *name, const gchar *path)
{
	Folder *folder;

	folder = (Folder *)g_new0(MBOXFolder, 1);
	folder->klass = &mailmbox_class;
        mailmbox_folder_init(folder, name, path);
        
	return folder;
}

static void mailmbox_folder_destroy(Folder *folder)
{
	folder_local_folder_destroy(LOCAL_FOLDER(folder));
}

typedef struct _MBOXFolderItem	MBOXFolderItem;
struct _MBOXFolderItem
{
	FolderItem item;
        guint old_max_uid;
        struct mailmbox_folder * mbox;
};

static FolderItem *mailmbox_folder_item_new(Folder *folder)
{
	MBOXFolderItem *item;
	
	item = g_new0(MBOXFolderItem, 1);
	item->mbox = NULL;
        item->old_max_uid = 0;

	return (FolderItem *)item;
}

#define MAX_UID_FILE "max-uid"

void read_max_uid_value(FolderItem *item, guint * pmax_uid)
{
        gchar * path;
        gchar * file;
        FILE * f;
        guint max_uid;
        size_t r;
        
	path = folder_item_get_path(item);
	file = g_strconcat(path, G_DIR_SEPARATOR_S, MAX_UID_FILE, NULL);
	g_free(path);
        
        f = fopen(file, "r");
        g_free(file);
        if (f == NULL)
                return;
        r = fread(&max_uid, sizeof(max_uid), 1, f);
        if (r == 0) {
                fclose(f);
                return;
        }
        
        fclose(f);
        
        * pmax_uid = max_uid;
}

void write_max_uid_value(FolderItem *item, guint max_uid)
{
        gchar * path;
        gchar * file;
        FILE * f;
        size_t r;
        
	path = folder_item_get_path(item);
	file = g_strconcat(path, G_DIR_SEPARATOR_S, MAX_UID_FILE, NULL);
	g_free(path);
        
        f = fopen(file, "w");
        g_free(file);
        if (f == NULL)
                return;
        r = fwrite(&max_uid, sizeof(max_uid), 1, f);
        if (r == 0) {
                fclose(f);
                return;
        }
        
        fclose(f);
}

static void mailmbox_folder_item_destroy(Folder *folder, FolderItem *_item)
{
	MBOXFolderItem *item = (MBOXFolderItem *)_item;

	g_return_if_fail(item != NULL);
        
        if (item->mbox != NULL) {
                write_max_uid_value(_item, item->mbox->written_uid);
                mailmbox_done(item->mbox);
        }
	g_free(_item);
}

static void mailmbox_folder_create_parent(const gchar * path)
{
	if (!is_file_exist(path)) {
		gchar * new_path;

		new_path = g_dirname(path);
		if (new_path[strlen(new_path) - 1] == G_DIR_SEPARATOR)
			new_path[strlen(new_path) - 1] = '\0';

		if (!is_dir_exist(new_path))
			make_dir_hier(new_path);
		g_free(new_path);
		
	}
}

static gchar * mailmbox_folder_get_path(Folder *folder, FolderItem *item)
{
	gchar *folder_path;
	gchar *path;

	g_return_val_if_fail(item != NULL, NULL);

	if (item->path && item->path[0] == G_DIR_SEPARATOR) {
		mailmbox_folder_create_parent(item->path);
		return g_strdup(item->path);
	}

	folder_path = g_strdup(LOCAL_FOLDER(item->folder)->rootpath);
	g_return_val_if_fail(folder_path != NULL, NULL);

	if (folder_path[0] == G_DIR_SEPARATOR) {
		if (item->path) {
			path = g_strconcat(folder_path, G_DIR_SEPARATOR_S,
					   item->path, NULL);
		}
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
	
	mailmbox_folder_create_parent(path);

	return path;
}

static int mailmbox_item_sync(FolderItem *_item, int validate_uid)
{
	MBOXFolderItem *item = (MBOXFolderItem *)_item;
        int r;

        if (item->mbox == NULL) {
                guint written_uid;
                gchar * path;
                
                written_uid = 0;
                read_max_uid_value(_item, &written_uid);
                path = mailmbox_folder_get_path(_item->folder, _item);
                r = mailmbox_init(path, 0, 0, written_uid, &item->mbox);
                g_free(path);
                if (r != MAILMBOX_NO_ERROR)
                        return -1;
        }

        if (!validate_uid) {
                r = mailmbox_validate_read_lock(item->mbox);
                if (r != MAILMBOX_NO_ERROR) {
                        goto err;
                }
                
                mailmbox_read_unlock(item->mbox);
        }
        else {
                r = mailmbox_validate_write_lock(item->mbox);
                if (r != MAILMBOX_NO_ERROR) {
                        goto err;
                }
                
                if (item->mbox->written_uid < item->mbox->max_uid) {
                        r = mailmbox_expunge_no_lock(item->mbox);
                        if (r != MAILMBOX_NO_ERROR)
                                goto unlock;
                }
                mailmbox_write_unlock(item->mbox);
        }
        
        return 0;
        
 unlock:
        mailmbox_write_unlock(item->mbox);
 err:
        return -1;
}

static struct mailmbox_folder * get_mbox(FolderItem *_item, int validate_uid)
{
	MBOXFolderItem *item = (MBOXFolderItem *)_item;
        
        mailmbox_item_sync(_item, validate_uid);
        
        return item->mbox;
}

static gint mailmbox_get_num_list(Folder *folder, FolderItem *item,
    GSList **list, gboolean *old_uids_valid)
{
	gint nummsgs = 0;
        guint i;
        struct mailmbox_folder * mbox;

	g_return_val_if_fail(item != NULL, -1);

	debug_print("mbox_get_last_num(): Scanning %s ...\n", item->path);
        
	*old_uids_valid = TRUE;
        
        mbox = get_mbox(item, 1);
        if (mbox == NULL)
                return -1;
        
        for(i = 0 ; i < carray_count(mbox->tab) ; i ++) {
                struct mailmbox_msg_info * msg;
                
                msg = carray_get(mbox->tab, i);
                if (msg != NULL) {
			*list = g_slist_prepend(*list,
                            GINT_TO_POINTER(msg->uid));
                        nummsgs ++;
                }
        }
        
	return nummsgs;
}

static gchar *s_mailmbox_fetch_msg(Folder *folder, FolderItem *item, gint num)
{
	gchar *path;
	gchar *file;
        int r;
        struct mailmbox_folder * mbox;
        char * data;
        size_t len;
        FILE * f;
        mode_t old_mask;

	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(num > 0, NULL);
        
        mbox = get_mbox(item, 0);
        if (mbox == NULL)
                return NULL;

	path = folder_item_get_path(item);
	file = g_strconcat(path, G_DIR_SEPARATOR_S, itos(num), NULL);
	g_free(path);
	if (is_file_exist(file)) {
		return file;
	}
        
        r = mailmbox_fetch_msg(mbox, num, &data, &len);
        if (r != MAILMBOX_NO_ERROR)
                goto free;
        
        fprintf(stderr, file);
        old_mask = umask(0077);
        f = fopen(file, "w");
        umask(old_mask);
        if (f == NULL)
                goto free_data;
        
        r = fwrite(data, 1, len, f);
        if (r == 0)
                goto close;
        
        fclose(f);
        free(data);
        
	return file;
        
 close:
        fclose(f);
        unlink(file);
 free_data:
        free(data);
 free:
        free(file);
 err:
        return NULL;
}

static MsgInfo *mailmbox_parse_msg(guint uid,
    char * data, size_t len, FolderItem *item)
{
	MsgInfo *msginfo;
	MsgFlags flags;

	flags.perm_flags = MSG_NEW|MSG_UNREAD;
	flags.tmp_flags = 0;

	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(data != NULL, NULL);

	if (item->stype == F_QUEUE) {
		MSG_SET_TMP_FLAGS(flags, MSG_QUEUED);
	} else if (item->stype == F_DRAFT) {
		MSG_SET_TMP_FLAGS(flags, MSG_DRAFT);
	}

        msginfo = procheader_parse_str(data, flags, FALSE, FALSE);
	if (!msginfo) return NULL;

	msginfo->msgnum = uid;
	msginfo->folder = item;

	return msginfo;
}

static MsgInfo *mailmbox_get_msginfo(Folder *folder,
    FolderItem *item, gint num)
{
	MsgInfo *msginfo;
        int r;
        char * data;
        size_t len;
        struct mailmbox_folder * mbox;

	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(num > 0, NULL);

        mbox = get_mbox(item, 0);
        if (mbox == NULL)
                goto err;

        r = mailmbox_validate_read_lock(mbox);
        if (r != MAILMBOX_NO_ERROR)
                goto err;
        
        r = mailmbox_fetch_msg_headers_no_lock(mbox, num, &data, &len);
        if (r != MAILMBOX_NO_ERROR)
                goto unlock;
        
	msginfo = mailmbox_parse_msg(num, data, len, item);
	if (!msginfo)
                goto unlock;

        msginfo->msgnum = num;

        mailmbox_read_unlock(mbox);

	return msginfo;

 unlock:
        mailmbox_read_unlock(mbox);
 err:
        return NULL;
}

static GSList *mailmbox_get_msginfos(Folder *folder, FolderItem *item,
    GSList *msgnum_list)
{
        int r;
        GSList * cur;
        GSList * ret;
        struct mailmbox_folder * mbox;
        
	g_return_val_if_fail(item != NULL, NULL);

        mbox = get_mbox(item, 0);
        if (mbox == NULL)
                goto err;

        r = mailmbox_validate_read_lock(mbox);
        if (r != MAILMBOX_NO_ERROR)
                goto err;

        ret = NULL;
        
        for (cur = msgnum_list ; cur != NULL ; cur = g_slist_next(cur)) {
                char * data;
                size_t len;
                gint num;
                MsgInfo *msginfo;
                
                num = GPOINTER_TO_INT(cur->data);
                
                r = mailmbox_fetch_msg_headers_no_lock(mbox, num, &data, &len);
                if (r != MAILMBOX_NO_ERROR)
                        continue;
                
                msginfo = mailmbox_parse_msg(num, data, len, item);
                if (!msginfo)
                        continue;
                
                msginfo->msgnum = num;
                
                ret = g_slist_append(ret, msginfo);
        }
        
        mailmbox_read_unlock(mbox);

	return ret;

 unlock:
        mailmbox_read_unlock(mbox);
 err:
        return NULL;
}

/* ok */

static gint mailmbox_add_msg(Folder *folder, FolderItem *dest,
    const gchar *file, MsgFlags *flags)
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

	ret = mailmbox_add_msgs(folder, dest, &file_list, NULL);
	return ret;
} 

/* ok */
 
static gint mailmbox_add_msgs(Folder *folder, FolderItem *dest,
    GSList *file_list, 
    GRelation *relation)
{ 
	gchar *destfile;
	GSList *cur;
        gint last_num;
        struct mailmbox_folder * mbox;
        carray * append_list;
        struct mailmbox_append_info append_info;
        int r;

	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(file_list != NULL, -1);
        
        mbox = get_mbox(dest, 0);
        if (mbox == NULL)
                return -1;

        r = mailmbox_validate_write_lock(mbox);
        if (r != MAILMBOX_NO_ERROR)
                return -1;
        
        r = mailmbox_expunge_no_lock(mbox);
        if (r != MAILMBOX_NO_ERROR)
                goto unlock;
        
        last_num = -1;

        append_list = carray_new(1);
        if (append_list == NULL)
                goto unlock;

        r = carray_set_size(append_list, 1);
        if (r < 0)
                goto free;
        
        carray_set(append_list, 0, &append_info);
        
	for (cur = file_list; cur != NULL; cur = cur->next) {
                int fd;
                struct stat stat_info;
                char * data;
                size_t len;
                struct mailmbox_msg_info * msg;
                size_t cur_token;
                MsgFileInfo *fileinfo;
                
		fileinfo = (MsgFileInfo *)cur->data;
                
                fd = open(fileinfo->file, O_RDONLY);
                if (fd == -1)
                        goto err;
                
                r = fstat(fd, &stat_info);
                if (r < 0)
                        goto close;
                
                len = stat_info.st_size;
                data = mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0);
                if (data == MAP_FAILED)
                        goto close;
                
                append_info.message = data;
                append_info.size = len;
                
                cur_token = mbox->mapping_size;
                
                r = mailmbox_append_message_list_no_lock(mbox, append_list);
                if (r != MAILMBOX_NO_ERROR)
                        goto unmap;

                munmap(data, len);
                close(fd);
                
                mailmbox_sync(mbox);
                
                r = mailmbox_parse_additionnal(mbox, &cur_token);
                if (r != MAILMBOX_NO_ERROR)
                        goto unlock;
                
                msg = carray_get(mbox->tab, carray_count(mbox->tab) - 1);

                if (relation != NULL)
                        g_relation_insert(relation, fileinfo,
                            GINT_TO_POINTER(msg->uid));
                
                last_num = msg->uid;
                
                continue;
                
        unmap:
                munmap(data, len);
        close:
                close(fd);
        err:
                continue;
        }
        
        carray_free(append_list);
        mailmbox_write_unlock(mbox);
        
	return last_num;

 free:
        carray_free(append_list);
 unlock:
        mailmbox_write_unlock(mbox);
return -1;
}

static gint s_mailmbox_copy_msg(Folder *folder,
    FolderItem *dest, MsgInfo *msginfo)
{
	GSList msglist;

	g_return_val_if_fail(msginfo != NULL, -1);

	msglist.data = msginfo;
	msglist.next = NULL;

	return mailmbox_copy_msgs(folder, dest, &msglist, NULL);
}

static gint mailmbox_copy_msgs(Folder *folder, FolderItem *dest, 
    MsgInfoList *msglist, GRelation *relation)
{
	MsgInfo *msginfo;
	GSList *file_list;
	gint ret;
        
	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(msglist != NULL, -1);

	msginfo = (MsgInfo *)msglist->data;
	g_return_val_if_fail(msginfo->folder != NULL, -1);

	file_list = procmsg_get_message_file_list(msglist);
	g_return_val_if_fail(file_list != NULL, -1);

	ret = mailmbox_add_msgs(folder, dest, file_list, relation);

	procmsg_message_file_list_free(file_list);

	return ret;
}


static gint mailmbox_remove_msg(Folder *folder, FolderItem *item, gint num)
{
        struct mailmbox_folder * mbox;
        int r;
        
	g_return_val_if_fail(item != NULL, -1);
        
        mbox = get_mbox(item, 0);
        if (mbox == NULL)
                return -1;
        
        r = mailmbox_delete_msg(mbox, num);
        if (r != MAILMBOX_NO_ERROR)
                return -1;

	return 0;
}

static gint mailmbox_remove_all_msg(Folder *folder, FolderItem *item)
{
        struct mailmbox_folder * mbox;
        int r;
        guint i;
        
	g_return_val_if_fail(item != NULL, -1);
        
        mbox = get_mbox(item, 0);
        if (mbox == NULL)
                return -1;
       
        for(i = 0 ; i < carray_count(mbox->tab) ; i ++) {
                struct mailmbox_msg_info * msg;
                
                msg = carray_get(mbox->tab, i);
                if (msg == NULL)
                        continue;
                
                r = mailmbox_delete_msg(mbox, msg->uid);
                if (r != MAILMBOX_NO_ERROR)
                        continue;
        }
        
	return 0;
}


static gchar * mailmbox_get_new_path(FolderItem * parent, gchar * name)
{
	gchar * path;

	if (strchr(name, G_DIR_SEPARATOR) == NULL) {
		if (parent->path != NULL)
			path = g_strconcat(parent->path, ".sbd", G_DIR_SEPARATOR_S, name, NULL);
		else
			path = g_strdup(name);
	}
	else
		path = g_strdup(name);

	return path;
}

static gchar * mailmbox_get_folderitem_name(gchar * name)
{
	gchar * foldername;

	foldername = g_strdup(g_basename(name));
	
	return foldername;
}

static FolderItem *mailmbox_create_folder(Folder *folder, FolderItem *parent,
    const gchar *name)
{
	gchar * path;
	FolderItem *new_item;
	gchar * foldername;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(parent != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);

	path = mailmbox_get_new_path(parent, (gchar *) name);

	foldername = mailmbox_get_folderitem_name((gchar *) name);

	new_item = folder_item_new(folder, foldername, path);
	folder_item_append(parent, new_item);

	if (!strcmp(name, "inbox")) {
		new_item->stype = F_INBOX;
		new_item->folder->inbox = new_item;
	} else if (!strcmp(name, "outbox")) {
		new_item->stype = F_OUTBOX;
		new_item->folder->outbox = new_item;
	} else if (!strcmp(name, "draft")) {
		new_item->stype = F_DRAFT;
		new_item->folder->draft = new_item;
	} else if (!strcmp(name, "queue")) {
		new_item->stype = F_QUEUE;
		new_item->folder->queue = new_item;
	} else if (!strcmp(name, "trash")) {
		new_item->stype = F_TRASH;
		new_item->folder->trash = new_item;
	}
	
	g_free(foldername);
	g_free(path);
	
	return new_item;
}



static gboolean mailmbox_scan_required(Folder *folder, FolderItem *_item)
{
        struct mailmbox_folder * mbox;
	MBOXFolderItem *item = (MBOXFolderItem *)_item;
        int scan_required;
        
	g_return_val_if_fail(folder != NULL, FALSE);
	g_return_val_if_fail(item != NULL, FALSE);

	if (item->item.path == NULL)
		return FALSE;
        
        mbox = get_mbox(_item, 0);
        if (mbox == NULL)
                return FALSE;
        
        scan_required = (item->old_max_uid != item->mbox->max_uid);
        
        item->old_max_uid = item->mbox->max_uid;

        return scan_required;
}


static gint mailmbox_rename_folder(Folder *folder,
    FolderItem *item, const gchar *name)
{
	gchar * path;
	gchar * foldername;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(item != NULL, -1);
	g_return_val_if_fail(item->path != NULL, -1);
	g_return_val_if_fail(name != NULL, -1);

	path = mailmbox_get_new_path(item->parent, (gchar *) name);
	foldername = mailmbox_get_folderitem_name((gchar *) name);

	if (rename(item->path, path) == -1) {
		g_free(foldername);
		g_free(path);
		g_warning("Cannot rename folder item");

		return -1;
	}
	else {
		g_free(item->name);
		g_free(item->path);
		item->path = path;
		item->name = foldername;
		
		return 0;
	}
}

static gint mailmbox_remove_folder(Folder *folder, FolderItem *item)
{
	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(item != NULL, -1);
	g_return_val_if_fail(item->path != NULL, -1);

	folder_item_remove(item);
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
		if (mkdir(dir, S_IRWXU) < 0) { \
			FILE_OP_ERROR(dir, "mkdir"); \
			return -1; \
		} \
		if (chmod(dir, S_IRWXU) < 0) \
			FILE_OP_ERROR(dir, "chmod"); \
	} \
}

static gint mailmbox_create_tree(Folder *folder)
{
	gchar *rootpath;

	g_return_val_if_fail(folder != NULL, -1);

	CHDIR_RETURN_VAL_IF_FAIL(get_home_dir(), -1);
	rootpath = LOCAL_FOLDER(folder)->rootpath;
	MAKE_DIR_IF_NOT_EXIST(rootpath);
	CHDIR_RETURN_VAL_IF_FAIL(rootpath, -1);

	return 0;
}

#undef MAKE_DIR_IF_NOT_EXIST


static char * quote_mailbox(char * mb)
{
        char path[PATH_MAX];
        char * str;
        size_t remaining;
        char * p;
        
        remaining = sizeof(path) - 1;
        p = path;

        while (* mb != 0) {
                char hex[3];
                
                if (((* mb >= 'a') && (* mb <= 'z')) ||
                    ((* mb >= 'A') && (* mb <= 'Z')) ||
                    ((* mb >= '0') && (* mb <= '9'))) {
                        if (remaining < 1)
                                return NULL;
                        * p = * mb;
                        p ++;
                        remaining --;
                }
                else {
                        if (remaining < 3)
                                return NULL;
                        * p = '%';
                        p ++;
                        snprintf(p, 3, "%02x", (unsigned char) (* mb));
                        p += 2;
                }
                mb ++;
        }
        
        * p = 0;

        str = strdup(path);
        if (str == NULL)
                return NULL;
        
        return str;
}

static gchar *mailmbox_item_get_path(Folder *folder, FolderItem *item)
{
	gchar *itempath, *path;
        gchar * folderpath;

        if (item->path == NULL)
                return NULL;

        if (folder->name == NULL)
                return NULL;

        folderpath = quote_mailbox(folder->name);
        if (folderpath == NULL)	 
    		return NULL;
        itempath = quote_mailbox(item->path);
        if (itempath == NULL) {
                free(folderpath);
    		return NULL;
        }
        path = g_strconcat(get_mbox_cache_dir(),
            G_DIR_SEPARATOR_S, folderpath,
            G_DIR_SEPARATOR_S, itempath, NULL);	 
        free(itempath);
        free(folderpath);
        
	return path;
}
