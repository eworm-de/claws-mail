/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2001 Hiroyuki Yamamoto & The Sylpheed Claws Team
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

#include <unistd.h>
#include <fcntl.h>
#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "mbox_folder.h"
#include "folder.h"
#include "procmsg.h"
#include "procheader.h"
#include "utils.h"
#include "intl.h"

#define MSGBUFSIZE	8192

static void	mbox_folder_init		(Folder		*folder,
						 const gchar	*name,
						 const gchar	*path);

static gboolean mbox_write_data(FILE * mbox_fp, FILE * new_fp,
				gchar * new_filename, gint size);
static gboolean mbox_rewrite(gchar * mbox);
static gboolean mbox_purge_deleted(gchar * mbox);
static gchar * mbox_get_new_path(FolderItem * parent, gchar * name);
static gchar * mbox_get_folderitem_name(gchar * name);

MsgInfo *mbox_fetch_msginfo(Folder *folder, FolderItem *item, gint num);
GSList *mbox_get_num_list(Folder *folder, FolderItem *item);
gboolean mbox_check_msgnum_validity(Folder *folder, FolderItem *item);

Folder *mbox_folder_new(const gchar *name, const gchar *path)
{
	Folder *folder;

	folder = (Folder *)g_new0(MBOXFolder, 1);
	mbox_folder_init(folder, name, path);

	return folder;
}

void mbox_folder_destroy(MBOXFolder *folder)
{
	folder_local_folder_destroy(LOCAL_FOLDER(folder));
}

static void mbox_folder_init(Folder *folder, const gchar *name, const gchar *path)
{
	folder_local_folder_init(folder, name, path);

	folder->type = F_MBOX;

/*
	folder->get_msg_list        = mbox_get_msg_list;
*/
	folder->fetch_msg           = mbox_fetch_msg;
	folder->fetch_msginfo	    = mbox_fetch_msginfo;
	folder->add_msg             = mbox_add_msg;
	folder->copy_msg            = mbox_copy_msg;
	folder->remove_msg          = mbox_remove_msg;
	folder->remove_all_msg      = mbox_remove_all_msg;
/*
	folder->scan                = mbox_scan_folder;
*/
	folder->get_num_list	    = mbox_get_num_list;
	folder->create_tree         = mbox_create_tree;
	folder->create_folder       = mbox_create_folder;
	folder->rename_folder       = mbox_rename_folder;
	folder->remove_folder       = mbox_remove_folder;
	folder->check_msgnum_validity
				    = mbox_check_msgnum_validity;
}

static gchar * mbox_folder_create_parent(const gchar * path)
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


