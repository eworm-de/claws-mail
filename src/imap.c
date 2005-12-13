/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2005 Hiroyuki Yamamoto
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <stdio.h>
#include <string.h>
#include "imap.h"
#include "imap_gtk.h"
#include "inc.h"

#ifdef HAVE_LIBETPAN

#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#if HAVE_ICONV
#  include <iconv.h>
#endif

#if USE_OPENSSL
#  include "ssl.h"
#endif

#include "folder.h"
#include "session.h"
#include "procmsg.h"
#include "socket.h"
#include "recv.h"
#include "procheader.h"
#include "prefs_account.h"
#include "codeconv.h"
#include "md5.h"
#include "base64.h"
#include "utils.h"
#include "prefs_common.h"
#include "inputdialog.h"
#include "log.h"
#include "remotefolder.h"
#include "alertpanel.h"
#include "sylpheed.h"
#include "statusbar.h"
#include "msgcache.h"
#include "imap-thread.h"

typedef struct _IMAPFolder	IMAPFolder;
typedef struct _IMAPSession	IMAPSession;
typedef struct _IMAPNameSpace	IMAPNameSpace;
typedef struct _IMAPFolderItem	IMAPFolderItem;

#include "prefs_account.h"

#define IMAP_FOLDER(obj)	((IMAPFolder *)obj)
#define IMAP_FOLDER_ITEM(obj)	((IMAPFolderItem *)obj)
#define IMAP_SESSION(obj)	((IMAPSession *)obj)

struct _IMAPFolder
{
	RemoteFolder rfolder;

	/* list of IMAPNameSpace */
	GList *ns_personal;
	GList *ns_others;
	GList *ns_shared;
	gchar last_seen_separator;
	guint refcnt;
};

struct _IMAPSession
{
	Session session;

	gboolean authenticated;

	GSList *capability;
	gboolean uidplus;

	gchar *mbox;
	guint cmd_count;

	/* CLAWS */
	gboolean folder_content_changed;
	guint exists;
	Folder * folder;
	gboolean busy;
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

#define IMAPBUFSIZE	8192

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

#define IMAP_CMD_LIMIT	1000

struct _IMAPFolderItem
{
	FolderItem item;

	guint lastuid;
	guint uid_next;
	GSList *uid_list;
	gboolean batching;

	time_t use_cache;
	gint c_messages;
	guint32 c_uid_next;
	guint32 c_uid_validity;
	gint c_unseen;

	GHashTable *flags_set_table;
	GHashTable *flags_unset_table;
};

static void imap_folder_init		(Folder		*folder,
					 const gchar	*name,
					 const gchar	*path);

static Folder	*imap_folder_new	(const gchar	*name,
					 const gchar	*path);
static void	 imap_folder_destroy	(Folder		*folder);

static IMAPSession *imap_session_new	(Folder         *folder,
					 const PrefsAccount 	*account);
static void 	imap_session_authenticate(IMAPSession 		*session,
				      	  const PrefsAccount 	*account);
static void 	imap_session_destroy	(Session 	*session);

static gchar   *imap_fetch_msg		(Folder 	*folder, 
					 FolderItem 	*item, 
					 gint 		 uid);
static gchar   *imap_fetch_msg_full	(Folder 	*folder, 
					 FolderItem 	*item, 
					 gint 		 uid,
					 gboolean	 headers,
					 gboolean	 body);
static gint 	imap_add_msg		(Folder 	*folder,
			 		 FolderItem 	*dest,
			 		 const gchar 	*file, 
					 MsgFlags 	*flags);
static gint 	imap_add_msgs		(Folder 	*folder, 
					 FolderItem 	*dest,
			  		 GSList 	*file_list,
			  		 GRelation 	*relation);

static gint 	imap_copy_msg		(Folder 	*folder,
			  		 FolderItem 	*dest, 
					 MsgInfo 	*msginfo);
static gint 	imap_copy_msgs		(Folder 	*folder, 
					 FolderItem 	*dest, 
		    			 MsgInfoList 	*msglist, 
					 GRelation 	*relation);

static gint 	imap_remove_msg		(Folder 	*folder, 
					 FolderItem 	*item, 
					 gint 		 uid);
static gint 	imap_remove_msgs	(Folder 	*folder, 
					 FolderItem 	*dest, 
		    			 MsgInfoList 	*msglist, 
					 GRelation 	*relation);
static gint 	imap_remove_all_msg	(Folder 	*folder, 
					 FolderItem 	*item);

static gboolean imap_is_msg_changed	(Folder 	*folder,
				    	 FolderItem 	*item, 
					 MsgInfo 	*msginfo);

static gint 	imap_close		(Folder 	*folder, 
					 FolderItem 	*item);

static gint 	imap_scan_tree		(Folder 	*folder);

static gint 	imap_create_tree	(Folder 	*folder);

static FolderItem *imap_create_folder	(Folder 	*folder,
				      	 FolderItem 	*parent,
				      	 const gchar 	*name);
static gint 	imap_rename_folder	(Folder 	*folder,
			       		 FolderItem 	*item, 
					 const gchar 	*name);
static gint 	imap_remove_folder	(Folder 	*folder, 
					 FolderItem 	*item);

static FolderItem *imap_folder_item_new	(Folder		*folder);
static void imap_folder_item_destroy	(Folder		*folder,
					 FolderItem	*item);

static IMAPSession *imap_session_get	(Folder		*folder);

static gint imap_auth			(IMAPSession	*session,
					 const gchar	*user,
					 const gchar	*pass,
					 IMAPAuthType	 type);

static gint imap_scan_tree_recursive	(IMAPSession	*session,
					 FolderItem	*item);

static void imap_create_missing_folders	(Folder		*folder);
static FolderItem *imap_create_special_folder
					(Folder			*folder,
					 SpecialFolderItemType	 stype,
					 const gchar		*name);

static gint imap_do_copy_msgs		(Folder		*folder,
					 FolderItem	*dest,
					 MsgInfoList	*msglist,
					 GRelation	*relation);

static void imap_delete_all_cached_messages	(FolderItem	*item);
static void imap_set_batch		(Folder		*folder,
					 FolderItem	*item,
					 gboolean	 batch);
static gint imap_set_message_flags	(IMAPSession	*session,
					 MsgNumberList	*numlist,
					 IMAPFlags	 flags,
					 gboolean	 is_set);
static gint imap_select			(IMAPSession	*session,
					 IMAPFolder	*folder,
					 const gchar	*path,
					 gint		*exists,
					 gint		*recent,
					 gint		*unseen,
					 guint32	*uid_validity,
					 gboolean	 block);
static gint imap_status			(IMAPSession	*session,
					 IMAPFolder	*folder,
					 const gchar	*path,
					 IMAPFolderItem *item,
					 gint		*messages,
					 guint32	*uid_next,
					 guint32	*uid_validity,
					 gint		*unseen,
					 gboolean	 block);

static IMAPNameSpace *imap_find_namespace	(IMAPFolder	*folder,
						 const gchar	*path);
static gchar imap_get_path_separator		(IMAPFolder	*folder,
						 const gchar	*path);
static gchar *imap_get_real_path		(IMAPFolder	*folder,
						 const gchar	*path);
static void imap_synchronise		(FolderItem	*item);

static void imap_free_capabilities	(IMAPSession 	*session);

/* low-level IMAP4rev1 commands */
static gint imap_cmd_login	(IMAPSession	*session,
				 const gchar	*user,
				 const gchar	*pass,
				 const gchar 	*type);
static gint imap_cmd_logout	(IMAPSession	*session);
static gint imap_cmd_noop	(IMAPSession	*session);
#if USE_OPENSSL
static gint imap_cmd_starttls	(IMAPSession	*session);
#endif
static gint imap_cmd_select	(IMAPSession	*session,
				 const gchar	*folder,
				 gint		*exists,
				 gint		*recent,
				 gint		*unseen,
				 guint32	*uid_validity,
				 gboolean	 block);
static gint imap_cmd_examine	(IMAPSession	*session,
				 const gchar	*folder,
				 gint		*exists,
				 gint		*recent,
				 gint		*unseen,
				 guint32	*uid_validity,
				 gboolean	 block);
static gint imap_cmd_create	(IMAPSession	*sock,
				 const gchar	*folder);
static gint imap_cmd_rename	(IMAPSession	*sock,
				 const gchar	*oldfolder,
				 const gchar	*newfolder);
static gint imap_cmd_delete	(IMAPSession	*session,
				 const gchar	*folder);
static gint imap_cmd_fetch	(IMAPSession	*sock,
				 guint32	 uid,
				 const gchar	*filename,
				 gboolean	 headers,
				 gboolean	 body);
static gint imap_cmd_append	(IMAPSession	*session,
				 const gchar	*destfolder,
				 const gchar	*file,
				 IMAPFlags	 flags,
				 guint32	*new_uid);
static gint imap_cmd_copy       (IMAPSession *session,
				 struct mailimap_set * set,
				 const gchar *destfolder,
				 GRelation *uid_mapping);
static gint imap_cmd_store	(IMAPSession	*session,
				 struct mailimap_set * set,
				 IMAPFlags flags,
				 int do_add);
static gint imap_cmd_expunge	(IMAPSession	*session);

static void imap_path_separator_subst		(gchar		*str,
						 gchar		 separator);

static gchar *imap_utf8_to_modified_utf7	(const gchar	*from);
static gchar *imap_modified_utf7_to_utf8	(const gchar	*mutf7_str);

static gboolean imap_rename_folder_func		(GNode		*node,
						 gpointer	 data);
static gint imap_get_num_list			(Folder 	*folder,
						 FolderItem 	*item,
						 GSList	       **list,
						 gboolean	*old_uids_valid);
static GSList *imap_get_msginfos		(Folder		*folder,
						 FolderItem	*item,
						 GSList		*msgnum_list);
static MsgInfo *imap_get_msginfo 		(Folder 	*folder,
						 FolderItem 	*item,
						 gint 		 num);
static gboolean imap_scan_required		(Folder 	*folder,
						 FolderItem 	*item);
static void imap_change_flags			(Folder 	*folder,
						 FolderItem 	*item,
						 MsgInfo 	*msginfo,
						 MsgPermFlags 	 newflags);
static gint imap_get_flags			(Folder 	*folder,
						 FolderItem 	*item,
                    				 MsgInfoList 	*msglist,
						 GRelation 	*msgflags);
static gchar *imap_folder_get_path		(Folder		*folder);
static gchar *imap_item_get_path		(Folder		*folder,
						 FolderItem	*item);
static MsgInfo *imap_parse_msg(const gchar *file, FolderItem *item);


/* data types conversion libetpan <-> sylpheed */
static GSList * imap_list_from_lep(IMAPFolder * folder,
				   clist * list, const gchar * real_path, gboolean all);
static GSList * imap_get_lep_set_from_numlist(MsgNumberList *numlist);
static GSList * imap_get_lep_set_from_msglist(MsgInfoList *msglist);
static GSList * imap_uid_list_from_lep(clist * list);
static GSList * imap_uid_list_from_lep_tab(carray * list);
static MsgInfo *imap_envelope_from_lep(struct imap_fetch_env_info * info,
				       FolderItem *item);
static void imap_lep_set_free(GSList *seq_list);
static struct mailimap_flag_list * imap_flag_to_lep(IMAPFlags flags);

typedef struct _hashtable_data {
	IMAPSession *session;
	GSList *msglist;
	IMAPFolderItem *item;
} hashtable_data;

static FolderClass imap_class;

typedef struct _thread_data {
	gchar *server;
	gushort port;
	gboolean done;
	SockInfo *sock;
#ifdef USE_OPENSSL
	SSLType ssl_type;
#endif
} thread_data;

FolderClass *imap_get_class(void)
{
	if (imap_class.idstr == NULL) {
		imap_class.type = F_IMAP;
		imap_class.idstr = "imap";
		imap_class.uistr = "IMAP4";

		/* Folder functions */
		imap_class.new_folder = imap_folder_new;
		imap_class.destroy_folder = imap_folder_destroy;
		imap_class.scan_tree = imap_scan_tree;
		imap_class.create_tree = imap_create_tree;

		/* FolderItem functions */
		imap_class.item_new = imap_folder_item_new;
		imap_class.item_destroy = imap_folder_item_destroy;
		imap_class.item_get_path = imap_item_get_path;
		imap_class.create_folder = imap_create_folder;
		imap_class.rename_folder = imap_rename_folder;
		imap_class.remove_folder = imap_remove_folder;
		imap_class.close = imap_close;
		imap_class.get_num_list = imap_get_num_list;
		imap_class.scan_required = imap_scan_required;

		/* Message functions */
		imap_class.get_msginfo = imap_get_msginfo;
		imap_class.get_msginfos = imap_get_msginfos;
		imap_class.fetch_msg = imap_fetch_msg;
		imap_class.fetch_msg_full = imap_fetch_msg_full;
		imap_class.add_msg = imap_add_msg;
		imap_class.add_msgs = imap_add_msgs;
		imap_class.copy_msg = imap_copy_msg;
		imap_class.copy_msgs = imap_copy_msgs;
		imap_class.remove_msg = imap_remove_msg;
		imap_class.remove_msgs = imap_remove_msgs;
		imap_class.remove_all_msg = imap_remove_all_msg;
		imap_class.is_msg_changed = imap_is_msg_changed;
		imap_class.change_flags = imap_change_flags;
		imap_class.get_flags = imap_get_flags;
		imap_class.set_batch = imap_set_batch;
		imap_class.synchronise = imap_synchronise;
#ifdef USE_PTREAD
		pthread_mutex_init(&imap_mutex, NULL);
#endif
	}
	
	return &imap_class;
}

static Folder *imap_folder_new(const gchar *name, const gchar *path)
{
	Folder *folder;

	folder = (Folder *)g_new0(IMAPFolder, 1);
	folder->klass = &imap_class;
	imap_folder_init(folder, name, path);

	return folder;
}

static void imap_folder_destroy(Folder *folder)
{
	gchar *dir;

	while (imap_folder_get_refcnt(folder) > 0)
		gtk_main_iteration();
	
	dir = imap_folder_get_path(folder);
	if (is_dir_exist(dir))
		remove_dir_recursive(dir);
	g_free(dir);

	folder_remote_folder_destroy(REMOTE_FOLDER(folder));
	imap_done(folder);
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

void imap_get_capabilities(IMAPSession *session)
{
	struct mailimap_capability_data *capabilities = NULL;
	clistiter *cur;

	if (session->capability != NULL)
		return;

	capabilities = imap_threaded_capability(session->folder);

	if (capabilities == NULL)
		return;

	for(cur = clist_begin(capabilities->cap_list) ; cur != NULL ;
	    cur = clist_next(cur)) {
		struct mailimap_capability * cap = 
			clist_content(cur);
		if (!cap || cap->cap_data.cap_name == NULL)
			continue;
		session->capability = g_slist_append
				(session->capability,
				 g_strdup(cap->cap_data.cap_name));
		debug_print("got capa %s\n", cap->cap_data.cap_name);
	}
	mailimap_capability_data_free(capabilities);
}

gboolean imap_has_capability(IMAPSession *session, const gchar *cap) 
{
	GSList *cur;
	for (cur = session->capability; cur; cur = cur->next) {
		if (!g_ascii_strcasecmp(cur->data, cap))
			return TRUE;
	}
	return FALSE;
}

static gint imap_auth(IMAPSession *session, const gchar *user, const gchar *pass,
		      IMAPAuthType type)
{
	gint ok = IMAP_ERROR;
	static time_t last_login_err = 0;
	
	imap_get_capabilities(session);

	switch(type) {
	case IMAP_AUTH_CRAM_MD5:
		ok = imap_cmd_login(session, user, pass, "CRAM-MD5");
		break;
	case IMAP_AUTH_LOGIN:
		ok = imap_cmd_login(session, user, pass, "LOGIN");
		break;
	default:
		debug_print("capabilities:\n"
				"\t CRAM-MD5 %d\n"
				"\t LOGIN %d\n", 
			imap_has_capability(session, "CRAM-MD5"),
			imap_has_capability(session, "LOGIN"));
		if (imap_has_capability(session, "CRAM-MD5"))
			ok = imap_cmd_login(session, user, pass, "CRAM-MD5");
		if (ok == IMAP_ERROR) /* we always try LOGIN before giving up */
			ok = imap_cmd_login(session, user, pass, "LOGIN");
	}
	if (ok == IMAP_SUCCESS)
		session->authenticated = TRUE;
	else {
		gchar *ext_info = NULL;
		
		if (type == IMAP_AUTH_CRAM_MD5) {
			ext_info = _("\n\nCRAM-MD5 logins only work if libetpan has been "
				     "compiled with SASL support and the "
				     "CRAM-MD5 SASL plugin is installed.");
		} else {
			ext_info = "";
		}
		
		if (time(NULL) - last_login_err > 10) {
			if (!prefs_common.no_recv_err_panel) {
				alertpanel_error(_("Connection to %s failed: "
					"login refused.%s"),
					SESSION(session)->server, ext_info);
			} else {
				log_error(_("Connection to %s failed: "
					"login refused.%s\n"),
					SESSION(session)->server, ext_info);
			}
		}
		last_login_err = time(NULL);
	}
	return ok;
}

static IMAPSession *imap_reconnect_if_possible(Folder *folder, IMAPSession *session)
{
	RemoteFolder *rfolder = REMOTE_FOLDER(folder);
	/* Check if this is the first try to establish a
	   connection, if yes we don't try to reconnect */
	debug_print("reconnecting\n");
	if (rfolder->session == NULL) {
		log_warning(_("Connecting to %s failed"),
			    folder->account->recv_server);
		session_destroy(SESSION(session));
		session = NULL;
	} else {
		log_warning(_("IMAP4 connection to %s has been"
			    " disconnected. Reconnecting...\n"),
			    folder->account->recv_server);
		statusbar_print_all(_("IMAP4 connection to %s has been"
			    " disconnected. Reconnecting...\n"),
			    folder->account->recv_server);
		SESSION(session)->state = SESSION_DISCONNECTED;
		session_destroy(SESSION(session));
		/* Clear folders session to make imap_session_get create
		   a new session, because of rfolder->session == NULL
		   it will not try to reconnect again and so avoid an
		   endless loop */
		rfolder->session = NULL;
		session = imap_session_get(folder);
		rfolder->session = SESSION(session);
		statusbar_pop_all();
	}
	return session;
}

#define lock_session() {\
	debug_print("locking session\n"); \
	session->busy = TRUE;\
}

#define unlock_session() {\
	debug_print("unlocking session\n"); \
	session->busy = FALSE;\
}

