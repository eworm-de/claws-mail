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

static gboolean mbox_write_data(FILE * mbox_fp, FILE * new_fp,
				gchar * new_filename, gint size);


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
	if ((lockfp = fopen(lockfile, "w")) == NULL) {
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
	gchar *lockfile, *locklink;
	gint retry = 0;
	FILE *lockfp;

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
		if (result = mbox_file_lock_file(base)) {
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
		if (result = mbox_file_lock_file(base)) {
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
	gboolean result;
	GSList * l;
	gboolean unlocked = FALSE;

	for(l = file_lock ; l != NULL ; l = g_slist_next(l)) {
		gchar * data = l->data;

		if (strcmp(data, base) == 0) {
			file_lock = g_slist_remove(file_lock, data);
			g_free(data);
			result = mbox_file_unlock_file(base);
			unlocked = TRUE;
			break;
		}
	}
	
	if (!unlocked)
		result = mbox_fcntl_unlock_file(fp);

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

#define STATE_MASK        0x0FF // filter state from functions

#define STATE_RESTORE_POS       0x100 // go back while reading

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
	int deleted;
	int offset;
	int header;
	int content;
	int end;
	int marked;
	gchar * messageid;
	gchar * fromspace;
	MsgFlags flags;
	gboolean fetched;
};


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
	char * r;
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
				data->deleted = 0;
				data->messageid = NULL;
				data->fromspace = NULL;
				data->flags = -1;
				data->fetched = FALSE;
				msg_list = g_list_append(msg_list, (gpointer) data);
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
			if (r == NULL)
				state = STATE_END;
			else if (startFrom(s)) {
				data->content = lastpos;

				state = STATE_END | STATE_RESTORE_POS;
			}
			else
				state = STATE_TEXT_READ;
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
			else if (startEmpty(s))
				state = STATE_TEXT_BEGIN;
			else
				state = STATE_FIELD_READ;
			break;
		}
      
		if ((state & STATE_MASK) == STATE_END) {
			state = STATE_BEGIN | (state & STATE_RESTORE_POS);
			if (state & STATE_RESTORE_POS)
				data->end = former_pos;
			else
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

	mf->msg_list = msg_list;

	mf->filename = g_strdup(filename);
	mf->count = msgnum;

	mailfile_error = MAILFILE_ERROR_NO_ERROR;

	return mf;
}

static mailfile * mailfile_init(char * filename)
{

	FILE * f;
	mailfile * mf;
  
	f = fopen(filename, "r");

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

	handle = fopen(filename, "r");

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

	handle = fopen(filename, "r");

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
	gint mtime;
};

typedef struct _mboxcache mboxcache;

static GHashTable * mbox_cache_table = NULL;
static mboxcache * current_mbox = NULL;

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
	if (cache->filename)
		g_free(cache->filename);
	g_free(cache);
}

static mboxcache * mbox_cache_read_mbox(gchar * filename)
{
	mboxcache * cache;
	struct stat s;
	mailfile * mf;

	if (stat(filename, &s) < 0)
		return NULL;

	mf = mailfile_init(filename);
	if (mf == NULL)
		return NULL;

	cache = g_new0(mboxcache, 1);

	cache->mtime = s.st_mtime;
	cache->mf = mf;
	cache->filename = g_strdup(filename);

	return cache;
}

static mboxcache * mbox_cache_read_mbox_from_file(FILE * fp, gchar * filename)
{
	mboxcache * cache;
	struct stat s;
	mailfile * mf;

	if (stat(filename, &s) < 0)
		return NULL;

	mf = mailfile_init_from_file(fp, filename);
	if (mf == NULL)
		return NULL;

	cache = g_new0(mboxcache, 1);

	cache->mtime = s.st_mtime;
	cache->mf = mf;
	cache->filename = g_strdup(filename);

	return cache;
}

