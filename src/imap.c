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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>

#include "intl.h"
#include "imap.h"
#include "socket.h"
#include "recv.h"
#include "procmsg.h"
#include "procheader.h"
#include "folder.h"
#include "statusbar.h"
#include "prefs_account.h"
#include "codeconv.h"
#include "utils.h"

#define IMAP4_PORT	143

static GList *session_list = NULL;

static gint imap_cmd_count = 0;

static IMAPSession *imap_session_connect_if_not	(Folder		*folder);

static gint imap_do_copy		(Folder		*folder,
					 FolderItem	*dest,
					 MsgInfo	*msginfo,
					 gboolean	 remove_source);
static gint imap_do_copy_msgs_with_dest	(Folder		*folder,
					 FolderItem	*dest,
					 GSList		*msglist,
					 gboolean	 remove_source);

static GSList *imap_get_uncached_messages	(IMAPSession	*session,
						 FolderItem	*item,
						 gint		 first,
						 gint		 last);
static GSList *imap_delete_cached_messages	(GSList		*mlist,
						 gint		 first,
						 gint		 last);
static void imap_delete_all_cached_messages	(FolderItem	*item);

static SockInfo *imap_open		(const gchar	*server,
					 gushort	 port,
					 gchar		*buf);

static gint imap_set_article_flags	(IMAPSession	*session,
					 gint		 first,
					 gint		 last,
					 IMAPFlags	 flag,
					 gboolean	 is_set);

static void imap_parse_namespace	(IMAPSession	*session,
					 Folder		*folder);

static gchar *imap_parse_atom		(SockInfo *sock,
					 gchar	  *src,
					 gchar	  *dest,
					 gchar	  *orig_buf);
static gchar *imap_parse_one_address	(SockInfo *sock,
					 gchar	  *start,
					 gchar	  *out_from_str,
					 gchar	  *out_fromname_str,
					 gchar	  *orig_buf);
static gchar *imap_parse_address	(SockInfo *sock,
					 gchar	  *start,
					 gchar   **out_from_str,
					 gchar   **out_fromname_str,
					 gchar	  *orig_buf);
static MsgFlags imap_parse_flags	(const gchar	*flag_str);
static MsgInfo *imap_parse_envelope	(SockInfo *sock,
					 gchar	  *line_str);

/* low-level IMAP4rev1 commands */
static gint imap_cmd_login	(SockInfo	*sock,
				 const gchar	*user,
				 const gchar	*pass);
static gint imap_cmd_logout	(SockInfo	*sock);
static gint imap_cmd_noop	(SockInfo	*sock);
static gint imap_cmd_namespace	(SockInfo	*sock,
				 gchar	       **ns_str);
static gint imap_cmd_select	(SockInfo	*sock,
				 const gchar	*folder,
				 gint		*exists,
				 gint		*recent,
				 gint		*unseen,
				 gulong		*uid);
static gint imap_cmd_status	(SockInfo	*sock,
				 const gchar	*folder,
				 const gchar	*status);
static gint imap_cmd_create	(SockInfo	*sock,
				 const gchar	*folder);
static gint imap_cmd_delete	(SockInfo	*sock,
				 const gchar	*folder);
static gint imap_cmd_envelope	(SockInfo	*sock,
				 gint		 first,
				 gint		 last);
#if 0
static gint imap_cmd_search	(SockInfo	*sock,
				 GSList		*numlist);
#endif
static gint imap_cmd_fetch	(SockInfo	*sock,
				 gint		 num,
				 const gchar	*filename);
static gint imap_cmd_append	(SockInfo	*sock,
				 const gchar	*destfolder,
				 const gchar	*file);
static gint imap_cmd_copy	(SockInfo	*sock,
				 gint		 num,
				 const gchar	*destfolder);
static gint imap_cmd_store	(SockInfo	*sock,
				 gint		 first,
				 gint		 last,
				 gchar		*sub_cmd);
static gint imap_cmd_expunge	(SockInfo	*sock);

static gint imap_cmd_ok		(SockInfo	*sock,
				 GPtrArray	*argbuf);
static void imap_cmd_gen_send	(SockInfo	*sock,
				 const gchar	*format, ...);
static gint imap_cmd_gen_recv	(SockInfo	*sock,
				 gchar		*buf,
				 gint		 size);

/* misc utility functions */
static gchar *strchr_cpy		(const gchar	*src,
					 gchar		 ch,
					 gchar		*dest,
					 gint		 len);
static gchar *search_array_contain_str	(GPtrArray	*array,
					 gchar		*str);
static void imap_path_subst_slash_to_dot(gchar *str);


static IMAPSession *imap_session_connect_if_not(Folder *folder)
{
	RemoteFolder *rfolder = REMOTE_FOLDER(folder);

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(folder->type == F_IMAP, NULL);
	g_return_val_if_fail(folder->account != NULL, NULL);

	if (!rfolder->session) {
		rfolder->session =
			imap_session_new(folder->account->recv_server,
					 IMAP4_PORT,
					 folder->account->userid,
					 folder->account->passwd);
		statusbar_pop_all();
		return IMAP_SESSION(rfolder->session);
	}

	if (imap_cmd_noop(rfolder->session->sock) != IMAP_SUCCESS) {
		log_warning(_("IMAP4 connection to %s:%d has been"
			      " disconnected. Reconnecting...\n"),
			    folder->account->recv_server, IMAP4_PORT);
		session_destroy(rfolder->session);
		rfolder->session =
			imap_session_new(folder->account->recv_server,
					 IMAP4_PORT, folder->account->userid,
					 folder->account->passwd);
	}

	statusbar_pop_all();
	return IMAP_SESSION(rfolder->session);
}

