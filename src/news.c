/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2004 Hiroyuki Yamamoto
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
#include <glib/gi18n.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <time.h>

#include "news.h"
#include "news_gtk.h"
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
#include "log.h"
#include "progressindicator.h"
#include "remotefolder.h"
#include "alertpanel.h"
#if USE_OPENSSL
#  include "ssl.h"
#endif

#define NNTP_PORT	119
#if USE_OPENSSL
#define NNTPS_PORT	563
#endif

typedef struct _NewsFolder	NewsFolder;

#define NEWS_FOLDER(obj)	((NewsFolder *)obj)

struct _NewsFolder
{
	RemoteFolder rfolder;

	gboolean use_auth;
};

static void news_folder_init		 (Folder	*folder,
					  const gchar	*name,
					  const gchar	*path);

static Folder	*news_folder_new	(const gchar	*name,
					 const gchar	*folder);
static void	 news_folder_destroy	(Folder		*folder);

static gchar *news_fetch_msg		(Folder		*folder,
					 FolderItem	*item,
					 gint		 num);


#if USE_OPENSSL
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
static MsgInfo *news_parse_xover	 (const gchar	*xover_str);
static gchar *news_parse_xhdr		 (const gchar	*xhdr_str,
					  MsgInfo	*msginfo);
gint news_get_num_list		 	 (Folder 	*folder, 
					  FolderItem 	*item,
					  GSList       **list,
					  gboolean	*old_uids_valid);
MsgInfo *news_get_msginfo		 (Folder 	*folder, 
					  FolderItem 	*item,
					  gint 		 num);
GSList *news_get_msginfos		 (Folder 	*folder,
					  FolderItem 	*item,
					  GSList 	*msgnum_list);
gboolean news_scan_required		 (Folder 	*folder,
					  FolderItem 	*item);

gint news_post_stream			 (Folder 	*folder, 
					  FILE 		*fp);
static gchar *news_folder_get_path	 (Folder	*folder);
gchar *news_item_get_path		 (Folder	*folder,
					  FolderItem	*item);

static FolderClass news_class;

FolderClass *news_get_class(void)
{
	if (news_class.idstr == NULL) {
		news_class.type = F_NEWS;
		news_class.idstr = "news";
		news_class.uistr = "News";

		/* Folder functions */
		news_class.new_folder = news_folder_new;
		news_class.destroy_folder = news_folder_destroy;

		/* FolderItem functions */
		news_class.item_get_path = news_item_get_path;
		news_class.get_num_list = news_get_num_list;
		news_class.scan_required = news_scan_required;

		/* Message functions */
		news_class.get_msginfo = news_get_msginfo;
		news_class.get_msginfos = news_get_msginfos;
		news_class.fetch_msg = news_fetch_msg;
	};

	return &news_class;
}

static Folder *news_folder_new(const gchar *name, const gchar *path)
{
	Folder *folder;

	folder = (Folder *)g_new0(NewsFolder, 1);
	folder->klass = &news_class;
	news_folder_init(folder, name, path);

	return folder;
}

static void news_folder_destroy(Folder *folder)
{
	gchar *dir;

	dir = news_folder_get_path(folder);
	if (is_dir_exist(dir))
		remove_dir_recursive(dir);
	g_free(dir);

	folder_remote_folder_destroy(REMOTE_FOLDER(folder));
}

static void news_folder_init(Folder *folder, const gchar *name,
			     const gchar *path)
{
	folder_remote_folder_init(folder, name, path);
}

#if USE_OPENSSL
static Session *news_session_new(const gchar *server, gushort port,
				 const gchar *userid, const gchar *passwd,
				 SSLType ssl_type)
#else
static Session *news_session_new(const gchar *server, gushort port,
				 const gchar *userid, const gchar *passwd)
#endif
{
	gchar buf[NNTPBUFSIZE];
	Session *session;

	g_return_val_if_fail(server != NULL, NULL);

	log_message(_("creating NNTP connection to %s:%d ...\n"), server, port);

#if USE_OPENSSL
	session = nntp_session_new(server, port, buf, userid, passwd, ssl_type);
#else
	session = nntp_session_new(server, port, buf, userid, passwd);
#endif

	return session;
}

