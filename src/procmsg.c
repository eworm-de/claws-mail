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
#include <stdio.h>
#include <stdlib.h>

#include "intl.h"
#include "main.h"
#include "utils.h"
#include "procmsg.h"
#include "procheader.h"
#include "send.h"
#include "procmime.h"
#include "statusbar.h"
#include "folder.h"

typedef struct _FlagInfo	FlagInfo;

struct _FlagInfo
{
	guint    msgnum;
	MsgFlags flags;
};

static void mark_sum_func			(gpointer	 key,
						 gpointer	 value,
						 gpointer	 data);

static GHashTable *procmsg_read_mark_file	(const gchar	*folder);
static gint procmsg_cmp_msgnum			(gconstpointer	 a,
						 gconstpointer	 b);
static gint procmsg_cmp_flag_msgnum		(gconstpointer	 a,
						 gconstpointer	 b);


GHashTable *procmsg_msg_hash_table_create(GSList *mlist)
{
	GHashTable *msg_table;

	if (mlist == NULL) return NULL;

	msg_table = g_hash_table_new(NULL, g_direct_equal);
	procmsg_msg_hash_table_append(msg_table, mlist);

	return msg_table;
}

void procmsg_msg_hash_table_append(GHashTable *msg_table, GSList *mlist)
{
	GSList *cur;
	MsgInfo *msginfo;

	if (msg_table == NULL || mlist == NULL) return;

	for (cur = mlist; cur != NULL; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;

		g_hash_table_insert(msg_table,
				    GUINT_TO_POINTER(msginfo->msgnum),
				    msginfo);
	}
}

GHashTable *procmsg_to_folder_hash_table_create(GSList *mlist)
{
	GHashTable *msg_table;
	GSList *cur;
	MsgInfo *msginfo;

	if (mlist == NULL) return NULL;

	msg_table = g_hash_table_new(NULL, g_direct_equal);

	for (cur = mlist; cur != NULL; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;
		g_hash_table_insert(msg_table, msginfo->to_folder, msginfo);
	}

	return msg_table;
}

static gint procmsg_read_cache_data_str(FILE *fp, gchar **str)
{
	gchar buf[BUFFSIZE];
	gint ret = 0;
	size_t len;

	if (fread(&len, sizeof(len), 1, fp) == 1) {
		if (len < 0)
			ret = -1;
		else {
			gchar *tmp = NULL;

			while (len > 0) {
				size_t size = MIN(len, BUFFSIZE - 1);

				if (fread(buf, size, 1, fp) != 1) {
					ret = -1;
					if (tmp) g_free(tmp);
					*str = NULL;
					break;
				}

				buf[size] = '\0';
				if (tmp) {
					*str = g_strconcat(tmp, buf, NULL);
					g_free(tmp);
					tmp = *str;
				} else
					tmp = *str = g_strdup(buf);

				len -= size;
			}
		}
	} else
		ret = -1;

	if (ret < 0)
		g_warning(_("Cache data is corrupted\n"));

	return ret;
}

#define READ_CACHE_DATA(data, fp) \
{ \
	if (procmsg_read_cache_data_str(fp, &data) < 0) { \
		procmsg_msginfo_free(msginfo); \
		break; \
	} \
}

#define READ_CACHE_DATA_INT(n, fp) \
{ \
	if (fread(&n, sizeof(n), 1, fp) != 1) { \
		g_warning(_("Cache data is corrupted\n")); \
		procmsg_msginfo_free(msginfo); \
		break; \
	} \
}

