/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2003 Hiroyuki Yamamoto
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
#ifdef WIN32
#include <w32lib.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif
#include <ctype.h>
#include <time.h>
#if HAVE_ICONV
#  include <iconv.h>
#endif

#if USE_OPENSSL
#  include "ssl.h"
#endif
#include "folder.h"
#include "session.h"
#include "procmsg.h"
#include "intl.h"
#include "imap.h"
#include "socket.h"
#include "ssl.h"
#include "recv.h"
#include "procheader.h"
#include "prefs_account.h"
#include "codeconv.h"
#include "utils.h"
#include "inputdialog.h"
#include "log.h"

typedef struct _IMAPFolder	IMAPFolder;
typedef struct _IMAPSession	IMAPSession;
typedef struct _IMAPNameSpace	IMAPNameSpace;
typedef struct _IMAPFolderItem	IMAPFolderItem;

#include "prefs_account.h"

#define IMAP_FOLDER(obj)	((IMAPFolder *)obj)
#define IMAP_SESSION(obj)	((IMAPSession *)obj)

struct _IMAPFolder
{
	RemoteFolder rfolder;

	/* list of IMAPNameSpace */
	GList *ns_personal;
	GList *ns_others;
	GList *ns_shared;
};

struct _IMAPSession
{
	Session session;

	gchar **capability;
	gchar *mbox;
	time_t last_access_time;
	gboolean authenticated;
	guint cmd_count;
	gboolean folder_content_changed;
	guint exists;
};

struct _IMAPNameSpace
{
	gchar *name;
	gchar separator;
};

#define IMAP_SUCCESS	0
#define IMAP_SOCKET	2
#define IMAP_AUTHFAIL	3
#define IMAP_PROTOCOL	4
#define IMAP_SYNTAX	5
#define IMAP_IOERR	6
#define IMAP_ERROR	7

#ifdef WIN32
#define IMAPBUFSIZE	8191
#else
#define IMAPBUFSIZE	8192
#endif
#define IMAPCMDLIMIT	1000

typedef enum
{
	IMAP_FLAG_SEEN		= 1 << 0,
	IMAP_FLAG_ANSWERED	= 1 << 1,
	IMAP_FLAG_FLAGGED	= 1 << 2,
	IMAP_FLAG_DELETED	= 1 << 3,
	IMAP_FLAG_DRAFT		= 1 << 4
} IMAPFlags;

#define IMAP_IS_SEEN(flags)	((flags & IMAP_FLAG_SEEN) != 0)
#define IMAP_IS_ANSWERED(flags)	((flags & IMAP_FLAG_ANSWERED) != 0)
#define IMAP_IS_FLAGGED(flags)	((flags & IMAP_FLAG_FLAGGED) != 0)
#define IMAP_IS_DELETED(flags)	((flags & IMAP_FLAG_DELETED) != 0)
#define IMAP_IS_DRAFT(flags)	((flags & IMAP_FLAG_DRAFT) != 0)


#define IMAP4_PORT	143
#if USE_OPENSSL
#define IMAPS_PORT	993
#endif

#define QUOTE_IF_REQUIRED(out, str)				\
{								\
	if (*str != '"' && strpbrk(str, " \t(){}%*") != NULL) {	\
		gchar *__tmp;					\
		gint len;					\
								\
		len = strlen(str) + 3;				\
		Xalloca(__tmp, len, return IMAP_ERROR);		\
		g_snprintf(__tmp, len, "\"%s\"", str);		\
		out = __tmp;					\
	} else {						\
		Xstrdup_a(out, str, return IMAP_ERROR);		\
	}							\
}

typedef gchar * IMAPSet;

struct _IMAPFolderItem
{
	FolderItem item;

	guint lastuid;
	guint uid_next;
	GSList *uid_list;
};

static Folder *imap_folder_new(const gchar * name, const gchar * path);
static void imap_folder_destroy(Folder * folder);

static IMAPSession *imap_session_new(const PrefsAccount * account);
static void imap_session_authenticate(IMAPSession * session,
				      const PrefsAccount * account);
static void imap_session_destroy(Session * session);

static gchar *imap_fetch_msg(Folder * folder, FolderItem * item, gint uid);
static gint imap_add_msg(Folder * folder,
			 FolderItem * dest,
			 const gchar * file, gboolean remove_source);

static gint imap_copy_msg(Folder * folder,
			  FolderItem * dest, MsgInfo * msginfo);

static gint imap_remove_msg(Folder * folder, FolderItem * item, gint uid);
static gint imap_remove_all_msg(Folder * folder, FolderItem * item);

static gboolean imap_is_msg_changed(Folder * folder,
				    FolderItem * item, MsgInfo * msginfo);

static void imap_scan_tree(Folder * folder);

static gint imap_create_tree(Folder * folder);

static FolderItem *imap_create_folder(Folder * folder,
				      FolderItem * parent,
				      const gchar * name);
static gint imap_rename_folder(Folder * folder,
			       FolderItem * item, const gchar * name);
static gint imap_remove_folder(Folder * folder, FolderItem * item);


static void imap_folder_init		(Folder		*folder,
					 const gchar	*name,
					 const gchar	*path);

static FolderItem *imap_folder_item_new	(Folder		*folder);
static void imap_folder_item_destroy	(Folder		*folder,
					 FolderItem	*item);

static IMAPSession *imap_session_get	(Folder		*folder);

static gint imap_scan_tree_recursive	(IMAPSession	*session,
					 FolderItem	*item);
static GSList *imap_parse_list		(IMAPFolder	*folder,
					 IMAPSession	*session,
					 const gchar	*real_path,
					 gchar		*separator);

static void imap_create_missing_folders	(Folder			*folder);
static FolderItem *imap_create_special_folder
					(Folder			*folder,
					 SpecialFolderItemType	 stype,
					 const gchar		*name);

static gint imap_do_copy		(Folder		*folder,
					 FolderItem	*dest,
					 MsgInfo	*msginfo,
					 gboolean	 remove_source);

static void imap_delete_all_cached_messages	(FolderItem	*item);

#if USE_OPENSSL
static SockInfo *imap_open		(const gchar	*server,
					 gushort	 port,
					 SSLType	 ssl_type);
#else
static SockInfo *imap_open		(const gchar	*server,
					 gushort	 port);
#endif

#if USE_OPENSSL
static SockInfo *imap_open_tunnel(const gchar *server,
				  const gchar *tunnelcmd,
				  SSLType ssl_type);
#else
static SockInfo *imap_open_tunnel(const gchar *server,
				  const gchar *tunnelcmd);
#endif

#if USE_OPENSSL
static SockInfo *imap_init_sock(SockInfo *sock, SSLType	ssl_type);
#else
static SockInfo *imap_init_sock(SockInfo *sock);
#endif

static gint imap_set_message_flags	(IMAPSession	*session,
					 MsgNumberList	*numlist,
					 IMAPFlags	 flag,
					 gboolean	 is_set);
static gint imap_select			(IMAPSession	*session,
					 IMAPFolder	*folder,
					 const gchar	*path,
					 gint		*exists,
					 gint		*recent,
					 gint		*unseen,
					 guint32	*uid_validity);
static gint imap_status			(IMAPSession	*session,
					 IMAPFolder	*folder,
					 const gchar	*path,
					 gint		*messages,
					 gint		*recent,
					 guint32	*uid_next,
					 guint32	*uid_validity,
					 gint		*unseen);

static void imap_parse_namespace		(IMAPSession	*session,
						 IMAPFolder	*folder);
static void imap_get_namespace_by_list		(IMAPSession	*session,
						 IMAPFolder	*folder);
static IMAPNameSpace *imap_find_namespace	(IMAPFolder	*folder,
						 const gchar	*path);
static gchar imap_get_path_separator		(IMAPFolder	*folder,
						 const gchar	*path);
static gchar *imap_get_real_path		(IMAPFolder	*folder,
						 const gchar	*path);

static gchar *imap_parse_atom		(SockInfo	*sock,
					 gchar		*src,
					 gchar		*dest,
					 gint		 dest_len,
					 GString	*str);
static MsgFlags imap_parse_flags	(const gchar	*flag_str);
static MsgInfo *imap_parse_envelope	(SockInfo	*sock,
					 FolderItem	*item,
					 GString	*line_str);
static gint imap_greeting		(IMAPSession	*session);
static gboolean imap_has_capability	(IMAPSession	*session,
 					 const gchar	*cap);
void imap_free_capabilities		(IMAPSession 	*session);
static const IMAPSet numberlist_to_imapset
					(MsgNumberList *list);

/* low-level IMAP4rev1 commands */
static gint imap_cmd_login	(IMAPSession	*sock,
				 const gchar	*user,
				 const gchar	*pass);
static gint imap_cmd_logout	(IMAPSession	*sock);
static gint imap_cmd_noop	(IMAPSession	*sock);
static gint imap_cmd_starttls	(IMAPSession	*sock);
static gint imap_cmd_namespace	(IMAPSession	*sock,
				 gchar	       **ns_str);
static gint imap_cmd_list	(IMAPSession	*session,
				 const gchar	*ref,
				 const gchar	*mailbox,
				 GPtrArray	*argbuf);
static gint imap_cmd_do_select	(IMAPSession	*sock,
				 const gchar	*folder,
				 gboolean	 examine,
				 gint		*exists,
				 gint		*recent,
				 gint		*unseen,
				 guint32	*uid_validity);
static gint imap_cmd_select	(IMAPSession	*sock,
				 const gchar	*folder,
				 gint		*exists,
				 gint		*recent,
				 gint		*unseen,
				 guint32	*uid_validity);
static gint imap_cmd_examine	(IMAPSession	*sock,
				 const gchar	*folder,
				 gint		*exists,
				 gint		*recent,
				 gint		*unseen,
				 guint32	*uid_validity);
static gint imap_cmd_create	(IMAPSession	*sock,
				 const gchar	*folder);
static gint imap_cmd_rename	(IMAPSession	*sock,
				 const gchar	*oldfolder,
				 const gchar	*newfolder);
static gint imap_cmd_delete	(IMAPSession	*sock,
				 const gchar	*folder);
static gint imap_cmd_envelope	(IMAPSession	*sock,
				 IMAPSet	 set);
static gint imap_cmd_fetch	(IMAPSession	*sock,
				 guint32	 uid,
				 const gchar	*filename);
static gint imap_cmd_append	(IMAPSession	*session,
				 const gchar	*destfolder,
				 const gchar	*file,
				 gint32		*newuid);
static gint imap_cmd_copy	(IMAPSession	*session,
				 gint32		msgnum,
				 const gchar	*destfolder,
				 gint32		*new_uid);
static gint imap_cmd_store	(IMAPSession	*sock,
				 IMAPSet	 set,
				 gchar		*sub_cmd);
static gint imap_cmd_expunge	(IMAPSession	*sock);

static gint imap_cmd_ok		(IMAPSession	*session,
				 GPtrArray	*argbuf);
static void imap_gen_send	(IMAPSession	*sock,
				 const gchar	*format, ...);
static gint imap_gen_recv	(IMAPSession	*sock,
				 gchar	       **buf);

/* misc utility functions */
static gchar *strchr_cpy			(const gchar	*src,
						 gchar		 ch,
						 gchar		*dest,
						 gint		 len);
static gchar *get_quoted			(const gchar	*src,
						 gchar		 ch,
						 gchar		*dest,
						 gint		 len);
static gchar *search_array_contain_str		(GPtrArray	*array,
						 const gchar	*str);
static gchar *search_array_str			(GPtrArray	*array,
						 const gchar	*str);
static void imap_path_separator_subst		(gchar		*str,
						 gchar		 separator);

static gchar *imap_modified_utf7_to_locale	(const gchar	*mutf7_str);
static gchar *imap_locale_to_modified_utf7	(const gchar	*from);

static gboolean imap_rename_folder_func		(GNode		*node,
						 gpointer	 data);
static gint imap_get_num_list			(Folder 	*folder,
						 FolderItem 	*item,
						 GSList	       **list);
static GSList *imap_get_msginfos		(Folder		*folder,
						 FolderItem	*item,
						 GSList		*msgnum_list);
static MsgInfo *imap_get_msginfo 		(Folder 	*folder,
						 FolderItem 	*item,
						 gint 		 num);
static gboolean imap_check_msgnum_validity	(Folder 	*folder,
						 FolderItem 	*item);
static void imap_change_flags			(Folder 	*folder,
						 FolderItem 	*item,
						 MsgInfo 	*msginfo,
						 MsgPermFlags 	newflags);

FolderClass imap_class =
{
	F_IMAP,
	"imap",
	"IMAP4",

	/* Folder functions */
	imap_folder_new,
	imap_folder_destroy,
	imap_scan_tree,
	imap_create_tree,

	/* FolderItem functions */
	imap_folder_item_new,
	imap_folder_item_destroy,
	imap_create_folder,
	imap_rename_folder,
	imap_remove_folder,
	imap_get_num_list,
	NULL,
	NULL,
	NULL,
	imap_check_msgnum_validity,

	/* Message functions */
	imap_get_msginfo,
	imap_get_msginfos,
	imap_fetch_msg,
	imap_add_msg,
	imap_copy_msg,
	imap_remove_msg,
	imap_remove_all_msg,
	imap_is_msg_changed,
	imap_change_flags,
};

FolderClass *imap_get_class(void)
{
	return &imap_class;
}

