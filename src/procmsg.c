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
#include "prefs_common.h"
#include "account.h"
#if USE_GPGME
#  include "rfc2015.h"
#endif
#include "alertpanel.h"
#include "news.h"
#include "imap.h"

typedef struct _FlagInfo	FlagInfo;

struct _FlagInfo
{
	guint    msgnum;
	MsgFlags flags;
};

static GHashTable *procmsg_read_mark_file	(const gchar	*folder);
void   procmsg_msginfo_write_flags		(MsgInfo 	*msginfo);


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
	if (type == F_MH || type == F_IMAP) {
		if (item->stype == F_QUEUE) {
			MSG_SET_TMP_FLAGS(default_flags, MSG_QUEUED);
		} else if (item->stype == F_DRAFT) {
			MSG_SET_TMP_FLAGS(default_flags, MSG_DRAFT);
		}
	}
	if (type == F_IMAP) {
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
	if ((fp = fopen(cache_file, "rb")) == NULL) {
		debug_print("\tNo cache file\n");
		g_free(cache_file);
		return NULL;
	}
	setvbuf(fp, file_buf, _IOFBF, sizeof(file_buf));
	g_free(cache_file);

	debug_print("\tReading summary cache...\n");

	/* compare cache version */
	if (fread(&ver, sizeof(ver), 1, fp) != 1 ||
	    CACHE_VERSION != ver) {
		debug_print("Cache version is different. Discarding it.\n");
		fclose(fp);
		return NULL;
	}

	while (fread(&num, sizeof(num), 1, fp) == 1) {
		msginfo = procmsg_msginfo_new();
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
                READ_CACHE_DATA(msginfo->xref, fp);


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
	debug_print("done.\n");

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

	debug_print("\tMarking the messages...\n");

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

	debug_print("done.\n");
	if (newmsg)
		debug_print("\t%d new message(s)\n", newmsg);

	hash_free_value_mem(mark_table);
	g_hash_table_destroy(mark_table);
}

gint procmsg_get_last_num_in_msg_list(GSList *mlist)
{
	GSList *cur;
	MsgInfo *msginfo;
	gint last = 0;

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
	WRITE_CACHE_DATA(msginfo->xref, fp);

}

void procmsg_write_flags(MsgInfo *msginfo, FILE *fp)
{
	MsgPermFlags flags = msginfo->flags.perm_flags;

	WRITE_CACHE_DATA_INT(msginfo->msgnum, fp);
	WRITE_CACHE_DATA_INT(flags, fp);
}

void procmsg_flush_mark_queue(FolderItem *item, FILE *fp)
{
	MsgInfo *flaginfo;

	g_return_if_fail(item != NULL);
	g_return_if_fail(fp != NULL);

	while (item->mark_queue != NULL) {
		flaginfo = (MsgInfo *)item->mark_queue->data;
		procmsg_write_flags(flaginfo, fp);
		procmsg_msginfo_free(flaginfo);
		item->mark_queue = g_slist_remove(item->mark_queue, flaginfo);
	}
}

void procmsg_add_flags(FolderItem *item, gint num, MsgFlags flags)
{
	FILE *fp;
	gchar *path;
	MsgInfo msginfo;

	g_return_if_fail(item != NULL);

	if (item->opened) {
		MsgInfo *queue_msginfo;

		queue_msginfo = g_new0(MsgInfo, 1);
		queue_msginfo->msgnum = num;
		queue_msginfo->flags = flags;
		item->mark_queue = g_slist_append
			(item->mark_queue, queue_msginfo);
		return;
	}

	path = folder_item_get_path(item);
	g_return_if_fail(path != NULL);

	if ((fp = procmsg_open_mark_file(path, TRUE)) == NULL) {
		g_warning(_("can't open mark file\n"));
		g_free(path);
		return;
	}
	g_free(path);

	msginfo.msgnum = num;
	msginfo.flags = flags;

	procmsg_write_flags(&msginfo, fp);
	fclose(fp);
}

struct MarkSum {
	gint *new;
	gint *unread;
	gint *total;
	gint *min;
	gint *max;
	gint first;
};

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
		if (fread(&perm_flags, sizeof(perm_flags), 1, fp) != 1) break;

		flags = g_hash_table_lookup(mark_table, GUINT_TO_POINTER(num));
		if (flags != NULL)
			g_free(flags);

		flags = g_new0(MsgFlags, 1);
		flags->perm_flags = perm_flags;
    
		if(!MSG_IS_REALLY_DELETED(*flags)) {
			g_hash_table_insert(mark_table, GUINT_TO_POINTER(num), flags);
		} else {
			g_hash_table_remove(mark_table, GUINT_TO_POINTER(num));
		}
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

	if ((fp = fopen(markfile, "rb")) == NULL)
		debug_print("Mark file not found.\n");
	else if (fread(&ver, sizeof(ver), 1, fp) != 1 || MARK_VERSION != ver) {
		debug_print("Mark version is different (%d != %d). "
			      "Discarding it.\n", ver, MARK_VERSION);
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
		if ((fp = fopen(markfile, "ab")) == NULL)
			g_warning(_("Can't open mark file with append mode.\n"));
	} else {
		/* open with overwrite mode if mark file doesn't exist or
		   version is different */
		if ((fp = fopen(markfile, "wb")) == NULL)
			g_warning(_("Can't open mark file with write mode.\n"));
		else {
			ver = MARK_VERSION;
			WRITE_CACHE_DATA_INT(ver, fp);
		}
	}

	g_free(markfile);
	return fp;
}