static void mbox_cache_insert_mbox(gchar * filename, mboxcache * data)
{
	if (mbox_cache_table == NULL)
		mbox_cache_init();

	g_hash_table_insert(mbox_cache_table, filename, data);
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

static void mbox_cache_synchronize_lists(GList * msg_new_list,
					 GList * msg_old_list)
{
	struct _message * msg_new;
	struct _message * msg_old;
	GList * l_new;
	GList * l_old;

	// could be improved with a hash table
	for(l_old = msg_old_list ; l_old != NULL ;
	    l_old = g_list_next(l_old)) {
		msg_old = (struct _message *) l_old->data;
		
		if ((msg_old->messageid == NULL) ||
		    (msg_old->fromspace == NULL))
			continue;
		
		for(l_new = msg_new_list ; l_new != NULL ;
		    l_new = g_list_next(l_new)) {
			msg_new = (struct _message *) l_new->data;
			
			if ((msg_new->messageid == NULL) ||
			    (msg_new->fromspace == NULL))
				continue;
			
			if (!strcmp(msg_new->messageid, msg_old->messageid) &&
			    !strcmp(msg_new->fromspace, msg_old->fromspace)) {
				// copy flags
				msg_new->deleted = msg_old->deleted;
				msg_new->flags = msg_old->flags;
				break;
			}
		}
	}
}

static void mbox_cache_synchronize(gchar * filename)
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
		new_cache = mbox_cache_read_mbox(filename);
		
		if (new_cache == NULL) {
			if (old_cache != NULL)
				mbox_cache_free_mbox(old_cache);
			return;
		}

		if (old_cache != NULL) {
			if ((old_cache->mf != NULL) &&
			    (new_cache->mf != NULL))
				mbox_cache_synchronize_lists(new_cache->mf->msg_list, old_cache->mf->msg_list);
			mbox_cache_free_mbox(old_cache);
		}
	
		mbox_cache_insert_mbox(filename, new_cache);
	}
}

static void mbox_cache_synchronize_from_file(FILE * fp, gchar * filename)
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
		new_cache = mbox_cache_read_mbox_from_file(fp, filename);
		
		if (new_cache == NULL) {
			if (old_cache != NULL)
				mbox_cache_free_mbox(old_cache);
			return;
		}

		if (old_cache != NULL) {
			if ((old_cache->mf != NULL) &&
			    (new_cache->mf != NULL))
				mbox_cache_synchronize_lists(new_cache->mf->msg_list, old_cache->mf->msg_list);
			mbox_cache_free_mbox(old_cache);
		}
	
		mbox_cache_insert_mbox(filename, new_cache);
	}
}

gboolean mbox_cache_msg_fetched(gchar * filename, gint num)
{
	GList * msg_list;
	struct _message * msg;

	msg_list = mbox_cache_get_msg_list(filename);
	msg = (struct _message *) g_list_nth_data(msg_list, num - 1);

	if (msg == NULL)
		return FALSE;

	return msg->fetched;
}

void mbox_cache_msg_set_fetched(gchar * filename, gint num)
{
	GList * msg_list;
	struct _message * msg;

	msg_list = mbox_cache_get_msg_list(filename);
	if ((msg = (struct _message *) g_list_nth_data(msg_list, num - 1))
	    == NULL)
		return;
	msg->fetched = TRUE;
}


/**********************************************************/
/*                                                        */
/*                   mbox operations                      */
/*                                                        */
/**********************************************************/


static MsgInfo *mbox_parse_msg(FILE * fp, gint msgnum, FolderItem *item)
{
	struct stat s;
	MsgInfo *msginfo;
	MsgFlags flags = MSG_NEW|MSG_UNREAD;

	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(fp != NULL, NULL);

	if (item->stype == F_QUEUE) {
		MSG_SET_FLAGS(flags, MSG_QUEUED);
	} else if (item->stype == F_DRAFT) {
		MSG_SET_FLAGS(flags, MSG_DRAFT);
	}

	msginfo = procheader_file_parse(fp, flags, FALSE);

	if (!msginfo) return NULL;

	msginfo->msgnum = msgnum;
	msginfo->folder = item;

	if (stat(item->path, &s) < 0) {
		FILE_OP_ERROR(item->path, "stat");
		msginfo->size = 0;
		msginfo->mtime = 0;
	} else {
		msginfo->size = s.st_size;
		msginfo->mtime = s.st_mtime;
	}

	return msginfo;
}