static IMAPSession *imap_session_get(Folder *folder)
{
	RemoteFolder *rfolder = REMOTE_FOLDER(folder);
	IMAPSession *session = NULL;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(FOLDER_CLASS(folder) == &imap_class, NULL);
	g_return_val_if_fail(folder->account != NULL, NULL);
	
	if (prefs_common.work_offline && !inc_offline_should_override()) {
		return NULL;
	}

	/* Make sure we have a session */
	if (rfolder->session != NULL) {
		session = IMAP_SESSION(rfolder->session);
		/* don't do that yet... 
		if (session->busy) {
			return NULL;
		} */
	} else {
		imap_reset_uid_lists(folder);
		session = imap_session_new(folder, folder->account);
	}
	if(session == NULL)
		return NULL;

	/* Make sure session is authenticated */
	if (!IMAP_SESSION(session)->authenticated)
		imap_session_authenticate(IMAP_SESSION(session), folder->account);
	
	if (!IMAP_SESSION(session)->authenticated) {
		session_destroy(SESSION(session));
		rfolder->session = NULL;
		return NULL;
	}

	/* I think the point of this code is to avoid sending a
	 * keepalive if we've used the session recently and therefore
	 * think it's still alive.  Unfortunately, most of the code
	 * does not yet check for errors on the socket, and so if the
	 * connection drops we don't notice until the timeout expires.
	 * A better solution than sending a NOOP every time would be
	 * for every command to be prepared to retry until it is
	 * successfully sent. -- mbp */
	if (time(NULL) - SESSION(session)->last_access_time > SESSION_TIMEOUT_INTERVAL) {
		/* verify that the session is still alive */
		if (imap_cmd_noop(session) != IMAP_SUCCESS) {
			debug_print("disconnected!\n");
			session = imap_reconnect_if_possible(folder, session);
		}
	}

	rfolder->session = SESSION(session);
	
	return IMAP_SESSION(session);
}

static IMAPSession *imap_session_new(Folder * folder,
				     const PrefsAccount *account)
{
	IMAPSession *session;
	gushort port;
	int r;
	int authenticated;
	
#ifdef USE_OPENSSL
	/* FIXME: IMAP over SSL only... */ 
	SSLType ssl_type;

	port = account->set_imapport ? account->imapport
		: account->ssl_imap == SSL_TUNNEL ? IMAPS_PORT : IMAP4_PORT;
	ssl_type = account->ssl_imap;	
#else
	if (account->ssl_imap != SSL_NONE) {
		if (alertpanel_full(_("Insecure connection"),
			_("This connection is configured to be secured "
			  "using SSL, but SSL is not available in this "
			  "build of Sylpheed-Claws. \n\n"
			  "Do you want to continue connecting to this "
			  "server? The communication would not be "
			  "secure."),
			  _("Con_tinue connecting"), 
			  GTK_STOCK_CANCEL, NULL,
			  FALSE, NULL, ALERT_WARNING,
			  G_ALERTALTERNATE) != G_ALERTDEFAULT)
			return NULL;
	}
	port = account->set_imapport ? account->imapport
		: IMAP4_PORT;
#endif

	imap_init(folder);
	statusbar_print_all(_("Connecting to IMAP4 server: %s..."), folder->account->recv_server);
	if (account->set_tunnelcmd) {
		r = imap_threaded_connect_cmd(folder,
					      account->tunnelcmd,
					      account->recv_server,
					      port);
	}
	else {
#ifdef USE_OPENSSL
		if (ssl_type == SSL_TUNNEL) {
			r = imap_threaded_connect_ssl(folder,
						      account->recv_server,
						      port);
		}
		else 
#endif
		{
			r = imap_threaded_connect(folder,
						  account->recv_server,
						  port);
		}
	}
	
	statusbar_pop_all();
	if (r == MAILIMAP_NO_ERROR_AUTHENTICATED) {
		authenticated = TRUE;
	}
	else if (r == MAILIMAP_NO_ERROR_NON_AUTHENTICATED) {
		authenticated = FALSE;
	}
	else {
		if(!prefs_common.no_recv_err_panel) {
			alertpanel_error(_("Can't connect to IMAP4 server: %s:%d"),
					 account->recv_server, port);
		} else {
			log_error(_("Can't connect to IMAP4 server: %s:%d\n"),
					 account->recv_server, port);
		} 
		
		return NULL;
	}
	
	session = g_new0(IMAPSession, 1);
	session_init(SESSION(session));
	SESSION(session)->type             = SESSION_IMAP;
	SESSION(session)->server           = g_strdup(account->recv_server);
 	SESSION(session)->sock             = NULL;
	
	SESSION(session)->destroy          = imap_session_destroy;

	session->capability = NULL;
	
	session->authenticated = authenticated;
	session->mbox = NULL;
	session->cmd_count = 0;
	session->folder = folder;
	IMAP_FOLDER(session->folder)->last_seen_separator = 0;

#if USE_OPENSSL
	if (account->ssl_imap == SSL_STARTTLS) {
		gint ok;

		ok = imap_cmd_starttls(session);
		if (ok != IMAP_SUCCESS) {
			log_warning(_("Can't start TLS session.\n"));
			session_destroy(SESSION(session));
			return NULL;
		}

		imap_free_capabilities(session);
		session->authenticated = FALSE;
		session->uidplus = FALSE;
		session->cmd_count = 1;
	}
#endif
	log_message("IMAP connection is %s-authenticated\n",
		    (session->authenticated) ? "pre" : "un");
	
	return session;
}

static void imap_session_authenticate(IMAPSession *session, 
				      const PrefsAccount *account)
{
	gchar *pass;

	g_return_if_fail(account->userid != NULL);

	pass = account->passwd;
	if (!pass) {
		gchar *tmp_pass;
		tmp_pass = input_dialog_query_password(account->recv_server, account->userid);
		if (!tmp_pass)
			tmp_pass = g_strdup(""); /* allow empty password */
		Xstrdup_a(pass, tmp_pass, {g_free(tmp_pass); return;});
		g_free(tmp_pass);
	}
	statusbar_print_all(_("Connecting to IMAP4 server %s...\n"),
				account->recv_server);
	if (imap_auth(session, account->userid, pass, account->imap_auth_type) != IMAP_SUCCESS) {
		imap_threaded_disconnect(session->folder);
		imap_cmd_logout(session);
		statusbar_pop_all();
		
		return;
	}
	statusbar_pop_all();
	session->authenticated = TRUE;
}

static void imap_session_destroy(Session *session)
{
	if (session->state != SESSION_DISCONNECTED)
		imap_threaded_disconnect(IMAP_SESSION(session)->folder);
	
	imap_free_capabilities(IMAP_SESSION(session));
	g_free(IMAP_SESSION(session)->mbox);
	sock_close(session->sock);
	session->sock = NULL;
}

static gchar *imap_fetch_msg(Folder *folder, FolderItem *item, gint uid)
{
	return imap_fetch_msg_full(folder, item, uid, TRUE, TRUE);
}

static guint get_size_with_crs(MsgInfo *info) 
{
	FILE *fp = NULL;
	guint cnt = 0;
	gchar buf[4096];
	
	if (info == NULL)
		return -1;
	
	fp = procmsg_open_message(info);
	if (!fp)
		return -1;
	
	while (fgets(buf, sizeof (buf), fp) != NULL) {
		cnt += strlen(buf);
		if (!strstr(buf, "\r") && strstr(buf, "\n"))
			cnt++;
	}
	
	fclose(fp);
	return cnt;
}

static gchar *imap_fetch_msg_full(Folder *folder, FolderItem *item, gint uid,
				  gboolean headers, gboolean body)
{
	gchar *path, *filename;
	IMAPSession *session;
	gint ok;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(item != NULL, NULL);

	if (uid == 0)
		return NULL;

	path = folder_item_get_path(item);
	if (!is_dir_exist(path))
		make_dir_hier(path);
	filename = g_strconcat(path, G_DIR_SEPARATOR_S, itos(uid), NULL);
	g_free(path);

	if (is_file_exist(filename)) {
		/* see whether the local file represents the whole message
		 * or not. As the IMAP server reports size with \r chars,
		 * we have to update the local file (UNIX \n only) size */
		MsgInfo *msginfo = imap_parse_msg(filename, item);
		MsgInfo *cached = msgcache_get_msg(item->cache,uid);
		guint have_size = get_size_with_crs(msginfo);

		if (cached)
			debug_print("message %d has been already %scached (%d/%d).\n", uid,
				have_size == cached->size ? "fully ":"",
				have_size, (int)cached->size);
		
		if (cached && (cached->size == have_size || !body)) {
			procmsg_msginfo_free(cached);
			procmsg_msginfo_free(msginfo);
			file_strip_crs(filename);
			return filename;
		} else {
			procmsg_msginfo_free(cached);
			procmsg_msginfo_free(msginfo);
		}
	}

	session = imap_session_get(folder);
	
	if (!session) {
		g_free(filename);
		return NULL;
	}

	lock_session();

	debug_print("IMAP fetching messages\n");
	ok = imap_select(session, IMAP_FOLDER(folder), item->path,
			 NULL, NULL, NULL, NULL, FALSE);
	if (ok != IMAP_SUCCESS) {
		g_warning("can't select mailbox %s\n", item->path);
		g_free(filename);
		unlock_session();
		return NULL;
	}

	debug_print("getting message %d...\n", uid);
	ok = imap_cmd_fetch(session, (guint32)uid, filename, headers, body);

	if (ok != IMAP_SUCCESS) {
		g_warning("can't fetch message %d\n", uid);
		g_free(filename);
		unlock_session();
		return NULL;
	}

	unlock_session();
	file_strip_crs(filename);
	return filename;
}

static gint imap_add_msg(Folder *folder, FolderItem *dest, 
			 const gchar *file, MsgFlags *flags)
{
	gint ret;
	GSList file_list;
	MsgFileInfo fileinfo;

	g_return_val_if_fail(file != NULL, -1);

	fileinfo.msginfo = NULL;
	fileinfo.file = (gchar *)file;
	fileinfo.flags = flags;
	file_list.data = &fileinfo;
	file_list.next = NULL;

	ret = imap_add_msgs(folder, dest, &file_list, NULL);
	return ret;
}

