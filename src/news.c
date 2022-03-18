/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2022 the Claws Mail team and Hiroyuki Yamamoto
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 * 
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#ifdef HAVE_LIBETPAN

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <time.h>
#include <libetpan/libetpan.h>

#include "nntp-thread.h"
#include "news.h"
#include "news_gtk.h"
#include "socket.h"
#include "recv.h"
#include "procmsg.h"
#include "procheader.h"
#include "folder.h"
#include "session.h"
#include "statusbar.h"
#include "codeconv.h"
#include "utils.h"
#include "passwordstore.h"
#include "prefs_common.h"
#include "prefs_account.h"
#include "inputdialog.h"
#include "log.h"
#include "progressindicator.h"
#include "remotefolder.h"
#include "alertpanel.h"
#include "inc.h"
#include "account.h"
#ifdef USE_GNUTLS
#  include "ssl.h"
#endif
#include "main.h"
#include "file-utils.h"

#define NNTP_PORT	119
#ifdef USE_GNUTLS
#define NNTPS_PORT	563
#endif

typedef struct _NewsFolder	NewsFolder;
typedef struct _NewsSession	NewsSession;

#define NEWS_FOLDER(obj)	((NewsFolder *)obj)
#define NEWS_SESSION(obj)       ((NewsSession *)obj)

struct _NewsFolder
{
	RemoteFolder rfolder;

	gboolean use_auth;
	gboolean lock_count;
	guint refcnt;
};

struct _NewsSession
{
	Session session;
	Folder *folder;
	gchar *group;
};

static void news_folder_init(Folder *folder, const gchar *name,
			     const gchar *path);

static Folder	*news_folder_new	(const gchar	*name,
					 const gchar	*folder);
static void	 news_folder_destroy	(Folder		*folder);

static gchar *news_fetch_msg		(Folder		*folder,
					 FolderItem	*item,
					 gint		 num);
static void news_remove_cached_msg	(Folder 	*folder, 
					 FolderItem 	*item, 
					 MsgInfo 	*msginfo);
#ifdef USE_GNUTLS
static Session *news_session_new	 (Folder		*folder,
					  const PrefsAccount 	*account,
					  gushort		 port,
					  SSLType		 ssl_type);
#else
static Session *news_session_new	 (Folder		*folder,
					  const PrefsAccount 	*account,
					  gushort		 port);
#endif

static gint news_get_article		 (Folder	*folder,
					  gint		 num,
					  gchar		*filename);

static gint news_select_group		 (Folder	*folder,
					  const gchar	*group,
					  gint		*num,
					  gint		*first,
					  gint		*last);
static MsgInfo *news_parse_xover	 (struct newsnntp_xover_resp_item *item);
static gint news_get_num_list		 	 (Folder 	*folder, 
					  FolderItem 	*item,
					  GSList       **list,
					  gboolean	*old_uids_valid);
static MsgInfo *news_get_msginfo		 (Folder 	*folder, 
					  FolderItem 	*item,
					  gint 		 num);
static GSList *news_get_msginfos		 (Folder 	*folder,
					  FolderItem 	*item,
					  GSList 	*msgnum_list);
static gboolean news_scan_required		 (Folder 	*folder,
					  FolderItem 	*item);

static gchar *news_folder_get_path	 (Folder	*folder);
static gchar *news_item_get_path		 (Folder	*folder,
					  FolderItem	*item);
static void news_synchronise		 (FolderItem	*item, gint days);
static int news_remove_msg		 (Folder 	*folder, 
					  FolderItem 	*item, 
					  gint 		 msgnum);
static gint news_rename_folder		 (Folder *folder,
					  FolderItem *item,
					  const gchar *name);
static gint news_remove_folder		 (Folder	*folder,
					  FolderItem	*item);
static FolderClass news_class;

FolderClass *news_get_class(void)
{
	if (news_class.idstr == NULL) {
		news_class.type = F_NEWS;
		news_class.idstr = "news";
		news_class.uistr = "News";
		news_class.supports_server_search = FALSE;

		/* Folder functions */
		news_class.new_folder = news_folder_new;
		news_class.destroy_folder = news_folder_destroy;

		/* FolderItem functions */
		news_class.item_get_path = news_item_get_path;
		news_class.get_num_list = news_get_num_list;
		news_class.scan_required = news_scan_required;
		news_class.rename_folder = news_rename_folder;
		news_class.remove_folder = news_remove_folder;

		/* Message functions */
		news_class.get_msginfo = news_get_msginfo;
		news_class.get_msginfos = news_get_msginfos;
		news_class.fetch_msg = news_fetch_msg;
		news_class.synchronise = news_synchronise;
		news_class.search_msgs = folder_item_search_msgs_local;
		news_class.remove_msg = news_remove_msg;
		news_class.remove_cached_msg = news_remove_cached_msg;
	};

	return &news_class;
}

guint nntp_folder_get_refcnt(Folder *folder)
{
	return ((NewsFolder *)folder)->refcnt;
}

void nntp_folder_ref(Folder *folder)
{
	((NewsFolder *)folder)->refcnt++;
}

void nntp_folder_unref(Folder *folder)
{
	if (((NewsFolder *)folder)->refcnt > 0)
		((NewsFolder *)folder)->refcnt--;
}

static int news_remove_msg		 (Folder 	*folder, 
					  FolderItem 	*item, 
					  gint 		 msgnum)
{
	gchar *path, *filename;

	cm_return_val_if_fail(folder != NULL, -1);
	cm_return_val_if_fail(item != NULL, -1);

	path = folder_item_get_path(item);
	if (!is_dir_exist(path))
		make_dir_hier(path);
	
	filename = g_strconcat(path, G_DIR_SEPARATOR_S, itos(msgnum), NULL);
	g_free(path);
	claws_unlink(filename);
	g_free(filename);
	return 0;
}

