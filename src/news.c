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
#if USE_SSL
#  include "ssl.h"
#endif

#define NNTP_PORT	119
#if USE_SSL
#define NNTPS_PORT	563
#endif

static void news_folder_init		 (Folder	*folder,
					  const gchar	*name,
					  const gchar	*path);

#if USE_SSL
static Session *news_session_new	 (const gchar	*server,
					  gushort	 port,
					  const gchar	*userid,
					  const gchar	*passwd,
					  SSLType	 ssl_type);
#else
static Session *news_session_new	 (const gchar	*server,
					  gushort	 port,
					  const gchar	*userid,
					  const gchar	*passwd);
#endif

static gint news_get_article_cmd	 (NNTPSession	*session,
					  const gchar	*cmd,
					  gint		 num,
					  gchar		*filename);
static gint news_get_article		 (NNTPSession	*session,
					  gint		 num,
					  gchar		*filename);

static gint news_select_group		 (NNTPSession	*session,
					  const gchar	*group,
					  gint		*num,
					  gint		*first,
					  gint		*last);
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
static void news_delete_expired_caches	 (GSList	*alist,
					  FolderItem	*item);

static gint news_remove_msg		 (Folder	*folder, 
					  FolderItem	*item, 
					  gint		 num);
gint news_get_num_list		 	 (Folder 	*folder, 
					  FolderItem 	*item,
					  GSList       **list);
MsgInfo *news_fetch_msginfo		 (Folder 	*folder, 
					  FolderItem 	*item,
					  gint 		 num);
GSList *news_fetch_msginfos		 (Folder 	*folder,
					  FolderItem 	*item,
					  GSList 	*msgnum_list);

gint news_post_stream			 (Folder 	*folder, 
					  FILE 		*fp);

Folder *news_folder_new(const gchar *name, const gchar *path)
{
	Folder *folder;

	folder = (Folder *)g_new0(NewsFolder, 1);
	news_folder_init(folder, name, path);

	return folder;
}

void news_folder_destroy(Folder *folder)
{
	gchar *dir;

	dir = folder_get_path(folder);
	if (is_dir_exist(dir))
		remove_dir_recursive(dir);
	g_free(dir);

	folder_remote_folder_destroy(REMOTE_FOLDER(folder));
}

static void news_folder_init(Folder *folder, const gchar *name,
			     const gchar *path)
{
	folder->type = F_NEWS;

	folder_remote_folder_init(folder, name, path);

/*
	folder->get_msg_list = news_get_article_list;
*/
	folder->fetch_msg    = news_fetch_msg;
/*
	folder->scan         = news_scan_group;
*/
	folder->destroy      = news_folder_destroy;
	folder->remove_msg   = news_remove_msg;
	folder->get_num_list = news_get_num_list;
	folder->fetch_msginfo = news_fetch_msginfo;
	folder->fetch_msginfos = news_fetch_msginfos;
}

#if USE_SSL
static Session *news_session_new(const gchar *server, gushort port,
				 const gchar *userid, const gchar *passwd,
				 SSLType ssl_type)
#else
static Session *news_session_new(const gchar *server, gushort port,
				 const gchar *userid, const gchar *passwd)
#endif
{
	gchar buf[NNTPBUFSIZE];
	NNTPSession *session;
	NNTPSockInfo *nntp_sock;

	g_return_val_if_fail(server != NULL, NULL);

	log_message(_("creating NNTP connection to %s:%d ...\n"), server, port);

#if USE_SSL
	if (userid && passwd)
		nntp_sock = nntp_open_auth(server, port, buf, userid, passwd,
					   ssl_type);
	else
		nntp_sock = nntp_open(server, port, buf, ssl_type);
#else
	if (userid && passwd)
		nntp_sock = nntp_open_auth(server, port, buf, userid, passwd);
	else
		nntp_sock = nntp_open(server, port, buf);
#endif

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

	SESSION(session)->destroy          = news_session_destroy;

	session->group = NULL;

	return SESSION(session);
}