Session *imap_session_new(const gchar *server, gushort port,
			  const gchar *user, const gchar *pass)
{
	gchar buf[IMAPBUFSIZE];
	IMAPSession *session;
	SockInfo *imap_sock;

	g_return_val_if_fail(server != NULL, NULL);

	log_message(_("creating IMAP4 connection to %s:%d ...\n"),
		    server, port);

	if ((imap_sock = imap_open(server, port, buf)) == NULL)
		return NULL;
	if (imap_cmd_login(imap_sock, user, pass) != IMAP_SUCCESS) {
		imap_cmd_logout(imap_sock);
		sock_close(imap_sock);
		return NULL;
	}

	session = g_new(IMAPSession, 1);
	SESSION(session)->type      = SESSION_IMAP;
	SESSION(session)->server    = g_strdup(server);
	SESSION(session)->sock      = imap_sock;
	SESSION(session)->connected = TRUE;
	SESSION(session)->phase     = SESSION_READY;
	SESSION(session)->data      = NULL;
	session->mbox = NULL;

	session_list = g_list_append(session_list, session);

	return SESSION(session);
}

void imap_session_destroy(IMAPSession *session)
{
	sock_close(SESSION(session)->sock);
	SESSION(session)->sock = NULL;

	g_free(session->mbox);

	session_list = g_list_remove(session_list, session);
}

void imap_session_destroy_all(void)
{
	while (session_list != NULL) {
		IMAPSession *session = (IMAPSession *)session_list->data;

		imap_cmd_logout(SESSION(session)->sock);
		imap_session_destroy(session);
	}
}

GSList *imap_get_msg_list(Folder *folder, FolderItem *item, gboolean use_cache)
{
	GSList *mlist = NULL;
	IMAPSession *session;
	gint ok, exists = 0, recent = 0, unseen = 0, begin = 1;
	gulong uid = 0, last_uid;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(folder->type == F_IMAP, NULL);
	g_return_val_if_fail(folder->account != NULL, NULL);

	session = imap_session_connect_if_not(folder);

	if (!session) {
		mlist = procmsg_read_cache(item, FALSE);
		item->last_num = procmsg_get_last_num_in_cache(mlist);
		procmsg_set_flags(mlist, item);
		statusbar_pop_all();
		return mlist;
	}

	last_uid = item->mtime;

	ok = imap_cmd_select(SESSION(session)->sock, item->path,
			     &exists, &recent, &unseen, &uid);
	if (ok != IMAP_SUCCESS) {
		log_warning(_("can't select folder: %s\n"), item->path);
		statusbar_pop_all();
		return NULL;
	}

	if (use_cache) {
		gint cache_last;

		mlist = procmsg_read_cache(item, FALSE);
		procmsg_set_flags(mlist, item);
		cache_last = procmsg_get_last_num_in_cache(mlist);

		/* calculating the range of envelope to get */
		if (exists < cache_last) {
			/* some messages are deleted (get all) */
			begin = 1;
		} else if (exists == cache_last) {
			if (last_uid != 0 && last_uid != uid) {
				/* some recent but deleted (get all) */
				begin = 1;
			} else {
				/* mailbox unchanged (get none)*/
				begin = -1;
			}
		} else {
			if (exists == cache_last + recent) {
				/* some recent */
				begin = cache_last + 1;
			} else {
				/* some recent but deleted (get all) */
				begin = 1;
			}
		}

		item->mtime = uid;
	}

	if (1 < begin && begin <= exists) {
		GSList *newlist;

		newlist = imap_get_uncached_messages
			(session, item, begin, exists);
		imap_delete_cached_messages(mlist, begin, INT_MAX);
		mlist = g_slist_concat(mlist, newlist);
	} else if (begin == 1) {
		if (begin <= exists)
			mlist = imap_get_uncached_messages
				(session, item, begin, exists);
		imap_delete_all_cached_messages(item);
	}

	item->last_num = exists;

	statusbar_pop_all();

	return mlist;
}

gchar *imap_fetch_msg(Folder *folder, FolderItem *item, gint num)
{
	gchar *path, *filename;
	IMAPSession *session;
	gint ok;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(item != NULL, NULL);

	session = imap_session_connect_if_not(folder);

	path = folder_item_get_path(item);
	filename = g_strconcat(path, G_DIR_SEPARATOR_S, itos(num), NULL);
	g_free(path);
 
	if (is_file_exist(filename)) {
		debug_print(_("message %d has been already cached.\n"), num);
		return filename;
	}

	if (!session) {
		g_free(filename);
		return NULL;
	}

	debug_print(_("getting message %d...\n"), num);
	ok = imap_cmd_fetch(SESSION(session)->sock, num, filename);

	statusbar_pop_all();

	if (ok != IMAP_SUCCESS) {
		g_warning(_("can't fetch message %d\n"), num);
		g_free(filename);
		return NULL;
	}

	return filename;
}

gint imap_add_msg(Folder *folder, FolderItem *dest, const gchar *file,
		  gboolean remove_source)
{
	IMAPSession *session;
	gint ok;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(file != NULL, -1);

	session = imap_session_connect_if_not(folder);
	if (!session)
		return -1;

	if (dest->last_num < 0) {
		imap_scan_folder(folder, dest);
		if (dest->last_num < 0) return -1;
	}

	ok = imap_cmd_append(SESSION(session)->sock, dest->path, file);
	if (ok != IMAP_SUCCESS) {
		g_warning(_("can't append message %s\n"), file);
		return -1;
	}

	if (remove_source) {
		if (unlink(file) < 0)
			FILE_OP_ERROR(file, "unlink");
	}

	return dest->last_num;
}

