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
#include <ctype.h>

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
#include "inputdialog.h"

#define IMAP4_PORT	143

static GList *session_list = NULL;

static gint imap_cmd_count = 0;

static IMAPSession *imap_session_get	(Folder		*folder);
static gchar *imap_query_password	(const gchar	*server,
					 const gchar	*user);

static void imap_scan_tree_recursive	(IMAPSession	*session,
					 FolderItem	*item,
					 IMAPNameSpace	*namespace);
static GSList *imap_parse_list		(IMAPSession	*session,
					 const gchar	*path);

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
						 guint32	 first_uid,
						 guint32	 last_uid);
static GSList *imap_delete_cached_messages	(GSList		*mlist,
						 FolderItem	*item,
						 guint32	 first_uid,
						 guint32	 last_uid);
static void imap_delete_all_cached_messages	(FolderItem	*item);

static SockInfo *imap_open		(const gchar	*server,
					 gushort	 port,
					 gchar		*buf);

static gint imap_set_message_flags	(IMAPSession	*session,
					 guint32	 first_uid,
					 guint32	 last_uid,
					 IMAPFlags	 flag,
					 gboolean	 is_set);
static gint imap_select			(IMAPSession	*session,
					 IMAPFolder	*folder,
					 const gchar	*path,
					 gint		*exists,
					 gint		*recent,
					 gint		*unseen,
					 guint32	*uid_validity);
static gint imap_get_uid		(IMAPSession	*session,
					 gint		 msgnum,
					 guint32	*uid);
#if 0
static gint imap_status_uidnext		(IMAPSession	*session,
					 IMAPFolder	*folder,
					 const gchar	*path,
					 guint32	*uid_next);
#endif

static void imap_parse_namespace		(IMAPSession	*session,
						 IMAPFolder	*folder);
static IMAPNameSpace *imap_find_namespace	(IMAPFolder	*folder,
						 const gchar	*path);
static gchar *imap_get_real_path		(IMAPFolder	*folder,
						 const gchar	*path);

static gchar *imap_parse_atom		(SockInfo	*sock,
					 gchar		*src,
					 gchar		*dest,
					 gint		 dest_len,
					 GString	*str);
static gchar *imap_parse_one_address	(SockInfo	*sock,
					 gchar		*start,
					 gchar		*out_from_str,
					 gchar		*out_fromname_str,
					 GString	*str);
static gchar *imap_parse_address	(SockInfo	*sock,
					 gchar		*start,
					 gchar	       **out_from_str,
					 gchar	       **out_fromname_str,
					 GString	*str);
static MsgFlags imap_parse_flags	(const gchar	*flag_str);
static MsgInfo *imap_parse_envelope	(SockInfo	*sock,
					 GString	*line_str);

/* low-level IMAP4rev1 commands */
static gint imap_cmd_login	(SockInfo	*sock,
				 const gchar	*user,
				 const gchar	*pass);
static gint imap_cmd_logout	(SockInfo	*sock);
static gint imap_cmd_noop	(SockInfo	*sock);
static gint imap_cmd_namespace	(SockInfo	*sock,
				 gchar	       **ns_str);
static gint imap_cmd_list	(SockInfo	*sock,
				 const gchar	*ref,
				 const gchar	*mailbox,
				 GPtrArray	*argbuf);
static gint imap_cmd_select	(SockInfo	*sock,
				 const gchar	*folder,
				 gint		*exists,
				 gint		*recent,
				 gint		*unseen,
				 guint32	*uid_validity);
#if 0
static gint imap_cmd_status	(SockInfo	*sock,
				 const gchar	*folder,
				 const gchar	*status,
				 gint		*value);
#endif
static gint imap_cmd_create	(SockInfo	*sock,
				 const gchar	*folder);
static gint imap_cmd_delete	(SockInfo	*sock,
				 const gchar	*folder);
static gint imap_cmd_envelope	(SockInfo	*sock,
				 guint32	 first_uid,
				 guint32	 last_uid);
#if 0
static gint imap_cmd_search	(SockInfo	*sock,
				 GSList		*numlist);
#endif
static gint imap_cmd_fetch	(SockInfo	*sock,
				 guint32	 uid,
				 const gchar	*filename);
static gint imap_cmd_append	(SockInfo	*sock,
				 const gchar	*destfolder,
				 const gchar	*file);
static gint imap_cmd_copy	(SockInfo	*sock,
				 guint32	 uid,
				 const gchar	*destfolder);
static gint imap_cmd_store	(SockInfo	*sock,
				 guint32	 first_uid,
				 guint32	 last_uid,
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
static gchar *strchr_cpy			(const gchar	*src,
						 gchar		 ch,
						 gchar		*dest,
						 gint		 len);
static gchar *search_array_contain_str		(GPtrArray	*array,
						 gchar		*str);
static void imap_path_separator_subst		(gchar		*str,
						 gchar		 separator);

static IMAPSession *imap_session_get(Folder *folder)
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
		if (rfolder->session)
			imap_parse_namespace(IMAP_SESSION(rfolder->session),
					     IMAP_FOLDER(folder));
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
		if (rfolder->session)
			imap_parse_namespace(IMAP_SESSION(rfolder->session),
					     IMAP_FOLDER(folder));
	}

	statusbar_pop_all();
	return IMAP_SESSION(rfolder->session);
}

static gchar *imap_query_password(const gchar *server, const gchar *user)
{
	gchar *message;
	gchar *pass;

	message = g_strdup_printf(_("Input password for %s on %s:"),
				  user, server);
	pass = input_dialog_with_invisible(_("Input password"), message, NULL);
	g_free(message);

	return pass;
}