static void news_folder_lock(NewsFolder *folder)
{
	folder->lock_count++;
}

static void news_folder_unlock(NewsFolder *folder)
{
	if (folder->lock_count > 0)
		folder->lock_count--;
}

int news_folder_locked(Folder *folder)
{
	if (folder == NULL)
		return 0;

	return NEWS_FOLDER(folder)->lock_count;
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

	while (nntp_folder_get_refcnt(folder) > 0)
		gtk_main_iteration();

	dir = news_folder_get_path(folder);
	if (is_dir_exist(dir))
		remove_dir_recursive(dir);
	g_free(dir);

	nntp_done(folder);
	folder_remote_folder_destroy(REMOTE_FOLDER(folder));
}

static void news_folder_init(Folder *folder, const gchar *name,
			     const gchar *path)
{
	folder_remote_folder_init(folder, name, path);
}

static void news_session_destroy(Session *session)
{
	NewsSession *news_session = NEWS_SESSION(session);

	cm_return_if_fail(session != NULL);

	if (news_session->group)
		g_free(news_session->group);
}

static gboolean nntp_ping(gpointer data)
{
	Session *session = (Session *)data;
	NewsSession *news_session = NEWS_SESSION(session);
	int r;
	struct tm lt;

	if (session->state != SESSION_READY || news_folder_locked(news_session->folder))
		return FALSE;
	
	news_folder_lock(NEWS_FOLDER(news_session->folder));

	if ((r = nntp_threaded_date(news_session->folder, &lt)) != NEWSNNTP_NO_ERROR) {
		if (r != NEWSNNTP_ERROR_COMMAND_NOT_SUPPORTED &&
		    r != NEWSNNTP_ERROR_COMMAND_NOT_UNDERSTOOD) {
			log_warning(LOG_PROTOCOL, _("NNTP connection to %s:%d has been"
			      " disconnected.\n"),
			    news_session->folder->account->nntp_server,
			    news_session->folder->account->set_nntpport ?
			    news_session->folder->account->nntpport : NNTP_PORT);
			REMOTE_FOLDER(news_session->folder)->session = NULL;
			news_folder_unlock(NEWS_FOLDER(news_session->folder));
			session->state = SESSION_DISCONNECTED;
			session->sock = NULL;
			session_destroy(session);
			return FALSE;
		}
	}

	news_folder_unlock(NEWS_FOLDER(news_session->folder));
	session_set_access_time(session);
	return TRUE;
}


#ifdef USE_GNUTLS
static Session *news_session_new(Folder *folder, const PrefsAccount *account, gushort port,
				 SSLType ssl_type)
#else
static Session *news_session_new(Folder *folder, const PrefsAccount *account, gushort port)
#endif
{
	NewsSession *session;
	const char *server = account->nntp_server;
	int r = 0;
	ProxyInfo *proxy_info = NULL;

	cm_return_val_if_fail(server != NULL, NULL);

	log_message(LOG_PROTOCOL,
			_("Account '%s': Connecting to NNTP server: %s:%d...\n"),
			folder->account->account_name, server, port);

	session = g_new0(NewsSession, 1);
	session_init(SESSION(session), folder->account, FALSE);
	SESSION(session)->type             = SESSION_NEWS;
	SESSION(session)->server           = g_strdup(server);
	SESSION(session)->port             = port;
 	SESSION(session)->sock             = NULL;
	SESSION(session)->destroy          = news_session_destroy;

	if (account->use_proxy) {
		if (account->use_default_proxy) {
			proxy_info = (ProxyInfo *)&(prefs_common.proxy_info);
			if (proxy_info->use_proxy_auth)
				proxy_info->proxy_pass = passwd_store_get(PWS_CORE, PWS_CORE_PROXY,
					PWS_CORE_PROXY_PASS);
		} else {
			proxy_info = (ProxyInfo *)&(account->proxy_info);
			if (proxy_info->use_proxy_auth)
				proxy_info->proxy_pass = passwd_store_get_account(account->account_id,
					PWS_ACCOUNT_PROXY_PASS);
		}
	}
	SESSION(session)->proxy_info = proxy_info;

	nntp_init(folder);

#ifdef USE_GNUTLS
	SESSION(session)->use_tls_sni = account->use_tls_sni;
	if (ssl_type != SSL_NONE)
		r = nntp_threaded_connect_ssl(folder, server, port, proxy_info);
	else
#endif
		r = nntp_threaded_connect(folder, server, port, proxy_info);
	
	if (r != NEWSNNTP_NO_ERROR) {
		log_error(LOG_PROTOCOL, _("Error logging in to %s:%d...\n"), server, port);
		session_destroy(SESSION(session));
		return NULL;
	}

	session->folder = folder;

	return SESSION(session);
}

