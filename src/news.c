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

#include "defs.h"

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <time.h>

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

#define NNTP_PORT	119

static Session *news_session_new	 (const gchar	*server,
					  gushort	 port,
					  const gchar	*userid,
					  const gchar	*passwd);

static gint news_get_article_cmd	 (NNTPSession	*session,
					  const gchar	*cmd,
					  gint		 num,
					  gchar		*filename);
static gint news_get_article		 (NNTPSession	*session,
					  gint		 num,
					  gchar		*filename);
static gint news_get_header		 (NNTPSession	*session,
					  gint		 num,
					  gchar		*filename);

static gint news_select_group		 (NNTPSession	*session,
					  const gchar	*group);
static GSList *news_get_uncached_articles(NNTPSession	*session,
					  FolderItem	*item,
					  gint		 cache_last,
					  gint		*rfirst,
					  gint		*rlast);
static MsgInfo *news_parse_xover	 (const gchar	*xover_str);
static gchar *news_parse_xhdr		 (const gchar	*xhdr_str,
					  MsgInfo	*msginfo);
static GSList *news_delete_old_articles	 (GSList	*alist,
					  FolderItem	*item,
					  gint		 first);
static void news_delete_all_articles	 (FolderItem	*item);

static void news_group_list_free(GSList * list);


static Session *news_session_new(const gchar *server, gushort port,
				 const gchar *userid, const gchar *passwd)
{
	gchar buf[NNTPBUFSIZE];
	NNTPSession *session;
	NNTPSockInfo *nntp_sock;

	g_return_val_if_fail(server != NULL, NULL);

	log_message(_("creating NNTP connection to %s:%d ...\n"), server, port);

	if (userid && passwd)
		nntp_sock = nntp_open_auth(server, port, buf, userid, passwd);
	else
		nntp_sock = nntp_open(server, port, buf);
	if (nntp_sock == NULL)
		return NULL;

	session = g_new(NNTPSession, 1);
	SESSION(session)->type             = SESSION_NEWS;
	SESSION(session)->server           = g_strdup(server);
	session->nntp_sock                 = nntp_sock;
	SESSION(session)->sock             = nntp_sock->sock;
	SESSION(session)->connected        = TRUE;
	SESSION(session)->phase            = SESSION_READY;
	SESSION(session)->last_access_time = time(NULL);
	SESSION(session)->data             = NULL;
	session->group = NULL;
	session->group_list = NULL;

	return SESSION(session);
}

void news_session_destroy(NNTPSession *session)
{
	nntp_close(session->nntp_sock);
	session->nntp_sock = NULL;
	SESSION(session)->sock = NULL;

	news_group_list_free(session->group_list);
	session->group_list = NULL;

	g_free(session->group);
}

static gchar *news_query_password(const gchar *server, const gchar *user)
{
	gchar *message;
	gchar *pass;

	message = g_strdup_printf(_("Input password for %s on %s:"),
				  user, server);
	pass = input_dialog_with_invisible(_("Input password"), message, NULL);
	g_free(message);

	return pass;
}

static Session *news_session_new_for_folder(Folder *folder)
{
	Session *session;
	PrefsAccount *ac;
	const gchar *userid = NULL;
	gchar *passwd = NULL;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(folder->account != NULL, NULL);

	ac = folder->account;
	if (ac->use_nntp_auth && ac->userid && ac->userid[0]) {
		userid = ac->userid;
		if (ac->passwd && ac->passwd[0])
			passwd = g_strdup(ac->passwd);
		else
			passwd = news_query_password(ac->nntp_server, userid);
	}

	session = news_session_new(ac->nntp_server,
				   ac->set_nntpport ? ac->nntpport : NNTP_PORT,
				   userid, passwd);
	g_free(passwd);

	return session;
}