Session *imap_session_new(const gchar *server, gushort port,
			  const gchar *user, const gchar *pass)
{
	gchar buf[IMAPBUFSIZE];
	IMAPSession *session;
	SockInfo *imap_sock;

	g_return_val_if_fail(server != NULL, NULL);
	g_return_val_if_fail(user != NULL, NULL);

	if (!pass) {
		gchar *tmp_pass;
		tmp_pass = imap_query_password(server, user);
		if (!tmp_pass)
			return NULL;
		Xstrdup_a(pass, tmp_pass, {g_free(tmp_pass); return NULL;});
		g_free(tmp_pass);
	}

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

#define THROW goto catch

GSList *imap_get_msg_list(Folder *folder, FolderItem *item, gboolean use_cache)
{
	GSList *mlist = NULL;
	IMAPSession *session;
	gint ok, exists = 0, recent = 0, unseen = 0;
	guint32 uid_validity = 0;
	guint32 first_uid = 0, last_uid = 0, begin;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(folder->type == F_IMAP, NULL);
	g_return_val_if_fail(folder->account != NULL, NULL);

	session = imap_session_get(folder);

	if (!session) {
		mlist = procmsg_read_cache(item, FALSE);
		item->last_num = procmsg_get_last_num_in_cache(mlist);
		procmsg_set_flags(mlist, item);
		statusbar_pop_all();
		return mlist;
	}

	ok = imap_select(session, IMAP_FOLDER(folder), item->path,
			 &exists, &recent, &unseen, &uid_validity);
	if (ok != IMAP_SUCCESS) THROW;
	if (exists > 0) {
		ok = imap_get_uid(session, 1, &first_uid);
		if (ok != IMAP_SUCCESS) THROW;
		if (1 != exists) {
			ok = imap_get_uid(session, exists, &last_uid);
			if (ok != IMAP_SUCCESS) THROW;
		} else
			last_uid = first_uid;
	} else {
		imap_delete_all_cached_messages(item);
		statusbar_pop_all();
		return NULL;
	}

	if (use_cache) {
		guint32 cache_last;

		mlist = procmsg_read_cache(item, FALSE);
		procmsg_set_flags(mlist, item);
		cache_last = procmsg_get_last_num_in_cache(mlist);

		/* calculating the range of envelope to get */
		if (item->mtime != uid_validity) {
			/* mailbox is changed (get all) */
			begin = first_uid;
		} else if (last_uid < cache_last) {
			/* mailbox is changed (get all) */
			begin = first_uid;
		} else if (last_uid == cache_last) {
			/* mailbox unchanged (get none)*/
			begin = 0;
		} else {
			begin = cache_last + 1;
		}

		item->mtime = uid_validity;

		if (first_uid > 0 && last_uid > 0) {
			mlist = imap_delete_cached_messages(mlist, item,
							    0, first_uid - 1);
			mlist = imap_delete_cached_messages(mlist, item,
							    last_uid + 1,
							    UINT_MAX);
		}
		if (begin > 0)
			mlist = imap_delete_cached_messages(mlist, item,
							    begin, UINT_MAX);
	} else {
		imap_delete_all_cached_messages(item);
		begin = first_uid;
	}

	if (begin > 0 && begin <= last_uid) {
		GSList *newlist;
		newlist = imap_get_uncached_messages(session, item,
						     begin, last_uid);
		mlist = g_slist_concat(mlist, newlist);
	}

	item->last_num = last_uid;

catch:
	statusbar_pop_all();
	return mlist;
}

#undef THROW

gchar *imap_fetch_msg(Folder *folder, FolderItem *item, gint uid)
{
	gchar *path, *filename;
	IMAPSession *session;
	gint ok;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(item != NULL, NULL);

	path = folder_item_get_path(item);
	filename = g_strconcat(path, G_DIR_SEPARATOR_S, itos(uid), NULL);
	g_free(path);
 
	if (is_file_exist(filename)) {
		debug_print(_("message %d has been already cached.\n"), uid);
		return filename;
	}

	session = imap_session_get(folder);
	if (!session) {
		g_free(filename);
		return NULL;
	}

	debug_print(_("getting message %d...\n"), uid);
	ok = imap_cmd_fetch(SESSION(session)->sock, (guint32)uid, filename);

	statusbar_pop_all();

	if (ok != IMAP_SUCCESS) {
		g_warning(_("can't fetch message %d\n"), uid);
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

	session = imap_session_get(folder);
	if (!session)
		return -1;

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
	gint ok;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(folder->type == F_IMAP, -1);
	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(msginfo != NULL, -1);

	session = imap_session_get(folder);
	if (!session) return -1;

	if (msginfo->folder == dest) {
		g_warning(_("the src folder is identical to the dest.\n"));
		return -1;
	}

	destdir = imap_get_real_path(IMAP_FOLDER(folder), dest->path);

	if (remove_source)
		debug_print(_("Moving message %s%c%d to %s ...\n"),
			    msginfo->folder->path, G_DIR_SEPARATOR,
			    msginfo->msgnum, destdir);
	else
		debug_print(_("Copying message %s%c%d to %s ...\n"),
			    msginfo->folder->path, G_DIR_SEPARATOR,
			    msginfo->msgnum, destdir);

	ok = imap_cmd_copy(SESSION(session)->sock, msginfo->msgnum, destdir);

	if (ok == IMAP_SUCCESS && remove_source) {
		imap_set_message_flags(session, msginfo->msgnum, msginfo->msgnum,
				       IMAP_FLAG_DELETED, TRUE);
		ok = imap_cmd_expunge(SESSION(session)->sock);
	}

	g_free(destdir);
	statusbar_pop_all();

	return ok;
}

static gint imap_do_copy_msgs_with_dest(Folder *folder, FolderItem *dest, 
					GSList *msglist,
					gboolean remove_source)
{
	gchar *destdir;
	GSList *cur;
	MsgInfo *msginfo;
	IMAPSession *session;
	gint ok;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(msglist != NULL, -1);

	session = imap_session_get(folder);
	if (!session) return -1;

	destdir = imap_get_real_path(IMAP_FOLDER(folder), dest->path);

	for (cur = msglist; cur != NULL; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;

		if (msginfo->folder == dest) {
			g_warning(_("the src folder is identical to the dest.\n"));
			continue;
		}

		if (remove_source)
			debug_print(_("Moving message %s%c%d to %s ...\n"),
				    msginfo->folder->path, G_DIR_SEPARATOR,
				    msginfo->msgnum, destdir);
		else
			debug_print(_("Copying message %s%c%d to %s ...\n"),
				    msginfo->folder->path, G_DIR_SEPARATOR,
				    msginfo->msgnum, destdir);

		ok = imap_cmd_copy(SESSION(session)->sock, msginfo->msgnum,
				   destdir);

		if (ok == IMAP_SUCCESS && remove_source) {
			imap_set_message_flags
				(session, msginfo->msgnum, msginfo->msgnum,
				 IMAP_FLAG_DELETED, TRUE);
		}
	}

	if (remove_source)
		ok = imap_cmd_expunge(SESSION(session)->sock);

	g_free(destdir);
	statusbar_pop_all();

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

gint imap_remove_msg(Folder *folder, FolderItem *item, gint uid)
{
	gint exists, recent, unseen;
	guint32 uid_validity;
	gint ok;
	IMAPSession *session;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(folder->type == F_IMAP, -1);
	g_return_val_if_fail(item != NULL, -1);

	session = imap_session_get(folder);
	if (!session) return -1;

	ok = imap_select(session, IMAP_FOLDER(folder), item->path,
			 &exists, &recent, &unseen, &uid_validity);
	statusbar_pop_all();
	if (ok != IMAP_SUCCESS)
		return ok;

	ok = imap_set_message_flags
		(IMAP_SESSION(REMOTE_FOLDER(folder)->session),
		 (guint32)uid, (guint32)uid, IMAP_FLAG_DELETED, TRUE);
	statusbar_pop_all();
	if (ok != IMAP_SUCCESS) {
		log_warning(_("can't set deleted flags: %d\n"), uid);
		return ok;
	}

	ok = imap_cmd_expunge(SESSION(session)->sock);
	statusbar_pop_all();
	if (ok != IMAP_SUCCESS) {
		log_warning(_("can't expunge\n"));
		return ok;
	}

	return IMAP_SUCCESS;
}

gint imap_remove_all_msg(Folder *folder, FolderItem *item)
{
	gint exists, recent, unseen;
	guint32 uid_validity;
	gint ok;
	IMAPSession *session;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(item != NULL, -1);

	session = imap_session_get(folder);
	if (!session) return -1;

	ok = imap_select(session, IMAP_FOLDER(folder), item->path,
			 &exists, &recent, &unseen, &uid_validity);
	statusbar_pop_all();
	if (ok != IMAP_SUCCESS)
		return ok;
	if (exists == 0)
		return IMAP_SUCCESS;

	imap_cmd_gen_send(SESSION(session)->sock,
			  "STORE 1:%d +FLAGS (\\Deleted)", exists);
	ok = imap_cmd_ok(SESSION(session)->sock, NULL);
	statusbar_pop_all();
	if (ok != IMAP_SUCCESS) {
		log_warning(_("can't set deleted flags: 1:%d\n"), exists);
		return ok;
	}

	ok = imap_cmd_expunge(SESSION(session)->sock);
	statusbar_pop_all();
	if (ok != IMAP_SUCCESS) {
		log_warning(_("can't expunge\n"));
		return ok;
	}

	return IMAP_SUCCESS;
}

void imap_scan_folder(Folder *folder, FolderItem *item)
{
}

void imap_scan_tree(Folder *folder)
{
	IMAPFolder *imapfolder = IMAP_FOLDER(folder);
	FolderItem *item, *inbox;
	IMAPSession *session;
	IMAPNameSpace *namespace = NULL;
	gchar *root_folder = NULL;

	g_return_if_fail(folder != NULL);
	g_return_if_fail(folder->account != NULL);

	session = imap_session_get(folder);
	if (!session) return;

	if (imapfolder->namespace && imapfolder->namespace->data)
		namespace = (IMAPNameSpace *)imapfolder->namespace->data;

	if (folder->account->imap_dir && *folder->account->imap_dir) {
		gchar *imap_dir;
		Xstrdup_a(imap_dir, folder->account->imap_dir, return);
		strtailchomp(imap_dir, '/');
		root_folder = g_strconcat
			(namespace && namespace->name ? namespace->name : "",
			 imap_dir, NULL);
		if (namespace && namespace->separator)
			subst_char(root_folder, namespace->separator, '/');
	}

	if (root_folder)
		debug_print("IMAP root directory: %s\n", root_folder);

	folder_tree_destroy(folder);
	item = folder_item_new(folder->name, root_folder);
	item->folder = folder;
	folder->node = g_node_new(item);
	g_free(root_folder);

	inbox = folder_item_new("INBOX", "INBOX");
	inbox->stype = F_INBOX;
	folder_item_append(item, inbox);
	folder->inbox = inbox;

	imap_scan_tree_recursive(session, item, namespace);
}

static void imap_scan_tree_recursive(IMAPSession *session,
				     FolderItem *item,
				     IMAPNameSpace *namespace)
{
	IMAPFolder *imapfolder;
	FolderItem *new_item;
	GSList *item_list, *cur;
	gchar *real_path;

	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);
	g_return_if_fail(item->no_sub == FALSE);

	imapfolder = IMAP_FOLDER(item->folder);

	if (item->folder->ui_func)
		item->folder->ui_func(item->folder, item,
				      item->folder->ui_func_data);

	if (item->path) {
		real_path = imap_get_real_path(imapfolder, item->path);
		imap_cmd_gen_send(SESSION(session)->sock, "LIST \"\" %s%c%%",
				  real_path,
				  namespace && namespace->separator
				  ? namespace->separator : '/');
	} else {
		real_path = g_strdup(namespace && namespace->name
				     ? namespace->name : "");
		imap_cmd_gen_send(SESSION(session)->sock, "LIST \"\" %s%%",
				  real_path);
	}

	strtailchomp(real_path, namespace && namespace->separator
		     ? namespace->separator : '/');

	item_list = imap_parse_list(session, real_path);
	for (cur = item_list; cur != NULL; cur = cur->next) {
		new_item = cur->data;
		folder_item_append(item, new_item);
		if (new_item->no_sub == FALSE)
			imap_scan_tree_recursive(session, new_item, namespace);
	}
}

static GSList *imap_parse_list(IMAPSession *session, const gchar *path)
{
	gchar buf[IMAPBUFSIZE];
	gchar flags[256];
	gchar separator[16];
	gchar *p;
	gchar *name;
	GSList *item_list = NULL;
	GString *str;
	FolderItem *new_item;

	debug_print("getting list of %s ...\n", *path ? path : "\"\"");

	str = g_string_new(NULL);

	for (;;) {
		if (sock_gets(SESSION(session)->sock, buf, sizeof(buf)) <= 0) {
			log_warning(_("error occured while getting LIST.\n"));
			break;
		}
		strretchomp(buf);
		if (buf[0] != '*' || buf[1] != ' ') {
			log_print("IMAP4< %s\n", buf);
			break;
		}
		debug_print("IMAP4< %s\n", buf);

		g_string_assign(str, buf);
		p = str->str + 2;
		if (strncmp(p, "LIST ", 5) != 0) continue;
		p += 5;

		if (*p != '(') continue;
		p++;
		p = strchr_cpy(p, ')', flags, sizeof(flags));
		if (!p) continue;
		while (*p == ' ') p++;

		p = strchr_cpy(p, ' ', separator, sizeof(separator));
		if (!p) continue;
		extract_quote(separator, '"');
		if (!strcmp(separator, "NIL"))
			separator[0] = '\0';

		buf[0] = '\0';
		while (*p == ' ') p++;
		if (*p == '{' || *p == '"')
			p = imap_parse_atom(SESSION(session)->sock, p,
					    buf, sizeof(buf), str);
		else
			strncpy2(buf, p, sizeof(buf));
		strtailchomp(buf, separator[0]);
		if (buf[0] == '\0') continue;
		if (!strcmp(buf, path)) continue;
		if (!strcmp(buf, "INBOX")) continue;

		if (separator[0] != '\0')
			subst_char(buf, separator[0], '/');
		name = g_basename(buf);
		if (name[0] == '.') continue;

		new_item = folder_item_new(name, buf);
		if (strcasestr(flags, "\\Noinferiors") != NULL)
			new_item->no_sub = TRUE;
		if (strcasestr(flags, "\\Noselect") != NULL)
			new_item->no_select = TRUE;

		item_list = g_slist_append(item_list, new_item);

		debug_print("folder %s has been added.\n", buf);
	}

	g_string_free(str, TRUE);
	statusbar_pop_all();

	return item_list;
}

gint imap_create_tree(Folder *folder)
{
	IMAPFolder *imapfolder = IMAP_FOLDER(folder);
	FolderItem *item;
	FolderItem *new_item;
	gchar *trash_path;
	gchar *imap_dir = "";

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(folder->node != NULL, -1);
	g_return_val_if_fail(folder->node->data != NULL, -1);
	g_return_val_if_fail(folder->account != NULL, -1);

	imap_session_get(folder);

	item = FOLDER_ITEM(folder->node->data);

	new_item = folder_item_new("INBOX", "INBOX");
	new_item->stype = F_INBOX;
	folder_item_append(item, new_item);
	folder->inbox = new_item;

	if (folder->account->imap_dir && *folder->account->imap_dir) {
		gchar *tmpdir;

		Xstrdup_a(tmpdir, folder->account->imap_dir, return -1);
		strtailchomp(tmpdir, '/');
		Xalloca(imap_dir, strlen(tmpdir) + 2, return -1);
		g_snprintf(imap_dir, strlen(tmpdir) + 2, "%s%c", tmpdir, '/');
	}

	if (imapfolder->namespace && imapfolder->namespace->data) {
		IMAPNameSpace *namespace =
			(IMAPNameSpace *)imapfolder->namespace->data;

		if (*namespace->name != '\0') {
			gchar *name;

			Xstrdup_a(name, namespace->name, return -1);
			subst_char(name, namespace->separator, '/');
			trash_path = g_strconcat(name, imap_dir, "trash", NULL);
		} else
			trash_path = g_strconcat(imap_dir, "trash", NULL);
	} else
		trash_path = g_strconcat(imap_dir, "trash", NULL);

	new_item = imap_create_folder(folder, item, trash_path);

	if (!new_item) {
		gchar *path;

		new_item = folder_item_new("Trash", trash_path);
		folder_item_append(item, new_item);

		path = folder_item_get_path(new_item);
		if (!is_dir_exist(path))
			make_dir_hier(path);
		g_free(path);
	} else {
		g_free(new_item->name);
		new_item->name = g_strdup("Trash");
	}
	new_item->stype = F_TRASH;
	folder->trash = new_item;

	g_free(trash_path);
	return 0;
}

FolderItem *imap_create_folder(Folder *folder, FolderItem *parent,
			       const gchar *name)
{
	gchar *dirpath, *imappath;
	IMAPSession *session;
	IMAPNameSpace *namespace;
	FolderItem *new_item;
	gchar *new_name;
	const gchar *p;
	gint ok;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(folder->account != NULL, NULL);
	g_return_val_if_fail(parent != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);

	session = imap_session_get(folder);
	if (!session) return NULL;

	if (parent->path)
		dirpath = g_strconcat(parent->path, "/", name, NULL);
	else if ((p = strchr(name, '/')) != NULL && *(p + 1) != '\0')
		dirpath = g_strdup(name);
	else if (folder->account->imap_dir && *folder->account->imap_dir) {
		gchar *imap_dir;

		Xstrdup_a(imap_dir, folder->account->imap_dir, return NULL);
		strtailchomp(imap_dir, '/');
		dirpath = g_strconcat(imap_dir, "/", name, NULL);
	} else
		dirpath = g_strdup(name);

	Xstrdup_a(imappath, dirpath, {g_free(dirpath); return NULL;});
	Xstrdup_a(new_name, name, {g_free(dirpath); return NULL;});
	namespace = imap_find_namespace(IMAP_FOLDER(folder), imappath);
	if (namespace && namespace->separator) {
		imap_path_separator_subst(imappath, namespace->separator);
		imap_path_separator_subst(new_name, namespace->separator);
		strtailchomp(new_name, namespace->separator);
	}
	strtailchomp(dirpath, '/');

	if (strcmp(name, "INBOX") != 0) {
		GPtrArray *argbuf;
		gint i;
		gboolean exist = FALSE;

		argbuf = g_ptr_array_new();
		ok = imap_cmd_list(SESSION(session)->sock, NULL, imappath,
				   argbuf);
		statusbar_pop_all();
		if (ok != IMAP_SUCCESS) {
			log_warning(_("can't create mailbox: LIST failed\n"));
			g_free(dirpath);
			g_ptr_array_free(argbuf, TRUE);
			return NULL;
		}

		for (i = 0; i < argbuf->len; i++) {
			gchar *str;
			str = g_ptr_array_index(argbuf, i);
			if (!strncmp(str, "LIST ", 5)) {
				exist = TRUE;
				break;
			}
		}
		g_ptr_array_free(argbuf, TRUE);

		if (!exist) {
			ok = imap_cmd_create(SESSION(session)->sock, imappath);
			statusbar_pop_all();
			if (ok != IMAP_SUCCESS) {
				log_warning(_("can't create mailbox\n"));
				g_free(dirpath);
				return NULL;
			}
		}
	}

	new_item = folder_item_new(new_name, dirpath);
	folder_item_append(parent, new_item);
	g_free(dirpath);

	dirpath = folder_item_get_path(new_item);
	if (!is_dir_exist(dirpath))
		make_dir_hier(dirpath);
	g_free(dirpath);

	return new_item;
}

gint imap_remove_folder(Folder *folder, FolderItem *item)
{
	gint ok;
	IMAPSession *session;
	gchar *path;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(item != NULL, -1);
	g_return_val_if_fail(item->path != NULL, -1);

	session = imap_session_get(folder);
	if (!session) return -1;

	path = imap_get_real_path(IMAP_FOLDER(folder), item->path);
	ok = imap_cmd_delete(SESSION(session)->sock, path);
	statusbar_pop_all();
	if (ok != IMAP_SUCCESS) {
		log_warning(_("can't delete mailbox\n"));
		g_free(path);
		return -1;
	}

	g_free(path);
	folder_item_remove(item);

	return 0;
}

static GSList *imap_get_uncached_messages(IMAPSession *session,
					  FolderItem *item,
					  guint32 first_uid, guint32 last_uid)
{
	gchar *tmp;
	GSList *newlist = NULL;
	GSList *llast = NULL;
	GString *str;
	MsgInfo *msginfo;

	g_return_val_if_fail(session != NULL, NULL);
	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(item->folder != NULL, NULL);
	g_return_val_if_fail(item->folder->type == F_IMAP, NULL);
	g_return_val_if_fail(first_uid <= last_uid, NULL);

	if (imap_cmd_envelope(SESSION(session)->sock, first_uid, last_uid)
	    != IMAP_SUCCESS) {
		log_warning(_("can't get envelope\n"));
		return NULL;
	}

	str = g_string_new(NULL);

	for (;;) {
		if ((tmp = sock_getline(SESSION(session)->sock)) == NULL) {
			log_warning(_("error occurred while getting envelope.\n"));
			g_string_free(str, TRUE);
			return newlist;
		}
		strretchomp(tmp);
		log_print("IMAP4< %s\n", tmp);
		if (tmp[0] != '*' || tmp[1] != ' ') {
			g_free(tmp);
			break;
		}
		g_string_assign(str, tmp);
		g_free(tmp);

		msginfo = imap_parse_envelope(SESSION(session)->sock, str);
		if (!msginfo) {
			log_warning(_("can't parse envelope: %s\n"), str->str);
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

	g_string_free(str, TRUE);

	return newlist;
}

static GSList *imap_delete_cached_messages(GSList *mlist, FolderItem *item,
					   guint32 first_uid, guint32 last_uid)
{
	GSList *cur, *next;
	MsgInfo *msginfo;
	gchar *dir;

	g_return_val_if_fail(item != NULL, mlist);
	g_return_val_if_fail(item->folder != NULL, mlist);
	g_return_val_if_fail(item->folder->type == F_IMAP, mlist);

	debug_print(_("Deleting cached messages %d - %d ... "),
		    first_uid, last_uid);

	dir = folder_item_get_path(item);
	remove_numbered_files(dir, first_uid, last_uid);
	g_free(dir);

	for (cur = mlist; cur != NULL; ) {
		next = cur->next;

		msginfo = (MsgInfo *)cur->data;
		if (msginfo != NULL && first_uid <= msginfo->msgnum &&
		    msginfo->msgnum <= last_uid) {
			procmsg_msginfo_free(msginfo);
			mlist = g_slist_remove(mlist, msginfo);
		}

		cur = next;
	}

	debug_print(_("done.\n"));

	return mlist;
}

static void imap_delete_all_cached_messages(FolderItem *item)
{
	gchar *dir;

	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);
	g_return_if_fail(item->folder->type == F_IMAP);

	debug_print(_("Deleting all cached messages... "));

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

static void imap_parse_namespace(IMAPSession *session, IMAPFolder *folder)
{
	gchar *ns_str;
	gchar *name;
	gchar *separator;
	gchar *p;
	IMAPNameSpace *namespace;
	GList *ns_list = NULL;

	g_return_if_fail(session != NULL);
	g_return_if_fail(folder != NULL);

	if (folder->namespace != NULL) return;

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
		/* parse ("#foo" "/") */

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

		while (*p && isspace(*p)) p++;
		if (*p == '\0') THROW;
		if (strncmp(p, "NIL", 3) == 0)
			separator = NULL;
		else if (*p == '"') {
			p++;
			separator = p;
			while (*p && *p != '"') p++;
			if (*p == '\0') THROW;
			*p = '\0';
			p++;
		} else THROW;

		while (*p && *p != ')') p++;
		if (*p == '\0') THROW;
		p++;

		namespace = g_new(IMAPNameSpace, 1);
		namespace->name = g_strdup(name);
		namespace->separator = separator ? separator[0] : '\0';
		ns_list = g_list_append(ns_list, namespace);
		IMAP_FOLDER(folder)->namespace = ns_list;
	}

catch:
	g_free(ns_str);
	return;
}

#undef THROW

static IMAPNameSpace *imap_find_namespace(IMAPFolder *folder,
					  const gchar *path)
{
	IMAPNameSpace *namespace = NULL;
	GList *ns_list;
	gchar *name;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(path != NULL, NULL);

	ns_list = folder->namespace;

	for (; ns_list != NULL; ns_list = ns_list->next) {
		IMAPNameSpace *tmp_ns = ns_list->data;

		Xstrdup_a(name, tmp_ns->name, return namespace);
		if (tmp_ns->separator && tmp_ns->separator != '/')
			subst_char(name, tmp_ns->separator, '/');
		if (strncmp(path, name, strlen(name)) == 0)
			namespace = tmp_ns;
	}

	return namespace;
}

static gchar *imap_get_real_path(IMAPFolder *folder, const gchar *path)
{
	gchar *real_path;
	IMAPNameSpace *namespace;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(path != NULL, NULL);

	real_path = g_strdup(path);
	namespace = imap_find_namespace(folder, path);
	if (namespace && namespace->separator)
		imap_path_separator_subst(real_path, namespace->separator);

	return real_path;
}

static gchar *imap_parse_atom(SockInfo *sock, gchar *src,
			      gchar *dest, gint dest_len, GString *str)
{
	gchar *cur_pos = src;

	g_return_val_if_fail(str != NULL, cur_pos);

	while (*cur_pos == ' ') cur_pos++;

	if (!strncmp(cur_pos, "NIL", 3)) {
		*dest = '\0';
		cur_pos += 3;
	} else if (*cur_pos == '\"') {
		gchar *p;

		p = strchr_cpy(cur_pos + 1, '\"', dest, dest_len);
		cur_pos = p ? p : cur_pos + 2;
	} else if (*cur_pos == '{') {
		gchar buf[32];
		gint len;
		gchar *nextline;

		cur_pos = strchr_cpy(cur_pos + 1, '}', buf, sizeof(buf));
		len = atoi(buf);

		if ((nextline = sock_getline(sock)) == NULL)
			return cur_pos;
		strretchomp(nextline);
		log_print("IMAP4< %s\n", nextline);
		g_string_assign(str, nextline);

		len = MIN(len, strlen(nextline));
		memcpy(dest, nextline, MIN(len, dest_len - 1));
		dest[MIN(len, dest_len - 1)] = '\0';
		cur_pos = str->str + len;
	}

	return cur_pos;
}

static gchar *imap_parse_one_address(SockInfo *sock, gchar *start,
				     gchar *out_from_str,
				     gchar *out_fromname_str,
				     GString *str)
{
	gchar buf[IMAPBUFSIZE];
	gchar *userid;
	gchar *domain;
	gchar *cur_pos = start;

	cur_pos = imap_parse_atom(sock, cur_pos, buf, sizeof(buf), str);
	conv_unmime_header(out_fromname_str, IMAPBUFSIZE, buf, NULL);

	cur_pos = imap_parse_atom(sock, cur_pos, buf, sizeof(buf), str);

	cur_pos = imap_parse_atom(sock, cur_pos, buf, sizeof(buf), str);
	Xstrdup_a(userid, buf, return cur_pos + 1);

	cur_pos = imap_parse_atom(sock, cur_pos, buf, sizeof(buf), str);
	Xstrdup_a(domain, buf, return cur_pos + 1);

	if (out_fromname_str[0] != '\0') {
		g_snprintf(out_from_str, IMAPBUFSIZE, "\"%s\" <%s@%s>",
			   out_fromname_str, userid, domain);
	} else {
		g_snprintf(out_from_str, IMAPBUFSIZE, "%s@%s",
			   userid, domain);
		strcpy(out_fromname_str, out_from_str);
	}

	while (*cur_pos == ' ') cur_pos++;
	g_return_val_if_fail(*cur_pos == ')', NULL);

	return cur_pos + 1;
}

static gchar *imap_parse_address(SockInfo *sock, gchar *start,
				 gchar **out_from_str,
				 gchar **out_fromname_str,
				 GString *str)
{
	gchar buf[IMAPBUFSIZE];
	gchar name_buf[IMAPBUFSIZE];
	gchar *cur_pos = start;
	GString *addr_str;

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

	addr_str = g_string_new(NULL);

	for (;;) {
		gchar ch = *cur_pos++;
		if (ch == ')') break;
		if (ch == '(') {
			cur_pos = imap_parse_one_address
				(sock, cur_pos, buf, name_buf, str);
			if (!cur_pos) {
				g_string_free(addr_str, TRUE);
				return NULL;
			}
			if (addr_str->str[0] != '\0')
				g_string_append(addr_str, ", ");
			g_string_append(addr_str, buf);
		}
	}

	if (out_from_str)     *out_from_str     = g_strdup(addr_str->str);
	if (out_fromname_str) *out_fromname_str = g_strdup(name_buf);

	g_string_free(addr_str, TRUE);

	return cur_pos;
}

static MsgFlags imap_parse_flags(const gchar *flag_str)  
{
	const gchar *p = flag_str;
	MsgFlags flags;

	flags = 0;
	MSG_SET_FLAGS(flags, MSG_UNREAD|MSG_IMAP);

	while ((p = strchr(p, '\\')) != NULL) {
		p++;

		if (g_strncasecmp(p, "Recent", 6) == 0) {
			MSG_SET_FLAGS(flags, MSG_NEW|MSG_UNREAD);
		} else if (g_strncasecmp(p, "Seen", 4) == 0) {
			MSG_UNSET_FLAGS(flags, MSG_NEW|MSG_UNREAD);
		} else if (g_strncasecmp(p, "Deleted", 7) == 0) {
			MSG_SET_FLAGS(flags, MSG_DELETED);
		} else if (g_strncasecmp(p, "Flagged", 7) == 0) {
			MSG_SET_FLAGS(flags, MSG_MARKED);
		}
	}

	return flags;
}

static MsgInfo *imap_parse_envelope(SockInfo *sock, GString *line_str)
{
	MsgInfo *msginfo;
	gchar buf[IMAPBUFSIZE];
	gchar tmp[IMAPBUFSIZE];
	gchar *cur_pos;
	gint msgnum;
	guint32 uid = 0;
	size_t size = 0;
	gchar *date = NULL;
	time_t date_t = 0;
	gchar *subject = NULL;
	gchar *tmp_from;
	gchar *tmp_fromname;
	gchar *from = NULL;
	gchar *fromname = NULL;
	gchar *tmp_to;
	gchar *to = NULL;
	gchar *inreplyto = NULL;
	gchar *msgid = NULL;
	MsgFlags flags = 0;
	gint n_element = 0;

	g_return_val_if_fail(line_str != NULL, NULL);
	g_return_val_if_fail(line_str->str[0] == '*' &&
			     line_str->str[1] == ' ', NULL);

	cur_pos = line_str->str + 2;

#define PARSE_ONE_ELEMENT(ch) \
{ \
	cur_pos = strchr_cpy(cur_pos, ch, buf, sizeof(buf)); \
	g_return_val_if_fail(cur_pos != NULL, NULL); \
}

	PARSE_ONE_ELEMENT(' ');
	msgnum = atoi(buf);

	PARSE_ONE_ELEMENT(' ');
	g_return_val_if_fail(!strcmp(buf, "FETCH"), NULL);

	g_return_val_if_fail(*cur_pos == '(', NULL);
	cur_pos++;

	while (n_element < 4) {
		while (*cur_pos == ' ') cur_pos++;

		if (!strncmp(cur_pos, "UID ", 4)) {
			cur_pos += 4;
			uid = strtoul(cur_pos, &cur_pos, 10);
		} else if (!strncmp(cur_pos, "FLAGS ", 6)) {
			cur_pos += 6;
			g_return_val_if_fail(*cur_pos == '(', NULL);
			cur_pos++;
			PARSE_ONE_ELEMENT(')');
			flags = imap_parse_flags(buf);
		} else if (!strncmp(cur_pos, "RFC822.SIZE ", 12)) {
			cur_pos += 12;
			size = strtol(cur_pos, &cur_pos, 10);
		} else if (!strncmp(cur_pos, "ENVELOPE ", 9)) {
			cur_pos += 9;
			g_return_val_if_fail(*cur_pos == '(', NULL);
			cur_pos = imap_parse_atom
				(sock, cur_pos + 1, buf, sizeof(buf), line_str);
			Xstrdup_a(date, buf, return NULL);
			date_t = procheader_date_parse(NULL, date, 0);

			cur_pos = imap_parse_atom
				(sock, cur_pos, buf, sizeof(buf), line_str);
			if (buf[0] != '\0') {
				conv_unmime_header(tmp, sizeof(tmp), buf, NULL);
				Xstrdup_a(subject, tmp, return NULL);
			}

			g_return_val_if_fail(*cur_pos == ' ', NULL);
			cur_pos = imap_parse_address(sock, cur_pos + 1,
						     &tmp_from, &tmp_fromname,
						     line_str);
			Xstrdup_a(from, tmp_from,
				  {g_free(tmp_from); g_free(tmp_fromname);
				   return NULL;});
			Xstrdup_a(fromname, tmp_fromname,
				  {g_free(tmp_from); g_free(tmp_fromname);
				   return NULL;});
			g_free(tmp_from);
			g_free(tmp_fromname);

#define SKIP_ONE_ELEMENT() \
{ \
	g_return_val_if_fail(*cur_pos == ' ', NULL); \
	cur_pos = imap_parse_address(sock, cur_pos + 1, NULL, NULL, \
				     line_str); \
}

			/* skip sender and reply-to */
			SKIP_ONE_ELEMENT();
			SKIP_ONE_ELEMENT();

			g_return_val_if_fail(*cur_pos == ' ', NULL);
			cur_pos = imap_parse_address(sock, cur_pos + 1,
						     &tmp_to, NULL, line_str);
			Xstrdup_a(to, tmp_to, {g_free(tmp_to); return NULL;});
			g_free(tmp_to);

			/* skip Cc and Bcc */
			SKIP_ONE_ELEMENT();
			SKIP_ONE_ELEMENT();

#undef SKIP_ONE_ELEMENT

			g_return_val_if_fail(*cur_pos == ' ', NULL);
			cur_pos = imap_parse_atom
				(sock, cur_pos, buf, sizeof(buf), line_str);
			if (buf[0] != '\0') {
				eliminate_parenthesis(buf, '(', ')');
				extract_parenthesis(buf, '<', '>');
				remove_space(buf);
				Xstrdup_a(inreplyto, buf, return NULL);
			}

			g_return_val_if_fail(*cur_pos == ' ', NULL);
			cur_pos = imap_parse_atom
				(sock, cur_pos, buf, sizeof(buf), line_str);
			if (buf[0] != '\0') {
				extract_parenthesis(buf, '<', '>');
				remove_space(buf);
				Xstrdup_a(msgid, buf, return NULL);
			}

			g_return_val_if_fail(*cur_pos == ')', NULL);
			cur_pos++;
		} else {
			g_warning("invalid FETCH response: %s\n", cur_pos);
			break;
		}

		if (*cur_pos == '\0' || *cur_pos == ')') break;
		n_element++;
	}

	msginfo = g_new0(MsgInfo, 1);
	msginfo->msgnum = uid;
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

static gint imap_set_message_flags(IMAPSession *session,
				   guint32 first_uid,
				   guint32 last_uid,
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

	ok = imap_cmd_store(SESSION(session)->sock, first_uid, last_uid,
			    buf->str);
	g_string_free(buf, TRUE);

	return ok;
}

static gint imap_select(IMAPSession *session, IMAPFolder *folder,
			const gchar *path,
			gint *exists, gint *recent, gint *unseen,
			guint32 *uid_validity)
{
	gchar *real_path;
	gint ok;

	real_path = imap_get_real_path(folder, path);
	ok = imap_cmd_select(SESSION(session)->sock, real_path,
			     exists, recent, unseen, uid_validity);
	if (ok != IMAP_SUCCESS)
		log_warning(_("can't select folder: %s\n"), real_path);
	g_free(real_path);

	return ok;
}

#define THROW(err) { ok = err; goto catch; }

static gint imap_get_uid(IMAPSession *session, gint msgnum, guint32 *uid)
{
	gint ok;
	GPtrArray *argbuf;
	gchar *str;
	gint num;

	*uid = 0;
	argbuf = g_ptr_array_new();

	imap_cmd_gen_send(SESSION(session)->sock, "FETCH %d (UID)", msgnum);
	if ((ok = imap_cmd_ok(SESSION(session)->sock, argbuf)) != IMAP_SUCCESS)
		THROW(ok);

	str = search_array_contain_str(argbuf, "FETCH");
	if (!str) THROW(IMAP_ERROR);

	if (sscanf(str, "%d FETCH (UID %d)", &num, uid) != 2 ||
	    num != msgnum) {
		g_warning("imap_get_uid(): invalid FETCH line.\n");
		THROW(IMAP_ERROR);
	}

catch:
	ptr_array_free_strings(argbuf);
	g_ptr_array_free(argbuf, TRUE);

	return ok;
}

#undef THROW

#if 0
static gint imap_status_uidnext(IMAPSession *session, IMAPFolder *folder,
				const gchar *path, guint32 *uid_next)
{
	gchar *real_path;
	gint ok;

	real_path = imap_get_real_path(folder, path);
	ok = imap_cmd_status(SESSION(session)->sock, real_path,
			     "UIDNEXT", uid_next);
	if (ok != IMAP_SUCCESS)
		log_warning(_("can't get the next uid of folder: %s\n"),
			    real_path);
	g_free(real_path);

	return ok;
}
#endif


/* low-level IMAP4rev1 commands */

static gint imap_cmd_login(SockInfo *sock,
			   const gchar *user, const gchar *pass)
{
	gint ok;

	if (strchr(user, ' ') != NULL)
		imap_cmd_gen_send(sock, "LOGIN \"%s\" %s", user, pass);
	else
		imap_cmd_gen_send(sock, "LOGIN %s %s", user, pass);

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

static gint imap_cmd_list(SockInfo *sock, const gchar *ref,
			  const gchar *mailbox, GPtrArray *argbuf)
{
	gchar *new_ref;
	gchar *new_mailbox;

	if (!ref) ref = "\"\"";
	if (!mailbox) mailbox = "\"\"";

	if (*ref != '"' && strchr(ref, ' ') != NULL)
		new_ref = g_strdup_printf("\"%s\"", ref);
	else
		new_ref = g_strdup(ref);
	if (*mailbox != '"' && strchr(mailbox, ' ') != NULL)
		new_mailbox = g_strdup_printf("\"%s\"", mailbox);
	else
		new_mailbox = g_strdup(mailbox);

	imap_cmd_gen_send(sock, "LIST %s %s", new_ref, new_mailbox);

	g_free(new_ref);
	g_free(new_mailbox);

	return imap_cmd_ok(sock, argbuf);
}

#define THROW goto catch

static gint imap_cmd_select(SockInfo *sock, const gchar *folder,
			    gint *exists, gint *recent, gint *unseen,
			    guint32 *uid_validity)
{
	gint ok;
	gchar *resp_str;
	GPtrArray *argbuf;

	*exists = *recent = *unseen = *uid_validity = 0;
	argbuf = g_ptr_array_new();

	if (strchr(folder, ' ') != NULL)
		imap_cmd_gen_send(sock, "SELECT \"%s\"", folder);
	else
		imap_cmd_gen_send(sock, "SELECT %s", folder);

	if ((ok = imap_cmd_ok(sock, argbuf)) != IMAP_SUCCESS) THROW;

	resp_str = search_array_contain_str(argbuf, "EXISTS");
	if (resp_str) {
		if (sscanf(resp_str,"%d EXISTS", exists) != 1) {
			g_warning("imap_cmd_select(): invalid EXISTS line.\n");
			THROW;
		}
	}

	resp_str = search_array_contain_str(argbuf, "RECENT");
	if (resp_str) {
		if (sscanf(resp_str, "%d RECENT", recent) != 1) {
			g_warning("imap_cmd_select(): invalid RECENT line.\n");
			THROW;
		}
	}

	resp_str = search_array_contain_str(argbuf, "UIDVALIDITY");
	if (resp_str) {
		if (sscanf(resp_str, "OK [UIDVALIDITY %u] ", uid_validity)
		    != 1) {
			g_warning("imap_cmd_select(): invalid UIDVALIDITY line.\n");
			THROW;
		}
	}

	resp_str = search_array_contain_str(argbuf, "UNSEEN");
	if (resp_str) {
		if (sscanf(resp_str, "OK [UNSEEN %d] ", unseen) != 1) {
			g_warning("imap_cmd_select(): invalid UNSEEN line.\n");
			THROW;
		}
	}

catch:
	ptr_array_free_strings(argbuf);
	g_ptr_array_free(argbuf, TRUE);

	return ok;
}

#if 0
static gint imap_cmd_status(SockInfo *sock, const gchar *folder,
			    const gchar *status, gint *value)
{
	gint ok;
	GPtrArray *argbuf;
	gchar *str;

	if (value) *value = 0;
	argbuf = g_ptr_array_new();

	if (strchr(folder, ' ') != NULL)
		imap_cmd_gen_send(sock, "STATUS \"%s\" (%s)", folder, status);
	else
		imap_cmd_gen_send(sock, "STATUS %s (%s)", folder, status);

	if ((ok = imap_cmd_ok(sock, argbuf)) != IMAP_SUCCESS) THROW;

	str = search_array_contain_str(argbuf, "STATUS");
	if (!str) THROW;
	str = strchr(str, '(');
	if (!str) THROW;
	str++;
	if (strncmp(str, status, strlen(status)) != 0) THROW;
	str += strlen(status);
	if (*str != ' ') THROW;
	str++;
	if (value) *value = atoi(str);

catch:
	ptr_array_free_strings(argbuf);
	g_ptr_array_free(argbuf, TRUE);

	return ok;
}
#endif

#undef THROW

static gint imap_cmd_create(SockInfo *sock, const gchar *folder)
{
	if (strchr(folder, ' ') != NULL)
		imap_cmd_gen_send(sock, "CREATE \"%s\"", folder);
	else
		imap_cmd_gen_send(sock, "CREATE %s", folder);

	return imap_cmd_ok(sock, NULL);
}

static gint imap_cmd_delete(SockInfo *sock, const gchar *folder)
{
	if (strchr(folder, ' ') != NULL)
		imap_cmd_gen_send(sock, "DELETE \"%s\"", folder);
	else
		imap_cmd_gen_send(sock, "DELETE %s", folder);

	return imap_cmd_ok(sock, NULL);
}

static gint imap_cmd_fetch(SockInfo *sock, guint32 uid, const gchar *filename)
{
	gint ok;
	gchar buf[IMAPBUFSIZE];
	gchar *cur_pos;
	gchar size_str[32];
	glong size_num;

	g_return_val_if_fail(filename != NULL, IMAP_ERROR);

	imap_cmd_gen_send(sock, "UID FETCH %d BODY[]", uid);

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

	if (buf[0] == '\0' || buf[strlen(buf) - 1] != ')')
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

static gint imap_cmd_copy(SockInfo *sock, guint32 uid, const gchar *destfolder)
{
	gint ok;

	g_return_val_if_fail(destfolder != NULL, IMAP_ERROR);

	if (strchr(destfolder, ' ') != NULL)
		imap_cmd_gen_send(sock, "UID COPY %d \"%s\"", uid, destfolder);
	else
		imap_cmd_gen_send(sock, "UID COPY %d %s", uid, destfolder);

	ok = imap_cmd_ok(sock, NULL);
	if (ok != IMAP_SUCCESS) {
		log_warning(_("can't copy %d to %s\n"), uid, destfolder);
		return -1;
	}

	return ok;
}

gint imap_cmd_envelope(SockInfo *sock, guint32 first_uid, guint32 last_uid)
{
	imap_cmd_gen_send
		(sock, "UID FETCH %d:%d (UID FLAGS RFC822.SIZE ENVELOPE)",
		 first_uid, last_uid);

	return IMAP_SUCCESS;
}

static gint imap_cmd_store(SockInfo *sock, guint32 first_uid, guint32 last_uid,
			   gchar *sub_cmd)
{
	gint ok;

	imap_cmd_gen_send(sock, "UID STORE %d:%d %s",
			  first_uid, last_uid, sub_cmd);

	if ((ok = imap_cmd_ok(sock, NULL)) != IMAP_SUCCESS) {
		log_warning(_("error while imap command: STORE %d:%d %s\n"),
			    first_uid, last_uid, sub_cmd);
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
				g_ptr_array_add(argbuf, g_strdup(buf + 2));
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
		*p = '\0';
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
	if (!tmp)
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

static void imap_path_separator_subst(gchar *str, gchar separator)
{
	if (separator && separator != '/')
		subst_char(str, '/', separator);
}