GSList *mbox_get_msg_list(Folder *folder, FolderItem *item, gboolean use_cache)
{
	GSList *mlist;
	GHashTable *msg_table;
	MsgFlags flags = MSG_NEW|MSG_UNREAD;
	MsgInfo * msginfo;
	GList * l;
	FILE * fp;

#ifdef MEASURE_TIME
	struct timeval tv_before, tv_after, tv_result;

	gettimeofday(&tv_before, NULL);
#endif

	mlist = NULL;

	fp = fopen(item->path, "r");

	mbox_lockread_file(fp, item->path);

	mbox_cache_synchronize_from_file(fp, item->path);

	if (mbox_cache_get_mbox(item->path) == NULL) {
		g_warning(_("parsing of %s failed.\n"), item->path);
		mbox_unlock_file(fp, item->path);
		fclose(fp);
		return NULL;
	}

	for(l = mbox_cache_get_msg_list(item->path) ; l != NULL ;
	    l = g_list_next(l)) {
		struct _message * msg;

		msg = (struct _message *) l->data;

		if (!msg->deleted) {
			fseek(fp, msg->header, SEEK_SET);
			msginfo = mbox_parse_msg(fp, msg->msgnum, item);
			if (msginfo) {
				if (msginfo->msgid)
					msg->messageid =
						g_strdup(msginfo->msgid);
				if (msginfo->fromspace)
					msg->fromspace =
						g_strdup(msginfo->fromspace);
				msginfo->data = msg;
			}
			if (msg->flags != -1)
				msginfo->flags = msg->flags;
			mlist = g_slist_append(mlist, msginfo);
		}
	}

	mbox_unlock_file(fp, item->path);

	fclose(fp);

#ifdef MEASURE_TIME
	gettimeofday(&tv_after, NULL);

	timersub(&tv_after, &tv_before, &tv_result);
	g_print("mbox_get_msg_list: %s: elapsed time: %ld.%06ld sec\n",
		item->path, tv_result.tv_sec, tv_result.tv_usec);
#endif

	return mlist;
}