static Session *news_session_new_for_folder(Folder *folder)
{
	Session *session;
	PrefsAccount *ac;
	const gchar *userid = NULL;
	gchar *passwd = NULL;
	gushort port;
	gchar buf[NNTPBUFSIZE];

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

#if USE_OPENSSL
	port = ac->set_nntpport ? ac->nntpport
		: ac->ssl_nntp ? NNTPS_PORT : NNTP_PORT;
	session = news_session_new(ac->nntp_server, port, userid, passwd,
				   ac->ssl_nntp);
#else
	port = ac->set_nntpport ? ac->nntpport : NNTP_PORT;
	session = news_session_new(ac->nntp_server, port, userid, passwd);
#endif
	if ((session != NULL) && ac->use_nntp_auth && ac->use_nntp_auth_onconnect)
		nntp_forceauth(NNTP_SESSION(session), buf, userid, passwd);

	g_free(passwd);

	return session;
}

static NNTPSession *news_session_get(Folder *folder)
{
	RemoteFolder *rfolder = REMOTE_FOLDER(folder);

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(FOLDER_CLASS(folder) == &news_class, NULL);
	g_return_val_if_fail(folder->account != NULL, NULL);

	if (prefs_common.work_offline && !news_gtk_should_override()) {
		return NULL;
	}

	if (!rfolder->session) {
		rfolder->session = news_session_new_for_folder(folder);
		return NNTP_SESSION(rfolder->session);
	}

	if (time(NULL) - rfolder->session->last_access_time <
		SESSION_TIMEOUT_INTERVAL) {
		return NNTP_SESSION(rfolder->session);
	}

	if (nntp_mode(NNTP_SESSION(rfolder->session), FALSE)
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
		session_set_access_time(rfolder->session);

	return NNTP_SESSION(rfolder->session);
}

static gchar *news_fetch_msg(Folder *folder, FolderItem *item, gint num)
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
	if (ok != NN_SUCCESS) {
		if (ok == NN_SOCKET) {
			session_destroy(SESSION(session));
			REMOTE_FOLDER(folder)->session = NULL;
		}
		g_free(filename);
		return NULL;
	}

	debug_print("getting article %d...\n", num);
	ok = news_get_article(NNTP_SESSION(REMOTE_FOLDER(folder)->session),
			      num, filename);
	if (ok != NN_SUCCESS) {
		g_warning("can't read article %d\n", num);
		if (ok == NN_SOCKET) {
			session_destroy(SESSION(session));
			REMOTE_FOLDER(folder)->session = NULL;
		}
		g_free(filename);
		return NULL;
	}

	return filename;
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
	return g_ascii_strcasecmp(ginfo1->name, ginfo2->name);
}