GSList *procmsg_read_cache(FolderItem *item, gboolean scan_file)
{
	GSList *mlist = NULL;
	GSList *last = NULL;
	gchar *cache_file;
	FILE *fp;
	MsgInfo *msginfo;
	MsgFlags default_flags;
	gchar file_buf[BUFFSIZE];
	gint ver;
	guint num;
	FolderType type;

	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(item->folder != NULL, NULL);
	type = item->folder->type;

	default_flags.perm_flags = MSG_NEW|MSG_UNREAD;
	default_flags.tmp_flags = MSG_CACHED;
	if (type == F_MH) {
		if (item->stype == F_QUEUE) {
			MSG_SET_TMP_FLAGS(default_flags, MSG_QUEUED);
		} else if (item->stype == F_DRAFT) {
			MSG_SET_TMP_FLAGS(default_flags, MSG_DRAFT);
		}
	} else if (type == F_IMAP) {
		MSG_SET_TMP_FLAGS(default_flags, MSG_IMAP);
	} else if (type == F_NEWS) {
		MSG_SET_TMP_FLAGS(default_flags, MSG_NEWS);
	}

	if (type == F_MH) {
		gchar *path;

		path = folder_item_get_path(item);
		if (change_dir(path) < 0) {
			g_free(path);
			return NULL;
		}
		g_free(path);
	}
	cache_file = folder_item_get_cache_file(item);
	if ((fp = fopen(cache_file, "r")) == NULL) {
		debug_print(_("\tNo cache file\n"));
		g_free(cache_file);
		return NULL;
	}
	setvbuf(fp, file_buf, _IOFBF, sizeof(file_buf));
	g_free(cache_file);

	debug_print(_("\tReading summary cache...\n"));

	/* compare cache version */
	if (fread(&ver, sizeof(ver), 1, fp) != 1 ||
	    CACHE_VERSION != ver) {
		debug_print(_("Cache version is different. Discarding it.\n"));
		fclose(fp);
		return NULL;
	}

	while (fread(&num, sizeof(num), 1, fp) == 1) {
		msginfo = g_new0(MsgInfo, 1);
		msginfo->msgnum = num;
		READ_CACHE_DATA_INT(msginfo->size, fp);
		READ_CACHE_DATA_INT(msginfo->mtime, fp);
		READ_CACHE_DATA_INT(msginfo->date_t, fp);
		READ_CACHE_DATA_INT(msginfo->flags.tmp_flags, fp);

		READ_CACHE_DATA(msginfo->fromname, fp);

		READ_CACHE_DATA(msginfo->date, fp);
		READ_CACHE_DATA(msginfo->from, fp);
		READ_CACHE_DATA(msginfo->to, fp);
		READ_CACHE_DATA(msginfo->cc, fp);
		READ_CACHE_DATA(msginfo->newsgroups, fp);
		READ_CACHE_DATA(msginfo->subject, fp);
		READ_CACHE_DATA(msginfo->msgid, fp);
		READ_CACHE_DATA(msginfo->inreplyto, fp);
		READ_CACHE_DATA(msginfo->references, fp);

		MSG_SET_PERM_FLAGS(msginfo->flags, default_flags.perm_flags);
		MSG_SET_TMP_FLAGS(msginfo->flags, default_flags.tmp_flags);

		/* if the message file doesn't exist or is changed,
		   don't add the data */
		if (type == F_MH && scan_file &&
		    folder_item_is_msg_changed(item, msginfo))
			procmsg_msginfo_free(msginfo);
		else {
			msginfo->folder = item;

			if (!mlist)
				last = mlist = g_slist_append(NULL, msginfo);
			else {
				last = g_slist_append(last, msginfo);
				last = last->next;
			}
		}
	}

	fclose(fp);
	debug_print(_("done.\n"));

	return mlist;
}

#undef READ_CACHE_DATA
#undef READ_CACHE_DATA_INT