static gint imap_do_copy(Folder *folder, FolderItem *dest, MsgInfo *msginfo,
			 gboolean remove_source)
{
	gchar *destdir;
	IMAPSession *session;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(msginfo != NULL, -1);

	session = imap_session_connect_if_not(folder);
	if (!session) return -1;

	if (msginfo->folder == dest) {
		g_warning(_("the src folder is identical to the dest.\n"));
		return -1;
	}

	Xstrdup_a(destdir, dest->path, return -1);
	/* imap_path_subst_slash_to_dot(destdir); */

	debug_print(_("%s message %s%c%d to %s ...\n"),
		    remove_source ? "Moving" : "Copying",
		    msginfo->folder->path, G_DIR_SEPARATOR,
		    msginfo->msgnum, destdir);

	imap_cmd_copy(SESSION(session)->sock, msginfo->msgnum, destdir);

	if (remove_source) {
		imap_set_article_flags(session, msginfo->msgnum, msginfo->msgnum,
				       IMAP_FLAG_DELETED, TRUE);
		imap_cmd_expunge(SESSION(session)->sock);
	}

	return IMAP_SUCCESS;
}

static gint imap_do_copy_msgs_with_dest(Folder *folder, FolderItem *dest,
					GSList *msglist,
					gboolean remove_source)
{
	gchar *destdir;
	GSList *cur;
	MsgInfo *msginfo;
	IMAPSession *session;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(msglist != NULL, -1);

	session = imap_session_connect_if_not(folder);
	if (!session) return -1;

	Xstrdup_a(destdir, dest->path, return -1);
	/* imap_path_subst_slash_to_dot(destdir); */

	for (cur = msglist; cur != NULL; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;

		if (msginfo->folder == dest) {
			g_warning(_("the src folder is identical to the dest.\n"));
			continue;
		}

		debug_print(_("%s message %s%c%d to %s ...\n"),
			    remove_source ? "Moving" : "Copying",
			    msginfo->folder->path, G_DIR_SEPARATOR,
			    msginfo->msgnum, destdir);

		imap_cmd_copy(SESSION(session)->sock, msginfo->msgnum,
			      destdir);

		if (remove_source) {
			imap_set_article_flags
				(session, msginfo->msgnum, msginfo->msgnum,
				 IMAP_FLAG_DELETED, TRUE);
		}
	}

	if (remove_source)
		imap_cmd_expunge(SESSION(session)->sock);

	return IMAP_SUCCESS;

}

gint imap_move_msg(Folder *folder, FolderItem *dest, MsgInfo *msginfo)
{
	return imap_do_copy(folder, dest, msginfo, TRUE);
}

gint imap_move_msgs_with_dest(Folder *folder, FolderItem *dest,
			      GSList *msglist)
{
	return imap_do_copy_msgs_with_dest(folder, dest, msglist, TRUE);
}

gint imap_copy_msg(Folder *folder, FolderItem *dest, MsgInfo *msginfo)
{
	return imap_do_copy(folder, dest, msginfo, FALSE);
}

gint imap_copy_msgs_with_dest(Folder *folder, FolderItem *dest,
			      GSList *msglist)
{
	return imap_do_copy_msgs_with_dest(folder, dest, msglist, FALSE);
}

gint imap_remove_msg(Folder *folder, FolderItem *item, gint num)
{
	gint exists, recent, unseen;
	gulong uid;
	gint ok;
	IMAPSession *session;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(item != NULL, -1);
	g_return_val_if_fail(num > 0 && num <= item->last_num, -1);

	session = imap_session_connect_if_not(folder);
	if (!session) return -1;

	ok = imap_cmd_select(SESSION(session)->sock, item->path,
			     &exists, &recent, &unseen, &uid);
	if (ok != IMAP_SUCCESS) {
		log_warning(_("can't select folder: %s\n"), item->path);
		return ok;
	}

	ok = imap_set_article_flags(IMAP_SESSION(REMOTE_FOLDER(folder)->session),
				    num, num, IMAP_FLAG_DELETED, TRUE);
	if (ok != IMAP_SUCCESS) {
		log_warning(_("can't set deleted flags: %d\n"), num);
		return ok;
	}

	ok = imap_cmd_expunge(SESSION(session)->sock);
	if (ok != IMAP_SUCCESS) {
		log_warning(_("can't expunge\n"));
		return ok;
	}

	return IMAP_SUCCESS;
}

gint imap_remove_all_msg(Folder *folder, FolderItem *item)
{
	gint exists, recent, unseen;
	gulong uid;
	gint ok;
	IMAPSession *session;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(item != NULL, -1);

	session = imap_session_connect_if_not(folder);
	if (!session) return -1;

	ok = imap_cmd_select(SESSION(session)->sock, item->path,
			     &exists, &recent, &unseen, &uid);
	if (ok != IMAP_SUCCESS) {
		log_warning(_("can't select folder: %s\n"), item->path);
		return ok;
	}

	ok = imap_set_article_flags(session, 1, exists,
				    IMAP_FLAG_DELETED, TRUE);
	if (ok != IMAP_SUCCESS) {
		log_warning(_("can't set deleted flags: 1:%d\n"), exists);
		return ok;
	}

	ok = imap_cmd_expunge(SESSION(session)->sock);
	if (ok != IMAP_SUCCESS) {
		log_warning(_("can't expunge\n"));
		return ok;
	}

	return IMAP_SUCCESS;
}

void imap_scan_folder(Folder *folder, FolderItem *item)
{
}