static gint imap_add_msgs(Folder *folder, FolderItem *dest, GSList *file_list,
		   GRelation *relation)
{
	gchar *destdir;
	IMAPSession *session;
	guint32 last_uid = 0;
	GSList *cur;
	MsgFileInfo *fileinfo;
	gint ok;
	gint curnum = 0, total = 0;


	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(file_list != NULL, -1);
	
	session = imap_session_get(folder);
	if (!session) {
		return -1;
	}
	lock_session();
	destdir = imap_get_real_path(IMAP_FOLDER(folder), dest->path);

	statusbar_print_all(_("Adding messages..."));
	total = g_slist_length(file_list);
	for (cur = file_list; cur != NULL; cur = cur->next) {
		IMAPFlags iflags = 0;
		guint32 new_uid = 0;
		gchar *real_file = NULL;
		gboolean file_is_tmp = FALSE;
		fileinfo = (MsgFileInfo *)cur->data;

		statusbar_progress_all(curnum, total, 1);
		curnum++;

		if (fileinfo->flags) {
			if (MSG_IS_MARKED(*fileinfo->flags))
				iflags |= IMAP_FLAG_FLAGGED;
			if (MSG_IS_REPLIED(*fileinfo->flags))
				iflags |= IMAP_FLAG_ANSWERED;
			if (!MSG_IS_UNREAD(*fileinfo->flags))
				iflags |= IMAP_FLAG_SEEN;
		}
		
		if (fileinfo->flags) {
			if ((MSG_IS_QUEUED(*fileinfo->flags) 
			     || MSG_IS_DRAFT(*fileinfo->flags))
			&& !folder_has_parent_of_type(dest, F_QUEUE)
			&& !folder_has_parent_of_type(dest, F_DRAFT)) {
				real_file = get_tmp_file();
				file_is_tmp = TRUE;
				if (procmsg_remove_special_headers(
						fileinfo->file, 
						real_file) !=0) {
					g_free(real_file);
					g_free(destdir);
					unlock_session();
					return -1;
				}
			} 
		}
		if (real_file == NULL)
			real_file = g_strdup(fileinfo->file);
		
		if (folder_has_parent_of_type(dest, F_QUEUE) ||
		    folder_has_parent_of_type(dest, F_OUTBOX) ||
		    folder_has_parent_of_type(dest, F_DRAFT) ||
		    folder_has_parent_of_type(dest, F_TRASH))
			iflags |= IMAP_FLAG_SEEN;

		ok = imap_cmd_append(session, destdir, real_file, iflags, 
				     &new_uid);

		if (ok != IMAP_SUCCESS) {
			g_warning("can't append message %s\n", real_file);
			if (file_is_tmp)
				g_unlink(real_file);
			g_free(real_file);
			g_free(destdir);
			unlock_session();
			statusbar_progress_all(0,0,0);
			statusbar_pop_all();
			return -1;
		}

		if (relation != NULL)
			g_relation_insert(relation, fileinfo->msginfo != NULL ? 
					  (gpointer) fileinfo->msginfo : (gpointer) fileinfo,
					  GINT_TO_POINTER(dest->last_num + 1));
		if (last_uid < new_uid)
			last_uid = new_uid;
		if (file_is_tmp)
			g_unlink(real_file);
	}
	statusbar_progress_all(0,0,0);
	statusbar_pop_all();
	
	unlock_session();
	
	g_free(destdir);

	return last_uid;
}

static gint imap_do_copy_msgs(Folder *folder, FolderItem *dest, 
			      MsgInfoList *msglist, GRelation *relation)
{
	FolderItem *src;
	gchar *destdir;
	GSList *seq_list, *cur;
	MsgInfo *msginfo;
	IMAPSession *session;
	gint ok = IMAP_SUCCESS;
	GRelation *uid_mapping;
	gint last_num = 0;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(msglist != NULL, -1);
	
	session = imap_session_get(folder);
	
	if (!session) {
		return -1;
	}
	lock_session();
	msginfo = (MsgInfo *)msglist->data;

	src = msginfo->folder;
	if (src == dest) {
		g_warning("the src folder is identical to the dest.\n");
		unlock_session();
		return -1;
	}

	ok = imap_select(session, IMAP_FOLDER(folder), msginfo->folder->path,
			 NULL, NULL, NULL, NULL, FALSE);
	if (ok != IMAP_SUCCESS) {
		unlock_session();
		return ok;
	}

	destdir = imap_get_real_path(IMAP_FOLDER(folder), dest->path);
	seq_list = imap_get_lep_set_from_msglist(msglist);
	uid_mapping = g_relation_new(2);
	g_relation_index(uid_mapping, 0, g_direct_hash, g_direct_equal);
	
	statusbar_print_all(_("Copying messages..."));
	for (cur = seq_list; cur != NULL; cur = g_slist_next(cur)) {
		struct mailimap_set * seq_set;
		seq_set = cur->data;

		debug_print("Copying messages from %s to %s ...\n",
			    src->path, destdir);

		ok = imap_cmd_copy(session, seq_set, destdir, uid_mapping);
		if (ok != IMAP_SUCCESS) {
			g_relation_destroy(uid_mapping);
			imap_lep_set_free(seq_list);
			unlock_session();
			return -1;
		}
	}

	for (cur = msglist; cur != NULL; cur = g_slist_next(cur)) {
		MsgInfo *msginfo = (MsgInfo *)cur->data;
		GTuples *tuples;

		tuples = g_relation_select(uid_mapping, 
					   GINT_TO_POINTER(msginfo->msgnum),
					   0);
		if (tuples->len > 0) {
			gint num = GPOINTER_TO_INT(g_tuples_index(tuples, 0, 1));
			g_relation_insert(relation, msginfo,
					  GPOINTER_TO_INT(num));
			if (num > last_num)
				last_num = num;
		} else
			g_relation_insert(relation, msginfo,
					  GPOINTER_TO_INT(0));
		g_tuples_destroy(tuples);
	}
	statusbar_pop_all();

	g_relation_destroy(uid_mapping);
	imap_lep_set_free(seq_list);

	g_free(destdir);
	
	IMAP_FOLDER_ITEM(dest)->lastuid = 0;
	IMAP_FOLDER_ITEM(dest)->uid_next = 0;
	g_slist_free(IMAP_FOLDER_ITEM(dest)->uid_list);
	IMAP_FOLDER_ITEM(dest)->uid_list = NULL;

	unlock_session();
	if (ok == IMAP_SUCCESS)
		return last_num;
	else
		return -1;
}

static gint imap_copy_msg(Folder *folder, FolderItem *dest, MsgInfo *msginfo)
{
	GSList msglist;

	g_return_val_if_fail(msginfo != NULL, -1);

	msglist.data = msginfo;
	msglist.next = NULL;

	return imap_copy_msgs(folder, dest, &msglist, NULL);
}

static gint imap_copy_msgs(Folder *folder, FolderItem *dest, 
		    MsgInfoList *msglist, GRelation *relation)
{
	MsgInfo *msginfo;
	GSList *file_list;
	gint ret;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(msglist != NULL, -1);

	msginfo = (MsgInfo *)msglist->data;
	g_return_val_if_fail(msginfo->folder != NULL, -1);

	if (folder == msginfo->folder->folder &&
	    !folder_has_parent_of_type(msginfo->folder, F_DRAFT) &&
	    !folder_has_parent_of_type(msginfo->folder, F_QUEUE)) {
		ret = imap_do_copy_msgs(folder, dest, msglist, relation);
		return ret;
	}

	file_list = procmsg_get_message_file_list(msglist);
	g_return_val_if_fail(file_list != NULL, -1);

	ret = imap_add_msgs(folder, dest, file_list, relation);

	procmsg_message_file_list_free(file_list);

	return ret;
}


static gint imap_do_remove_msgs(Folder *folder, FolderItem *dest, 
			        MsgInfoList *msglist, GRelation *relation)
{
	gchar *destdir;
	GSList *numlist = NULL, *cur;
	MsgInfo *msginfo;
	IMAPSession *session;
	gint ok = IMAP_SUCCESS;
	GRelation *uid_mapping;
	
	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(msglist != NULL, -1);

	session = imap_session_get(folder);
	if (!session) {
		return -1;
	}
	lock_session();
	msginfo = (MsgInfo *)msglist->data;

	ok = imap_select(session, IMAP_FOLDER(folder), msginfo->folder->path,
			 NULL, NULL, NULL, NULL, FALSE);
	if (ok != IMAP_SUCCESS) {
		unlock_session();
		return ok;
	}

	destdir = imap_get_real_path(IMAP_FOLDER(folder), dest->path);
	for (cur = msglist; cur; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;
		if (!MSG_IS_DELETED(msginfo->flags))
			numlist = g_slist_append(numlist, GINT_TO_POINTER(msginfo->msgnum));
	}

	uid_mapping = g_relation_new(2);
	g_relation_index(uid_mapping, 0, g_direct_hash, g_direct_equal);

	ok = imap_set_message_flags
		(IMAP_SESSION(REMOTE_FOLDER(folder)->session),
		numlist, IMAP_FLAG_DELETED, TRUE);
	if (ok != IMAP_SUCCESS) {
		log_warning(_("can't set deleted flags\n"));
		unlock_session();
		return ok;
	}
	ok = imap_cmd_expunge(session);
	if (ok != IMAP_SUCCESS) {
		log_warning(_("can't expunge\n"));
		unlock_session();
		return ok;
	}
	
	g_relation_destroy(uid_mapping);
	g_slist_free(numlist);

	g_free(destdir);
	unlock_session();
	if (ok == IMAP_SUCCESS)
		return 0;
	else
		return -1;
}

static gint imap_remove_msgs(Folder *folder, FolderItem *dest, 
		    MsgInfoList *msglist, GRelation *relation)
{
	MsgInfo *msginfo;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(dest != NULL, -1);
	if (msglist == NULL)
		return 0;

	msginfo = (MsgInfo *)msglist->data;
	g_return_val_if_fail(msginfo->folder != NULL, -1);

	return imap_do_remove_msgs(folder, dest, msglist, relation);
}

static gint imap_remove_all_msg(Folder *folder, FolderItem *item)
{
	GSList *list = folder_item_get_msg_list(item);
	gint res = imap_remove_msgs(folder, item, list, NULL);
	procmsg_msg_list_free(list);
	return res;
}

static gboolean imap_is_msg_changed(Folder *folder, FolderItem *item,
				    MsgInfo *msginfo)
{
	/* TODO: properly implement this method */
	return FALSE;
}

static gint imap_close(Folder *folder, FolderItem *item)
{
	return 0;
}

static gint imap_scan_tree(Folder *folder)
{
	FolderItem *item = NULL;
	IMAPSession *session;
	gchar *root_folder = NULL;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(folder->account != NULL, -1);

	session = imap_session_get(folder);
	if (!session) {
		if (!folder->node) {
			folder_tree_destroy(folder);
			item = folder_item_new(folder, folder->name, NULL);
			item->folder = folder;
			folder->node = item->node = g_node_new(item);
		}
		return -1;
	}

	lock_session();
	if (folder->account->imap_dir && *folder->account->imap_dir) {
		gchar *real_path;
		int r;
		clist * lep_list;

		Xstrdup_a(root_folder, folder->account->imap_dir, {unlock_session();return -1;});
		extract_quote(root_folder, '"');
		subst_char(root_folder,
			   imap_get_path_separator(IMAP_FOLDER(folder),
						   root_folder),
			   '/');
		strtailchomp(root_folder, '/');
		real_path = imap_get_real_path
			(IMAP_FOLDER(folder), root_folder);
		debug_print("IMAP root directory: %s\n", real_path);

		/* check if root directory exist */

		r = imap_threaded_list(session->folder, "", real_path,
				       &lep_list);
		if ((r != MAILIMAP_NO_ERROR) || (clist_count(lep_list) == 0)) {
			if (!folder->node) {
				item = folder_item_new(folder, folder->name, NULL);
				item->folder = folder;
				folder->node = item->node = g_node_new(item);
			}
			unlock_session();
			return -1;
		}
		mailimap_list_result_free(lep_list);
		
		g_free(real_path);
	}

	if (folder->node)
		item = FOLDER_ITEM(folder->node->data);
	if (!item || ((item->path || root_folder) &&
		      strcmp2(item->path, root_folder) != 0)) {
		folder_tree_destroy(folder);
		item = folder_item_new(folder, folder->name, root_folder);
		item->folder = folder;
		folder->node = item->node = g_node_new(item);
	}

	imap_scan_tree_recursive(session, FOLDER_ITEM(folder->node->data));
	imap_create_missing_folders(folder);
	unlock_session();

	return 0;
}

