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

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>

#include "defs.h"
#include "intl.h"
#include "news.h"
#include "nntp.h"
#include "socket.h"
#include "recv.h"
#include "procmsg.h"
#include "procheader.h"
#include "folder.h"
#include "session.h"
#include "statusbar.h"
#include "codeconv.h"
#include "utils.h"
#include "prefs_common.h"
#include "prefs_account.h"
#include "inputdialog.h"
#include "alertpanel.h"

static gint news_get_article_cmd	 (Folder	*folder,
					  NNTPSession	*session,
					  const gchar	*cmd,
					  gint		 num,
					  gchar		*filename);
static gint news_get_article		 (Folder	*folder,
					  NNTPSession	*session,
					  gint		 num,
					  gchar		*filename);
static gint news_get_header		 (Folder	*folder,
					  NNTPSession	*session,
					  gint		 num,
					  gchar		*filename);

static GSList *news_get_uncached_articles(NNTPSession	*session,
					  FolderItem	*item,
					  gint		 cache_last,
					  gint		*rfirst,
					  gint		*rlast);
static MsgInfo *news_parse_xover	 (const gchar	*xover_str);
static GSList *news_delete_old_article	 (GSList	*alist,
					  gint		 first);
static void news_delete_all_article	 (FolderItem	*item);

static gchar *news_query_password	 (NNTPSession	*session,
					  const gchar	*user);
static gint news_authenticate		 (Folder	*folder);
static gint news_nntp_group		 (Folder 	*folder,
					  SockInfo	*sock,
					  const gchar	*group,
					  gint		*num,
					  gint		*first,
					  gint		*last);
static gint news_nntp_list		 (Folder	*folder,
					  SockInfo	*sock);
static gint news_nntp_xover		 (Folder	*folder,
					  SockInfo	*sock,
					  gint		 first,
					  gint		 last);
static gint news_nntp_post		 (Folder	*folder,
					  SockInfo	*sock,
					  FILE		*fp);
static gint news_nntp_mode		 (Folder	*folder,
					  SockInfo	*sock,
					  gboolean	 stream);

Session *news_session_new(const gchar *server, gushort port)
{
	gchar buf[NNTPBUFSIZE];
	NNTPSession *session;
	SockInfo *nntp_sock;

	g_return_val_if_fail(server != NULL, NULL);

	log_message(_("creating NNTP connection to %s:%d ...\n"), server, port);

	if ((nntp_sock = nntp_open(server, port, buf)) < 0)
		return NULL;

	session = g_new(NNTPSession, 1);
	SESSION(session)->type      = SESSION_NEWS;
	SESSION(session)->server    = g_strdup(server);
	SESSION(session)->sock      = nntp_sock;
	SESSION(session)->connected = TRUE;
	SESSION(session)->phase     = SESSION_READY;
	SESSION(session)->data      = NULL;
	session->group = NULL;

	return SESSION(session);
}

void news_session_destroy(NNTPSession *session)
{
	sock_close(SESSION(session)->sock);
	SESSION(session)->sock = NULL;

	g_free(session->group);
}

NNTPSession *news_session_get(Folder *folder)
{
	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(folder->type == F_NEWS, NULL);
	g_return_val_if_fail(folder->account != NULL, NULL);

	if (!REMOTE_FOLDER(folder)->session) {
		REMOTE_FOLDER(folder)->session =
			news_session_new(folder->account->nntp_server, 119);
	} else {
		if (news_nntp_mode(folder,
				   REMOTE_FOLDER(folder)->session->sock,
				   FALSE)
		    != NN_SUCCESS) {
			log_warning(_("NNTP connection to %s:%d has been"
				      " disconnected. Reconnecting...\n"),
				    folder->account->nntp_server, 119);
			session_destroy(REMOTE_FOLDER(folder)->session);
			REMOTE_FOLDER(folder)->session =
				news_session_new(folder->account->nntp_server,
						 119);
		}
	}

	return NNTP_SESSION(REMOTE_FOLDER(folder)->session);
}

GSList *news_get_article_list(Folder *folder, FolderItem *item,
			      gboolean use_cache)
{
	GSList *alist;
	NNTPSession *session;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(folder->type == F_NEWS, NULL);

	session = news_session_get(folder);

	if (!session) {
		alist = procmsg_read_cache(item, FALSE);
		item->last_num = procmsg_get_last_num_in_cache(alist);
	} else if (use_cache) {
		GSList *newlist;
		gint cache_last;
		gint first, last;

		alist = procmsg_read_cache(item, FALSE);

		cache_last = procmsg_get_last_num_in_cache(alist);
		newlist = news_get_uncached_articles
			(session, item, cache_last, &first, &last);
		alist = news_delete_old_article(alist, first);

		alist = g_slist_concat(alist, newlist);
		item->last_num = last;
	} else {
		gint last;

		alist = news_get_uncached_articles
			(session, item, 0, NULL, &last);
		news_delete_all_article(item);
		item->last_num = last;
	}

	procmsg_set_flags(alist, item);

	statusbar_pop_all();

	return alist;
}