FolderItem *imap_create_folder(Folder *folder, FolderItem *parent,
			       const gchar *name)
{
	gchar *dirpath, *imappath;
	IMAPSession *session;
	FolderItem *new_item;
	gint ok;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(parent != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);

	session = imap_session_connect_if_not(folder);
	if (!session) return NULL;

	if (parent->path)
		dirpath = g_strconcat(parent->path, G_DIR_SEPARATOR_S, name,
				      NULL);
	else
		dirpath = g_strdup(name);

	imappath = g_strdup(dirpath);
	/* imap_path_subst_slash_to_dot(imappath); */

	if (strcmp(name, "INBOX") != 0) {
		ok = imap_cmd_status(SESSION(session)->sock, imappath,
				     "MESSAGES");
		if (ok != IMAP_SUCCESS) {
			ok = imap_cmd_create(SESSION(session)->sock, imappath);
			statusbar_pop_all();
			if (ok != IMAP_SUCCESS) {
				log_warning(_("can't create mailbox\n"));
				g_free(imappath);
				g_free(dirpath);
				return NULL;
			}
		}
		statusbar_pop_all();
	}

	new_item = folder_item_new(name, dirpath);
	folder_item_append(parent, new_item);
	g_free(imappath);
	g_free(dirpath);

	return new_item;
}

gint imap_remove_folder(Folder *folder, FolderItem *item)
{
	gint ok;
	IMAPSession *session;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(item != NULL, -1);
	g_return_val_if_fail(item->path != NULL, -1);

	session = imap_session_connect_if_not(folder);
	if (!session) return -1;

	ok = imap_cmd_delete(SESSION(session)->sock, item->path);
	statusbar_pop_all();
	if (ok != IMAP_SUCCESS) {
		log_warning(_("can't delete mailbox\n"));
		return -1;
	}

	folder_item_remove(item);

	return 0;
}

static GSList *imap_get_uncached_messages(IMAPSession *session,
					  FolderItem *item,
					  gint first, gint last)
{
	gchar buf[IMAPBUFSIZE];
	GSList *newlist = NULL;
	GSList *llast = NULL;
	MsgInfo *msginfo;

	g_return_val_if_fail(session != NULL, NULL);
	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(item->folder != NULL, NULL);
	g_return_val_if_fail(item->folder->type == F_IMAP, NULL);
	g_return_val_if_fail(first <= last, NULL);

	if (imap_cmd_envelope(SESSION(session)->sock, first, last)
	    != IMAP_SUCCESS) {
		log_warning(_("can't get envelope\n"));
		return NULL;
	}

	for (;;) {
		if (sock_gets(SESSION(session)->sock, buf, sizeof(buf)) < 0) {
			log_warning(_("error occurred while getting envelope.\n"));
			return newlist;
		}
		strretchomp(buf);
		if (buf[0] != '*' || buf[1] != ' ') break;

		msginfo = imap_parse_envelope(SESSION(session)->sock, buf);
		if (!msginfo) {
			log_warning(_("can't parse envelope: %s\n"), buf);
			continue;
		}

		msginfo->folder = item;

		if (!newlist)
			llast = newlist = g_slist_append(newlist, msginfo);
		else {
			llast = g_slist_append(llast, msginfo);
			llast = llast->next;
		}
	}

	return newlist;
}

static GSList *imap_delete_cached_messages(GSList *mlist,
					   gint first, gint last)
{
	GSList *cur, *next;
	MsgInfo *msginfo;
	gchar *cache_file;

	for (cur = mlist; cur != NULL; ) {
		next = cur->next;

		msginfo = (MsgInfo *)cur->data;
		if (msginfo != NULL && first <= msginfo->msgnum &&
		    msginfo->msgnum <= last) {
			debug_print(_("deleting message %d...\n"),
				    msginfo->msgnum);

			cache_file = procmsg_get_message_file_path(msginfo);
			if (is_file_exist(cache_file)) unlink(cache_file);
			g_free(cache_file);

			procmsg_msginfo_free(msginfo);
			mlist = g_slist_remove(mlist, msginfo);
		}

		cur = next;
	}

	return mlist;
}

static void imap_delete_all_cached_messages(FolderItem *item)
{
	gchar *dir;

	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);
	g_return_if_fail(item->folder->type == F_IMAP);

	debug_print(_("\tDeleting all cached messages... "));

	dir = folder_item_get_path(item);
	remove_all_numbered_files(dir);
	g_free(dir);

	debug_print(_("done.\n"));
}

static SockInfo *imap_open(const gchar *server, gushort port, gchar *buf)
{
	SockInfo *sock;

	if ((sock = sock_connect(server, port)) == NULL) {
		log_warning(_("Can't connect to IMAP4 server: %s:%d\n"),
			    server, port);
		return NULL;
	}

	imap_cmd_count = 0;

	if (imap_cmd_noop(sock) != IMAP_SUCCESS) {
		sock_close(sock);
		return NULL;
	}

	return sock;
}

#define THROW goto catch

static void imap_parse_namespace(IMAPSession *session, Folder *folder)
{
	gchar *ns_str;
	gchar *name;
	gchar *separator;
	gchar *p;
	IMAPNameSpace *namespace;
	GList *ns_list = NULL;

	g_return_if_fail(session != NULL);
	g_return_if_fail(folder != NULL);
	g_return_if_fail(folder->type == F_IMAP);

	if (imap_cmd_namespace(SESSION(session)->sock, &ns_str)
	    != IMAP_SUCCESS) {
		log_warning(_("can't get namespace\n"));
		return;
	}

	/* get the first element */
	extract_one_parenthesis_with_skip_quote(ns_str, '"', '(', ')');
	g_strstrip(ns_str);
	p = ns_str;

	while (*p != '\0') {
		while (*p && *p != '(') p++;
		if (*p == '\0') THROW;
		p++;

		while (*p && *p != '"') p++;
		if (*p == '\0') THROW;
		p++;
		name = p;

		while (*p && *p != '"') p++;
		if (*p == '\0') THROW;
		*p = '\0';
		p++;

		while (*p && *p != '"') p++;
		if (*p == '\0') THROW;
		p++;
		separator = p;

		while (*p && *p != '"') p++;
		if (*p == '\0') THROW;
		*p = '\0';
		p++;

		while (*p && *p != ')') p++;
		if (*p == '\0') THROW;
		p++;

		namespace = g_new(IMAPNameSpace, 1);
		namespace->name = g_strdup(name);
		namespace->separator = separator[0];
		ns_list = g_list_append(ns_list, namespace);
		IMAP_FOLDER(folder)->namespace = ns_list;
	}

catch:
	g_free(ns_str);
	return;
}