static gint imap_scan_tree_recursive(IMAPSession *session, FolderItem *item)
{
	Folder *folder;
	IMAPFolder *imapfolder;
	FolderItem *new_item;
	GSList *item_list, *cur;
	GNode *node;
	gchar *real_path;
	gchar *wildcard_path;
	gchar separator;
	gchar wildcard[3];
	clist * lep_list;
	int r;
	
	g_return_val_if_fail(item != NULL, -1);
	g_return_val_if_fail(item->folder != NULL, -1);
	g_return_val_if_fail(item->no_sub == FALSE, -1);

	folder = item->folder;
	imapfolder = IMAP_FOLDER(folder);

	separator = imap_get_path_separator(imapfolder, item->path);

	if (folder->ui_func)
		folder->ui_func(folder, item, folder->ui_func_data);

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
	lep_list = NULL;
	r = imap_threaded_list(folder, "", wildcard_path, &lep_list);
	if (r != MAILIMAP_NO_ERROR) {
		item_list = NULL;
	}
	else {
		item_list = imap_list_from_lep(imapfolder,
					       lep_list, real_path, FALSE);
		mailimap_list_result_free(lep_list);
	}
	
	g_free(real_path);

	node = item->node->children;
	while (node != NULL) {
		FolderItem *old_item = FOLDER_ITEM(node->data);
		GNode *next = node->next;

		new_item = NULL;
		for (cur = item_list; cur != NULL; cur = cur->next) {
			FolderItem *cur_item = FOLDER_ITEM(cur->data);
			if (!strcmp2(old_item->path, cur_item->path)) {
				new_item = cur_item;
				break;
			}
		}
		if (!new_item) {
			debug_print("folder '%s' not found. removing...\n",
				    old_item->path);
			folder_item_remove(old_item);
		} else {
			old_item->no_sub = new_item->no_sub;
			old_item->no_select = new_item->no_select;
			if (old_item->no_sub == TRUE && node->children) {
				debug_print("folder '%s' doesn't have "
					    "subfolders. removing...\n",
					    old_item->path);
				folder_item_remove_children(old_item);
			}
		}

		node = next;
	}

	for (cur = item_list; cur != NULL; cur = cur->next) {
		FolderItem *cur_item = FOLDER_ITEM(cur->data);
		new_item = NULL;

		for (node = item->node->children; node != NULL;
		     node = node->next) {
			if (!strcmp2(FOLDER_ITEM(node->data)->path,
				     cur_item->path)) {
				new_item = FOLDER_ITEM(node->data);
				folder_item_destroy(cur_item);
				cur_item = NULL;
				break;
			}
		}
		if (!new_item) {
			new_item = cur_item;
			debug_print("new folder '%s' found.\n", new_item->path);
			folder_item_append(item, new_item);
		}

		if (!strcmp(new_item->path, "INBOX")) {
			new_item->stype = F_INBOX;
			folder->inbox = new_item;
		} else if (!folder_item_parent(item) || item->stype == F_INBOX) {
			gchar *base;

			base = g_path_get_basename(new_item->path);

			if (!folder->outbox && !g_ascii_strcasecmp(base, "Sent")) {
				new_item->stype = F_OUTBOX;
				folder->outbox = new_item;
			} else if (!folder->draft && !g_ascii_strcasecmp(base, "Drafts")) {
				new_item->stype = F_DRAFT;
				folder->draft = new_item;
			} else if (!folder->queue && !g_ascii_strcasecmp(base, "Queue")) {
				new_item->stype = F_QUEUE;
				folder->queue = new_item;
			} else if (!folder->trash && !g_ascii_strcasecmp(base, "Trash")) {
				new_item->stype = F_TRASH;
				folder->trash = new_item;
			}
			g_free(base);
		}

		if (new_item->no_sub == FALSE)
			imap_scan_tree_recursive(session, new_item);
	}

	g_slist_free(item_list);

	return IMAP_SUCCESS;
}

static gint imap_create_tree(Folder *folder)
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
	if (!folder->trash)
		folder->trash = imap_create_special_folder
			(folder, F_TRASH, "Trash");
	if (!folder->queue)
		folder->queue = imap_create_special_folder
			(folder, F_QUEUE, "Queue");
	if (!folder->outbox)
		folder->outbox = imap_create_special_folder
			(folder, F_OUTBOX, "Sent");
	if (!folder->draft)
		folder->draft = imap_create_special_folder
			(folder, F_DRAFT, "Drafts");
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

static gchar *imap_folder_get_path(Folder *folder)
{
	gchar *folder_path;

	g_return_val_if_fail(folder != NULL, NULL);
        g_return_val_if_fail(folder->account != NULL, NULL);

        folder_path = g_strconcat(get_imap_cache_dir(),
                                  G_DIR_SEPARATOR_S,
                                  folder->account->recv_server,
                                  G_DIR_SEPARATOR_S,
                                  folder->account->userid,
                                  NULL);

	return folder_path;
}