NNTPSession *news_session_get(Folder *folder)
{
	NNTPSession *session;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(folder->type == F_NEWS, NULL);
	g_return_val_if_fail(folder->account != NULL, NULL);

	if (!REMOTE_FOLDER(folder)->session) {
		REMOTE_FOLDER(folder)->session =
			news_session_new_for_folder(folder);
		statusbar_pop_all();
		return NNTP_SESSION(REMOTE_FOLDER(folder)->session);
	}

	session = NNTP_SESSION(REMOTE_FOLDER(folder)->session);

	if (time(NULL) - SESSION(session)->last_access_time < SESSION_TIMEOUT) {
		SESSION(session)->last_access_time = time(NULL);
		statusbar_pop_all();
		return session;
	}

	if (nntp_mode(session->nntp_sock, FALSE) != NN_SUCCESS) {
		log_warning(_("NNTP connection to %s:%d has been"
			      " disconnected. Reconnecting...\n"),
			    folder->account->nntp_server,
			    folder->account->set_nntpport ?
			    folder->account->nntpport : NNTP_PORT);
		session_destroy(REMOTE_FOLDER(folder)->session);
		REMOTE_FOLDER(folder)->session =
			news_session_new_for_folder(folder);
	}

	statusbar_pop_all();
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
		alist = news_delete_old_articles(alist, item, first);

		alist = g_slist_concat(alist, newlist);
		item->last_num = last;
	} else {
		gint last;

		alist = news_get_uncached_articles
			(session, item, 0, NULL, &last);
		news_delete_all_articles(item);
		item->last_num = last;
	}

	procmsg_set_flags(alist, item);

	statusbar_pop_all();

	return alist;
}