#undef THROW

static gchar *imap_parse_atom(SockInfo *sock, gchar *src, gchar *dest,
			      gchar *orig_buf)
{
	gchar *cur_pos = src;

	while (*cur_pos == ' ') cur_pos++;

	if (!strncmp(cur_pos, "NIL", 3)) {
		*dest = '\0';
		cur_pos += 3;
	} else if (*cur_pos == '\"') {
		gchar *p;

		p = strchr_cpy(cur_pos + 1, '\"', dest, IMAPBUFSIZE);
		cur_pos = p ? p : cur_pos + 2;
	} else if (*cur_pos == '{') {
		gchar buf[32];
		gint len;

		cur_pos = strchr_cpy(cur_pos + 1, '}', buf, sizeof(buf));
		len = atoi(buf);

		g_return_val_if_fail(orig_buf != NULL, cur_pos);

		if (sock_gets(sock, orig_buf, IMAPBUFSIZE) < 0)
			return cur_pos;
		strretchomp(orig_buf);
		log_print("IMAP4< %s\n", orig_buf);
		memcpy(dest, orig_buf, len);
		dest[len] = '\0';
		cur_pos = orig_buf + len;
	}

	return cur_pos;
}

static gchar *imap_parse_one_address(SockInfo *sock, gchar *start,
				     gchar *out_from_str,
				     gchar *out_fromname_str,
				     gchar *orig_buf)
{
	gchar buf[IMAPBUFSIZE];
	gchar *cur_pos = start;

	cur_pos = imap_parse_atom(sock, cur_pos, buf, orig_buf);
	conv_unmime_header(out_fromname_str, 256, buf, NULL);

	if (out_fromname_str[0] != '\0') {
		strcat(out_from_str, "\"");
		strcat(out_from_str, out_fromname_str);
		strcat(out_from_str, "\"");
	}

	cur_pos = imap_parse_atom(sock, cur_pos, buf, orig_buf);

	strcat(out_from_str, " <");

	cur_pos = imap_parse_atom(sock, cur_pos, buf, orig_buf);
	strcat(out_from_str, buf);

	cur_pos = imap_parse_atom(sock, cur_pos, buf, orig_buf);
	strcat(out_from_str, "@");
	strcat(out_from_str, buf);
	strcat(out_from_str, ">");

	while (*cur_pos == ' ') cur_pos++;

	g_return_val_if_fail(*cur_pos == ')', cur_pos + 1);

	return cur_pos + 1;
}

static gchar *imap_parse_address(SockInfo *sock, gchar *start,
				 gchar **out_from_str,
				 gchar **out_fromname_str,
				 gchar *orig_buf)
{
	gchar buf[IMAPBUFSIZE];
	gchar name_buf[IMAPBUFSIZE];
	gchar *cur_pos = start;
	gboolean first = TRUE;

	if (out_from_str)     *out_from_str     = NULL;
	if (out_fromname_str) *out_fromname_str = NULL;
	buf[0] = name_buf[0] = '\0';

	if (!strncmp(cur_pos, "NIL", 3)) {
		if (out_from_str)     *out_from_str     = g_strdup("");
		if (out_fromname_str) *out_fromname_str = g_strdup("");
		return cur_pos + 3;
	}

	g_return_val_if_fail(*cur_pos == '(', NULL);
	cur_pos++;

	for (;;) {
		gchar ch = *cur_pos++;
		if (ch == ')') break;
		if (ch == '(') {
			if (!first) strcat(buf, ", ");
			first = FALSE;
			cur_pos = imap_parse_one_address
				(sock, cur_pos, buf, name_buf, orig_buf);
			if (!cur_pos) return NULL;
		}
	}

	if (out_from_str)     *out_from_str     = g_strdup(buf);
	if (out_fromname_str) *out_fromname_str = g_strdup(name_buf);

	return cur_pos;
}

static MsgFlags imap_parse_flags(const gchar *flag_str)  
{
	gchar buf[32];
	const gchar *cur_pos = flag_str;
	const gchar *last_pos;
	MsgFlags flags;

	flags = 0;
	MSG_SET_FLAGS(flags, MSG_UNREAD|MSG_IMAP);

	while (cur_pos != NULL) {
		cur_pos = strchr(cur_pos, '\\');
		if (cur_pos == NULL) break;

		last_pos = cur_pos + 1;
		cur_pos = strchr_cpy(last_pos, ' ', buf, sizeof(buf));
		if (cur_pos == NULL)
			strncpy2(buf, last_pos, sizeof(buf));

		if (g_strcasecmp(buf, "Recent") == 0) {
			MSG_SET_FLAGS(flags, MSG_NEW|MSG_UNREAD);
		} else if (g_strcasecmp(buf, "Seen") == 0) {
			MSG_UNSET_FLAGS(flags, MSG_NEW|MSG_UNREAD);
		} else if (g_strcasecmp(buf, "Deleted") == 0) {
			MSG_SET_FLAGS(flags, MSG_DELETED);
		} else if (g_strcasecmp(buf, "Flagged") == 0) {
			MSG_SET_FLAGS(flags, MSG_MARKED);
		}
	}

	return flags;
}