gchar *news_fetch_msg(Folder *folder, FolderItem *item, gint num)
{
	gchar *path, *filename;
	gint ok;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(item != NULL, NULL);

	path = folder_item_get_path(item);
	filename = g_strconcat(path, G_DIR_SEPARATOR_S, itos(num), NULL);
	g_free(path);

	if (is_file_exist(filename)) {
		debug_print(_("article %d has been already cached.\n"), num);
		return filename;
	}

	if (!REMOTE_FOLDER(folder)->session) {
		g_free(filename);
		return NULL;
	}

	debug_print(_("getting article %d...\n"), num);
	ok = news_get_article(folder,
			      NNTP_SESSION(REMOTE_FOLDER(folder)->session),
			      num, filename);
	statusbar_pop_all();
	if (ok < 0) {
		g_warning(_("can't read article %d\n"), num);
		g_free(filename);
		return NULL;
	}

	return filename;
}

void news_scan_group(Folder *folder, FolderItem *item)
{
}

gint news_post(Folder *folder, const gchar *file)
{
	NNTPSession *session;
	FILE *fp;
	gint ok;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(folder->type == F_NEWS, -1);
	g_return_val_if_fail(file != NULL, -1);

	session = news_session_get(folder);
	if (!session) return -1;

	if ((fp = fopen(file, "r")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		return -1;
	}

	ok = news_nntp_post(folder, SESSION(session)->sock, fp);
	if (ok != NN_SUCCESS) {
		log_warning(_("can't post article.\n"));
		return -1;
	}

	fclose(fp);

	statusbar_pop_all();

	return 0;
}

static gint news_get_article_cmd(Folder *folder, NNTPSession *session,
				 const gchar *cmd,
				 gint num, gchar *filename)
{
	gint ok;
	gchar *msgid;

	ok = nntp_get_article(SESSION(session)->sock, cmd, num, &msgid);
	if (ok == NN_AUTHREQ) {
		ok = news_authenticate(folder);
		if (ok != NN_SUCCESS)
			return -1;
		ok = nntp_get_article(SESSION(session)->sock, cmd, num,
				      &msgid);
	}
	if (ok != NN_SUCCESS)
		return -1;

	debug_print("Message-Id = %s, num = %d\n", msgid, num);
	g_free(msgid);

	if (recv_write_to_file(SESSION(session)->sock, filename) < 0) {
		log_warning(_("can't retrieve article %d\n"), num);
		return -1;
	}

	return 0;
}

static gint news_get_article(Folder *folder, NNTPSession *session,
			     gint num, gchar *filename)
{
	return news_get_article_cmd(folder, session, "ARTICLE", num, filename);
}

static gint news_get_header(Folder *folder, NNTPSession *session, 
			    gint num, gchar *filename)
{
	return news_get_article_cmd(folder, session, "HEAD", num, filename);
}

static GSList *news_get_uncached_articles(NNTPSession *session,
					  FolderItem *item, gint cache_last,
					  gint *rfirst, gint *rlast)
{
	gint ok;
	gint num = 0, first = 0, last = 0, begin = 0, end = 0;
	gchar buf[NNTPBUFSIZE];
	GSList *newlist = NULL;
	GSList *llast = NULL;
	MsgInfo *msginfo;

	if (rfirst) *rfirst = 0;
	if (rlast)  *rlast  = 0;

	g_return_val_if_fail(session != NULL, NULL);
	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(item->folder != NULL, NULL);
	g_return_val_if_fail(item->folder->type == F_NEWS, NULL);

	ok = news_nntp_group(item->folder, SESSION(session)->sock, item->path,
			     &num, &first, &last);
	if (ok != NN_SUCCESS) {
		log_warning(_("can't set group: %s\n"), item->path);
		return NULL;
	}

	/* calculate getting overview range */
	if (first > last) {
		log_warning(_("invalid article range: %d - %d\n"),
			    first, last);
		return NULL;
	}
	if (cache_last < first)
		begin = first;
	else if (last < cache_last)
		begin = first;
	else if (last == cache_last) {
		debug_print(_("no new articles.\n"));
		return NULL;
	} else
		begin = cache_last + 1;
	end = last;

	if (prefs_common.max_articles > 0 &&
	    end - begin + 1 > prefs_common.max_articles)
		begin = end - prefs_common.max_articles + 1;

	log_message(_("getting xover %d - %d in %s...\n"),
		    begin, end, item->path);
	if (news_nntp_xover(item->folder, SESSION(session)->sock,
			    begin, end) != NN_SUCCESS) {
		log_warning(_("can't get xover\n"));
		return NULL;
	}

	for (;;) {
		if (sock_read(SESSION(session)->sock, buf, sizeof(buf)) < 0) {
			log_warning(_("error occurred while getting xover.\n"));
			return newlist;
		}

		if (buf[0] == '.' && buf[1] == '\r') break;

		msginfo = news_parse_xover(buf);
		if (!msginfo) {
			log_warning(_("invalid xover line: %s\n"), buf);
			continue;
		}

		msginfo->folder = item;
		msginfo->flags = MSG_NEW|MSG_UNREAD|MSG_NEWS;

		if (!newlist)
			llast = newlist = g_slist_append(newlist, msginfo);
		else {
			llast = g_slist_append(llast, msginfo);
			llast = llast->next;
		}
	}

	if (rfirst) *rfirst = first;
	if (rlast)  *rlast  = last;
	return newlist;
}

#define PARSE_ONE_PARAM(p, srcp) \
{ \
	p = strchr(srcp, '\t'); \
	if (!p) return NULL; \
	else \
		*p++ = '\0'; \
}

static MsgInfo *news_parse_xover(const gchar *xover_str)
{
	MsgInfo *msginfo;
	gchar buf[NNTPBUFSIZE];
	gchar *subject, *sender, *size, *line, *date, *msgid, *ref, *tmp;
	gchar *p;
	gint num, size_int, line_int;
	gchar *xover_buf;

	Xalloca(xover_buf, strlen(xover_str) + 1, return NULL);
	strcpy(xover_buf, xover_str);

	PARSE_ONE_PARAM(subject, xover_buf);
	PARSE_ONE_PARAM(sender, subject);
	PARSE_ONE_PARAM(date, sender);
	PARSE_ONE_PARAM(msgid, date);
	PARSE_ONE_PARAM(ref, msgid);
	PARSE_ONE_PARAM(size, ref);
	PARSE_ONE_PARAM(line, size);

	tmp = strchr(line, '\t');
	if (!tmp) tmp = strchr(line, '\r');
	if (!tmp) tmp = strchr(line, '\n');
	if (tmp) *tmp = '\0';

	num = atoi(xover_str);
	size_int = atoi(size);
	line_int = atoi(line);

	/* set MsgInfo */
	msginfo = g_new0(MsgInfo, 1);
	msginfo->msgnum = num;
	msginfo->size = size_int;

	msginfo->date = g_strdup(date);
	msginfo->date_t = procheader_date_parse(NULL, date, 0);

	conv_unmime_header(buf, sizeof(buf), sender, NULL);
	msginfo->from = g_strdup(buf);
	msginfo->fromname = procheader_get_fromname(buf);

	conv_unmime_header(buf, sizeof(buf), subject, NULL);
	msginfo->subject = g_strdup(buf);

	extract_parenthesis(msgid, '<', '>');
	remove_space(msgid);
	if (*msgid != '\0')
		msginfo->msgid = g_strdup(msgid);

	eliminate_parenthesis(ref, '(', ')');
	if ((p = strrchr(ref, '<')) != NULL) {
		extract_parenthesis(p, '<', '>');
		remove_space(p);
		if (*p != '\0')
			msginfo->inreplyto = g_strdup(p);
	}

	return msginfo;
}

static GSList *news_delete_old_article(GSList *alist, gint first)
{
	GSList *cur, *next;
	MsgInfo *msginfo;
	gchar *cache_file;

	if (first < 2) return alist;

	for (cur = alist; cur != NULL; ) {
		next = cur->next;

		msginfo = (MsgInfo *)cur->data;
		if (msginfo && msginfo->msgnum < first) {
			debug_print(_("deleting article %d...\n"),
				    msginfo->msgnum);

			cache_file = procmsg_get_message_file_path(msginfo);
			if (is_file_exist(cache_file)) unlink(cache_file);
			g_free(cache_file);

			procmsg_msginfo_free(msginfo);
			alist = g_slist_remove(alist, msginfo);
		}

		cur = next;
	}

	return alist;
}

static void news_delete_all_article(FolderItem *item)
{
	DIR *dp;
	struct dirent *d;
	gchar *dir;
	gchar *file;

	dir = folder_item_get_path(item);
	if ((dp = opendir(dir)) == NULL) {
		FILE_OP_ERROR(dir, "opendir");
		g_free(dir);
		return;
	}

	debug_print(_("\tDeleting all cached articles... "));

	while ((d = readdir(dp)) != NULL) {
		if (to_number(d->d_name) < 0) continue;

		file = g_strconcat(dir, G_DIR_SEPARATOR_S, d->d_name, NULL);

		if (is_file_exist(file)) {
			if (unlink(file) < 0)
				FILE_OP_ERROR(file, "unlink");
		}

		g_free(file);
	}

	closedir(dp);
	g_free(dir);

	debug_print(_("done.\n"));
}

/*
  news_get_group_list returns a strings list.
  These strings are the names of the newsgroups of a server.
  item is the FolderItem of the news server.
  The names of the newsgroups are cached into a file so that
  when the function is called again, there is no need to make
  a request to the server.
 */

GSList * news_get_group_list(FolderItem *item)
{
	gchar *path, *filename;
	gint ok;
	NNTPSession *session;
	GSList * group_list = NULL;
	FILE * f;
	gchar buf[NNTPBUFSIZE];
	int len;

	if (item == NULL)
	  return NULL;

	path = folder_item_get_path(item);

	if (!is_dir_exist(path))
		make_dir_hier(path);

	filename = g_strconcat(path, G_DIR_SEPARATOR_S, GROUPLIST_FILE, NULL);
	g_free(path);

	session = news_session_get(item->folder);

	if (session == NULL)
	  return NULL;

	if (is_file_exist(filename)) {
		debug_print(_("group list has been already cached.\n"));
	}
	else {
	    ok = news_nntp_list(item->folder, SESSION(session)->sock);
	    if (ok != NN_SUCCESS)
	      return NULL;
	    
	    if (recv_write_to_file(SESSION(session)->sock, filename) < 0) {
	      log_warning(_("can't retrieve group list\n"));
	      return NULL;
	    }
	}

	f = fopen(filename, "r");
	while (fgets(buf, NNTPBUFSIZE, f)) {
	    char * s;

	    len = 0;
	    while ((buf[len] != 0) && (buf[len] != ' '))
	      len++;
	    buf[len] = 0;
	    s = g_strdup(buf);

	    group_list = g_slist_append(group_list, s);
	}
	fclose(f);
	g_free(filename);

	group_list = g_slist_sort(group_list, (GCompareFunc) g_strcasecmp);

	return group_list;
}

/*
  remove the cache file of the names of the newsgroups.
 */

void news_reset_group_list(FolderItem *item)
{
	gchar *path, *filename;

	debug_print(_("\tDeleting cached group list... "));
	path = folder_item_get_path(item);
	filename = g_strconcat(path, G_DIR_SEPARATOR_S, GROUPLIST_FILE, NULL);
	g_free(path);
	if (remove(filename) != 0)
	  log_warning(_("can't delete cached group list %s\n"), filename);
	g_free(filename);
}


/* NNTP Authentication support (RFC2980). */


/**
 * news_query_password:
 * @session: Active NNTP session.
 * @user: User name.
 * 
 * Ask user for the NNTP authentication password.
 * 
 * Return value: Password string; needs to be freed with g_free()
 * after use.
 **/
static gchar *news_query_password(NNTPSession *session,
				  const gchar *user)
{
	gchar *message;
	gchar *pass;

	message = g_strdup_printf
		(_("Input password for %s on %s:"),
		 user,
		 SESSION(session)->server);

	pass = input_dialog_with_invisible(_("Input password"),
					   message, NULL);
	g_free(message);
/*  	manage_window_focus_in(inc_dialog->mainwin->window, */
/*  			       NULL, NULL); */
	return pass;
}

/**
 * news_authenticate:
 * @folder: NNTP folder.
 * 
 * Send NNTP authentication information (AUTHINFO USER, AUTHINFO PASS)
 * as described in RFC2980.  May ask user for the password if it is
 * not stored in account settings.
 * 
 * Return value: NNTP result code (NN_*).
 **/
static gint news_authenticate(Folder *folder)
{
	NNTPSession *session;
	gint ok;
	const gchar *user;
	gchar *pass;
	gboolean need_free_pass = FALSE;

	debug_print(_("news server requested authentication\n"));
	if (!folder || !folder->account)
		return NN_ERROR;
	user = folder->account->userid;
	if (!user)
		return NN_ERROR;
	session = NNTP_SESSION(REMOTE_FOLDER(folder)->session);
	if (!session)
		return NN_ERROR;
	ok = nntp_authinfo_user(SESSION(session)->sock, user);
	if (ok == NN_AUTHCONT) {
		pass = folder->account->passwd;
		if (!pass || !pass[0]) {
			pass = news_query_password(session, user);
			need_free_pass = TRUE;
		}
		ok = nntp_authinfo_pass(SESSION(session)->sock, pass);
		if (need_free_pass)
			g_free(pass);
	}
	if (ok != NN_SUCCESS) {
		log_warning(_("NNTP authentication failed\n"));
		alertpanel_error(_("Authentication for %s on %s failed."),
				 user, SESSION(session)->server);
	}
	return ok;
}


/**
 * news_nntp_group:
 * @folder: NNTP folder.
 * @sock: NNTP socket.
 * @group: Group name to select.
 * @num: Pointer to returned number of messages in the group.
 * @first: Pointer to returned number of first message.
 * @last: Pointer to returned number of last message.
 * 
 * Wrapper for nntp_group() to do authentication if requested.
 * Selects newsgroup @group and returns information from the server
 * response in @num, @first, @last (which cannot be NULL).
 * 
 * Return value: NNTP result code (NN_*).
 **/
static gint news_nntp_group(Folder *folder, SockInfo *sock,
			    const gchar *group,
			    gint *num, gint *first, gint *last)
{
	gint ok;

	if ((ok = nntp_group(sock, group, num, first, last)) == NN_AUTHREQ) {
		ok = news_authenticate(folder);
		if (ok != NN_SUCCESS)
			return ok;
		ok = nntp_group(sock, group, num, first, last);
	}
	return ok;
}

/**
 * news_nntp_list:
 * @folder: NNTP folder.
 * @sock: NNTP socket.
 * 
 * Wrapper for nntp_list() to do authentication if requested.  Sends
 * the LIST command to @sock and receives the status code line.
 * 
 * Return value: NNTP result code (NN_*).
 **/
static gint news_nntp_list(Folder *folder, SockInfo *sock)
{
	gint ok;

	if ((ok = nntp_list(sock)) == NN_AUTHREQ) {
		ok = news_authenticate(folder);
		if (ok != NN_SUCCESS)
			return ok;
		ok = nntp_list(sock);
	}
	return ok;
}

/**
 * news_nntp_xover:
 * @folder: NNTP folder.
 * @sock: NNTP socket.
 * @first: First message number.
 * @last: Last message number.
 * 
 * Wrapper for nntp_xover() to do authentication if requested.  Sends
 * the XOVER command with parameters @first and @last to @sock and
 * receives the status code line.
 * 
 * Return value: NNTP result code (NN_*).
 **/
static gint news_nntp_xover(Folder *folder, SockInfo *sock,
			    gint first, gint last)
{
	gint ok;

	if ((ok = nntp_xover(sock, first, last)) == NN_AUTHREQ) {
		ok = news_authenticate(folder);
		if (ok != NN_SUCCESS)
			return ok;
		ok = nntp_xover(sock, first, last);
	}
	return ok;
}

/**
 * news_nntp_post:
 * @folder: NNTP folder.
 * @sock: NNTP socket.
 * @fp: File with the message (header and body), opened for reading.
 * 
 * Wrapper for nntp_post() to do authentication if requested.  Sends
 * the POST command to @sock, and if the server accepts it, sends the
 * message from @fp.
 * 
 * Return value: NNTP result code (NN_*).
 **/
static gint news_nntp_post(Folder *folder, SockInfo *sock, FILE *fp)
{
	gint ok;

	if ((ok = nntp_post(sock, fp)) == NN_AUTHREQ) {
		ok = news_authenticate(folder);
		if (ok != NN_SUCCESS)
			return ok;
		ok = nntp_post(sock, fp);
	}
	return ok;
}

/**
 * news_nntp_mode:
 * @folder: NNTP folder.
 * @sock: NNTP socket.
 * @stream: TRUE for stream mode, FALSE for reader mode.
 * 
 * Wrapper for nntp_mode() to do authentication if requested.  Sends
 * the "MODE READER" or "MODE STREAM" command to @sock, depending on
 * @stream, and receives the status responce.
 * 
 * Return value: NNTP result code (NN_*).
 **/
static gint news_nntp_mode(Folder *folder, SockInfo *sock, gboolean stream)
{
	gint ok;

	if ((ok = nntp_mode(sock, stream)) == NN_AUTHREQ) {
		ok = news_authenticate(folder);
		if (ok != NN_SUCCESS)
			return ok;
		ok = nntp_mode(sock, stream);
	}
	return ok;
}