GSList *news_get_group_list(Folder *folder)
{
	gchar *path, *filename;
	FILE *fp;
	GSList *list = NULL;
	GSList *last = NULL;
	gchar buf[NNTPBUFSIZE];

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(FOLDER_CLASS(folder) == &news_class, NULL);

	path = folder_item_get_path(FOLDER_ITEM(folder->node->data));
	if (!is_dir_exist(path))
		make_dir_hier(path);
	filename = g_strconcat(path, G_DIR_SEPARATOR_S, NEWSGROUP_LIST, NULL);
	g_free(path);

	if ((fp = fopen(filename, "rb")) == NULL) {
		NNTPSession *session;
		gint ok;

		session = news_session_get(folder);
		if (!session) {
			g_free(filename);
			return NULL;
		}

		ok = nntp_list(session);
		if (ok != NN_SUCCESS) {
			if (ok == NN_SOCKET) {
				session_destroy(SESSION(session));
				REMOTE_FOLDER(folder)->session = NULL;
			}
			g_free(filename);
			return NULL;
		}
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
	g_return_if_fail(FOLDER_CLASS(folder) == &news_class);

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
	g_return_val_if_fail(FOLDER_CLASS(folder) == &news_class, -1);
	g_return_val_if_fail(file != NULL, -1);

	if ((fp = fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		return -1;
	}

	ok = news_post_stream(folder, fp);

	fclose(fp);

	return ok;
}

gint news_post_stream(Folder *folder, FILE *fp)
{
	NNTPSession *session;
	gint ok;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(FOLDER_CLASS(folder) == &news_class, -1);
	g_return_val_if_fail(fp != NULL, -1);

	session = news_session_get(folder);
	if (!session) return -1;

	ok = nntp_post(session, fp);
	if (ok != NN_SUCCESS) {
		log_warning(_("can't post article.\n"));
		if (ok == NN_SOCKET) {
			session_destroy(SESSION(session));
			REMOTE_FOLDER(folder)->session = NULL;
		}
		return -1;
	}

	return 0;
}

static gint news_get_article_cmd(NNTPSession *session, const gchar *cmd,
				 gint num, gchar *filename)
{
	gchar *msgid;
	gint ok;

	ok = nntp_get_article(session, cmd, num, &msgid);
	if (ok != NN_SUCCESS)
		return ok;

	debug_print("Message-ID = %s, num = %d\n", msgid, num);
	g_free(msgid);

	ok = recv_write_to_file(SESSION(session)->sock, filename);
	if (ok < 0) {
		log_warning(_("can't retrieve article %d\n"), num);
		if (ok == -2)
			return NN_SOCKET;
		else
			return NN_IOERR;
	}

	return NN_SUCCESS;
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
		if (session->group && g_ascii_strcasecmp(session->group, group) == 0)
			return NN_SUCCESS;
		num = &num_;
		first = &first_;
		last = &last_;
	}

	g_free(session->group);
	session->group = NULL;

	ok = nntp_group(session, group, num, first, last);
	if (ok == NN_SUCCESS)
		session->group = g_strdup(group);
	else
		log_warning(_("can't select group: %s\n"), group);

	return ok;
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
	/*
	 * PARSE_ONE_PARAM(xref, line);
	 *
         * if we parse extra headers we should first examine the
	 * LIST OVERVIEW.FMT response from the server. See
	 * RFC2980 for details
	 */

	tmp = strchr(line, '\t');
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

        msginfo->from = conv_unmime_header(sender, NULL);
	msginfo->fromname = procheader_get_fromname(msginfo->from);

        msginfo->subject = conv_unmime_header(subject, NULL);

        if (msgid) {
                extract_parenthesis(msgid, '<', '>');
                remove_space(msgid);
                if (*msgid != '\0')
                        msginfo->msgid = g_strdup(msgid);
        }                        

        /* FIXME: this is a quick fix; references' meaning was changed
         * into having the actual list of references in the References: header.
         * We need a GSList here, so msginfo_free() and msginfo_copy() can do 
         * their things properly. */ 
        if (ref && strlen(ref)) {       
		gchar **ref_tokens = g_strsplit(ref, " ", -1);
		guint i = 0;
		
		while (ref_tokens[i]) {
			gchar *cur_ref = ref_tokens[i];
			msginfo->references = references_list_append(msginfo->references, 
					g_strdup(cur_ref));
			i++;
		}
		g_strfreev(ref_tokens);
		
                eliminate_parenthesis(ref, '(', ')');
                if ((p = strrchr(ref, '<')) != NULL) {
                        extract_parenthesis(p, '<', '>');
                        remove_space(p);
                        if (*p != '\0')
                                msginfo->inreplyto = g_strdup(p);
                }
        } 

	/*
	msginfo->xref = g_strdup(xref);
	p = msginfo->xref+strlen(msginfo->xref) - 1;
	while (*p == '\r' || *p == '\n') {
		*p = '\0';
		p--;
	}
	*/

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
		g_warning("can't change file mode\n");
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

static gchar *news_folder_get_path(Folder *folder)
{
	gchar *folder_path;

        g_return_val_if_fail(folder->account != NULL, NULL);

        folder_path = g_strconcat(get_news_cache_dir(),
                                  G_DIR_SEPARATOR_S,
                                  folder->account->nntp_server,
                                  NULL);
	return folder_path;
}

gchar *news_item_get_path(Folder *folder, FolderItem *item)
{
	gchar *folder_path, *path;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(item != NULL, NULL);
	folder_path = news_folder_get_path(folder);

        g_return_val_if_fail(folder_path != NULL, NULL);
        if (folder_path[0] == G_DIR_SEPARATOR) {
                if (item->path)
                        path = g_strconcat(folder_path, G_DIR_SEPARATOR_S,
                                           item->path, NULL);
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

	return path;
}

gint news_get_num_list(Folder *folder, FolderItem *item, GSList **msgnum_list, gboolean *old_uids_valid)
{
	NNTPSession *session;
	gint i, ok, num, first, last, nummsgs = 0;
	gchar *dir;

	g_return_val_if_fail(item != NULL, -1);
	g_return_val_if_fail(item->folder != NULL, -1);
	g_return_val_if_fail(FOLDER_CLASS(folder) == &news_class, -1);

	session = news_session_get(folder);
	g_return_val_if_fail(session != NULL, -1);

	*old_uids_valid = TRUE;

	ok = news_select_group(session, item->path, &num, &first, &last);
	if (ok != NN_SUCCESS) {
		log_warning(_("can't set group: %s\n"), item->path);
		return -1;
	}

	dir = news_folder_get_path(folder);
	if (num <= 0)
		remove_all_numbered_files(dir);
	else if (last < first)
		log_warning(_("invalid article range: %d - %d\n"),
			    first, last);
	else {
		for (i = first; i <= last; i++) {
			*msgnum_list = g_slist_prepend(*msgnum_list, 
						       GINT_TO_POINTER(i));
			nummsgs++;
		}
		debug_print("removing old messages from %d to %d in %s\n",
			    first, last, dir);
		remove_numbered_files(dir, 1, first - 1);
	}
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

MsgInfo *news_get_msginfo(Folder *folder, FolderItem *item, gint num)
{
	NNTPSession *session;
	MsgInfo *msginfo = NULL;
	gchar buf[NNTPBUFSIZE];
	gint ok;

	session = news_session_get(folder);
	g_return_val_if_fail(session != NULL, NULL);
	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(item->folder != NULL, NULL);
	g_return_val_if_fail(FOLDER_CLASS(item->folder) == &news_class, NULL);

	log_message(_("getting xover %d in %s...\n"),
		    num, item->path);
	ok = nntp_xover(session, num, num);
	if (ok != NN_SUCCESS) {
		log_warning(_("can't get xover\n"));
		if (ok == NN_SOCKET) {
			session_destroy(SESSION(session));
			REMOTE_FOLDER(item->folder)->session = NULL;
		}
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

	ok = nntp_xhdr(session, "to", num, num);
	if (ok != NN_SUCCESS) {
		log_warning(_("can't get xhdr\n"));
		if (ok == NN_SOCKET) {
			session_destroy(SESSION(session));
			REMOTE_FOLDER(item->folder)->session = NULL;
		}
		return msginfo;
	}

	if (sock_gets(SESSION(session)->sock, buf, sizeof(buf)) < 0) {
		log_warning(_("error occurred while getting xhdr.\n"));
		return msginfo;
	}

	msginfo->to = news_parse_xhdr(buf, msginfo);

	READ_TO_LISTEND("xhdr (to)");

	ok = nntp_xhdr(session, "cc", num, num);
	if (ok != NN_SUCCESS) {
		log_warning(_("can't get xhdr\n"));
		if (ok == NN_SOCKET) {
			session_destroy(SESSION(session));
			REMOTE_FOLDER(item->folder)->session = NULL;
		}
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

static GSList *news_get_msginfos_for_range(NNTPSession *session, FolderItem *item, guint begin, guint end)
{
	gchar buf[NNTPBUFSIZE];
	GSList *newlist = NULL;
	GSList *llast = NULL;
	MsgInfo *msginfo;
	guint count = 0, lines = (end - begin + 2) * 3;
	gint ok;

	g_return_val_if_fail(session != NULL, NULL);
	g_return_val_if_fail(item != NULL, NULL);

	log_message(_("getting xover %d - %d in %s...\n"),
		    begin, end, item->path);
	ok = nntp_xover(session, begin, end);
	if (ok != NN_SUCCESS) {
		log_warning(_("can't get xover\n"));
		if (ok == NN_SOCKET) {
			session_destroy(SESSION(session));
			REMOTE_FOLDER(item->folder)->session = NULL;
		}
		return NULL;
	}

	for (;;) {
		if (sock_gets(SESSION(session)->sock, buf, sizeof(buf)) < 0) {
			log_warning(_("error occurred while getting xover.\n"));
			return newlist;
		}
		count++;
		progressindicator_set_percentage
			(PROGRESS_TYPE_NETWORK,
			 session->fetch_base_percentage +
			 (((gfloat) count) / ((gfloat) lines)) * session->fetch_total_percentage);

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

	ok = nntp_xhdr(session, "to", begin, end);
	if (ok != NN_SUCCESS) {
		log_warning(_("can't get xhdr\n"));
		if (ok == NN_SOCKET) {
			session_destroy(SESSION(session));
			REMOTE_FOLDER(item->folder)->session = NULL;
		}
		return newlist;
	}

	llast = newlist;

	for (;;) {
		if (sock_gets(SESSION(session)->sock, buf, sizeof(buf)) < 0) {
			log_warning(_("error occurred while getting xhdr.\n"));
			return newlist;
		}
		count++;
		progressindicator_set_percentage
			(PROGRESS_TYPE_NETWORK,
			 session->fetch_base_percentage +
			 (((gfloat) count) / ((gfloat) lines)) * session->fetch_total_percentage);

		if (buf[0] == '.' && buf[1] == '\r') break;
		if (!llast) {
			g_warning("llast == NULL\n");
			continue;
		}

		msginfo = (MsgInfo *)llast->data;
		msginfo->to = news_parse_xhdr(buf, msginfo);

		llast = llast->next;
	}

	ok = nntp_xhdr(session, "cc", begin, end);
	if (ok != NN_SUCCESS) {
		log_warning(_("can't get xhdr\n"));
		if (ok == NN_SOCKET) {
			session_destroy(SESSION(session));
			REMOTE_FOLDER(item->folder)->session = NULL;
		}
		return newlist;
	}

	llast = newlist;

	for (;;) {
		if (sock_gets(SESSION(session)->sock, buf, sizeof(buf)) < 0) {
			log_warning(_("error occurred while getting xhdr.\n"));
			return newlist;
		}
		count++;
		progressindicator_set_percentage
			(PROGRESS_TYPE_NETWORK,
			 session->fetch_base_percentage +
			 (((gfloat) count) / ((gfloat) lines)) * session->fetch_total_percentage);

		if (buf[0] == '.' && buf[1] == '\r') break;
		if (!llast) {
			g_warning("llast == NULL\n");
			continue;
		}

		msginfo = (MsgInfo *)llast->data;
		msginfo->cc = news_parse_xhdr(buf, msginfo);

		llast = llast->next;
	}

	session_set_access_time(SESSION(session));

	return newlist;
}

GSList *news_get_msginfos(Folder *folder, FolderItem *item, GSList *msgnum_list)
{
	NNTPSession *session;
	GSList *elem, *msginfo_list = NULL, *tmp_msgnum_list, *tmp_msginfo_list;
	guint first, last, next;
	guint tofetch, fetched;
	
	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(FOLDER_CLASS(folder) == &news_class, NULL);
	g_return_val_if_fail(msgnum_list != NULL, NULL);
	g_return_val_if_fail(item != NULL, NULL);
	
	session = news_session_get(folder);
	g_return_val_if_fail(session != NULL, NULL);

	tmp_msgnum_list = g_slist_copy(msgnum_list);
	tmp_msgnum_list = g_slist_sort(tmp_msgnum_list, g_int_compare);

	progressindicator_start(PROGRESS_TYPE_NETWORK);
	tofetch = g_slist_length(tmp_msgnum_list);
	fetched = 0;

	first = GPOINTER_TO_INT(tmp_msgnum_list->data);
	last = first;
	for(elem = g_slist_next(tmp_msgnum_list); elem != NULL; elem = g_slist_next(elem)) {
		next = GPOINTER_TO_INT(elem->data);
		if(next != (last + 1)) {
			session->fetch_base_percentage = ((gfloat) fetched) / ((gfloat) tofetch);
			session->fetch_total_percentage = ((gfloat) (last - first + 1)) / ((gfloat) tofetch);
			tmp_msginfo_list = news_get_msginfos_for_range(session, item, first, last);
			msginfo_list = g_slist_concat(msginfo_list, tmp_msginfo_list);
			fetched = last - first + 1;
			first = next;
		}
		last = next;
	}
	session->fetch_base_percentage = ((gfloat) fetched) / ((gfloat) tofetch);
	session->fetch_total_percentage = ((gfloat) (last - first + 1)) / ((gfloat) tofetch);
	tmp_msginfo_list = news_get_msginfos_for_range(session, item, first, last);
	msginfo_list = g_slist_concat(msginfo_list, tmp_msginfo_list);

	g_slist_free(tmp_msgnum_list);
	
	progressindicator_stop(PROGRESS_TYPE_NETWORK);

	return msginfo_list;
}

gboolean news_scan_required(Folder *folder, FolderItem *item)
{
	return TRUE;
}