static gchar *imap_item_get_path(Folder *folder, FolderItem *item)
{
	gchar *folder_path, *path;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(item != NULL, NULL);
	folder_path = imap_folder_get_path(folder);

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

static FolderItem *imap_create_folder(Folder *folder, FolderItem *parent,
			       const gchar *name)
{
	gchar *dirpath, *imap_path;
	IMAPSession *session;
	FolderItem *new_item;
	gchar separator;
	gchar *new_name;
	const gchar *p;
	gint ok;
	gboolean no_select = FALSE, no_sub = FALSE;
	
	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(folder->account != NULL, NULL);
	g_return_val_if_fail(parent != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);

	session = imap_session_get(folder);
	if (!session) {
		return NULL;
	}

	lock_session();
	if (!folder_item_parent(parent) && strcmp(name, "INBOX") == 0) {
		dirpath = g_strdup(name);
	}else if (parent->path)
		dirpath = g_strconcat(parent->path, "/", name, NULL);
	else if ((p = strchr(name, '/')) != NULL && *(p + 1) != '\0')
		dirpath = g_strdup(name);
	else if (folder->account->imap_dir && *folder->account->imap_dir) {
		gchar *imap_dir;

		Xstrdup_a(imap_dir, folder->account->imap_dir, {unlock_session();return NULL;});
		strtailchomp(imap_dir, '/');
		dirpath = g_strconcat(imap_dir, "/", name, NULL);
	} else
		dirpath = g_strdup(name);
		
	

	/* keep trailing directory separator to create a folder that contains
	   sub folder */
	imap_path = imap_utf8_to_modified_utf7(dirpath);

	strtailchomp(dirpath, '/');
	Xstrdup_a(new_name, name, {
		g_free(dirpath); 
		unlock_session();		
		return NULL;});

	separator = imap_get_path_separator(IMAP_FOLDER(folder), imap_path);
	imap_path_separator_subst(imap_path, separator);
	/* remove trailing / for display */
	strtailchomp(new_name, '/');

	if (strcmp(dirpath, "INBOX") != 0) {
		GPtrArray *argbuf;
		gboolean exist = FALSE;
		int r;
		clist * lep_list;
		
		argbuf = g_ptr_array_new();
		r = imap_threaded_list(folder, "", imap_path, &lep_list);
		if (r != MAILIMAP_NO_ERROR) {
			log_warning(_("can't create mailbox: LIST failed\n"));
			g_free(imap_path);
			g_free(dirpath);
			ptr_array_free_strings(argbuf);
			g_ptr_array_free(argbuf, TRUE);
			unlock_session();
			return NULL;
		}
		
		if (clist_count(lep_list) > 0)
			exist = TRUE;
		mailimap_list_result_free(lep_list);
		lep_list = NULL;
		if (!exist) {
			ok = imap_cmd_create(session, imap_path);
			if (ok != IMAP_SUCCESS) {
				log_warning(_("can't create mailbox\n"));
				g_free(imap_path);
				g_free(dirpath);
				unlock_session();
				return NULL;
			}
			r = imap_threaded_list(folder, "", imap_path, &lep_list);
			if (r == MAILIMAP_NO_ERROR) {
				GSList *item_list = imap_list_from_lep(IMAP_FOLDER(folder),
					       lep_list, dirpath, TRUE);
				if (item_list) {
					FolderItem *cur_item = FOLDER_ITEM(item_list->data);
					no_select = cur_item->no_select;
					no_sub = cur_item->no_sub;
					g_slist_free(item_list);
				} 
				mailimap_list_result_free(lep_list);
			}

		}
	} else {
		clist *lep_list;
		int r;
		/* just get flags */
		r = imap_threaded_list(folder, "", "INBOX", &lep_list);
		if (r == MAILIMAP_NO_ERROR) {
			GSList *item_list = imap_list_from_lep(IMAP_FOLDER(folder),
				       lep_list, dirpath, TRUE);
			if (item_list) {
				FolderItem *cur_item = FOLDER_ITEM(item_list->data);
				no_select = cur_item->no_select;
				no_sub = cur_item->no_sub;
				g_slist_free(item_list);
			} 
			mailimap_list_result_free(lep_list);
		}
	}

	new_item = folder_item_new(folder, new_name, dirpath);
	new_item->no_select = no_select;
	new_item->no_sub = no_sub;
	folder_item_append(parent, new_item);
	g_free(imap_path);
	g_free(dirpath);

	dirpath = folder_item_get_path(new_item);
	if (!is_dir_exist(dirpath))
		make_dir_hier(dirpath);
	g_free(dirpath);
	unlock_session();
	return new_item;
}

static gint imap_rename_folder(Folder *folder, FolderItem *item,
			       const gchar *name)
{
	gchar *dirpath;
	gchar *newpath;
	gchar *real_oldpath;
	gchar *real_newpath;
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
	if (!session) {
		return -1;
	}
	lock_session();

	if (strchr(name, imap_get_path_separator(IMAP_FOLDER(folder), item->path)) != NULL) {
		g_warning(_("New folder name must not contain the namespace "
			    "path separator"));
		unlock_session();
		return -1;
	}

	real_oldpath = imap_get_real_path(IMAP_FOLDER(folder), item->path);

	g_free(session->mbox);
	session->mbox = NULL;
	ok = imap_cmd_examine(session, "INBOX",
			      &exists, &recent, &unseen, &uid_validity, FALSE);
	if (ok != IMAP_SUCCESS) {
		g_free(real_oldpath);
		unlock_session();
		return -1;
	}

	separator = imap_get_path_separator(IMAP_FOLDER(folder), item->path);
	if (strchr(item->path, G_DIR_SEPARATOR)) {
		dirpath = g_path_get_dirname(item->path);
		newpath = g_strconcat(dirpath, G_DIR_SEPARATOR_S, name, NULL);
		g_free(dirpath);
	} else
		newpath = g_strdup(name);

	real_newpath = imap_utf8_to_modified_utf7(newpath);
	imap_path_separator_subst(real_newpath, separator);

	ok = imap_cmd_rename(session, real_oldpath, real_newpath);
	if (ok != IMAP_SUCCESS) {
		log_warning(_("can't rename mailbox: %s to %s\n"),
			    real_oldpath, real_newpath);
		g_free(real_oldpath);
		g_free(newpath);
		g_free(real_newpath);
		unlock_session();
		return -1;
	}

	g_free(item->name);
	item->name = g_strdup(name);

	old_cache_dir = folder_item_get_path(item);

	paths[0] = g_strdup(item->path);
	paths[1] = newpath;
	g_node_traverse(item->node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
			imap_rename_folder_func, paths);

	if (is_dir_exist(old_cache_dir)) {
		new_cache_dir = folder_item_get_path(item);
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
	unlock_session();
	return 0;
}

static gint imap_remove_folder_real(Folder *folder, FolderItem *item)
{
	gint ok;
	IMAPSession *session;
	gchar *path;
	gchar *cache_dir;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(item != NULL, -1);
	g_return_val_if_fail(item->path != NULL, -1);

	session = imap_session_get(folder);
	if (!session) {
		return -1;
	}
	lock_session();
	path = imap_get_real_path(IMAP_FOLDER(folder), item->path);

	ok = imap_cmd_delete(session, path);
	if (ok != IMAP_SUCCESS) {
		gchar *tmp = g_strdup_printf("%s%c", path, 
				imap_get_path_separator(IMAP_FOLDER(folder), path));
		g_free(path);
		path = tmp;
		ok = imap_cmd_delete(session, path);
	}

	if (ok != IMAP_SUCCESS) {
		log_warning(_("can't delete mailbox\n"));
		g_free(path);
		unlock_session();
		return -1;
	}

	g_free(path);
	cache_dir = folder_item_get_path(item);
	if (is_dir_exist(cache_dir) && remove_dir_recursive(cache_dir) < 0)
		g_warning("can't remove directory '%s'\n", cache_dir);
	g_free(cache_dir);
	folder_item_remove(item);
	unlock_session();
	return 0;
}

static gint imap_remove_folder(Folder *folder, FolderItem *item)
{
	GNode *node, *next;

	g_return_val_if_fail(item != NULL, -1);
	g_return_val_if_fail(item->folder != NULL, -1);
	g_return_val_if_fail(item->node != NULL, -1);

	node = item->node->children;
	while (node != NULL) {
		next = node->next;
		if (imap_remove_folder(folder, FOLDER_ITEM(node->data)) < 0)
			return -1;
		node = next;
	}
	debug_print("IMAP removing %s\n", item->path);

	if (imap_remove_all_msg(folder, item) < 0)
		return -1;
	return imap_remove_folder_real(folder, item);
}

typedef struct _uncached_data {
	IMAPSession *session;
	FolderItem *item;
	MsgNumberList *numlist;
	guint cur;
	guint total;
	gboolean done;
} uncached_data;

static void *imap_get_uncached_messages_thread(void *data)
{
	uncached_data *stuff = (uncached_data *)data;
	IMAPSession *session = stuff->session;
	FolderItem *item = stuff->item;
	MsgNumberList *numlist = stuff->numlist;
	
	GSList *newlist = NULL;
	GSList *llast = NULL;
	GSList *seq_list, *cur;

	debug_print("uncached_messages\n");
	
	if (session == NULL || item == NULL || item->folder == NULL
	    || FOLDER_CLASS(item->folder) != &imap_class) {
		stuff->done = TRUE;
		return NULL;
	}
	
	seq_list = imap_get_lep_set_from_numlist(numlist);
	debug_print("get msgs info\n");
	for (cur = seq_list; cur != NULL; cur = g_slist_next(cur)) {
		struct mailimap_set * imapset;
		unsigned int i;
		int r;
		carray * env_list;
		int count;
		
		imapset = cur->data;
		
		r = imap_threaded_fetch_env(session->folder,
					    imapset, &env_list);
		if (r != MAILIMAP_NO_ERROR)
			continue;
		
		count = 0;
		for(i = 0 ; i < carray_count(env_list) ; i ++) {
			struct imap_fetch_env_info * info;
			MsgInfo * msginfo;
			
			info = carray_get(env_list, i);
			msginfo = imap_envelope_from_lep(info, item);
			if (msginfo == NULL)
				continue;
			msginfo->folder = item;
			if (!newlist)
				llast = newlist = g_slist_append(newlist, msginfo);
			else {
				llast = g_slist_append(llast, msginfo);
				llast = llast->next;
			}
			count ++;
		}
		
		imap_fetch_env_free(env_list);
	}
	
	for (cur = seq_list; cur != NULL; cur = g_slist_next(cur)) {
		struct mailimap_set * imapset;
		
		imapset = cur->data;
		mailimap_set_free(imapset);
	}
	
	session_set_access_time(SESSION(session));
	stuff->done = TRUE;
	return newlist;
}

#define MAX_MSG_NUM 50

static GSList *imap_get_uncached_messages(IMAPSession *session,
					FolderItem *item,
					MsgNumberList *numlist)
{
	GSList *result = NULL;
	GSList * cur;
	uncached_data *data = g_new0(uncached_data, 1);
	int finished;
	
	finished = 0;
	cur = numlist;
	data->total = g_slist_length(numlist);
	debug_print("messages list : %i\n", data->total);

	while (cur != NULL) {
		GSList * partial_result;
		int count;
		GSList * newlist;
		GSList * llast;
		
		llast = NULL;
		count = 0;
		newlist = NULL;
		while (count < MAX_MSG_NUM) {
			void * p;
			
			p = cur->data;
			
			if (newlist == NULL)
				llast = newlist = g_slist_append(newlist, p);
			else {
				llast = g_slist_append(llast, p);
				llast = llast->next;
			}
			count ++;
			
			cur = cur->next;
			if (cur == NULL)
				break;
		}
		
		data->done = FALSE;
		data->session = session;
		data->item = item;
		data->numlist = newlist;
		data->cur += count;
		
		if (prefs_common.work_offline && !inc_offline_should_override()) {
			g_free(data);
			return NULL;
		}
		
		partial_result =
			(GSList *)imap_get_uncached_messages_thread(data);
		
		statusbar_progress_all(data->cur,data->total, 1);
		
		g_slist_free(newlist);
		
		result = g_slist_concat(result, partial_result);
	}
	g_free(data);
	
	statusbar_progress_all(0,0,0);
	statusbar_pop_all();
	
	return result;
}

static void imap_delete_all_cached_messages(FolderItem *item)
{
	gchar *dir;

	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);
	g_return_if_fail(FOLDER_CLASS(item->folder) == &imap_class);

	debug_print("Deleting all cached messages...\n");

	dir = folder_item_get_path(item);
	if (is_dir_exist(dir))
		remove_all_numbered_files(dir);
	g_free(dir);

	debug_print("done.\n");
}

static IMAPNameSpace *imap_find_namespace_from_list(GList *ns_list,
						    const gchar *path)
{
	IMAPNameSpace *namespace = NULL;
	gchar *tmp_path, *name;

	if (!path) path = "";

	for (; ns_list != NULL; ns_list = ns_list->next) {
		IMAPNameSpace *tmp_ns = ns_list->data;

		Xstrcat_a(tmp_path, path, "/", return namespace);
		Xstrdup_a(name, tmp_ns->name, return namespace);
		if (tmp_ns->separator && tmp_ns->separator != '/') {
			subst_char(tmp_path, tmp_ns->separator, '/');
			subst_char(name, tmp_ns->separator, '/');
		}
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

	if (folder->last_seen_separator == 0) {
		clist * lep_list;
		int r = imap_threaded_list((Folder *)folder, "", "", &lep_list);
		if (r != MAILIMAP_NO_ERROR) {
			log_warning(_("LIST failed\n"));
			return '/';
		}
		
		if (clist_count(lep_list) > 0) {
			clistiter * iter = clist_begin(lep_list); 
			struct mailimap_mailbox_list * mb;
			mb = clist_content(iter);
		
			folder->last_seen_separator = mb->mb_delimiter;
			debug_print("got separator: %c\n", folder->last_seen_separator);
		}
		mailimap_list_result_free(lep_list);
	}

	if (folder->last_seen_separator != 0) {
		debug_print("using separator: %c\n", folder->last_seen_separator);
		return folder->last_seen_separator;
	}

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

	real_path = imap_utf8_to_modified_utf7(path);
	separator = imap_get_path_separator(folder, path);
	imap_path_separator_subst(real_path, separator);

	return real_path;
}

static gint imap_set_message_flags(IMAPSession *session,
				   MsgNumberList *numlist,
				   IMAPFlags flags,
				   gboolean is_set)
{
	gint ok = 0;
	GSList *seq_list;
	GSList * cur;

	seq_list = imap_get_lep_set_from_numlist(numlist);
	
	for(cur = seq_list ; cur != NULL ; cur = g_slist_next(cur)) {
		struct mailimap_set * imapset;
		
		imapset = cur->data;
		
		ok = imap_cmd_store(session, imapset,
				    flags, is_set);
	}
	
	imap_lep_set_free(seq_list);
	
	return IMAP_SUCCESS;
}

typedef struct _select_data {
	IMAPSession *session;
	gchar *real_path;
	gint *exists;
	gint *recent;
	gint *unseen;
	guint32 *uid_validity;
	gboolean done;
} select_data;

static gint imap_select(IMAPSession *session, IMAPFolder *folder,
			const gchar *path,
			gint *exists, gint *recent, gint *unseen,
			guint32 *uid_validity, gboolean block)
{
	gchar *real_path;
	gint ok;
	gint exists_, recent_, unseen_;
	guint32 uid_validity_;
	
	if (!exists && !recent && !unseen && !uid_validity) {
		if (session->mbox && strcmp(session->mbox, path) == 0)
			return IMAP_SUCCESS;
	}
	if (!exists)
		exists = &exists_;
	if (!recent)
		recent = &recent_;
	if (!unseen)
		unseen = &unseen_;
	if (!uid_validity)
		uid_validity = &uid_validity_;

	g_free(session->mbox);
	session->mbox = NULL;

	real_path = imap_get_real_path(folder, path);

	ok = imap_cmd_select(session, real_path,
			     exists, recent, unseen, uid_validity, block);
	if (ok != IMAP_SUCCESS)
		log_warning(_("can't select folder: %s\n"), real_path);
	else {
		session->mbox = g_strdup(path);
		session->folder_content_changed = FALSE;
	}
	g_free(real_path);

	return ok;
}

static gint imap_status(IMAPSession *session, IMAPFolder *folder,
			const gchar *path, IMAPFolderItem *item,
			gint *messages,
			guint32 *uid_next, guint32 *uid_validity,
			gint *unseen, gboolean block)
{
	int r;
	clistiter * iter;
	struct mailimap_mailbox_data_status * data_status;
	int got_values;
	gchar *real_path;
	guint mask = 0;
	
	real_path = imap_get_real_path(folder, path);

#if 0
	if (time(NULL) - item->last_update >= 5 && item->last_update != 1) {
		/* do the full stuff */
		item->last_update = 1; /* force update */
		debug_print("updating everything\n");
		r = imap_status(session, folder, path, item,
		&item->c_messages, &item->c_uid_next,
		&item->c_uid_validity, &item->c_unseen, block);
		if (r != MAILIMAP_NO_ERROR) {
			debug_print("status err %d\n", r);
			return IMAP_ERROR;
		}
		item->last_update = time(NULL);
		if (messages) 
			*messages = item->c_messages;
		if (uid_next)
			*uid_next = item->c_uid_next;
		if (uid_validity)
			*uid_validity = item->c_uid_validity;
		if (unseen)
			*unseen = item->c_unseen;
		return 0;
	} else if (time(NULL) - item->last_update < 5) {
		/* return cached stuff */
		debug_print("using cache\n");
		if (messages) 
			*messages = item->c_messages;
		if (uid_next)
			*uid_next = item->c_uid_next;
		if (uid_validity)
			*uid_validity = item->c_uid_validity;
		if (unseen)
			*unseen = item->c_unseen;
		return 0;
	}
#endif

	/* if we get there, we're updating cache */

	if (messages) {
		mask |= 1 << 0;
	}
	if (uid_next) {
		mask |= 1 << 2;
	}
	if (uid_validity) {
		mask |= 1 << 3;
	}
	if (unseen) {
		mask |= 1 << 4;
	}
	r = imap_threaded_status(FOLDER(folder), real_path, 
		&data_status, mask);

	g_free(real_path);
	if (r != MAILIMAP_NO_ERROR) {
		debug_print("status err %d\n", r);
		return IMAP_ERROR;
	}
	
	if (data_status->st_info_list == NULL) {
		mailimap_mailbox_data_status_free(data_status);
		debug_print("status->st_info_list == NULL\n");
		return IMAP_ERROR;
	}
	
	got_values = 0;
	for(iter = clist_begin(data_status->st_info_list) ; iter != NULL ;
	    iter = clist_next(iter)) {
		struct mailimap_status_info * info;		
		
		info = clist_content(iter);
		switch (info->st_att) {
		case MAILIMAP_STATUS_ATT_MESSAGES:
			* messages = info->st_value;
			got_values |= 1 << 0;
			break;
			
		case MAILIMAP_STATUS_ATT_UIDNEXT:
			* uid_next = info->st_value;
			got_values |= 1 << 2;
			break;
			
		case MAILIMAP_STATUS_ATT_UIDVALIDITY:
			* uid_validity = info->st_value;
			got_values |= 1 << 3;
			break;
			
		case MAILIMAP_STATUS_ATT_UNSEEN:
			* unseen = info->st_value;
			got_values |= 1 << 4;
			break;
		}
	}
	mailimap_mailbox_data_status_free(data_status);
	
	if (got_values != mask) {
		debug_print("status: incomplete values received (%d)\n", got_values);
		return IMAP_ERROR;
	}
	return IMAP_SUCCESS;
}

static void imap_free_capabilities(IMAPSession *session)
{
	slist_free_strings(session->capability);
	g_slist_free(session->capability);
	session->capability = NULL;
}

/* low-level IMAP4rev1 commands */

#if 0
static gint imap_cmd_authenticate(IMAPSession *session, const gchar *user,
				  const gchar *pass, IMAPAuthType type)
{
	gchar *auth_type;
	gint ok;
	gchar *buf = NULL;
	gchar *challenge;
	gint challenge_len;
	gchar hexdigest[33];
	gchar *response;
	gchar *response64;

	auth_type = "CRAM-MD5";

	imap_gen_send(session, "AUTHENTICATE %s", auth_type);
	ok = imap_gen_recv(session, &buf);
	if (ok != IMAP_SUCCESS || buf[0] != '+' || buf[1] != ' ') {
		g_free(buf);
		return IMAP_ERROR;
	}

	challenge = g_malloc(strlen(buf + 2) + 1);
	challenge_len = base64_decode(challenge, buf + 2, -1);
	challenge[challenge_len] = '\0';
	g_free(buf);

	md5_hex_hmac(hexdigest, challenge, challenge_len, pass, strlen(pass));
	g_free(challenge);

	response = g_strdup_printf("%s %s", user, hexdigest);
	response64 = g_malloc((strlen(response) + 3) * 2 + 1);
	base64_encode(response64, response, strlen(response));
	g_free(response);

	sock_puts(SESSION(session)->sock, response64);
	ok = imap_cmd_ok(session, NULL);
	if (ok != IMAP_SUCCESS)
		log_warning(_("IMAP4 authentication failed.\n"));

	return ok;
}
#endif

static gint imap_cmd_login(IMAPSession *session,
			   const gchar *user, const gchar *pass,
			   const gchar *type)
{
	int r;
	gint ok;

	log_print("IMAP4> Logging %s to %s using %s\n", 
			user,
			SESSION(session)->server,
			type);
	r = imap_threaded_login(session->folder, user, pass, type);
	if (r != MAILIMAP_NO_ERROR) {
		log_error("IMAP4< Error logging in to %s\n",
				SESSION(session)->server);
		ok = IMAP_ERROR;
	} else {
		ok = IMAP_SUCCESS;
	}
	return ok;
}

static gint imap_cmd_logout(IMAPSession *session)
{
	imap_threaded_disconnect(session->folder);

	return IMAP_SUCCESS;
}

static gint imap_cmd_noop(IMAPSession *session)
{
	int r;
	unsigned int exists;
	
	r = imap_threaded_noop(session->folder, &exists);
	if (r != MAILIMAP_NO_ERROR) {
		debug_print("noop err %d\n", r);
		return IMAP_ERROR;
	}
	session->exists = exists;
	session_set_access_time(SESSION(session));

	return IMAP_SUCCESS;
}

#if USE_OPENSSL
static gint imap_cmd_starttls(IMAPSession *session)
{
	int r;
	
	r = imap_threaded_starttls(session->folder);
	if (r != MAILIMAP_NO_ERROR) {
		debug_print("starttls err %d\n", r);
		return IMAP_ERROR;
	}
	return IMAP_SUCCESS;
}
#endif

static gint imap_cmd_select(IMAPSession *session, const gchar *folder,
			    gint *exists, gint *recent, gint *unseen,
			    guint32 *uid_validity, gboolean block)
{
	int r;

	r = imap_threaded_select(session->folder, folder,
				 exists, recent, unseen, uid_validity);
	if (r != MAILIMAP_NO_ERROR) {
		debug_print("select err %d\n", r);
		return IMAP_ERROR;
	}
	return IMAP_SUCCESS;
}

static gint imap_cmd_examine(IMAPSession *session, const gchar *folder,
			     gint *exists, gint *recent, gint *unseen,
			     guint32 *uid_validity, gboolean block)
{
	int r;

	r = imap_threaded_examine(session->folder, folder,
				  exists, recent, unseen, uid_validity);
	if (r != MAILIMAP_NO_ERROR) {
		debug_print("examine err %d\n", r);
		
		return IMAP_ERROR;
	}
	return IMAP_SUCCESS;
}

static gint imap_cmd_create(IMAPSession *session, const gchar *folder)
{
	int r;

	r = imap_threaded_create(session->folder, folder);
	if (r != MAILIMAP_NO_ERROR) {
		
		return IMAP_ERROR;
	}

	return IMAP_SUCCESS;
}

static gint imap_cmd_rename(IMAPSession *session, const gchar *old_folder,
			    const gchar *new_folder)
{
	int r;

	r = imap_threaded_rename(session->folder, old_folder,
				 new_folder);
	if (r != MAILIMAP_NO_ERROR) {
		
		return IMAP_ERROR;
	}

	return IMAP_SUCCESS;
}

static gint imap_cmd_delete(IMAPSession *session, const gchar *folder)
{
	int r;
	

	r = imap_threaded_delete(session->folder, folder);
	if (r != MAILIMAP_NO_ERROR) {
		
		return IMAP_ERROR;
	}

	return IMAP_SUCCESS;
}

typedef struct _fetch_data {
	IMAPSession *session;
	guint32 uid;
	const gchar *filename;
	gboolean headers;
	gboolean body;
	gboolean done;
} fetch_data;

static void *imap_cmd_fetch_thread(void *data)
{
	fetch_data *stuff = (fetch_data *)data;
	IMAPSession *session = stuff->session;
	guint32 uid = stuff->uid;
	const gchar *filename = stuff->filename;
	int r;
	
	if (stuff->body) {
		r = imap_threaded_fetch_content(session->folder,
					       uid, 1, filename);
	}
	else {
		r = imap_threaded_fetch_content(session->folder,
						uid, 0, filename);
	}
	if (r != MAILIMAP_NO_ERROR) {
		debug_print("fetch err %d\n", r);
		return GINT_TO_POINTER(IMAP_ERROR);
	}
	return GINT_TO_POINTER(IMAP_SUCCESS);
}

static gint imap_cmd_fetch(IMAPSession *session, guint32 uid,
				const gchar *filename, gboolean headers,
				gboolean body)
{
	fetch_data *data = g_new0(fetch_data, 1);
	int result = 0;
	data->done = FALSE;
	data->session = session;
	data->uid = uid;
	data->filename = filename;
	data->headers = headers;
	data->body = body;

	if (prefs_common.work_offline && !inc_offline_should_override()) {
		g_free(data);
		return -1;
	}
	statusbar_print_all(_("Fetching message..."));
	result = GPOINTER_TO_INT(imap_cmd_fetch_thread(data));
	statusbar_pop_all();
	g_free(data);
	return result;
}


static gint imap_cmd_append(IMAPSession *session, const gchar *destfolder,
			    const gchar *file, IMAPFlags flags, 
			    guint32 *new_uid)
{
	struct mailimap_flag_list * flag_list;
	int r;
	
	g_return_val_if_fail(file != NULL, IMAP_ERROR);

	flag_list = imap_flag_to_lep(flags);
	r = imap_threaded_append(session->folder, destfolder,
			 file, flag_list);
	mailimap_flag_list_free(flag_list);
	if (new_uid != NULL)
		*new_uid = 0;

	if (r != MAILIMAP_NO_ERROR) {
		debug_print("append err %d\n", r);
		return IMAP_ERROR;
	}
	return IMAP_SUCCESS;
}

static gint imap_cmd_copy(IMAPSession *session, struct mailimap_set * set,
			  const gchar *destfolder, GRelation *uid_mapping)
{
	int r;
	
	g_return_val_if_fail(session != NULL, IMAP_ERROR);
	g_return_val_if_fail(set != NULL, IMAP_ERROR);
	g_return_val_if_fail(destfolder != NULL, IMAP_ERROR);

	r = imap_threaded_copy(session->folder, set, destfolder);
	if (r != MAILIMAP_NO_ERROR) {
		
		return IMAP_ERROR;
	}

	return IMAP_SUCCESS;
}

static gint imap_cmd_store(IMAPSession *session, struct mailimap_set * set,
			   IMAPFlags flags, int do_add)
{
	int r;
	struct mailimap_flag_list * flag_list;
	struct mailimap_store_att_flags * store_att_flags;
	
	flag_list = imap_flag_to_lep(flags);
	
	if (do_add)
		store_att_flags =
			mailimap_store_att_flags_new_add_flags_silent(flag_list);
	else
		store_att_flags =
			mailimap_store_att_flags_new_remove_flags_silent(flag_list);
	
	r = imap_threaded_store(session->folder, set, store_att_flags);
	mailimap_store_att_flags_free(store_att_flags);
	if (r != MAILIMAP_NO_ERROR) {
		
		return IMAP_ERROR;
	}
	
	return IMAP_SUCCESS;
}

static gint imap_cmd_expunge(IMAPSession *session)
{
	int r;
	
	if (prefs_common.work_offline && !inc_offline_should_override()) {
		return -1;
	}

	r = imap_threaded_expunge(session->folder);
	if (r != MAILIMAP_NO_ERROR) {
		
		return IMAP_ERROR;
	}

	return IMAP_SUCCESS;
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

static gchar *imap_modified_utf7_to_utf8(const gchar *mutf7_str)
{
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
		cd = iconv_open(CS_INTERNAL, CS_UTF_7);
		if (cd == (iconv_t)-1) {
			g_warning("iconv cannot convert UTF-7 to %s\n",
				  CS_INTERNAL);
			iconv_ok = FALSE;
			return g_strdup(mutf7_str);
		}
	}

	/* modified UTF-7 to normal UTF-7 conversion */
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
			  conv_get_locale_charset_str());
		g_string_free(norm_utf7, TRUE);
		g_free(to_str);
		return g_strdup(mutf7_str);
	}

	/* second iconv() call for flushing */
	iconv(cd, NULL, NULL, &to_p, &to_len);
	g_string_free(norm_utf7, TRUE);
	*to_p = '\0';

	return to_str;
}