void news_session_destroy(Session *session)
{
	nntp_close(NNTP_SESSION(session)->nntp_sock);
	NNTP_SESSION(session)->nntp_sock = NULL;
	session->sock = NULL;

	g_free(NNTP_SESSION(session)->group);
}

static Session *news_session_new_for_folder(Folder *folder)
{
	Session *session;
	PrefsAccount *ac;
	const gchar *userid = NULL;
	gchar *passwd = NULL;
	gushort port;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(folder->account != NULL, NULL);

	ac = folder->account;
	if (ac->use_nntp_auth && ac->userid && ac->userid[0]) {
		userid = ac->userid;
		if (ac->passwd && ac->passwd[0])
			passwd = g_strdup(ac->passwd);
		else
			passwd = input_dialog_query_password(ac->nntp_server,
							     userid);
	}

#if USE_SSL
	port = ac->set_nntpport ? ac->nntpport
		: ac->ssl_nntp ? NNTPS_PORT : NNTP_PORT;
	session = news_session_new(ac->nntp_server, port, userid, passwd,
				   ac->ssl_nntp);
#else
	port = ac->set_nntpport ? ac->nntpport : NNTP_PORT;
	session = news_session_new(ac->nntp_server, port, userid, passwd);
#endif

	g_free(passwd);

	return session;
}

NNTPSession *news_session_get(Folder *folder)
{
	RemoteFolder *rfolder = REMOTE_FOLDER(folder);

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(folder->type == F_NEWS, NULL);
	g_return_val_if_fail(folder->account != NULL, NULL);

	if (!rfolder->session) {
		rfolder->session = news_session_new_for_folder(folder);
		statusbar_pop_all();
		return NNTP_SESSION(rfolder->session);
	}

	if (time(NULL) - rfolder->session->last_access_time < SESSION_TIMEOUT) {
		rfolder->session->last_access_time = time(NULL);
		statusbar_pop_all();
		return NNTP_SESSION(rfolder->session);
	}

	if (nntp_mode(NNTP_SESSION(rfolder->session)->nntp_sock, FALSE)
	    != NN_SUCCESS) {
		log_warning(_("NNTP connection to %s:%d has been"
			      " disconnected. Reconnecting...\n"),
			    folder->account->nntp_server,
			    folder->account->set_nntpport ?
			    folder->account->nntpport : NNTP_PORT);
		session_destroy(rfolder->session);
		rfolder->session = news_session_new_for_folder(folder);
	}

	if (rfolder->session)
		rfolder->session->last_access_time = time(NULL);
	statusbar_pop_all();
	return NNTP_SESSION(rfolder->session);
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
		item->last_num = procmsg_get_last_num_in_msg_list(alist);
	} else if (use_cache) {
		GSList *newlist;
		gint cache_last;
		gint first, last;

		alist = procmsg_read_cache(item, FALSE);

		cache_last = procmsg_get_last_num_in_msg_list(alist);
		newlist = news_get_uncached_articles
			(session, item, cache_last, &first, &last);
		if (first == 0 && last == 0) {
			news_delete_all_articles(item);
			procmsg_msg_list_free(alist);
			alist = NULL;
		} else {
			alist = news_delete_old_articles(alist, item, first);
			news_delete_expired_caches(alist, item);
		}

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
		debug_print("article %d has been already cached.\n", num);
		return filename;
	}

	session = news_session_get(folder);
	if (!session) {
		g_free(filename);
		return NULL;
	}

	ok = news_select_group(session, item->path, NULL, NULL, NULL);
	statusbar_pop_all();
	if (ok != NN_SUCCESS) {
		g_warning(_("can't select group %s\n"), item->path);
		g_free(filename);
		return NULL;
	}

	debug_print("getting article %d...\n", num);
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