Folder *imap_folder_new(const gchar *name, const gchar *path)
{
	Folder *folder;

	folder = (Folder *)g_new0(IMAPFolder, 1);
	folder->klass = &imap_class;
	imap_folder_init(folder, name, path);

	return folder;
}

void imap_folder_destroy(Folder *folder)
{
	gchar *dir;

	dir = folder_get_path(folder);
#ifdef WIN32
	subst_char(dir, '/', G_DIR_SEPARATOR);
#endif
	if (is_dir_exist(dir))
		remove_dir_recursive(dir);
	g_free(dir);

	folder_remote_folder_destroy(REMOTE_FOLDER(folder));
}

static void imap_folder_init(Folder *folder, const gchar *name,
			     const gchar *path)
{
	folder_remote_folder_init((Folder *)folder, name, path);
}

static FolderItem *imap_folder_item_new(Folder *folder)
{
	IMAPFolderItem *item;
	
	item = g_new0(IMAPFolderItem, 1);
	item->lastuid = 0;
	item->uid_next = 0;
	item->uid_list = NULL;

	return (FolderItem *)item;
}

static void imap_folder_item_destroy(Folder *folder, FolderItem *_item)
{
	IMAPFolderItem *item = (IMAPFolderItem *)_item;

	g_return_if_fail(item != NULL);
	g_slist_free(item->uid_list);

	g_free(_item);
}

static gboolean imap_reset_uid_lists_func(GNode *node, gpointer data)
{
	IMAPFolderItem *item = (IMAPFolderItem *)node->data;
	
	item->lastuid = 0;
	item->uid_next = 0;
	g_slist_free(item->uid_list);
	item->uid_list = NULL;
	
	return FALSE;
}

static void imap_reset_uid_lists(Folder *folder)
{
	if(folder->node == NULL)
		return;
	
	/* Destroy all uid lists and rest last uid */
	g_node_traverse(folder->node, G_IN_ORDER, G_TRAVERSE_ALL, -1, imap_reset_uid_lists_func, NULL);	
}

static IMAPSession *imap_session_get(Folder *folder)
{
	RemoteFolder *rfolder = REMOTE_FOLDER(folder);
	IMAPSession *session = NULL;
	gushort port;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(FOLDER_CLASS(folder) == &imap_class, NULL);
	g_return_val_if_fail(folder->account != NULL, NULL);

#if USE_OPENSSL
	port = folder->account->set_imapport ? folder->account->imapport
		: folder->account->ssl_imap == SSL_TUNNEL
		? IMAPS_PORT : IMAP4_PORT;
#else
	port = folder->account->set_imapport ? folder->account->imapport
		: IMAP4_PORT;
#endif

	/* Make sure we have a session */
	if (rfolder->session != NULL) {
		session = IMAP_SESSION(rfolder->session);
	} else {
		imap_reset_uid_lists(folder);
		session = imap_session_new(folder->account);
	}
	if(session == NULL)
		return NULL;

	if (SESSION(session)->sock->state == CONN_DISCONNECTED) {
		debug_print("IMAP server disconnected\n");
		session_destroy(SESSION(session));
		imap_reset_uid_lists(folder);
		session = imap_session_new(folder->account);
	}

	/* Make sure session is authenticated */
	if (!IMAP_SESSION(session)->authenticated)
		imap_session_authenticate(IMAP_SESSION(session), folder->account);
	if (!IMAP_SESSION(session)->authenticated) {
		session_destroy(SESSION(session));
		rfolder->session = NULL;
		return NULL;
	}

	/* Make sure we have parsed the IMAP namespace */
	imap_parse_namespace(IMAP_SESSION(session),
			     IMAP_FOLDER(folder));

	/* I think the point of this code is to avoid sending a
	 * keepalive if we've used the session recently and therefore
	 * think it's still alive.  Unfortunately, most of the code
	 * does not yet check for errors on the socket, and so if the
	 * connection drops we don't notice until the timeout expires.
	 * A better solution than sending a NOOP every time would be
	 * for every command to be prepared to retry until it is
	 * successfully sent. -- mbp */
	if (time(NULL) - session->last_access_time > SESSION_TIMEOUT) {
		/* verify that the session is still alive */
		if (imap_cmd_noop(session) != IMAP_SUCCESS) {
			/* Check if this is the first try to establish a
			   connection, if yes we don't try to reconnect */
			if (rfolder->session == NULL) {
				log_warning(_("Connecting %s:%d failed"),
					    folder->account->recv_server, port);
				session_destroy(SESSION(session));
				session = NULL;
			} else {
				log_warning(_("IMAP4 connection to %s:%d has been"
					      " disconnected. Reconnecting...\n"),
					    folder->account->recv_server, port);
				session_destroy(SESSION(session));
				/* Clear folders session to make imap_session_get create
				   a new session, because of rfolder->session == NULL
				   it will not try to reconnect again and so avoid an
				   endless loop */
				rfolder->session = NULL;
				session = imap_session_get(folder);
			}
		}
	}

	rfolder->session = SESSION(session);
	if (session) {
		session->last_access_time = time(NULL);
	}
	return IMAP_SESSION(session);
}

IMAPSession *imap_session_new(const PrefsAccount *account)
{
	IMAPSession *session;
	SockInfo *imap_sock;
	gushort port;
	gboolean is_preauth;

#ifdef USE_OPENSSL
	/* FIXME: IMAP over SSL only... */ 
	SSLType ssl_type;

	port = account->set_imapport ? account->imapport
		: account->ssl_imap == SSL_TUNNEL ? IMAPS_PORT : IMAP4_PORT;
	ssl_type = account->ssl_imap;	
#else
	port = account->set_imapport ? account->imapport
		: IMAP4_PORT;
#endif

	if (account->set_tunnelcmd) {
		log_message(_("creating tunneled IMAP4 connection\n"));
#if USE_OPENSSL
		if ((imap_sock = imap_open_tunnel(account->recv_server, 
						  account->tunnelcmd,
						  ssl_type)) == NULL)
#else
		if ((imap_sock = imap_open_tunnel(account->recv_server, 
						  account->tunnelcmd)) == NULL)
#endif
			return NULL;
	} else {
		g_return_val_if_fail(account->recv_server != NULL, NULL);

		log_message(_("creating IMAP4 connection to %s:%d ...\n"),
			    account->recv_server, port);
		
#if USE_OPENSSL
		if ((imap_sock = imap_open(account->recv_server, port,
					   ssl_type)) == NULL)
#else
	       	if ((imap_sock = imap_open(account->recv_server, port)) == NULL)
#endif
			return NULL;
	}

	session = g_new0(IMAPSession, 1);
	session_init(SESSION(session));
	SESSION(session)->type             = SESSION_IMAP;
	SESSION(session)->server           = g_strdup(account->recv_server);
	SESSION(session)->sock             = imap_sock;

	SESSION(session)->destroy          = imap_session_destroy;

	session->capability = NULL;

	session->mbox = NULL;
	session->authenticated = is_preauth;
	session->cmd_count = 0;

	/* Only need to log in if the connection was not PREAUTH */
	if (imap_greeting(session) != IMAP_SUCCESS) {
		session_destroy(SESSION(session));
		return NULL;
	}

#if USE_OPENSSL
	if (account->ssl_imap == SSL_STARTTLS && imap_has_capability(session, "STARTTLS")) {
		gint ok;

		ok = imap_cmd_starttls(session);
		if (ok != IMAP_SUCCESS) {
			log_warning(_("Can't start TLS session.\n"));
			session_destroy(SESSION(session));
			return NULL;
		}
		if (!ssl_init_socket_with_method(SESSION(session)->sock, SSL_METHOD_TLSv1)) {
			session_destroy(SESSION(session));
			return NULL;
		}

		imap_free_capabilities(session);
		session->authenticated = is_preauth;
		session->cmd_count = 1;

		if (imap_greeting(session) != IMAP_SUCCESS) {
			session_destroy(SESSION(session));
			return NULL;
		}		
	}
#endif
	log_message("IMAP connection is %s-authenticated\n",
		    (is_preauth) ? "pre" : "un");

	return session;
}

void imap_session_authenticate(IMAPSession *session, const PrefsAccount *account)
{
	gchar *pass;

	g_return_if_fail(account->userid != NULL);

	pass = account->passwd;
	if (!pass) {
		gchar *tmp_pass;
		tmp_pass = input_dialog_query_password(account->recv_server, account->userid);
		if (!tmp_pass)
			return;
		Xstrdup_a(pass, tmp_pass, {g_free(tmp_pass); return;});
		g_free(tmp_pass);
	}

	if (imap_cmd_login(session, account->userid, pass) != IMAP_SUCCESS) {
		imap_cmd_logout(session);
		return;
	}

	session->authenticated = TRUE;
}

void imap_session_destroy(Session *session)
{
	sock_close(session->sock);
	session->sock = NULL;

	g_free(IMAP_SESSION(session)->mbox);
	imap_free_capabilities(IMAP_SESSION(session));
}

gchar *imap_fetch_msg(Folder *folder, FolderItem *item, gint uid)
{
	gchar *path, *filename;
	IMAPSession *session;
	gint ok;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(item != NULL, NULL);

	path = folder_item_get_path(item);
#ifdef WIN32
	subst_char(path, '/', G_DIR_SEPARATOR);
#endif
	if (!is_dir_exist(path))
		make_dir_hier(path);
	filename = g_strconcat(path, G_DIR_SEPARATOR_S, itos(uid), NULL);
	g_free(path);
 
	if (is_file_exist(filename)) {
		debug_print("message %d has been already cached.\n", uid);
		return filename;
	}

	session = imap_session_get(folder);
	if (!session) {
		g_free(filename);
		return NULL;
	}

	ok = imap_select(session, IMAP_FOLDER(folder), item->path,
			 NULL, NULL, NULL, NULL);
	if (ok != IMAP_SUCCESS) {
		g_warning("can't select mailbox %s\n", item->path);
		g_free(filename);
		return NULL;
	}

	debug_print("getting message %d...\n", uid);
	ok = imap_cmd_fetch(session, (guint32)uid, filename);

	if (ok != IMAP_SUCCESS) {
		g_warning("can't fetch message %d\n", uid);
		g_free(filename);
		return NULL;
	}

	return filename;
}

gint imap_add_msg(Folder *folder, FolderItem *dest, const gchar *file,
		  gboolean remove_source)
{
	gchar *destdir;
	IMAPSession *session;
	gint ok, newuid;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(file != NULL, -1);

	session = imap_session_get(folder);
	if (!session) return -1;

	destdir = imap_get_real_path(IMAP_FOLDER(folder), dest->path);
	ok = imap_cmd_append(session, destdir, file, &newuid);
	g_free(destdir);

	if (ok != IMAP_SUCCESS) {
		g_warning("can't append message %s\n", file);
		return -1;
	}

	if (remove_source) {
		if (unlink(file) < 0)
			FILE_OP_ERROR(file, "unlink");
	}

	return newuid;
}

static gint imap_do_copy(Folder *folder, FolderItem *dest, MsgInfo *msginfo,
			 gboolean remove_source)
{
	gchar *destdir;
	IMAPSession *session;
	guint32 newuid = 0;
	gint ok;
	
	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(FOLDER_CLASS(folder) == &imap_class, -1);
	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(msginfo != NULL, -1);

	session = imap_session_get(folder);
	if (!session) return -1;

	if (msginfo->folder == dest) {
		g_warning("the src folder is identical to the dest.\n");
		return -1;
	}

	destdir = imap_get_real_path(IMAP_FOLDER(folder), dest->path);

	/* ensure source folder selected */
	ok = imap_select(session, IMAP_FOLDER(folder), msginfo->folder->path,
			 NULL, NULL, NULL, NULL);
	if (ok != IMAP_SUCCESS)
	        return -1;
        
	if (remove_source)
		debug_print("Moving message %s%c%d to %s ...\n",
			    msginfo->folder->path, G_DIR_SEPARATOR,
			    msginfo->msgnum, destdir);
	else
		debug_print("Copying message %s%c%d to %s ...\n",
			    msginfo->folder->path, G_DIR_SEPARATOR,
			    msginfo->msgnum, destdir);

	ok = imap_cmd_copy(session, msginfo->msgnum, destdir, &newuid);

	if (ok == IMAP_SUCCESS && remove_source) {
		MsgNumberList numlist;

		numlist.next = NULL;
		numlist.data = GINT_TO_POINTER(msginfo->msgnum);
		
		imap_set_message_flags(session, &numlist,
				       IMAP_FLAG_DELETED, TRUE);
		ok = imap_cmd_expunge(session);
	}

	g_free(destdir);

	if (ok == IMAP_SUCCESS)
		return newuid;
	else
		return -1;
}

gint imap_copy_msg(Folder *folder, FolderItem *dest, MsgInfo *msginfo)
{
	gchar *srcfile;
	gint ret = 0;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(msginfo != NULL, -1);
	g_return_val_if_fail(msginfo->folder != NULL, -1);

	if (folder == msginfo->folder->folder)
		return imap_do_copy(folder, dest, msginfo, FALSE);

	srcfile = procmsg_get_message_file(msginfo);
	if (!srcfile) return -1;

	ret = imap_add_msg(folder, dest, srcfile, FALSE);

	g_free(srcfile);

	return ret;
}