void procmsg_set_flags(GSList *mlist, FolderItem *item)
{
	GSList *cur, *tmp;
	gint newmsg = 0;
	gint lastnum = 0;
	gchar *markdir;
	MsgInfo *msginfo;
	GHashTable *mark_table;
	MsgFlags *flags;

	if (!mlist) return;
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);

	debug_print(_("\tMarking the messages...\n"));

	markdir = folder_item_get_path(item);
	if (!is_dir_exist(markdir))
		make_dir_hier(markdir);

	mark_table = procmsg_read_mark_file(markdir);
	g_free(markdir);

	if (!mark_table) return;

	for (cur = mlist; cur != NULL; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;

		if (lastnum < msginfo->msgnum)
			lastnum = msginfo->msgnum;

		flags = g_hash_table_lookup
			(mark_table, GUINT_TO_POINTER(msginfo->msgnum));

		if (flags != NULL) {
			/* add the permanent flags only */
			msginfo->flags.perm_flags = flags->perm_flags;
			if (item->folder->type == F_IMAP) {
				MSG_SET_TMP_FLAGS(msginfo->flags, MSG_IMAP);
			} else if (item->folder->type == F_NEWS) {
				MSG_SET_TMP_FLAGS(msginfo->flags, MSG_NEWS);
			}
		} else {
			/* not found (new message) */
			if (newmsg == 0) {
				for (tmp = mlist; tmp != cur; tmp = tmp->next)
					MSG_UNSET_PERM_FLAGS
						(((MsgInfo *)tmp->data)->flags,
						 MSG_NEW);
			}
			newmsg++;
		}
	}

	item->last_num = lastnum;

	debug_print(_("done.\n"));
	if (newmsg)
		debug_print(_("\t%d new message(s)\n"), newmsg);

	hash_free_value_mem(mark_table);
	g_hash_table_destroy(mark_table);
}

gint procmsg_get_last_num_in_cache(GSList *mlist)
{
	GSList *cur;
	MsgInfo *msginfo;
	gint last = 0;

	if (mlist == NULL) return 0;

	for (cur = mlist; cur != NULL; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;
		if (msginfo && msginfo->msgnum > last)
			last = msginfo->msgnum;
	}

	return last;
}

void procmsg_msg_list_free(GSList *mlist)
{
	GSList *cur;
	MsgInfo *msginfo;

	for (cur = mlist; cur != NULL; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;
		procmsg_msginfo_free(msginfo);
	}
	g_slist_free(mlist);
}

void procmsg_write_cache(MsgInfo *msginfo, FILE *fp)
{
	MsgTmpFlags flags = msginfo->flags.tmp_flags & MSG_CACHED_FLAG_MASK;

	WRITE_CACHE_DATA_INT(msginfo->msgnum, fp);
	WRITE_CACHE_DATA_INT(msginfo->size, fp);
	WRITE_CACHE_DATA_INT(msginfo->mtime, fp);
	WRITE_CACHE_DATA_INT(msginfo->date_t, fp);
	WRITE_CACHE_DATA_INT(flags, fp);

	WRITE_CACHE_DATA(msginfo->fromname, fp);

	WRITE_CACHE_DATA(msginfo->date, fp);
	WRITE_CACHE_DATA(msginfo->from, fp);
	WRITE_CACHE_DATA(msginfo->to, fp);
	WRITE_CACHE_DATA(msginfo->cc, fp);
	WRITE_CACHE_DATA(msginfo->newsgroups, fp);
	WRITE_CACHE_DATA(msginfo->subject, fp);
	WRITE_CACHE_DATA(msginfo->msgid, fp);
	WRITE_CACHE_DATA(msginfo->inreplyto, fp);
	WRITE_CACHE_DATA(msginfo->references, fp);
}

void procmsg_write_flags(MsgInfo *msginfo, FILE *fp)
{
	MsgPermFlags flags = msginfo->flags.perm_flags;

	WRITE_CACHE_DATA_INT(msginfo->msgnum, fp);
	WRITE_CACHE_DATA_INT(flags, fp);
}

struct MarkSum {
	gint *new;
	gint *unread;
	gint *total;
};