static gchar *imap_utf8_to_modified_utf7(const gchar *from)
{
	static iconv_t cd = (iconv_t)-1;
	static gboolean iconv_ok = TRUE;
	gchar *norm_utf7, *norm_utf7_p;
	size_t from_len, norm_utf7_len;
	GString *to_str;
	gchar *from_tmp, *to, *p;
	gboolean in_escape = FALSE;

	if (!iconv_ok) return g_strdup(from);

	if (cd == (iconv_t)-1) {
		cd = iconv_open(CS_UTF_7, CS_INTERNAL);
		if (cd == (iconv_t)-1) {
			g_warning(_("iconv cannot convert %s to UTF-7\n"),
				  CS_INTERNAL);
			iconv_ok = FALSE;
			return g_strdup(from);
		}
	}

	/* UTF-8 to normal UTF-7 conversion */
	Xstrdup_a(from_tmp, from, return g_strdup(from));
	from_len = strlen(from);
	norm_utf7_len = from_len * 5;
	Xalloca(norm_utf7, norm_utf7_len + 1, return g_strdup(from));
	norm_utf7_p = norm_utf7;

#define IS_PRINT(ch) (isprint(ch) && IS_ASCII(ch))

	while (from_len > 0) {
		if (*from_tmp == '+') {
			*norm_utf7_p++ = '+';
			*norm_utf7_p++ = '-';
			norm_utf7_len -= 2;
			from_tmp++;
			from_len--;
		} else if (IS_PRINT(*(guchar *)from_tmp)) {
			/* printable ascii char */
			*norm_utf7_p = *from_tmp;
			norm_utf7_p++;
			norm_utf7_len--;
			from_tmp++;
			from_len--;
		} else {
			size_t conv_len = 0;

			/* unprintable char: convert to UTF-7 */
			p = from_tmp;
			while (!IS_PRINT(*(guchar *)p) && conv_len < from_len) {
				conv_len += g_utf8_skip[*(guchar *)p];
				p += g_utf8_skip[*(guchar *)p];
			}

			from_len -= conv_len;
			if (iconv(cd, (ICONV_CONST gchar **)&from_tmp,
				  &conv_len,
				  &norm_utf7_p, &norm_utf7_len) == -1) {
				g_warning(_("iconv cannot convert UTF-8 to UTF-7\n"));
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

typedef struct _get_list_uid_data {
	Folder *folder;
	IMAPSession *session;
	IMAPFolderItem *item;
	GSList **msgnum_list;
	gboolean done;
} get_list_uid_data;

static void *get_list_of_uids_thread(void *data)
{
	get_list_uid_data *stuff = (get_list_uid_data *)data;
	Folder *folder = stuff->folder;
	IMAPFolderItem *item = stuff->item;
	GSList **msgnum_list = stuff->msgnum_list;
	gint ok, nummsgs = 0, lastuid_old;
	IMAPSession *session;
	GSList *uidlist, *elem;
	struct mailimap_set * set;
	clist * lep_uidlist;
	int r;

	session = stuff->session;
	if (session == NULL) {
		stuff->done = TRUE;
		return GINT_TO_POINTER(-1);
	}
	/* no session locking here, it's already locked by caller */
	ok = imap_select(session, IMAP_FOLDER(folder), item->item.path,
			 NULL, NULL, NULL, NULL, TRUE);
	if (ok != IMAP_SUCCESS) {
		stuff->done = TRUE;
		return GINT_TO_POINTER(-1);
	}

	uidlist = NULL;
	
	set = mailimap_set_new_interval(item->lastuid + 1, 0);

	r = imap_threaded_search(folder, IMAP_SEARCH_TYPE_SIMPLE, set,
				 &lep_uidlist);
	mailimap_set_free(set);
	
	if (r == MAILIMAP_NO_ERROR) {
		GSList * fetchuid_list;
		
		fetchuid_list =
			imap_uid_list_from_lep(lep_uidlist);
		mailimap_search_result_free(lep_uidlist);
		
		uidlist = g_slist_concat(fetchuid_list, uidlist);
	}
	else {
		GSList * fetchuid_list;
		carray * lep_uidtab;
		
		r = imap_threaded_fetch_uid(folder, item->lastuid + 1,
					    &lep_uidtab);
		if (r == MAILIMAP_NO_ERROR) {
			fetchuid_list =
				imap_uid_list_from_lep_tab(lep_uidtab);
			imap_fetch_uid_list_free(lep_uidtab);
			uidlist = g_slist_concat(fetchuid_list, uidlist);
		}
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
	stuff->done = TRUE;
	return GINT_TO_POINTER(nummsgs);
}

static gint get_list_of_uids(IMAPSession *session, Folder *folder, IMAPFolderItem *item, GSList **msgnum_list)
{
	gint result;
	get_list_uid_data *data = g_new0(get_list_uid_data, 1);
	data->done = FALSE;
	data->folder = folder;
	data->item = item;
	data->msgnum_list = msgnum_list;
	data->session = session;
	if (prefs_common.work_offline && !inc_offline_should_override()) {
		g_free(data);
		return -1;
	}

	result = GPOINTER_TO_INT(get_list_of_uids_thread(data));
	g_free(data);
	return result;

}

gint imap_get_num_list(Folder *folder, FolderItem *_item, GSList **msgnum_list, gboolean *old_uids_valid)
{
	IMAPFolderItem *item = (IMAPFolderItem *)_item;
	IMAPSession *session;
	gint ok, nummsgs = 0, exists, uid_val, uid_next;
	GSList *uidlist = NULL;
	gchar *dir;
	gboolean selected_folder;
	
	debug_print("get_num_list\n");
	
	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(item != NULL, -1);
	g_return_val_if_fail(item->item.path != NULL, -1);
	g_return_val_if_fail(FOLDER_CLASS(folder) == &imap_class, -1);
	g_return_val_if_fail(folder->account != NULL, -1);

	session = imap_session_get(folder);
	g_return_val_if_fail(session != NULL, -1);
	lock_session();
	statusbar_print_all("Scanning %s...\n", FOLDER_ITEM(item)->path 
				? FOLDER_ITEM(item)->path:"");

	selected_folder = (session->mbox != NULL) &&
			  (!strcmp(session->mbox, item->item.path));
	if (selected_folder) {
		ok = imap_cmd_noop(session);
		if (ok != IMAP_SUCCESS) {
			debug_print("disconnected!\n");
			session = imap_reconnect_if_possible(folder, session);
			if (session == NULL) {
				statusbar_pop_all();
				unlock_session();
				return -1;
			}
		}
		exists = session->exists;

		*old_uids_valid = TRUE;
	} else {
		if (item->use_cache && time(NULL) - item->use_cache < 2) {
			exists = item->c_messages;
			uid_next = item->c_uid_next;
			uid_val = item->c_uid_validity;
			ok = IMAP_SUCCESS;
			debug_print("using cache %d %d %d\n", exists, uid_next, uid_val);
		} else {
			ok = imap_status(session, IMAP_FOLDER(folder), item->item.path, item,
				 &exists, &uid_next, &uid_val, NULL, FALSE);
		}
		item->use_cache = (time_t)0;
		if (ok != IMAP_SUCCESS) {
			statusbar_pop_all();
			unlock_session();
			return -1;
		}
		if(item->item.mtime == uid_val)
			*old_uids_valid = TRUE;
		else {
			*old_uids_valid = FALSE;

			debug_print("Freeing imap uid cache\n");
			item->lastuid = 0;
			g_slist_free(item->uid_list);
			item->uid_list = NULL;
		
    			item->item.mtime = uid_val;

			imap_delete_all_cached_messages((FolderItem *)item);
		}
	}

	if (!selected_folder)
		item->uid_next = uid_next;

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
			statusbar_pop_all();
			unlock_session();
			return nummsgs;
		} else if (exists < nummsgs) {
			debug_print("Freeing imap uid cache");
			item->lastuid = 0;
			g_slist_free(item->uid_list);
			item->uid_list = NULL;
		}
	}

	if (exists == 0) {
		*msgnum_list = NULL;
		statusbar_pop_all();
		unlock_session();
		return 0;
	}

	nummsgs = get_list_of_uids(session, folder, item, &uidlist);

	if (nummsgs < 0) {
		statusbar_pop_all();
		unlock_session();
		return -1;
	}

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

		nummsgs = get_list_of_uids(session, folder, item, &uidlist);
	}

	*msgnum_list = uidlist;

	dir = folder_item_get_path((FolderItem *)item);
	debug_print("removing old messages from %s\n", dir);
	remove_numbered_files_not_in_list(dir, *msgnum_list);
	g_free(dir);
	
	debug_print("get_num_list - ok - %i\n", nummsgs);
	statusbar_pop_all();
	unlock_session();
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

	if (folder_has_parent_of_type(item, F_QUEUE)) {
		MSG_SET_TMP_FLAGS(flags, MSG_QUEUED);
	} else if (folder_has_parent_of_type(item, F_DRAFT)) {
		MSG_SET_TMP_FLAGS(flags, MSG_DRAFT);
	}

	msginfo = procheader_parse_file(file, flags, FALSE, FALSE);
	if (!msginfo) return NULL;
	
	msginfo->plaintext_file = g_strdup(file);
	msginfo->folder = item;

	return msginfo;
}

GSList *imap_get_msginfos(Folder *folder, FolderItem *item,
			  GSList *msgnum_list)
{
	IMAPSession *session;
	MsgInfoList *ret = NULL;
	gint ok;
	
	debug_print("get_msginfos\n");
	
	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(msgnum_list != NULL, NULL);

	session = imap_session_get(folder);
	g_return_val_if_fail(session != NULL, NULL);
	lock_session();
	debug_print("IMAP getting msginfos\n");
	ok = imap_select(session, IMAP_FOLDER(folder), item->path,
			 NULL, NULL, NULL, NULL, FALSE);
	if (ok != IMAP_SUCCESS) {
		unlock_session();
		return NULL;
	}
	if (!(folder_has_parent_of_type(item, F_DRAFT) || 
	      folder_has_parent_of_type(item, F_QUEUE))) {
		ret = g_slist_concat(ret,
			imap_get_uncached_messages(session, item,
						   msgnum_list));
	} else {
		MsgNumberList *sorted_list, *elem;
		gint startnum, lastnum;

 		sorted_list = g_slist_sort(g_slist_copy(msgnum_list), g_int_compare);

		startnum = lastnum = GPOINTER_TO_INT(sorted_list->data);

		for (elem = sorted_list;; elem = g_slist_next(elem)) {
			guint num = 0;

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
	unlock_session();
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

gboolean imap_scan_required(Folder *folder, FolderItem *_item)
{
	IMAPSession *session;
	IMAPFolderItem *item = (IMAPFolderItem *)_item;
	gint ok, exists = 0, unseen = 0;
	guint32 uid_next, uid_val;
	gboolean selected_folder;
	
	g_return_val_if_fail(folder != NULL, FALSE);
	g_return_val_if_fail(item != NULL, FALSE);
	g_return_val_if_fail(item->item.folder != NULL, FALSE);
	g_return_val_if_fail(FOLDER_CLASS(item->item.folder) == &imap_class, FALSE);

	if (item->item.path == NULL)
		return FALSE;

	session = imap_session_get(folder);
	g_return_val_if_fail(session != NULL, FALSE);
	lock_session();
	selected_folder = (session->mbox != NULL) &&
			  (!strcmp(session->mbox, item->item.path));
	if (selected_folder) {
		ok = imap_cmd_noop(session);
		if (ok != IMAP_SUCCESS) {
			debug_print("disconnected!\n");
			session = imap_reconnect_if_possible(folder, session);
			if (session == NULL)
				return FALSE;
			lock_session();
		}

		if (session->folder_content_changed
		||  session->exists != item->item.total_msgs) {
			unlock_session();
			return TRUE;
		}
	} else {
		ok = imap_status(session, IMAP_FOLDER(folder), item->item.path, IMAP_FOLDER_ITEM(item),
				 &exists, &uid_next, &uid_val, &unseen, FALSE);
		if (ok != IMAP_SUCCESS) {
			unlock_session();
			return FALSE;
		}

		item->use_cache = time(NULL);
		item->c_messages = exists;
		item->c_uid_next = uid_next;
		item->c_uid_validity = uid_val;
		item->c_unseen = unseen;

		if ((uid_next != item->uid_next) || (exists != item->item.total_msgs)) {
			unlock_session();
			return TRUE;
		}
	}
	unlock_session();
	return FALSE;
}

void imap_change_flags(Folder *folder, FolderItem *item, MsgInfo *msginfo, MsgPermFlags newflags)
{
	IMAPSession *session;
	IMAPFlags flags_set = 0, flags_unset = 0;
	gint ok = IMAP_SUCCESS;
	MsgNumberList numlist;
	hashtable_data *ht_data = NULL;

	g_return_if_fail(folder != NULL);
	g_return_if_fail(folder->klass == &imap_class);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder == folder);
	g_return_if_fail(msginfo != NULL);
	g_return_if_fail(msginfo->folder == item);

	session = imap_session_get(folder);
	if (!session) {
		return;
	}
	lock_session();
	if ((ok = imap_select(session, IMAP_FOLDER(folder), msginfo->folder->path,
	    NULL, NULL, NULL, NULL, FALSE)) != IMAP_SUCCESS) {
	    	unlock_session();
		return;
	}

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
		flags_unset |= IMAP_FLAG_ANSWERED;

	if (!MSG_IS_DELETED(msginfo->flags) &&  (newflags & MSG_DELETED))
		flags_set |= IMAP_FLAG_DELETED;
	if ( MSG_IS_DELETED(msginfo->flags) && !(newflags & MSG_DELETED))
		flags_unset |= IMAP_FLAG_DELETED;

	numlist.next = NULL;
	numlist.data = GINT_TO_POINTER(msginfo->msgnum);

	if (IMAP_FOLDER_ITEM(item)->batching) {
		/* instead of performing an UID STORE command for each message change,
		 * as a lot of them can change "together", we just fill in hashtables
		 * and defer the treatment so that we're able to send only one
		 * command.
		 */
		debug_print("IMAP batch mode on, deferring flags change\n");
		if (flags_set) {
			ht_data = g_hash_table_lookup(IMAP_FOLDER_ITEM(item)->flags_set_table, 
				GINT_TO_POINTER(flags_set));
			if (ht_data == NULL) {
				ht_data = g_new0(hashtable_data, 1);
				ht_data->session = session;
				ht_data->item = IMAP_FOLDER_ITEM(item);
				g_hash_table_insert(IMAP_FOLDER_ITEM(item)->flags_set_table, 
					GINT_TO_POINTER(flags_set), ht_data);
			}
			if (!g_slist_find(ht_data->msglist, GINT_TO_POINTER(msginfo->msgnum)))
				ht_data->msglist = g_slist_prepend(ht_data->msglist, GINT_TO_POINTER(msginfo->msgnum));
		} 
		if (flags_unset) {
			ht_data = g_hash_table_lookup(IMAP_FOLDER_ITEM(item)->flags_unset_table, 
				GINT_TO_POINTER(flags_unset));
			if (ht_data == NULL) {
				ht_data = g_new0(hashtable_data, 1);
				ht_data->session = session;
				ht_data->item = IMAP_FOLDER_ITEM(item);
				g_hash_table_insert(IMAP_FOLDER_ITEM(item)->flags_unset_table, 
					GINT_TO_POINTER(flags_unset), ht_data);
			}
			if (!g_slist_find(ht_data->msglist, GINT_TO_POINTER(msginfo->msgnum)))
				ht_data->msglist = g_slist_prepend(ht_data->msglist, 
					GINT_TO_POINTER(msginfo->msgnum));		
		}
	} else {
		debug_print("IMAP changing flags\n");
		if (flags_set) {
			ok = imap_set_message_flags(session, &numlist, flags_set, TRUE);
			if (ok != IMAP_SUCCESS) {
				unlock_session();
				return;
			}
		}

		if (flags_unset) {
			ok = imap_set_message_flags(session, &numlist, flags_unset, FALSE);
			if (ok != IMAP_SUCCESS) {
				unlock_session();
				return;
			}
		}
	}
	msginfo->flags.perm_flags = newflags;
	unlock_session();
	return;
}

static gint imap_remove_msg(Folder *folder, FolderItem *item, gint uid)
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
	lock_session();
	ok = imap_select(session, IMAP_FOLDER(folder), item->path,
			 NULL, NULL, NULL, NULL, FALSE);
	if (ok != IMAP_SUCCESS) {
		unlock_session();
		return ok;
	}
	numlist.next = NULL;
	numlist.data = GINT_TO_POINTER(uid);
	
	ok = imap_set_message_flags
		(IMAP_SESSION(REMOTE_FOLDER(folder)->session),
		&numlist, IMAP_FLAG_DELETED, TRUE);
	if (ok != IMAP_SUCCESS) {
		log_warning(_("can't set deleted flags: %d\n"), uid);
		unlock_session();
		return ok;
	}

	if (!session->uidplus) {
		ok = imap_cmd_expunge(session);
	} else {
		gchar *uidstr;

		uidstr = g_strdup_printf("%u", uid);
		ok = imap_cmd_expunge(session);
		g_free(uidstr);
	}
	if (ok != IMAP_SUCCESS) {
		log_warning(_("can't expunge\n"));
		unlock_session();
		return ok;
	}

	IMAP_FOLDER_ITEM(item)->uid_list = g_slist_remove(
	    IMAP_FOLDER_ITEM(item)->uid_list, numlist.data);
	dir = folder_item_get_path(item);
	if (is_dir_exist(dir))
		remove_numbered_files(dir, uid, uid);
	g_free(dir);
	unlock_session();
	return IMAP_SUCCESS;
}

static gint compare_msginfo(gconstpointer a, gconstpointer b)
{
	return ((MsgInfo *)a)->msgnum - ((MsgInfo *)b)->msgnum;
}

static guint gslist_find_next_num(MsgNumberList **list, guint num)
{
	GSList *elem;

	g_return_val_if_fail(list != NULL, -1);

	for (elem = *list; elem != NULL; elem = g_slist_next(elem))
		if (GPOINTER_TO_INT(elem->data) >= num)
			break;
	*list = elem;
	return elem != NULL ? GPOINTER_TO_INT(elem->data) : (gint)-1;
}

/*
 * NEW and DELETED flags are not syncronized
 * - The NEW/RECENT flags in IMAP folders can not really be directly
 *   modified by Sylpheed
 * - The DELETE/DELETED flag in IMAP and Sylpheed don't have the same
 *   meaning, in IMAP it always removes the messages from the FolderItem
 *   in Sylpheed it can mean to move the message to trash
 */

typedef struct _get_flags_data {
	Folder *folder;
	FolderItem *item;
	MsgInfoList *msginfo_list;
	GRelation *msgflags;
	gboolean full_search;
	gboolean done;
} get_flags_data;

static /*gint*/ void *imap_get_flags_thread(void *data)
{
	get_flags_data *stuff = (get_flags_data *)data;
	Folder *folder = stuff->folder;
	FolderItem *item = stuff->item;
	MsgInfoList *msginfo_list = stuff->msginfo_list;
	GRelation *msgflags = stuff->msgflags;
	gboolean full_search = stuff->full_search;
	IMAPSession *session;
	GSList *sorted_list = NULL;
	GSList *unseen = NULL, *answered = NULL, *flagged = NULL, *deleted = NULL;
	GSList *p_unseen, *p_answered, *p_flagged, *p_deleted;
	GSList *elem;
	GSList *seq_list, *cur;
	gboolean reverse_seen = FALSE;
	GString *cmd_buf;
	gint ok;
	gint exists_cnt, unseen_cnt;
	gboolean selected_folder;
	
	if (folder == NULL || item == NULL) {
		stuff->done = TRUE;
		return GINT_TO_POINTER(-1);
	}

	session = imap_session_get(folder);
	if (session == NULL) {
		stuff->done = TRUE;
		return GINT_TO_POINTER(-1);
	}
	lock_session();
	selected_folder = (session->mbox != NULL) &&
			  (!strcmp(session->mbox, item->path));

	if (!selected_folder) {
		ok = imap_select(session, IMAP_FOLDER(folder), item->path,
			&exists_cnt, NULL, &unseen_cnt, NULL, TRUE);
		if (ok != IMAP_SUCCESS) {
			stuff->done = TRUE;
			unlock_session();
			return GINT_TO_POINTER(-1);
		}

		if (unseen_cnt > exists_cnt / 2)
			reverse_seen = TRUE;
	} 
	else {
		if (item->unread_msgs > item->total_msgs / 2)
			reverse_seen = TRUE;
	}

	cmd_buf = g_string_new(NULL);

	sorted_list = g_slist_sort(g_slist_copy(msginfo_list), compare_msginfo);
	if (!full_search) {
		seq_list = imap_get_lep_set_from_msglist(msginfo_list);
	} else {
		struct mailimap_set * set;
		set = mailimap_set_new_interval(1, 0);
		seq_list = g_slist_append(NULL, set);
	}

	for (cur = seq_list; cur != NULL; cur = g_slist_next(cur)) {
		struct mailimap_set * imapset;
		clist * lep_uidlist;
		int r;
		
		imapset = cur->data;
		if (reverse_seen) {
			r = imap_threaded_search(folder, IMAP_SEARCH_TYPE_SEEN,
						 imapset, &lep_uidlist);
		}
		else {
			r = imap_threaded_search(folder,
						 IMAP_SEARCH_TYPE_UNSEEN,
						 imapset, &lep_uidlist);
		}
		if (r == MAILIMAP_NO_ERROR) {
			GSList * uidlist;
			
			uidlist = imap_uid_list_from_lep(lep_uidlist);
			mailimap_search_result_free(lep_uidlist);
			
			unseen = g_slist_concat(unseen, uidlist);
		}
		
		r = imap_threaded_search(folder, IMAP_SEARCH_TYPE_ANSWERED,
					 imapset, &lep_uidlist);
		if (r == MAILIMAP_NO_ERROR) {
			GSList * uidlist;
			
			uidlist = imap_uid_list_from_lep(lep_uidlist);
			mailimap_search_result_free(lep_uidlist);
			
			answered = g_slist_concat(answered, uidlist);
		}

		r = imap_threaded_search(folder, IMAP_SEARCH_TYPE_FLAGGED,
					 imapset, &lep_uidlist);
		if (r == MAILIMAP_NO_ERROR) {
			GSList * uidlist;
			
			uidlist = imap_uid_list_from_lep(lep_uidlist);
			mailimap_search_result_free(lep_uidlist);
			
			flagged = g_slist_concat(flagged, uidlist);
		}
		
		r = imap_threaded_search(folder, IMAP_SEARCH_TYPE_DELETED,
					 imapset, &lep_uidlist);
		if (r == MAILIMAP_NO_ERROR) {
			GSList * uidlist;
			
			uidlist = imap_uid_list_from_lep(lep_uidlist);
			mailimap_search_result_free(lep_uidlist);
			
			deleted = g_slist_concat(deleted, uidlist);
		}
	}

	p_unseen = unseen;
	p_answered = answered;
	p_flagged = flagged;
	p_deleted = deleted;

	for (elem = sorted_list; elem != NULL; elem = g_slist_next(elem)) {
		MsgInfo *msginfo;
		MsgPermFlags flags;
		gboolean wasnew;
		
		msginfo = (MsgInfo *) elem->data;
		flags = msginfo->flags.perm_flags;
		wasnew = (flags & MSG_NEW);
		flags &= ~((reverse_seen ? 0 : MSG_UNREAD | MSG_NEW) | MSG_REPLIED | MSG_MARKED);
		if (reverse_seen)
			flags |= MSG_UNREAD | (wasnew ? MSG_NEW : 0);
		if (gslist_find_next_num(&p_unseen, msginfo->msgnum) == msginfo->msgnum) {
			if (!reverse_seen) {
				flags |= MSG_UNREAD | (wasnew ? MSG_NEW : 0);
			} else {
				flags &= ~(MSG_UNREAD | MSG_NEW);
			}
		}
		if (gslist_find_next_num(&p_answered, msginfo->msgnum) == msginfo->msgnum)
			flags |= MSG_REPLIED;
		else
			flags &= ~MSG_REPLIED;
		if (gslist_find_next_num(&p_flagged, msginfo->msgnum) == msginfo->msgnum)
			flags |= MSG_MARKED;
		else
			flags &= ~MSG_MARKED;
		if (gslist_find_next_num(&p_deleted, msginfo->msgnum) == msginfo->msgnum)
			flags |= MSG_DELETED;
		else
			flags &= ~MSG_DELETED;
		g_relation_insert(msgflags, msginfo, GINT_TO_POINTER(flags));
	}

	imap_lep_set_free(seq_list);
	g_slist_free(flagged);
	g_slist_free(deleted);
	g_slist_free(answered);
	g_slist_free(unseen);
	g_slist_free(sorted_list);
	g_string_free(cmd_buf, TRUE);

	stuff->done = TRUE;
	unlock_session();
	return GINT_TO_POINTER(0);
}

static gint imap_get_flags(Folder *folder, FolderItem *item,
                           MsgInfoList *msginfo_list, GRelation *msgflags)
{
	gint result;
	get_flags_data *data = g_new0(get_flags_data, 1);
	data->done = FALSE;
	data->folder = folder;
	data->item = item;
	data->msginfo_list = msginfo_list;
	data->msgflags = msgflags;
	data->full_search = FALSE;

	GSList *tmp = NULL, *cur;
	
	if (prefs_common.work_offline && !inc_offline_should_override()) {
		g_free(data);
		return -1;
	}

	tmp = folder_item_get_msg_list(item);

	if (g_slist_length(tmp) == g_slist_length(msginfo_list))
		data->full_search = TRUE;
	
	for (cur = tmp; cur; cur = cur->next)
		procmsg_msginfo_free((MsgInfo *)cur->data);
	
	g_slist_free(tmp);

	result = GPOINTER_TO_INT(imap_get_flags_thread(data));
	
	g_free(data);
	return result;

}

static gboolean process_flags(gpointer key, gpointer value, gpointer user_data)
{
	gboolean flags_set = GPOINTER_TO_INT(user_data);
	gint flags_value = GPOINTER_TO_INT(key);
	hashtable_data *data = (hashtable_data *)value;
	IMAPFolderItem *_item = data->item;
	FolderItem *item = (FolderItem *)_item;
	gint ok = IMAP_ERROR;
	IMAPSession *session = imap_session_get(item->folder);

	data->msglist = g_slist_reverse(data->msglist);
	
	debug_print("IMAP %ssetting flags to %d for %d messages\n",
		flags_set?"":"un",
		flags_value,
		g_slist_length(data->msglist));
	
	if (session) {
		lock_session();
		ok = imap_select(session, IMAP_FOLDER(item->folder), item->path,
			 NULL, NULL, NULL, NULL, FALSE);
	}
	if (ok == IMAP_SUCCESS) {
		imap_set_message_flags(data->session, data->msglist, flags_value, flags_set);
	} else {
		g_warning("can't select mailbox %s\n", item->path);
	}
	if (session)
		unlock_session();
	g_slist_free(data->msglist);	
	g_free(data);
	return TRUE;
}

static void process_hashtable(IMAPFolderItem *item)
{
	if (item->flags_set_table) {
		g_hash_table_foreach_remove(item->flags_set_table, process_flags, GINT_TO_POINTER(TRUE));
		g_hash_table_destroy(item->flags_set_table);
		item->flags_set_table = NULL;
	}
	if (item->flags_unset_table) {
		g_hash_table_foreach_remove(item->flags_unset_table, process_flags, GINT_TO_POINTER(FALSE));
		g_hash_table_destroy(item->flags_unset_table);
		item->flags_unset_table = NULL;
	}
}

static void imap_set_batch (Folder *folder, FolderItem *_item, gboolean batch)
{
	IMAPFolderItem *item = (IMAPFolderItem *)_item;

	g_return_if_fail(item != NULL);
	
	if (item->batching == batch)
		return;
	
	if (batch) {
		item->batching = TRUE;
		debug_print("IMAP switching to batch mode\n");
		if (!item->flags_set_table) {
			item->flags_set_table = g_hash_table_new(NULL, g_direct_equal);
		}
		if (!item->flags_unset_table) {
			item->flags_unset_table = g_hash_table_new(NULL, g_direct_equal);
		}
	} else {
		debug_print("IMAP switching away from batch mode\n");
		/* process stuff */
		process_hashtable(item);
		item->batching = FALSE;
	}
}



/* data types conversion libetpan <-> sylpheed */



#define ETPAN_IMAP_MB_MARKED      1
#define ETPAN_IMAP_MB_UNMARKED    2
#define ETPAN_IMAP_MB_NOSELECT    4
#define ETPAN_IMAP_MB_NOINFERIORS 8

static int imap_flags_to_flags(struct mailimap_mbx_list_flags * imap_flags)
{
  int flags;
  clistiter * cur;
  
  flags = 0;
  if (imap_flags->mbf_type == MAILIMAP_MBX_LIST_FLAGS_SFLAG) {
    switch (imap_flags->mbf_sflag) {
    case MAILIMAP_MBX_LIST_SFLAG_MARKED:
      flags |= ETPAN_IMAP_MB_MARKED;
      break;
    case MAILIMAP_MBX_LIST_SFLAG_NOSELECT:
      flags |= ETPAN_IMAP_MB_NOSELECT;
      break;
    case MAILIMAP_MBX_LIST_SFLAG_UNMARKED:
      flags |= ETPAN_IMAP_MB_UNMARKED;
      break;
    }
  }
  
  for(cur = clist_begin(imap_flags->mbf_oflags) ; cur != NULL ;
      cur = clist_next(cur)) {
    struct mailimap_mbx_list_oflag * oflag;
    
    oflag = clist_content(cur);
    
    switch (oflag->of_type) {
    case MAILIMAP_MBX_LIST_OFLAG_NOINFERIORS:
      flags |= ETPAN_IMAP_MB_NOINFERIORS;
      break;
    }
  }
  
  return flags;
}

static GSList * imap_list_from_lep(IMAPFolder * folder,
				   clist * list, const gchar * real_path, gboolean all)
{
	clistiter * iter;
	GSList * item_list;
	
	item_list = NULL;
	
	for(iter = clist_begin(list) ; iter != NULL ;
	    iter = clist_next(iter)) {
		struct mailimap_mailbox_list * mb;
		int flags;
		char delimiter;
		char * name;
		char * dup_name;
		gchar * base;
		gchar * loc_name;
		gchar * loc_path;
		FolderItem *new_item;
		
		mb = clist_content(iter);

		if (mb == NULL)
			continue;

		flags = 0;
		if (mb->mb_flag != NULL)
			flags = imap_flags_to_flags(mb->mb_flag);
		
		delimiter = mb->mb_delimiter;
		name = mb->mb_name;
		
		dup_name = strdup(name);		
		if (delimiter != '\0')
			subst_char(dup_name, delimiter, '/');
		
		base = g_path_get_basename(dup_name);
		if (base[0] == '.') {
			g_free(base);
			free(dup_name);
			continue;
		}
		
		if (!all && strcmp(dup_name, real_path) == 0) {
			g_free(base);
			free(dup_name);
			continue;
		}

		if (!all && dup_name[strlen(dup_name)-1] == '/') {
			g_free(base);
			free(dup_name);
			continue;
		}
		
		loc_name = imap_modified_utf7_to_utf8(base);
		loc_path = imap_modified_utf7_to_utf8(dup_name);
		
		new_item = folder_item_new(FOLDER(folder), loc_name, loc_path);
		if ((flags & ETPAN_IMAP_MB_NOINFERIORS) != 0)
			new_item->no_sub = TRUE;
		if (strcmp(dup_name, "INBOX") != 0 &&
		    ((flags & ETPAN_IMAP_MB_NOSELECT) != 0))
			new_item->no_select = TRUE;
		
		item_list = g_slist_append(item_list, new_item);
		
		debug_print("folder '%s' found.\n", loc_path);
		g_free(base);
		g_free(loc_path);
		g_free(loc_name);
		
		free(dup_name);
	}
	
	return item_list;
}

static GSList * imap_get_lep_set_from_numlist(MsgNumberList *numlist)
{
	GSList *sorted_list, *cur;
	guint first, last, next;
	GSList *ret_list = NULL;
	unsigned int count;
	struct mailimap_set * current_set;
	unsigned int item_count;
	
	if (numlist == NULL)
		return NULL;
	
	count = 0;
	current_set = mailimap_set_new_empty();
	
	sorted_list = g_slist_copy(numlist);
	sorted_list = g_slist_sort(sorted_list, g_int_compare);

	first = GPOINTER_TO_INT(sorted_list->data);
	
	item_count = 0;
	for (cur = sorted_list; cur != NULL; cur = g_slist_next(cur)) {
		if (GPOINTER_TO_INT(cur->data) == 0)
			continue;
		
		item_count ++;

		last = GPOINTER_TO_INT(cur->data);
		if (cur->next)
			next = GPOINTER_TO_INT(cur->next->data);
		else
			next = 0;

		if (last + 1 != next || next == 0) {

			struct mailimap_set_item * item;
			item = mailimap_set_item_new(first, last);
			mailimap_set_add(current_set, item);
			count ++;
			
			first = next;
			
			if (count >= IMAP_SET_MAX_COUNT) {
				ret_list = g_slist_append(ret_list,
							  current_set);
				current_set = mailimap_set_new_empty();
				count = 0;
				item_count = 0;
			}
		}
	}
	
	if (clist_count(current_set->set_list) > 0) {
		ret_list = g_slist_append(ret_list,
					  current_set);
	}
	
	g_slist_free(sorted_list);

	return ret_list;
}

static GSList * imap_get_lep_set_from_msglist(MsgInfoList *msglist)
{
	MsgNumberList *numlist = NULL;
	MsgInfoList *cur;
	GSList *seq_list;

	for (cur = msglist; cur != NULL; cur = g_slist_next(cur)) {
		MsgInfo *msginfo = (MsgInfo *) cur->data;

		numlist = g_slist_append(numlist, GINT_TO_POINTER(msginfo->msgnum));
	}
	seq_list = imap_get_lep_set_from_numlist(numlist);
	g_slist_free(numlist);

	return seq_list;
}

static GSList * imap_uid_list_from_lep(clist * list)
{
	clistiter * iter;
	GSList * result;
	
	result = NULL;
	
	for(iter = clist_begin(list) ; iter != NULL ;
	    iter = clist_next(iter)) {
		uint32_t * puid;
		
		puid = clist_content(iter);
		result = g_slist_append(result, GINT_TO_POINTER(* puid));
	}
	
	return result;
}

static GSList * imap_uid_list_from_lep_tab(carray * list)
{
	unsigned int i;
	GSList * result;
	
	result = NULL;
	
	for(i = 0 ; i < carray_count(list) ; i ++) {
		uint32_t * puid;
		
		puid = carray_get(list, i);
		result = g_slist_append(result, GINT_TO_POINTER(* puid));
	}
	
	return result;
}

static MsgInfo *imap_envelope_from_lep(struct imap_fetch_env_info * info,
				       FolderItem *item)
{
	MsgInfo *msginfo = NULL;
	guint32 uid = 0;
	size_t size = 0;
	MsgFlags flags = {0, 0};
	
	if (info->headers == NULL)
		return NULL;

	MSG_SET_TMP_FLAGS(flags, MSG_IMAP);
	if (folder_has_parent_of_type(item, F_QUEUE)) {
		MSG_SET_TMP_FLAGS(flags, MSG_QUEUED);
	} else if (folder_has_parent_of_type(item, F_DRAFT)) {
		MSG_SET_TMP_FLAGS(flags, MSG_DRAFT);
	}
	flags.perm_flags = info->flags;
	
	uid = info->uid;
	size = info->size;
	msginfo = procheader_parse_str(info->headers, flags, FALSE, FALSE);
	
	if (msginfo) {
		msginfo->msgnum = uid;
		msginfo->size = size;
	}

	return msginfo;
}

static void imap_lep_set_free(GSList *seq_list)
{
	GSList * cur;
	
	for(cur = seq_list ; cur != NULL ; cur = g_slist_next(cur)) {
		struct mailimap_set * imapset;
		
		imapset = cur->data;
		mailimap_set_free(imapset);
	}
	g_slist_free(seq_list);
}

static struct mailimap_flag_list * imap_flag_to_lep(IMAPFlags flags)
{
	struct mailimap_flag_list * flag_list;
	
	flag_list = mailimap_flag_list_new_empty();
	
	if (IMAP_IS_SEEN(flags))
		mailimap_flag_list_add(flag_list,
				       mailimap_flag_new_seen());
	if (IMAP_IS_ANSWERED(flags))
		mailimap_flag_list_add(flag_list,
				       mailimap_flag_new_answered());
	if (IMAP_IS_FLAGGED(flags))
		mailimap_flag_list_add(flag_list,
				       mailimap_flag_new_flagged());
	if (IMAP_IS_DELETED(flags))
		mailimap_flag_list_add(flag_list,
				       mailimap_flag_new_deleted());
	if (IMAP_IS_DRAFT(flags))
		mailimap_flag_list_add(flag_list,
				       mailimap_flag_new_draft());
	
	return flag_list;
}

guint imap_folder_get_refcnt(Folder *folder)
{
	return ((IMAPFolder *)folder)->refcnt;
}

void imap_folder_ref(Folder *folder)
{
	((IMAPFolder *)folder)->refcnt++;
}

void imap_folder_unref(Folder *folder)
{
	if (((IMAPFolder *)folder)->refcnt > 0)
		((IMAPFolder *)folder)->refcnt--;
}

#else /* HAVE_LIBETPAN */

static FolderClass imap_class;

static Folder	*imap_folder_new	(const gchar	*name,
					 const gchar	*path)
{
	return NULL;
}
static gint 	imap_create_tree	(Folder 	*folder)
{
	return -1;
}
static FolderItem *imap_create_folder	(Folder 	*folder,
				      	 FolderItem 	*parent,
				      	 const gchar 	*name)
{
	return NULL;
}
static gint 	imap_rename_folder	(Folder 	*folder,
			       		 FolderItem 	*item, 
					 const gchar 	*name)
{
	return -1;
}

FolderClass *imap_get_class(void)
{
	if (imap_class.idstr == NULL) {
		imap_class.type = F_IMAP;
		imap_class.idstr = "imap";
		imap_class.uistr = "IMAP4";

		imap_class.new_folder = imap_folder_new;
		imap_class.create_tree = imap_create_tree;
		imap_class.create_folder = imap_create_folder;
		imap_class.rename_folder = imap_rename_folder;
		/* nothing implemented */
	}

	return &imap_class;
}
#endif

void imap_synchronise(FolderItem *item) 
{
	imap_gtk_synchronise(item);
}