gint imap_remove_msg(Folder *folder, FolderItem *item, gint uid)
{
	gint ok;
	IMAPSession *session;
	gchar *dir;
	MsgNumberList numlist;
	
	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(FOLDER_CLASS(folder) == &imap_class, -1);
	g_return_val_if_fail(item != NULL, -1);

	session = imap_session_get(folder);
	if (!session) return -1;

	ok = imap_select(session, IMAP_FOLDER(folder), item->path,
			 NULL, NULL, NULL, NULL);
	if (ok != IMAP_SUCCESS)
		return ok;

	numlist.next = NULL;
	numlist.data = GINT_TO_POINTER(uid);
	
	ok = imap_set_message_flags
		(IMAP_SESSION(REMOTE_FOLDER(folder)->session),
		&numlist, IMAP_FLAG_DELETED, TRUE);
	if (ok != IMAP_SUCCESS) {
		log_warning(_("can't set deleted flags: %d\n"), uid);
		return ok;
	}

	ok = imap_cmd_expunge(session);
	if (ok != IMAP_SUCCESS) {
		log_warning(_("can't expunge\n"));
		return ok;
	}

	dir = folder_item_get_path(item);
#ifdef WIN32
	subst_char(dir, '/', G_DIR_SEPARATOR);
#endif
	if (is_dir_exist(dir))
		remove_numbered_files(dir, uid, uid);
	g_free(dir);

	return IMAP_SUCCESS;
}

gint imap_remove_all_msg(Folder *folder, FolderItem *item)
{
        gint exists, recent, unseen;
        guint32 uid_validity;
	gint ok;
	IMAPSession *session;
	gchar *dir;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(item != NULL, -1);

	session = imap_session_get(folder);
	if (!session) return -1;

	ok = imap_select(session, IMAP_FOLDER(folder), item->path,
			 &exists, &recent, &unseen, &uid_validity);
	if (ok != IMAP_SUCCESS)
		return ok;
	if (exists == 0)
		return IMAP_SUCCESS;

	imap_gen_send(session,
		      "STORE 1:%d +FLAGS (\\Deleted)", exists);
	ok = imap_cmd_ok(session, NULL);
	if (ok != IMAP_SUCCESS) {
		log_warning(_("can't set deleted flags: 1:%d\n"), exists);
		return ok;
	}

	ok = imap_cmd_expunge(session);
	if (ok != IMAP_SUCCESS) {
		log_warning(_("can't expunge\n"));
		return ok;
	}

	dir = folder_item_get_path(item);
#ifdef WIN32
	subst_char(dir, '/', G_DIR_SEPARATOR);
#endif
	if (is_dir_exist(dir))
		remove_all_numbered_files(dir);
	g_free(dir);

	return IMAP_SUCCESS;
}

gboolean imap_is_msg_changed(Folder *folder, FolderItem *item, MsgInfo *msginfo)
{
	/* TODO: properly implement this method */
	return FALSE;
}

void imap_scan_tree(Folder *folder)
{
	FolderItem *item;
	IMAPSession *session;
	gchar *root_folder = NULL;

	g_return_if_fail(folder != NULL);
	g_return_if_fail(folder->account != NULL);

	session = imap_session_get(folder);
	if (!session) {
		if (!folder->node) {
			folder_tree_destroy(folder);
			item = folder_item_new(folder, folder->name, NULL);
			item->folder = folder;
			folder->node = g_node_new(item);
		}
		return;
	}

	if (folder->account->imap_dir && *folder->account->imap_dir) {
		Xstrdup_a(root_folder, folder->account->imap_dir, return);
		strtailchomp(root_folder, '/');
		debug_print("IMAP root directory: %s\n", root_folder);
	}

	item = folder_item_new(folder, folder->name, root_folder);
	item->folder = folder;
	item->no_select = TRUE;
	folder->node = g_node_new(item);

	imap_scan_tree_recursive(session, item);

	imap_create_missing_folders(folder);
}

static gint imap_scan_tree_recursive(IMAPSession *session, FolderItem *item)
{
	Folder *folder;
	IMAPFolder *imapfolder;
	FolderItem *new_item;
	GSList *item_list, *cur;
	gchar *real_path;
	gchar *wildcard_path;
	gchar separator;
	gchar wildcard[3];

	g_return_val_if_fail(item != NULL, -1);
	g_return_val_if_fail(item->folder != NULL, -1);
	g_return_val_if_fail(item->no_sub == FALSE, -1);

	folder = FOLDER(item->folder);
	imapfolder = IMAP_FOLDER(folder);

	separator = imap_get_path_separator(imapfolder, item->path);

	if (item->folder->ui_func)
		item->folder->ui_func(folder, item, folder->ui_func_data);

	if (item->path) {
		wildcard[0] = separator;
		wildcard[1] = '%';
		wildcard[2] = '\0';
		real_path = imap_get_real_path(imapfolder, item->path);
	} else {
		wildcard[0] = '%';
		wildcard[1] = '\0';
		real_path = g_strdup("");
	}

	Xstrcat_a(wildcard_path, real_path, wildcard,
		  {g_free(real_path); return IMAP_ERROR;});
	QUOTE_IF_REQUIRED(wildcard_path, wildcard_path);

	imap_gen_send(session, "LIST \"\" %s",
		      wildcard_path);

	strtailchomp(real_path, separator);
	item_list = imap_parse_list(imapfolder, session, real_path, NULL);
	g_free(real_path);

	for (cur = item_list; cur != NULL; cur = cur->next) {
		new_item = cur->data;
		if (!strcmp(new_item->path, "INBOX")) {
			if (!folder->inbox) {
				new_item->stype = F_INBOX;
				item->folder->inbox = new_item;
			} else {
				folder_item_destroy(new_item);
				continue;
			}
		} else if (!item->parent || item->stype == F_INBOX) {
			gchar *base;

#ifdef WIN32
			base = g_strrstr(new_item->path, "/");
			if (!base)
				base = new_item->path;
			else {
				base++;
				if (!*base)
					base = new_item->path;
			}
#else
			base = g_basename(new_item->path);
#endif

			if (!folder->outbox && !strcasecmp(base, "Sent")) {
				new_item->stype = F_OUTBOX;
				folder->outbox = new_item;
			} else if (!folder->draft && !strcasecmp(base, "Drafts")) {
				new_item->stype = F_DRAFT;
				folder->draft = new_item;
			} else if (!folder->queue && !strcasecmp(base, "Queue")) {
				new_item->stype = F_QUEUE;
				folder->queue = new_item;
			} else if (!folder->trash && !strcasecmp(base, "Trash")) {
				new_item->stype = F_TRASH;
				folder->trash = new_item;
			}
		}
		folder_item_append(item, new_item);
		if (new_item->no_sub == FALSE)
			imap_scan_tree_recursive(session, new_item);
	}

	return IMAP_SUCCESS;
}