gint news_scan_group(Folder *folder, FolderItem *item)
{
	NNTPSession *session;
	gint num = 0, first = 0, last = 0;
	gint ok;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(item != NULL, -1);

	session = news_session_get(folder);
	if (!session) return -1;

	ok = news_select_group(session, item->path, &num, &first, &last);
	if (ok != NN_SUCCESS) {
		log_warning(_("can't set group: %s\n"), item->path);
		return -1;
	}

	if (num == 0) {
		item->new = item->unread = item->total = item->last_num = 0;
		return 0;
	}

/*
	path = folder_item_get_path(item);
	if (path && is_dir_exist(path)) {
		procmsg_get_mark_sum(path, &new, &unread, &total, &min, &max,
				     first);
	}
	g_free(path);

	if (max < first || last < min)
		new = unread = total = num;
	else {
		if (min < first)
			min = first;

		if (last < max)
			max = last;
		else if (max < last) {
			new += last - max;
			unread += last - max;
		}

		if (new > num) new = num;
		if (unread > num) unread = num;
	}

	item->new = new;
	item->unread = unread;
	item->total = num;
	item->last_num = last;
*/
	return 0;
}

static NewsGroupInfo *news_group_info_new(const gchar *name,
					  gint first, gint last, gchar type)
{
	NewsGroupInfo *ginfo;

	ginfo = g_new(NewsGroupInfo, 1);
	ginfo->name = g_strdup(name);
	ginfo->first = first;
	ginfo->last = last;
	ginfo->type = type;

	return ginfo;
}

static void news_group_info_free(NewsGroupInfo *ginfo)
{
	g_free(ginfo->name);
	g_free(ginfo);
}

static gint news_group_info_compare(NewsGroupInfo *ginfo1,
				    NewsGroupInfo *ginfo2)
{
	return g_strcasecmp(ginfo1->name, ginfo2->name);
}

GSList *news_get_group_list(Folder *folder)
{
	gchar *path, *filename;
	FILE *fp;
	GSList *list = NULL;
	GSList *last = NULL;
	gchar buf[NNTPBUFSIZE];

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(folder->type == F_NEWS, NULL);

	path = folder_item_get_path(FOLDER_ITEM(folder->node->data));
	if (!is_dir_exist(path))
		make_dir_hier(path);
	filename = g_strconcat(path, G_DIR_SEPARATOR_S, NEWSGROUP_LIST, NULL);
	g_free(path);

	if ((fp = fopen(filename, "rb")) == NULL) {
		NNTPSession *session;

		session = news_session_get(folder);
		if (!session) {
			g_free(filename);
			return NULL;
		}

		if (nntp_list(session->nntp_sock) != NN_SUCCESS) {
			g_free(filename);
			statusbar_pop_all();
			return NULL;
		}
		statusbar_pop_all();
		if (recv_write_to_file(SESSION(session)->sock, filename) < 0) {
			log_warning(_("can't retrieve newsgroup list\n"));
			session_destroy(SESSION(session));
			REMOTE_FOLDER(folder)->session = NULL;
			g_free(filename);
			return NULL;
		}

		if ((fp = fopen(filename, "rb")) == NULL) {
			FILE_OP_ERROR(filename, "fopen");
			g_free(filename);
			return NULL;
		}
	}

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		gchar *p = buf;
		gchar *name;
		gint last_num;
		gint first_num;
		gchar type;
		NewsGroupInfo *ginfo;

		p = strchr(p, ' ');
		if (!p) continue;
		*p = '\0';
		p++;
		name = buf;

		if (sscanf(p, "%d %d %c", &last_num, &first_num, &type) < 3)
			continue;

		ginfo = news_group_info_new(name, first_num, last_num, type);

		if (!last)
			last = list = g_slist_append(NULL, ginfo);
		else {
			last = g_slist_append(last, ginfo);
			last = last->next;
		}
	}

	fclose(fp);
	g_free(filename);

	list = g_slist_sort(list, (GCompareFunc)news_group_info_compare);

	statusbar_pop_all();

	return list;
}

void news_group_list_free(GSList *group_list)
{
	GSList *cur;

	if (!group_list) return;

	for (cur = group_list; cur != NULL; cur = cur->next)
		news_group_info_free((NewsGroupInfo *)cur->data);
	g_slist_free(group_list);
}