static MsgInfo *imap_parse_envelope(SockInfo *sock, gchar *line_str)
{
	MsgInfo *msginfo;
	gchar buf[IMAPBUFSIZE];
	gchar tmp[IMAPBUFSIZE];
	gchar *cur_pos;
	gint msgnum;
	size_t size;
	gchar *date = NULL;
	time_t date_t;
	gchar *subject = NULL;
	gchar *tmp_from;
	gchar *tmp_fromname;
	gchar *from = NULL;
	gchar *fromname = NULL;
	gchar *tmp_to;
	gchar *to = NULL;
	gchar *inreplyto = NULL;
	gchar *msgid = NULL;
	MsgFlags flags;

	log_print("IMAP4< %s\n", line_str);

	g_return_val_if_fail(line_str != NULL, NULL);
	g_return_val_if_fail(line_str[0] == '*' && line_str[1] == ' ', NULL);

	cur_pos = line_str + 2;

#define PARSE_ONE_ELEMENT(ch) \
{ \
	cur_pos = strchr_cpy(cur_pos, ch, buf, sizeof(buf)); \
	g_return_val_if_fail(cur_pos != NULL, NULL); \
}

	PARSE_ONE_ELEMENT(' ');
	msgnum = atoi(buf);

	PARSE_ONE_ELEMENT(' ');
	g_return_val_if_fail(!strcmp(buf, "FETCH"), NULL);

	PARSE_ONE_ELEMENT(' ');
	g_return_val_if_fail(!strcmp(buf, "(FLAGS"), NULL);

	PARSE_ONE_ELEMENT(')');
	g_return_val_if_fail(*buf == '(', NULL);
	flags = imap_parse_flags(buf + 1);

	g_return_val_if_fail(*cur_pos == ' ', NULL);
	g_return_val_if_fail
		((cur_pos = strchr_cpy(cur_pos + 1, ' ', buf, sizeof(buf))),
		 NULL);
	g_return_val_if_fail(!strcmp(buf, "RFC822.SIZE"), NULL);

	PARSE_ONE_ELEMENT(' ');
	size = atoi(buf);

	PARSE_ONE_ELEMENT(' ');
	g_return_val_if_fail(!strcmp(buf, "ENVELOPE"), NULL);

	g_return_val_if_fail(*cur_pos == '(', NULL);
	cur_pos = imap_parse_atom(sock, cur_pos + 1, buf, line_str);
	Xstrdup_a(date, buf, return NULL);
	date_t = procheader_date_parse(NULL, date, 0);

	cur_pos = imap_parse_atom(sock, cur_pos, buf, line_str);
	if (buf[0] != '\0') {
		conv_unmime_header(tmp, sizeof(tmp), buf, NULL);
		Xstrdup_a(subject, tmp, return NULL);
	}

	g_return_val_if_fail(*cur_pos == ' ', NULL);
	cur_pos = imap_parse_address(sock, cur_pos + 1,
				     &tmp_from, &tmp_fromname, line_str);
	Xstrdup_a(from, tmp_from,
		  {g_free(tmp_from); g_free(tmp_fromname); return NULL;});
	Xstrdup_a(fromname, tmp_fromname,
		  {g_free(tmp_from); g_free(tmp_fromname); return NULL;});
	g_free(tmp_from);
	g_free(tmp_fromname);

#define SKIP_ONE_ELEMENT() \
{ \
	g_return_val_if_fail(*cur_pos == ' ', NULL); \
	cur_pos = imap_parse_address(sock, cur_pos + 1, \
				     NULL, NULL, line_str); \
}

	/* skip sender and reply-to */
	SKIP_ONE_ELEMENT();
	SKIP_ONE_ELEMENT();

	g_return_val_if_fail(*cur_pos == ' ', NULL);
	cur_pos = imap_parse_address(sock, cur_pos + 1, &tmp_to, NULL, line_str);
	Xstrdup_a(to, tmp_to, {g_free(tmp_to); return NULL;});
	g_free(tmp_to);

	/* skip Cc and Bcc */
	SKIP_ONE_ELEMENT();
	SKIP_ONE_ELEMENT();

#undef SKIP_ONE_ELEMENT

	g_return_val_if_fail(*cur_pos == ' ', NULL);
	cur_pos = imap_parse_atom(sock, cur_pos, buf, line_str);
	if (buf[0] != '\0') {
		eliminate_parenthesis(buf, '(', ')');
		extract_parenthesis(buf, '<', '>');
		remove_space(buf);
		Xstrdup_a(inreplyto, buf, return NULL);
	}

	g_return_val_if_fail(*cur_pos == ' ', NULL);
	cur_pos = imap_parse_atom(sock, cur_pos, buf, line_str);
	if (buf[0] != '\0') {
		extract_parenthesis(buf, '<', '>');
		remove_space(buf);
		Xstrdup_a(msgid, buf, return NULL);
	}

	msginfo = g_new0(MsgInfo, 1);
	msginfo->msgnum = msgnum;
	msginfo->size = size;
	msginfo->date = g_strdup(date);
	msginfo->date_t = date_t;
	msginfo->subject = g_strdup(subject);
	msginfo->from = g_strdup(from);
	msginfo->fromname = g_strdup(fromname);
	msginfo->to = g_strdup(to);
	msginfo->inreplyto = g_strdup(inreplyto);
	msginfo->msgid = g_strdup(msgid);
	msginfo->flags = flags;

	return msginfo;
}