gchar *news_fetch_msg(Folder *folder, FolderItem *item, gint num)
{
	gchar *path, *filename;
	NNTPSession *session;
	gint ok;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(item != NULL, NULL);

	path = folder_item_get_path(item);
	if (!is_dir_exist(path))
		make_dir_hier(path);
	filename = g_strconcat(path, G_DIR_SEPARATOR_S, itos(num), NULL);
	g_free(path);

	if (is_file_exist(filename)) {
		debug_print(_("article %d has been already cached.\n"), num);
		return filename;
	}

	session = news_session_get(folder);
	if (!session) {
		g_free(filename);
		return NULL;
	}

	ok = news_select_group(session, item->path);
	statusbar_pop_all();
	if (ok != NN_SUCCESS) {
		g_warning(_("can't select group %s\n"), item->path);
		g_free(filename);
		return NULL;
	}

	debug_print(_("getting article %d...\n"), num);
	ok = news_get_article(NNTP_SESSION(REMOTE_FOLDER(folder)->session),
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

static struct NNTPGroupInfo * group_info_new(gchar * name,
					     gint first, gint last,
					     gchar type)
{
	struct NNTPGroupInfo * info;

	info = g_new(struct NNTPGroupInfo, 1);
	if (info == NULL)
		return NULL;
	info->name = g_strdup(name);
	info->first = first;
	info->last = last;
	info->type = type;

	return info;
}

static void group_info_free(struct NNTPGroupInfo * info)
{
  g_free(info->name);
  g_free(info);
}

static void news_group_list_free(GSList * list)
{
  g_slist_foreach(list, (GFunc) group_info_free, NULL);
  g_slist_free(list);
}

gint news_group_info_compare(struct NNTPGroupInfo * info1,
			     struct NNTPGroupInfo * info2)
{
  return g_strcasecmp(info1->name, info2->name);
}

GSList *news_get_group_list(Folder *folder)
{
	gchar *path, *filename;
	FILE *fp;
	GSList *list = NULL;
	GSList *last = NULL;
	gchar buf[NNTPBUFSIZE];
	NNTPSession *session;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(folder->type == F_NEWS, NULL);

	path = folder_item_get_path(FOLDER_ITEM(folder->node->data));
	if (!is_dir_exist(path))
		make_dir_hier(path);
	filename = g_strconcat(path, G_DIR_SEPARATOR_S, NEWSGROUP_LIST, NULL);
	g_free(path);

	session = news_session_get(folder);
	if (!session) {
		g_free(filename);
		return NULL;
	}

	if (session->group_list) {
		g_free(filename);
		return session->group_list;
	}

	if ((fp = fopen(filename, "r")) == NULL) {
		if (nntp_list(session->nntp_sock) != NN_SUCCESS) {
			g_free(filename);
			statusbar_pop_all();
			return NULL;
		}
		statusbar_pop_all();
		if (recv_write_to_file(SESSION(session)->sock, filename) < 0) {
			log_warning(_("can't retrieve newsgroup list\n"));
			g_free(filename);
			return NULL;
		}

		if ((fp = fopen(filename, "r")) == NULL) {
			FILE_OP_ERROR(filename, "fopen");
			g_free(filename);
			return NULL;
		}
	}

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		gchar * p;
		gchar * cur;
		gchar * name;
		gint last_article;
		gint first_article;
		gchar type;
		struct NNTPGroupInfo * info;

		cur = buf;
		p = strchr(cur, ' ');
		if (p == NULL)
			continue;
		* p = 0;

		name = cur;
		
		cur = p + 1;
		p = strchr(cur, ' ');
		if (p == NULL)
			continue;
		* p = 0;
		last_article = atoi(cur);

		cur = p + 1;
		p = strchr(cur, ' ');
		if (p == NULL)
			continue;
		* p = 0;
		first_article = atoi(cur);

		cur = p + 1;
		type = * cur;

		info = group_info_new(name, first_article,
				      last_article, type);
		if (info == NULL)
			continue;

		if (!last)
			last = list = g_slist_append(NULL, info);
		else {
			last = g_slist_append(last, info);
			last = last->next;
		}
	}

	fclose(fp);
	g_free(filename);

	list = g_slist_sort(list, (GCompareFunc) news_group_info_compare);

	session->group_list = list;

	statusbar_pop_all();

	return list;
}

void news_cancel_group_list_cache(Folder *folder)
{
	gchar *path, *filename;
	NNTPSession *session;

	g_return_if_fail(folder != NULL);
	g_return_if_fail(folder->type == F_NEWS);

	session = news_session_get(folder);
	if (!session)
		return;

	news_group_list_free(session->group_list);
	session->group_list = NULL;

	path = folder_item_get_path(FOLDER_ITEM(folder->node->data));
	filename = g_strconcat(path, G_DIR_SEPARATOR_S, NEWSGROUP_LIST, NULL);
	g_free(path);

	if (is_file_exist(filename)) {
		if (remove(filename) < 0)
			FILE_OP_ERROR(filename, "remove");
	}
	g_free(filename);
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

	ok = nntp_post(session->nntp_sock, fp);
	if (ok != NN_SUCCESS) {
		log_warning(_("can't post article.\n"));
		return -1;
	}

	fclose(fp);

	statusbar_pop_all();

	return 0;
}

static gint news_get_article_cmd(NNTPSession *session, const gchar *cmd,
				 gint num, gchar *filename)
{
	gchar *msgid;

	if (nntp_get_article(session->nntp_sock, cmd, num, &msgid)
	    != NN_SUCCESS)
		return -1;

	debug_print("Message-Id = %s, num = %d\n", msgid, num);
	g_free(msgid);

	if (recv_write_to_file(session->nntp_sock->sock, filename) < 0) {
		log_warning(_("can't retrieve article %d\n"), num);
		return -1;
	}

	return 0;
}

static gint news_get_article(NNTPSession *session, gint num, gchar *filename)
{
	return news_get_article_cmd(session, "ARTICLE", num, filename);
}

static gint news_get_header(NNTPSession *session, gint num, gchar *filename)
{
	return news_get_article_cmd(session, "HEAD", num, filename);
}

/**
 * news_select_group:
 * @session: Active NNTP session.
 * @group: Newsgroup name.
 *
 * Select newsgroup @group with the GROUP command if it is not already
 * selected in @session.
 *
 * Return value: NNTP result code.
 **/
static gint news_select_group(NNTPSession *session, const gchar *group)
{
	gint ok;
	gint num, first, last;

	if (session->group && g_strcasecmp(session->group, group) == 0)
		return NN_SUCCESS;

	g_free(session->group);
	session->group = NULL;

	ok = nntp_group(session->nntp_sock, group, &num, &first, &last);
	if (ok == NN_SUCCESS)
		session->group = g_strdup(group);

	return ok;
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

	g_free(session->group);
	session->group = NULL;

	ok = nntp_group(session->nntp_sock, item->path,
			&num, &first, &last);
	if (ok != NN_SUCCESS) {
		log_warning(_("can't set group: %s\n"), item->path);
		return NULL;
	}
	session->group = g_strdup(item->path);

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

	if (rfirst) *rfirst = first;
	if (rlast)  *rlast  = last;

	if (prefs_common.max_articles > 0 &&
	    end - begin + 1 > prefs_common.max_articles)
		begin = end - prefs_common.max_articles + 1;

	log_message(_("getting xover %d - %d in %s...\n"),
		    begin, end, item->path);
	if (nntp_xover(session->nntp_sock, begin, end) != NN_SUCCESS) {
		log_warning(_("can't get xover\n"));
		return NULL;
	}

	for (;;) {
		if (sock_gets(SESSION(session)->sock, buf, sizeof(buf)) < 0) {
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
		msginfo->flags.perm_flags = MSG_NEW|MSG_UNREAD;
		msginfo->flags.tmp_flags = MSG_NEWS;
		msginfo->newsgroups = g_strdup(item->path);

		if (!newlist)
			llast = newlist = g_slist_append(newlist, msginfo);
		else {
			llast = g_slist_append(llast, msginfo);
			llast = llast->next;
		}
	}

	if (nntp_xhdr(session->nntp_sock, "to", begin, end) != NN_SUCCESS) {
		log_warning(_("can't get xhdr\n"));
		return newlist;
	}

	llast = newlist;

	for (;;) {
		if (sock_gets(SESSION(session)->sock, buf, sizeof(buf)) < 0) {
			log_warning(_("error occurred while getting xhdr.\n"));
			return newlist;
		}

		if (buf[0] == '.' && buf[1] == '\r') break;
		if (!llast) {
			g_warning("llast == NULL\n");
			continue;
		}

		msginfo = (MsgInfo *)llast->data;
		msginfo->to = news_parse_xhdr(buf, msginfo);

		llast = llast->next;
	}

	if (nntp_xhdr(session->nntp_sock, "cc", begin, end) != NN_SUCCESS) {
		log_warning(_("can't get xhdr\n"));
		return newlist;
	}

	llast = newlist;

	for (;;) {
		if (sock_gets(SESSION(session)->sock, buf, sizeof(buf)) < 0) {
			log_warning(_("error occurred while getting xhdr.\n"));
			return newlist;
		}

		if (buf[0] == '.' && buf[1] == '\r') break;
		if (!llast) {
			g_warning("llast == NULL\n");
			continue;
		}

		msginfo = (MsgInfo *)llast->data;
		msginfo->cc = news_parse_xhdr(buf, msginfo);

		llast = llast->next;
	}

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

	Xstrdup_a(xover_buf, xover_str, return NULL);

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

	msginfo->references = g_strdup(ref);
	eliminate_parenthesis(ref, '(', ')');
	if ((p = strrchr(ref, '<')) != NULL) {
		extract_parenthesis(p, '<', '>');
		remove_space(p);
		if (*p != '\0')
			msginfo->inreplyto = g_strdup(p);
	}

	return msginfo;
}

static gchar *news_parse_xhdr(const gchar *xhdr_str, MsgInfo *msginfo)
{
	gchar *p;
	gchar *tmp;
	gint num;

	p = strchr(xhdr_str, ' ');
	if (!p)
		return NULL;
	else
		p++;

	num = atoi(xhdr_str);
	if (msginfo->msgnum != num) return NULL;

	tmp = strchr(p, '\r');
	if (!tmp) tmp = strchr(p, '\n');

	if (tmp)
		return g_strndup(p, tmp - p);
	else
		return g_strdup(p);
}

static GSList *news_delete_old_articles(GSList *alist, FolderItem *item,
					gint first)
{
	GSList *cur, *next;
	MsgInfo *msginfo;
	gchar *dir;

	g_return_val_if_fail(item != NULL, alist);
	g_return_val_if_fail(item->folder != NULL, alist);
	g_return_val_if_fail(item->folder->type == F_NEWS, alist);

	if (first < 2) return alist;

	debug_print(_("Deleting cached articles 1 - %d ... "), first - 1);

	dir = folder_item_get_path(item);
	remove_numbered_files(dir, 1, first - 1);
	g_free(dir);

	for (cur = alist; cur != NULL; ) {
		next = cur->next;

		msginfo = (MsgInfo *)cur->data;
		if (msginfo && msginfo->msgnum < first) {
			procmsg_msginfo_free(msginfo);
			alist = g_slist_remove(alist, msginfo);
		}

		cur = next;
	}

	return alist;
}

static void news_delete_all_articles(FolderItem *item)
{
	gchar *dir;

	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);
	g_return_if_fail(item->folder->type == F_NEWS);

	debug_print(_("\tDeleting all cached articles... "));

	dir = folder_item_get_path(item);
	remove_all_numbered_files(dir);
	g_free(dir);

	debug_print(_("done.\n"));
}