static gchar *mbox_folder_get_path(FolderItem *item)
{
	gchar *folder_path;
	gchar *path;

	g_return_val_if_fail(item != NULL, NULL);

	if (item->path && item->path[0] == G_DIR_SEPARATOR) {
		mbox_folder_create_parent(item->path);
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
	
	mbox_folder_create_parent(path);

	return path;
}


/**********************************************************/
/*                                                        */
/*                   file lock                            */
/*                                                        */
/**********************************************************/


static GSList * file_lock = NULL;

static gboolean mbox_file_lock_file(gchar * base)
{
	gchar *lockfile, *locklink;
	gint retry = 0;
	FILE *lockfp;

	lockfile = g_strdup_printf("%s.%d", base, getpid());
	if ((lockfp = fopen(lockfile, "wb")) == NULL) {
		FILE_OP_ERROR(lockfile, "fopen");
		g_warning(_("can't create lock file %s\n"), lockfile);
		g_warning(_("use 'flock' instead of 'file' if possible.\n"));
		g_free(lockfile);
		return FALSE;
	}
	
	fprintf(lockfp, "%d\n", getpid());
	fclose(lockfp);
	
	locklink = g_strconcat(base, ".lock", NULL);
	while (link(lockfile, locklink) < 0) {
		FILE_OP_ERROR(lockfile, "link");
		if (retry >= 5) {
			g_warning(_("can't create %s\n"), lockfile);
			unlink(lockfile);
			g_free(lockfile);
			return -1;
		}
		if (retry == 0)
			g_warning(_("mailbox is owned by another"
				    " process, waiting...\n"));
		retry++;
		sleep(5);
	}
	unlink(lockfile);
	g_free(lockfile);

	return TRUE;
}

static gboolean mbox_fcntl_lockwrite_file(FILE * fp)
{
	struct flock lck;

	lck.l_type = F_WRLCK;
	lck.l_whence = 0;
	lck.l_start = 0;
	lck.l_len = 0;
	
	if (fcntl(fileno(fp), F_SETLK, &lck) < 0)
		return FALSE;
	else
		return TRUE;
}

static gboolean mbox_fcntl_lockread_file(FILE * fp)
{
	struct flock lck;

	lck.l_type = F_RDLCK;
	lck.l_whence = 0;
	lck.l_start = 0;
	lck.l_len = 0;
	
	if (fcntl(fileno(fp), F_SETLK, &lck) < 0)
		return FALSE;
	else
		return TRUE;
}

static gboolean mbox_fcntl_unlock_file(FILE * fp)
{
	struct flock lck;

	lck.l_type = F_UNLCK;
	lck.l_whence = 0;
	lck.l_start = 0;
	lck.l_len = 0;
	
	if (fcntl(fileno(fp), F_SETLK, &lck) < 0)
		return FALSE;
	else
		return TRUE;
}

static gboolean mbox_file_unlock_file(gchar * base)
{
	gchar *lockfile;

	lockfile = g_strdup_printf("%s.lock", base);
	unlink(lockfile);
	g_free(lockfile);

	return TRUE;
}

static gboolean mbox_lockread_file(FILE * fp, gchar * base)
{
	gboolean result;

	result = mbox_fcntl_lockread_file(fp);
	if (!result) {
		if ((result = mbox_file_lock_file(base)) == TRUE) {
			file_lock = g_slist_append(file_lock, g_strdup(base));
			debug_print("lockfile lock %s.\n", base);
		}
		else
			g_warning(_("could not lock read file %s\n"), base);
	}
	else
		debug_print("fcntl lock %s.\n", base);

	return result;
}

static gboolean mbox_lockwrite_file(FILE * fp, gchar * base)
{
	gboolean result;

	result = mbox_fcntl_lockwrite_file(fp);
	if (!result) {
		if ((result = mbox_file_lock_file(base)) == TRUE) {
			file_lock = g_slist_append(file_lock, g_strdup(base));
			debug_print("lockfile lock %s.\n", base);
		}
		else
			g_warning(_("could not lock write file %s\n"), base);
	}
	else
		debug_print("fcntl lock %s.\n", base);

	return result;
}

static gboolean mbox_unlock_file(FILE * fp, gchar * base)
{
	gboolean result = FALSE;
	GSList * l;
	gboolean unlocked = FALSE;

	for(l = file_lock ; l != NULL ; l = g_slist_next(l)) {
		gchar * data = l->data;

		if (strcmp(data, base) == 0) {
			file_lock = g_slist_remove(file_lock, data);
			g_free(data);
			result = mbox_file_unlock_file(base);
			unlocked = TRUE;
			debug_print("lockfile unlock - %s.\n", base);
			break;
		}
	}
	
	if (!unlocked) {
		result = mbox_fcntl_unlock_file(fp);
		debug_print("fcntl unlock - %s.\n", base);
	}

	return result;
}

/**********************************************************/
/*                                                        */
/*                   mbox parsing                         */
/*                                                        */
/**********************************************************/

#define MAILFILE_ERROR_NO_ERROR          0x000
#define MAILFILE_ERROR_FILE_NOT_FOUND    0x001
#define MAILFILE_ERROR_MEMORY            0x002
#define MAILFILE_ERROR_MESSAGE_NOT_FOUND 0x003

static int mailfile_error = MAILFILE_ERROR_NO_ERROR;

#define STATE_BEGIN       0x000
#define STATE_TEXT_READ   0x001
#define STATE_FROM_READ   0x002
#define STATE_FIELD_READ  0x003
#define STATE_END         0x004
#define STATE_END_OF_FILE 0x005
#define STATE_MEM_ERROR   0x006
#define STATE_TEXT_BEGIN  0x007

#define STATE_MASK        0x0FF /* filter state from functions */

#define STATE_RESTORE_POS       0x100 /* go back while reading */

typedef struct _mailfile mailfile;

struct _mailfile
{
	gint count;
	gchar * filename;
	GList * msg_list;
};

struct _message
{
	int msgnum;
	int offset;
	int header;
	int content;
	int end;
	int marked;
	gchar * messageid;
	gchar * fromspace;
	MsgFlags flags;
	MsgFlags old_flags;
	gboolean fetched;
};

#define MSG_IS_INVALID(msg) \
	((msg).perm_flags == (msg).tmp_flags && (msg).tmp_flags == -1)

#define MSG_SET_INVALID(msg) \
	((msg).perm_flags = (msg).tmp_flags = -1)

static int startFrom(char * s)
{
	return (strncmp(s, "From ", 5) == 0);
}

static int startSpace(char * s)
{
	return ((*s == ' ') || (*s == '\t'));
}

static int startEmpty(char * s)
{
	return (*s == '\n');
}

static void free_msg_list(GList * l)
{
	GList * elt = g_list_first(l);

	while (elt)
		{
			g_free(elt->data);
			elt = g_list_next(elt);
		}

	g_list_free(l);
}


static mailfile * mailfile_init_from_file(FILE * f, gchar * filename)
{
	int state;
	GList * msg_list = NULL;
	char * r = NULL;
	char s[256];
	int lastpos = 0;
	int former_pos = 0;
	int ignore_next = 0;
	int msgnum = 0;
	struct _message * data = NULL;
	mailfile * mf;

	state = STATE_BEGIN;


	while (state != STATE_END_OF_FILE) {
		if ((state & STATE_RESTORE_POS) == 0) {
			former_pos = lastpos;
			lastpos = ftell(f);

			r = fgets(s, 256, f);

			if (r != NULL && *r)
				ignore_next = (s[strlen(s) - 1] != '\n');
			else
				ignore_next = 0;
		}

		switch(state & 0x0F) {
		case STATE_BEGIN:
			if (r == NULL)
				state = STATE_END_OF_FILE;
			else if (startFrom(s)) {
				state = STATE_FROM_READ;

				data = g_new0(struct _message, 1);
				if (data == NULL) {
					free_msg_list(msg_list);
					return NULL;
				}
				
				msgnum ++;
				data->msgnum = msgnum;
				data->offset = lastpos;
				data->header = lastpos;
				data->end = 0;
				data->content = 0;
				data->messageid = NULL;
				data->fromspace = NULL;
				MSG_SET_INVALID(data->flags);
				MSG_SET_INVALID(data->old_flags);
				data->fetched = FALSE;
				msg_list = g_list_append(msg_list,
							 (gpointer) data);
			}
			else
				state = STATE_BEGIN;

			break;

		case STATE_TEXT_READ:
			if (r == NULL)
				state = STATE_END;
			else if (startFrom(s))
				state = STATE_END | STATE_RESTORE_POS;
			else
				state = STATE_TEXT_READ;
			break;

		case STATE_TEXT_BEGIN:
			data->content = lastpos;
			if (r == NULL)
				state = STATE_END;
			else if (startFrom(s)) {
				state = STATE_END | STATE_RESTORE_POS;
			}
			else {
				state = STATE_TEXT_READ;
			}
			break;
	  
		case STATE_FROM_READ:
			data->content = lastpos;
			if (r == NULL)
				state = STATE_END;
			else if (startSpace(s))
				state = STATE_FROM_READ;
			else if (startEmpty(s))
				state = STATE_TEXT_READ;
			else
				state = STATE_FIELD_READ;
			break;
	  
		case STATE_FIELD_READ:
			data->content = lastpos;
			if (r == NULL)
				state = STATE_END;
			else if (startSpace(s))
				state = STATE_FIELD_READ;
			else if (startEmpty(s)) {
				state = STATE_TEXT_BEGIN;
			}
			else
				state = STATE_FIELD_READ;
			break;
		}
      
		if ((state & STATE_MASK) == STATE_END) {
			state = STATE_BEGIN | (state & STATE_RESTORE_POS);
			data->end = lastpos;
		}

		if (ignore_next) {
			do {
				r = fgets(s, 256, f);
				if (r == NULL || *r == '\0')
					break;
			}
			while (s[strlen(s) - 1] != '\n');
		}
	}

	mf = (mailfile *) g_new0(struct _mailfile, 1);
	if (mf == NULL) {
		free_msg_list(msg_list);
		mailfile_error = MAILFILE_ERROR_MEMORY;
		return NULL;
	}

	mf->msg_list = g_list_first(msg_list);

	mf->filename = g_strdup(filename);
	mf->count = msgnum;

	mailfile_error = MAILFILE_ERROR_NO_ERROR;

	return mf;
}

static mailfile * mailfile_init(char * filename)
{

	FILE * f;
	mailfile * mf;
  
	f = fopen(filename, "rb");

	if (f == NULL) {
		mailfile_error = MAILFILE_ERROR_FILE_NOT_FOUND;
		return NULL;
	}

	mbox_lockread_file(f, filename);

	mf = mailfile_init_from_file(f, filename);

	mbox_unlock_file(f, filename);

	fclose(f);

	return mf;
}

static void mailfile_done(mailfile * f)
{
	free_msg_list(f->msg_list);
	g_free(f->filename);

	g_free(f);
}

/*
#define MAX_READ 4096

static char * readfile(char * filename, int offset, int max_offset)
{
	char * message;
	int size;
	int pos;
	int max;
	int bread;
	FILE * handle;

	handle = fopen(filename, "rb");

	if (handle == NULL) {
		mailfile_error = MAILFILE_ERROR_FILE_NOT_FOUND;
		return NULL;
	}

	size = max_offset - offset;

	message = (char *) malloc(size + 1);
	if (message == NULL) {
		fclose(handle);
		mailfile_error = MAILFILE_ERROR_MEMORY;
		return NULL;
	}

	fseek(handle, offset, SEEK_SET);

	pos = 0;
	while (pos < size) {
		if ((size - pos) > MAX_READ)
			max = MAX_READ;
		else
			max = (size - pos);

		bread = fread(message + pos, 1, max, handle);

		if (bread != -1)
			pos += bread;

		if (bread < max)
			break;
	}

	message[pos] = 0;

	fclose(handle);

	return message;
}

static char * mailfile_readmsg(mailfile f, int index)
{
	GList * nth;
	int max_offset;
	int offset;
	char * message;
	struct _message * msginfo;

	nth = g_list_nth(f->msg_list, index);

	if (!nth) {
		mailfile_error = MAILFILE_ERROR_MESSAGE_NOT_FOUND;
		return NULL;
	}

	msginfo = (struct _message *)nth->data;

	offset = msginfo->offset;
	max_offset = msginfo->end;
	message = readfile(f->filename, offset, max_offset);

	mailfile_error = MAILFILE_ERROR_NO_ERROR;

	return message;
}

static char * mailfile_readheader(mailfile f, int index)
{
	GList * nth;
	int max_offset;
	int offset;
	char * message;
	struct _message * msginfo;

	nth = g_list_nth(f->msg_list, index);

	if (!nth) {
		mailfile_error = MAILFILE_ERROR_MESSAGE_NOT_FOUND;
		return NULL;
	}

	msginfo = (struct _message *)nth->data;

	offset = msginfo->offset;
	max_offset = msginfo->content;
	message = readfile(f->filename, offset, max_offset);

	mailfile_error = MAILFILE_ERROR_NO_ERROR;

	return message;
}

static int mailfile_count(mailfile * f)
{
	return g_list_length(f->msg_list);
}

static int mailfile_find_deleted(mailfile f, char * filename)
{
	FILE * handle;

	handle = fopen(filename, "rb");

	while (elt) {
		struct _message m = elt->data;
		n = fread(&m.deleted, sizeof(int), 1, handle);
		if (!n)
			break;
		elt = g_list_next(elt);
	}

	fclose(handle);
}
*/



/**********************************************************/
/*                                                        */
/*                   mbox cache operations                */
/*                                                        */
/**********************************************************/

struct _mboxcache {
	gchar * filename;
	mailfile * mf;
	GPtrArray * tab_mf;
	gint mtime;
	gboolean modification;
};

typedef struct _mboxcache mboxcache;

static GHashTable * mbox_cache_table = NULL;

static MsgInfo *mbox_parse_msg(FILE * fp, struct _message * msg,
			       FolderItem *item)
{
	MsgInfo *msginfo;
	MsgFlags flags = { 0, 0 };

	MSG_SET_PERM_FLAGS(flags, MSG_NEW | MSG_UNREAD);

	g_return_val_if_fail(fp != NULL, NULL);

	if (item != NULL) {
		if (item->stype == F_QUEUE) {
			MSG_SET_TMP_FLAGS(flags, MSG_QUEUED);
		} else if (item->stype == F_DRAFT) {
			MSG_SET_TMP_FLAGS(flags, MSG_DRAFT);
		}
	}

	msginfo = procheader_parse_stream(fp, flags, FALSE, FALSE);

	if (!msginfo) return NULL;

	if (item != NULL) {
		msginfo->msgnum = msg->msgnum;
		msginfo->folder = item;
	}

	return msginfo;
}

static void mbox_cache_init()
{
	mbox_cache_table = g_hash_table_new(g_str_hash, g_str_equal);
}

static void mbox_cache_done()
{
	g_hash_table_destroy(mbox_cache_table);
}

static void mbox_cache_free_mbox(mboxcache * cache)
{
	g_hash_table_remove(mbox_cache_table, cache->filename);

	if (cache->mf)
		mailfile_done(cache->mf);
	if (cache->tab_mf)
		g_ptr_array_free(cache->tab_mf, FALSE);
	if (cache->filename)
		g_free(cache->filename);
	g_free(cache);
}

static void mbox_cache_get_msginfo_from_file(FILE * fp, GList * msg_list)
{
	GList * l;
	MsgInfo * msginfo;

	for(l = msg_list ; l != NULL ; l = g_list_next(l)) {
		struct _message * msg;

		msg = (struct _message *) l->data;

		fseek(fp, msg->header, SEEK_SET);
		msginfo = mbox_parse_msg(fp, msg, NULL);
		if (msginfo) {
			if (msginfo->msgid)
				msg->messageid =
					g_strdup(msginfo->msgid);
			if (msginfo->fromspace)
				msg->fromspace =
					g_strdup(msginfo->fromspace);
			msg->flags = msginfo->flags;
			msg->old_flags = msginfo->flags;

			procmsg_msginfo_free(msginfo);
		}
	}
}

static void mbox_cache_get_msginfo(gchar * filename, GList * msg_list)
{
	FILE * fp;

	fp = fopen(filename, "rb");
	if (fp == NULL)
		return;

	mbox_cache_get_msginfo_from_file(fp, msg_list);
	fclose(fp);
}

static mboxcache * mbox_cache_read_mbox(gchar * filename)
{
	mboxcache * cache;
	struct stat s;
	mailfile * mf;
	GList * l;

	if (stat(filename, &s) < 0)
		return NULL;

	mf = mailfile_init(filename);
	if (mf == NULL)
		return NULL;

	cache = g_new0(mboxcache, 1);

	cache->mtime = s.st_mtime;
	cache->mf = mf;
	cache->filename = g_strdup(filename);
	cache->modification = FALSE;

	cache->tab_mf = g_ptr_array_new();
	for(l = mf->msg_list ; l != NULL ; l = g_list_next(l))
		g_ptr_array_add(cache->tab_mf, l->data);

	mbox_cache_get_msginfo(filename, mf->msg_list);

	debug_print(_("read mbox - %s\n"), filename);

	return cache;
}

static mboxcache * mbox_cache_read_mbox_from_file(FILE * fp, gchar * filename)
{
	mboxcache * cache;
	struct stat s;
	mailfile * mf;
	GList * l;

	if (stat(filename, &s) < 0)
		return NULL;

	mf = mailfile_init_from_file(fp, filename);
	if (mf == NULL)
		return NULL;

	cache = g_new0(mboxcache, 1);

	cache->mtime = s.st_mtime;
	cache->mf = mf;
	cache->filename = g_strdup(filename);

	cache->tab_mf = g_ptr_array_new();
	for(l = mf->msg_list ; l != NULL ; l = g_list_next(l))
		g_ptr_array_add(cache->tab_mf, l->data);

	mbox_cache_get_msginfo_from_file(fp, mf->msg_list);

	debug_print(_("read mbox from file - %s\n"), filename);

	return cache;
}

static void mbox_cache_insert_mbox(mboxcache * data)
{
	if (mbox_cache_table == NULL)
		mbox_cache_init();

	g_hash_table_insert(mbox_cache_table, data->filename, data);
}

static mboxcache * mbox_cache_get_mbox(gchar * filename)
{
	if (mbox_cache_table == NULL)
		mbox_cache_init();

	return g_hash_table_lookup(mbox_cache_table, filename);
}


static gint mbox_cache_get_count(gchar * filename)
{
	mboxcache * cache;

	cache = mbox_cache_get_mbox(filename);
	if (cache == NULL)
		return -1;
	if (cache->mf == NULL)
		return -1;
	return cache->mf->count;
}

static gint mbox_cache_get_mtime(gchar * filename)
{
	mboxcache * cache;

	cache = mbox_cache_get_mbox(filename);
	if (cache == NULL)
		return -1;
	return cache->mtime;
}

static GList * mbox_cache_get_msg_list(gchar * filename)
{
	mboxcache * cache;

	cache = mbox_cache_get_mbox(filename);

	if (cache == NULL)
		return NULL;

	if (cache->mf == NULL)
		return NULL;

	return cache->mf->msg_list;
}

static void mbox_cache_synchronize_lists(GList * old_msg_list,
					 GList * new_msg_list)
{
	GList * l;
	GList * l2;

	for(l2 = old_msg_list ; l2 != NULL ; l2 = g_list_next(l2)) {
		struct _message * msg2 = l2->data;

		if ((msg2->messageid == NULL) ||
		    (msg2->fromspace == NULL))
			continue;

		for(l = new_msg_list ; l != NULL ; l = g_list_next(l)) {
			struct _message * msg = l->data;
			
			if ((msg->messageid == NULL) ||
			    (msg->fromspace == NULL))
				continue;

			if ((strcmp(msg->messageid, msg2->messageid) == 0) &&
			    (strcmp(msg->fromspace, msg2->fromspace) == 0)) {
				if (msg2->flags.perm_flags != msg2->old_flags.perm_flags) {
					msg->flags = msg2->flags;
					break;
				}
			}
		}
	}
}

static void mbox_cache_synchronize(gchar * filename, gboolean sync)
{
	mboxcache * new_cache;
	mboxcache * old_cache;
	gboolean scan_new = TRUE;
	struct stat s;

	old_cache = mbox_cache_get_mbox(filename);

	if (old_cache != NULL) {
		if (stat(filename, &s) < 0) {
			FILE_OP_ERROR(filename, "stat");
		} else if (old_cache->mtime == s.st_mtime) {
			debug_print("Folder is not modified.\n");
			scan_new = FALSE;
		}
	}

	if (scan_new) {
		GList * l;

		/*		
		if (strstr(filename, "trash") == 0)
			printf("old_cache: %p %s\n", old_cache, filename);
			if (old_cache) {
				printf("begin old\n");
				for(l = old_cache->mf->msg_list ; l != NULL ;
				    l = g_list_next(l)) {
					struct _message * msg = l->data;
					printf("%p\n", msg);
				}
				printf("end old\n");
			}
		*/		

		new_cache = mbox_cache_read_mbox(filename);

		/*
		if (strstr(filename, "trash") == 0) 
			printf("new_cache: %p %s\n", new_cache, filename);
			if (new_cache) {
				printf("begin new\n");
				for(l = new_cache->mf->msg_list ; l != NULL ;
				    l = g_list_next(l)) {
					struct _message * msg = l->data;
					printf("%p\n", msg);
				}
				printf("end new\n");
			}
		*/

		if (!new_cache)
			return;

		if (sync && new_cache && old_cache)
			mbox_cache_synchronize_lists(old_cache->mf->msg_list,
						     new_cache->mf->msg_list);

		if (old_cache != NULL)
			mbox_cache_free_mbox(old_cache);

		if (new_cache) {
			mbox_cache_insert_mbox(new_cache);
			/*
			printf("insert %p %s\n", new_cache, new_cache->filename);
			printf("inserted %s %p\n", filename,
			       mbox_cache_get_mbox(filename));
			*/
		}
	}
}

static void mbox_cache_synchronize_from_file(FILE * fp, gchar * filename,
					     gboolean sync)
{
	mboxcache * new_cache;
	mboxcache * old_cache;
	gboolean scan_new = TRUE;
	struct stat s;

	old_cache = mbox_cache_get_mbox(filename);

	if (old_cache != NULL) {
		if (stat(filename, &s) < 0) {
			FILE_OP_ERROR(filename, "stat");
		} else if (old_cache->mtime == s.st_mtime) {
			debug_print("Folder is not modified.\n");
			scan_new = FALSE;
		}
	}


	if (scan_new) {

		/*
		GList * l;

		if (strstr(filename, "trash") == 0)
			printf("old_cache: %p %s\n", old_cache, filename);

			if (old_cache) {
				printf("begin old\n");
				for(l = old_cache->mf->msg_list ; l != NULL ;
				    l = g_list_next(l)) {
					struct _message * msg = l->data;
					printf("%p\n", msg);
				}
				printf("end old\n");
			}
		*/
		
		new_cache = mbox_cache_read_mbox_from_file(fp, filename);

		/*
		if (strstr(filename, "trash") == 0) 
			printf("new_cache: %p %s\n", new_cache, filename);

			if (new_cache) {
				printf("begin new\n");
				for(l = new_cache->mf->msg_list ; l != NULL ;
				    l = g_list_next(l)) {
					struct _message * msg = l->data;
					printf("%p\n", msg);
				}
				printf("end new\n");
			}
		*/

		if (!new_cache)
			return;

		if (sync && new_cache && old_cache)
			mbox_cache_synchronize_lists(old_cache->mf->msg_list,
						     new_cache->mf->msg_list);

		if (old_cache != NULL)
			mbox_cache_free_mbox(old_cache);

		if (new_cache) {
			mbox_cache_insert_mbox(new_cache);
			/*
			printf("insert %p %s\n", new_cache, new_cache->filename);
			printf("inserted %s %p\n", filename,
			       mbox_cache_get_mbox(filename));
			*/
		}
	}
}

gboolean mbox_cache_msg_fetched(gchar * filename, gint num)
{
	struct _message * msg;
	mboxcache * cache;

	cache = mbox_cache_get_mbox(filename);

	if (cache == NULL)
		return FALSE;

	msg = (struct _message *) g_ptr_array_index(cache->tab_mf,
						    num - 1);
	if (msg == NULL)
		return FALSE;

	return msg->fetched;
}

void mbox_cache_msg_set_fetched(gchar * filename, gint num)
{
	struct _message * msg;
	mboxcache * cache;

	cache = mbox_cache_get_mbox(filename);

	if (cache == NULL)
		return;

	msg = (struct _message *) g_ptr_array_index(cache->tab_mf,
						    num - 1);
	if (msg == NULL)
		return;

	msg->fetched = TRUE;
}

struct _message * mbox_cache_get_msg(gchar * filename, gint num)
{
	mboxcache * cache;

	cache = mbox_cache_get_mbox(filename);

	if (cache == NULL) {
		return NULL;
	}

	return (struct _message *) g_ptr_array_index(cache->tab_mf,
						     num - 1);
}


/**********************************************************/
/*                                                        */
/*                   mbox operations                      */
/*                                                        */
/**********************************************************/


GSList *mbox_get_msg_list(Folder *folder, FolderItem *item, gboolean use_cache)
{
	GSList *mlist;
	MsgInfo * msginfo;
	GList * l;
	FILE * fp;
	gchar * mbox_path;

#ifdef MEASURE_TIME
	struct timeval tv_before, tv_after, tv_result;

	gettimeofday(&tv_before, NULL);
#endif

	mlist = NULL;

	mbox_path = mbox_folder_get_path(item);

	if (mbox_path == NULL)
		return NULL;

	mbox_purge_deleted(mbox_path);

	fp = fopen(mbox_path, "rb");
	
	if (fp == NULL) {
		g_free(mbox_path);
		return NULL;
	}

	mbox_lockread_file(fp, mbox_path);

	mbox_cache_synchronize_from_file(fp, mbox_path, TRUE);

	item->last_num = mbox_cache_get_count(mbox_path);

	for(l = mbox_cache_get_msg_list(mbox_path) ; l != NULL ;
	    l = g_list_next(l)) {
		struct _message * msg;

		msg = (struct _message *) l->data;

		if (MSG_IS_INVALID(msg->flags) || !MSG_IS_REALLY_DELETED(msg->flags)) {
			fseek(fp, msg->header, SEEK_SET);

			msginfo = mbox_parse_msg(fp, msg, item);

			if (!MSG_IS_INVALID(msg->flags))
				msginfo->flags = msg->flags;
			else {
				msg->old_flags = msginfo->flags;
				msg->flags = msginfo->flags;
			}

			mlist = g_slist_append(mlist, msginfo);
		}
		else {
			MSG_SET_PERM_FLAGS(msg->flags, MSG_REALLY_DELETED);
		}
	}

	mbox_unlock_file(fp, mbox_path);

	g_free(mbox_path);

	fclose(fp);

#ifdef MEASURE_TIME
	gettimeofday(&tv_after, NULL);

	timersub(&tv_after, &tv_before, &tv_result);
	g_print("mbox_get_msg_list: %s: elapsed time: %ld.%06ld sec\n",
		mbox_path, tv_result.tv_sec, tv_result.tv_usec);
#endif

	return mlist;
}

static gboolean mbox_extract_msg(FolderItem * item, gint msgnum,
				 gchar * dest_filename)
{
	struct _message * msg;
	gint offset;
	gint max_offset;
	gint size;
	FILE * src;
	FILE * dest;
	gboolean err;
	/*	GList * msg_list;*/
	gboolean already_fetched;
	gchar * mbox_path;

	mbox_path = mbox_folder_get_path(item);

	if (mbox_path == NULL)
		return FALSE;

	src = fopen(mbox_path, "rb");
	if (src == NULL) {
		g_free(mbox_path);
		return FALSE;
	}

	mbox_lockread_file(src, mbox_path);

	mbox_cache_synchronize_from_file(src, mbox_path, TRUE);

	already_fetched = mbox_cache_msg_fetched(mbox_path, msgnum);

	if (already_fetched) {
		mbox_unlock_file(src, mbox_path);
		fclose(src);
		g_free(mbox_path);
		return TRUE;
	}

	msg = mbox_cache_get_msg(mbox_path, msgnum);

	if (msg == NULL) {
		mbox_unlock_file(src, mbox_path);
		fclose(src);
		g_free(mbox_path);
		return FALSE;
	}

	offset = msg->offset;
	max_offset = msg->end;
	
	size = max_offset - offset;

	fseek(src, offset, SEEK_SET);

	dest = fopen(dest_filename, "wb");
	if (dest == NULL) {
		mbox_unlock_file(src, mbox_path);
		fclose(src);
		g_free(mbox_path);
		return FALSE;
	}

	if (change_file_mode_rw(dest, dest_filename) < 0) {
		FILE_OP_ERROR(dest_filename, "chmod");
		g_warning(_("can't change file mode\n"));
	}

	if (!mbox_write_data(src, dest, dest_filename, size)) {
		mbox_unlock_file(src, mbox_path);
		fclose(dest);
		fclose(src);
		unlink(dest_filename);
		g_free(mbox_path);
		return FALSE;
	}

	err = FALSE;

	if (ferror(src)) {
		FILE_OP_ERROR(mbox_path, "fread");
		err = TRUE;
	}

	mbox_cache_msg_set_fetched(mbox_path, msgnum);

	if (fclose(dest) == -1) {
		FILE_OP_ERROR(dest_filename, "fclose");
		err = TRUE;
	}

	mbox_unlock_file(src, mbox_path);

	if (fclose(src) == -1) {
		FILE_OP_ERROR(mbox_path, "fclose");
		err = TRUE;
	}

	g_free(mbox_path);

	if (err) {
		unlink(dest_filename);
		return FALSE;
	}

	return TRUE;
}

gchar *mbox_fetch_msg(Folder *folder, FolderItem *item, gint num)
{
	gchar *path;
	gchar *filename;
	
	g_return_val_if_fail(item != NULL, NULL);

	path = folder_item_get_path(item);
	if (!is_dir_exist(path))
		make_dir_hier(path);

	filename = g_strconcat(path, G_DIR_SEPARATOR_S, itos(num), NULL);

	g_free(path);

	if (!mbox_extract_msg(item, num, filename)) {
		g_free(filename);
		return NULL;
	}

	return filename;
}

gint mbox_add_msg(Folder *folder, FolderItem *dest, const gchar *file,
		  gboolean remove_source)
{
	FILE * src_fp;
	FILE * dest_fp;
	gchar buf[BUFSIZ];
	gint old_size;
	gint n_read;
	gboolean err;
	gchar * mbox_path;
	gchar from_line[MSGBUFSIZE];

	if (dest->last_num < 0) {
		mbox_scan_folder(folder, dest);
		if (dest->last_num < 0) return -1;
	}

	src_fp = fopen(file, "rb");
	if (src_fp == NULL) {
		return -1;
	}

	mbox_path = mbox_folder_get_path(dest);
	if (mbox_path == NULL)
		return -1;

	dest_fp = fopen(mbox_path, "ab");
	if (dest_fp == NULL) {
		fclose(src_fp);
		g_free(mbox_path);
		return -1;
	}

	if (change_file_mode_rw(dest_fp, mbox_path) < 0) {
		FILE_OP_ERROR(mbox_path, "chmod");
		g_warning(_("can't change file mode\n"));
	}

	old_size = ftell(dest_fp);

	mbox_lockwrite_file(dest_fp, mbox_path);

	if (fgets(from_line, sizeof(from_line), src_fp) == NULL) {
		mbox_unlock_file(dest_fp, mbox_path);
		g_warning(_("unvalid file - %s.\n"), file);
		fclose(dest_fp);
		fclose(src_fp);
		g_free(mbox_path);
		return -1;
	}
	
	if (strncmp(from_line, "From ", 5) != 0) {
		struct stat s;

		if (stat(file, &s) < 0) {
			mbox_unlock_file(dest_fp, mbox_path);
			g_warning(_("invalid file - %s.\n"), file);
			fclose(dest_fp);
			fclose(src_fp);
			g_free(mbox_path);
			return -1;
		}

		fprintf(dest_fp, "From - %s", ctime(&s.st_mtime));
	}

	fputs(from_line, dest_fp);

	while (1) {
		n_read = fread(buf, 1, sizeof(buf), src_fp);
		if ((n_read < (gint) sizeof(buf)) && ferror(src_fp))
			break;
		if (fwrite(buf, n_read, 1, dest_fp) < 1) {
			mbox_unlock_file(dest_fp, mbox_path);
			g_warning(_("writing to %s failed.\n"), mbox_path);
			ftruncate(fileno(dest_fp), old_size);
			fclose(dest_fp);
			fclose(src_fp);
			g_free(mbox_path);
			return -1;
		}

		if (n_read < (gint) sizeof(buf))
			break;
	}

	err = FALSE;

	if (ferror(src_fp)) {
		FILE_OP_ERROR(mbox_path, "fread");
	}

	mbox_unlock_file(dest_fp, mbox_path);

	if (fclose(src_fp) == -1) {
		FILE_OP_ERROR(file, "fclose");
		err = TRUE;
	}

	if (fclose(dest_fp) == -1) {
		FILE_OP_ERROR(mbox_path, "fclose");
		g_free(mbox_path);
		return -1;
	}

	if (err) {
		ftruncate(fileno(dest_fp), old_size);
		g_free(mbox_path);
		return -1;
	}

	if (remove_source) {
		if (unlink(file) < 0)
			FILE_OP_ERROR(file, "unlink");
	}

	g_free(mbox_path);

	dest->last_num++;
	return dest->last_num;

}

gint mbox_remove_msg(Folder *folder, FolderItem *item, gint num)
{
	struct _message * msg;
	gchar * mbox_path;

	mbox_path = mbox_folder_get_path(item);
	if (mbox_path == NULL)
		return -1;

	mbox_cache_synchronize(mbox_path, TRUE);

	msg = mbox_cache_get_msg(mbox_path, num);

	g_free(mbox_path);

	if (msg != NULL)
		MSG_SET_PERM_FLAGS(msg->flags, MSG_REALLY_DELETED);

	return 0;
}

gint mbox_remove_all_msg(Folder *folder, FolderItem *item)
{
	FILE * fp;
	gchar * mbox_path;

	mbox_path = mbox_folder_get_path(item);
	if (mbox_path == NULL)
		return -1;

	fp = fopen(mbox_path, "wb");
	if (fp == NULL) {
		g_free(mbox_path);
		return -1;
	}

	fclose(fp);

	g_free(mbox_path);

	return 0;
}

/*
gint mbox_move_msg(Folder *folder, FolderItem *dest, MsgInfo *msginfo)
{
	gchar * filename;
	gint msgnum;

	filename = mbox_fetch_msg(folder, msginfo->folder, msginfo->msgnum);
	if (filename == NULL)
		return -1;

	msgnum = mbox_add_msg(folder, dest, filename, TRUE);

	if (msgnum != -1) {
		MSG_SET_FLAGS(msginfo->flags, MSG_REALLY_DELETED);
		mbox_change_flags(folder, msginfo->folder, msginfo);
	}

	return msgnum;
}

gint mbox_move_msgs_with_dest(Folder *folder, FolderItem *dest, GSList *msglist)
{
	GSList * l;
	gchar * mbox_path = NULL;

	for(l = msglist ; l != NULL ; l = g_slist_next(l)) {
		MsgInfo * msginfo = (MsgInfo *) l->data;

		if (msginfo->folder && mbox_path == NULL)
			mbox_path = mbox_folder_get_path(msginfo->folder);

		mbox_move_msg(folder, dest, msginfo);
	}

	if (mbox_path) {
		mbox_cache_synchronize(mbox_path);
		g_free(mbox_path);
	}

	mbox_path = mbox_folder_get_path(dest);
	mbox_cache_synchronize(mbox_path);
	g_free(mbox_path);

	return dest->last_num;
}
*/

/*
gint mbox_copy_msg(Folder *folder, FolderItem *dest, MsgInfo *msginfo)
{
	gchar * filename;
	gint msgnum;

	filename = mbox_fetch_msg(folder, msginfo->folder, msginfo->msgnum);
	if (filename == NULL)
		return -1;

	msgnum = mbox_add_msg(folder, dest, filename, FALSE);

	return msgnum;
}

gint mbox_copy_msgs_with_dest(Folder *folder, FolderItem *dest, GSList *msglist)
{
	GSList * l;
	gchar * mbox_path = NULL;

	for(l = msglist ; l != NULL ; l = g_slist_next(l)) {
		MsgInfo * msginfo = (MsgInfo *) l->data;

		if (msginfo->folder && mbox_path == NULL)
			mbox_path = mbox_folder_get_path(msginfo->folder);

		mbox_copy_msg(folder, dest, msginfo);
	}

	if (mbox_path) {
		mbox_cache_synchronize(mbox_path);
		g_free(mbox_path);
	}

	mbox_path = mbox_folder_get_path(dest);
	mbox_cache_synchronize(mbox_path);
	g_free(mbox_path);

	return dest->last_num;
}
*/

struct _copy_flags_info
{
	gint num;
	MsgFlags flags;
};

typedef struct _copy_flags_info CopyFlagsInfo;

GSList * copy_flags_data = NULL;

gint mbox_copy_msg(Folder *folder, FolderItem *dest, MsgInfo *msginfo)
{
	Folder * src_folder;
	gchar * filename;
	gint num;
	gchar * destdir;
	gchar * mbox_path;
	struct _message * msg;
	CopyFlagsInfo * flags_info;

	src_folder = msginfo->folder->folder;
	
	g_return_val_if_fail(src_folder->fetch_msg != NULL, -1);
	
	/*
	mbox_path = mbox_folder_get_path(msginfo->folder);
	mbox_rewrite(mbox_path);
	g_free(mbox_path);
	*/

	filename = src_folder->fetch_msg(src_folder,
					 msginfo->folder,
					 msginfo->msgnum);
	if (filename == NULL)
		return -1;

	num = folder->add_msg(folder, dest, filename, FALSE);

	/*
	mbox_path = mbox_folder_get_path(dest);
	msg = mbox_cache_get_msg(mbox_path, num);
	if (msg != NULL)
		msg->flags = msginfo->flags;
	g_free(mbox_path);
	*/

	if (num == -1)
		return -1;

	flags_info = g_new0(CopyFlagsInfo, 1);
	flags_info->num = num;
	flags_info->flags = msginfo->flags;
	copy_flags_data = g_slist_append(copy_flags_data, flags_info);

	return num;
}

void mbox_finished_copy(Folder *folder, FolderItem *dest)
{
	gchar * mbox_path;
	GSList * l;
	mboxcache * cache;
	
	mbox_path = mbox_folder_get_path(dest);
	if (mbox_path == NULL)
		return;

	mbox_cache_synchronize(mbox_path, TRUE);

	for(l = copy_flags_data ; l != NULL ; l = g_slist_next(l)) {
		CopyFlagsInfo * flags_info = l->data;
		struct _message * msg;

		msg = mbox_cache_get_msg(mbox_path, flags_info->num);
		if (msg != NULL)
			msg->flags = flags_info->flags;
		g_free(flags_info);
	}

	if (copy_flags_data != NULL) {
		cache = mbox_cache_get_mbox(mbox_path);
		cache->modification = TRUE;
	}

	g_slist_free(copy_flags_data);
	copy_flags_data = NULL;

	mbox_rewrite(mbox_path);

	g_free(mbox_path);
}

void mbox_scan_folder(Folder *folder, FolderItem *item)
{
	gchar *mbox_path;
	gint n_msg;
	mboxcache * cached;
	GList * l;

	mbox_path = mbox_folder_get_path(item);
	if (mbox_path == NULL)
		return;

	mbox_cache_synchronize(mbox_path, TRUE);

	cached = mbox_cache_get_mbox(mbox_path);

	if (cached == NULL) {
		item->new = 0;
		item->unread = 0;
		item->total = 0;
		item->last_num = 0;
	        g_free(mbox_path);
		return;
	}

	n_msg = mbox_cache_get_count(mbox_path);

	if (n_msg == 0) {
		item->new = item->unread = item->total = 0;
	}
	else {
		gint new = 0;
		gint unread = 0;
		gint total = 0;

		for(l = mbox_cache_get_msg_list(mbox_path) ; l != NULL ;
		    l = g_list_next(l)) {
			struct _message * msg = (struct _message *) l->data;
			if (!MSG_IS_REALLY_DELETED(msg->flags))
				total ++;
			if (MSG_IS_NEW(msg->flags) && !MSG_IS_IGNORE_THREAD(msg->flags))
				new ++;
			if (MSG_IS_UNREAD(msg->flags) && !MSG_IS_IGNORE_THREAD(msg->flags))
				unread ++;
		}
		
		item->new = new;
		item->unread = unread;
		item->total = total;
	}

	debug_print(_("Last number in dir %s = %d\n"), mbox_path,
		    item->total);
	item->last_num = n_msg;
	g_free(mbox_path);
}

gchar * mbox_get_virtual_path(FolderItem * item)
{
	if (item == NULL)
		return NULL;

	if (item->parent == NULL) {
		return NULL;
	}
	else {
		gchar * parent_path;
		gchar * result_path;

		parent_path = mbox_get_virtual_path(item->parent);
		if (parent_path == NULL)
			result_path = g_strdup(item->name);
		else
			result_path = g_strconcat(parent_path,
						  G_DIR_SEPARATOR_S,
						  item->name, NULL);
		g_free(parent_path);

		return result_path;
	}
}

static gboolean mbox_write_data(FILE * mbox_fp, FILE * new_fp,
				gchar * new_filename, gint size)
{	
	gint n_read;
	gint pos;
	gchar buf[BUFSIZ];
	gint max;

	pos = 0;
	while (pos < size) {
		if ((size - pos) > (gint) sizeof(buf))
			max = sizeof(buf);
		else
			max = (size - pos);
		
		n_read = fread(buf, 1, max, mbox_fp);

		if (n_read < max && ferror(mbox_fp)) {
			return FALSE;
		}
		if (fwrite(buf, n_read, 1, new_fp) < 1) {
			g_warning(_("writing to %s failed.\n"), new_filename);
			return FALSE;
		}
		
		if (n_read != -1)
			pos += n_read;
		
		if (n_read < max)
			break;
	}
	return TRUE;
}

static gboolean mbox_write_message(FILE * mbox_fp, FILE * new_fp,
				   gchar * new_filename,
				   struct _message * msg)
{
	gint size;
	GPtrArray * headers;
	gint i;
	
	fseek(mbox_fp, msg->header, SEEK_SET);

	headers = procheader_get_header_array_asis(mbox_fp);

	for (i = 0; i < (gint) headers->len; i++) {
		Header * h = g_ptr_array_index(headers, i);
		
		if (!procheader_headername_equal(h->name, 
						 "Status") &&
		    !procheader_headername_equal(h->name, 
						 "X-Status")) {
			fwrite(h->name, strlen(h->name),
			       1, new_fp);
			if (h->name[strlen(h->name) - 1] != ' ')
				fwrite(" ", 1, 1, new_fp);
			fwrite(h->body, strlen(h->body),
			       1, new_fp);
			fwrite("\n", 1, 1, new_fp);
		}
		procheader_header_free(h);
		g_ptr_array_remove_index(headers, i);
		i--;
	}

	g_ptr_array_free(headers, FALSE);

	if (!MSG_IS_INVALID(msg->flags)) {
		/* Status header */
		fwrite("Status: ", strlen("Status: "), 1, new_fp);
		if (!MSG_IS_UNREAD(msg->flags))
			fwrite("R", 1, 1, new_fp);
		fwrite("O", 1, 1, new_fp);
		fwrite("\n", 1, 1, new_fp);
		
		/* X-Status header */
		if (MSG_IS_REALLY_DELETED(msg->flags)
		||  MSG_IS_MARKED(msg->flags)
		||  MSG_IS_DELETED(msg->flags)
		||  MSG_IS_REPLIED(msg->flags)
		||  MSG_IS_FORWARDED(msg->flags)) {
			fwrite("X-Status: ", strlen("X-Status: "), 1, new_fp);
			if (MSG_IS_REALLY_DELETED(msg->flags))
				fwrite("D", 1, 1, new_fp); /* really deleted */
			else {
				if (MSG_IS_MARKED(msg->flags))
					fwrite("F", 1, 1, new_fp);
				if (MSG_IS_DELETED(msg->flags))
					fwrite("d", 1, 1, new_fp);
				if (MSG_IS_REPLIED(msg->flags))
					fwrite("r", 1, 1, new_fp);
				if (MSG_IS_FORWARDED(msg->flags))
					fwrite("f", 1, 1, new_fp);
			}
			fwrite("\n", 1, 1, new_fp);
		}
	}

	fwrite("\n", 1, 1, new_fp);

	size = msg->end - msg->content;
	fseek(mbox_fp, msg->content, SEEK_SET);

	return mbox_write_data(mbox_fp, new_fp, new_filename, size);
}

void mbox_update_mark(Folder * folder, FolderItem * item)
{
	gchar * mbox_path;

	mbox_path = mbox_folder_get_path(item);
	if (mbox_path == NULL)
		return;

	mbox_rewrite(mbox_path);
	g_free(mbox_path);
}

void mbox_change_flags(Folder * folder, FolderItem * item, MsgInfo * info)
{
	struct _message * msg;
	mboxcache * cache;
	gchar * mbox_path;

	mbox_path = mbox_folder_get_path(item);
	if (mbox_path == NULL)
		return;

	msg = mbox_cache_get_msg(mbox_path, info->msgnum);

	cache = mbox_cache_get_mbox(mbox_path);

	g_free(mbox_path);

	if ((msg == NULL) || (cache == NULL))
		return;

	msg->flags = info->flags;

	cache->modification = TRUE;
}


static gboolean mbox_rewrite(gchar * mbox)
{
	FILE * mbox_fp;
	FILE * new_fp;
	gchar * new;
	GList * l;
	gboolean result;
	GList * msg_list;
	gint count;
	mboxcache * cache;

	msg_list = mbox_cache_get_msg_list(mbox);

	cache = mbox_cache_get_mbox(mbox);
	if (cache == NULL)
		return FALSE;

	if (!cache->modification) {
		debug_print(_("no modification - %s\n"), mbox);
		return FALSE;
	}

	debug_print(_("save modification - %s\n"), mbox);

	mbox_fp = fopen(mbox, "rb+");
	mbox_lockwrite_file(mbox_fp, mbox);

	mbox_cache_synchronize_from_file(mbox_fp, mbox, TRUE);

	new = g_strconcat(mbox, ".", itos((int) mbox), NULL);
	new_fp = fopen(new, "wb");

	if (change_file_mode_rw(new_fp, new) < 0) {
		FILE_OP_ERROR(new, "chmod");
		g_warning(_("can't change file mode\n"));
	}

	mbox_lockwrite_file(new_fp, new);

	result = TRUE;

	count = 0;
	msg_list = mbox_cache_get_msg_list(mbox);
	for(l = msg_list ; l != NULL ; l = g_list_next(l)) {
		struct _message * msg = (struct _message *) l->data;
		if (!mbox_write_message(mbox_fp, new_fp, new, msg)) {
			result = FALSE;
			break;
		}
		count ++;
	}

	unlink(mbox);

	if (rename(new, mbox) == -1) {
		g_warning(_("can't rename %s to %s\n"), new, mbox);
		mbox_unlock_file(new_fp, new);
		fclose(new_fp);
		mbox_unlock_file(mbox_fp, mbox);
		fclose(mbox_fp);
		return -1;
	}

	if (change_file_mode_rw(new_fp, mbox) < 0) {
		FILE_OP_ERROR(new, "chmod");
		g_warning(_("can't change file mode\n"));
	}

	mbox_unlock_file(new_fp, new);

	fclose(new_fp);

	mbox_unlock_file(mbox_fp, mbox);

	fclose(mbox_fp);

	debug_print(_("%i messages written - %s\n"), count, mbox);

	cache = mbox_cache_get_mbox(mbox);

	if (cache != NULL)
		cache->mtime = -1;

	mbox_cache_synchronize(mbox, FALSE);

	return result;
}

static gboolean mbox_purge_deleted(gchar * mbox)
{
	FILE * mbox_fp;
	FILE * new_fp;
	gchar * new;
	GList * l;
	gboolean result;
	gboolean modification = FALSE;
	GList * msg_list;
	gint count;

	mbox_cache_synchronize(mbox, TRUE);

	msg_list = mbox_cache_get_msg_list(mbox);

	for(l = msg_list ; l != NULL ; l = g_list_next(l)) {
		struct _message * msg = (struct _message *) l->data;
		if (!MSG_IS_INVALID(msg->flags) && MSG_IS_REALLY_DELETED(msg->flags)) {
			modification = TRUE;
			break;
		}
	}

	if (!modification) {
		debug_print(_("no deleted messages - %s\n"), mbox);
		return FALSE;
	}

	debug_print(_("purge deleted messages - %s\n"), mbox);

	mbox_fp = fopen(mbox, "rb+");
	mbox_lockwrite_file(mbox_fp, mbox);

	mbox_cache_synchronize_from_file(mbox_fp, mbox, TRUE);

	new = g_strconcat(mbox, ".", itos((int) mbox), NULL);
	new_fp = fopen(new, "wb");

	if (change_file_mode_rw(new_fp, new) < 0) {
		FILE_OP_ERROR(new, "chmod");
		g_warning(_("can't change file mode\n"));
	}

	mbox_lockwrite_file(new_fp, new);

	result = TRUE;

	count = 0;
	msg_list = mbox_cache_get_msg_list(mbox);
	for(l = msg_list ; l != NULL ; l = g_list_next(l)) {
		struct _message * msg = (struct _message *) l->data;
		if (MSG_IS_INVALID(msg->flags) || !MSG_IS_REALLY_DELETED(msg->flags)) {
			if (!mbox_write_message(mbox_fp, new_fp, new, msg)) {
				result = FALSE;
				break;
			}
			count ++;
		}
	}

	unlink(mbox);

	if (rename(new, mbox) == -1) {
		g_warning(_("can't rename %s to %s\n"), new, mbox);
		mbox_unlock_file(new_fp, new);
		fclose(new_fp);
		mbox_unlock_file(mbox_fp, mbox);
		fclose(mbox_fp);
		return -1;
	}

	if (change_file_mode_rw(new_fp, mbox) < 0) {
		FILE_OP_ERROR(new, "chmod");
		g_warning(_("can't change file mode\n"));
	}

	mbox_unlock_file(new_fp, new);

	fclose(new_fp);

	mbox_unlock_file(mbox_fp, mbox);

	fclose(mbox_fp);

	debug_print(_("%i messages written - %s\n"), count, mbox);

	mbox_cache_synchronize(mbox, FALSE);

	return result;
}

#define MAKE_DIR_IF_NOT_EXIST(dir) \
{ \
	if (!is_dir_exist(dir)) { \
		if (is_file_exist(dir)) { \
			g_warning(_("File `%s' already exists.\n" \
				    "Can't create folder."), dir); \
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

gint mbox_create_tree(Folder *folder)
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

static gchar * mbox_get_new_path(FolderItem * parent, gchar * name)
{
	gchar * path;

	if (strchr(name, '/') == NULL) {
		if (parent->path != NULL)
			path = g_strconcat(parent->path, ".sbd", G_DIR_SEPARATOR_S, name, NULL);
		else
			path = g_strdup(name);
	}
	else
		path = g_strdup(name);

	return path;
}

static gchar * mbox_get_folderitem_name(gchar * name)
{
	gchar * foldername;

	foldername = g_strdup(g_basename(name));
	
	return foldername;
}

FolderItem *mbox_create_folder(Folder *folder, FolderItem *parent,
			       const gchar *name)
{
	gchar * path;
	FolderItem *new_item;
	gchar * foldername;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(parent != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);

	path = mbox_get_new_path(parent, (gchar *) name);

	foldername = mbox_get_folderitem_name((gchar *) name);

	new_item = folder_item_new(foldername, path);
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

gint mbox_rename_folder(Folder *folder, FolderItem *item, const gchar *name)
{
	gchar * path;
	gchar * foldername;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(item != NULL, -1);
	g_return_val_if_fail(item->path != NULL, -1);
	g_return_val_if_fail(name != NULL, -1);

	path = mbox_get_new_path(item->parent, (gchar *) name);
	foldername = mbox_get_folderitem_name((gchar *) name);

	if (rename(item->path, path) == -1) {
		g_free(foldername);
		g_free(path);
		g_warning(_("Cannot rename folder item"));

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

gint mbox_remove_folder(Folder *folder, FolderItem *item)
{
	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(item != NULL, -1);
	g_return_val_if_fail(item->path != NULL, -1);

	folder_item_remove(item);
	return 0;
}

GSList *mbox_get_num_list(Folder *folder, FolderItem *item)
{
	GSList *mlist;
	GList * l;
	FILE * fp;
	gchar * mbox_path;

	mlist = NULL;

	mbox_path = mbox_folder_get_path(item);

	if (mbox_path == NULL)
		return NULL;

	mbox_purge_deleted(mbox_path);

	fp = fopen(mbox_path, "rb");
	
	if (fp == NULL) {
		g_free(mbox_path);
		return NULL;
	}

	mbox_lockread_file(fp, mbox_path);

	mbox_cache_synchronize_from_file(fp, mbox_path, TRUE);

	item->last_num = mbox_cache_get_count(mbox_path);

	for(l = mbox_cache_get_msg_list(mbox_path) ; l != NULL ;
	    l = g_list_next(l)) {
		struct _message * msg;

		msg = (struct _message *) l->data;

		if (MSG_IS_INVALID(msg->flags) || !MSG_IS_REALLY_DELETED(msg->flags)) {
			mlist = g_slist_append(mlist, GINT_TO_POINTER(msg->msgnum));
		} else {
			MSG_SET_PERM_FLAGS(msg->flags, MSG_REALLY_DELETED);
		}
	}

	mbox_unlock_file(fp, mbox_path);

	g_free(mbox_path);

	fclose(fp);

	return mlist;
}

MsgInfo *mbox_fetch_msginfo(Folder *folder, FolderItem *item, gint num)
{
	gchar *mbox_path;
	struct _message *msg;
	FILE *src;
	MsgInfo *msginfo;
	
	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(item != NULL, NULL);

	mbox_path = mbox_folder_get_path(item);

	g_return_val_if_fail(mbox_path != NULL, NULL);
	
	src = fopen(mbox_path, "rb");
	if (src == NULL) {
		g_free(mbox_path);
		return NULL;
	}
	mbox_lockread_file(src, mbox_path);
	mbox_cache_synchronize_from_file(src, mbox_path, TRUE);

	msg = mbox_cache_get_msg(mbox_path, num);
	if (msg == NULL) {
		mbox_unlock_file(src, mbox_path);
		fclose(src);
		g_free(mbox_path);
		return NULL;
	}
	
	fseek(src, msg->header, SEEK_SET);
	msginfo = mbox_parse_msg(src, msg, item);

	mbox_unlock_file(src, mbox_path);
	fclose(src);
	g_free(mbox_path);

	return msginfo;
}

gboolean mbox_check_msgnum_validity(Folder *folder, FolderItem *item)
{
	mboxcache * old_cache;
	gboolean scan_new = TRUE;
	struct stat s;
	gchar *filename;

	filename = mbox_folder_get_path(item);
	
	old_cache = mbox_cache_get_mbox(filename);

	if (old_cache != NULL) {
		if (stat(filename, &s) < 0) {
			FILE_OP_ERROR(filename, "stat");
		} else if (old_cache->mtime == s.st_mtime) {
			debug_print("Folder is not modified.\n");
			scan_new = FALSE;
		}
	}

	g_free(filename);
	
	return !scan_new;
}