static gint imap_set_article_flags(IMAPSession *session,
				   gint first,
				   gint last,
				   IMAPFlags flags,
				   gboolean is_set)
{
	GString *buf;
	gint ok;

	buf = g_string_new(is_set ? "+FLAGS (" : "-FLAGS (");

	if (IMAP_IS_SEEN(flags))	g_string_append(buf, "\\Seen ");
	if (IMAP_IS_ANSWERED(flags))	g_string_append(buf, "\\Answered ");
	if (IMAP_IS_FLAGGED(flags))	g_string_append(buf, "\\Flagged ");
	if (IMAP_IS_DELETED(flags))	g_string_append(buf, "\\Deleted ");
	if (IMAP_IS_DRAFT(flags))	g_string_append(buf, "\\Draft");

	if (buf->str[buf->len - 1] == ' ')
		g_string_truncate(buf, buf->len - 1);

	g_string_append_c(buf, ')');

	ok = imap_cmd_store(SESSION(session)->sock, first, last, buf->str);
	g_string_free(buf, TRUE);

	return ok;
}


/* low-level IMAP4rev1 commands */

static gint imap_cmd_login(SockInfo *sock,
			   const gchar *user, const gchar *pass)
{
	gint ok;

	imap_cmd_gen_send(sock, "LOGIN \"%s\" %s", user, pass);
	ok = imap_cmd_ok(sock, NULL);
	if (ok != IMAP_SUCCESS)
		log_warning(_("IMAP4 login failed.\n"));

	return ok;
}

static gint imap_cmd_logout(SockInfo *sock)
{
	imap_cmd_gen_send(sock, "LOGOUT");
	return imap_cmd_ok(sock, NULL);
}

static gint imap_cmd_noop(SockInfo *sock)
{
	imap_cmd_gen_send(sock, "NOOP");
	return imap_cmd_ok(sock, NULL);
}

#define THROW(err) { ok = err; goto catch; }

static gint imap_cmd_namespace(SockInfo *sock, gchar **ns_str)
{
	gint ok;
	GPtrArray *argbuf;
	gchar *str;

	argbuf = g_ptr_array_new();

	imap_cmd_gen_send(sock, "NAMESPACE");
	if ((ok = imap_cmd_ok(sock, argbuf)) != IMAP_SUCCESS) THROW(ok);

	str = search_array_contain_str(argbuf, "NAMESPACE");
	if (!str) THROW(IMAP_ERROR);

	*ns_str = g_strdup(str);

catch:
	ptr_array_free_strings(argbuf);
	g_ptr_array_free(argbuf, TRUE);

	return ok;
}

#undef THROW

static gint imap_cmd_select(SockInfo *sock, const gchar *folder,
			    gint *exists, gint *recent, gint *unseen,
			    gulong *uid)
{
	gint ok;
	gchar *resp_str;
	GPtrArray *argbuf;
	gchar *imappath;

	*exists = *recent = *unseen = *uid = 0;
	argbuf = g_ptr_array_new();

	Xstrdup_a(imappath, folder, return -1);
	/* imap_path_subst_slash_to_dot(imappath); */

	imap_cmd_gen_send(sock, "SELECT \"%s\"", imappath);
	if ((ok = imap_cmd_ok(sock, argbuf)) != IMAP_SUCCESS)
		goto bail;

	resp_str = search_array_contain_str(argbuf, "EXISTS");
	if (resp_str) {
		if (sscanf(resp_str,"%d EXISTS", exists) != 1) {
			g_warning("imap_select(): invalid EXISTS line.\n");
			goto bail;
		}
	}

	resp_str = search_array_contain_str(argbuf, "RECENT");
	if (resp_str) {
		if (sscanf(resp_str, "%d RECENT", recent) != 1) {
			g_warning("imap_select(): invalid RECENT line.\n");
			goto bail;
		}
	}

	resp_str = search_array_contain_str(argbuf, "UIDVALIDITY");
	if (resp_str) {
		if (sscanf(resp_str, "OK [UIDVALIDITY %lu] ", uid) != 1) {
			g_warning("imap_select(): invalid UIDVALIDITY line.\n");
			goto bail;
		}
	}

	resp_str = search_array_contain_str(argbuf, "UNSEEN");
	if (resp_str) {
		if (sscanf(resp_str, "OK [UNSEEN %d] ", unseen) != 1) {
			g_warning("imap_select(): invalid UNSEEN line.\n");
			goto bail;
		}
	}

bail:
	ptr_array_free_strings(argbuf);
	g_ptr_array_free(argbuf, TRUE);

	return ok;
}

static gint imap_cmd_status(SockInfo *sock, const gchar *folder,
			    const gchar *status)
{
	imap_cmd_gen_send(sock, "STATUS \"%s\" (%s)", folder, status);
	return imap_cmd_ok(sock, NULL);
}

static gint imap_cmd_create(SockInfo *sock, const gchar *folder)
{
	imap_cmd_gen_send(sock, "CREATE \"%s\"", folder);
	return imap_cmd_ok(sock, NULL);
}

static gint imap_cmd_delete(SockInfo *sock, const gchar *folder)
{
	imap_cmd_gen_send(sock, "DELETE \"%s\"", folder);
	return imap_cmd_ok(sock, NULL);
}

static gint imap_cmd_fetch(SockInfo *sock, gint num, const gchar *filename)
{
	gint ok;
	gchar buf[IMAPBUFSIZE];
	gchar *cur_pos;
	gchar size_str[32];
	glong size_num;

	g_return_val_if_fail(filename != NULL, IMAP_ERROR);

	imap_cmd_gen_send(sock, "FETCH %d BODY[]", num);

	if (sock_gets(sock, buf, sizeof(buf)) < 0)
		return IMAP_ERROR;
	strretchomp(buf);
	if (buf[0] != '*' || buf[1] != ' ')
		return IMAP_ERROR;
	log_print("IMAP4< %s\n", buf);

	cur_pos = strchr(buf, '{');
	g_return_val_if_fail(cur_pos != NULL, IMAP_ERROR);
	cur_pos = strchr_cpy(cur_pos + 1, '}', size_str, sizeof(size_str));
	g_return_val_if_fail(cur_pos != NULL, IMAP_ERROR);
	size_num = atol(size_str);

	if (*cur_pos != '\0') return IMAP_ERROR;

	if (recv_bytes_write_to_file(sock, size_num, filename) != 0)
		return IMAP_ERROR;

	if (imap_cmd_gen_recv(sock, buf, sizeof(buf)) != IMAP_SUCCESS)
		return IMAP_ERROR;

	if (buf[0] != ')')
		return IMAP_ERROR;

	ok = imap_cmd_ok(sock, NULL);

	return ok;
}