static Session *news_session_new_for_folder(Folder *folder)
{
	Session *session;
	PrefsAccount *ac;
	const gchar *userid = NULL;
	gchar *passwd = NULL;
	gushort port;
	int r;

	cm_return_val_if_fail(folder != NULL, NULL);
	cm_return_val_if_fail(folder->account != NULL, NULL);

	ac = folder->account;

#ifdef USE_GNUTLS
	port = ac->set_nntpport ? ac->nntpport
		: ac->ssl_nntp ? NNTPS_PORT : NNTP_PORT;
	session = news_session_new(folder, ac, port, ac->ssl_nntp);
#else
	if (ac->ssl_nntp != SSL_NONE) {
		if (alertpanel_full(_("Insecure connection"),
			_("This connection is configured to be secured "
			  "using TLS, but TLS is not available "
			  "in this build of Claws Mail. \n\n"
			  "Do you want to continue connecting to this "
			  "server? The communication would not be "
			  "secure."),
			  NULL, _("_Cancel"), NULL, _("Con_tinue connecting"),
			  NULL, NULL, ALERTFOCUS_FIRST, FALSE, NULL, ALERT_WARNING) != G_ALERTALTERNATE)
			return NULL;
	}
	port = ac->set_nntpport ? ac->nntpport : NNTP_PORT;
	session = news_session_new(folder, ac, port);
#endif

	if (ac->use_nntp_auth && ac->userid && ac->userid[0]) {
		userid = ac->userid;
		if (password_get(userid, ac->nntp_server, "nntp", port, &passwd)) {
			/* NOP */;
		} else if ((passwd = passwd_store_get_account(ac->account_id,
					PWS_ACCOUNT_RECV)) == NULL) {
			passwd = input_dialog_query_password_keep(ac->nntp_server,
								  userid,
								  &(ac->session_passwd));
		}
	}

	if (session != NULL)
		r = nntp_threaded_mode_reader(folder);
	else
		r = NEWSNNTP_ERROR_CONNECTION_REFUSED;

	if (r != NEWSNNTP_NO_ERROR) {
	    if (r == NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_USERNAME) {
	        /*
	           FIX ME when libetpan implements 480 to indicate authorization
	           is required to use this capability. Libetpan treats a 480 as a
	           381 which is clearly wrong.
	           RFC 4643 section 2.
	           Response code 480
	           Generic response
	           Meaning: command unavailable until the client
	           has authenticated itself.
	        */
		/* if the server does not advertise the capability MODE-READER,
		   we normally should not send MODE READER. However this can't
		   hurt: a transit-only server returns 502 and closes the cnx.
		   Ref.: http://tools.ietf.org/html/rfc3977#section-5.3
		*/
	        log_error(LOG_PROTOCOL, _("Libetpan does not support return code 480 "
	        "so for now we choose to continue\n"));
	    }
	    else if (r == NEWSNNTP_ERROR_UNEXPECTED_RESPONSE) {
		/* if the server does not advertise the capability MODE-READER,
		   we normally should not send MODE READER. However this can't
		   hurt: a transit-only server returns 502 and closes the cnx.
		   Ref.: http://tools.ietf.org/html/rfc3977#section-5.3
		*/
		log_error(LOG_PROTOCOL, _("Mode reader failed, continuing nevertheless\n")); 
	    }
	    else {
	        /* An error state bail out */
	        log_error(LOG_PROTOCOL, _("Error creating session with %s:%d\n"), ac->nntp_server, port);
		if (session != NULL)
			session_destroy(SESSION(session));
		g_free(passwd);
		if (ac->session_passwd) {
			g_free(ac->session_passwd);
			ac->session_passwd = NULL;
		}
		return NULL;
	    }
	}

	if ((session != NULL) && ac->use_nntp_auth) { /* FIXME:  && ac->use_nntp_auth_onconnect */
		if (nntp_threaded_login(folder, userid, passwd) !=
			NEWSNNTP_NO_ERROR) {
			log_error(LOG_PROTOCOL, _("Error authenticating to %s:%d...\n"), ac->nntp_server, port);
			session_destroy(SESSION(session));
			g_free(passwd);
			if (ac->session_passwd) {
				g_free(ac->session_passwd);
				ac->session_passwd = NULL;
			}
			return NULL;
		}
	}
	g_free(passwd);

	return session;
}

static NewsSession *news_session_get(Folder *folder)
{
	RemoteFolder *rfolder = REMOTE_FOLDER(folder);
	
	cm_return_val_if_fail(folder != NULL, NULL);
	cm_return_val_if_fail(FOLDER_CLASS(folder) == &news_class, NULL);
	cm_return_val_if_fail(folder->account != NULL, NULL);

	if (prefs_common.work_offline && 
	    !inc_offline_should_override(FALSE,
		_("Claws Mail needs network access in order "
		  "to access the News server."))) {
		return NULL;
	}

	if (!rfolder->session) {
		rfolder->session = news_session_new_for_folder(folder);
		session_register_ping(SESSION(rfolder->session), nntp_ping);
		return NEWS_SESSION(rfolder->session);
	}

	/* Handle port change (also ssl/nossl change) without needing to
	 * restart application. */
	if (rfolder->session->port != folder->account->nntpport) {
		session_destroy(rfolder->session);
		rfolder->session = news_session_new_for_folder(folder);
		session_register_ping(SESSION(rfolder->session), nntp_ping);
		goto newsession;
	}
	
	if (time(NULL) - rfolder->session->last_access_time <
		SESSION_TIMEOUT_INTERVAL) {
		return NEWS_SESSION(rfolder->session);
	}

	if (!nntp_ping(rfolder->session)) {
		rfolder->session = news_session_new_for_folder(folder);
		session_register_ping(SESSION(rfolder->session), nntp_ping);
	}

newsession:
	if (rfolder->session)
		session_set_access_time(rfolder->session);

	return NEWS_SESSION(rfolder->session);
}

static void news_remove_cached_msg(Folder *folder, FolderItem *item, MsgInfo *msginfo)
{
	gchar *path, *filename;

	path = folder_item_get_path(item);

	if (!is_dir_exist(path)) {
		g_free(path);
		return;
	}

	filename = g_strconcat(path, G_DIR_SEPARATOR_S, itos(msginfo->msgnum), NULL);
	g_free(path);

	if (is_file_exist(filename)) {
		claws_unlink(filename);
	}
	g_free(filename);
}