static void mark_sum_func(gpointer key, gpointer value, gpointer data)
{
	MsgFlags *flags = value;
	struct MarkSum *marksum = data;

	if (MSG_IS_NEW(*flags) && !MSG_IS_IGNORE_THREAD(*flags)) (*marksum->new)++;
	if (MSG_IS_UNREAD(*flags) && !MSG_IS_IGNORE_THREAD(*flags)) (*marksum->unread)++;
	(*marksum->total)++;

	g_free(flags);
}

void procmsg_get_mark_sum(const gchar *folder,
			  gint *new, gint *unread, gint *total)
{
	GHashTable *mark_table;
	struct MarkSum marksum;

	*new = *unread = *total = 0;
	marksum.new    = new;
	marksum.unread = unread;
	marksum.total  = total;

	mark_table = procmsg_read_mark_file(folder);

	if (mark_table) {
		g_hash_table_foreach(mark_table, mark_sum_func, &marksum);
		g_hash_table_destroy(mark_table);
	}
}

static GHashTable *procmsg_read_mark_file(const gchar *folder)
{
	FILE *fp;
	GHashTable *mark_table = NULL;
	gint num;
	MsgFlags *flags;
	MsgPermFlags perm_flags;

	if ((fp = procmsg_open_mark_file(folder, FALSE)) == NULL)
		return NULL;

	mark_table = g_hash_table_new(NULL, g_direct_equal);

	while (fread(&num, sizeof(num), 1, fp) == 1) {
		if (fread(&perm_flags, sizeof(flags), 1, fp) != 1) break;

		flags = g_new0(MsgFlags, 1);
		flags->perm_flags = perm_flags;

		g_hash_table_insert(mark_table, GUINT_TO_POINTER(num), flags);
	}

	fclose(fp);
	return mark_table;
}

FILE *procmsg_open_mark_file(const gchar *folder, gboolean append)
{
	gchar *markfile;
	FILE *fp;
	gint ver;

	markfile = g_strconcat(folder, G_DIR_SEPARATOR_S, MARK_FILE, NULL);

	if ((fp = fopen(markfile, "r")) == NULL)
		debug_print(_("Mark file not found.\n"));
	else if (fread(&ver, sizeof(ver), 1, fp) != 1 || MARK_VERSION != ver) {
		debug_print(_("Mark version is different (%d != %d). "
			      "Discarding it.\n"), ver, MARK_VERSION);
		fclose(fp);
		fp = NULL;
	}

	/* read mode */
	if (append == FALSE) {
		g_free(markfile);
		return fp;
	}

	if (fp) {
		/* reopen with append mode */
		fclose(fp);
		if ((fp = fopen(markfile, "a")) == NULL)
			g_warning(_("Can't open mark file with append mode.\n"));
	} else {
		/* open with overwrite mode if mark file doesn't exist or
		   version is different */
		if ((fp = fopen(markfile, "w")) == NULL)
			g_warning(_("Can't open mark file with write mode.\n"));
		else {
			ver = MARK_VERSION;
			WRITE_CACHE_DATA_INT(ver, fp);
		}
	}

	g_free(markfile);
	return fp;
}