static gint imap_cmd_append(SockInfo *sock, const gchar *destfolder,
			    const gchar *file)
{
	gint ok;
	gint size;

	g_return_val_if_fail(file != NULL, IMAP_ERROR);

	size = get_file_size(file);
	imap_cmd_gen_send(sock, "APPEND %s {%d}", destfolder, size);
	ok = imap_cmd_ok(sock, NULL);
	if (ok != IMAP_SUCCESS) {
		log_warning(_("can't append %s to %s\n"), file, destfolder);
		return -1;
	}

	return ok;
}

static gint imap_cmd_copy(SockInfo *sock, gint num, const gchar *destfolder)
{
	gint ok;

	g_return_val_if_fail(destfolder != NULL, IMAP_ERROR);

	imap_cmd_gen_send(sock, "COPY %d %s", num, destfolder);
	ok = imap_cmd_ok(sock, NULL);
	if (ok != IMAP_SUCCESS) {
		log_warning(_("can't copy %d to %s\n"), num, destfolder);
		return -1;
	}

	return ok;
}


gint imap_cmd_envelope(SockInfo *sock, gint first, gint last)
{
	imap_cmd_gen_send(sock, "FETCH %d:%d (FLAGS RFC822.SIZE ENVELOPE)",
			  first, last);

	return IMAP_SUCCESS;
}

static gint imap_cmd_store(SockInfo *sock, gint first, gint last,
			   gchar *sub_cmd)
{
	gint ok;

	imap_cmd_gen_send(sock, "STORE %d:%d %s", first, last, sub_cmd);

	if ((ok = imap_cmd_ok(sock, NULL)) != IMAP_SUCCESS) {
		log_warning(_("error while imap command: STORE %d:%d %s\n"),
			    first, last, sub_cmd);
		return ok;
	}

	return IMAP_SUCCESS;
}

static gint imap_cmd_expunge(SockInfo *sock)
{
	gint ok;

	imap_cmd_gen_send(sock, "EXPUNGE");
	if ((ok = imap_cmd_ok(sock, NULL)) != IMAP_SUCCESS) {
		log_warning(_("error while imap command: EXPUNGE\n"));
		return ok;
	}

	return IMAP_SUCCESS;
}

static gint imap_cmd_ok(SockInfo *sock, GPtrArray *argbuf)
{
	gint ok;
	gchar buf[IMAPBUFSIZE];
	gint cmd_num;
	gchar cmd_status[IMAPBUFSIZE];

	while ((ok = imap_cmd_gen_recv(sock, buf, sizeof(buf)))
	       == IMAP_SUCCESS) {
		if (buf[0] == '*' && buf[1] == ' ') {
			if (argbuf)
				g_ptr_array_add(argbuf, g_strdup(&buf[2]));
			continue;
		}

		if (sscanf(buf, "%d %s", &cmd_num, cmd_status) < 2)
			return IMAP_ERROR;
		else if (cmd_num == imap_cmd_count &&
			 !strcmp(cmd_status, "OK")) {
			if (argbuf)
				g_ptr_array_add(argbuf, g_strdup(buf));
			return IMAP_SUCCESS;
		} else
			return IMAP_ERROR;
	}

	return ok;
}

static void imap_cmd_gen_send(SockInfo *sock, const gchar *format, ...)
{
	gchar buf[IMAPBUFSIZE];
	gchar tmp[IMAPBUFSIZE];
	gchar *p;
	va_list args;

	va_start(args, format);
	g_vsnprintf(tmp, sizeof(tmp), format, args);
	va_end(args);

	imap_cmd_count++;

	g_snprintf(buf, sizeof(buf), "%d %s\r\n", imap_cmd_count, tmp);
	if (!strncasecmp(tmp, "LOGIN ", 6) && (p = strchr(tmp + 6, ' '))) {
		*(p + 1) = '\0';
		log_print("IMAP4> %d %s ********\n", imap_cmd_count, tmp);
	} else
		log_print("IMAP4> %d %s\n", imap_cmd_count, tmp);

	sock_write(sock, buf, strlen(buf));
}

static gint imap_cmd_gen_recv(SockInfo *sock, gchar *buf, gint size)
{
	if (sock_gets(sock, buf, size) == -1)
		return IMAP_SOCKET;

	strretchomp(buf);

	log_print("IMAP4< %s\n", buf);

	return IMAP_SUCCESS;
}


/* misc utility functions */

static gchar *strchr_cpy(const gchar *src, gchar ch, gchar *dest, gint len)
{
	gchar *tmp;

	dest[0] = '\0';
	tmp = strchr(src, ch);
	if (!tmp || tmp == src)
		return NULL;

	memcpy(dest, src, MIN(tmp - src, len - 1));
	dest[MIN(tmp - src, len - 1)] = '\0';

	return tmp + 1;
}

static gchar *search_array_contain_str(GPtrArray *array, gchar *str)
{
	gint i;

	for (i = 0; i < array->len; i++) {
		gchar *tmp;

		tmp = g_ptr_array_index(array, i);
		if (strstr(tmp, str) != NULL)
			return tmp;
	}

	return NULL;
}

static void imap_path_subst_slash_to_dot(gchar *str)
{
	subst_char(str, '/', '.');
}