static GSList *imap_parse_list(IMAPFolder *folder, IMAPSession *session,
			       const gchar *real_path, gchar *separator)
{
	gchar buf[IMAPBUFSIZE];
	gchar flags[256];
	gchar separator_str[16];
	gchar *p;
	gchar *name;
	gchar *loc_name, *loc_path;
	GSList *item_list = NULL;
	GString *str;
	FolderItem *new_item;

	debug_print("getting list of %s ...\n",
		    *real_path ? real_path : "\"\"");

	str = g_string_new(NULL);

	for (;;) {
		if (sock_gets(SESSION(session)->sock, buf, sizeof(buf)) <= 0) {
			log_warning(_("error occurred while getting LIST.\n"));
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

		p = strchr_cpy(p, ' ', separator_str, sizeof(separator_str));
		if (!p) continue;
		extract_quote(separator_str, '"');
		if (!strcmp(separator_str, "NIL"))
			separator_str[0] = '\0';
		if (separator)
			*separator = separator_str[0];

		buf[0] = '\0';
		while (*p == ' ') p++;
		if (*p == '{' || *p == '"')
			p = imap_parse_atom(SESSION(session)->sock, p,
					    buf, sizeof(buf), str);
		else
			strncpy2(buf, p, sizeof(buf));
		strtailchomp(buf, separator_str[0]);
		if (buf[0] == '\0') continue;
		if (!strcmp(buf, real_path)) continue;

		if (separator_str[0] != '\0')
			subst_char(buf, separator_str[0], '/');
#ifdef WIN32
		name = g_strrstr(buf, "/");
		if (!name)
			name = buf;
		else {
			name++;
			if (!*name)
				name = buf;
		}
#else
		name = g_basename(buf);
#endif
		if (name[0] == '.') continue;

		loc_name = imap_modified_utf7_to_locale(name);
		loc_path = imap_modified_utf7_to_locale(buf);
		new_item = folder_item_new(FOLDER(folder), loc_name, loc_path);
		if (strcasestr(flags, "\\Noinferiors") != NULL)
			new_item->no_sub = TRUE;
		if (strcmp(buf, "INBOX") != 0 &&
		    strcasestr(flags, "\\Noselect") != NULL)
			new_item->no_select = TRUE;

		item_list = g_slist_append(item_list, new_item);

		debug_print("folder %s has been added.\n", loc_path);
		g_free(loc_path);
		g_free(loc_name);
	}

	g_string_free(str, TRUE);

	return item_list;
}

gint imap_create_tree(Folder *folder)
{
	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(folder->node != NULL, -1);
	g_return_val_if_fail(folder->node->data != NULL, -1);
	g_return_val_if_fail(folder->account != NULL, -1);

	imap_scan_tree(folder);
	imap_create_missing_folders(folder);

	return 0;
}

static void imap_create_missing_folders(Folder *folder)
{
	g_return_if_fail(folder != NULL);

	if (!folder->inbox)
		folder->inbox = imap_create_special_folder
			(folder, F_INBOX, "INBOX");
#if 0
	if (!folder->outbox)
		folder->outbox = imap_create_special_folder
			(folder, F_OUTBOX, "Sent");
	if (!folder->draft)
		folder->draft = imap_create_special_folder
			(folder, F_DRAFT, "Drafts");
	if (!folder->queue)
		folder->queue = imap_create_special_folder
			(folder, F_QUEUE, "Queue");
#endif
	if (!folder->trash)
		folder->trash = imap_create_special_folder
			(folder, F_TRASH, "Trash");
}

static FolderItem *imap_create_special_folder(Folder *folder,
					      SpecialFolderItemType stype,
					      const gchar *name)
{
	FolderItem *item;
	FolderItem *new_item;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(folder->node != NULL, NULL);
	g_return_val_if_fail(folder->node->data != NULL, NULL);
	g_return_val_if_fail(folder->account != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);

	item = FOLDER_ITEM(folder->node->data);
	new_item = imap_create_folder(folder, item, name);

	if (!new_item) {
		g_warning("Can't create '%s'\n", name);
		if (!folder->inbox) return NULL;

		new_item = imap_create_folder(folder, folder->inbox, name);
		if (!new_item)
			g_warning("Can't create '%s' under INBOX\n", name);
		else
			new_item->stype = stype;
	} else
		new_item->stype = stype;

	return new_item;
}

FolderItem *imap_create_folder(Folder *folder, FolderItem *parent,
			       const gchar *name)
{
	gchar *dirpath, *imap_path;
	IMAPSession *session;
	FolderItem *new_item;
	gchar separator;
	gchar *new_name;
	const gchar *p;
	gint ok;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(folder->account != NULL, NULL);
	g_return_val_if_fail(parent != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);

	session = imap_session_get(folder);
	if (!session) return NULL;

	if (!parent->parent && strcmp(name, "INBOX") == 0)
		dirpath = g_strdup(name);
	else if (parent->path)
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

	/* keep trailing directory separator to create a folder that contains
	   sub folder */
	imap_path = imap_locale_to_modified_utf7(dirpath);
	strtailchomp(dirpath, '/');
	Xstrdup_a(new_name, name, {g_free(dirpath); return NULL;});
	strtailchomp(new_name, '/');
	separator = imap_get_path_separator(IMAP_FOLDER(folder), imap_path);
	imap_path_separator_subst(imap_path, separator);
	subst_char(new_name, '/', separator);

	if (strcmp(name, "INBOX") != 0) {
		GPtrArray *argbuf;
		gint i;
		gboolean exist = FALSE;

		argbuf = g_ptr_array_new();
		ok = imap_cmd_list(session, NULL, imap_path,
				   argbuf);
		if (ok != IMAP_SUCCESS) {
			log_warning(_("can't create mailbox: LIST failed\n"));
			g_free(imap_path);
			g_free(dirpath);
			ptr_array_free_strings(argbuf);
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
		ptr_array_free_strings(argbuf);
		g_ptr_array_free(argbuf, TRUE);

		if (!exist) {
			ok = imap_cmd_create(session, imap_path);
			if (ok != IMAP_SUCCESS) {
				log_warning(_("can't create mailbox\n"));
				g_free(imap_path);
				g_free(dirpath);
				return NULL;
			}
		}
	}

	new_item = folder_item_new(folder, new_name, dirpath);
	folder_item_append(parent, new_item);
	g_free(imap_path);
	g_free(dirpath);

	dirpath = folder_item_get_path(new_item);
#ifdef WIN32
	subst_char(dirpath, '/', G_DIR_SEPARATOR);
#endif
	if (!is_dir_exist(dirpath))
		make_dir_hier(dirpath);
	g_free(dirpath);

	return new_item;
}

gint imap_rename_folder(Folder *folder, FolderItem *item, const gchar *name)
{
	gchar *dirpath;
	gchar *newpath;
	gchar *real_oldpath;
	gchar *real_newpath;
	GNode *node;
	gchar *paths[2];
	gchar *old_cache_dir;
	gchar *new_cache_dir;
	IMAPSession *session;
	gchar separator;
	gint ok;
	gint exists, recent, unseen;
	guint32 uid_validity;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(item != NULL, -1);
	g_return_val_if_fail(item->path != NULL, -1);
	g_return_val_if_fail(name != NULL, -1);

	session = imap_session_get(folder);
	if (!session) return -1;

	real_oldpath = imap_get_real_path(IMAP_FOLDER(folder), item->path);

	g_free(session->mbox);
	session->mbox = NULL;
	ok = imap_cmd_examine(session, "INBOX",
			      &exists, &recent, &unseen, &uid_validity);
	if (ok != IMAP_SUCCESS) {
		g_free(real_oldpath);
		return -1;
	}

	separator = imap_get_path_separator(IMAP_FOLDER(folder), item->path);
#ifdef WIN32
	{
		gchar *base;

		base = g_strrstr(item->path, "/");
		if (!base)
			newpath = g_strdup(item->name);
		else {
			base++;
			if (!*base)
				base = item->path;
			dirpath = g_strndup(item->path, base - item->path - 1);
			newpath = g_strconcat(dirpath, "/", name, NULL);
			g_free(dirpath);
		}
	}
#else
	if (strchr(item->path, G_DIR_SEPARATOR)) {
		dirpath = g_dirname(item->path);
		newpath = g_strconcat(dirpath, G_DIR_SEPARATOR_S, name, NULL);
		g_free(dirpath);
	} else
		newpath = g_strdup(name);
#endif

	real_newpath = imap_locale_to_modified_utf7(newpath);
	imap_path_separator_subst(real_newpath, separator);

	ok = imap_cmd_rename(session, real_oldpath, real_newpath);
	if (ok != IMAP_SUCCESS) {
		log_warning(_("can't rename mailbox: %s to %s\n"),
			    real_oldpath, real_newpath);
		g_free(real_oldpath);
		g_free(newpath);
		g_free(real_newpath);
		return -1;
	}

	g_free(item->name);
#ifdef WIN32
	locale_to_utf8(&name);
#endif
	item->name = g_strdup(name);

	old_cache_dir = folder_item_get_path(item);

	node = g_node_find(item->folder->node, G_PRE_ORDER, G_TRAVERSE_ALL,
			   item);
	paths[0] = g_strdup(item->path);
	paths[1] = newpath;
	g_node_traverse(node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
			imap_rename_folder_func, paths);

#ifdef WIN32
	subst_char(old_cache_dir, '/', G_DIR_SEPARATOR);
#endif
	if (is_dir_exist(old_cache_dir)) {
		new_cache_dir = folder_item_get_path(item);
#ifdef WIN32
	subst_char(new_cache_dir, '/', G_DIR_SEPARATOR);
#endif
		if (rename(old_cache_dir, new_cache_dir) < 0) {
			FILE_OP_ERROR(old_cache_dir, "rename");
		}
		g_free(new_cache_dir);
	}

	g_free(old_cache_dir);
	g_free(paths[0]);
	g_free(newpath);
	g_free(real_oldpath);
	g_free(real_newpath);

	return 0;
}

gint imap_remove_folder(Folder *folder, FolderItem *item)
{
	gint ok;
	IMAPSession *session;
	gchar *path;
	gchar *cache_dir;
	gint exists, recent, unseen;
	guint32 uid_validity;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(item != NULL, -1);
	g_return_val_if_fail(item->path != NULL, -1);

	session = imap_session_get(folder);
	if (!session) return -1;

	path = imap_get_real_path(IMAP_FOLDER(folder), item->path);

	ok = imap_cmd_examine(session, "INBOX",
			      &exists, &recent, &unseen, &uid_validity);
	if (ok != IMAP_SUCCESS) {
		g_free(path);
		return -1;
	}

	ok = imap_cmd_delete(session, path);
	if (ok != IMAP_SUCCESS) {
		log_warning(_("can't delete mailbox\n"));
		g_free(path);
		return -1;
	}

	g_free(path);
	cache_dir = folder_item_get_path(item);
#ifdef WIN32
	subst_char(cache_dir, '/', G_DIR_SEPARATOR);
#endif
	if (is_dir_exist(cache_dir) && remove_dir_recursive(cache_dir) < 0)
		g_warning("can't remove directory '%s'\n", cache_dir);
	g_free(cache_dir);
	folder_item_remove(item);

	return 0;
}

static GSList *imap_get_uncached_messages(IMAPSession *session,
					  FolderItem *item,
					  MsgNumberList *numlist)
{
	gchar *tmp;
	GSList *newlist = NULL;
	GSList *llast = NULL;
	GString *str;
	MsgInfo *msginfo;
	IMAPSet imapset;

	g_return_val_if_fail(session != NULL, NULL);
	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(item->folder != NULL, NULL);
	g_return_val_if_fail(FOLDER_CLASS(item->folder) == &imap_class, NULL);

	imapset = numberlist_to_imapset(numlist);
	while (imapset != NULL) {
		if (imap_cmd_envelope(session, imapset)
		    != IMAP_SUCCESS) {
			log_warning(_("can't get envelope\n"));
			continue;
		}

		str = g_string_new(NULL);

		for (;;) {
			if ((tmp = sock_getline(SESSION(session)->sock)) == NULL) {
				log_warning(_("error occurred while getting envelope.\n"));
				g_string_free(str, TRUE);
				break;
			}
			strretchomp(tmp);
			if (tmp[0] != '*' || tmp[1] != ' ') {
				log_print("IMAP4< %s\n", tmp);
				g_free(tmp);
				break;
	    		}
			if (strstr(tmp, "FETCH") == NULL) {
				log_print("IMAP4< %s\n", tmp);
				g_free(tmp);
				continue;
			}
			log_print("IMAP4< %s\n", tmp);
			g_string_assign(str, tmp);
			g_free(tmp);

			msginfo = imap_parse_envelope
				(SESSION(session)->sock, item, str);
			if (!msginfo) {
				log_warning(_("can't parse envelope: %s\n"), str->str);
				continue;
			}
			if (item->stype == F_QUEUE) {
				MSG_SET_TMP_FLAGS(msginfo->flags, MSG_QUEUED);
			} else if (item->stype == F_DRAFT) {
				MSG_SET_TMP_FLAGS(msginfo->flags, MSG_DRAFT);
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
		imapset = numberlist_to_imapset(NULL);
	}

	return newlist;
}

static void imap_delete_all_cached_messages(FolderItem *item)
{
	gchar *dir;

	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);
	g_return_if_fail(FOLDER_CLASS(item->folder) == &imap_class);

	debug_print("Deleting all cached messages...\n");

	dir = folder_item_get_path(item);
#ifdef WIN32
	subst_char(dir, '/', G_DIR_SEPARATOR);
#endif
	if (is_dir_exist(dir))
		remove_all_numbered_files(dir);
	g_free(dir);

	debug_print("done.\n");
}

#if USE_OPENSSL
static SockInfo *imap_open_tunnel(const gchar *server,
			   const gchar *tunnelcmd,
			   SSLType ssl_type)
#else
static SockInfo *imap_open_tunnel(const gchar *server,
			   const gchar *tunnelcmd)
#endif
{
	SockInfo *sock;

	if ((sock = sock_connect_cmd(server, tunnelcmd)) == NULL) {
		log_warning(_("Can't establish IMAP4 session with: %s\n"),
			    server);
		return NULL;
	}
#if USE_OPENSSL
	return imap_init_sock(sock, ssl_type);
#else
	return imap_init_sock(sock);
#endif
}


#if USE_OPENSSL
static SockInfo *imap_open(const gchar *server, gushort port,
			   SSLType ssl_type)
#else
static SockInfo *imap_open(const gchar *server, gushort port)
#endif
{
	SockInfo *sock;

	if ((sock = sock_connect(server, port)) == NULL) {
		log_warning(_("Can't connect to IMAP4 server: %s:%d\n"),
			    server, port);
		return NULL;
	}

#if USE_OPENSSL
	if (ssl_type == SSL_TUNNEL && !ssl_init_socket(sock)) {
		log_warning(_("Can't establish IMAP4 session with: %s:%d\n"),
			    server, port);
		sock_close(sock);
		return NULL;
	}
#endif
	return sock;
}

#if USE_OPENSSL
static SockInfo *imap_init_sock(SockInfo *sock, SSLType ssl_type)
#else
static SockInfo *imap_init_sock(SockInfo *sock)
#endif
{

	return sock;
}

static GList *imap_parse_namespace_str(gchar *str)
{
	gchar *p = str;
	gchar *name;
	gchar *separator;
	IMAPNameSpace *namespace;
	GList *ns_list = NULL;

	while (*p != '\0') {
		/* parse ("#foo" "/") */

		while (*p && *p != '(') p++;
		if (*p == '\0') break;
		p++;

		while (*p && *p != '"') p++;
		if (*p == '\0') break;
		p++;
		name = p;

		while (*p && *p != '"') p++;
		if (*p == '\0') break;
		*p = '\0';
		p++;

		while (*p && isspace(*p)) p++;
		if (*p == '\0') break;
		if (strncmp(p, "NIL", 3) == 0)
			separator = NULL;
		else if (*p == '"') {
			p++;
			separator = p;
			while (*p && *p != '"') p++;
			if (*p == '\0') break;
			*p = '\0';
			p++;
		} else break;

		while (*p && *p != ')') p++;
		if (*p == '\0') break;
		p++;

		namespace = g_new(IMAPNameSpace, 1);
		namespace->name = g_strdup(name);
		namespace->separator = separator ? separator[0] : '\0';
		ns_list = g_list_append(ns_list, namespace);
	}

	return ns_list;
}

static void imap_parse_namespace(IMAPSession *session, IMAPFolder *folder)
{
	gchar *ns_str;
	gchar **str_array;

	g_return_if_fail(session != NULL);
	g_return_if_fail(folder != NULL);

	if (folder->ns_personal != NULL ||
	    folder->ns_others   != NULL ||
	    folder->ns_shared   != NULL)
		return;

	if (!imap_has_capability(session, "NAMESPACE")) {
		imap_get_namespace_by_list(session, folder);
		return;
	}
	
	if (imap_cmd_namespace(session, &ns_str)
	    != IMAP_SUCCESS) {
		log_warning(_("can't get namespace\n"));
		return;
	}

	str_array = strsplit_parenthesis(ns_str, '(', ')', 3);
	if (str_array == NULL) {
		g_free(ns_str);
		imap_get_namespace_by_list(session, folder);
		return;
	}
	if (str_array[0])
		folder->ns_personal = imap_parse_namespace_str(str_array[0]);
	if (str_array[0] && str_array[1])
		folder->ns_others = imap_parse_namespace_str(str_array[1]);
	if (str_array[0] && str_array[1] && str_array[2])
		folder->ns_shared = imap_parse_namespace_str(str_array[2]);
	g_strfreev(str_array);
	g_free(ns_str);
}

static void imap_get_namespace_by_list(IMAPSession *session, IMAPFolder *folder)
{
	GSList *item_list, *cur;
	gchar separator = '\0';
	IMAPNameSpace *namespace;

	g_return_if_fail(session != NULL);
	g_return_if_fail(folder != NULL);

	if (folder->ns_personal != NULL ||
	    folder->ns_others   != NULL ||
	    folder->ns_shared   != NULL)
		return;

	imap_gen_send(session, "LIST \"\" \"\"");
	item_list = imap_parse_list(folder, session, "", &separator);
	for (cur = item_list; cur != NULL; cur = cur->next)
		folder_item_destroy(FOLDER_ITEM(cur->data));
	g_slist_free(item_list);

	namespace = g_new(IMAPNameSpace, 1);
	namespace->name = g_strdup("");
	namespace->separator = separator;
	folder->ns_personal = g_list_append(NULL, namespace);
}

static IMAPNameSpace *imap_find_namespace_from_list(GList *ns_list,
						    const gchar *path)
{
	IMAPNameSpace *namespace = NULL;
	gchar *tmp_path, *name;

	if (!path) path = "";

	Xstrcat_a(tmp_path, path, "/", return NULL);

	for (; ns_list != NULL; ns_list = ns_list->next) {
		IMAPNameSpace *tmp_ns = ns_list->data;

		Xstrdup_a(name, tmp_ns->name, return namespace);
		if (tmp_ns->separator && tmp_ns->separator != '/')
			subst_char(name, tmp_ns->separator, '/');
		if (strncmp(tmp_path, name, strlen(name)) == 0)
			namespace = tmp_ns;
	}

	return namespace;
}

static IMAPNameSpace *imap_find_namespace(IMAPFolder *folder,
					  const gchar *path)
{
	IMAPNameSpace *namespace;

	g_return_val_if_fail(folder != NULL, NULL);

	namespace = imap_find_namespace_from_list(folder->ns_personal, path);
	if (namespace) return namespace;
	namespace = imap_find_namespace_from_list(folder->ns_others, path);
	if (namespace) return namespace;
	namespace = imap_find_namespace_from_list(folder->ns_shared, path);
	if (namespace) return namespace;

	return NULL;
}

static gchar imap_get_path_separator(IMAPFolder *folder, const gchar *path)
{
	IMAPNameSpace *namespace;
	gchar separator = '/';

	namespace = imap_find_namespace(folder, path);
	if (namespace && namespace->separator)
		separator = namespace->separator;

	return separator;
}

static gchar *imap_get_real_path(IMAPFolder *folder, const gchar *path)
{
	gchar *real_path;
	gchar separator;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(path != NULL, NULL);

	real_path = imap_locale_to_modified_utf7(path);
	separator = imap_get_path_separator(folder, path);
	imap_path_separator_subst(real_path, separator);

	return real_path;
}

static gchar *imap_parse_atom(SockInfo *sock, gchar *src,
			      gchar *dest, gint dest_len, GString *str)
{
	gchar *cur_pos = src;
	gchar *nextline;

	g_return_val_if_fail(str != NULL, cur_pos);

	/* read the next line if the current response buffer is empty */
	while (isspace(*cur_pos)) cur_pos++;
	while (*cur_pos == '\0') {
		if ((nextline = sock_getline(sock)) == NULL)
			return cur_pos;
		g_string_assign(str, nextline);
		cur_pos = str->str;
		strretchomp(nextline);
		/* log_print("IMAP4< %s\n", nextline); */
		debug_print("IMAP4< %s\n", nextline);
		g_free(nextline);

		while (isspace(*cur_pos)) cur_pos++;
	}

	if (!strncmp(cur_pos, "NIL", 3)) {
		*dest = '\0';
		cur_pos += 3;
	} else if (*cur_pos == '\"') {
		gchar *p;

		p = get_quoted(cur_pos, '\"', dest, dest_len);
		cur_pos = p ? p : cur_pos + 2;
	} else if (*cur_pos == '{') {
		gchar buf[32];
		gint len;
		gint line_len = 0;

		cur_pos = strchr_cpy(cur_pos + 1, '}', buf, sizeof(buf));
		len = atoi(buf);
		g_return_val_if_fail(len > 0, cur_pos);

		g_string_truncate(str, 0);
		cur_pos = str->str;

		do {
			if ((nextline = sock_getline(sock)) == NULL)
				return cur_pos;
			line_len += strlen(nextline);
			g_string_append(str, nextline);
			cur_pos = str->str;
			strretchomp(nextline);
			/* log_print("IMAP4< %s\n", nextline); */
			debug_print("IMAP4< %s\n", nextline);
			g_free(nextline);
		} while (line_len < len);

		memcpy(dest, cur_pos, MIN(len, dest_len - 1));
		dest[MIN(len, dest_len - 1)] = '\0';
		cur_pos += len;
	}

	return cur_pos;
}

static gchar *imap_get_header(SockInfo *sock, gchar *cur_pos, gchar **headers,
			      GString *str)
{
	gchar *nextline;
	gchar buf[32];
	gint len;
	gint block_len = 0;

	*headers = NULL;

	g_return_val_if_fail(str != NULL, cur_pos);

	while (isspace(*cur_pos)) cur_pos++;

	g_return_val_if_fail(*cur_pos == '{', cur_pos);

	cur_pos = strchr_cpy(cur_pos + 1, '}', buf, sizeof(buf));
	len = atoi(buf);
	g_return_val_if_fail(len > 0, cur_pos);

	g_string_truncate(str, 0);
	cur_pos = str->str;

	do {
		if ((nextline = sock_getline(sock)) == NULL)
			return cur_pos;
#ifdef WIN32
#ifdef USE_OPENSSL
		if (!sock->ssl)
#endif /* USE_OPENSSL */
		{
			gchar *newstr = g_strdup_printf("%s%c", nextline, 0x0a);
			g_free(nextline);
			nextline=newstr;
		}
#endif /* WIN32 */
		block_len += strlen(nextline);
		g_string_append(str, nextline);
		cur_pos = str->str;
		strretchomp(nextline);
		/* debug_print("IMAP4< %s\n", nextline); */
		g_free(nextline);
	} while (block_len < len);

	debug_print("IMAP4< [contents of BODY.PEEK[HEADER.FIELDS (...)]]\n");

	*headers = g_strndup(cur_pos, len);
	cur_pos += len;

	while (isspace(*cur_pos)) cur_pos++;
	while (*cur_pos == '\0') {
		if ((nextline = sock_getline(sock)) == NULL)
			return cur_pos;
		g_string_assign(str, nextline);
		cur_pos = str->str;
		strretchomp(nextline);
		debug_print("IMAP4< %s\n", nextline);
		g_free(nextline);

		while (isspace(*cur_pos)) cur_pos++;
	}

	return cur_pos;
}

static MsgFlags imap_parse_flags(const gchar *flag_str)  
{
	const gchar *p = flag_str;
	MsgFlags flags = {0, 0};

	flags.perm_flags = MSG_UNREAD;

	while ((p = strchr(p, '\\')) != NULL) {
		p++;

		if (g_strncasecmp(p, "Recent", 6) == 0 && MSG_IS_UNREAD(flags)) {
			MSG_SET_PERM_FLAGS(flags, MSG_NEW);
		} else if (g_strncasecmp(p, "Seen", 4) == 0) {
			MSG_UNSET_PERM_FLAGS(flags, MSG_NEW|MSG_UNREAD);
		} else if (g_strncasecmp(p, "Deleted", 7) == 0) {
			MSG_SET_PERM_FLAGS(flags, MSG_DELETED);
		} else if (g_strncasecmp(p, "Flagged", 7) == 0) {
			MSG_SET_PERM_FLAGS(flags, MSG_MARKED);
		} else if (g_strncasecmp(p, "Answered", 8) == 0) {
			MSG_SET_PERM_FLAGS(flags, MSG_REPLIED);
		}
	}

	return flags;
}

static MsgInfo *imap_parse_envelope(SockInfo *sock, FolderItem *item,
				    GString *line_str)
{
	gchar buf[IMAPBUFSIZE];
	MsgInfo *msginfo = NULL;
	gchar *cur_pos;
	gint msgnum;
	guint32 uid = 0;
	size_t size = 0;
	MsgFlags flags = {0, 0}, imap_flags = {0, 0};

	g_return_val_if_fail(line_str != NULL, NULL);
	g_return_val_if_fail(line_str->str[0] == '*' &&
			     line_str->str[1] == ' ', NULL);

	MSG_SET_TMP_FLAGS(flags, MSG_IMAP);
	if (item->stype == F_QUEUE) {
		MSG_SET_TMP_FLAGS(flags, MSG_QUEUED);
	} else if (item->stype == F_DRAFT) {
		MSG_SET_TMP_FLAGS(flags, MSG_DRAFT);
	}

	cur_pos = line_str->str + 2;

#define PARSE_ONE_ELEMENT(ch)					\
{								\
	cur_pos = strchr_cpy(cur_pos, ch, buf, sizeof(buf));	\
	if (cur_pos == NULL) {					\
		g_warning("cur_pos == NULL\n");			\
		procmsg_msginfo_free(msginfo);			\
		return NULL;					\
	}							\
}

	PARSE_ONE_ELEMENT(' ');
	msgnum = atoi(buf);

	PARSE_ONE_ELEMENT(' ');
	g_return_val_if_fail(!strcmp(buf, "FETCH"), NULL);

	g_return_val_if_fail(*cur_pos == '(', NULL);
	cur_pos++;

	while (*cur_pos != '\0' && *cur_pos != ')') {
		while (*cur_pos == ' ') cur_pos++;

		if (!strncmp(cur_pos, "UID ", 4)) {
			cur_pos += 4;
			uid = strtoul(cur_pos, &cur_pos, 10);
		} else if (!strncmp(cur_pos, "FLAGS ", 6)) {
			cur_pos += 6;
			if (*cur_pos != '(') {
				g_warning("*cur_pos != '('\n");
				procmsg_msginfo_free(msginfo);
				return NULL;
			}
			cur_pos++;
			PARSE_ONE_ELEMENT(')');
			imap_flags = imap_parse_flags(buf);
		} else if (!strncmp(cur_pos, "RFC822.SIZE ", 12)) {
			cur_pos += 12;
			size = strtol(cur_pos, &cur_pos, 10);
		} else if (!strncmp(cur_pos, "BODY[HEADER.FIELDS ", 19)) {
			gchar *headers;

			cur_pos += 19;
			if (*cur_pos != '(') {
				g_warning("*cur_pos != '('\n");
				procmsg_msginfo_free(msginfo);
				return NULL;
			}
			cur_pos++;
			PARSE_ONE_ELEMENT(')');
			if (*cur_pos != ']') {
				g_warning("*cur_pos != ']'\n");
				procmsg_msginfo_free(msginfo);
				return NULL;
			}
			cur_pos++;

			cur_pos = imap_get_header(sock, cur_pos, &headers,
						  line_str);
			msginfo = procheader_parse_str(headers, flags, FALSE, FALSE);
			g_free(headers);
		} else {
			g_warning("invalid FETCH response: %s\n", cur_pos);
			break;
		}
	}

	if (msginfo) {
		msginfo->msgnum = uid;
		msginfo->size = size;
		msginfo->flags.tmp_flags |= imap_flags.tmp_flags;
		msginfo->flags.perm_flags = imap_flags.perm_flags;
	}

	return msginfo;
}

static gint imap_set_message_flags(IMAPSession *session,
				   MsgNumberList *numlist,
				   IMAPFlags flags,
				   gboolean is_set)
{
	GString *buf;
	gint ok;
	IMAPSet imapset;

	buf = g_string_new(is_set ? "+FLAGS (" : "-FLAGS (");

	if (IMAP_IS_SEEN(flags))	g_string_append(buf, "\\Seen ");
	if (IMAP_IS_ANSWERED(flags))	g_string_append(buf, "\\Answered ");
	if (IMAP_IS_FLAGGED(flags))	g_string_append(buf, "\\Flagged ");
	if (IMAP_IS_DELETED(flags))	g_string_append(buf, "\\Deleted ");
	if (IMAP_IS_DRAFT(flags))	g_string_append(buf, "\\Draft");

	if (buf->str[buf->len - 1] == ' ')
		g_string_truncate(buf, buf->len - 1);

	g_string_append_c(buf, ')');

	imapset = numberlist_to_imapset(numlist);
	while (imapset != NULL) {
		ok = imap_cmd_store(session, imapset,
				    buf->str);
		imapset = numberlist_to_imapset(NULL);
	}
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
	gint exists_, recent_, unseen_, uid_validity_;

	if (!exists || !recent || !unseen || !uid_validity) {
		if (session->mbox && strcmp(session->mbox, path) == 0)
			return IMAP_SUCCESS;
		exists = &exists_;
		recent = &recent_;
		unseen = &unseen_;
		uid_validity = &uid_validity_;
	}

	g_free(session->mbox);
	session->mbox = NULL;

	real_path = imap_get_real_path(folder, path);
	ok = imap_cmd_select(session, real_path,
			     exists, recent, unseen, uid_validity);
	if (ok != IMAP_SUCCESS)
		log_warning(_("can't select folder: %s\n"), real_path);
	else {
		session->mbox = g_strdup(path);
		session->folder_content_changed = FALSE;
	}
	g_free(real_path);

	return ok;
}

#define THROW(err) { ok = err; goto catch; }

static gint imap_status(IMAPSession *session, IMAPFolder *folder,
			const gchar *path,
			gint *messages, gint *recent,
			guint32 *uid_next, guint32 *uid_validity,
			gint *unseen)
{
	gchar *real_path;
	gchar *real_path_;
	gint ok;
	GPtrArray *argbuf;
	gchar *str;

	*messages = *recent = *uid_next = *uid_validity = *unseen = 0;

	argbuf = g_ptr_array_new();

	real_path = imap_get_real_path(folder, path);
	QUOTE_IF_REQUIRED(real_path_, real_path);
	imap_gen_send(session, "STATUS %s "
		      "(MESSAGES RECENT UIDNEXT UIDVALIDITY UNSEEN)",
		      real_path_);

	ok = imap_cmd_ok(session, argbuf);
	if (ok != IMAP_SUCCESS) THROW(ok);

	str = search_array_str(argbuf, "STATUS");
	if (!str) THROW(IMAP_ERROR);

	str = strchr(str, '(');
	if (!str) THROW(IMAP_ERROR);
	str++;
	while (*str != '\0' && *str != ')') {
		while (*str == ' ') str++;

		if (!strncmp(str, "MESSAGES ", 9)) {
			str += 9;
			*messages = strtol(str, &str, 10);
		} else if (!strncmp(str, "RECENT ", 7)) {
			str += 7;
			*recent = strtol(str, &str, 10);
		} else if (!strncmp(str, "UIDNEXT ", 8)) {
			str += 8;
			*uid_next = strtoul(str, &str, 10);
		} else if (!strncmp(str, "UIDVALIDITY ", 12)) {
			str += 12;
			*uid_validity = strtoul(str, &str, 10);
		} else if (!strncmp(str, "UNSEEN ", 7)) {
			str += 7;
			*unseen = strtol(str, &str, 10);
		} else {
			g_warning("invalid STATUS response: %s\n", str);
			break;
		}
	}

catch:
	g_free(real_path);
	ptr_array_free_strings(argbuf);
	g_ptr_array_free(argbuf, TRUE);

	return ok;
}

#undef THROW


/* low-level IMAP4rev1 commands */

static gint imap_cmd_login(IMAPSession *session,
			   const gchar *user, const gchar *pass)
{
	gchar *user_, *pass_;
	gint ok;

	QUOTE_IF_REQUIRED(user_, user);
	QUOTE_IF_REQUIRED(pass_, pass);
	imap_gen_send(session, "LOGIN %s %s", user_, pass_);

	ok = imap_cmd_ok(session, NULL);
	if (ok != IMAP_SUCCESS)
		log_warning(_("IMAP4 login failed.\n"));

	return ok;
}

static gint imap_cmd_logout(IMAPSession *session)
{
	imap_gen_send(session, "LOGOUT");
	return imap_cmd_ok(session, NULL);
}

/* Send CAPABILITY, and examine the server's response to see whether this
 * connection is pre-authenticated or not and build a list of CAPABILITIES. */
static gint imap_greeting(IMAPSession *session)
{
	gchar *capstr;
	GPtrArray *argbuf;

	imap_gen_send(session, "CAPABILITY");
	
	argbuf = g_ptr_array_new();

	if (imap_cmd_ok(session, argbuf) != IMAP_SUCCESS ||
	    ((capstr = search_array_str(argbuf, "CAPABILITY ")) == NULL)) {
		ptr_array_free_strings(argbuf);
		g_ptr_array_free(argbuf, TRUE);
		return -1;
	}

	session->authenticated = search_array_str(argbuf, "PREAUTH") != NULL;
	
	capstr += strlen("CAPABILITY ");

	IMAP_SESSION(session)->capability = g_strsplit(capstr, " ", 0);
	
	ptr_array_free_strings(argbuf);
	g_ptr_array_free(argbuf, TRUE);

	return 0;
}

static gboolean imap_has_capability(IMAPSession *session, const gchar *cap)
{
	gchar **p;
	
	for (p = session->capability; *p != NULL; ++p)
		if (g_strcasecmp(*p, cap) == 0)
			return TRUE;

	return FALSE;
}

void imap_free_capabilities(IMAPSession *session)
{
	g_strfreev(session->capability);
	session->capability = NULL;
}

static const IMAPSet numberlist_to_imapset(MsgNumberList *list)
{
	static GString *imapset = NULL;
	static MsgNumberList *numlist, *elem;
	guint first, last, next;

	if (imapset == NULL)
		imapset = g_string_sized_new(256);
	else
		g_string_truncate(imapset, 0);

	if (list != NULL) {
		g_slist_free(numlist);
		numlist = g_slist_copy(list);
		numlist = g_slist_sort(numlist, g_int_compare);
	} else if (numlist == NULL) {
		return NULL;
	}

	first = GPOINTER_TO_INT(numlist->data);
	last = first;
	for(elem = g_slist_next(numlist); elem != NULL; elem = g_slist_next(elem)) {
		next = GPOINTER_TO_INT(elem->data);

		if(next != (last + 1)) {
			if (imapset->len > 0)
				g_string_append(imapset, ",");
			if (first == last)
				g_string_sprintfa(imapset, "%d", first);
			else
				g_string_sprintfa(imapset, "%d:%d", first, last);

			if (imapset->len > IMAPCMDLIMIT) {
				last = 0;
				break;
			}

			first = next;
		}
		last = next;
	}
	if (last != 0) {
		if (imapset->len > 0)
			g_string_append(imapset, ",");
		if (first == last)
			g_string_sprintfa(imapset, "%d", first);
		else
			g_string_sprintfa(imapset, "%d:%d", first, last);

		g_slist_free(numlist);
		numlist = NULL;
	} else {
		MsgNumberList *remaining;

		remaining = elem->next;
		remaining = g_slist_prepend(remaining, elem->data);
		elem->next = NULL;
		g_slist_free(numlist);
		numlist = remaining;
	}

	return imapset->str;
}

static gint imap_cmd_noop(IMAPSession *session)
{
	imap_gen_send(session, "NOOP");
	return imap_cmd_ok(session, NULL);
}

static gint imap_cmd_starttls(IMAPSession *session)
{
	imap_gen_send(session, "STARTTLS");
	return imap_cmd_ok(session, NULL);
}

#define THROW(err) { ok = err; goto catch; }

static gint imap_cmd_namespace(IMAPSession *session, gchar **ns_str)
{
	gint ok;
	GPtrArray *argbuf;
	gchar *str;

	argbuf = g_ptr_array_new();

	imap_gen_send(session, "NAMESPACE");
	if ((ok = imap_cmd_ok(session, argbuf)) != IMAP_SUCCESS) THROW(ok);

	str = search_array_str(argbuf, "NAMESPACE");
	if (!str) THROW(IMAP_ERROR);

	*ns_str = g_strdup(str);

catch:
	ptr_array_free_strings(argbuf);
	g_ptr_array_free(argbuf, TRUE);

	return ok;
}

#undef THROW

static gint imap_cmd_list(IMAPSession *session, const gchar *ref,
			  const gchar *mailbox, GPtrArray *argbuf)
{
	gchar *ref_, *mailbox_;

	if (!ref) ref = "\"\"";
	if (!mailbox) mailbox = "\"\"";

	QUOTE_IF_REQUIRED(ref_, ref);
	QUOTE_IF_REQUIRED(mailbox_, mailbox);
	imap_gen_send(session, "LIST %s %s", ref_, mailbox_);

	return imap_cmd_ok(session, argbuf);
}

#define THROW goto catch

static gint imap_cmd_do_select(IMAPSession *session, const gchar *folder,
			       gboolean examine,
			       gint *exists, gint *recent, gint *unseen,
			       guint32 *uid_validity)
{
	gint ok;
	gchar *resp_str;
	GPtrArray *argbuf;
	gchar *select_cmd;
	gchar *folder_;

	*exists = *recent = *unseen = *uid_validity = 0;
	argbuf = g_ptr_array_new();

	if (examine)
		select_cmd = "EXAMINE";
	else
		select_cmd = "SELECT";

	QUOTE_IF_REQUIRED(folder_, folder);
	imap_gen_send(session, "%s %s", select_cmd, folder_);

	if ((ok = imap_cmd_ok(session, argbuf)) != IMAP_SUCCESS) THROW;

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

static gint imap_cmd_select(IMAPSession *session, const gchar *folder,
			    gint *exists, gint *recent, gint *unseen,
			    guint32 *uid_validity)
{
	return imap_cmd_do_select(session, folder, FALSE,
				  exists, recent, unseen, uid_validity);
}

static gint imap_cmd_examine(IMAPSession *session, const gchar *folder,
			     gint *exists, gint *recent, gint *unseen,
			     guint32 *uid_validity)
{
	return imap_cmd_do_select(session, folder, TRUE,
				  exists, recent, unseen, uid_validity);
}

#undef THROW

static gint imap_cmd_create(IMAPSession *session, const gchar *folder)
{
	gchar *folder_;

	QUOTE_IF_REQUIRED(folder_, folder);
	imap_gen_send(session, "CREATE %s", folder_);

	return imap_cmd_ok(session, NULL);
}

static gint imap_cmd_rename(IMAPSession *session, const gchar *old_folder,
			    const gchar *new_folder)
{
	gchar *old_folder_, *new_folder_;

	QUOTE_IF_REQUIRED(old_folder_, old_folder);
	QUOTE_IF_REQUIRED(new_folder_, new_folder);
	imap_gen_send(session, "RENAME %s %s", old_folder_, new_folder_);

	return imap_cmd_ok(session, NULL);
}

static gint imap_cmd_delete(IMAPSession *session, const gchar *folder)
{
	gchar *folder_;

	QUOTE_IF_REQUIRED(folder_, folder);
	imap_gen_send(session, "DELETE %s", folder_);

	return imap_cmd_ok(session, NULL);
}

static gint imap_cmd_search(IMAPSession *session, const gchar *criteria, GSList **list)
{
	gint ok;
	gchar *uidlist;
	GPtrArray *argbuf;

	g_return_val_if_fail(criteria != NULL, IMAP_ERROR);
	g_return_val_if_fail(list != NULL, IMAP_ERROR);

	*list = NULL;
	
	argbuf = g_ptr_array_new();
	imap_gen_send(session, "UID SEARCH %s", criteria);

	ok = imap_cmd_ok(session, argbuf);
	if (ok != IMAP_SUCCESS) {
		ptr_array_free_strings(argbuf);
		g_ptr_array_free(argbuf, TRUE);
		return ok;
	}

	if ((uidlist = search_array_str(argbuf, "SEARCH ")) != NULL) {
		gchar **strlist, **p;

		strlist = g_strsplit(uidlist + 7, " ", 0);
		for (p = strlist; *p != NULL; ++p) {
			guint msgnum;

			if (sscanf(*p, "%d", &msgnum) == 1)
				*list = g_slist_append(*list, GINT_TO_POINTER(msgnum));
		}
		g_strfreev(strlist);
	}
	ptr_array_free_strings(argbuf);
	g_ptr_array_free(argbuf, TRUE);

	return IMAP_SUCCESS;
}

static gint imap_cmd_fetch(IMAPSession *session, guint32 uid, const gchar *filename)
{
	gint ok;
	gchar *buf;
	gchar *cur_pos;
	gchar size_str[32];
	glong size_num;

	g_return_val_if_fail(filename != NULL, IMAP_ERROR);

	imap_gen_send(session, "UID FETCH %d BODY.PEEK[]", uid);

	while ((ok = imap_gen_recv(session, &buf))
	       == IMAP_SUCCESS) {
		if (buf[0] != '*' || buf[1] != ' ') {
			g_free(buf);
			return IMAP_ERROR;
		}
		if (strstr(buf, "FETCH") != NULL)
			break;
	}
	if (ok != IMAP_SUCCESS)
		return ok;

	cur_pos = strchr(buf, '{');
	if (cur_pos == NULL) {
		g_free(buf);
		return IMAP_ERROR;
	}
	cur_pos = strchr_cpy(cur_pos + 1, '}', size_str, sizeof(size_str));
	if (cur_pos == NULL) {
		g_free(buf);
		return IMAP_ERROR;
	}
	size_num = atol(size_str);
	g_return_val_if_fail(size_num > 0, IMAP_ERROR);

	if (*cur_pos != '\0') {
		g_free(buf);
		return IMAP_ERROR;
	}

	if (recv_bytes_write_to_file(SESSION(session)->sock, size_num, filename) != 0) {
		g_free(buf);
		return IMAP_ERROR;
	}

	if (imap_gen_recv(session, &buf) != IMAP_SUCCESS) {
		g_free(buf);
		return IMAP_ERROR;
	}

	if (buf[0] == '\0' || buf[strlen(buf) - 1] != ')') {
		g_free(buf);
		return IMAP_ERROR;
	}

	g_free(buf);
	ok = imap_cmd_ok(session, NULL);

	return ok;
}

static gint imap_cmd_append(IMAPSession *session, const gchar *destfolder,
			    const gchar *file, gint32 *new_uid)
{
	gint ok;
	gint size, newuid;
	gchar *destfolder_;
	gchar buf[BUFFSIZE], *imapbuf;
	FILE *fp;
	GPtrArray *reply;
	gchar *okmsginfo;

	g_return_val_if_fail(file != NULL, IMAP_ERROR);

	size = get_file_size_as_crlf(file);
	if ((fp = fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		return -1;
	}
	QUOTE_IF_REQUIRED(destfolder_, destfolder);
	imap_gen_send(session, "APPEND %s (\\Seen) {%d}", destfolder_, size);

	ok = imap_gen_recv(session, &imapbuf);
	if (ok != IMAP_SUCCESS || imapbuf[0] != '+' || imapbuf[1] != ' ') {
		log_warning(_("can't append %s to %s\n"), file, destfolder_);
		g_free(imapbuf);
		fclose(fp);
		return IMAP_ERROR;
	}
	g_free(imapbuf);

	log_print("IMAP4> %s\n", _("(sending file...)"));

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		strretchomp(buf);
		if (sock_puts(SESSION(session)->sock, buf) < 0) {
			fclose(fp);
			return -1;
		}
	}

	if (ferror(fp)) {
		FILE_OP_ERROR(file, "fgets");
		fclose(fp);
		return -1;
	}

	sock_puts(SESSION(session)->sock, "");

	fclose(fp);

	reply = g_ptr_array_new();

	*new_uid = 0;
	ok = imap_cmd_ok(session, reply);
	if (ok != IMAP_SUCCESS)
		log_warning(_("can't append message to %s\n"), destfolder_);
	else if (
	    (new_uid != NULL) && 
	    (imap_has_capability(session, "UIDPLUS") && reply->len > 0) &&
	    ((okmsginfo = g_ptr_array_index(reply, reply->len - 1)) != NULL) &&
	    (sscanf(okmsginfo, "%*u OK [APPENDUID %*u %u]", &newuid) == 1)) {
		*new_uid = newuid;
	}

	ptr_array_free_strings(reply);
	g_ptr_array_free(reply, TRUE);
	return ok;
}


static gint imap_cmd_copy(IMAPSession * session,
			  gint32 msgnum,
			  const gchar * destfolder, gint32 * new_uid)
{
	gint ok;
	gint32 olduid, newuid;
	gchar *okmsginfo;
	gchar *destfolder_;
	GPtrArray *reply;

	g_return_val_if_fail(destfolder != NULL, IMAP_ERROR);
	g_return_val_if_fail(session != NULL, IMAP_ERROR);
	g_return_val_if_fail(new_uid != NULL, IMAP_ERROR);

	QUOTE_IF_REQUIRED(destfolder_, destfolder);
	imap_gen_send(session, "UID COPY %d %s", msgnum, destfolder_);

	reply = g_ptr_array_new();

	*new_uid = 0;
	ok = imap_cmd_ok(session, reply);
	if (ok != IMAP_SUCCESS)
		log_warning(_("can't copy %d to %s\n"), msgnum, destfolder_);
	else if (imap_has_capability(session, "UIDPLUS") && reply->len > 0)
		if ((okmsginfo = g_ptr_array_index(reply, reply->len - 1)) != NULL &&
		    sscanf(okmsginfo, "%*u OK [COPYUID %*u %u %u]", &olduid, &newuid) == 2 &&
		    olduid == msgnum)
			*new_uid = newuid;

	ptr_array_free_strings(reply);
	g_ptr_array_free(reply, TRUE);
	return ok;
}

gint imap_cmd_envelope(IMAPSession *session, IMAPSet set)
{
	static GString *header_fields = NULL;

	if (header_fields == NULL) {
		const HeaderEntry *headers, *elem;

		headers = procheader_get_headernames(FALSE);
		header_fields = g_string_new("");

		for (elem = headers; elem->name != NULL; ++elem) {
			gint namelen = strlen(elem->name);

			/* Header fields ending with space are not rfc822 headers */
			if (elem->name[namelen - 1] == ' ')
				continue;

			/* strip : at the of header field */
			if(elem->name[namelen - 1] == ':')
				namelen--;
			
			if (namelen <= 0)
				continue;

			g_string_sprintfa(header_fields, "%s%.*s",
					header_fields->str[0] != '\0' ? " " : "",
					namelen, elem->name);
		}
	}

	imap_gen_send
		(session, "UID FETCH %s (UID FLAGS RFC822.SIZE BODY.PEEK[HEADER.FIELDS (%s)])",
		 set, header_fields->str);

	return IMAP_SUCCESS;
}

static gint imap_cmd_store(IMAPSession *session, IMAPSet set,
			   gchar *sub_cmd)
{
	gint ok;

	imap_gen_send(session, "UID STORE %s %s",
			  set, sub_cmd);

	if ((ok = imap_cmd_ok(session, NULL)) != IMAP_SUCCESS) {
		log_warning(_("error while imap command: STORE %s %s\n"),
			    set, sub_cmd);
		return ok;
	}

	return IMAP_SUCCESS;
}

static gint imap_cmd_expunge(IMAPSession *session)
{
	gint ok;

	imap_gen_send(session, "EXPUNGE");
	if ((ok = imap_cmd_ok(session, NULL)) != IMAP_SUCCESS) {
		log_warning(_("error while imap command: EXPUNGE\n"));
		return ok;
	}

	return IMAP_SUCCESS;
}

static gint imap_cmd_ok(IMAPSession *session, GPtrArray *argbuf)
{
	gint ok;
	gchar *buf;
	gint cmd_num;
	gchar *data;

	while ((ok = imap_gen_recv(session, &buf))
	       == IMAP_SUCCESS) {
		// make sure data is long enough for any substring of buf
		data = alloca(strlen(buf) + 1);

		if (buf[0] == '*' && buf[1] == ' ') {
			gint num;
			if (argbuf)
				g_ptr_array_add(argbuf, g_strdup(buf + 2));

			if (sscanf(buf + 2, "%d %s", &num, data) < 2)
				continue;

			if (!strcmp(data, "EXISTS")) {
				session->exists = num;
				session->folder_content_changed = TRUE;
			}

			if(!strcmp(data, "EXPUNGE")) {
				session->exists--;
				session->folder_content_changed = TRUE;
			}

			continue;
		}

		if (sscanf(buf, "%d %s", &cmd_num, data) < 2) {
			g_free(buf);
			return IMAP_ERROR;
		} else if (cmd_num == session->cmd_count &&
			 !strcmp(data, "OK")) {
			if (argbuf)
				g_ptr_array_add(argbuf, g_strdup(buf));
			g_free(buf);
			return IMAP_SUCCESS;
		} else {
			g_free(buf);
			return IMAP_ERROR;
		}
	}
	g_free(buf);

	return ok;
}

static void imap_gen_send(IMAPSession *session, const gchar *format, ...)
{
	gchar *buf;
	gchar *tmp;
	gchar *p;
	va_list args;

	va_start(args, format);
	tmp = g_strdup_vprintf(format, args);
	va_end(args);

	session->cmd_count++;

	buf = g_strdup_printf("%d %s\r\n", session->cmd_count, tmp);
	if (!strncasecmp(tmp, "LOGIN ", 6) && (p = strchr(tmp + 6, ' '))) {
		*p = '\0';
		log_print("IMAP4> %d %s ********\n", session->cmd_count, tmp);
	} else
		log_print("IMAP4> %d %s\n", session->cmd_count, tmp);

	sock_write_all(SESSION(session)->sock, buf, strlen(buf));
	g_free(tmp);
	g_free(buf);
}

static gint imap_gen_recv(IMAPSession *session, gchar **buf)
{
	if ((*buf = sock_getline(SESSION(session)->sock)) == NULL)
		return IMAP_SOCKET;

	strretchomp(*buf);

	log_print("IMAP4< %s\n", *buf);

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

static gchar *get_quoted(const gchar *src, gchar ch, gchar *dest, gint len)
{
	const gchar *p = src;
	gint n = 0;

	g_return_val_if_fail(*p == ch, NULL);

	*dest = '\0';
	p++;

	while (*p != '\0' && *p != ch) {
		if (n < len - 1) {
			if (*p == '\\' && *(p + 1) != '\0')
				p++;
			*dest++ = *p++;
		} else
			p++;
		n++;
	}

	*dest = '\0';
	return (gchar *)(*p == ch ? p + 1 : p);
}

static gchar *search_array_contain_str(GPtrArray *array, const gchar *str)
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

static gchar *search_array_str(GPtrArray *array, const gchar *str)
{
	gint i;
	gint len;

	len = strlen(str);

	for (i = 0; i < array->len; i++) {
		gchar *tmp;

		tmp = g_ptr_array_index(array, i);
		if (!strncmp(tmp, str, len))
			return tmp;
	}

	return NULL;
}

static void imap_path_separator_subst(gchar *str, gchar separator)
{
	gchar *p;
	gboolean in_escape = FALSE;

	if (!separator || separator == '/') return;

	for (p = str; *p != '\0'; p++) {
		if (*p == '/' && !in_escape)
			*p = separator;
		else if (*p == '&' && *(p + 1) != '-' && !in_escape)
			in_escape = TRUE;
		else if (*p == '-' && in_escape)
			in_escape = FALSE;
	}
}

static gchar *imap_modified_utf7_to_locale(const gchar *mutf7_str)
{
#if !HAVE_ICONV
	const gchar *from_p;
	gchar *to, *to_p;

	to = g_malloc(strlen(mutf7_str) + 1);
	to_p = to;

	for (from_p = mutf7_str; *from_p != '\0'; from_p++) {
		if (*from_p == '&' && *(from_p + 1) == '-') {
			*to_p++ = '&';
			from_p++;
		} else
			*to_p++ = *from_p;
	}
	*to_p = '\0';

	return to;
#else
	static iconv_t cd = (iconv_t)-1;
	static gboolean iconv_ok = TRUE;
	GString *norm_utf7;
	gchar *norm_utf7_p;
	size_t norm_utf7_len;
	const gchar *p;
	gchar *to_str, *to_p;
	size_t to_len;
	gboolean in_escape = FALSE;

	if (!iconv_ok) return g_strdup(mutf7_str);

	if (cd == (iconv_t)-1) {
		cd = iconv_open(conv_get_current_charset_str(), "UTF-7");
		if (cd == (iconv_t)-1) {
			g_warning("iconv cannot convert UTF-7 to %s\n",
				  conv_get_current_charset_str());
			iconv_ok = FALSE;
			return g_strdup(mutf7_str);
		}
	}

	norm_utf7 = g_string_new(NULL);

	for (p = mutf7_str; *p != '\0'; p++) {
		/* replace: '&'  -> '+',
			    "&-" -> '&',
			    escaped ','  -> '/' */
		if (!in_escape && *p == '&') {
			if (*(p + 1) != '-') {
				g_string_append_c(norm_utf7, '+');
				in_escape = TRUE;
			} else {
				g_string_append_c(norm_utf7, '&');
				p++;
			}
		} else if (in_escape && *p == ',') {
			g_string_append_c(norm_utf7, '/');
		} else if (in_escape && *p == '-') {
			g_string_append_c(norm_utf7, '-');
			in_escape = FALSE;
		} else {
			g_string_append_c(norm_utf7, *p);
		}
	}

	norm_utf7_p = norm_utf7->str;
	norm_utf7_len = norm_utf7->len;
	to_len = strlen(mutf7_str) * 5;
	to_p = to_str = g_malloc(to_len + 1);

	if (iconv(cd, (ICONV_CONST gchar **)&norm_utf7_p, &norm_utf7_len,
		  &to_p, &to_len) == -1) {
		g_warning(_("iconv cannot convert UTF-7 to %s\n"),
			  conv_get_current_charset_str());
		g_string_free(norm_utf7, TRUE);
		g_free(to_str);
		return g_strdup(mutf7_str);
	}

	/* second iconv() call for flushing */
	iconv(cd, NULL, NULL, &to_p, &to_len);
	g_string_free(norm_utf7, TRUE);
	*to_p = '\0';

	return to_str;
#endif /* !HAVE_ICONV */
}

static gchar *imap_locale_to_modified_utf7(const gchar *from)
{
#if !HAVE_ICONV
	const gchar *from_p;
	gchar *to, *to_p;

	to = g_malloc(strlen(from) * 2 + 1);
	to_p = to;

	for (from_p = from; *from_p != '\0'; from_p++) {
		if (*from_p == '&') {
			*to_p++ = '&';
			*to_p++ = '-';
		} else
			*to_p++ = *from_p;
	}
	*to_p = '\0';

	return to;
#else
	static iconv_t cd = (iconv_t)-1;
	static gboolean iconv_ok = TRUE;
	gchar *norm_utf7, *norm_utf7_p;
	size_t from_len, norm_utf7_len;
	GString *to_str;
	gchar *from_tmp, *to, *p;
	gboolean in_escape = FALSE;

	if (!iconv_ok) return g_strdup(from);

	if (cd == (iconv_t)-1) {
		cd = iconv_open("UTF-7", conv_get_current_charset_str());
		if (cd == (iconv_t)-1) {
			g_warning("iconv cannot convert %s to UTF-7\n",
				  conv_get_current_charset_str());
			iconv_ok = FALSE;
			return g_strdup(from);
		}
	}

	Xstrdup_a(from_tmp, from, return g_strdup(from));
	from_len = strlen(from);
	norm_utf7_len = from_len * 5;
	Xalloca(norm_utf7, norm_utf7_len + 1, return g_strdup(from));
	norm_utf7_p = norm_utf7;

#define IS_PRINT(ch) (isprint(ch) && isascii(ch))

	while (from_len > 0) {
		if (*from_tmp == '+') {
			*norm_utf7_p++ = '+';
			*norm_utf7_p++ = '-';
			norm_utf7_len -= 2;
			from_tmp++;
			from_len--;
		} else if (IS_PRINT(*from_tmp)) {
			/* printable ascii char */
			*norm_utf7_p = *from_tmp;
			norm_utf7_p++;
			norm_utf7_len--;
			from_tmp++;
			from_len--;
		} else {
			size_t mb_len = 0, conv_len = 0;

			/* unprintable char: convert to UTF-7 */
			p = from_tmp;
			while (!IS_PRINT(*p) && conv_len < from_len) {
				mb_len = mblen(p, MB_LEN_MAX);
				if (mb_len <= 0) {
					g_warning("wrong multibyte sequence\n");
					return g_strdup(from);
				}
				conv_len += mb_len;
				p += mb_len;
			}

			from_len -= conv_len;
			if (iconv(cd, (ICONV_CONST gchar **)&from_tmp,
				  &conv_len,
				  &norm_utf7_p, &norm_utf7_len) == -1) {
				g_warning("iconv cannot convert %s to UTF-7\n",
					  conv_get_current_charset_str());
				return g_strdup(from);
			}

			/* second iconv() call for flushing */
			iconv(cd, NULL, NULL, &norm_utf7_p, &norm_utf7_len);
		}
	}

#undef IS_PRINT

	*norm_utf7_p = '\0';
	to_str = g_string_new(NULL);
	for (p = norm_utf7; p < norm_utf7_p; p++) {
		/* replace: '&' -> "&-",
			    '+' -> '&',
			    "+-" -> '+',
			    BASE64 '/' -> ',' */
		if (!in_escape && *p == '&') {
			g_string_append(to_str, "&-");
		} else if (!in_escape && *p == '+') {
			if (*(p + 1) == '-') {
				g_string_append_c(to_str, '+');
				p++;
			} else {
				g_string_append_c(to_str, '&');
				in_escape = TRUE;
			}
		} else if (in_escape && *p == '/') {
			g_string_append_c(to_str, ',');
		} else if (in_escape && *p == '-') {
			g_string_append_c(to_str, '-');
			in_escape = FALSE;
		} else {
			g_string_append_c(to_str, *p);
		}
	}

	if (in_escape) {
		in_escape = FALSE;
		g_string_append_c(to_str, '-');
	}

	to = to_str->str;
	g_string_free(to_str, FALSE);

	return to;
#endif /* !HAVE_ICONV */
}

static gboolean imap_rename_folder_func(GNode *node, gpointer data)
{
	FolderItem *item = node->data;
	gchar **paths = data;
	const gchar *oldpath = paths[0];
	const gchar *newpath = paths[1];
	gchar *base;
	gchar *new_itempath;
	gint oldpathlen;

	oldpathlen = strlen(oldpath);
	if (strncmp(oldpath, item->path, oldpathlen) != 0) {
		g_warning("path doesn't match: %s, %s\n", oldpath, item->path);
		return TRUE;
	}

	base = item->path + oldpathlen;
	while (*base == G_DIR_SEPARATOR) base++;
	if (*base == '\0')
		new_itempath = g_strdup(newpath);
	else
		new_itempath = g_strconcat(newpath, G_DIR_SEPARATOR_S, base,
					   NULL);
	g_free(item->path);
	item->path = new_itempath;

	return FALSE;
}

static gint get_list_of_uids(Folder *folder, IMAPFolderItem *item, GSList **msgnum_list)
{
	gint ok, nummsgs = 0, lastuid_old;
	IMAPSession *session;
	GSList *uidlist, *elem;
	gchar *cmd_buf;

	session = imap_session_get(folder);
	g_return_val_if_fail(session != NULL, -1);

	ok = imap_select(session, IMAP_FOLDER(folder), item->item.path,
			 NULL, NULL, NULL, NULL);
	if (ok != IMAP_SUCCESS)
		return -1;

	cmd_buf = g_strdup_printf("UID %d:*", item->lastuid + 1);
	ok = imap_cmd_search(session, cmd_buf, &uidlist);
	g_free(cmd_buf);

	if (ok == IMAP_SOCKET) {
		session_destroy((Session *)session);
		((RemoteFolder *)folder)->session = NULL;
		return -1;
	}

	if (ok != IMAP_SUCCESS) {
		gint i;
		GPtrArray *argbuf;

		argbuf = g_ptr_array_new();

		cmd_buf = g_strdup_printf("UID FETCH %d:* (UID)", item->lastuid + 1);
		imap_gen_send(session, cmd_buf);
		g_free(cmd_buf);
		ok = imap_cmd_ok(session, argbuf);
		if (ok != IMAP_SUCCESS) {
			ptr_array_free_strings(argbuf);
			g_ptr_array_free(argbuf, TRUE);
			return -1;
		}
	
		for(i = 0; i < argbuf->len; i++) {
			int ret, msgnum;
	
			if((ret = sscanf(g_ptr_array_index(argbuf, i), 
				    "%*d FETCH (UID %d)", &msgnum)) == 1)
				uidlist = g_slist_prepend(uidlist, GINT_TO_POINTER(msgnum));
		}
		ptr_array_free_strings(argbuf);
		g_ptr_array_free(argbuf, TRUE);
	}

	lastuid_old = item->lastuid;
	*msgnum_list = g_slist_copy(item->uid_list);
	nummsgs = g_slist_length(*msgnum_list);
	debug_print("Got %d uids from cache\n", g_slist_length(item->uid_list));

	for (elem = uidlist; elem != NULL; elem = g_slist_next(elem)) {
		guint msgnum;

		msgnum = GPOINTER_TO_INT(elem->data);
		if (msgnum > lastuid_old) {
			*msgnum_list = g_slist_prepend(*msgnum_list, GINT_TO_POINTER(msgnum));
			item->uid_list = g_slist_prepend(item->uid_list, GINT_TO_POINTER(msgnum));
			nummsgs++;

			if(msgnum > item->lastuid)
				item->lastuid = msgnum;
		}
	}
	g_slist_free(uidlist);

	return nummsgs;
}

gint imap_get_num_list(Folder *folder, FolderItem *_item, GSList **msgnum_list)
{
	IMAPFolderItem *item = (IMAPFolderItem *)_item;
	IMAPSession *session;
	gint ok, nummsgs = 0, exists, recent, unseen, uid_val, uid_next;
	GSList *uidlist;
	gchar *dir;
	gboolean selected_folder;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(item != NULL, -1);
	g_return_val_if_fail(item->item.path != NULL, -1);
	g_return_val_if_fail(FOLDER_CLASS(folder) == &imap_class, -1);
	g_return_val_if_fail(folder->account != NULL, -1);

	session = imap_session_get(folder);
	g_return_val_if_fail(session != NULL, -1);

	selected_folder = (session->mbox != NULL) &&
			  (!strcmp(session->mbox, item->item.path));
	if (selected_folder) {
		ok = imap_cmd_noop(session);
		if (ok != IMAP_SUCCESS)
			return -1;
		exists = session->exists;
	} else {
		ok = imap_status(session, IMAP_FOLDER(folder), item->item.path,
				 &exists, &recent, &uid_next, &uid_val, &unseen);
		if (ok != IMAP_SUCCESS)
			return -1;
	}

	/* If old uid_next matches new uid_next we can be sure no message
	   was added to the folder */
	if (( selected_folder && !session->folder_content_changed) ||
	    (!selected_folder && uid_next == item->uid_next)) {
		nummsgs = g_slist_length(item->uid_list);

		/* If number of messages is still the same we
                   know our caches message numbers are still valid,
                   otherwise if the number of messages has decrease
		   we discard our cache to start a new scan to find
		   out which numbers have been removed */
		if (exists == nummsgs) {
			*msgnum_list = g_slist_copy(item->uid_list);
			return nummsgs;
		} else if (exists < nummsgs) {
			debug_print("Freeing imap uid cache");
			item->lastuid = 0;
			g_slist_free(item->uid_list);
			item->uid_list = NULL;
		}
	}
	if (!selected_folder)
		item->uid_next = uid_next;

	if (exists == 0) {
		*msgnum_list = NULL;
		return 0;
	}

	nummsgs = get_list_of_uids(folder, item, &uidlist);

	if (nummsgs != exists) {
		/* Cache contains more messages then folder, we have cached
                   an old UID of a message that was removed and new messages
                   have been added too, otherwise the uid_next check would
		   not have failed */
		debug_print("Freeing imap uid cache");
		item->lastuid = 0;
		g_slist_free(item->uid_list);
		item->uid_list = NULL;

		g_slist_free(*msgnum_list);

		nummsgs = get_list_of_uids(folder, item, &uidlist);
	}

	*msgnum_list = uidlist;

	dir = folder_item_get_path((FolderItem *)item);
	debug_print("removing old messages from %s\n", dir);
	remove_numbered_files_not_in_list(dir, *msgnum_list);
	g_free(dir);

	return nummsgs;
}

static MsgInfo *imap_parse_msg(const gchar *file, FolderItem *item)
{
	MsgInfo *msginfo;
	MsgFlags flags;

	flags.perm_flags = MSG_NEW|MSG_UNREAD;
	flags.tmp_flags = 0;

	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(file != NULL, NULL);

	if (item->stype == F_QUEUE) {
		MSG_SET_TMP_FLAGS(flags, MSG_QUEUED);
	} else if (item->stype == F_DRAFT) {
		MSG_SET_TMP_FLAGS(flags, MSG_DRAFT);
	}

	msginfo = procheader_parse_file(file, flags, FALSE, FALSE);
	if (!msginfo) return NULL;

	msginfo->folder = item;

	return msginfo;
}

GSList *imap_get_msginfos(Folder *folder, FolderItem *item, GSList *msgnum_list)
{
	IMAPSession *session;
	MsgInfoList *ret = NULL;
	gint ok;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(msgnum_list != NULL, NULL);

	session = imap_session_get(folder);
	g_return_val_if_fail(session != NULL, NULL);

	ok = imap_select(session, IMAP_FOLDER(folder), item->path,
			 NULL, NULL, NULL, NULL);
	if (ok != IMAP_SUCCESS)
		return NULL;

	if (!(item->stype == F_QUEUE || item->stype == F_DRAFT)) {
		ret = g_slist_concat(ret,
			imap_get_uncached_messages(
			session, item, msgnum_list));
	} else {
		MsgNumberList *sorted_list, *elem;
		gint startnum, lastnum;

 		sorted_list = g_slist_sort(g_slist_copy(msgnum_list), g_int_compare);

		startnum = lastnum = GPOINTER_TO_INT(sorted_list->data);

		for (elem = sorted_list;; elem = g_slist_next(elem)) {
			guint num;

			if (elem)
				num = GPOINTER_TO_INT(elem->data);

			if (num > lastnum + 1 || elem == NULL) {
				int i;
				for (i = startnum; i <= lastnum; ++i) {
					gchar *file;
			
					file = imap_fetch_msg(folder, item, i);
					if (file != NULL) {
						MsgInfo *msginfo = imap_parse_msg(file, item);
						if (msginfo != NULL) {
							msginfo->msgnum = i;
							ret = g_slist_append(ret, msginfo);
						}
						g_free(file);
					}
				}

				if (elem == NULL)
					break;

				startnum = num;
			}
			lastnum = num;
		}

		g_slist_free(sorted_list);
	}

	return ret;
}

MsgInfo *imap_get_msginfo(Folder *folder, FolderItem *item, gint uid)
{
	MsgInfo *msginfo = NULL;
	MsgInfoList *msginfolist;
	MsgNumberList numlist;

	numlist.next = NULL;
	numlist.data = GINT_TO_POINTER(uid);

	msginfolist = imap_get_msginfos(folder, item, &numlist);
	if (msginfolist != NULL) {
		msginfo = msginfolist->data;
		g_slist_free(msginfolist);
	}

	return msginfo;
}

gboolean imap_check_msgnum_validity(Folder *folder, FolderItem *_item)
{
	IMAPSession *session;
	IMAPFolderItem *item = (IMAPFolderItem *)_item;
	gint ok, exists = 0, recent = 0, unseen = 0;
	guint32 uid_next, uid_validity = 0;
	
	g_return_val_if_fail(folder != NULL, FALSE);
	g_return_val_if_fail(item != NULL, FALSE);
	g_return_val_if_fail(item->item.folder != NULL, FALSE);
	g_return_val_if_fail(FOLDER_CLASS(item->item.folder) == &imap_class, FALSE);

	session = imap_session_get(folder);
	g_return_val_if_fail(session != NULL, FALSE);

	ok = imap_status(session, IMAP_FOLDER(folder), item->item.path,
			 &exists, &recent, &uid_next, &uid_validity, &unseen);
	if (ok != IMAP_SUCCESS)
		return FALSE;

	if(item->item.mtime == uid_validity)
		return TRUE;

	debug_print("Freeing imap uid cache\n");
	item->lastuid = 0;
	g_slist_free(item->uid_list);
	item->uid_list = NULL;
		
	item->item.mtime = uid_validity;

	imap_delete_all_cached_messages((FolderItem *)item);

	return FALSE;
}

void imap_change_flags(Folder *folder, FolderItem *item, MsgInfo *msginfo, MsgPermFlags newflags)
{
	IMAPSession *session;
	IMAPFlags flags_set = 0, flags_unset = 0;
	gint ok = IMAP_SUCCESS;
	MsgNumberList numlist;
	
	g_return_if_fail(folder != NULL);
	g_return_if_fail(folder->klass == &imap_class);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder == folder);
	g_return_if_fail(msginfo != NULL);
	g_return_if_fail(msginfo->folder == item);

	session = imap_session_get(folder);
	if (!session) return;

	if ((ok = imap_select(session, IMAP_FOLDER(folder), msginfo->folder->path,
	    NULL, NULL, NULL, NULL)) != IMAP_SUCCESS)
		return;

	if (!MSG_IS_MARKED(msginfo->flags) &&  (newflags & MSG_MARKED))
		flags_set |= IMAP_FLAG_FLAGGED;
	if ( MSG_IS_MARKED(msginfo->flags) && !(newflags & MSG_MARKED))
		flags_unset |= IMAP_FLAG_FLAGGED;

	if (!MSG_IS_UNREAD(msginfo->flags) &&  (newflags & MSG_UNREAD))
		flags_unset |= IMAP_FLAG_SEEN;
	if ( MSG_IS_UNREAD(msginfo->flags) && !(newflags & MSG_UNREAD))
		flags_set |= IMAP_FLAG_SEEN;

	if (!MSG_IS_REPLIED(msginfo->flags) &&  (newflags & MSG_REPLIED))
		flags_set |= IMAP_FLAG_ANSWERED;
	if ( MSG_IS_REPLIED(msginfo->flags) && !(newflags & MSG_REPLIED))
		flags_set |= IMAP_FLAG_ANSWERED;

	numlist.next = NULL;
	numlist.data = GINT_TO_POINTER(msginfo->msgnum);
	
	if (flags_set) {
		ok = imap_set_message_flags(session, &numlist, flags_set, TRUE);
		if (ok != IMAP_SUCCESS) return;
	}

	if (flags_unset) {
		ok = imap_set_message_flags(session, &numlist, flags_unset, FALSE);
		if (ok != IMAP_SUCCESS) return;
	}

	msginfo->flags.perm_flags = newflags;

	return;
}