static gchar *news_fetch_msg(Folder *folder, FolderItem *item, gint num)
{
	gchar *path, *filename;
	NewsSession *session;
	gint ok;

	cm_return_val_if_fail(folder != NULL, NULL);
	cm_return_val_if_fail(item != NULL, NULL);

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

	ok = news_select_group(folder, item->path, NULL, NULL, NULL);
	if (ok != NEWSNNTP_NO_ERROR) {
		if (ok == NEWSNNTP_ERROR_STREAM) {
			session_destroy(SESSION(session));
			REMOTE_FOLDER(folder)->session = NULL;
		}
		g_free(filename);
		return NULL;
	}

	debug_print("getting article %d...\n", num);
	ok = news_get_article(folder,
			      num, filename);
	if (ok != NEWSNNTP_NO_ERROR) {
		g_warning("can't read article %d", num);
		if (ok == NEWSNNTP_ERROR_STREAM) {
			session_destroy(SESSION(session));
			REMOTE_FOLDER(folder)->session = NULL;
		}
		g_free(filename);
		return NULL;
	}
	GTK_EVENTS_FLUSH();
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
	gchar buf[BUFFSIZE];

	cm_return_val_if_fail(folder != NULL, NULL);
	cm_return_val_if_fail(FOLDER_CLASS(folder) == &news_class, NULL);

	path = folder_item_get_path(FOLDER_ITEM(folder->node->data));
	if (!is_dir_exist(path))
		make_dir_hier(path);
	filename = g_strconcat(path, G_DIR_SEPARATOR_S, NEWSGROUP_LIST, NULL);
	g_free(path);

	if ((fp = claws_fopen(filename, "rb")) == NULL) {
		NewsSession *session;
		gint ok;
		clist *grouplist = NULL;
		clistiter *cur;
		fp = claws_fopen(filename, "wb");
		
		if (!fp) {
			g_free(filename);
			return NULL;
		}
		session = news_session_get(folder);
		if (!session) {
			claws_fclose(fp);
			g_free(filename);
			return NULL;
		}

		ok = nntp_threaded_list(folder, &grouplist);
		
		if (ok != NEWSNNTP_NO_ERROR) {
			if (ok == NEWSNNTP_ERROR_STREAM) {
				session_destroy(SESSION(session));
				REMOTE_FOLDER(folder)->session = NULL;
			}
			claws_fclose(fp);
			g_free(filename);
			return NULL;
		}
		
		if (grouplist) {
			for (cur = clist_begin(grouplist); cur; cur = clist_next(cur)) {
				struct newsnntp_group_info *info = (struct newsnntp_group_info *)
									clist_content(cur);
				if (fprintf(fp, "%s %d %d %c\n",
					info->grp_name,
					info->grp_last,
					info->grp_first,
					info->grp_type) < 0) {
					log_error(LOG_PROTOCOL, ("Can't write newsgroup list\n"));
					session_destroy(SESSION(session));
					REMOTE_FOLDER(folder)->session = NULL;
					claws_fclose(fp);
					g_free(filename);
					newsnntp_list_free(grouplist);
					return NULL;
				}
			}
			newsnntp_list_free(grouplist);
		}
		if (claws_safe_fclose(fp) == EOF) {
			log_error(LOG_PROTOCOL, ("Can't write newsgroup list\n"));
			session_destroy(SESSION(session));
			REMOTE_FOLDER(folder)->session = NULL;
			g_free(filename);
			return NULL;
		}

		if ((fp = claws_fopen(filename, "rb")) == NULL) {
			FILE_OP_ERROR(filename, "claws_fopen");
			g_free(filename);
			return NULL;
		}
	}

	while (claws_fgets(buf, sizeof(buf), fp) != NULL) {
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

	claws_fclose(fp);
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

	cm_return_if_fail(folder != NULL);
	cm_return_if_fail(FOLDER_CLASS(folder) == &news_class);

	path = folder_item_get_path(FOLDER_ITEM(folder->node->data));
	filename = g_strconcat(path, G_DIR_SEPARATOR_S, NEWSGROUP_LIST, NULL);
	g_free(path);

	if (is_file_exist(filename)) {
		if (claws_unlink(filename) < 0)
			FILE_OP_ERROR(filename, "remove");
	}
	g_free(filename);
}

gint news_post(Folder *folder, const gchar *file)
{
	gint ok;
	char *contents = file_read_to_str_no_recode(file);
	NewsSession *session;

	cm_return_val_if_fail(folder != NULL, -1);
	cm_return_val_if_fail(FOLDER_CLASS(folder) == &news_class, -1);
	cm_return_val_if_fail(contents != NULL, -1);
	
	session = news_session_get(folder);
	if (!session)  {
		g_free(contents);
		return -1;
	}
	
	ok = nntp_threaded_post(folder, contents, strlen(contents));

	g_free(contents);

	if (ok == NEWSNNTP_ERROR_STREAM) {
		session_destroy(SESSION(session));
		REMOTE_FOLDER(folder)->session = NULL;
	}

	return (ok == NEWSNNTP_NO_ERROR ? 0 : -1);
}

static gint news_get_article(Folder *folder, gint num, gchar *filename)
{
	size_t len;
	char *result = NULL;
	int r;
	
	r = nntp_threaded_article(folder, num, &result, &len);
	
	if (r == NEWSNNTP_NO_ERROR) {
		if (str_write_to_file(result, filename, FALSE) < 0) {
			mmap_string_unref(result);
			return -1;
		}
		mmap_string_unref(result);
	}
	
	return r;
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
static gint news_select_group(Folder *folder, const gchar *group,
			      gint *num, gint *first, gint *last)
{
	gint ok;
	gint num_, first_, last_;
	struct newsnntp_group_info *info = NULL;
	NewsSession *session = NEWS_SESSION(news_session_get(folder));

	cm_return_val_if_fail(session != NULL, -1);
	
	if (!num || !first || !last) {
		if (session->group && g_ascii_strcasecmp(session->group, group) == 0)
			return NEWSNNTP_NO_ERROR;
		num = &num_;
		first = &first_;
		last = &last_;
	}

	g_free(session->group);
	session->group = NULL;

	ok = nntp_threaded_group(folder, group, &info);
	
	if (ok == NEWSNNTP_NO_ERROR && info) {
		session->group = g_strdup(group);
		*num = info->grp_first;
		*first = info->grp_first;
		*last = info->grp_last;
		newsnntp_group_free(info);
	} else {
		log_warning(LOG_PROTOCOL, _("couldn't select group: %s\n"), group);
	}
	return ok;
}

static MsgInfo *news_parse_xover(struct newsnntp_xover_resp_item *item)
{
	MsgInfo *msginfo;

	/* set MsgInfo */
	msginfo = procmsg_msginfo_new();
	msginfo->msgnum = item->ovr_article;
	msginfo->size = item->ovr_size;

	msginfo->date = g_strdup(item->ovr_date);
	msginfo->date_t = procheader_date_parse(NULL, item->ovr_date, 0);

	msginfo->from = conv_unmime_header(item->ovr_author, NULL, TRUE);
	msginfo->fromname = procheader_get_fromname(msginfo->from);

	msginfo->subject = conv_unmime_header(item->ovr_subject, NULL, TRUE);

	remove_return(msginfo->from);
	remove_return(msginfo->fromname);
	remove_return(msginfo->subject);

        if (item->ovr_message_id) {
		gchar *tmp = g_strdup(item->ovr_message_id);
                extract_parenthesis(tmp, '<', '>');
                remove_space(tmp);
                if (*tmp != '\0')
                        msginfo->msgid = g_strdup(tmp);
		g_free(tmp);
        }                        

        /* FIXME: this is a quick fix; references' meaning was changed
         * into having the actual list of references in the References: header.
         * We need a GSList here, so msginfo_free() and msginfo_copy() can do 
         * their things properly. */ 
        if (item->ovr_references && *(item->ovr_references)) {	 
		gchar **ref_tokens = g_strsplit(item->ovr_references, " ", -1);
		guint i = 0;
		char *tmp;
		char *p;
		while (ref_tokens[i]) {
			gchar *cur_ref = ref_tokens[i];
			msginfo->references = references_list_append(msginfo->references, 
					cur_ref);
			i++;
		}
		g_strfreev(ref_tokens);
		
		tmp = g_strdup(item->ovr_references);
                eliminate_parenthesis(tmp, '(', ')');
                if ((p = strrchr(tmp, '<')) != NULL) {
                        extract_parenthesis(p, '<', '>');
                        remove_space(p);
                        if (*p != '\0')
                                msginfo->inreplyto = g_strdup(p);
                }
		g_free(tmp);
	} 

	return msginfo;
}

gint news_cancel_article(Folder * folder, MsgInfo * msginfo)
{
	gchar * tmp;
	FILE * tmpfp;
	gchar date[RFC822_DATE_BUFFSIZE];

	tmp = g_strdup_printf("%s%ccancel%p", get_tmp_dir(),
			      G_DIR_SEPARATOR, msginfo);
	if (tmp == NULL)
		return -1;

	if ((tmpfp = claws_fopen(tmp, "wb")) == NULL) {
		FILE_OP_ERROR(tmp, "claws_fopen");
		g_free(tmp);
		return -1;
	}
	if (change_file_mode_rw(tmpfp, tmp) < 0) {
		FILE_OP_ERROR(tmp, "chmod");
		g_warning("can't change file mode");
	}
	
	if (prefs_common.hide_timezone)
		get_rfc822_date_hide_tz(date, sizeof(date));
	else
		get_rfc822_date(date, sizeof(date));
	if (fprintf(tmpfp, "From: %s\r\n"
		       "Newsgroups: %s\r\n"
		       "Subject: cmsg cancel <%s>\r\n"
		       "Control: cancel <%s>\r\n"
		       "Approved: %s\r\n"
		       "X-Cancelled-by: %s\r\n"
		       "Date: %s\r\n"
		       "\r\n"
		       "removed with Claws Mail\r\n",
		       msginfo->from,
		       msginfo->newsgroups,
		       msginfo->msgid,
		       msginfo->msgid,
		       msginfo->from,
		       msginfo->from,
		       date) < 0) {
		FILE_OP_ERROR(tmp, "fprintf");
		claws_fclose(tmpfp);
		claws_unlink(tmp);
		g_free(tmp);
		return -1;
	}

	if (claws_safe_fclose(tmpfp) == EOF) {
		FILE_OP_ERROR(tmp, "claws_fclose");
		claws_unlink(tmp);
		g_free(tmp);
		return -1;
	}

	news_post(folder, tmp);
	claws_unlink(tmp);

	g_free(tmp);

	return 0;
}

static gchar *news_folder_get_path(Folder *folder)
{
	gchar *folder_path;

        cm_return_val_if_fail(folder->account != NULL, NULL);

        folder_path = g_strconcat(get_news_cache_dir(),
                                  G_DIR_SEPARATOR_S,
                                  folder->account->nntp_server,
                                  NULL);
	return folder_path;
}

static gchar *news_item_get_path(Folder *folder, FolderItem *item)
{
	gchar *folder_path, *path;

	cm_return_val_if_fail(folder != NULL, NULL);
	cm_return_val_if_fail(item != NULL, NULL);
	folder_path = news_folder_get_path(folder);

        cm_return_val_if_fail(folder_path != NULL, NULL);
        if (g_path_is_absolute(folder_path)) {
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
#ifdef G_OS_WIN32
	while (strchr(path, '/'))
		*strchr(path, '/') = '\\';
#endif
	return path;
}

static gint news_get_num_list(Folder *folder, FolderItem *item, GSList **msgnum_list, gboolean *old_uids_valid)
{
	NewsSession *session;
	gint i, ok, num, first, last, nummsgs = 0;
	gchar *dir;

	cm_return_val_if_fail(item != NULL, -1);
	cm_return_val_if_fail(item->folder != NULL, -1);
	cm_return_val_if_fail(FOLDER_CLASS(folder) == &news_class, -1);

	session = news_session_get(folder);
	cm_return_val_if_fail(session != NULL, -1);

	*old_uids_valid = TRUE;
	
	news_folder_lock(NEWS_FOLDER(item->folder));

	ok = news_select_group(folder, item->path, &num, &first, &last);
	if (ok != NEWSNNTP_NO_ERROR) {
		log_warning(LOG_PROTOCOL, _("couldn't set group: %s\n"), item->path);
		news_folder_unlock(NEWS_FOLDER(item->folder));
		return -1;
	}

	dir = news_folder_get_path(folder);
	if (num <= 0)
		remove_all_numbered_files(dir);
	else if (last < first)
		log_warning(LOG_PROTOCOL, _("invalid article range: %d - %d\n"),
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
	news_folder_unlock(NEWS_FOLDER(item->folder));
	return nummsgs;
}

static void news_set_msg_flags(FolderItem *item, MsgInfo *msginfo)
{
	msginfo->flags.tmp_flags = 0;
	if (item->folder->account->mark_crosspost_read && msginfo->msgid) {
		if (item->folder->newsart &&
		    g_hash_table_lookup(item->folder->newsart, msginfo->msgid) != NULL) {
			msginfo->flags.perm_flags = MSG_COLORLABEL_TO_FLAGS(item->folder->account->crosspost_col);
				
		} else {
			if (!item->folder->newsart) 
				item->folder->newsart = g_hash_table_new(g_str_hash, g_str_equal);
			g_hash_table_insert(item->folder->newsart,
					g_strdup(msginfo->msgid), GINT_TO_POINTER(1));
			msginfo->flags.perm_flags = MSG_NEW|MSG_UNREAD;
		}
	} else {
		msginfo->flags.perm_flags = MSG_NEW|MSG_UNREAD;
	}
}

static void news_get_extra_fields(NewsSession *session, FolderItem *item, GSList *msglist)
{
	MsgInfo *msginfo = NULL;
	gint ok;
	GSList *cur;
	clist *hdrlist = NULL;
	clistiter *hdr;
	gint first = -1, last = -1;
	GHashTable *hash_table;
	
	cm_return_if_fail(session != NULL);
	cm_return_if_fail(item != NULL);
	cm_return_if_fail(item->folder != NULL);
	cm_return_if_fail(FOLDER_CLASS(item->folder) == &news_class);

	if (msglist == NULL)
		return;

	news_folder_lock(NEWS_FOLDER(item->folder));

	hash_table = g_hash_table_new(g_direct_hash, g_direct_equal);
	
	for (cur = msglist; cur; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;
		if (first == -1 || msginfo->msgnum < first)
			first = msginfo->msgnum;
		if (last == -1 || msginfo->msgnum > last)
			last = msginfo->msgnum;
		g_hash_table_insert(hash_table,
				GINT_TO_POINTER(msginfo->msgnum), msginfo);
	}

	if (first == -1 || last == -1) {
		g_hash_table_destroy(hash_table);
		return;
	}

/* Newsgroups */
	ok = nntp_threaded_xhdr(item->folder, "newsgroups", first, last, &hdrlist);

	if (ok != NEWSNNTP_NO_ERROR) {
		log_warning(LOG_PROTOCOL, _("couldn't get xhdr\n"));
		if (ok == NEWSNNTP_ERROR_STREAM) {
			session_destroy(SESSION(session));
			REMOTE_FOLDER(item->folder)->session = NULL;
		}
		news_folder_unlock(NEWS_FOLDER(item->folder));
		if (hdrlist != NULL)
			newsnntp_xhdr_free(hdrlist);
		return;
	}

	for (hdr = clist_begin(hdrlist); hdr; hdr = clist_next(hdr)) {
		struct newsnntp_xhdr_resp_item *hdrval = clist_content(hdr);
		msginfo = g_hash_table_lookup(hash_table, GINT_TO_POINTER(hdrval->hdr_article));
		if (msginfo) {
			if (msginfo->newsgroups)
				g_free(msginfo->newsgroups);
			msginfo->newsgroups = g_strdup(hdrval->hdr_value);
		}
	}
	newsnntp_xhdr_free(hdrlist);
	hdrlist = NULL;
	
/* To */
	ok = nntp_threaded_xhdr(item->folder, "to", first, last, &hdrlist);

	if (ok != NEWSNNTP_NO_ERROR) {
		log_warning(LOG_PROTOCOL, _("couldn't get xhdr\n"));
		if (ok == NEWSNNTP_ERROR_STREAM) {
			session_destroy(SESSION(session));
			REMOTE_FOLDER(item->folder)->session = NULL;
		}
		news_folder_unlock(NEWS_FOLDER(item->folder));
		if (hdrlist != NULL)
			newsnntp_xhdr_free(hdrlist);
		return;
	}

	for (hdr = clist_begin(hdrlist); hdr; hdr = clist_next(hdr)) {
		struct newsnntp_xhdr_resp_item *hdrval = clist_content(hdr);
		msginfo = g_hash_table_lookup(hash_table, GINT_TO_POINTER(hdrval->hdr_article));
		if (msginfo) {
			if (msginfo->to)
				g_free(msginfo->to);
			msginfo->to = g_strdup(hdrval->hdr_value);
		}
	}
	newsnntp_xhdr_free(hdrlist);
	hdrlist = NULL;
	
/* Cc */
	ok = nntp_threaded_xhdr(item->folder, "cc", first, last, &hdrlist);

	if (ok != NEWSNNTP_NO_ERROR) {
		log_warning(LOG_PROTOCOL, _("couldn't get xhdr\n"));
		if (ok == NEWSNNTP_ERROR_STREAM) {
			session_destroy(SESSION(session));
			REMOTE_FOLDER(item->folder)->session = NULL;
		}
		news_folder_unlock(NEWS_FOLDER(item->folder));
		if (hdrlist != NULL)
			newsnntp_xhdr_free(hdrlist);
		return;
	}

	for (hdr = clist_begin(hdrlist); hdr; hdr = clist_next(hdr)) {
		struct newsnntp_xhdr_resp_item *hdrval = clist_content(hdr);
		msginfo = g_hash_table_lookup(hash_table, GINT_TO_POINTER(hdrval->hdr_article));
		if (msginfo) {
			if (msginfo->cc)
				g_free(msginfo->cc);
			msginfo->cc = g_strdup(hdrval->hdr_value);
		}
	}
	newsnntp_xhdr_free(hdrlist);
	hdrlist = NULL;

	g_hash_table_destroy(hash_table);
	news_folder_unlock(NEWS_FOLDER(item->folder));
}

static GSList *news_get_msginfos_for_range(NewsSession *session, FolderItem *item, guint begin, guint end)
{
	GSList *newlist = NULL;
	GSList *llast = NULL;
	MsgInfo *msginfo;
	gint ok;
	clist *msglist = NULL;
	clistiter *cur;
	cm_return_val_if_fail(session != NULL, NULL);
	cm_return_val_if_fail(item != NULL, NULL);

	log_message(LOG_PROTOCOL, _("getting xover %d - %d in %s...\n"),
		    begin, end, item->path);

	news_folder_lock(NEWS_FOLDER(item->folder));
	
	ok = news_select_group(item->folder, item->path, NULL, NULL, NULL);
	if (ok != NEWSNNTP_NO_ERROR) {
		log_warning(LOG_PROTOCOL, _("couldn't set group: %s\n"), item->path);
		news_folder_unlock(NEWS_FOLDER(item->folder));
		return NULL;
	}

	ok = nntp_threaded_xover(item->folder, begin, end, NULL, &msglist);
	
	if (ok != NEWSNNTP_NO_ERROR) {
		log_warning(LOG_PROTOCOL, _("couldn't get xover\n"));
		if (ok == NEWSNNTP_ERROR_STREAM) {
			session_destroy(SESSION(session));
			REMOTE_FOLDER(item->folder)->session = NULL;
		}
		news_folder_unlock(NEWS_FOLDER(item->folder));
		if (msglist != NULL)
			newsnntp_xover_resp_list_free(msglist);
		return NULL;
	}

	if (msglist) {
		for (cur = clist_begin(msglist); cur; cur = clist_next(cur)) {
			struct newsnntp_xover_resp_item *ritem = (struct newsnntp_xover_resp_item *)clist_content(cur);
			msginfo = news_parse_xover(ritem);
			
			if (!msginfo) {
				log_warning(LOG_PROTOCOL, _("invalid xover line\n"));
				continue;
			}

			msginfo->folder = item;
			news_set_msg_flags(item, msginfo);
			msginfo->flags.tmp_flags |= MSG_NEWS;

			if (!newlist)
				llast = newlist = g_slist_append(newlist, msginfo);
			else {
				llast = g_slist_append(llast, msginfo);
				llast = llast->next;
			}
		}
		newsnntp_xover_resp_list_free(msglist);
	}

	news_folder_unlock(NEWS_FOLDER(item->folder));

	session_set_access_time(SESSION(session));

	news_get_extra_fields(session, item, newlist);
	
	return newlist;
}

static MsgInfo *news_get_msginfo(Folder *folder, FolderItem *item, gint num)
{
	GSList *msglist = NULL;
	NewsSession *session;
	MsgInfo *msginfo = NULL;

	session = news_session_get(folder);
	cm_return_val_if_fail(session != NULL, NULL);
	cm_return_val_if_fail(item != NULL, NULL);
	cm_return_val_if_fail(item->folder != NULL, NULL);
	cm_return_val_if_fail(FOLDER_CLASS(item->folder) == &news_class, NULL);

 	msglist = news_get_msginfos_for_range(session, item, num, num);
 
 	if (msglist)
		msginfo = msglist->data;
	
	g_slist_free(msglist);
	
	return msginfo;
}

static GSList *news_get_msginfos(Folder *folder, FolderItem *item, GSList *msgnum_list)
{
	NewsSession *session;
	GSList *elem, *msginfo_list = NULL, *tmp_msgnum_list, *tmp_msginfo_list;
	guint first, last, next;
	
	cm_return_val_if_fail(folder != NULL, NULL);
	cm_return_val_if_fail(FOLDER_CLASS(folder) == &news_class, NULL);
	cm_return_val_if_fail(msgnum_list != NULL, NULL);
	cm_return_val_if_fail(item != NULL, NULL);
	
	session = news_session_get(folder);
	cm_return_val_if_fail(session != NULL, NULL);

	tmp_msgnum_list = g_slist_copy(msgnum_list);
	tmp_msgnum_list = g_slist_sort(tmp_msgnum_list, g_int_compare);

	progressindicator_start(PROGRESS_TYPE_NETWORK);

	first = GPOINTER_TO_INT(tmp_msgnum_list->data);
	last = first;
	
	news_folder_lock(NEWS_FOLDER(item->folder));
	
	for(elem = g_slist_next(tmp_msgnum_list); elem != NULL; elem = g_slist_next(elem)) {
		next = GPOINTER_TO_INT(elem->data);
		if(next != (last + 1)) {
			tmp_msginfo_list = news_get_msginfos_for_range(session, item, first, last);
			msginfo_list = g_slist_concat(msginfo_list, tmp_msginfo_list);
			first = next;
		}
		last = next;
	}
	
	news_folder_unlock(NEWS_FOLDER(item->folder));
	
	tmp_msginfo_list = news_get_msginfos_for_range(session, item, first, last);
	msginfo_list = g_slist_concat(msginfo_list, tmp_msginfo_list);

	g_slist_free(tmp_msgnum_list);
	
	progressindicator_stop(PROGRESS_TYPE_NETWORK);

	return msginfo_list;
}

static gboolean news_scan_required(Folder *folder, FolderItem *item)
{
	return TRUE;
}

void news_synchronise(FolderItem *item, gint days) 
{
	news_gtk_synchronise(item, days);
}

static gint news_rename_folder(Folder *folder, FolderItem *item,
				const gchar *name)
{
	gchar *path;
	 
	cm_return_val_if_fail(folder != NULL, -1);
	cm_return_val_if_fail(item != NULL, -1);
	cm_return_val_if_fail(item->path != NULL, -1);
	cm_return_val_if_fail(name != NULL, -1);

	path = folder_item_get_path(item);
	if (!is_dir_exist(path))
		make_dir_hier(path);

	g_free(item->name);
	item->name = g_strdup(name);

	return 0;
}

static gint news_remove_folder(Folder *folder, FolderItem *item)
{
	gchar *path;

	cm_return_val_if_fail(folder != NULL, -1);
	cm_return_val_if_fail(item != NULL, -1);
	cm_return_val_if_fail(item->path != NULL, -1);

	path = folder_item_get_path(item);
	if (remove_dir_recursive(path) < 0) {
		g_warning("can't remove directory '%s'", path);
		g_free(path);
		return -1;
	}

	g_free(path);
	folder_item_remove(item);
	return 0;
}

void nntp_disconnect_all(gboolean have_connectivity)
{
	GList *list;
	gboolean short_timeout;
#ifdef HAVE_NETWORKMANAGER_SUPPORT
	GError *error;
#endif

#ifdef HAVE_NETWORKMANAGER_SUPPORT
	error = NULL;
	short_timeout = !networkmanager_is_online(&error);
	if(error) {
		short_timeout = TRUE;
		g_error_free(error);
	}
#else
	short_timeout = TRUE;
#endif

	if(short_timeout)
		nntp_main_set_timeout(1);

	for (list = account_get_list(); list != NULL; list = list->next) {
		PrefsAccount *account = list->data;
		if (account->protocol == A_NNTP) {
			RemoteFolder *folder = (RemoteFolder *)account->folder;
			if (folder && folder->session) {
				NewsSession *session = (NewsSession *)folder->session;
				if (have_connectivity)
					nntp_threaded_disconnect(FOLDER(folder));
				SESSION(session)->state = SESSION_DISCONNECTED;
				SESSION(session)->sock = NULL;
				session_destroy(SESSION(session));
				folder->session = NULL;
			}
		}
	}

	if(short_timeout)
		nntp_main_set_timeout(prefs_common.io_timeout_secs);
}

#else
#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include "folder.h"
#include "alertpanel.h"

static FolderClass news_class;

static void warn_etpan(void)
{
	static gboolean missing_news_warning = TRUE;
	if (missing_news_warning) {
		missing_news_warning = FALSE;
		alertpanel_error(
			_("You have one or more News accounts "
			  "defined. However this version of "
			  "Claws Mail has been built without "
			  "News support; your News accounts are "
			  "disabled.\n\n"
			  "You probably need to "
			  "install libetpan and recompile "
			  "Claws Mail."));
	}
}
static Folder *news_folder_new(const gchar *name, const gchar *path)
{
	warn_etpan();
	return NULL;
}
void news_group_list_free(GSList *group_list)
{
	warn_etpan();
}
void news_remove_group_list_cache(Folder *folder)
{
	warn_etpan();
}
int news_folder_locked(Folder *folder)
{
	warn_etpan();
	return 0;
}
gint news_post(Folder *folder, const gchar *file)
{
	warn_etpan();
	return -1;
}

gint news_cancel_article(Folder * folder, MsgInfo * msginfo)
{
	warn_etpan();
	return -1;
}

GSList *news_get_group_list(Folder *folder)
{
	warn_etpan();
	return NULL;
}


FolderClass *news_get_class(void)
{
	if (news_class.idstr == NULL) {
		news_class.type = F_NEWS;
		news_class.idstr = "news";
		news_class.uistr = "News";

		/* Folder functions */
		news_class.new_folder = news_folder_new;
	};

	return &news_class;
}

void nntp_disconnect_all(gboolean have_connectivity)
{
}

#endif