/* return the reversed thread tree */
GNode *procmsg_get_thread_tree(GSList *mlist)
{
	GNode *root, *parent, *node, *next;
	GHashTable *msgid_table;
	GHashTable *subject_table;
	MsgInfo *msginfo;
	const gchar *msgid;
	const gchar *subject;

	root = g_node_new(NULL);
	msgid_table = g_hash_table_new(g_str_hash, g_str_equal);
	subject_table = g_hash_table_new(g_str_hash, g_str_equal);

	for (; mlist != NULL; mlist = mlist->next) {
		msginfo = (MsgInfo *)mlist->data;
		parent = root;

		if (msginfo->inreplyto) {
			parent = g_hash_table_lookup(msgid_table, msginfo->inreplyto);
			if (parent == NULL) {
				parent = root;
			} else {
				if(MSG_IS_IGNORE_THREAD(((MsgInfo *)parent->data)->flags)) {
					MSG_SET_PERM_FLAGS(msginfo->flags, MSG_IGNORE_THREAD);
				}
			}
		}
		node = g_node_insert_data_before
			(parent, parent == root ? parent->children : NULL,
			 msginfo);
		if ((msgid = msginfo->msgid) &&
		    g_hash_table_lookup(msgid_table, msgid) == NULL)
			g_hash_table_insert(msgid_table, (gchar *)msgid, node);

		subject = msginfo->subject;
		if (subject_table_lookup(subject_table,
					 (gchar *) subject) == NULL)
			subject_table_insert(subject_table, (gchar *)subject,
					     node);
	}

	/* complete the unfinished threads */
	for (node = root->children; node != NULL; ) {
		next = node->next;
		msginfo = (MsgInfo *)node->data;
		parent = NULL;

		if (msginfo->inreplyto)
			parent = g_hash_table_lookup(msgid_table, msginfo->inreplyto);
		if (parent == NULL && !subject_is_reply(msginfo->subject))
			parent = subject_table_lookup(subject_table, msginfo->subject);
		
		if (parent && parent != node) {
			g_node_unlink(node);
			g_node_insert_before
				(parent, parent->children, node);
			if(MSG_IS_IGNORE_THREAD(((MsgInfo *)parent->data)->flags)) {
				MSG_SET_PERM_FLAGS(msginfo->flags, MSG_IGNORE_THREAD);
			}
		}
		node = next;
	}

	g_hash_table_destroy(subject_table);
	g_hash_table_destroy(msgid_table);

	return root;
}

void procmsg_move_messages(GSList *mlist)
{
	GSList *cur, *movelist = NULL;
	MsgInfo *msginfo;
	FolderItem *dest = NULL;
	GHashTable *hash;

	if (!mlist) return;

	hash = procmsg_to_folder_hash_table_create(mlist);
	folder_item_scan_foreach(hash);
	g_hash_table_destroy(hash);

	for (cur = mlist; cur != NULL; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;
		if (!dest) {
			dest = msginfo->to_folder;
			movelist = g_slist_append(movelist, msginfo);
		} else if (dest == msginfo->to_folder) {
			movelist = g_slist_append(movelist, msginfo);
		} else {
			folder_item_move_msgs_with_dest(dest, movelist);
			g_slist_free(movelist);
			movelist = NULL;
			dest = msginfo->to_folder;
			movelist = g_slist_append(movelist, msginfo);
		}
	}

	if (movelist) {
		folder_item_move_msgs_with_dest(dest, movelist);
		g_slist_free(movelist);
	}
}

void procmsg_copy_messages(GSList *mlist)
{
	GSList *cur, *copylist = NULL;
	MsgInfo *msginfo;
	FolderItem *dest = NULL;
	GHashTable *hash;

	if (!mlist) return;

	hash = procmsg_to_folder_hash_table_create(mlist);
	folder_item_scan_foreach(hash);
	g_hash_table_destroy(hash);

	for (cur = mlist; cur != NULL; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;
		if (!dest) {
			dest = msginfo->to_folder;
			copylist = g_slist_append(copylist, msginfo);
		} else if (dest == msginfo->to_folder) {
			copylist = g_slist_append(copylist, msginfo);
		} else {
			folder_item_copy_msgs_with_dest(dest, copylist);
			g_slist_free(copylist);
			copylist = NULL;
			dest = msginfo->to_folder;
			copylist = g_slist_append(copylist, msginfo);
		}
	}

	if (copylist) {
		folder_item_copy_msgs_with_dest(dest, copylist);
		g_slist_free(copylist);
	}
}

gchar *procmsg_get_message_file_path(MsgInfo *msginfo)
{
	gchar *path, *file;

	g_return_val_if_fail(msginfo != NULL, NULL);

	if (msginfo->plaintext_file)
		file = g_strdup(msginfo->plaintext_file);
	else {
		path = folder_item_get_path(msginfo->folder);
		file = g_strconcat(path, G_DIR_SEPARATOR_S,
				   itos(msginfo->msgnum), NULL);
		g_free(path);
	}

	return file;
}