static gboolean mbox_extract_msg(gchar * mbox_filename, gint msgnum,
				 gchar * dest_filename)
{
	struct _message * msg;
	gint offset;
	gint max_offset;
	gint size;
	FILE * src;
	FILE * dest;
	gboolean err;
	GList * msg_list;
	gboolean already_fetched;

	src = fopen(mbox_filename, "r");
	if (src == NULL)
		return FALSE;

	mbox_lockread_file(src, mbox_filename);

	mbox_cache_synchronize_from_file(src, mbox_filename);

	already_fetched = mbox_cache_msg_fetched(mbox_filename, msgnum);

	if (already_fetched) {
		mbox_unlock_file(src, mbox_filename);
		fclose(src);
		return TRUE;
	}

	msg_list = mbox_cache_get_msg_list(mbox_filename);

	msg = (struct _message *) g_list_nth_data(msg_list, msgnum - 1);

	if (msg == NULL) {
		mbox_unlock_file(src, mbox_filename);
		fclose(src);
		return FALSE;
	}

	offset = msg->offset;
	max_offset = msg->end;
	
	size = max_offset - offset;

	fseek(src, offset, SEEK_SET);

	dest = fopen(dest_filename, "w");
	if (dest == NULL) {
		mbox_unlock_file(src, mbox_filename);
		fclose(src);
		return FALSE;
	}

	if (change_file_mode_rw(dest, dest_filename) < 0) {
		FILE_OP_ERROR(dest_filename, "chmod");
		g_warning(_("can't change file mode\n"));
	}

	if (!mbox_write_data(src, dest, dest_filename, size)) {
		mbox_unlock_file(src, mbox_filename);
		fclose(dest);
		fclose(src);
		unlink(dest_filename);
		return FALSE;
	}

	err = FALSE;

	if (ferror(src)) {
		FILE_OP_ERROR(mbox_filename, "fread");
		err = TRUE;
	}

	mbox_cache_msg_set_fetched(mbox_filename, msgnum);

	if (fclose(dest) == -1) {
		FILE_OP_ERROR(dest_filename, "fclose");
		return FALSE;
	}

	mbox_unlock_file(src, mbox_filename);

	if (fclose(src) == -1) {
		FILE_OP_ERROR(mbox_filename, "fclose");
		err = TRUE;
	}

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
	/*
	g_return_val_if_fail(num > 0 && num <= item->last_num, NULL);
	*/

	path = folder_item_get_path(item);
	if (!is_dir_exist(path))
		make_dir_hier(path);

	filename = g_strconcat(path, G_DIR_SEPARATOR_S, itos(num), NULL);

	g_free(path);

	if (!mbox_extract_msg(item->path, num, filename)) {
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

	if (dest->last_num < 0) {
		mbox_scan_folder(folder, dest);
		if (dest->last_num < 0) return -1;
	}

	src_fp = fopen(file, "r");
	if (src_fp == NULL) {
		return -1;
	}

	dest_fp = fopen(dest->path, "a");
	if (dest_fp == NULL) {
		fclose(src_fp);
		return -1;
	}
	old_size = ftell(dest_fp);

	mbox_lockwrite_file(dest_fp, dest->path);
	
	while (1) {
		n_read = fread(buf, 1, sizeof(buf), src_fp);
		if (n_read < sizeof(buf) && ferror(src_fp))
			break;
		if (fwrite(buf, n_read, 1, dest_fp) < 1) {
			g_warning(_("writing to %s failed.\n"), dest->path);
			ftruncate(fileno(dest_fp), old_size);
			fclose(dest_fp);
			fclose(src_fp);
			return -1;
		}

		if (n_read < sizeof(buf))
			break;
	}

	err = FALSE;

	if (ferror(src_fp)) {
		FILE_OP_ERROR(dest->path, "fread");
	}

	mbox_unlock_file(src_fp, dest->path);

	if (fclose(src_fp) == -1) {
		FILE_OP_ERROR(file, "fclose");
		err = TRUE;
	}

	if (fclose(dest_fp) == -1) {
		FILE_OP_ERROR(dest->path, "fclose");
		return -1;
	}

	if (err) {
		ftruncate(fileno(dest_fp), old_size);
		return -1;
	}

	if (remove_source) {
		if (unlink(file) < 0)
			FILE_OP_ERROR(file, "unlink");
	}

	dest->last_num++;
	return dest->last_num;

}

gint mbox_remove_msg(Folder *folder, FolderItem *item, gint num)
{
	struct _message * msg;
	GList * msg_list;

	mbox_cache_synchronize(item->path);

	msg_list = mbox_cache_get_msg_list(item->path);

	msg = (struct _message *) g_list_nth_data(msg_list,
						  num - 1);
	msg->deleted = 1;

	/*
	if (!mbox_rewrite(item->path)) {
		printf("rewrite %s\n", item->path);
		return -1;
	}
	*/

	return 0;
}

gint mbox_remove_all_msg(Folder *folder, FolderItem *item)
{
	FILE * fp;

	fp = fopen(item->path, "w");
	if (fp == NULL) {
		return -1;
	}

	fclose(fp);

	return 0;
}

gint mbox_move_msg(Folder *folder, FolderItem *dest, MsgInfo *msginfo)
{
	/*

	mbox_cache_synchronize(item->path);

	size = msg->end - msg->offset;

	src = fopen(mbox_filename, "r");
	if (src == NULL)
		return FALSE;
	fseek(src, msg->offset, SEEK_SET);

	mbox_lockread_file(src, mbox_filename);

	dest = fopen(dest_filename, "a");
	if (dest == NULL) {
		fclose(src);
		return FALSE;
	}

	if (change_file_mode_rw(dest, dest_filename) < 0) {
		FILE_OP_ERROR(dest_filename, "chmod");
		g_warning(_("can't change file mode\n"));
	}

	if (!mbox_write_data(src, dest, dest_filename, size)) {
			fclose(dest);
			fclose(src);
			unlink(dest_filename);
			return FALSE;
	}
	
	return -1;
	*/
	gchar * filename;

	filename = mbox_fetch_msg(folder, msginfo->folder, msginfo->msgnum);
	if (filename == NULL)
		return -1;

	((struct _message *) msginfo->data)->deleted = TRUE;

	return mbox_add_msg(folder, dest, filename, TRUE);
}

gint mbox_move_msgs_with_dest(Folder *folder, FolderItem *dest, GSList *msglist)
{
	GSList * l;
	gchar * path = NULL;

	for(l = msglist ; l != NULL ; l = g_slist_next(l)) {
		MsgInfo * msginfo = (MsgInfo *) l->data;

		if (msginfo->folder)
			path = msginfo->folder->path;

		mbox_move_msg(folder, dest, msginfo);
	}

	if (path) {
		mbox_rewrite(path);
		printf("fini\n");
		mbox_cache_synchronize(path);
	}
	mbox_cache_synchronize(dest->path);

	return dest->last_num;
}

void mbox_scan_folder(Folder *folder, FolderItem *item)
{
	gchar *path;
	struct stat s;
	gint max = 0;
	gint num;
	gint n_msg;
	mboxcache * cached;

	mbox_cache_synchronize(item->path);

	cached = mbox_cache_get_mbox(item->path);

	if (cached == NULL) {
		item->new = 0;
		item->unread = 0;
		item->total = 0;
		item->last_num = 0;
		return;
	}

	n_msg = mbox_cache_get_count(item->path);

	if (n_msg == 0)
		item->new = item->unread = item->total = 0;
	else {
		gint new, unread, total;

		//		procmsg_get_mark_sum(".", &new, &unread, &total);
		if (n_msg > total) {
			new += n_msg - total;
			unread += n_msg - total;
		}
		item->new = new;
		item->unread = unread;
		item->total = n_msg;
	}

	debug_print(_("Last number in dir %s = %d\n"), item->path,
		    item->total);
	item->last_num = item->total;
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
		if ((size - pos) > sizeof(buf))
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
	GSList * headers;
	GSList * cur;
	gint size;
	
	fseek(mbox_fp, msg->header, SEEK_SET);
	headers = procheader_get_header_list(mbox_fp);
	for(cur = headers ; cur != NULL ;
	    cur = g_slist_next(cur)) {
		Header * h = (Header *) cur->data;
		
		if (!procheader_headername_equal(h->name, 
						 "Status") &&
		    !procheader_headername_equal(h->name, 
						 "X-Status")) {
			fwrite(h->name, strlen(h->name),
			       1, new_fp);
			fwrite(" ", 1, 1, new_fp);
			fwrite(h->body, strlen(h->body),
			       1, new_fp);
			fwrite("\r\n", 2, 1, new_fp);
		}
	}
	fwrite("\r\n", 2, 1, new_fp);
	
	size = msg->end - msg->content;
	fseek(mbox_fp, msg->content, SEEK_SET);

	return mbox_write_data(mbox_fp, new_fp, new_filename, size);
}

void mbox_update_mark(Folder * folder, FolderItem * item)
{
	GList * msg_list;
	GList * l;

	/*	
	msg_list = mbox_cache_get_msg_list(item->path);
	
	for(l = msg_list ; l != NULL ; l = g_list_next(l)) {
		struct _message * msg = (struct _message *) l->data;

		if (msg->msginfo != NULL) {
			msg->flags = msg->msginfo->flags &
				MSG_PERMANENT_FLAG_MASK;
		}
	}
	*/
}

void mbox_change_flags(Folder * folder, FolderItem * item, MsgInfo * info)
{
	struct _message * msg = (struct _message *) info->data;

	msg->flags = info->flags;
}

gboolean mbox_rewrite(gchar * mbox)
{
	FILE * mbox_fp;
	FILE * new_fp;
	gchar * new;
	GList * l;
	gboolean result;
	gboolean modification = FALSE;
	GList * msg_list;

	msg_list = mbox_cache_get_msg_list(mbox);

	for(l = msg_list ; l != NULL ; l = g_list_next(l)) {
		struct _message * msg = (struct _message *) l->data;
		if (msg->deleted) {
			printf("modification\n");
			modification = TRUE;
			break;
		}
	}

	if (!modification)
		return FALSE;

	mbox_fp = fopen(mbox, "r+");
	mbox_lockwrite_file(mbox_fp, mbox);

	mbox_cache_synchronize_from_file(mbox_fp, mbox);

	new = g_strconcat(mbox, ".new", NULL);
	new_fp = fopen(new, "w");

	mbox_lockwrite_file(new_fp, new);

	result = TRUE;

	msg_list = mbox_cache_get_msg_list(mbox);
	for(l = msg_list ; l != NULL ; l = g_list_next(l)) {
		struct _message * msg = (struct _message *) l->data;
		if (!msg->deleted)
			if (!mbox_write_message(mbox_fp, new_fp, new, msg)) {
				result = FALSE;
				break;
			}
	}

	unlink(mbox);

	g_warning(_("can't rename %s to %s\n"), new, mbox);
	if (rename(new_fp, mbox) == -1) {
		g_warning(_("can't rename %s to %s\n"), new, mbox);
		mbox_unlock_file(new_fp, new);
		fclose(new_fp);
		mbox_unlock_file(mbox_fp, mbox);
		fclose(mbox_fp);
		return -1;
	}

	mbox_unlock_file(new_fp, new);

	fclose(new_fp);

	mbox_unlock_file(mbox_fp, mbox);

	fclose(mbox_fp);
	return result;
}