static gboolean procmsg_ignore_node(GNode *node, gpointer data)
{
	MsgInfo *msginfo = (MsgInfo *)node->data;
	
	procmsg_msginfo_set_flags(msginfo, MSG_IGNORE_THREAD, 0);

	return FALSE;
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
	GNode *found_subject;

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
				if(MSG_IS_IGNORE_THREAD(((MsgInfo *)parent->data)->flags) && !MSG_IS_IGNORE_THREAD(msginfo->flags)) {
					procmsg_msginfo_set_flags(msginfo, MSG_IGNORE_THREAD, 0);
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
		found_subject = subject_table_lookup(subject_table,
						     (gchar *) subject);
		if (found_subject == NULL)
			subject_table_insert(subject_table, (gchar *) subject,
					     node);
		else {
			/* replace if msg in table is older than current one 
			 * can add here more stuff.  */
			if ( ((MsgInfo*)(found_subject->data))->date_t >
			     ((MsgInfo*)(node->data))->date_t )  {
				subject_table_remove(subject_table, (gchar *) subject);
				subject_table_insert(subject_table, (gchar *) subject, node);
			}	
		}
	}

	/* complete the unfinished threads */
	for (node = root->children; node != NULL; ) {
		next = node->next;
		msginfo = (MsgInfo *)node->data;
		parent = NULL;
		if (msginfo->inreplyto) 
			parent = g_hash_table_lookup(msgid_table, msginfo->inreplyto);
		if (parent && parent != node) {
			g_node_unlink(node);
			g_node_insert_before
				(parent, parent->children, node);
			/* CLAWS: ignore thread */
			if(MSG_IS_IGNORE_THREAD(((MsgInfo *)parent->data)->flags) && !MSG_IS_IGNORE_THREAD(msginfo->flags)) {
				g_node_traverse(node, G_PRE_ORDER, G_TRAVERSE_ALL, -1, procmsg_ignore_node, NULL);
			}
		}
		node = next;
	}

	/* CLAWS: now see if the first level (below root) still has some nodes that can be
	 * threaded by subject line. we need to handle this in a special way to prevent
	 * circular reference from a node that has already been threaded by IN-REPLY-TO
	 * but is also in the subject line hash table */
	for (node = root->children; node != NULL; ) {
		next = node->next;
		msginfo = (MsgInfo *) node->data;
		parent = NULL;
		if (subject_is_reply(msginfo->subject)) {
			parent = subject_table_lookup(subject_table,
						      msginfo->subject);
			/* the node may already be threaded by IN-REPLY-TO,
			   so go up in the tree to find the parent node */
			if (parent != NULL) {
				if (g_node_is_ancestor(node, parent))
					parent = NULL;
				if (parent == node)
					parent = NULL;
			}

			if (parent) {
				g_node_unlink(node);
				g_node_append(parent, node);
				/* CLAWS: ignore thread */
				if(MSG_IS_IGNORE_THREAD(((MsgInfo *)parent->data)->flags) && !MSG_IS_IGNORE_THREAD(msginfo->flags)) {
					g_node_traverse(node, G_PRE_ORDER, G_TRAVERSE_ALL, -1, procmsg_ignore_node, NULL);
				}
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

	if (!mlist) return;

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

	if (!mlist) return;

	/* 
	
	Horrible: Scanning 2 times for every copy!

	hash = procmsg_to_folder_hash_table_create(mlist);
	folder_item_scan_foreach(hash);
	g_hash_table_destroy(hash);
	*/

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

	if (!is_file_exist(file)) {
		g_free(file);
		file = procmsg_get_message_file(msginfo);
		g_return_val_if_fail(file != NULL, NULL);
	}

	if ((fp = fopen(file, "rb")) == NULL) {
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

#if USE_GPGME
FILE *procmsg_open_message_decrypted(MsgInfo *msginfo, MimeInfo **mimeinfo)
{
	FILE *fp;
	MimeInfo *mimeinfo_;

	g_return_val_if_fail(msginfo != NULL, NULL);

	if (mimeinfo) *mimeinfo = NULL;

	if ((fp = procmsg_open_message(msginfo)) == NULL) return NULL;

	mimeinfo_ = procmime_scan_mime_header(fp);
	if (!mimeinfo_) {
		fclose(fp);
		return NULL;
	}

	if (!MSG_IS_ENCRYPTED(msginfo->flags) &&
	    rfc2015_is_encrypted(mimeinfo_)) {
		MSG_SET_TMP_FLAGS(msginfo->flags, MSG_ENCRYPTED);
	}

	if (MSG_IS_ENCRYPTED(msginfo->flags) &&
	    !msginfo->plaintext_file &&
	    !msginfo->decryption_failed) {
		rfc2015_decrypt_message(msginfo, mimeinfo_, fp);
		if (msginfo->plaintext_file &&
		    !msginfo->decryption_failed) {
			fclose(fp);
			procmime_mimeinfo_free_all(mimeinfo_);
			if ((fp = procmsg_open_message(msginfo)) == NULL)
				return NULL;
			mimeinfo_ = procmime_scan_mime_header(fp);
			if (!mimeinfo_) {
				fclose(fp);
				return NULL;
			}
		}
	}

	if (mimeinfo) *mimeinfo = mimeinfo_;
	return fp;
}
#endif

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
		if (trash && trash->total > 0)
			folder_item_remove_all_msg(trash);
	}
}

gint procmsg_send_queue(FolderItem *queue, gboolean save_msgs)
{
	gint ret = 0;
	GSList *list, *elem;

	if (!queue)
		queue = folder_get_default_queue();
	g_return_val_if_fail(queue != NULL, -1);

	folder_item_scan(queue);
	list = folder_item_get_msg_list(queue);


	for(elem = list; elem != NULL; elem = elem->next) {
		gchar *file;
		MsgInfo *msginfo;
		
		msginfo = (MsgInfo *)(elem->data);

		file = folder_item_fetch_msg(queue, msginfo->msgnum);
		if (file) {
			if (procmsg_send_message_queue(file) < 0) {
				g_warning(_("Sending queued message %d failed.\n"), msginfo->msgnum);
				ret = -1;
			} else {
			/* CLAWS: 
			 * We save in procmsg_send_message_queue because
			 * we need the destination folder from the queue
			 * header
			 			
				if (save_msgs)
					procmsg_save_to_outbox
						(queue->folder->outbox,
						 file, TRUE);
*/
				folder_item_remove_msg(queue, msginfo->msgnum);
			}
			g_free(file);
		}
		procmsg_msginfo_free(msginfo);
	}

	folderview_update_item(queue, FALSE);

	return ret;
}

gint procmsg_save_to_outbox(FolderItem *outbox, const gchar *file,
			    gboolean is_queued)
{
	gint num;
	FILE *fp;
	MsgInfo *msginfo;

	debug_print("saving sent message...\n");

	if (!outbox)
		outbox = folder_get_default_outbox();
	g_return_val_if_fail(outbox != NULL, -1);

	/* remove queueing headers */
	if (is_queued) {
		gchar tmp[MAXPATHLEN + 1];
		gchar buf[BUFFSIZE];
		FILE *outfp;

		g_snprintf(tmp, sizeof(tmp), "%s%ctmpmsg.out.%08x",
			   get_rc_dir(), G_DIR_SEPARATOR, (guint)random());
		if ((fp = fopen(file, "rb")) == NULL) {
			FILE_OP_ERROR(file, "fopen");
			return -1;
		}
		if ((outfp = fopen(tmp, "wb")) == NULL) {
			FILE_OP_ERROR(tmp, "fopen");
			fclose(fp);
			return -1;
		}
		while (fgets(buf, sizeof(buf), fp) != NULL)
			if (buf[0] == '\r' || buf[0] == '\n') break;
		while (fgets(buf, sizeof(buf), fp) != NULL)
			fputs(buf, outfp);
		fclose(outfp);
		fclose(fp);

		folder_item_scan(outbox);
		if ((num = folder_item_add_msg(outbox, tmp, TRUE)) < 0) {
			g_warning("can't save message\n");
			unlink(tmp);
			return -1;
		}
	} else {
		folder_item_scan(outbox);
		if ((num = folder_item_add_msg(outbox, file, FALSE)) < 0) {
			g_warning("can't save message\n");
			return -1;
		}
		return -1;
	}
	msginfo = folder_item_fetch_msginfo(outbox, num);
	if(msginfo != NULL) {
	    procmsg_msginfo_unset_flags(msginfo, ~0, 0);
	    procmsg_msginfo_free(msginfo);
	}

	if(is_queued) {
		unlink(file);
	}

	return 0;
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

	if ((prfp = fopen(prtmp, "wb")) == NULL) {
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

MsgInfo *procmsg_msginfo_new_ref(MsgInfo *msginfo)
{
	msginfo->refcnt++;
	
	return msginfo;
}

MsgInfo *procmsg_msginfo_new()
{
	MsgInfo *newmsginfo;

	newmsginfo = g_new0(MsgInfo, 1);
	newmsginfo->refcnt = 1;
	
	return newmsginfo;
}

MsgInfo *procmsg_msginfo_copy(MsgInfo *msginfo)
{
	MsgInfo *newmsginfo;

	if (msginfo == NULL) return NULL;

	newmsginfo = g_new0(MsgInfo, 1);

	newmsginfo->refcnt = 1;

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
	MEMBDUP(xref);

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

	msginfo->refcnt--;
	if(msginfo->refcnt > 0)
		return;

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
	g_free(msginfo->xref);

	g_free(msginfo);
}

guint procmsg_msginfo_memusage(MsgInfo *msginfo)
{
	guint memusage = 0;
	
	memusage += sizeof(MsgInfo);
	if(msginfo->fromname)
		memusage += strlen(msginfo->fromname);
	if(msginfo->date)
		memusage += strlen(msginfo->date);
	if(msginfo->from)
		memusage += strlen(msginfo->from);
	if(msginfo->to)
		memusage += strlen(msginfo->to);
	if(msginfo->cc)
		memusage += strlen(msginfo->cc);
	if(msginfo->newsgroups)
		memusage += strlen(msginfo->newsgroups);
	if(msginfo->subject)
		memusage += strlen(msginfo->subject);
	if(msginfo->msgid)
		memusage += strlen(msginfo->msgid);
	if(msginfo->inreplyto)
		memusage += strlen(msginfo->inreplyto);
	if(msginfo->xface)
		memusage += strlen(msginfo->xface);
	if(msginfo->dispositionnotificationto)
		memusage += strlen(msginfo->dispositionnotificationto);
	if(msginfo->returnreceiptto)
		memusage += strlen(msginfo->returnreceiptto);
	if(msginfo->references)
		memusage += strlen(msginfo->references);
	if(msginfo->fromspace)
		memusage += strlen(msginfo->fromspace);

	return memusage;
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

enum
{
	Q_SENDER           = 0,
	Q_SMTPSERVER       = 1,
	Q_RECIPIENTS       = 2,
	Q_NEWSGROUPS       = 3,
	Q_MAIL_ACCOUNT_ID  = 4,
	Q_NEWS_ACCOUNT_ID  = 5,
	Q_SAVE_COPY_FOLDER = 6,
	Q_REPLY_MESSAGE_ID = 7,
};

gint procmsg_send_message_queue(const gchar *file)
{
	static HeaderEntry qentry[] = {{"S:",    NULL, FALSE},
				       {"SSV:",  NULL, FALSE},
				       {"R:",    NULL, FALSE},
				       {"NG:",   NULL, FALSE},
				       {"MAID:", NULL, FALSE},
				       {"NAID:", NULL, FALSE},
				       {"SCF:",  NULL, FALSE},
				       {"RMID:", NULL, FALSE},
				       {NULL,    NULL, FALSE}};
	FILE *fp;
	gint filepos;
	gint mailval = 0, newsval = 0;
	gchar *from = NULL;
	gchar *smtpserver = NULL;
	GSList *to_list = NULL;
	GSList *newsgroup_list = NULL;
	gchar *savecopyfolder = NULL;
	gchar *replymessageid = NULL;
	gchar buf[BUFFSIZE];
	gint hnum;
	PrefsAccount *mailac = NULL, *newsac = NULL;
	int local = 0;

	g_return_val_if_fail(file != NULL, -1);

	if ((fp = fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		return -1;
	}

	while ((hnum = procheader_get_one_field(buf, sizeof(buf), fp, qentry))
	       != -1) {
		gchar *p = buf + strlen(qentry[hnum].name);

		switch (hnum) {
		case Q_SENDER:
			if (!from) from = g_strdup(p);
			break;
		case Q_SMTPSERVER:
			if (!smtpserver) smtpserver = g_strdup(p);
			break;
		case Q_RECIPIENTS:
			to_list = address_list_append(to_list, p);
			break;
		case Q_NEWSGROUPS:
			newsgroup_list = newsgroup_list_append(newsgroup_list, p);
			break;
		case Q_MAIL_ACCOUNT_ID:
			mailac = account_find_from_id(atoi(p));
			break;
		case Q_NEWS_ACCOUNT_ID:
			newsac = account_find_from_id(atoi(p));
			break;
		case Q_SAVE_COPY_FOLDER:
			if (!savecopyfolder) savecopyfolder = g_strdup(p);
			break;
		case Q_REPLY_MESSAGE_ID:
			if (!replymessageid) replymessageid = g_strdup(p);
			break;
		}
	}
	filepos = ftell(fp);

	if (to_list) {
		debug_print("Sending message by mail\n");
		if(!from) {
			g_warning(_("Queued message header is broken.\n"));
			mailval = -1;
		} else if (mailac && mailac->use_mail_command &&
			   mailac->mail_command && (* mailac->mail_command)) {
			mailval = send_message_local(mailac->mail_command, fp);
			local = 1;
		} else if (prefs_common.use_extsend && prefs_common.extsend_cmd) {
			mailval = send_message_local(prefs_common.extsend_cmd, fp);
			local = 1;
		} else {
			if (!mailac) {
				mailac = account_find_from_smtp_server(from, smtpserver);
				if (!mailac) {
					g_warning(_("Account not found. "
						    "Using current account...\n"));
					mailac = cur_account;
				}
			}

			if (mailac)
				mailval = send_message_smtp(mailac, to_list, fp);
			else {
				PrefsAccount tmp_ac;

				g_warning(_("Account not found.\n"));

				memset(&tmp_ac, 0, sizeof(PrefsAccount));
				tmp_ac.address = from;
				tmp_ac.smtp_server = smtpserver;
				tmp_ac.smtpport = SMTP_PORT;
				mailval = send_message_smtp(&tmp_ac, to_list, fp);
			}
		}
		if (mailval < 0) {
            		if (!local)
				alertpanel_error(
					_("Error occurred while sending the message to `%s'."),
					mailac ? mailac->smtp_server : smtpserver);
			else
				alertpanel_error(
					_("Error occurred while sending the message with command `%s'."),
					(mailac && mailac->use_mail_command && 
					 mailac->mail_command && (*mailac->mail_command)) ? 
						mailac->mail_command : prefs_common.extsend_cmd);
		}
	}

	fseek(fp, filepos, SEEK_SET);
	if(newsgroup_list && (newsval == 0)) {
		Folder *folder;
		gchar *tmp = NULL;
		FILE *tmpfp;

    		/* write to temporary file */
    		tmp = g_strdup_printf("%s%ctmp%d", g_get_tmp_dir(),
                    	    G_DIR_SEPARATOR, (gint)file);
    		if ((tmpfp = fopen(tmp, "wb")) == NULL) {
            		FILE_OP_ERROR(tmp, "fopen");
            		newsval = -1;
			alertpanel_error(_("Could not create temporary file for news sending."));
    		} else {
    			if (change_file_mode_rw(tmpfp, tmp) < 0) {
            			FILE_OP_ERROR(tmp, "chmod");
            			g_warning(_("can't change file mode\n"));
    			}

			while ((newsval == 0) && fgets(buf, sizeof(buf), fp) != NULL) {
				if (fputs(buf, tmpfp) == EOF) {
					FILE_OP_ERROR(tmp, "fputs");
					newsval = -1;
					alertpanel_error(_("Error when writing temporary file for news sending."));
				}
			}
			fclose(tmpfp);

			if(newsval == 0) {
				debug_print("Sending message by news\n");

				folder = FOLDER(newsac->folder);

    				newsval = news_post(folder, tmp);
    				if (newsval < 0) {
            				alertpanel_error(_("Error occurred while posting the message to %s ."),
                            			 newsac->nntp_server);
    				}
			}
			unlink(tmp);
		}
		g_free(tmp);
	}

	slist_free_strings(to_list);
	g_slist_free(to_list);
	slist_free_strings(newsgroup_list);
	g_slist_free(newsgroup_list);
	g_free(from);
	g_free(smtpserver);
	fclose(fp);

	/* save message to outbox */
	if (mailval == 0 && newsval == 0 && savecopyfolder) {
		FolderItem *outbox;

		debug_print("saving sent message...\n");

		outbox = folder_find_item_from_identifier(savecopyfolder);
		if(!outbox)
			outbox = folder_get_default_outbox();

		procmsg_save_to_outbox(outbox, file, TRUE);
	}

	if(replymessageid != NULL) {
		gchar **tokens;
		FolderItem *item;
		
		tokens = g_strsplit(replymessageid, "\x7f", 0);
		item = folder_find_item_from_identifier(tokens[0]);
		if(item != NULL) {
			MsgInfo *msginfo;
			
			msginfo = folder_item_fetch_msginfo(item, atoi(tokens[1]));
			if((msginfo != NULL) && (strcmp(msginfo->msgid, tokens[2]) != 0)) {
				procmsg_msginfo_free(msginfo);
				msginfo = NULL;
			}
			
			if(msginfo == NULL) {
				msginfo = folder_item_fetch_msginfo_by_id(item, tokens[2]);
			}
			
			if(msginfo != NULL) {
				procmsg_msginfo_unset_flags(msginfo, MSG_FORWARDED, 0);
				procmsg_msginfo_set_flags(msginfo, MSG_REPLIED, 0);

				procmsg_msginfo_free(msginfo);
			}
		}
		g_strfreev(tokens);
	}

	g_free(savecopyfolder);
	g_free(replymessageid);
	
	return (newsval != 0 ? newsval : mailval);
}

#define CHANGE_FLAGS(msginfo) \
{ \
if (msginfo->folder->folder->change_flags != NULL) \
msginfo->folder->folder->change_flags(msginfo->folder->folder, \
				      msginfo->folder, \
				      msginfo); \
}

void procmsg_msginfo_set_flags(MsgInfo *msginfo, MsgPermFlags perm_flags, MsgTmpFlags tmp_flags)
{
	gboolean changed = FALSE;
	FolderItem *item = msginfo->folder;

	debug_print("Setting flags for message %d in folder %s\n", msginfo->msgnum, item->path);

	/* if new flag is set */
	if((perm_flags & MSG_NEW) && !MSG_IS_NEW(msginfo->flags) &&
	   !MSG_IS_IGNORE_THREAD(msginfo->flags)) {
		item->new++;
		changed = TRUE;
	}

	/* if unread flag is set */
	if((perm_flags & MSG_UNREAD) && !MSG_IS_UNREAD(msginfo->flags) &&
	   !MSG_IS_IGNORE_THREAD(msginfo->flags)) {
		item->unread++;
		changed = TRUE;
	}

	/* if ignore thread flag is set */
	if((perm_flags & MSG_IGNORE_THREAD) && !MSG_IS_IGNORE_THREAD(msginfo->flags)) {
		if(MSG_IS_NEW(msginfo->flags) || (perm_flags & MSG_NEW)) {
			item->new--;
			changed = TRUE;
		}
		if(MSG_IS_UNREAD(msginfo->flags) || (perm_flags & MSG_UNREAD)) {
			item->unread--;
			changed = TRUE;
		}
	}

	if (MSG_IS_IMAP(msginfo->flags))
		imap_msg_set_perm_flags(msginfo, perm_flags);

	msginfo->flags.perm_flags |= perm_flags;
	msginfo->flags.tmp_flags |= tmp_flags;

	if(changed) {
		folderview_update_item(item, FALSE);
	}
	CHANGE_FLAGS(msginfo);
	procmsg_msginfo_write_flags(msginfo);
}

void procmsg_msginfo_unset_flags(MsgInfo *msginfo, MsgPermFlags perm_flags, MsgTmpFlags tmp_flags)
{
	gboolean changed = FALSE;
	FolderItem *item = msginfo->folder;
	
	debug_print("Unsetting flags for message %d in folder %s\n", msginfo->msgnum, item->path);

	/* if new flag is unset */
	if((perm_flags & MSG_NEW) && MSG_IS_NEW(msginfo->flags) &&
	   !MSG_IS_IGNORE_THREAD(msginfo->flags)) {
		item->new--;
		changed = TRUE;
	}

	/* if unread flag is unset */
	if((perm_flags & MSG_UNREAD) && MSG_IS_UNREAD(msginfo->flags) &&
	   !MSG_IS_IGNORE_THREAD(msginfo->flags)) {
		item->unread--;
		changed = TRUE;
	}

	/* if ignore thread flag is unset */
	if((perm_flags & MSG_IGNORE_THREAD) && MSG_IS_IGNORE_THREAD(msginfo->flags)) {
		if(MSG_IS_NEW(msginfo->flags) && !(perm_flags & MSG_NEW)) {
			item->new++;
			changed = TRUE;
		}
		if(MSG_IS_UNREAD(msginfo->flags) && !(perm_flags & MSG_UNREAD)) {
			item->unread++;
			changed = TRUE;
		}
	}

	if (MSG_IS_IMAP(msginfo->flags))
		imap_msg_unset_perm_flags(msginfo, perm_flags);

	msginfo->flags.perm_flags &= ~perm_flags;
	msginfo->flags.tmp_flags &= ~tmp_flags;

	if(changed) {
		folderview_update_item(item, FALSE);
	}
	CHANGE_FLAGS(msginfo);
	procmsg_msginfo_write_flags(msginfo);
}

void procmsg_msginfo_write_flags(MsgInfo *msginfo)
{
	gchar *destdir;
	FILE *fp;

	destdir = folder_item_get_path(msginfo->folder);
	if (!is_dir_exist(destdir))
		make_dir_hier(destdir);

	if ((fp = procmsg_open_mark_file(destdir, TRUE))) {
		procmsg_write_flags(msginfo, fp);
		fclose(fp);
	} else {
		g_warning(_("Can't open mark file.\n"));
	}
	
	g_free(destdir);
}