gchar *procmsg_get_message_file(MsgInfo *msginfo)
{
	gchar *filename = NULL;

	g_return_val_if_fail(msginfo != NULL, NULL);

	filename = folder_item_fetch_msg(msginfo->folder, msginfo->msgnum);
	if (!filename)
		g_warning(_("can't fetch message %d\n"), msginfo->msgnum);

	return filename;
}

FILE *procmsg_open_message(MsgInfo *msginfo)
{
	FILE *fp;
	gchar *file;

	g_return_val_if_fail(msginfo != NULL, NULL);

	file = procmsg_get_message_file_path(msginfo);
	g_return_val_if_fail(file != NULL, NULL);

	if ((fp = fopen(file, "r")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		g_free(file);
		return NULL;
	}

	g_free(file);

	if (MSG_IS_QUEUED(msginfo->flags)) {
		gchar buf[BUFFSIZE];

		while (fgets(buf, sizeof(buf), fp) != NULL)
			if (buf[0] == '\r' || buf[0] == '\n') break;
	}

	return fp;
}

gboolean procmsg_msg_exist(MsgInfo *msginfo)
{
	gchar *path;
	gboolean ret;

	if (!msginfo) return FALSE;

	path = folder_item_get_path(msginfo->folder);
	change_dir(path);
	ret = !folder_item_is_msg_changed(msginfo->folder, msginfo);
	g_free(path);

	return ret;
}

void procmsg_empty_trash(void)
{
	FolderItem *trash;
	GList *cur;

	for (cur = folder_get_list(); cur != NULL; cur = cur->next) {
		trash = FOLDER(cur->data)->trash;
		if (trash) folder_item_remove_all_msg(trash);
	}
}

gint procmsg_send_queue(void)
{
	FolderItem *queue;
	gint i;
	gint ret = 0;

	queue = folder_get_default_queue();
	g_return_val_if_fail(queue != NULL, -1);
	folder_item_scan(queue);
	if (queue->last_num < 0) return -1;
	else if (queue->last_num == 0) return 0;

	for (i = 1; i <= queue->last_num; i++) {
		gchar *file;

		file = folder_item_fetch_msg(queue, i);
		if (file) {
			if (send_message_queue(file) < 0) {
				g_warning(_("Sending queued message %d failed.\n"), i);
				ret = -1;
			} else
				folder_item_remove_msg(queue, i);
			g_free(file);
		}
	}

	return ret;
}

void procmsg_print_message(MsgInfo *msginfo, const gchar *cmdline)
{
	static const gchar *def_cmd = "lpr %s";
	static guint id = 0;
	gchar *prtmp;
	FILE *tmpfp, *prfp;
	gchar buf[1024];
	gchar *p;

	g_return_if_fail(msginfo);

	if ((tmpfp = procmime_get_first_text_content(msginfo)) == NULL) {
		g_warning(_("Can't get text part\n"));
		return;
	}

	prtmp = g_strdup_printf("%s%cprinttmp.%08x",
				get_mime_tmp_dir(), G_DIR_SEPARATOR, id++);

	if ((prfp = fopen(prtmp, "w")) == NULL) {
		FILE_OP_ERROR(prtmp, "fopen");
		g_free(prtmp);
		fclose(tmpfp);
		return;
	}

	if (msginfo->date) fprintf(prfp, "Date: %s\n", msginfo->date);
	if (msginfo->from) fprintf(prfp, "From: %s\n", msginfo->from);
	if (msginfo->to)   fprintf(prfp, "To: %s\n", msginfo->to);
	if (msginfo->cc)   fprintf(prfp, "Cc: %s\n", msginfo->cc);
	if (msginfo->newsgroups)
		fprintf(prfp, "Newsgroups: %s\n", msginfo->newsgroups);
	if (msginfo->subject) fprintf(prfp, "Subject: %s\n", msginfo->subject);
	fputc('\n', prfp);

	while (fgets(buf, sizeof(buf), tmpfp) != NULL)
		fputs(buf, prfp);

	fclose(prfp);
	fclose(tmpfp);

	if (cmdline && (p = strchr(cmdline, '%')) && *(p + 1) == 's' &&
	    !strchr(p + 2, '%'))
		g_snprintf(buf, sizeof(buf) - 1, cmdline, prtmp);
	else {
		if (cmdline)
			g_warning(_("Print command line is invalid: `%s'\n"),
				  cmdline);
		g_snprintf(buf, sizeof(buf) - 1, def_cmd, prtmp);
	}

	g_free(prtmp);

	g_strchomp(buf);
	if (buf[strlen(buf) - 1] != '&') strcat(buf, "&");
	system(buf);
}

MsgInfo *procmsg_msginfo_copy(MsgInfo *msginfo)
{
	MsgInfo *newmsginfo;

	if (msginfo == NULL) return NULL;

	newmsginfo = g_new0(MsgInfo, 1);

#define MEMBCOPY(mmb)	newmsginfo->mmb = msginfo->mmb
#define MEMBDUP(mmb)	newmsginfo->mmb = msginfo->mmb ? \
			g_strdup(msginfo->mmb) : NULL

	MEMBCOPY(msgnum);
	MEMBCOPY(size);
	MEMBCOPY(mtime);
	MEMBCOPY(date_t);
	MEMBCOPY(flags);

	MEMBDUP(fromname);

	MEMBDUP(date);
	MEMBDUP(from);
	MEMBDUP(to);
	MEMBDUP(cc);
	MEMBDUP(newsgroups);
	MEMBDUP(subject);
	MEMBDUP(msgid);
	MEMBDUP(inreplyto);

	MEMBCOPY(folder);
	MEMBCOPY(to_folder);

	MEMBDUP(xface);
	MEMBDUP(dispositionnotificationto);
	MEMBDUP(returnreceiptto);
	MEMBDUP(references);

	MEMBCOPY(score);
	MEMBCOPY(threadscore);

	return newmsginfo;
}

void procmsg_msginfo_free(MsgInfo *msginfo)
{
	if (msginfo == NULL) return;

	g_free(msginfo->fromspace);
	g_free(msginfo->references);
	g_free(msginfo->returnreceiptto);
	g_free(msginfo->dispositionnotificationto);
	g_free(msginfo->xface);

	g_free(msginfo->fromname);

	g_free(msginfo->date);
	g_free(msginfo->from);
	g_free(msginfo->to);
	g_free(msginfo->cc);
	g_free(msginfo->newsgroups);
	g_free(msginfo->subject);
	g_free(msginfo->msgid);
	g_free(msginfo->inreplyto);

	g_free(msginfo);
}

static gint procmsg_cmp_msgnum(gconstpointer a, gconstpointer b)
{
	const MsgInfo *msginfo = a;
	const guint msgnum = GPOINTER_TO_UINT(b);

	if (!msginfo)
		return -1;

	return msginfo->msgnum - msgnum;
}

gint procmsg_cmp_msgnum_for_sort(gconstpointer a, gconstpointer b)
{
	const MsgInfo *msginfo1 = a;
	const MsgInfo *msginfo2 = b;

	if (!msginfo1)
		return -1;
	if (!msginfo2)
		return -1;

	return msginfo1->msgnum - msginfo2->msgnum;
}

static gint procmsg_cmp_flag_msgnum(gconstpointer a, gconstpointer b)
{
	const FlagInfo *finfo = a;
	const guint msgnum = GPOINTER_TO_UINT(b);

	if (!finfo)
		return -1;

	return finfo->msgnum - msgnum;
}