void news_remove_group_list_cache(Folder *folder)
{
	gchar *path, *filename;

	g_return_if_fail(folder != NULL);
	g_return_if_fail(folder->type == F_NEWS);

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
	FILE *fp;
	gint ok;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(folder->type == F_NEWS, -1);
	g_return_val_if_fail(file != NULL, -1);

	if ((fp = fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		return -1;
	}

	ok = news_post_stream(folder, fp);

	fclose(fp);

	statusbar_pop_all();

	return ok;
}

gint news_post_stream(Folder *folder, FILE *fp)
{
	NNTPSession *session;
	gint ok;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(folder->type == F_NEWS, -1);
	g_return_val_if_fail(fp != NULL, -1);

	session = news_session_get(folder);
	if (!session) return -1;

	ok = nntp_post(session->nntp_sock, fp);
	if (ok != NN_SUCCESS) {
		log_warning(_("can't post article.\n"));
		return -1;
	}

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

static gint news_remove_msg(Folder *folder, FolderItem *item, gint num)
{
	gchar * dir;
	gint r;

	dir = folder_item_get_path(item);
	debug_print("news_remove_msg: removing msg %d in %s\n",num,dir);
	r = remove_numbered_files(dir, num, num);
	g_free(dir);

	return r;
}

static gint news_get_article(NNTPSession *session, gint num, gchar *filename)
{
	return news_get_article_cmd(session, "ARTICLE", num, filename);
}

/**
 * news_select_group:
 * @session: Active NNTP session.
 * @group: Newsgroup name.
 * @num: Estimated number of articles.
 * @first: First article number.
 * @last: Last article number.
 *
 * Select newsgroup @group with the GROUP command if it is not already
 * selected in @session, or article numbers need to be returned.
 *
 * Return value: NNTP result code.
 **/
static gint news_select_group(NNTPSession *session, const gchar *group,
			      gint *num, gint *first, gint *last)
{
	gint ok;
	gint num_, first_, last_;

	if (!num || !first || !last) {
		if (session->group && g_strcasecmp(session->group, group) == 0)
			return NN_SUCCESS;
		num = &num_;
		first = &first_;
		last = &last_;
	}

	g_free(session->group);
	session->group = NULL;

	ok = nntp_group(session->nntp_sock, group, num, first, last);
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

	if (rfirst) *rfirst = -1;
	if (rlast)  *rlast  = -1;

	g_return_val_if_fail(session != NULL, NULL);
	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(item->folder != NULL, NULL);
	g_return_val_if_fail(item->folder->type == F_NEWS, NULL);

	ok = news_select_group(session, item->path, &num, &first, &last);
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

	if (rfirst) *rfirst = first;
	if (rlast)  *rlast  = last;

	if (cache_last < first)
		begin = first;
	else if (last < cache_last)
		begin = first;
	else if (last == cache_last) {
		debug_print("no new articles.\n");
		return NULL;
	} else
		begin = cache_last + 1;
	end = last;

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
	gchar *subject, *sender, *size, *line, *date, *msgid, *ref, *tmp, *xref;
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
	PARSE_ONE_PARAM(xref, line);

	tmp = strchr(xref, '\t');
	if (!tmp) tmp = strchr(line, '\r');
	if (!tmp) tmp = strchr(line, '\n');
	if (tmp) *tmp = '\0';

	num = atoi(xover_str);
	size_int = atoi(size);
	line_int = atoi(line);

	/* set MsgInfo */
	msginfo = procmsg_msginfo_new();
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

	msginfo->xref = g_strdup(xref);
	p = msginfo->xref+strlen(msginfo->xref) - 1;
	while (*p == '\r' || *p == '\n') {
		*p = '\0';
		p--;
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

	debug_print("Deleting cached articles 1 - %d ...\n", first - 1);

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

	debug_print("Deleting all cached articles...\n");

	dir = folder_item_get_path(item);
	remove_all_numbered_files(dir);
	g_free(dir);
}

static void news_delete_expired_caches(GSList *alist, FolderItem *item)
{
	gchar *dir;

	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);
	g_return_if_fail(item->folder->type == F_NEWS);

	debug_print("Deleting expired cached articles...\n");

	dir = folder_item_get_path(item);
	remove_expired_files(dir, 24 * 7);
	g_free(dir);
}

gint news_cancel_article(Folder * folder, MsgInfo * msginfo)
{
	gchar * tmp;
	FILE * tmpfp;
	gchar buf[BUFFSIZE];

	tmp = g_strdup_printf("%s%ctmp%d", g_get_tmp_dir(),
			      G_DIR_SEPARATOR, (gint)msginfo);
	if (tmp == NULL)
		return -1;

	if ((tmpfp = fopen(tmp, "wb")) == NULL) {
		FILE_OP_ERROR(tmp, "fopen");
		return -1;
	}
	if (change_file_mode_rw(tmpfp, tmp) < 0) {
		FILE_OP_ERROR(tmp, "chmod");
		g_warning(_("can't change file mode\n"));
	}
	
	fprintf(tmpfp, "From: %s\r\n", msginfo->from);
	fprintf(tmpfp, "Newsgroups: %s\r\n", msginfo->newsgroups);
	fprintf(tmpfp, "Subject: cmsg cancel <%s>\r\n", msginfo->msgid);
	fprintf(tmpfp, "Control: cancel <%s>\r\n", msginfo->msgid);
	fprintf(tmpfp, "Approved: %s\r\n", msginfo->from);
	fprintf(tmpfp, "X-Cancelled-by: %s\r\n", msginfo->from);
	get_rfc822_date(buf, sizeof(buf));
	fprintf(tmpfp, "Date: %s\r\n", buf);
	fprintf(tmpfp, "\r\n");
	fprintf(tmpfp, "removed with sylpheed\r\n");

	fclose(tmpfp);

	news_post(folder, tmp);
	remove(tmp);

	g_free(tmp);

	return 0;
}

gint news_get_num_list(Folder *folder, FolderItem *item, GSList **msgnum_list)
{
	NNTPSession *session;
	gint i, ok, num, first, last, nummsgs = 0;
	gchar *dir;

	g_return_val_if_fail(item != NULL, -1);
	g_return_val_if_fail(item->folder != NULL, -1);
	g_return_val_if_fail(item->folder->type == F_NEWS, -1);

	session = news_session_get(folder);
	g_return_val_if_fail(session != NULL, -1);

	ok = news_select_group(session, item->path, &num, &first, &last);
	if (ok != NN_SUCCESS) {
		log_warning(_("can't set group: %s\n"), item->path);
		return -1;
	}

	if(last < first) {
		log_warning(_("invalid article range: %d - %d\n"),
			    first, last);
		return 0;
	}

	for(i = first; i <= last; i++) {
		*msgnum_list = g_slist_prepend(*msgnum_list, GINT_TO_POINTER(i));
		nummsgs++;
	}

	dir = folder_item_get_path(item);
	debug_print("removing old messages from %d to %d in %s\n", first, last, dir);
	remove_numbered_files(dir, 1, first - 1);
	g_free(dir);

	return nummsgs;
}

#define READ_TO_LISTEND(hdr) \
	while (!(buf[0] == '.' && buf[1] == '\r')) { \
		if (sock_gets(SESSION(session)->sock, buf, sizeof(buf)) < 0) { \
			log_warning(_("error occurred while getting %s.\n"), hdr); \
			return msginfo; \
		} \
	}

MsgInfo *news_fetch_msginfo(Folder *folder, FolderItem *item, gint num)
{
	NNTPSession *session;
	MsgInfo *msginfo = NULL;
	gchar buf[NNTPBUFSIZE];

	session = news_session_get(folder);
	g_return_val_if_fail(session != NULL, NULL);
	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(item->folder != NULL, NULL);
	g_return_val_if_fail(item->folder->type == F_NEWS, NULL);

	log_message(_("getting xover %d in %s...\n"),
		    num, item->path);
	if (nntp_xover(session->nntp_sock, num, num) != NN_SUCCESS) {
		log_warning(_("can't get xover\n"));
		return NULL;
	}
	
	if (sock_gets(SESSION(session)->sock, buf, sizeof(buf)) < 0) {
		log_warning(_("error occurred while getting xover.\n"));
		return NULL;
	}
	
	msginfo = news_parse_xover(buf);
	if (!msginfo) {
		log_warning(_("invalid xover line: %s\n"), buf);
	}

	READ_TO_LISTEND("xover");

	if(!msginfo)
		return NULL;
	
	msginfo->folder = item;
	msginfo->flags.perm_flags = MSG_NEW|MSG_UNREAD;
	msginfo->flags.tmp_flags = MSG_NEWS;
	msginfo->newsgroups = g_strdup(item->path);

	if (nntp_xhdr(session->nntp_sock, "to", num, num) != NN_SUCCESS) {
		log_warning(_("can't get xhdr\n"));
		return msginfo;
	}

	if (sock_gets(SESSION(session)->sock, buf, sizeof(buf)) < 0) {
		log_warning(_("error occurred while getting xhdr.\n"));
		return msginfo;
	}

	msginfo->to = news_parse_xhdr(buf, msginfo);

	READ_TO_LISTEND("xhdr (to)");

	if (nntp_xhdr(session->nntp_sock, "cc", num, num) != NN_SUCCESS) {
		log_warning(_("can't get xhdr\n"));
		return msginfo;
	}

	if (sock_gets(SESSION(session)->sock, buf, sizeof(buf)) < 0) {
		log_warning(_("error occurred while getting xhdr.\n"));
		return msginfo;
	}

	msginfo->cc = news_parse_xhdr(buf, msginfo);

	READ_TO_LISTEND("xhdr (cc)");

	return msginfo;
}

static GSList *news_fetch_msginfo_from_to(NNTPSession *session, FolderItem *item, guint begin, guint end)
{
	gchar buf[NNTPBUFSIZE];
	GSList *newlist = NULL;
	GSList *llast = NULL;
	MsgInfo *msginfo;

	g_return_val_if_fail(session != NULL, NULL);
	g_return_val_if_fail(item != NULL, NULL);

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

gint news_fetch_msgnum_sort(gconstpointer a, gconstpointer b)
{
	return (GPOINTER_TO_INT(a) - GPOINTER_TO_INT(b));
}

GSList *news_fetch_msginfos(Folder *folder, FolderItem *item, GSList *msgnum_list)
{
	NNTPSession *session;
	GSList *elem, *msginfo_list = NULL, *tmp_msgnum_list, *tmp_msginfo_list;
	guint first, last, next;
	
	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(folder->type == F_NEWS, NULL);
	g_return_val_if_fail(msgnum_list != NULL, NULL);
	g_return_val_if_fail(item != NULL, NULL);
	
	session = news_session_get(folder);
	g_return_val_if_fail(session != NULL, NULL);

	tmp_msgnum_list = g_slist_copy(msgnum_list);
	tmp_msgnum_list = g_slist_sort(tmp_msgnum_list, news_fetch_msgnum_sort);

	first = GPOINTER_TO_INT(tmp_msgnum_list->data);
	last = first;
	for(elem = g_slist_next(tmp_msgnum_list); elem != NULL; elem = g_slist_next(elem)) {
		next = GPOINTER_TO_INT(elem->data);
		if(next != (last + 1)) {
			tmp_msginfo_list = news_fetch_msginfo_from_to(session, item, first, last);
			msginfo_list = g_slist_concat(msginfo_list, tmp_msginfo_list);
			first = next;
		}
		last = next;
	}
	tmp_msginfo_list = news_fetch_msginfo_from_to(session, item, first, last);
	msginfo_list = g_slist_concat(msginfo_list, tmp_msginfo_list);

	g_slist_free(tmp_msgnum_list);
	
	return msginfo_list;
}
