/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2023 the Claws Mail team and Hiroyuki Yamamoto
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
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <stdio.h>
#include <string.h>
#include "imap.h"
#include "imap_gtk.h"
#include "inc.h"
#include "xml.h"
#include "alertpanel.h"

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

#ifdef USE_GNUTLS
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
#include "utils.h"
#include "prefs_common.h"
#include "inputdialog.h"
#include "log.h"
#include "remotefolder.h"
#include "claws.h"
#include "statusbar.h"
#include "msgcache.h"
#include "imap-thread.h"
#include "account.h"
#include "tags.h"
#include "main.h"
#include "passwordstore.h"
#include "file-utils.h"
#ifdef USE_OAUTH2
#include "oauth2.h"
#endif

typedef struct _IMAPFolder	IMAPFolder;
typedef struct _IMAPSession	IMAPSession;
typedef struct _IMAPNameSpace	IMAPNameSpace;
typedef struct _IMAPFolderItem	IMAPFolderItem;

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
	guint max_set_size;
	gchar *search_charset;
	gboolean search_charset_supported;
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
	guint recent;
	guint expunge;
	guint unseen;
	guint uid_validity;
	guint uid_next;

	Folder * folder;
	gboolean busy;
	gboolean cancelled;
	gboolean sens_update_block;
	gboolean do_destroy;

    gint scan_tree_recurs_depth;
};

struct _IMAPNameSpace
{
	gchar *name;
	gchar separator;
};

#define IMAPBUFSIZE	8192

#define IMAP_IS_SEEN(flags)	((flags & IMAP_FLAG_SEEN) != 0)
#define IMAP_IS_ANSWERED(flags)	((flags & IMAP_FLAG_ANSWERED) != 0)
#define IMAP_IS_FLAGGED(flags)	((flags & IMAP_FLAG_FLAGGED) != 0)
#define IMAP_IS_DELETED(flags)	((flags & IMAP_FLAG_DELETED) != 0)
#define IMAP_IS_DRAFT(flags)	((flags & IMAP_FLAG_DRAFT) != 0)
#define IMAP_IS_FORWARDED(flags)	((flags & IMAP_FLAG_FORWARDED) != 0)
#define IMAP_IS_SPAM(flags)	((flags & IMAP_FLAG_SPAM) != 0)
#define IMAP_IS_HAM(flags)	((flags & IMAP_FLAG_HAM) != 0)


#define IMAP4_PORT	143
#ifdef USE_GNUTLS
#define IMAPS_PORT	993
#endif

#define IMAP_CMD_LIMIT	1000

enum {
	ITEM_CAN_CREATE_FLAGS_UNKNOWN = 0,
	ITEM_CAN_CREATE_FLAGS,
	ITEM_CANNOT_CREATE_FLAGS
};

struct _IMAPFolderItem
{
	FolderItem item;

	guint lastuid;
	guint uid_next;
	GSList *uid_list;
	gboolean batching;

	GHashTable *flags_set_table;
	GHashTable *flags_unset_table;
	guint32 last_change;
	guint32 last_sync;
	gboolean should_update;
	gboolean should_trash_cache;
	gint can_create_flags;

	GHashTable *tags_set_table;
	GHashTable *tags_unset_table;
	GSList *ok_flags;

};

static XMLTag *imap_item_get_xml(Folder *folder, FolderItem *item);
static void imap_item_set_xml(Folder *folder, FolderItem *item, XMLTag *tag);

static void imap_folder_init		(Folder		*folder,
					 const gchar	*name,
					 const gchar	*path);

static Folder	*imap_folder_new	(const gchar	*name,
					 const gchar	*path);
static void	 imap_folder_destroy	(Folder		*folder);

static IMAPSession *imap_session_new	(Folder         *folder,
					 const PrefsAccount 	*account);
static gint 	imap_session_authenticate(IMAPSession 	*session,
				      	  PrefsAccount 	*account);
static void 	imap_session_destroy	(Session 	*session);

static gchar   *imap_fetch_msg		(Folder 	*folder, 
					 FolderItem 	*item, 
					 gint 		 uid);
static gchar   *imap_fetch_msg_full	(Folder 	*folder, 
					 FolderItem 	*item, 
					 gint 		 uid,
					 gboolean	 headers,
					 gboolean	 body);
static void	imap_remove_cached_msg	(Folder 	*folder, 
					 FolderItem 	*item, 
					 MsgInfo	*msginfo);
static gint 	imap_add_msg		(Folder 	*folder,
			 		 FolderItem 	*dest,
			 		 const gchar 	*file, 
					 MsgFlags 	*flags);
static gint 	imap_add_msgs		(Folder 	*folder, 
					 FolderItem 	*dest,
			  		 GSList 	*file_list,
			  		 GHashTable 	*relation);

static gint 	imap_copy_msg		(Folder 	*folder,
			  		 FolderItem 	*dest, 
					 MsgInfo 	*msginfo);
static gint 	imap_copy_msgs		(Folder 	*folder, 
					 FolderItem 	*dest, 
		    			 MsgInfoList 	*msglist, 
					 GHashTable 	*relation);

static gint	search_msgs		(Folder			*folder,
					 FolderItem		*container,
					 MsgNumberList		**msgs,
					 gboolean		*on_server,
					 MatcherList		*predicate,
					 SearchProgressNotify	progress_cb,
					 gpointer		progress_data);

static gint 	imap_remove_msg		(Folder 	*folder, 
					 FolderItem 	*item, 
					 gint 		 uid);
static gint 	imap_remove_msgs	(Folder 	*folder, 
					 FolderItem 	*dest, 
		    			 MsgInfoList 	*msglist, 
					 GHashTable 	*relation);
static gint 	imap_expunge		(Folder 	*folder, 
					 FolderItem 	*dest);
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
					 FolderItem	*item,
					 gboolean	 subs_only);
static gint imap_scan_tree_recursive_dive	(IMAPSession	*session,
					 FolderItem	*item,
					 gboolean	 subs_only);

static void imap_create_missing_folders	(Folder		*folder);
static FolderItem *imap_create_special_folder
					(Folder			*folder,
					 SpecialFolderItemType	 stype,
					 const gchar		*name);

static gint imap_do_copy_msgs		(Folder		*folder,
					 FolderItem	*dest,
					 MsgInfoList	*msglist,
					 GHashTable	*relation,
					 gboolean	 same_dest_ok);

static gint imap_do_remove_msgs		(Folder		*folder,
					 FolderItem	*dest,
					 MsgInfoList	*msglist,
					 GHashTable	*relation);

static void imap_delete_all_cached_messages	(FolderItem	*item);
static void imap_set_batch		(Folder		*folder,
					 FolderItem	*item,
					 gboolean	 batch);
static gint imap_set_message_flags	(IMAPSession	*session,
					 IMAPFolderItem *item,
					 MsgNumberList	*numlist,
					 IMAPFlags	 flags,
					 GSList		*tags,
					 gboolean	 is_set);
static gint imap_select			(IMAPSession	*session,
					 IMAPFolder	*folder,
					 FolderItem	*item,
					 gint		*exists,
					 gint		*recent,
					 gint		*unseen,
					 guint32	*uid_validity,
					 gint		*can_create_flags,
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
static void	imap_commit_tags	(FolderItem 	*item, 
					 MsgInfo	*msginfo,
					 GSList		*set_tags,
					 GSList		*unset_tags);

static gchar imap_get_path_separator		(IMAPSession	*session,
						 IMAPFolder	*folder,
						 const gchar	*path,
						 gint		*ok);
static gchar *imap_get_real_path		(IMAPSession	*session,
						 IMAPFolder	*folder,
						 const gchar	*path,
						 gint		*ok);
#ifdef HAVE_LIBETPAN
static void imap_synchronise		(FolderItem	*item, gint days);
#endif
static gboolean imap_is_busy		(Folder *folder);

static void imap_free_capabilities	(IMAPSession 	*session);

/* low-level IMAP4rev1 commands */
static gint imap_cmd_login	(IMAPSession	*session,
				 const gchar	*user,
				 const gchar	*pass,
				 const gchar 	*type);
static gint imap_cmd_noop	(IMAPSession	*session);
#ifdef USE_GNUTLS
static gint imap_cmd_starttls	(IMAPSession	*session);
#endif
static gint imap_cmd_select	(IMAPSession	*session,
				 const gchar	*folder,
				 gint		*exists,
				 gint		*recent,
				 gint		*unseen,
				 guint32	*uid_validity,
				 gint		*can_create_flags,
				 GSList		**ok_flags,
				 gboolean	 block);
static gint imap_cmd_close	(IMAPSession 	*session);
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
				 IMAPFolderItem *item,
				 const gchar	*destfolder,
				 const gchar	*file,
				 IMAPFlags	 flags,
				 guint32	*new_uid);
static gint imap_cmd_copy       (IMAPSession *session,
				 struct mailimap_set * set,
				 const gchar *destfolder,
				 struct mailimap_set ** source,
				 struct mailimap_set ** dest);
static gint imap_cmd_store	(IMAPSession	*session,
			   	 IMAPFolderItem *item,
				 struct mailimap_set * set,
				 IMAPFlags flags,
				 GSList *tags,
				 int do_add);
static gint imap_cmd_expunge	(IMAPSession	*session);

static void imap_path_separator_subst		(gchar		*str,
						 gchar		 separator);

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
						 GHashTable 	*msgflags);
static gchar *imap_item_get_path		(Folder		*folder,
						 FolderItem	*item);
static MsgInfo *imap_parse_msg(const gchar *file, FolderItem *item);


/* data types conversion libetpan <-> claws */
static GSList * imap_list_from_lep(IMAPFolder * folder,
				   clist * list, const gchar * real_path, gboolean all);
static GSList * imap_get_lep_set_from_numlist(IMAPFolder *folder, MsgNumberList *numlist);
static GSList * imap_get_lep_set_from_msglist(IMAPFolder *folder, MsgInfoList *msglist);
static GSList * imap_uid_list_from_lep(clist * list, gint* length);
static GSList * imap_uid_list_from_lep_tab(carray * list);
static void imap_flags_hash_from_lep_uid_flags_tab(carray * list,
						   GHashTable * hash,
						   GHashTable *tags_hash);
static MsgInfo *imap_envelope_from_lep(struct imap_fetch_env_info * info,
				       FolderItem *item);
static void imap_lep_set_free(GSList *seq_list);
static struct mailimap_flag_list * imap_flag_to_lep(IMAPFolderItem *item, IMAPFlags flags, GSList *tags);

typedef struct _hashtable_data {
	GSList *msglist;
	IMAPFolderItem *item;
} hashtable_data;

static FolderClass imap_class;

FolderClass *imap_get_class(void)
{
	if (imap_class.idstr == NULL) {
		imap_class.type = F_IMAP;
		imap_class.idstr = "imap";
		imap_class.uistr = "IMAP";
		imap_class.supports_server_search = TRUE;

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
		imap_class.set_xml = folder_set_xml;
		imap_class.get_xml = folder_get_xml;
		imap_class.item_set_xml = imap_item_set_xml;
		imap_class.item_get_xml = imap_item_get_xml;

		/* Message functions */
		imap_class.get_msginfo = imap_get_msginfo;
		imap_class.get_msginfos = imap_get_msginfos;
		imap_class.fetch_msg = imap_fetch_msg;
		imap_class.fetch_msg_full = imap_fetch_msg_full;
		imap_class.add_msg = imap_add_msg;
		imap_class.add_msgs = imap_add_msgs;
		imap_class.copy_msg = imap_copy_msg;
		imap_class.copy_msgs = imap_copy_msgs;
		imap_class.search_msgs = search_msgs;
		imap_class.remove_msg = imap_remove_msg;
		imap_class.remove_msgs = imap_remove_msgs;
		imap_class.expunge = imap_expunge;
		imap_class.remove_all_msg = imap_remove_all_msg;
		imap_class.is_msg_changed = imap_is_msg_changed;
		imap_class.change_flags = imap_change_flags;
		imap_class.get_flags = imap_get_flags;
		imap_class.set_batch = imap_set_batch;
		imap_class.synchronise = imap_synchronise;
		imap_class.remove_cached_msg = imap_remove_cached_msg;
		imap_class.commit_tags = imap_commit_tags;
	}
	
	return &imap_class;
}

static void imap_refresh_sensitivity (IMAPSession *session)
{
        MainWindow *mainwin;

	if (session->sens_update_block)
		return;
	mainwin = mainwindow_get_mainwindow();
	if (mainwin) {
		toolbar_main_set_sensitive(mainwin);
		main_window_set_menu_sensitive(mainwin);
	}
}

static void lock_session(IMAPSession *session)
{
	if (session) {
		debug_print("locking session %p (%d)\n", session, session->busy);
		if (session->busy)
			debug_print("         SESSION WAS LOCKED !!      \n");
                session->busy = TRUE;
		imap_refresh_sensitivity(session);
	} else {
		debug_print("can't lock null session\n");
	}
}

static void unlock_session(IMAPSession *session)
{
	if (session) {
		debug_print("unlocking session %p\n", session);
		session->busy = FALSE;
		imap_refresh_sensitivity(session);
	} else {
		debug_print("can't unlock null session\n");
	}
}

static gboolean imap_ping(gpointer data)
{
	Session *session = (Session *)data;
	IMAPSession *imap_session = IMAP_SESSION(session);
	int r;

	if (session->state != SESSION_READY)
		return FALSE;
	if (imap_session->busy || !imap_session->authenticated)
		return TRUE;
	
	lock_session(imap_session);
	r = imap_cmd_noop(imap_session);
	unlock_session(imap_session);

	return r == MAILIMAP_NO_ERROR;
}

static void imap_disc_session_destroy(IMAPSession *session)
{
	RemoteFolder *rfolder = NULL;

	if (session == NULL)
		return;

	rfolder = REMOTE_FOLDER(IMAP_SESSION(session)->folder);
	
	if (rfolder == NULL)
		return;
	log_warning(LOG_PROTOCOL, _("IMAP connection broken\n"));
	SESSION(session)->state = SESSION_DISCONNECTED;
	SESSION(session)->sock = NULL;
}

static void imap_safe_destroy(IMAPSession *session)
{
	if (!session->busy)
		session_destroy(SESSION(session));
	else
		session->do_destroy = TRUE;
}

static gboolean is_fatal(int libetpan_errcode)
{
	switch(libetpan_errcode) {
	case MAILIMAP_ERROR_STREAM:
	case MAILIMAP_ERROR_PROTOCOL:
	case MAILIMAP_ERROR_PARSE:
	case MAILIMAP_ERROR_BAD_STATE:
		return TRUE;
	default:
		return FALSE;
	}
}

#define MY_LOG_WARNING(concat_cmd, ...) \
	msg = concat_cmd; \
	log_warning(LOG_PROTOCOL, msg, __VA_ARGS__); \
	g_free(msg);

static void imap_handle_error(Session *session, const gchar *server, int libetpan_errcode)
{
	const gchar *session_server = (session ? session->server : NULL);
	gchar *msg;

	if (session_server == NULL)
		session_server = server;
	if (session_server == NULL)
		session_server = "(null)";

	switch(libetpan_errcode) {
	case MAILIMAP_NO_ERROR:
		return;
	case MAILIMAP_NO_ERROR_AUTHENTICATED:
		MY_LOG_WARNING(g_strconcat(_("IMAP error on %s:"), " ", _("authenticated"), "\n", NULL), session_server)
		break;
	case MAILIMAP_NO_ERROR_NON_AUTHENTICATED:
		MY_LOG_WARNING(g_strconcat(_("IMAP error on %s:"), " ", _("not authenticated"), "\n", NULL), session_server)
		break;
	case MAILIMAP_ERROR_BAD_STATE:
		MY_LOG_WARNING(g_strconcat(_("IMAP error on %s:"), " ", _("bad state"), "\n", NULL), session_server)
		break;
	case MAILIMAP_ERROR_STREAM:
		MY_LOG_WARNING(g_strconcat(_("IMAP error on %s:"), " ", _("stream error"), "\n", NULL), session_server)
		break;
	case MAILIMAP_ERROR_PARSE:
		MY_LOG_WARNING(g_strconcat(_("IMAP error on %s:"), " ", _("parse error "
					    "(very probably non-RFC compliance from the server)"), "\n", NULL), session_server)
		break;
	case MAILIMAP_ERROR_CONNECTION_REFUSED:
		MY_LOG_WARNING(g_strconcat(_("IMAP error on %s:"), " ", _("connection refused"), "\n", NULL), session_server)
		break;
	case MAILIMAP_ERROR_MEMORY:
		MY_LOG_WARNING(g_strconcat(_("IMAP error on %s:"), " ", _("memory error"), "\n", NULL), session_server)
		break;
	case MAILIMAP_ERROR_FATAL:
		MY_LOG_WARNING(g_strconcat(_("IMAP error on %s:"), " ", _("fatal error"), "\n", NULL), session_server)
		break;
	case MAILIMAP_ERROR_PROTOCOL:
		MY_LOG_WARNING(g_strconcat(_("IMAP error on %s:"), " ", _("protocol error "
					    "(very probably non-RFC compliance from the server)"), "\n", NULL), session_server)
		break;
	case MAILIMAP_ERROR_DONT_ACCEPT_CONNECTION:
		MY_LOG_WARNING(g_strconcat(_("IMAP error on %s:"), " ", _("connection not accepted"), "\n", NULL), session_server)
		break;
	case MAILIMAP_ERROR_APPEND:
		MY_LOG_WARNING(g_strconcat(_("IMAP error on %s:"), " ", _("APPEND error"), "\n", NULL), session_server)
		break;
	case MAILIMAP_ERROR_NOOP:
		MY_LOG_WARNING(g_strconcat(_("IMAP error on %s:"), " ", _("NOOP error"), "\n", NULL), session_server)
		break;
	case MAILIMAP_ERROR_LOGOUT:
		MY_LOG_WARNING(g_strconcat(_("IMAP error on %s:"), " ", _("LOGOUT error"), "\n", NULL), session_server)
		break;
	case MAILIMAP_ERROR_CAPABILITY:
		MY_LOG_WARNING(g_strconcat(_("IMAP error on %s:"), " ", _("CAPABILITY error"), "\n", NULL), session_server)
		break;
	case MAILIMAP_ERROR_CHECK:
		MY_LOG_WARNING(g_strconcat(_("IMAP error on %s:"), " ", _("CHECK error"), "\n", NULL), session_server)
		break;
	case MAILIMAP_ERROR_CLOSE:
		MY_LOG_WARNING(g_strconcat(_("IMAP error on %s:"), " ", _("CLOSE error"), "\n", NULL), session_server)
		break;
	case MAILIMAP_ERROR_EXPUNGE:
		MY_LOG_WARNING(g_strconcat(_("IMAP error on %s:"), " ", _("EXPUNGE error"), "\n", NULL), session_server)
		break;
	case MAILIMAP_ERROR_COPY:
		MY_LOG_WARNING(g_strconcat(_("IMAP error on %s:"), " ", _("COPY error"), "\n", NULL), session_server)
		break;
	case MAILIMAP_ERROR_UID_COPY:
		MY_LOG_WARNING(g_strconcat(_("IMAP error on %s:"), " ", _("UID COPY error"), "\n", NULL), session_server)
		break;
	case MAILIMAP_ERROR_CREATE:
		MY_LOG_WARNING(g_strconcat(_("IMAP error on %s:"), " ", _("CREATE error"), "\n", NULL), session_server)
		break;
	case MAILIMAP_ERROR_DELETE:
		MY_LOG_WARNING(g_strconcat(_("IMAP error on %s:"), " ", _("DELETE error"), "\n", NULL), session_server)
		break;
	case MAILIMAP_ERROR_EXAMINE:
		MY_LOG_WARNING(g_strconcat(_("IMAP error on %s:"), " ", _("EXAMINE error"), "\n", NULL), session_server)
		break;
	case MAILIMAP_ERROR_FETCH:
		MY_LOG_WARNING(g_strconcat(_("IMAP error on %s:"), " ", _("FETCH error"), "\n", NULL), session_server)
		break;
	case MAILIMAP_ERROR_UID_FETCH:
		MY_LOG_WARNING(g_strconcat(_("IMAP error on %s:"), " ", _("UID FETCH error"), "\n", NULL), session_server)
		break;
	case MAILIMAP_ERROR_LIST:
		MY_LOG_WARNING(g_strconcat(_("IMAP error on %s:"), " ", _("LIST error"), "\n", NULL), session_server)
		break;
	case MAILIMAP_ERROR_LOGIN:
		MY_LOG_WARNING(g_strconcat(_("IMAP error on %s:"), " ", _("LOGIN error"), "\n", NULL), session_server)
		break;
	case MAILIMAP_ERROR_LSUB:
		MY_LOG_WARNING(g_strconcat(_("IMAP error on %s:"), " ", _("LSUB error"), "\n", NULL), session_server)
		break;
	case MAILIMAP_ERROR_RENAME:
		MY_LOG_WARNING(g_strconcat(_("IMAP error on %s:"), " ", _("RENAME error"), "\n", NULL), session_server)
		break;
	case MAILIMAP_ERROR_SEARCH:
		MY_LOG_WARNING(g_strconcat(_("IMAP error on %s:"), " ", _("SEARCH error"), "\n", NULL), session_server)
		break;
	case MAILIMAP_ERROR_UID_SEARCH:
		MY_LOG_WARNING(g_strconcat(_("IMAP error on %s:"), " ", _("UID SEARCH error"), "\n", NULL), session_server)
		break;
	case MAILIMAP_ERROR_SELECT:
		MY_LOG_WARNING(g_strconcat(_("IMAP error on %s:"), " ", _("SELECT error"), "\n", NULL), session_server)
		break;
	case MAILIMAP_ERROR_STATUS:
		MY_LOG_WARNING(g_strconcat(_("IMAP error on %s:"), " ", _("STATUS error"), "\n", NULL), session_server)
		break;
	case MAILIMAP_ERROR_STORE:
		MY_LOG_WARNING(g_strconcat(_("IMAP error on %s:"), " ", _("STORE error"), "\n", NULL), session_server)
		break;
	case MAILIMAP_ERROR_UID_STORE:
		MY_LOG_WARNING(g_strconcat(_("IMAP error on %s:"), " ", _("UID STORE error"), "\n", NULL), session_server)
		break;
	case MAILIMAP_ERROR_SUBSCRIBE:
		MY_LOG_WARNING(g_strconcat(_("IMAP error on %s:"), " ", _("SUBSCRIBE error"), "\n", NULL), session_server)
		break;
	case MAILIMAP_ERROR_UNSUBSCRIBE:
		MY_LOG_WARNING(g_strconcat(_("IMAP error on %s:"), " ", _("UNSUBSCRIBE error"), "\n", NULL), session_server)
		break;
	case MAILIMAP_ERROR_STARTTLS:
		MY_LOG_WARNING(g_strconcat(_("IMAP error on %s:"), " ", _("STARTTLS error"), "\n", NULL), session_server)
		break;
	case MAILIMAP_ERROR_INVAL:
		MY_LOG_WARNING(g_strconcat(_("IMAP error on %s:"), " ", _("INVAL error"), "\n", NULL), session_server)
		break;
	case MAILIMAP_ERROR_EXTENSION:
		MY_LOG_WARNING(g_strconcat(_("IMAP error on %s:"), " ", _("EXTENSION error"), "\n", NULL), session_server)
		break;
	case MAILIMAP_ERROR_SASL:
		MY_LOG_WARNING(g_strconcat(_("IMAP error on %s:"), " ", _("SASL error"), "\n", NULL), session_server)
		break;
#ifdef USE_GNUTLS
	case MAILIMAP_ERROR_SSL:
		MY_LOG_WARNING(g_strconcat(_("IMAP error on %s:"), " ", _("TLS error"), "\n", NULL), session_server)
		break;
#endif
	default:
		MY_LOG_WARNING(g_strconcat(_("IMAP error on %s:"), " ", _("Unknown error [%d]"), "\n", NULL),
			session_server, libetpan_errcode)
		break;
	}

	if (session && is_fatal(libetpan_errcode)) {
		imap_disc_session_destroy(IMAP_SESSION(session));
	} else if (session && !is_fatal(libetpan_errcode)) {
		if (IMAP_SESSION(session)->busy)
			unlock_session(IMAP_SESSION(session));
	}
}

#undef MY_LOG_WARNING

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
	while (imap_folder_get_refcnt(folder) > 0)
		gtk_main_iteration();

	g_free(IMAP_FOLDER(folder)->search_charset);

	folder_remote_folder_destroy(REMOTE_FOLDER(folder));
	imap_done(folder);
}

static void imap_folder_init(Folder *folder, const gchar *name,
			     const gchar *path)
{
	folder_remote_folder_init((Folder *)folder, name, path);
	IMAP_FOLDER(folder)->search_charset_supported = TRUE;
	IMAP_FOLDER(folder)->search_charset = g_strdup(conv_get_locale_charset_str_no_utf8());
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

static int imap_get_capabilities(IMAPSession *session)
{
	struct mailimap_capability_data *capabilities = NULL;
	clistiter *cur;
	int result;

	if (session->capability != NULL)
		return MAILIMAP_NO_ERROR;

	result = imap_threaded_capability(session->folder, &capabilities);

	if (result != MAILIMAP_NO_ERROR) {
		return result;
	}

	if (capabilities == NULL || capabilities->cap_list == NULL) {
		return MAILIMAP_NO_ERROR;
	}

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
	return MAILIMAP_NO_ERROR;
}

static gboolean imap_has_capability(IMAPSession *session, const gchar *cap) 
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
	gint ok = MAILIMAP_ERROR_LOGIN;
	static time_t last_login_err = 0;
	gchar *ext_info = "";
	int r;
	gchar *server = NULL;
	if ((r = imap_get_capabilities(session)) != MAILIMAP_NO_ERROR) {
		imap_handle_error(SESSION(session), NULL, r);
		return r;
	}
	server = g_strdup(SESSION(session)->server);
	switch(type) {
	case IMAP_AUTH_ANON:
		ok = imap_cmd_login(session, user, pass, "ANONYMOUS");
		break;
	case IMAP_AUTH_CRAM_MD5:
		ok = imap_cmd_login(session, user, pass, "CRAM-MD5");
		break;
	case IMAP_AUTH_DIGEST_MD5:
		ok = imap_cmd_login(session, user, pass, "DIGEST-MD5");
		break;
	case IMAP_AUTH_SCRAM_SHA1:
		ok = imap_cmd_login(session, user, pass, "SCRAM-SHA-1");
		break;
	case IMAP_AUTH_SCRAM_SHA224:
		ok = imap_cmd_login(session, user, pass, "SCRAM-SHA-224");
		break;
	case IMAP_AUTH_SCRAM_SHA256:
		ok = imap_cmd_login(session, user, pass, "SCRAM-SHA-256");
		break;
	case IMAP_AUTH_SCRAM_SHA384:
		ok = imap_cmd_login(session, user, pass, "SCRAM-SHA-384");
		break;
	case IMAP_AUTH_SCRAM_SHA512:
		ok = imap_cmd_login(session, user, pass, "SCRAM-SHA-512");
		break;
	case IMAP_AUTH_PLAIN:
		ok = imap_cmd_login(session, user, pass, "PLAIN");
		break;
#ifdef USE_GNUTLS
	case IMAP_AUTH_OAUTH2:
		ok = imap_cmd_login(session, user, pass, "XOAUTH2");
		break;
#endif
	case IMAP_AUTH_LOGIN:
		ok = imap_cmd_login(session, user, pass, "LOGIN");
		break;
	case IMAP_AUTH_PLAINTEXT:
		ok = imap_cmd_login(session, user, pass, "plaintext");
		break;
	case IMAP_AUTH_GSSAPI:
		ok = imap_cmd_login(session, user, pass, "GSSAPI");
		break;
	default:
		debug_print("capabilities:\n"
				"\t ANONYMOUS %d\n"
				"\t CRAM-MD5 %d\n"
				"\t DIGEST-MD5 %d\n"
				"\t SCRAM-SHA-1 %d\n"
				"\t SCRAM-SHA-224 %d\n"
				"\t SCRAM-SHA-256 %d\n"
				"\t SCRAM-SHA-384 %d\n"
				"\t SCRAM-SHA-512 %d\n"
				"\t PLAIN %d\n"
#ifdef USE_GNUTLS
				"\t OAUTH2 %d\n"
#endif
				"\t LOGIN %d\n"
				"\t GSSAPI %d\n", 
			imap_has_capability(session, "ANONYMOUS"),
			imap_has_capability(session, "CRAM-MD5"),
			imap_has_capability(session, "DIGEST-MD5"),
			imap_has_capability(session, "SCRAM-SHA-1"),
			imap_has_capability(session, "SCRAM-SHA-224"),
			imap_has_capability(session, "SCRAM-SHA-256"),
			imap_has_capability(session, "SCRAM-SHA-384"),
			imap_has_capability(session, "SCRAM-SHA-512"),
			imap_has_capability(session, "PLAIN"),
#ifdef USE_GNUTLS
			imap_has_capability(session, "XOAUTH2"),
#endif
			imap_has_capability(session, "LOGIN"),
			imap_has_capability(session, "GSSAPI"));
		if (imap_has_capability(session, "CRAM-MD5"))
			ok = imap_cmd_login(session, user, pass, "CRAM-MD5");
		if (ok == MAILIMAP_ERROR_LOGIN && imap_has_capability(session, "DIGEST-MD5"))
			ok = imap_cmd_login(session, user, pass, "DIGEST-MD5");
		if (ok == MAILIMAP_ERROR_LOGIN && imap_has_capability(session, "SCRAM-SHA-512"))
			ok = imap_cmd_login(session, user, pass, "SCRAM-SHA-512");
		if (ok == MAILIMAP_ERROR_LOGIN && imap_has_capability(session, "SCRAM-SHA-384"))
			ok = imap_cmd_login(session, user, pass, "SCRAM-SHA-384");
		if (ok == MAILIMAP_ERROR_LOGIN && imap_has_capability(session, "SCRAM-SHA-256"))
			ok = imap_cmd_login(session, user, pass, "SCRAM-SHA-256");
		if (ok == MAILIMAP_ERROR_LOGIN && imap_has_capability(session, "SCRAM-SHA-224"))
			ok = imap_cmd_login(session, user, pass, "SCRAM-SHA-224");
		if (ok == MAILIMAP_ERROR_LOGIN && imap_has_capability(session, "SCRAM-SHA-1"))
			ok = imap_cmd_login(session, user, pass, "SCRAM-SHA-1");
		if (ok == MAILIMAP_ERROR_LOGIN && imap_has_capability(session, "PLAIN"))
			ok = imap_cmd_login(session, user, pass, "PLAIN");
		if (ok == MAILIMAP_ERROR_LOGIN && imap_has_capability(session, "LOGIN"))
			ok = imap_cmd_login(session, user, pass, "LOGIN");
		if (ok == MAILIMAP_ERROR_LOGIN && imap_has_capability(session, "GSSAPI"))
			ok = imap_cmd_login(session, user, pass, "GSSAPI");
		if (ok == MAILIMAP_ERROR_LOGIN) /* we always try plaintext login before giving up */
			ok = imap_cmd_login(session, user, pass, "plaintext");
#ifdef USE_GNUTLS
		if (ok == MAILIMAP_ERROR_LOGIN && imap_has_capability(session, "XOAUTH2"))
			ok = imap_cmd_login(session, user, pass, "XOAUTH2");
#endif
	}

	if (ok == MAILIMAP_NO_ERROR)
		session->authenticated = TRUE;
	else {
		if (type == IMAP_AUTH_CRAM_MD5) {
			ext_info = _("\n\nCRAM-MD5 logins only work if libetpan has been "
				     "compiled with SASL support and the "
				     "CRAM-MD5 SASL plugin is installed.");
		} 

		if (type == IMAP_AUTH_DIGEST_MD5) {
			ext_info = _("\n\nDIGEST-MD5 logins only work if libetpan has been "
				     "compiled with SASL support and the "
				     "DIGEST-MD5 SASL plugin is installed.");
		} 

		if (type == IMAP_AUTH_SCRAM_SHA1) {
			ext_info = _("\n\nSCRAM-SHA-1 logins only work if libetpan has been "
				     "compiled with SASL support and the "
				     "SCRAM SASL plugin is installed.");
		}

		if (type == IMAP_AUTH_SCRAM_SHA224) {
			ext_info = _("\n\nSCRAM-SHA-224 logins only work if libetpan has been "
				     "compiled with SASL support and the "
				     "SCRAM SASL plugin is installed.");
		}

		if (type == IMAP_AUTH_SCRAM_SHA256) {
			ext_info = _("\n\nSCRAM-SHA-256 logins only work if libetpan has been "
				     "compiled with SASL support and the "
				     "SCRAM SASL plugin is installed.");
		}

		if (type == IMAP_AUTH_SCRAM_SHA384) {
			ext_info = _("\n\nSCRAM-SHA-384 logins only work if libetpan has been "
				     "compiled with SASL support and the "
				     "SCRAM SASL plugin is installed.");
		}

		if (type == IMAP_AUTH_SCRAM_SHA512) {
			ext_info = _("\n\nSCRAM-SHA-512 logins only work if libetpan has been "
				     "compiled with SASL support and the "
				     "SCRAM SASL plugin is installed.");
		}

		if (type == IMAP_AUTH_PLAIN) {
			ext_info = _("\n\nPLAIN logins only work if libetpan has been "
				     "compiled with SASL support and the "
				     "PLAIN SASL plugin is installed.");
		}

		if (type == IMAP_AUTH_LOGIN) {
			ext_info = _("\n\nLOGIN logins only work if libetpan has been "
				     "compiled with SASL support and the "
				     "LOGIN SASL plugin is installed.");
		}
#ifdef USE_GNUTLS
		if (type == IMAP_AUTH_OAUTH2) {
			ext_info = _("\n\nOAuth2 error. Check and correct your OAuth2 "
				     "account preferences.");
		} 
#endif
		if (time(NULL) - last_login_err > 10) {
			if (!prefs_common.no_recv_err_panel) {
				alertpanel_error_log(_("Connection to %s failed: "
					"login refused.%s"),
					server, ext_info);
			} else {
				log_error(LOG_PROTOCOL, _("Connection to %s failed: "
					"login refused.%s\n"),
					server, ext_info);
			}
		}
		last_login_err = time(NULL);
	}
	g_free(server);
	return ok;
}

static IMAPSession *imap_reconnect_if_possible(Folder *folder, IMAPSession *session)
{
	RemoteFolder *rfolder = REMOTE_FOLDER(folder);
	/* Check if this is the first try to establish a
	   connection, if yes we don't try to reconnect */
	debug_print("reconnecting\n");
	if (rfolder->session == NULL) {
		log_warning(LOG_PROTOCOL, _("Connecting to %s failed"),
			    folder->account->recv_server);
		SESSION(session)->sock = NULL;
		imap_safe_destroy(session);
		session = NULL;
	} else {
		rfolder->session = NULL;
		log_warning(LOG_PROTOCOL, _("IMAP connection to %s has been"
			    " disconnected. Reconnecting...\n"),
			    folder->account->recv_server);
		statusbar_print_all(_("IMAP connection to %s has been"
			    " disconnected. Reconnecting...\n"),
			    folder->account->recv_server);
		SESSION(session)->state = SESSION_DISCONNECTED;
		SESSION(session)->sock = NULL;
		imap_safe_destroy(session);
		/* Clear folders session to make imap_session_get create
		   a new session, because of rfolder->session == NULL
		   it will not try to reconnect again and so avoid an
		   endless loop */
		debug_print("getting session...\n");
		session = imap_session_get(folder);
		rfolder->session = SESSION(session);
		statusbar_pop_all();
	}
	return session;
}

static IMAPSession *imap_session_get(Folder *folder)
{
	RemoteFolder *rfolder = REMOTE_FOLDER(folder);
	IMAPSession *session = NULL;
	gint r = MAILIMAP_NO_ERROR;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(FOLDER_CLASS(folder) == &imap_class, NULL);
	g_return_val_if_fail(folder->account != NULL, NULL);
	
	if (prefs_common.work_offline && 
	    !inc_offline_should_override(FALSE,
		_("Claws Mail needs network access in order "
		  "to access the IMAP server."))) {
		return NULL;
	}

	/* check for deferred destroy */
	if (rfolder->session != NULL) {
		session = IMAP_SESSION(rfolder->session);
		if (!session->busy && session->do_destroy) {
			rfolder->session = NULL;
			imap_safe_destroy(session);
			session = NULL;
		}
	}

	/* Make sure we have a session */
	if (rfolder->session != NULL && rfolder->session->state != SESSION_DISCONNECTED) {
		session = IMAP_SESSION(rfolder->session);
	} else if (rfolder->session != NULL && rfolder->session->state == SESSION_DISCONNECTED) {
		imap_safe_destroy(IMAP_SESSION(rfolder->session));
		rfolder->session = NULL;
		goto new_conn;
	} else if (rfolder->connecting) {
		debug_print("already connecting\n");
		return NULL;
	} else {
new_conn:
		imap_reset_uid_lists(folder);
		if (time(NULL) - rfolder->last_failure <= 2)
			return NULL;
		rfolder->connecting = TRUE;
		session = imap_session_new(folder, folder->account);
	}
	if(session == NULL) {
		rfolder->last_failure = time(NULL);
		rfolder->connecting = FALSE;
		return NULL;
	}

	/* Make sure session is authenticated */
	if (!IMAP_SESSION(session)->authenticated)
		r = imap_session_authenticate(IMAP_SESSION(session), folder->account);
	
	if (r != MAILIMAP_NO_ERROR || (!is_fatal(r) && !IMAP_SESSION(session)->authenticated)) {
		rfolder->session = NULL;
		if (!is_fatal(r)) {
			imap_threaded_disconnect(session->folder);
		}
		SESSION(session)->state = SESSION_DISCONNECTED;
		SESSION(session)->sock = NULL;
		imap_safe_destroy(session);
		rfolder->last_failure = time(NULL);
		rfolder->connecting = FALSE;
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
	if ((time(NULL) - SESSION(session)->last_access_time > SESSION_TIMEOUT_INTERVAL) || session->cancelled) {
		/* verify that the session is still alive */
		r = imap_cmd_noop(session);

		if (r != MAILIMAP_NO_ERROR) {
			debug_print("disconnected!\n");
			if (!is_fatal(r))
				session = imap_reconnect_if_possible(folder, session);
			else {
				rfolder->session = NULL;
				rfolder->connecting = FALSE;
				SESSION(session)->state = SESSION_DISCONNECTED;
				SESSION(session)->sock = NULL;
				imap_safe_destroy(session);
				session = imap_session_get(folder);
			}
		}
		if (session)
			session->cancelled = FALSE;
	}

	rfolder->session = SESSION(session);
	rfolder->connecting = FALSE;

	return IMAP_SESSION(session);
}

static IMAPSession *imap_session_new(Folder * folder,
				     const PrefsAccount *account)
{
	IMAPSession *session;
	ProxyInfo *proxy_info = NULL;
	gushort port;
	int r;
	int authenticated = FALSE;
	gchar *buf;

#ifdef USE_GNUTLS
	/* FIXME: IMAP over SSL only... */ 
	SSLType ssl_type;

	port = account->set_imapport ? account->imapport
		: account->ssl_imap == SSL_TUNNEL ? IMAPS_PORT : IMAP4_PORT;
	ssl_type = account->ssl_imap;	
#else
	if (account->ssl_imap != SSL_NONE) {
		if (alertpanel_full(_("Insecure connection"),
			_("This connection is configured to be secured "
			  "using TLS, but TLS is not available "
			  "in this build of Claws Mail. \n\n"
			  "Do you want to continue connecting to this "
			  "server? The communication would not be "
			  "secure."),
			  NULL, _("_Cancel"), NULL, _("Con_tinue connecting"), NULL, NULL,
			  ALERTFOCUS_FIRST, FALSE, NULL, ALERT_WARNING) != G_ALERTALTERNATE)
			return NULL;
	}
	port = account->set_imapport ? account->imapport
		: IMAP4_PORT;
#endif

	imap_init(folder);
	IMAP_FOLDER(folder)->max_set_size = account->imap_batch_size;
	buf = g_strdup_printf(_("Account '%s': Connecting to IMAP server: %s:%d..."),
				folder->account->account_name, folder->account->recv_server,
				port);
	statusbar_print_all("%s", buf);
	log_message(LOG_PROTOCOL, "%s\n", buf);
	g_free(buf);

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

#ifndef G_OS_WIN32
	if (account->set_tunnelcmd) {
		r = imap_threaded_connect_cmd(folder,
					      account->tunnelcmd,
					      account->recv_server,
					      port);
	}
	else 
#endif
	{
#ifdef USE_GNUTLS

		if (ssl_type == SSL_TUNNEL) {
			r = imap_threaded_connect_ssl(folder,
						      account->recv_server,
						      port,
						      proxy_info);
		}
		else 
#endif
		{
			r = imap_threaded_connect(folder,
						  account->recv_server,
						  port,
						  proxy_info);
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
#ifdef USE_GNUTLS
		if (r == MAILIMAP_ERROR_SSL)
			log_error(LOG_PROTOCOL, _("TLS handshake failed\n"));
		else
#endif
			imap_handle_error(NULL, account->recv_server, r);

		if(!prefs_common.no_recv_err_panel) {
			alertpanel_error_log(_("Can't connect to IMAP server: %s:%d"),
					 account->recv_server, port);
		} else {
			log_error(LOG_PROTOCOL, _("Can't connect to IMAP server: %s:%d\n"),
					 account->recv_server, port);
		} 
		
		return NULL;
	}
	
	session = g_new0(IMAPSession, 1);
	session_init(SESSION(session), account, FALSE);
	SESSION(session)->type             = SESSION_IMAP;
	SESSION(session)->server           = g_strdup(account->recv_server);
	SESSION(session)->port             = port;
 	SESSION(session)->sock             = NULL;
	SESSION(session)->proxy_info       = proxy_info;
	SESSION(session)->destroy          = imap_session_destroy;

	session->capability = NULL;
	session->authenticated = authenticated;
	session->mbox = NULL;
	session->exists = 0;
	session->recent = 0;
	session->expunge = 0;
	session->cmd_count = 0;
	session->folder = folder;
	IMAP_FOLDER(session->folder)->last_seen_separator = 0;

#ifdef USE_GNUTLS
	if (account->ssl_imap == SSL_STARTTLS) {
		gint ok;

		ok = imap_cmd_starttls(session);
		if (ok != MAILIMAP_NO_ERROR) {
			log_warning(LOG_PROTOCOL, _("Can't start STARTTLS session.\n"));
			if (!is_fatal(ok)) {
				SESSION(session)->sock = NULL;
				imap_safe_destroy(session);
			}
			return NULL;
		}

		imap_free_capabilities(session);
		session->authenticated = FALSE;
		session->uidplus = FALSE;
		session->cmd_count = 1;
	}
	SESSION(session)->use_tls_sni = account->use_tls_sni;
#endif

	log_message(LOG_PROTOCOL, "IMAP connection is %s-authenticated\n",
		    (session->authenticated) ? "pre" : "un");
	
	session_register_ping(SESSION(session), imap_ping);

	return session;
}

static gint imap_session_authenticate(IMAPSession *session, 
				      PrefsAccount *account)
{
	gchar *pass, *acc_pass = NULL;
	gboolean failed = FALSE;
	gint ok = MAILIMAP_NO_ERROR;
	g_return_val_if_fail(account->userid != NULL, MAILIMAP_ERROR_BAD_STATE);
#ifdef USE_OAUTH2
	if(account->imap_auth_type == IMAP_AUTH_OAUTH2)
	        oauth2_check_passwds (account);
#endif
	if (!password_get(account->userid, account->recv_server, "imap",
			 SESSION(session)->port, &acc_pass)) {
		acc_pass = passwd_store_get_account(account->account_id,
				PWS_ACCOUNT_RECV);
	}

try_again:
	pass = acc_pass;
	if (!pass && account->imap_auth_type != IMAP_AUTH_ANON && account->imap_auth_type != IMAP_AUTH_GSSAPI) {
		gchar *tmp_pass;
		tmp_pass = input_dialog_query_password_keep(account->recv_server, 
							    account->userid,
							    &(account->session_passwd));
		if (!tmp_pass) {
			return MAILIMAP_NO_ERROR;
		}
		Xstrdup_a(pass, tmp_pass, {g_free(tmp_pass); return MAILIMAP_NO_ERROR;});
		g_free(tmp_pass);
	} else if (account->imap_auth_type == IMAP_AUTH_ANON || account->imap_auth_type == IMAP_AUTH_GSSAPI) {
		pass = "";
	}
	if ((ok = imap_auth(session, account->userid, pass, account->imap_auth_type)) != MAILIMAP_NO_ERROR) {
		
		if (!failed && !is_fatal(ok)) {
			if (acc_pass != NULL) {
				memset(acc_pass, 0, strlen(acc_pass));
				g_free(acc_pass);
				acc_pass = NULL;
			}
			failed = TRUE;
			if (account->session_passwd != NULL) {
				g_free(account->session_passwd);
				account->session_passwd = NULL;
			}
			goto try_again;
		} else {
			if (prefs_common.no_recv_err_panel) {
				log_error(LOG_PROTOCOL, _("Couldn't login to IMAP server %s.\n"), account->recv_server);
				mainwindow_show_error();
			} else
				alertpanel_error_log(_("Couldn't login to IMAP server %s."), account->recv_server);
		}

		if (acc_pass != NULL) {
			memset(acc_pass, 0, strlen(acc_pass));
			g_free(acc_pass);
			acc_pass = NULL;
		}

		return ok;
	} 

	if (acc_pass) {
		memset(acc_pass, 0, strlen(acc_pass));
		g_free(acc_pass);
	}
	statusbar_pop_all();
	session->authenticated = TRUE;
	return MAILIMAP_NO_ERROR;
}

static void imap_session_destroy(Session *session)
{
	if (session->state != SESSION_DISCONNECTED)
		imap_threaded_disconnect(IMAP_SESSION(session)->folder);
	
	imap_free_capabilities(IMAP_SESSION(session));
	g_free(IMAP_SESSION(session)->mbox);
}

static gchar *imap_fetch_msg(Folder *folder, FolderItem *item, gint uid)
{
	return imap_fetch_msg_full(folder, item, uid, TRUE, TRUE);
}

static guint get_file_size_with_crs(const gchar *filename) 
{
	FILE *fp = NULL;
	guint cnt = 0;
	gchar buf[4096];
	
	if (filename == NULL)
		return -1;
	
	fp = claws_fopen(filename, "rb");
	if (!fp)
		return -1;
	
	while (claws_fgets(buf, sizeof (buf), fp) != NULL) {
		cnt += strlen(buf);
		if (!strstr(buf, "\r\n") && strstr(buf, "\n"))
			cnt++;
	}
	
	claws_fclose(fp);
	return cnt;
}

static gchar *imap_get_cached_filename(FolderItem *item, guint msgnum)
{
	gchar *path, *filename;

	cm_return_val_if_fail(item != NULL, NULL);

	path = folder_item_get_path(item);

	if (!is_dir_exist(path)) {
		g_free(path);
		return NULL;
	}

	filename = g_strconcat(path, G_DIR_SEPARATOR_S, itos(msgnum), NULL);

	if (is_dir_exist(filename)) {
		g_free(filename);
		filename = g_strconcat(path, G_DIR_SEPARATOR_S, ".", itos(msgnum), NULL);
	}
	g_free(path);

	return filename;
}

static void imap_remove_cached_msg(Folder *folder, FolderItem *item, MsgInfo *msginfo)
{
	gchar *filename;

	filename = imap_get_cached_filename(item, msginfo->msgnum);

	cm_return_if_fail(filename != NULL);

	if (is_file_exist(filename)) {
		claws_unlink(filename);
	}
	g_free(filename);
}

typedef struct _TagsData {
	gchar *str;
	GSList *msglist;
	IMAPFolderItem *item;
} TagsData;

static void imap_commit_tags(FolderItem *item, MsgInfo *msginfo, GSList *tags_set, GSList *tags_unset)
{
	IMAPSession *session;
	gint ok, can_create_tags;
	Folder *folder = NULL;
	TagsData *ht_data = NULL;
	GSList *cur;

	g_return_if_fail(item != NULL);
	g_return_if_fail(msginfo != NULL);

	folder = item->folder;
	debug_print("getting session...\n");
	session = imap_session_get(folder);
	
	if (!session) {
		debug_print("can't get session\n");
		return;
	}

	ok = imap_select(session, IMAP_FOLDER(folder), item,
			 NULL, NULL, NULL, NULL, &can_create_tags, FALSE);

	if (ok != MAILIMAP_NO_ERROR) {
		return;
	}

	
	if (IMAP_FOLDER_ITEM(item)->can_create_flags != ITEM_CAN_CREATE_FLAGS)
		return;
	
	if (IMAP_FOLDER_ITEM(item)->batching) {
		/* instead of performing an UID STORE command for each message change,
		 * as a lot of them can change "together", we just fill in hashtables
		 * and defer the treatment so that we're able to send only one
		 * command.
		 */
		debug_print("IMAP batch mode on, deferring tags change\n");
		for (cur = tags_set; cur; cur = cur->next) {
			gint cur_tag = GPOINTER_TO_INT(cur->data);
			if (cur_tag) {
				ht_data = g_hash_table_lookup(IMAP_FOLDER_ITEM(item)->tags_set_table, 
					GINT_TO_POINTER(cur_tag));
				if (ht_data == NULL) {
					ht_data = g_new0(TagsData, 1);
					ht_data->str = g_strdup(tags_get_tag(cur_tag));
					ht_data->item = IMAP_FOLDER_ITEM(item);
					g_hash_table_insert(IMAP_FOLDER_ITEM(item)->tags_set_table, 
						GINT_TO_POINTER(cur_tag), ht_data);
				}
				ht_data->msglist = g_slist_prepend(ht_data->msglist, GINT_TO_POINTER(msginfo->msgnum));
			} 
		}
		for (cur = tags_unset; cur; cur = cur->next) {
			gint cur_tag = GPOINTER_TO_INT(cur->data);
			if (cur_tag) {
				ht_data = g_hash_table_lookup(IMAP_FOLDER_ITEM(item)->tags_unset_table, 
					GINT_TO_POINTER(cur_tag));
				if (ht_data == NULL) {
					ht_data = g_new0(TagsData, 1);
					ht_data->str = g_strdup(tags_get_tag(cur_tag));
					ht_data->item = IMAP_FOLDER_ITEM(item);
					g_hash_table_insert(IMAP_FOLDER_ITEM(item)->tags_unset_table, 
						GINT_TO_POINTER(cur_tag), ht_data);
				}
				ht_data->msglist = g_slist_prepend(ht_data->msglist, GINT_TO_POINTER(msginfo->msgnum));
			}
		}
	} else {
		GSList *list_set = NULL;
		GSList *list_unset = NULL;
		GSList numlist;
		
		numlist.data = GINT_TO_POINTER(msginfo->msgnum);
		numlist.next = NULL;
	
		debug_print("IMAP changing tags NOW\n");
		for (cur = tags_set; cur; cur = cur->next) {
			gint cur_tag = GPOINTER_TO_INT(cur->data);
			const gchar *str = tags_get_tag(cur_tag);
			if (IS_NOT_RESERVED_TAG(str))
				list_set = g_slist_prepend(list_set, g_strdup(str));
		}
		if (list_set) {
			ok = imap_set_message_flags(session, 
				IMAP_FOLDER_ITEM(item), &numlist, 0, list_set, TRUE);
			slist_free_strings_full(list_set);
			if (ok != MAILIMAP_NO_ERROR) {
				return;
			}
		}

		for (cur = tags_unset; cur; cur = cur->next) {
			gint cur_tag = GPOINTER_TO_INT(cur->data);
			const gchar *str = tags_get_tag(cur_tag);
			if (IS_NOT_RESERVED_TAG(str))
				list_unset = g_slist_prepend(list_unset, g_strdup(str));
		}
		if (list_unset) {
			ok = imap_set_message_flags(session, 
				IMAP_FOLDER_ITEM(item), &numlist, 0, list_unset, FALSE);
			slist_free_strings_full(list_unset);
			if (ok != MAILIMAP_NO_ERROR) {
				return;
			}
		}
	}
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
	if (!is_dir_exist(path)) {
		if(is_file_exist(path))
			claws_unlink(path);
		make_dir_hier(path);
	}
	g_free(path);

	filename = imap_get_cached_filename(item, uid);
	debug_print("trying to fetch cached %s\n", filename);

	if (is_file_exist(filename)) {
		/* see whether the local file represents the whole message
		 * or not. As the IMAP server reports size with \r chars,
		 * we have to update the local file (UNIX \n only) size */
		MsgInfo *cached = msgcache_get_msg(item->cache,uid);
		guint have_size = -1;

		if (cached)
			debug_print("message %d has been already %scached.\n", uid,
				MSG_IS_FULLY_CACHED(cached->flags) ? "fully ":"");
		
		if (!cached || !MSG_IS_FULLY_CACHED(cached->flags)) {
			have_size = get_file_size_with_crs(filename);
			if (cached && (cached->size <= have_size || !body)) {
				ok = file_strip_crs(filename);
				if (ok == 0 && cached && cached->size <= have_size) {
					/* we have it all and stripped */
					debug_print("...fully cached in fact (%u/%"G_GOFFSET_FORMAT"); setting flag.\n",
							have_size, cached->size);
					procmsg_msginfo_set_flags(cached, MSG_FULLY_CACHED, 0);
				}
				procmsg_msginfo_free(&cached);
				return filename;
			} else if (!cached && time(NULL) - get_file_mtime(filename) < 60) {
				debug_print("message not cached and file recent, considering file complete\n");
				ok = file_strip_crs(filename);
				if (ok == 0)
					return filename;
			} else {
				procmsg_msginfo_free(&cached);
			}
		}
		if (cached && MSG_IS_FULLY_CACHED(cached->flags)) {
			procmsg_msginfo_free(&cached);
			return filename;
		}
	} else {
		MsgInfo *cached = msgcache_get_msg(item->cache,uid);
		if (cached) {
			procmsg_msginfo_unset_flags(cached, MSG_FULLY_CACHED, 0);
			procmsg_msginfo_free(&cached);
		}
	}

	debug_print("getting session...\n");
	session = imap_session_get(folder);
	
	if (!session) {
		g_free(filename);
		return NULL;
	}
	session_set_access_time(SESSION(session));
	lock_session(session); /* unlocked later in the function */

	debug_print("IMAP fetching messages\n");
	ok = imap_select(session, IMAP_FOLDER(folder), item,
			 NULL, NULL, NULL, NULL, NULL, FALSE);
	if (ok != MAILIMAP_NO_ERROR) {
		g_warning("can't select mailbox %s", item->path);
		g_free(filename);
		return NULL;
	}

	session_set_access_time(SESSION(session));

	debug_print("getting message %d...\n", uid);
	ok = imap_cmd_fetch(session, (guint32)uid, filename, headers, body);

	if (ok != MAILIMAP_NO_ERROR) {
		g_warning("can't fetch message %d", uid);
		g_free(filename);
		return NULL;
	}

	session_set_access_time(SESSION(session));
	unlock_session(session);

	ok = file_strip_crs(filename);

	if (ok == 0 && headers && body) {
		MsgInfo *cached = msgcache_get_msg(item->cache,uid);
		if (cached) {
			procmsg_msginfo_set_flags(cached, MSG_FULLY_CACHED, 0);
			procmsg_msginfo_free(&cached);
		}
	} else if (ok == -1) {
		MsgInfo *cached = msgcache_get_msg(item->cache,uid);
		if (cached) {
			procmsg_msginfo_unset_flags(cached, MSG_FULLY_CACHED, 0);
			procmsg_msginfo_free(&cached);
		}
	}
	return filename;
}

static gboolean imap_is_msg_fully_cached(Folder *folder, FolderItem *item, gint uid)
{
	gchar *filename;
	guint size = 0;
	MsgInfo *cached = msgcache_get_msg(item->cache,uid);
	
	if (!cached)
		return FALSE;

	if (MSG_IS_FULLY_CACHED(cached->flags)) {
		procmsg_msginfo_free(&cached);
		return TRUE;
	}

	filename = imap_get_cached_filename(item, uid);

	if (is_file_exist(filename)) {
		if (cached && cached->total_size == cached->size) {
			/* fast path */
			g_free(filename);
			procmsg_msginfo_set_flags(cached, MSG_FULLY_CACHED, 0);
			return TRUE;
		}
		size = get_file_size_with_crs(filename);
	}
	g_free(filename);
	debug_print("msg %d cached, has size %d, full should be %"G_GOFFSET_FORMAT".\n", uid, size, cached->size);
	if (cached && size >= cached->size) {
		cached->total_size = cached->size;
		procmsg_msginfo_set_flags(cached, MSG_FULLY_CACHED, 0);
		procmsg_msginfo_free(&cached);
		return TRUE;
	}
	if (cached)
		procmsg_msginfo_free(&cached);
	return FALSE;	
}

void imap_cache_msg(FolderItem *item, gint msgnum)
{
	Folder *folder = NULL;
	
	if (!item)
		return;
	folder = item->folder;
	
	if (!imap_is_msg_fully_cached(folder, item, msgnum)) {
		gchar *tmp = imap_fetch_msg_full(folder, item, msgnum, TRUE, TRUE);
		debug_print("fetched %s\n", tmp);
		g_free(tmp);
	}
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
		   GHashTable *relation)
{
	gchar *destdir;
	IMAPSession *session;
	guint32 last_uid = 0;
	GSList *cur;
	MsgFileInfo *fileinfo;
	gint ok = MAILIMAP_NO_ERROR;
	gint curnum = 0, total = 0;
	gboolean missing_uids = FALSE;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(file_list != NULL, -1);
	
	debug_print("getting session...\n");
	session = imap_session_get(folder);
	if (!session) {
		return -1;
	}
	destdir = imap_get_real_path(session, IMAP_FOLDER(folder), dest->path, &ok);
	if (is_fatal(ok)) {
		g_free(destdir);
		return -1;
	}
	statusbar_print_all(_("Adding messages..."));
	total = g_slist_length(file_list);
	for (cur = file_list; cur != NULL; cur = cur->next) {
		IMAPFlags iflags = 0;
		guint32 new_uid = 0;
		gchar *real_file = NULL;
		fileinfo = (MsgFileInfo *)cur->data;

		statusbar_progress_all(curnum, total, total < 10 ? 1:10);
		curnum++;

		if (fileinfo->flags) {
			if (MSG_IS_MARKED(*fileinfo->flags))
				iflags |= IMAP_FLAG_FLAGGED;
			if (MSG_IS_REPLIED(*fileinfo->flags))
				iflags |= IMAP_FLAG_ANSWERED;
			if (MSG_IS_FORWARDED(*fileinfo->flags))
				iflags |= IMAP_FLAG_FORWARDED;
			if (MSG_IS_SPAM(*fileinfo->flags))
				iflags |= IMAP_FLAG_SPAM;
			else
				iflags |= IMAP_FLAG_HAM;
			if (!MSG_IS_UNREAD(*fileinfo->flags))
				iflags |= IMAP_FLAG_SEEN;
			
		}
		
		if (real_file == NULL)
			real_file = g_strdup(fileinfo->file);
		
		if (folder_has_parent_of_type(dest, F_QUEUE) ||
		    folder_has_parent_of_type(dest, F_OUTBOX) ||
		    folder_has_parent_of_type(dest, F_DRAFT) ||
		    folder_has_parent_of_type(dest, F_TRASH))
			iflags |= IMAP_FLAG_SEEN;

		ok = imap_cmd_append(session, IMAP_FOLDER_ITEM(dest), destdir, real_file, iflags, 
				     &new_uid);

		if (ok != MAILIMAP_NO_ERROR) {
			g_warning("can't append message %s", real_file);
			g_free(real_file);
			g_free(destdir);
			statusbar_progress_all(0,0,0);
			statusbar_pop_all();
			return -1;
		} else {
			debug_print("appended new message as %d\n", new_uid);
			/* put the local file in the imapcache, so that we don't
			 * have to fetch it back later. */
			
			if (new_uid == 0) {
				missing_uids = TRUE;
				debug_print("Missing UID (0)\n");
			}
			if (new_uid > 0) {
				gchar *cache_path = folder_item_get_path(dest);
				if (!is_dir_exist(cache_path))
					make_dir_hier(cache_path);
				if (is_dir_exist(cache_path)) {
					gchar *cache_file = g_strconcat(
						cache_path, G_DIR_SEPARATOR_S, 
						itos(new_uid), NULL);
					copy_file(real_file, cache_file, TRUE);
					debug_print("got UID %d, copied to cache: %s\n", new_uid, cache_file);
					g_free(cache_file);
				}
				g_free(cache_path);
			}
		}

		if (relation != NULL)
			g_hash_table_insert(relation, fileinfo->msginfo != NULL ? 
					  (gpointer) fileinfo->msginfo : (gpointer) fileinfo,
					  GINT_TO_POINTER(new_uid));
		if (last_uid < new_uid) {
			last_uid = new_uid;
		}

		g_free(real_file);
	}
	
	statusbar_progress_all(0,0,0);
	statusbar_pop_all();
	
	
	g_free(destdir);

	imap_scan_required(folder, dest);

	session = imap_session_get(folder);
	if (!session) {
		return -1;
	}
	if (missing_uids) {
		gint a;
		ok = imap_select(session, IMAP_FOLDER(folder), dest,
			 &a, NULL, NULL, NULL, NULL, FALSE);
	}
	return last_uid;
}

static GSList *flatten_mailimap_set(struct mailimap_set * set) 
{
	GSList *result = NULL;
	clistiter *list;
	guint32 start, end, t;
	GSList *cur;

	if (!set || !set->set_list)
		return NULL;

	for (list = clist_begin(set->set_list); list; list = clist_next(list)) {
		struct mailimap_set_item *item = (struct mailimap_set_item *)clist_content(list);
		start = item->set_first;
		end = item->set_last;
		if (start <= end) {
			for (t = start; t <= end; t++) {
				result = g_slist_prepend(result, GINT_TO_POINTER(t));
			}
		}
	}
	result = g_slist_reverse(result);
	if (debug_get_mode()) {
		debug_print("flat imap set: ");
		for (cur = result; cur; cur = cur->next) {
			debug_print("%d ", GPOINTER_TO_INT(cur->data));
		}
		debug_print("\n");
	}
	
	return result;
}
static gint imap_do_copy_msgs(Folder *folder, FolderItem *dest, 
			      MsgInfoList *msglist, GHashTable *relation,
			      gboolean same_dest_ok)
{
	FolderItem *src;
	gchar *destdir;
	GSList *seq_list, *cur;
	MsgInfo *msginfo;
	IMAPSession *session;
	gint ok = MAILIMAP_NO_ERROR;
	GHashTable *uid_hash;
	gint last_num = 0;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(msglist != NULL, -1);
	
	debug_print("getting session...\n");
	session = imap_session_get(folder);
	
	if (!session) {
		return -1;
	}

	msginfo = (MsgInfo *)msglist->data;
	src = msginfo->folder;
	if (!same_dest_ok && src == dest) {
		g_warning("the src folder is identical to the dest");
		return -1;
	}

	if (src->folder != dest->folder) {
		GSList *infolist = NULL, *cur;
		int res = -1;
		for (cur = msglist; cur; cur = cur->next) {
			msginfo = (MsgInfo *)cur->data;
			MsgFileInfo *fileinfo = g_new0(MsgFileInfo, 1);
			fileinfo->file = procmsg_get_message_file(msginfo);
			fileinfo->flags = &(msginfo->flags);
			infolist = g_slist_prepend(infolist, fileinfo);
		}
		infolist = g_slist_reverse(infolist);
		res = folder_item_add_msgs(dest, infolist, FALSE);
		for (cur = infolist; cur; cur = cur->next) {
			MsgFileInfo *info = (MsgFileInfo *)cur->data;
			g_free(info->file);
			g_free(info);
		}
		g_slist_free(infolist);
		return res;
	} 

	lock_session(session); /* unlocked later in the function */

	ok = imap_select(session, IMAP_FOLDER(folder), msginfo->folder,
			 NULL, NULL, NULL, NULL, NULL, FALSE);
	if (ok != MAILIMAP_NO_ERROR) {
		return ok;
	}

	unlock_session(session);

	destdir = imap_get_real_path(session, IMAP_FOLDER(folder), dest->path, &ok);

	if (is_fatal(ok)) {
		g_free(destdir);
		return ok;
	}

	seq_list = imap_get_lep_set_from_msglist(IMAP_FOLDER(folder), msglist);
	uid_hash = g_hash_table_new(g_direct_hash, g_direct_equal);
	
	statusbar_print_all(_("Copying messages..."));
	for (cur = seq_list; cur != NULL; cur = g_slist_next(cur)) {
		struct mailimap_set * seq_set;
		struct mailimap_set * source = NULL;
		struct mailimap_set * dest = NULL;
		seq_set = cur->data;

		debug_print("Copying messages from %s to %s ...\n",
			    src->path, destdir);

		lock_session(session); /* unlocked later in the function */
		ok = imap_cmd_copy(session, seq_set, destdir,
			&source, &dest);
		
		if (is_fatal(ok)) {
			session = NULL;
		}

		if (ok == MAILIMAP_NO_ERROR) {
			unlock_session(session);
			if (relation && source && dest) {
				GSList *s_list = flatten_mailimap_set(source);
				GSList *d_list = flatten_mailimap_set(dest);
				GSList *s_cur, *d_cur;
				if (g_slist_length(s_list) == g_slist_length(d_list)) {

					for (s_cur = s_list, d_cur = d_list; 
					     s_cur && d_cur; 
					     s_cur = s_cur->next, d_cur = d_cur->next) {
						g_hash_table_insert(uid_hash, s_cur->data, d_cur->data);
					}

				} else {
					debug_print("hhhmm, source list length != dest list length.\n");
				}
				g_slist_free(s_list);
				g_slist_free(d_list);
			}
		}


		if (source)
			mailimap_set_free(source);
		if (dest)
			mailimap_set_free(dest);

		if (ok != MAILIMAP_NO_ERROR) {
			g_hash_table_destroy(uid_hash);
			imap_lep_set_free(seq_list);
			statusbar_pop_all();
			return -1;
		}
	}

	for (cur = msglist; cur != NULL; cur = g_slist_next(cur)) {
		MsgInfo *msginfo = (MsgInfo *)cur->data;
		gpointer hashval;

		hashval = g_hash_table_lookup(uid_hash, GINT_TO_POINTER(msginfo->msgnum));
		
		if (hashval != NULL) {
			gint num = GPOINTER_TO_INT(hashval);
			g_hash_table_insert(relation, msginfo,
					  GINT_TO_POINTER(num));
			if (num > last_num)
				last_num = num;
			debug_print("copied message %d as %d\n", msginfo->msgnum, num);
			/* put the local file in the imapcache, so that we don't
			 * have to fetch it back later. */
			if (num > 0) {
				gchar *cache_path = folder_item_get_path(msginfo->folder);
				gchar *real_file = g_strconcat(
					cache_path, G_DIR_SEPARATOR_S, 
					itos(msginfo->msgnum), NULL);
				gchar *cache_file = NULL;
				g_free(cache_path);
				cache_path = folder_item_get_path(dest);
				cache_file = g_strconcat(
					cache_path, G_DIR_SEPARATOR_S, 
					itos(num), NULL);
				if (!is_dir_exist(cache_path))
					make_dir_hier(cache_path);
				if (is_file_exist(real_file) && is_dir_exist(cache_path)) {
					if (copy_file(real_file, cache_file, TRUE) < 0)
						debug_print("couldn't cache to %s: %s\n", cache_file,
							    strerror(errno));
					else
						debug_print("copied to cache: %s\n", cache_file);
				}
				g_free(real_file);
				g_free(cache_file);
				g_free(cache_path);
			}
		} else
			g_hash_table_insert(relation, msginfo,
					  GINT_TO_POINTER(0));
	}
	statusbar_pop_all();

	g_hash_table_destroy(uid_hash);
	imap_lep_set_free(seq_list);

	g_free(destdir);
	
	IMAP_FOLDER_ITEM(dest)->lastuid = 0;
	IMAP_FOLDER_ITEM(dest)->uid_next = 0;
	g_slist_free(IMAP_FOLDER_ITEM(dest)->uid_list);
	IMAP_FOLDER_ITEM(dest)->uid_list = NULL;

	imap_scan_required(folder, dest);
	if (ok == MAILIMAP_NO_ERROR)
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
		    MsgInfoList *msglist, GHashTable *relation)
{
	MsgInfo *msginfo;
	gint ret;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(msglist != NULL, -1);

	msginfo = (MsgInfo *)msglist->data;
	g_return_val_if_fail(msginfo->folder != NULL, -1);

	ret = imap_do_copy_msgs(folder, dest, msglist, relation, FALSE);
	return ret;
}

static gboolean imap_matcher_type_is_local(gint matchertype)
{
	switch (matchertype) {
	case MATCHCRITERIA_FROM:
	case MATCHCRITERIA_TO:
	case MATCHCRITERIA_CC:
	case MATCHCRITERIA_TO_OR_CC:
	case MATCHCRITERIA_SUBJECT:
	case MATCHCRITERIA_REFERENCES:
	case MATCHCRITERIA_MESSAGEID:
	case MATCHCRITERIA_INREPLYTO:
	case MATCHCRITERIA_AGE_GREATER:
	case MATCHCRITERIA_AGE_LOWER:
	case MATCHCRITERIA_FORWARDED:
	case MATCHCRITERIA_SPAM:
	case MATCHCRITERIA_UNREAD:
	case MATCHCRITERIA_NEW:
	case MATCHCRITERIA_MARKED:
	case MATCHCRITERIA_REPLIED:
	case MATCHCRITERIA_DELETED:
	case MATCHCRITERIA_SIZE_GREATER:
	case MATCHCRITERIA_SIZE_SMALLER:
	case MATCHCRITERIA_SIZE_EQUAL:
		return TRUE;
	}
	return FALSE;
}

static IMAPSearchKey* search_make_key(MatcherProp* match, gboolean* is_all)
{
	if (match->matchtype == MATCHTYPE_MATCHCASE || match->matchtype == MATCHTYPE_MATCH) {
		IMAPSearchKey* result = NULL;
		gboolean invert = FALSE;
		gint matchertype = match->criteria;

		if (is_all) {
			*is_all = FALSE;
		}

		switch (matchertype) {
		case MATCHCRITERIA_NOT_NEW: invert = TRUE; matchertype = MATCHCRITERIA_NEW; break;
		case MATCHCRITERIA_NOT_MARKED: invert = TRUE; matchertype = MATCHCRITERIA_MARKED; break;
		case MATCHCRITERIA_NOT_FORWARDED: invert = TRUE; matchertype = MATCHCRITERIA_FORWARDED; break;
		case MATCHCRITERIA_NOT_SPAM: invert = TRUE; matchertype = MATCHCRITERIA_SPAM; break;
		case MATCHCRITERIA_NOT_SUBJECT: invert = TRUE; matchertype = MATCHCRITERIA_SUBJECT; break;
		case MATCHCRITERIA_NOT_FROM: invert = TRUE; matchertype = MATCHCRITERIA_FROM; break;
		case MATCHCRITERIA_NOT_TO: invert = TRUE; matchertype = MATCHCRITERIA_TO; break;
		case MATCHCRITERIA_NOT_CC: invert = TRUE; matchertype = MATCHCRITERIA_CC; break;
		case MATCHCRITERIA_NOT_REFERENCES: invert = TRUE; matchertype = MATCHCRITERIA_REFERENCES; break;
		case MATCHCRITERIA_NOT_HEADER: invert = TRUE; matchertype = MATCHCRITERIA_HEADER; break;
		case MATCHCRITERIA_NOT_TAG: invert = TRUE; matchertype = MATCHCRITERIA_TAG; break;
		case MATCHCRITERIA_NOT_HEADERS_PART: invert = TRUE; matchertype = MATCHCRITERIA_HEADERS_PART; break;
		case MATCHCRITERIA_NOT_HEADERS_CONT: invert = TRUE; matchertype = MATCHCRITERIA_HEADERS_CONT; break;
		case MATCHCRITERIA_NOT_MESSAGE: invert = TRUE; matchertype = MATCHCRITERIA_MESSAGE; break;
		case MATCHCRITERIA_NOT_BODY_PART: invert = TRUE; matchertype = MATCHCRITERIA_BODY_PART; break;
		case MATCHCRITERIA_NOT_TO_AND_NOT_CC: invert = TRUE; matchertype = MATCHCRITERIA_TO_OR_CC; break;
		case MATCHCRITERIA_NOT_MESSAGEID: invert = TRUE; matchertype = MATCHCRITERIA_MESSAGEID; break;
		case MATCHCRITERIA_NOT_INREPLYTO: invert = TRUE; matchertype = MATCHCRITERIA_INREPLYTO; break;
		}

		/* 
		 * this aborts conversion even for predicates understood by the following code.
		 * while that might seem wasteful, claws local search for information listed below
		 * has proven faster than IMAP search plus network roundtrips. once this changes,
		 * consider removing these exceptions.
		 */
		if (imap_matcher_type_is_local(matchertype))
			return NULL;

		/* the Message-ID header is also cached */
		if (matchertype == MATCHCRITERIA_HEADER && g_strcmp0("Message-ID", match->header) == 0) {
			return NULL;
		}

		switch (matchertype) {
		case MATCHCRITERIA_FORWARDED:
			result = imap_search_new(IMAP_SEARCH_CRITERIA_TAG, NULL, RTAG_FORWARDED, 0);
			break;

		case MATCHCRITERIA_SPAM:
			result = imap_search_new(IMAP_SEARCH_CRITERIA_TAG, NULL, RTAG_JUNK, 0);
			break;

		case MATCHCRITERIA_MESSAGEID:
			result = imap_search_new(IMAP_SEARCH_CRITERIA_HEADER, "Message-ID", match->expr, 0);
			break;

		case MATCHCRITERIA_INREPLYTO:
			result = imap_search_new(IMAP_SEARCH_CRITERIA_HEADER, "In-Reply-To", match->expr, 0);
			break;

		case MATCHCRITERIA_REFERENCES:
			result = imap_search_new(IMAP_SEARCH_CRITERIA_HEADER, "References", match->expr, 0);
			break;

		case MATCHCRITERIA_TO_OR_CC:
			result = imap_search_or(
					imap_search_new(IMAP_SEARCH_CRITERIA_TO, NULL, match->expr, 0),
					imap_search_new(IMAP_SEARCH_CRITERIA_CC, NULL, match->expr, 0)
					);
			break;

		case MATCHCRITERIA_HEADERS_PART:
		case MATCHCRITERIA_HEADERS_CONT:
			result = imap_search_and(
					imap_search_not(imap_search_new(IMAP_SEARCH_CRITERIA_BODY, NULL, match->expr, 0)),
					imap_search_new(IMAP_SEARCH_CRITERIA_MESSAGE, NULL, match->expr, 0)
					);
			break;

		case MATCHCRITERIA_SIZE_EQUAL:
			result = imap_search_and(
					imap_search_not(imap_search_new(IMAP_SEARCH_CRITERIA_SIZE_SMALLER, NULL, NULL, match->value)),
					imap_search_not(imap_search_new(IMAP_SEARCH_CRITERIA_SIZE_GREATER, NULL, NULL, match->value))
					);
			break;

		case MATCHCRITERIA_NOT_UNREAD:
			result = imap_search_new(IMAP_SEARCH_CRITERIA_READ, NULL, NULL, 0);
			break;

		case MATCHCRITERIA_UNREAD:
			result = imap_search_new(IMAP_SEARCH_CRITERIA_UNREAD, NULL, NULL, 0);
			break;

		case MATCHCRITERIA_NEW:
			result = imap_search_new(IMAP_SEARCH_CRITERIA_NEW, NULL, NULL, 0);
			break;

		case MATCHCRITERIA_MARKED:
			result = imap_search_new(IMAP_SEARCH_CRITERIA_MARKED, NULL, NULL, 0);
			break;

		case MATCHCRITERIA_DELETED:
			result = imap_search_new(IMAP_SEARCH_CRITERIA_DELETED, NULL, NULL, 0);
			break;

		case MATCHCRITERIA_REPLIED:
			result = imap_search_new(IMAP_SEARCH_CRITERIA_REPLIED, NULL, NULL, 0);
			break;

		case MATCHCRITERIA_TAG:
			{
				gchar *tmp = imap_utf8_to_modified_utf7(match->expr, TRUE);
				result = imap_search_new(IMAP_SEARCH_CRITERIA_TAG, NULL, tmp, 0);
				g_free(tmp);
			}
			break;

		case MATCHCRITERIA_SUBJECT:
			result = imap_search_new(IMAP_SEARCH_CRITERIA_SUBJECT, NULL, match->expr, 0);
			break;

		case MATCHCRITERIA_FROM:
			result = imap_search_new(IMAP_SEARCH_CRITERIA_FROM, NULL, match->expr, 0);
			break;

		case MATCHCRITERIA_TO:
			result = imap_search_new(IMAP_SEARCH_CRITERIA_TO, NULL, match->expr, 0);
			break;

		case MATCHCRITERIA_CC:
			result = imap_search_new(IMAP_SEARCH_CRITERIA_CC, NULL, match->expr, 0);
			break;

		case MATCHCRITERIA_AGE_GREATER:
			result = imap_search_new(IMAP_SEARCH_CRITERIA_AGE_GREATER, NULL, NULL, match->value);
			break;

		case MATCHCRITERIA_AGE_LOWER:
			result = imap_search_new(IMAP_SEARCH_CRITERIA_AGE_LOWER, NULL, NULL, match->value);
			break;

		case MATCHCRITERIA_BODY_PART:
			result = imap_search_new(IMAP_SEARCH_CRITERIA_BODY, NULL, match->expr, 0);
			break;

		case MATCHCRITERIA_MESSAGE:
			result = imap_search_new(IMAP_SEARCH_CRITERIA_MESSAGE, NULL, match->expr, 0);
			break;

		case MATCHCRITERIA_HEADER:
			result = imap_search_new(IMAP_SEARCH_CRITERIA_HEADER, match->header, match->expr, 0);
			break;

		case MATCHCRITERIA_SIZE_GREATER:
			result = imap_search_new(IMAP_SEARCH_CRITERIA_SIZE_GREATER, NULL, NULL, match->value);
			break;

		case MATCHCRITERIA_SIZE_SMALLER:
			result = imap_search_new(IMAP_SEARCH_CRITERIA_SIZE_SMALLER, NULL, NULL, match->value);
			break;

		default:
			result = imap_search_new(IMAP_SEARCH_CRITERIA_ALL, NULL, NULL, 0);
			if (is_all) {
				*is_all = TRUE;
			}
			break;
		}

		if (invert) {
			result = imap_search_not(result);
			if (is_all && *is_all) {
				*is_all = FALSE;
			}
		}

		return result;
	}

	return NULL;
}

static void imap_change_search_charset(IMAPFolder *folder)
{
	/* If server supports charset in searches, but the last used one failed,
	 * changed to the next preferred charset. If none are still available,
	 * disable charset searches.
	 * Charsets are tried in the following order: 
	 * UTF-8, locale's charset, UTF-7.
	 */

	if (folder->search_charset_supported) {
		if (folder->search_charset && !strcmp(folder->search_charset, conv_get_locale_charset_str_no_utf8()))
			folder->search_charset = "UTF-8";
		else if (folder->search_charset && !strcmp(folder->search_charset, "UTF-8"))
			folder->search_charset = "UTF-7";
		else {
			folder->search_charset = NULL;
			folder->search_charset_supported = FALSE;
		}
	}
}

static MatcherProp *imap_matcher_prop_set_charset(IMAPFolder *folder,
						  MatcherProp *utf8_prop,
						  gchar **charset)
{
	/* If the match is going to be done locally, or the criteria is on
	 * tag (special-cased to modified-UTF-7), or the expression searched
	 * is ASCII, don't bother converting.
	 */
	if (imap_matcher_type_is_local(utf8_prop->criteria)
	 || utf8_prop->criteria == MATCHCRITERIA_TAG
	 || utf8_prop->criteria == MATCHCRITERIA_NOT_TAG
	 || utf8_prop->expr == NULL
	 || is_ascii_str(utf8_prop->expr))
		return matcherprop_new(utf8_prop->criteria,
			       utf8_prop->header,
			       utf8_prop->matchtype,
			       utf8_prop->expr,
			       utf8_prop->value);
	else {
		gchar *conv_expr = NULL;

		/* If the search is server-side and the server doesn't support
		 * searching with the charsets we handle, bail out.
		 */
		if (folder->search_charset_supported == FALSE)
			return NULL;

		/* Else, convert. */
		if (*charset == NULL)
			*charset = g_strdup(folder->search_charset);

		conv_expr = conv_codeset_strdup(utf8_prop->expr, CS_UTF_8, *charset);

		if (conv_expr == NULL)
			conv_expr = g_strdup(utf8_prop->expr);

		return matcherprop_new(utf8_prop->criteria,
			       utf8_prop->header,
			       utf8_prop->matchtype,
			       conv_expr,
			       utf8_prop->value);
	}
}

static gint	search_msgs		(Folder			*folder,
					 FolderItem		*container,
					 MsgNumberList		**msgs,
					 gboolean		*on_server,
					 MatcherList		*predicate,
					 SearchProgressNotify	progress_cb,
					 gpointer		progress_data)
{
	IMAPSearchKey* key = NULL;
	GSList* cur;
	int result = -1;
	clist* uidlist = NULL;
	gboolean server_filtering_useless = FALSE;
        IMAPSession *session;
	gchar *charset_to_use = NULL;

	if (on_server == NULL || !*on_server) {
		return folder_item_search_msgs_local(folder, container, msgs, on_server,
				predicate, progress_cb, progress_data);
	}

	for (cur = predicate->matchers; cur != NULL; cur = cur->next) {
		IMAPSearchKey* matcherPart = NULL;
		MatcherProp* prop = (MatcherProp*) cur->data;
		gboolean is_all;
		MatcherProp *imap_prop = imap_matcher_prop_set_charset(IMAP_FOLDER(folder), prop, &charset_to_use);

		if (imap_prop == NULL) {
			/* Couldn't convert matcherprop to IMAP - probably not ascii
			 * and server doesn't support the charsets we do. */
			 return -1;
		}

		matcherPart = search_make_key(imap_prop, &is_all);

		matcherprop_free(imap_prop);

		if (on_server) {
			*on_server &= matcherPart != NULL && prop->matchtype == MATCHTYPE_MATCHCASE;
		}

		if (matcherPart) {
			if (key == NULL) {
				key = matcherPart;
				server_filtering_useless = is_all;
			} else if (predicate->bool_and) {
				key = imap_search_and(key, matcherPart);
				server_filtering_useless &= is_all;
			} else {
				key = imap_search_or(key, matcherPart);
				server_filtering_useless |= is_all;
			}
		}
	}

	if (server_filtering_useless) {
		imap_search_free(key);
		key = NULL;
	}

	if (key == NULL && progress_cb != NULL) {
		GSList* cur;
		GSList* list;
		int count = 0;

		progress_cb(progress_data, TRUE, 0, 0, container->total_msgs);
		progress_cb(progress_data, TRUE, container->total_msgs, 0, container->total_msgs);

		list = folder_item_get_msg_list(container);
		for (cur = list; cur != NULL; cur = cur->next) {
			*msgs = g_slist_prepend(*msgs, GUINT_TO_POINTER(((MsgInfo*) cur->data)->msgnum));
			count++;
		}
		procmsg_msg_list_free(list);

		*msgs = g_slist_reverse(*msgs);

		return count;
	}

	session = imap_session_get(folder);
        if (!session) {
                return -1;
        }
	result = imap_select(session, IMAP_FOLDER(folder), FOLDER_ITEM(container),
			 NULL, NULL, NULL, NULL, NULL, TRUE);
	if (result != MAILIMAP_NO_ERROR)
		return -1;

	if (progress_cb)
		progress_cb(progress_data, TRUE, 0, 0, container->total_msgs);
	result = imap_threaded_search(folder, IMAP_SEARCH_TYPE_KEYED, key, charset_to_use, NULL, &uidlist);
	if (progress_cb)
		progress_cb(progress_data, TRUE, container->total_msgs, 0, container->total_msgs);

	if (result == MAILIMAP_ERROR_PROTOCOL) {
		debug_print("Server side search unavailable, using local search\n");
		imap_handle_error(SESSION(session), NULL, result);
		result = folder_item_search_msgs_local(folder, container, msgs,	NULL,
						       predicate, progress_cb, progress_data);
		if (result < 0) {
			debug_print("search_msgs - got protocol error, aborting\n");
			alertpanel_error_log(_("Search failed due to server error."));
			return -1;
		}

		return result;
	}
	
	if (result == MAILIMAP_NO_ERROR) {
		gint result = 0;

		*msgs = imap_uid_list_from_lep(uidlist, &result);

		mailimap_search_result_free(uidlist);

		if (charset_to_use != NULL)
			g_free(charset_to_use);

		return result;
	} else if (charset_to_use != NULL) {
		/* If search failed and was on an 8-bit string, try the next
		 * available charset to search if there still are some.
		 */
		g_free(charset_to_use);
		
		imap_change_search_charset(IMAP_FOLDER(folder));
		if (IMAP_FOLDER(folder)->search_charset_supported)
			return search_msgs(folder, container, msgs, on_server, predicate,
				   progress_cb, progress_data);
		else {
			imap_handle_error(SESSION(session), NULL, result);
			return -1;
		}
	} else {
		imap_handle_error(SESSION(session), NULL, result);
		return -1;
	}
}


static gint imap_do_remove_msgs(Folder *folder, FolderItem *dest, 
			        MsgInfoList *msglist, GHashTable *relation)
{
	gchar *destdir, *dir;
	GSList *numlist = NULL, *cur;
	MsgInfo *msginfo;
	IMAPSession *session;
	gint ok = MAILIMAP_NO_ERROR;
	
	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(msglist != NULL, -1);

	debug_print("getting session...\n");
	session = imap_session_get(folder);
	if (!session) {
		return -1;
	}

	lock_session(session); /* unlocked later in the function */

	msginfo = (MsgInfo *)msglist->data;

	ok = imap_select(session, IMAP_FOLDER(folder), msginfo->folder,
			 NULL, NULL, NULL, NULL, NULL, FALSE);
	if (ok != MAILIMAP_NO_ERROR) {
		return ok;
	}

	destdir = imap_get_real_path(session, IMAP_FOLDER(folder), dest->path, &ok);
	if (is_fatal(ok)) {
		g_free(destdir);
		return ok;
	}
	for (cur = msglist; cur; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;
		if (!MSG_IS_DELETED(msginfo->flags))
			numlist = g_slist_prepend(numlist, GINT_TO_POINTER(msginfo->msgnum));
	}
	numlist = g_slist_reverse(numlist);

	if (numlist != NULL) {
		ok = imap_set_message_flags
			(session, IMAP_FOLDER_ITEM(msginfo->folder), numlist, IMAP_FLAG_DELETED, NULL, TRUE);
		if (ok != MAILIMAP_NO_ERROR) {
			log_warning(LOG_PROTOCOL, _("can't set deleted flags\n"));
			g_free(destdir);
			return ok;
		}
	} /* else we just need to expunge */
	ok = imap_cmd_expunge(session);
	if (ok != MAILIMAP_NO_ERROR) {
		log_warning(LOG_PROTOCOL, _("can't expunge\n"));
		g_free(destdir);
		return ok;
	}
	
	session->folder_content_changed = TRUE;
	unlock_session(session);

	dir = folder_item_get_path(msginfo->folder);
	if (is_dir_exist(dir)) {
		for (cur = msglist; cur; cur = cur->next) {
			msginfo = (MsgInfo *)cur->data;
			remove_numbered_files(dir, msginfo->msgnum, msginfo->msgnum);
		}
	}
	g_free(dir);

	g_slist_free(numlist);

	imap_scan_required(folder, dest);

	g_free(destdir);

	return 0;
}

static gint imap_remove_msgs(Folder *folder, FolderItem *dest, 
		    MsgInfoList *msglist, GHashTable *relation)
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

static gint imap_scan_tree_real(Folder *folder, gboolean subs_only)
{
	FolderItem *item = NULL;
	IMAPSession *session;
	gchar *root_folder = NULL;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(folder->account != NULL, -1);

	debug_print("getting session...\n");
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

	if (folder->account->imap_dir && *folder->account->imap_dir) {
		gchar *real_path;
		int r = MAILIMAP_NO_ERROR;
		clist * lep_list;

		Xstrdup_a(root_folder, folder->account->imap_dir, {return -1;});
		extract_quote(root_folder, '"');
		subst_char(root_folder,
			   imap_get_path_separator(session, IMAP_FOLDER(folder),
						   root_folder, &r),
			   '/');
		if (is_fatal(r))
			return -1;
		strtailchomp(root_folder, '/');
		real_path = imap_get_real_path
			(session, IMAP_FOLDER(folder), root_folder, &r);
		if (is_fatal(r)) {
			g_free(real_path);
			return -1;
		}
		debug_print("IMAP root directory: %s\n", real_path);

		/* check if root directory exist */

		r = imap_threaded_list(session->folder, "", real_path,
				       &lep_list);

		if (r != MAILIMAP_NO_ERROR)
			imap_handle_error(SESSION(session), NULL, r);

		if ((r != MAILIMAP_NO_ERROR) || (clist_count(lep_list) == 0)) {
			if (!folder->node) {
				item = folder_item_new(folder, folder->name, NULL);
				item->folder = folder;
				folder->node = item->node = g_node_new(item);
			}
			return -1;
		}
		mailimap_list_result_free(lep_list);
				
		g_free(real_path);
	}

	if (folder->node)
		item = FOLDER_ITEM(folder->node->data);
		
	if (item && !item->path && root_folder) {
		item->path = g_strdup(root_folder);
	}

	if (!item || ((item->path || root_folder) &&
		      g_strcmp0(item->path, root_folder) != 0)) {
		folder_tree_destroy(folder);
		item = folder_item_new(folder, folder->name, root_folder);
		item->folder = folder;
		folder->node = item->node = g_node_new(item);
	}

	imap_scan_tree_recursive(session, FOLDER_ITEM(folder->node->data), subs_only);
	imap_create_missing_folders(folder);

	return 0;
}

static gint imap_scan_tree(Folder *folder)
{
	gboolean subs_only = FALSE;
	if (folder->account) {
		debug_print(" scanning only subs %d\n", folder->account->imap_subsonly);
		subs_only = folder->account->imap_subsonly;
	}
	return imap_scan_tree_real(folder, subs_only);
}

static gint imap_scan_tree_recursive(IMAPSession *session, FolderItem *item, gboolean subs_only)
{
    /* reset recursion depth counter */
    session->scan_tree_recurs_depth = 0;

    return imap_scan_tree_recursive_dive(session, item, subs_only);
}

static gint imap_scan_tree_recursive_dive(IMAPSession *session, FolderItem *item, gboolean subs_only)
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
	int r = MAILIMAP_NO_ERROR;
	
	g_return_val_if_fail(item != NULL, -1);
	g_return_val_if_fail(item->folder != NULL, -1);
	g_return_val_if_fail(item->no_sub == FALSE, -1);

    /* recursion depth limiter */
    if(session->scan_tree_recurs_depth >= prefs_common.imap_scan_tree_recurs_limit) {
        g_warning("IMAP scan tree recursion limit reached (%d, folder '%s')",
                prefs_common.imap_scan_tree_recurs_limit, item->folder->name);
        return -1;
    }
    /* entering recursion func: increase depth counter */
    session->scan_tree_recurs_depth++;

	folder = item->folder;
	imapfolder = IMAP_FOLDER(folder);

	separator = imap_get_path_separator(session, imapfolder, item->path, &r);
	if (is_fatal(r))
		return r;

	if (folder->ui_func)
		folder->ui_func(folder, item, folder->ui_func_data);

	if (item->path) {
		wildcard[0] = separator;
		wildcard[1] = '%';
		wildcard[2] = '\0';
		real_path = imap_get_real_path(session, imapfolder, item->path, &r);
		if (is_fatal(r)) {
			g_free(real_path);
			return r;
		}
	} else {
		wildcard[0] = '%';
		wildcard[1] = '\0';
		real_path = g_strdup("");
	}

	Xstrcat_a(wildcard_path, real_path, wildcard,
		  {g_free(real_path); return MAILIMAP_ERROR_BAD_STATE;});
	lep_list = NULL;
	
	if (subs_only)
		r = imap_threaded_lsub(folder, "", wildcard_path, &lep_list);
	else
		r = imap_threaded_list(folder, "", wildcard_path, &lep_list);

	if (r != MAILIMAP_NO_ERROR) {
		imap_handle_error(SESSION(session), NULL, r);
		item_list = NULL;
		g_free(real_path);
		return r;
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
			if (!g_strcmp0(old_item->path, cur_item->path)) {
				new_item = cur_item;
				break;
			}
		}
		if (!new_item) {
			if (old_item && old_item->path && !strcasecmp(old_item->path, "INBOX")) {
				debug_print("not removing INBOX\n");
			} else {
				debug_print("folder '%s' not found. removing...\n",
					    old_item->path);
				folder_item_remove(old_item);
			}
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
			if (!g_strcmp0(FOLDER_ITEM(node->data)->path,
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

		if (!strcasecmp(new_item->path, "INBOX")) {
			new_item->stype = F_INBOX;
			folder->inbox = new_item;
		} else if (!folder_item_parent(item) || item->stype == F_INBOX) {
			gchar *base;

			base = g_path_get_basename(new_item->path);

			if (!folder->outbox && !g_ascii_strcasecmp(base, "Sent")) {
				new_item->stype = F_OUTBOX;
				folder->outbox = new_item;
			} else if (!folder->draft && (!g_ascii_strcasecmp(base, "Drafts") || !g_ascii_strcasecmp(base, "Draft"))) {
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

		if (new_item->no_sub == FALSE) {
			imap_scan_tree_recursive_dive(session, new_item, subs_only);

            /* entering recursion func: increase depth counter */
            session->scan_tree_recurs_depth--;
            if (session->scan_tree_recurs_depth < 0)
                g_error("IMAP scan tree recursion underflow (%d)",
                        session->scan_tree_recurs_depth);
        }
	}

	g_slist_free(item_list);

	return MAILIMAP_NO_ERROR;
}

GList *imap_scan_subtree(Folder *folder, FolderItem *item, gboolean unsubs_only, gboolean recursive)
{
	IMAPSession *session = imap_session_get(folder);
	gchar *real_path;
	gchar *wildcard_path;
	gchar separator;
	gchar wildcard[3];
	clist * lep_list;
	GSList *item_list = NULL, *cur;
	GList *child_list = NULL, *tmplist = NULL;
	GSList *sub_list = NULL;
	int r = MAILIMAP_NO_ERROR;

	if (!session)
		return NULL;

	separator = imap_get_path_separator(session, IMAP_FOLDER(folder), item->path, &r);
	if (is_fatal(r))
		return NULL;

	if (item->path) {
		wildcard[0] = separator;
		wildcard[1] = '%';
		wildcard[2] = '\0';
		real_path = imap_get_real_path(session, IMAP_FOLDER(folder), item->path, &r);
		if (is_fatal(r)) {
			g_free(real_path);
			return NULL;
		}
	} else {
		wildcard[0] = '%';
		wildcard[1] = '\0';
		real_path = g_strdup("");
	}

	Xstrcat_a(wildcard_path, real_path, wildcard,
		  {g_free(real_path); return NULL;});
	lep_list = NULL;
	
	if (unsubs_only)
		statusbar_print_all(_("Looking for unsubscribed folders in %s..."), 
				item->path?item->path:item->name);
	else
		statusbar_print_all(_("Looking for subfolders of %s..."), 
				item->path?item->path:item->name);

	r = imap_threaded_list(folder, "", wildcard_path, &lep_list);
	if (r) {
		g_free(real_path);
		statusbar_pop_all();
		return NULL;
	}
	item_list = imap_list_from_lep(IMAP_FOLDER(folder),
			       lep_list, real_path, FALSE);
	mailimap_list_result_free(lep_list);

	for (cur = item_list; cur != NULL; cur = cur->next) {
		FolderItem *cur_item = FOLDER_ITEM(cur->data);
		if (recursive) {
			tmplist = imap_scan_subtree(folder, cur_item, 
					unsubs_only, recursive);
			if (tmplist)
				child_list = g_list_concat(child_list, tmplist);
		}
		child_list = g_list_prepend(child_list,
				imap_get_real_path(session, 
					IMAP_FOLDER(folder), cur_item->path, &r));
		if (is_fatal(r)) {
			g_free(real_path);
			statusbar_pop_all();
			return NULL;
		}
		folder_item_destroy(cur_item);
	}
	child_list = g_list_reverse(child_list);
	g_slist_free(item_list);

	if (unsubs_only) {
		r = imap_threaded_lsub(folder, "", wildcard_path, &lep_list);
		if (r) {
			g_free(real_path);
			statusbar_pop_all();
			return NULL;
		}
		sub_list = imap_list_from_lep(IMAP_FOLDER(folder),
				       lep_list, real_path, FALSE);
		mailimap_list_result_free(lep_list);

		for (cur = sub_list; cur != NULL; cur = cur->next) {
			FolderItem *cur_item = FOLDER_ITEM(cur->data);
			GList *oldlitem = NULL;
			gchar *tmp = imap_get_real_path(session, 
					IMAP_FOLDER(folder), cur_item->path, &r);
			if (r) {
				g_free(real_path);
				statusbar_pop_all();
				g_free(tmp);
				return NULL;
			}
			folder_item_destroy(cur_item);
			oldlitem = g_list_find_custom(
					child_list, tmp, (GCompareFunc)g_strcmp0);
			if (oldlitem) {
				child_list = g_list_remove_link(child_list, oldlitem);
				g_free(oldlitem->data);
				g_list_free(oldlitem);
			}
			g_free(tmp);
		}
	}

	g_free(real_path);
	statusbar_pop_all();

	return child_list;
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
		g_warning("can't create '%s'", name);
		if (!folder->inbox) return NULL;

		new_item = imap_create_folder(folder, folder->inbox, name);
		if (!new_item)
			g_warning("can't create '%s' under INBOX", name);
		else
			new_item->stype = stype;
	} else
		new_item->stype = stype;

	return new_item;
}

#ifdef G_OS_WIN32
static gchar *imap_encode_unsafe_chars(const gchar *str)
{
	gchar *ret = NULL, *o_ret;
	gchar *i;
	if (!str) 
		return NULL;
	ret = g_malloc(3*strlen(str)+1);
	o_ret = ret;
	for (i = (gchar *)str; *i; i++) {
		switch(*i) {
			case ':':
			case '|':
			case '<':
			case '>':
			case '*':
			case '?':
			case '#':
				*ret++ = '%';
				*ret++ = '0'+(*i/10);
				*ret++ = '0'+(*i%10);
				break;
			default:
				*ret++ = *i;
		}
	}
	*ret++ = '\0';
	return o_ret;
}
#endif
static gchar *imap_item_get_path(Folder *folder, FolderItem *item)
{
	gchar *folder_path, *path;
	gchar *item_path = NULL;
	
	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(folder->account != NULL, NULL);
	g_return_val_if_fail(item != NULL, NULL);
	folder_path = prefs_account_cache_dir(folder->account, FALSE);

	g_return_val_if_fail(folder_path != NULL, NULL);

#ifdef G_OS_UNIX
	item_path = g_strdup(item->path);
#else
	item_path = imap_encode_unsafe_chars(item->path);
#endif	

        if (g_path_is_absolute(folder_path)) {
                if (item_path)
                        path = g_strconcat(folder_path, G_DIR_SEPARATOR_S,
                                           item_path, NULL);
                else
                        path = g_strdup(folder_path);
        } else {
                if (item_path)
                        path = g_strconcat(get_home_dir(), G_DIR_SEPARATOR_S,
                                           folder_path, G_DIR_SEPARATOR_S,
                                           item_path, NULL);
                else
                        path = g_strconcat(get_home_dir(), G_DIR_SEPARATOR_S,
                                           folder_path, NULL);
        }
        g_free(folder_path);
        g_free(item_path);
#ifdef G_OS_WIN32
	while (strchr(path, '/'))
		*strchr(path, '/') = '\\';
#endif

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
	gint ok = MAILIMAP_NO_ERROR;
	gboolean no_select = FALSE, no_sub = FALSE;
	gboolean exist = FALSE;
	
	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(folder->account != NULL, NULL);
	g_return_val_if_fail(parent != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);

	if (to_number(name) > 0) {
		gchar *cached_msg = imap_get_cached_filename(parent, to_number(name));
		if (is_file_exist(cached_msg)) {
			if (claws_unlink(cached_msg) != 0) {
				g_free(cached_msg);
				return NULL;
			}
		}
		g_free(cached_msg);
	}

	debug_print("getting session...\n");
	session = imap_session_get(folder);
	if (!session) {
		return NULL;
	}

	if (!folder_item_parent(parent) && strcasecmp(name, "INBOX") == 0) {
		dirpath = g_strdup(name);
	}else if (parent->path)
		dirpath = g_strconcat(parent->path, "/", name, NULL);
	else if ((p = strchr(name, '/')) != NULL && *(p + 1) != '\0')
		dirpath = g_strdup(name);
	else if (folder->account->imap_dir && *folder->account->imap_dir) {
		gchar *imap_dir;

		Xstrdup_a(imap_dir, folder->account->imap_dir, {return NULL;});
		strtailchomp(imap_dir, '/');
		dirpath = g_strconcat(imap_dir, "/", name, NULL);
	} else
		dirpath = g_strdup(name);
		
	

	/* keep trailing directory separator to create a folder that contains
	   sub folder */
	imap_path = imap_utf8_to_modified_utf7(dirpath, FALSE);

	strtailchomp(dirpath, '/');
	Xstrdup_a(new_name, name, {
		g_free(dirpath); 
		g_free(imap_path);
		return NULL;});

	separator = imap_get_path_separator(session, IMAP_FOLDER(folder), imap_path, &ok);
	if (is_fatal(ok)) {
		g_free(dirpath);
		g_free(imap_path);
		return NULL;
	}
	imap_path_separator_subst(imap_path, separator);
	/* remove trailing / for display */
	strtailchomp(new_name, '/');

	if (strcasecmp(dirpath, "INBOX") != 0) {
		int r;
		clist * lep_list;
		
		r = imap_threaded_list(folder, "", imap_path, &lep_list);
		if (r != MAILIMAP_NO_ERROR) {
			imap_handle_error(SESSION(session), NULL, r);
			log_warning(LOG_PROTOCOL, _("can't create mailbox: LIST failed\n"));
			g_free(imap_path);
			g_free(dirpath);
			return NULL;
		}
		
		if (clist_count(lep_list) > 0)
			exist = TRUE;
		mailimap_list_result_free(lep_list);
		lep_list = NULL;
		if (!exist) {
			ok = imap_cmd_create(session, imap_path);
			if (ok != MAILIMAP_NO_ERROR) {
				log_warning(LOG_PROTOCOL, _("can't create mailbox\n"));
				g_free(imap_path);
				g_free(dirpath);
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
			} else {
				imap_handle_error(SESSION(session), NULL, r);
			}
		}
		imap_threaded_subscribe(folder, imap_path, TRUE);
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
		} else {
			imap_handle_error(SESSION(session), NULL, r);
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

	if (exist) {
		/* folder existed, scan it */
		imap_scan_required(folder, new_item);
		folder_item_scan_full(new_item, FALSE);
	}

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
	gint ok = MAILIMAP_NO_ERROR;
	gint exists, recent, unseen;
	guint32 uid_validity;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(item != NULL, -1);
	g_return_val_if_fail(item->path != NULL, -1);
	g_return_val_if_fail(name != NULL, -1);

	debug_print("getting session...\n");
	session = imap_session_get(folder);
	if (!session) {
		return -1;
	}

	if (strchr(name, imap_get_path_separator(session, IMAP_FOLDER(folder), item->path, &ok)) != NULL ||
		is_fatal(ok)) {
		g_warning("new folder name must not contain the namespace path separator");
		return -1;
	}

	real_oldpath = imap_get_real_path(session, IMAP_FOLDER(folder), item->path, &ok);
	if (is_fatal(ok)) {
		g_free(real_oldpath);
		return -1;
	}

	g_free(session->mbox);
	session->mbox = NULL;
	session->exists = 0;
	session->recent = 0;
	session->expunge = 0;
	ok = imap_cmd_examine(session, "INBOX",
			      &exists, &recent, &unseen, &uid_validity, FALSE);
	if (ok != MAILIMAP_NO_ERROR) {
		g_free(real_oldpath);
		return -1;
	}

	separator = imap_get_path_separator(session, IMAP_FOLDER(folder), item->path, &ok);
	if (is_fatal(ok)) {
		g_free(real_oldpath);
		return -1;
	}
	if (strchr(item->path, '/')) {
		dirpath = g_path_get_dirname(item->path);
		newpath = g_strconcat(dirpath, "/", name, NULL);
		g_free(dirpath);
	} else
		newpath = g_strdup(name);

	real_newpath = imap_utf8_to_modified_utf7(newpath, FALSE);
	imap_path_separator_subst(real_newpath, separator);

	ok = imap_cmd_rename(session, real_oldpath, real_newpath);
	if (ok != MAILIMAP_NO_ERROR) {
		log_warning(LOG_PROTOCOL, _("can't rename mailbox: %s to %s\n"),
			    real_oldpath, real_newpath);
		g_free(real_oldpath);
		g_free(newpath);
		g_free(real_newpath);
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
		if (g_rename(old_cache_dir, new_cache_dir) < 0) {
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

gint imap_subscribe(Folder *folder, FolderItem *item, gchar *rpath, gboolean sub)
{
	gchar *path;
	gint r = MAILIMAP_NO_ERROR;
	IMAPSession *session;
	debug_print("getting session...\n");

	session = imap_session_get(folder);
	if (!session) {
		return -1;
	}
	if (item && item->path) {
		path = imap_get_real_path(session, IMAP_FOLDER(folder), item->path, &r);
		if (!path)
			return -1;
		if (is_fatal(r)) {
			g_free(path);
			return -1;
		}
		if (!strcasecmp(path, "INBOX") && sub == FALSE) {
			g_free(path);
			return -1;
		}
		debug_print("%ssubscribing %s\n", sub?"":"un", path);
		r = imap_threaded_subscribe(folder, path, sub);
		g_free(path);
	} else if (rpath) {
		r = imap_threaded_subscribe(folder, rpath, sub);
	} else
		return -1;
	return r;
}

static gint imap_remove_folder_real(Folder *folder, FolderItem *item)
{
	gint ok = MAILIMAP_NO_ERROR;
	IMAPSession *session;
	gchar *path;
	gchar *cache_dir;
	gboolean selected_folder;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(item != NULL, -1);
	g_return_val_if_fail(item->path != NULL, -1);

	debug_print("getting session...\n");
	session = imap_session_get(folder);
	if (!session) {
		return -1;
	}
	path = imap_get_real_path(session, IMAP_FOLDER(folder), item->path, &ok);
	if (is_fatal(ok)) {
		g_free(path);
		return -1;
	}
	imap_threaded_subscribe(folder, path, FALSE);

	selected_folder = (session->mbox != NULL) &&
			  (!strcmp(session->mbox, item->path));
	if (selected_folder) {
		ok = imap_cmd_close(session);
		if (ok != MAILIMAP_NO_ERROR) {
			debug_print("close err %d\n", ok);
			return ok;
		}
	}
	ok = imap_cmd_delete(session, path);
	if (ok != MAILIMAP_NO_ERROR && !is_fatal(ok)) {
		gchar *tmp = NULL;
		
		ok = MAILIMAP_NO_ERROR;
		tmp = g_strdup_printf("%s%c", path, 
				imap_get_path_separator(session, IMAP_FOLDER(folder), path, &ok));
		g_free(path);
		path = tmp;
		if (!is_fatal(ok))
			ok = imap_cmd_delete(session, path);
	}

	if (ok != MAILIMAP_NO_ERROR) {
		log_warning(LOG_PROTOCOL, _("can't delete mailbox\n"));
		g_free(path);
		return -1;
	}

	g_free(path);
	cache_dir = folder_item_get_path(item);
	if (is_dir_exist(cache_dir) && remove_dir_recursive(cache_dir) < 0)
		g_warning("can't remove directory '%s'", cache_dir);
	g_free(cache_dir);
	folder_item_remove(item);
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
	int ok;
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
	gboolean got_alien_tags = FALSE;

	debug_print("uncached_messages\n");
	
	if (session == NULL || item == NULL || item->folder == NULL
	    || FOLDER_CLASS(item->folder) != &imap_class) {
		stuff->done = TRUE;
		return NULL;
	}
	
	seq_list = imap_get_lep_set_from_numlist(IMAP_FOLDER(item->folder), numlist);
	debug_print("get msgs info\n");
	for (cur = seq_list; cur != NULL; cur = g_slist_next(cur)) {
		struct mailimap_set * imapset;
		unsigned int i;
		int r;
		carray * env_list;
		int count;
		
		if (session->cancelled)
			break;
		
		imapset = cur->data;
		
		r = imap_threaded_fetch_env(session->folder,
					    imapset, &env_list);
		if (r != MAILIMAP_NO_ERROR) {
			imap_handle_error(SESSION(session), NULL, r);
			if (is_fatal(r)) {
				stuff->ok = r;
				return NULL;
			}
			continue;
		}

		session_set_access_time(SESSION(session));

		count = 0;
		for(i = 0 ; i < carray_count(env_list) ; i += 2) {
			struct imap_fetch_env_info * info;
			MsgInfo * msginfo;
			GSList *tags = NULL, *cur = NULL;
			info = carray_get(env_list, i);
			tags = carray_get(env_list, i+1);
			msginfo = imap_envelope_from_lep(info, item);
			if (msginfo == NULL) {
				slist_free_strings_full(tags);
				continue;
			}
			g_slist_free(msginfo->tags);
			msginfo->tags = NULL;

			for (cur = tags; cur; cur = cur->next) {
				gchar *real_tag = imap_modified_utf7_to_utf8(cur->data, TRUE);
				gint id = 0;
				id = tags_get_id_for_str(real_tag);
				if (id == -1) {
					id = tags_add_tag(real_tag);
					got_alien_tags = TRUE;
				}
				if (!g_slist_find(msginfo->tags, GINT_TO_POINTER(id))) {
					msginfo->tags = g_slist_prepend(
							msginfo->tags,
							GINT_TO_POINTER(id));
				}
				g_free(real_tag);
			}
			if (msginfo->tags)
				msginfo->tags = g_slist_reverse(msginfo->tags);
			slist_free_strings_full(tags);
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
	
	if (got_alien_tags) {
		tags_write_tags();
		main_window_reflect_tags_changes(mainwindow_get_mainwindow());
	}

	imap_lep_set_free(seq_list);
	
	session_set_access_time(SESSION(session));
	stuff->done = TRUE;
	return newlist;
}

#define MAX_MSG_NUM 50

static GSList *imap_get_uncached_messages(IMAPSession *session,
					FolderItem *item,
					MsgNumberList *numlist,
					int *r)
{
	GSList *result = NULL;
	GSList * cur;
	uncached_data *data = g_new0(uncached_data, 1);
	
	cur = numlist;
	data->total = g_slist_length(numlist);
	data->ok = MAILIMAP_NO_ERROR;
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
		
		if (prefs_common.work_offline && 
		    !inc_offline_should_override(FALSE,
			_("Claws Mail needs network access in order "
			  "to access the IMAP server."))) {
			g_free(data);
			return NULL;
		}
		
		partial_result =
			(GSList *)imap_get_uncached_messages_thread(data);
		*r = data->ok;
		if (data->ok != MAILIMAP_NO_ERROR) {
			goto bail;
		}
		statusbar_progress_all(data->cur,data->total, 1);
		
		g_slist_free(newlist);
		
		result = g_slist_concat(result, partial_result);
	}
bail:
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

	debug_print("Deleting all cached messages done.\n");
}

gchar imap_get_path_separator_for_item(FolderItem *item)
{
	Folder *folder = NULL;
	IMAPFolder *imap_folder = NULL;
	IMAPSession *session = NULL;
	gchar result = '/';
	gint ok = MAILIMAP_NO_ERROR;
	if (!item)
		return '/';
	folder = item->folder;

	if (!folder)
		return '/';

	imap_folder = IMAP_FOLDER(folder);

	debug_print("getting session...\n");
	session = imap_session_get(FOLDER(folder));
	result = imap_get_path_separator(session, imap_folder, item->path, &ok);
	return result;
}

static gchar imap_refresh_path_separator(IMAPSession *session, IMAPFolder *folder, const gchar *subfolder, gint *ok)
{
	clist * lep_list;
	int r;
	gchar separator = '\0';
	
	g_return_val_if_fail(session != NULL, '/');
	r = imap_threaded_list((Folder *)folder, "", subfolder, &lep_list);
	
	if (r != MAILIMAP_NO_ERROR) {
		imap_handle_error(SESSION(session), NULL, r);
		log_warning(LOG_PROTOCOL, _("LIST failed\n"));
		*ok = r;
		return '\0';
	}

	if (lep_list != NULL && clist_count(lep_list) > 0) {
		clistiter * iter = clist_begin(lep_list); 
		struct mailimap_mailbox_list * mb;
		mb = clist_content(iter);

		separator = mb->mb_delimiter;
		debug_print("got separator: %c\n", folder->last_seen_separator);
	}
	*ok = MAILIMAP_NO_ERROR;
	mailimap_list_result_free(lep_list);
	return separator;
}

static gchar imap_get_path_separator(IMAPSession *session, IMAPFolder *folder, const gchar *path, gint *ok)
{
	gchar separator = '/';
	*ok = MAILIMAP_NO_ERROR;
	if (folder->last_seen_separator == 0) {
		folder->last_seen_separator = imap_refresh_path_separator(session, folder, "", ok);
	}

	if (folder->last_seen_separator == 0) {
		folder->last_seen_separator = imap_refresh_path_separator(session, folder, "INBOX", ok);
	}

	if (folder->last_seen_separator != 0) {
		debug_print("using separator: %c\n", folder->last_seen_separator);
		return folder->last_seen_separator;
	}

	return separator;
}

static gchar *imap_get_real_path(IMAPSession *session, IMAPFolder *folder, const gchar *path, gint *ok)
{
	gchar *real_path = NULL;
	gchar separator;
	
	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(path != NULL, NULL);

	*ok = MAILIMAP_NO_ERROR;
	
	real_path = imap_utf8_to_modified_utf7(path, FALSE);
	separator = imap_get_path_separator(session, folder, path, ok);
	if (*ok == MAILIMAP_NO_ERROR)
		imap_path_separator_subst(real_path, separator);

	return real_path;
}

static gint imap_set_message_flags(IMAPSession *session,
				   IMAPFolderItem *item,
				   MsgNumberList *numlist,
				   IMAPFlags flags,
				   GSList *tags,
				   gboolean is_set)
{
	gint ok = 0;
	GSList *seq_list;
	GSList * cur;
	gint total = 0;
	IMAPFolder *folder = NULL;
	GSList *sorted_list = NULL;

	if (numlist == NULL || session == NULL)
		return MAILIMAP_ERROR_BAD_STATE;
	
	folder = IMAP_FOLDER(session->folder);
	
	sorted_list = g_slist_copy(numlist);
	sorted_list = g_slist_sort(sorted_list, g_int_compare);
	
	cur = g_slist_last(sorted_list);

	if (cur)
		total = GPOINTER_TO_INT(cur->data);
	
	seq_list = imap_get_lep_set_from_numlist(IMAP_FOLDER(session->folder), sorted_list);

	statusbar_print_all(_("Flagging messages..."));

	for(cur = seq_list ; cur != NULL ; cur = g_slist_next(cur)) {
		struct mailimap_set * imapset = (struct mailimap_set *)cur->data;
		struct mailimap_set_item *set_item = NULL;
		
		if (imapset->set_list)
			set_item = clist_content(clist_begin(imapset->set_list));
		else
			continue;

		if (set_item == NULL)
			continue;

		statusbar_progress_all(set_item->set_first, total, 1);

		ok = imap_cmd_store(session, item, imapset,
				    flags, tags, is_set);
		statusbar_progress_all(set_item->set_last, total, 1);
		if (ok != MAILIMAP_NO_ERROR && folder->max_set_size > 20) {
			/* reduce max set size */
			folder->max_set_size /= 2;
		}
		if (ok != MAILIMAP_NO_ERROR && is_fatal(ok)) {
			break;
		}
	}
	
	g_slist_free(sorted_list);

	statusbar_progress_all(0,0,0);
	statusbar_pop_all();

	imap_lep_set_free(seq_list);
	
	return ok;
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
			FolderItem *item,
			gint *exists, gint *recent, gint *unseen,
			guint32 *uid_validity, gint *can_create_flags,
			gboolean block)
{
	gchar *real_path;
	gint ok = MAILIMAP_NO_ERROR;
	gint exists_, recent_, unseen_;
	guint32 uid_validity_;
	gint can_create_flags_;
	const gchar *path = item ? item->path:NULL;

	if (!item) {
		return MAILIMAP_ERROR_BAD_STATE;
	}

	if (!exists && !recent && !unseen && !uid_validity && !can_create_flags) {
		if (session->mbox && strcmp(session->mbox, path) == 0)
			return MAILIMAP_NO_ERROR;
	}
	if (!exists && !recent && !unseen && !uid_validity && can_create_flags) {
		if (session->mbox && strcmp(session->mbox, path) == 0) {
			if (IMAP_FOLDER_ITEM(item)->can_create_flags != ITEM_CAN_CREATE_FLAGS_UNKNOWN)
				return MAILIMAP_NO_ERROR;
		}
	}
	if (!exists)
		exists = &exists_;
	if (!recent)
		recent = &recent_;
	if (!unseen)
		unseen = &unseen_;
	if (!uid_validity)
		uid_validity = &uid_validity_;
	if (!can_create_flags)
		can_create_flags = &can_create_flags_;

	g_free(session->mbox);
	session->mbox = NULL;
	session->exists = 0;
	session->recent = 0;
	session->expunge = 0;

	real_path = imap_get_real_path(session, folder, path, &ok);
	if (is_fatal(ok)) {
		g_free(real_path);
		return ok;	
	}	
	g_slist_free(IMAP_FOLDER_ITEM(item)->ok_flags);
	IMAP_FOLDER_ITEM(item)->ok_flags = NULL;
	ok = imap_cmd_select(session, real_path,
			     exists, recent, unseen, uid_validity, can_create_flags, 
			     &(IMAP_FOLDER_ITEM(item)->ok_flags), block);
	if (ok != MAILIMAP_NO_ERROR) {
		log_warning(LOG_PROTOCOL, _("can't select folder: %s\n"), real_path);
	} else {
		session->mbox = g_strdup(path);
		session->folder_content_changed = FALSE;
		session->exists = *exists;
		session->recent = *recent;
		session->expunge = 0;
		session->unseen = *unseen;
		session->uid_validity = *uid_validity;
		debug_print("select: exists %d recent %d expunge %d uid_validity %d can_create_flags %d\n", 
			session->exists, session->recent, session->expunge,
			session->uid_validity, *can_create_flags);
	}
	if (*can_create_flags) {
		IMAP_FOLDER_ITEM(item)->can_create_flags = ITEM_CAN_CREATE_FLAGS;
	} else {
		IMAP_FOLDER_ITEM(item)->can_create_flags = ITEM_CANNOT_CREATE_FLAGS;
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
	int r = MAILIMAP_NO_ERROR;
	clistiter * iter;
	struct mailimap_mailbox_data_status * data_status;
	int got_values;
	gchar *real_path;
	guint mask = 0;
	
	real_path = imap_get_real_path(session, folder, path, &r);
	if (is_fatal(r)) {
		g_free(real_path);
		return r;
	}
	if (messages) {
		mask |= 1 << 0;
		*messages = 0;
	}
	if (uid_next) {
		mask |= 1 << 2;
		*uid_next = 0;
	}
	if (uid_validity) {
		mask |= 1 << 3;
		*uid_validity = 0;
	}
	if (unseen) {
		mask |= 1 << 4;
		*unseen = 0;
	}
	
	if (session->mbox != NULL &&
	    !strcmp(session->mbox, item->item.path)) {
		r = imap_cmd_close(session);
		if (r != MAILIMAP_NO_ERROR) {
			debug_print("close err %d\n", r);
			g_free(real_path);
			return r;
		}
	}
	
	r = imap_threaded_status(FOLDER(folder), real_path, 
		&data_status, mask);

	g_free(real_path);
	if (r != MAILIMAP_NO_ERROR) {
		imap_handle_error(SESSION(session), NULL, r);
		debug_print("status err %d\n", r);
		return r;
	}
	
	if (data_status == NULL || data_status->st_info_list == NULL) {
		debug_print("data_status %p\n", data_status);
		if (data_status) {
			debug_print("data_status->st_info_list %p\n", data_status->st_info_list);
			mailimap_mailbox_data_status_free(data_status);
		}
		return MAILIMAP_ERROR_BAD_STATE;
	}
	
	got_values = 0;
	if (data_status->st_info_list) {
		for(iter = clist_begin(data_status->st_info_list) ; iter != NULL ;
		    iter = clist_next(iter)) {
			struct mailimap_status_info * info;		

			info = clist_content(iter);
			switch (info->st_att) {
			case MAILIMAP_STATUS_ATT_MESSAGES:
				if (messages) {
					* messages = info->st_value;
					got_values |= 1 << 0;
				}
				break;

			case MAILIMAP_STATUS_ATT_UIDNEXT:
				if (uid_next) {
					* uid_next = info->st_value;
					got_values |= 1 << 2;
				}
				break;

			case MAILIMAP_STATUS_ATT_UIDVALIDITY:
				if (uid_validity) {
					* uid_validity = info->st_value;
					got_values |= 1 << 3;
				}
				break;

			case MAILIMAP_STATUS_ATT_UNSEEN:
				if (unseen) {
					* unseen = info->st_value;
					got_values |= 1 << 4;
				}
				break;
			}
		}
	}
	mailimap_mailbox_data_status_free(data_status);
	
	if (got_values != mask) {
		g_warning("status: incomplete values received (%d)", got_values);
	}
	return MAILIMAP_NO_ERROR;
}

static void imap_free_capabilities(IMAPSession *session)
{
	slist_free_strings_full(session->capability);
	session->capability = NULL;
}

/* low-level IMAP4rev1 commands */

static gint imap_cmd_login(IMAPSession *session,
			   const gchar *user, const gchar *pass,
			   const gchar *type)
{
	int r;
	gint ok;

	if (!strcmp(type, "plaintext") && imap_has_capability(session, "LOGINDISABLED")) {
		ok = MAILIMAP_ERROR_BAD_STATE;
		if (imap_has_capability(session, "STARTTLS")) {
#ifdef USE_GNUTLS
			log_warning(LOG_PROTOCOL, _("Server requires STARTTLS to log in.\n"));
			ok = imap_cmd_starttls(session);
			if (ok != MAILIMAP_NO_ERROR) {
				log_warning(LOG_PROTOCOL, _("Can't start STARTTLS session.\n"));
				return ok;
			} else {
				/* refresh capas */
				imap_free_capabilities(session);
				if ((r = imap_get_capabilities(session)) != MAILIMAP_NO_ERROR) {
					imap_handle_error(SESSION(session), NULL, r);
					log_warning(LOG_PROTOCOL, _("Can't refresh capabilities.\n"));
					return r;
				}
			}
#else		
			log_error(LOG_PROTOCOL, _("Connection to %s failed: "
					"server requires STARTTLS, but Claws Mail "
					"has been compiled without STARTTLS "
					"support.\n"),
					SESSION(session)->server);
			return MAILIMAP_ERROR_LOGIN;
#endif
		} else {
			log_error(LOG_PROTOCOL, _("Server logins are disabled.\n"));
			return MAILIMAP_ERROR_LOGIN;
		}
	}

	log_print(LOG_PROTOCOL, "IMAP> Logging %s to %s using %s\n", 
			user,
			SESSION(session)->server,
			type);
	r = imap_threaded_login(session->folder, user, pass, type);
	if (r != MAILIMAP_NO_ERROR) {
		imap_handle_error(SESSION(session), NULL, r);
		log_print(LOG_PROTOCOL, "IMAP< Error logging in to %s\n",
				SESSION(session)->server);
		ok = r;
	} else {
		log_print(LOG_PROTOCOL, "IMAP< Login to %s successful\n",
				SESSION(session)->server);
		ok = MAILIMAP_NO_ERROR;
	}
	return ok;
}

static gint imap_cmd_noop(IMAPSession *session)
{
	int r;
	unsigned int exists, recent, expunge, unseen, uidnext, uidval;
	
	r = imap_threaded_noop(session->folder, &exists, &recent, &expunge, &unseen, &uidnext, &uidval);
	if (r != MAILIMAP_NO_ERROR) {
		imap_handle_error(SESSION(session), NULL, r);
		debug_print("noop err %d\n", r);
		return r;
	}

	if ((exists && exists != session->exists)
	 || (recent && recent != session->recent)
	 || (expunge && expunge != session->expunge)
	 || (unseen && unseen != session->unseen)) {
		session->folder_content_changed = TRUE;
	}
	if (uidnext != 0 && uidnext != session->uid_next) {
		session->uid_next = uidnext;
		session->folder_content_changed = TRUE;
	}
	if (uidval != 0 && uidval != session->uid_validity) {
		session->uid_validity = uidval;
		session->folder_content_changed = TRUE;
	}

	session->exists = exists;
	session->recent = recent;
	session->expunge = expunge;
	session->unseen = unseen;

	session_set_access_time(SESSION(session));

	return MAILIMAP_NO_ERROR;
}

#ifdef USE_GNUTLS
static gint imap_cmd_starttls(IMAPSession *session)
{
	int r;
	
	r = imap_threaded_starttls(session->folder, 
		SESSION(session)->server, SESSION(session)->port);
	if (r != MAILIMAP_NO_ERROR) {
		imap_handle_error(SESSION(session), NULL, r);
		debug_print("STARTTLS err %d\n", r);
		return r;
	}
	return MAILIMAP_NO_ERROR;
}
#endif

static gint imap_cmd_select(IMAPSession *session, const gchar *folder,
			    gint *exists, gint *recent, gint *unseen,
			    guint32 *uid_validity, gint *can_create_flags,
			    GSList **ok_flags, gboolean block)
{
	int r;

	r = imap_threaded_select(session->folder, folder,
				 exists, recent, unseen, uid_validity, can_create_flags, ok_flags);
	if (r != MAILIMAP_NO_ERROR) {
		imap_handle_error(SESSION(session), NULL, r);
		debug_print("select err %d\n", r);
		return r;
	}
	return MAILIMAP_NO_ERROR;
}

static gint imap_cmd_close(IMAPSession *session)
{
	int r;

	r = imap_threaded_close(session->folder);
	if (r != MAILIMAP_NO_ERROR) {
		imap_handle_error(SESSION(session), NULL, r);
		debug_print("close err %d\n", r);
		return r;
	}
	g_free(session->mbox);
	session->mbox = NULL;
	session->exists = 0;
	session->recent = 0;
	session->expunge = 0;
	return MAILIMAP_NO_ERROR;
}

static gint imap_cmd_examine(IMAPSession *session, const gchar *folder,
			     gint *exists, gint *recent, gint *unseen,
			     guint32 *uid_validity, gboolean block)
{
	int r;

	r = imap_threaded_examine(session->folder, folder,
				  exists, recent, unseen, uid_validity);
	if (r != MAILIMAP_NO_ERROR) {
		imap_handle_error(SESSION(session), NULL, r);
		debug_print("examine err %d\n", r);
		
		return r;
	}
	return MAILIMAP_NO_ERROR;
}

static gint imap_cmd_create(IMAPSession *session, const gchar *folder)
{
	int r;

	r = imap_threaded_create(session->folder, folder);
	if (r != MAILIMAP_NO_ERROR) {
		imap_handle_error(SESSION(session), NULL, r);
		return r;
	}

	return MAILIMAP_NO_ERROR;
}

static gint imap_cmd_rename(IMAPSession *session, const gchar *old_folder,
			    const gchar *new_folder)
{
	int r;

	r = imap_threaded_rename(session->folder, old_folder,
				 new_folder);
	if (r != MAILIMAP_NO_ERROR) {
		imap_handle_error(SESSION(session), NULL, r);
		return r;
	}

	return MAILIMAP_NO_ERROR;
}

static gint imap_cmd_delete(IMAPSession *session, const gchar *folder)
{
	int r;
	

	r = imap_threaded_delete(session->folder, folder);
	if (r != MAILIMAP_NO_ERROR) {
		imap_handle_error(SESSION(session), NULL, r);
		return r;
	}

	return MAILIMAP_NO_ERROR;
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
		imap_handle_error(SESSION(session), NULL, r);
		debug_print("fetch err %d\n", r);
		return GINT_TO_POINTER(r);
	}
	return GINT_TO_POINTER(MAILIMAP_NO_ERROR);
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

	if (prefs_common.work_offline && 
	    !inc_offline_should_override(FALSE,
		_("Claws Mail needs network access in order "
		  "to access the IMAP server."))) {
		g_free(data);
		return -1;
	}
	statusbar_print_all(_("Fetching message..."));
	result = GPOINTER_TO_INT(imap_cmd_fetch_thread(data));
	statusbar_pop_all();
	g_free(data);
	return result;
}


static gint imap_cmd_append(IMAPSession *session, 
			    IMAPFolderItem *item,
			    const gchar *destfolder,
			    const gchar *file, IMAPFlags flags, 
			    guint32 *new_uid)
{
	struct mailimap_flag_list * flag_list;
	int r;
	
	cm_return_val_if_fail(file != NULL, MAILIMAP_ERROR_BAD_STATE);

	flag_list = imap_flag_to_lep(item, flags, NULL);
	lock_session(session);
	r = imap_threaded_append(session->folder, destfolder,
			 file, flag_list, (int *)new_uid);
	mailimap_flag_list_free(flag_list);

	if (r != MAILIMAP_NO_ERROR) {
		imap_handle_error(SESSION(session), NULL, r);
		debug_print("append err %d\n", r);
		return r;
	}

	unlock_session(session);

	return MAILIMAP_NO_ERROR;
}

static gint imap_cmd_copy(IMAPSession *session, struct mailimap_set * set,
			  const gchar *destfolder,
			  struct mailimap_set **source, struct mailimap_set **dest)
{
	int r;
	
	g_return_val_if_fail(session != NULL, MAILIMAP_ERROR_BAD_STATE);
	g_return_val_if_fail(set != NULL, MAILIMAP_ERROR_BAD_STATE);
	g_return_val_if_fail(destfolder != NULL, MAILIMAP_ERROR_BAD_STATE);

	r = imap_threaded_copy(session->folder, set, destfolder, source, dest);
	if (r != MAILIMAP_NO_ERROR) {
		imap_handle_error(SESSION(session), NULL, r);
		return r;
	}

	return MAILIMAP_NO_ERROR;
}

static gint imap_cmd_store(IMAPSession *session, 
			   IMAPFolderItem *item,
			   struct mailimap_set * set,
			   IMAPFlags flags, GSList *tags, int do_add)
{
	int r;
	struct mailimap_flag_list * flag_list = NULL;
	struct mailimap_store_att_flags * store_att_flags;
	
	flag_list = imap_flag_to_lep(item, flags, tags);

	if (do_add)
		store_att_flags =
			mailimap_store_att_flags_new_add_flags_silent(flag_list);
	else
		store_att_flags =
			mailimap_store_att_flags_new_remove_flags_silent(flag_list);
	
	r = imap_threaded_store(session->folder, set, store_att_flags);
	mailimap_store_att_flags_free(store_att_flags);
	if (r != MAILIMAP_NO_ERROR) {
		imap_handle_error(SESSION(session), NULL, r);
		return r;
	}
	
	return MAILIMAP_NO_ERROR;
}

static gint imap_cmd_expunge(IMAPSession *session)
{
	int r;
	
	if (prefs_common.work_offline && 
	    !inc_offline_should_override(FALSE,
		_("Claws Mail needs network access in order "
		  "to access the IMAP server."))) {
		return -1;
	}

	statusbar_print_all(_("Expunging deleted messages..."));
	r = imap_threaded_expunge(session->folder);
	statusbar_pop_all();
	if (r != MAILIMAP_NO_ERROR) {
		imap_handle_error(SESSION(session), NULL, r);
		return r;
	}

	return MAILIMAP_NO_ERROR;
}

gint imap_expunge(Folder *folder, FolderItem *item)
{
	IMAPSession *session = imap_session_get(folder);
	if (session == NULL)
		return -1;
	
	return imap_cmd_expunge(session);
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

static gboolean imap_rename_folder_func(GNode *node, gpointer data)
{
	FolderItem *item = node->data;
	gchar **paths = data;
	const gchar *oldpath = paths[0];
	const gchar *newpath = paths[1];
	gchar *real_oldpath, *real_newpath;
	gchar *base;
	gchar *new_itempath;
	gint oldpathlen;
	IMAPSession *session = imap_session_get(item->folder);
	gint ok = MAILIMAP_NO_ERROR;
	oldpathlen = strlen(oldpath);
	if (strncmp(oldpath, item->path, oldpathlen) != 0) {
		g_warning("path doesn't match: %s, %s", oldpath, item->path);
		return TRUE;
	}

	base = item->path + oldpathlen;
	while (*base == '/') base++;
	if (*base == '\0')
		new_itempath = g_strdup(newpath);
	else
		new_itempath = g_strconcat(newpath, "/", base,
					   NULL);

	real_oldpath = imap_get_real_path(session, IMAP_FOLDER(item->folder), item->path, &ok);
	g_free(item->path);
	item->path = new_itempath;
	
	real_newpath = imap_get_real_path(session, IMAP_FOLDER(item->folder), item->path, &ok);
	
	imap_threaded_subscribe(item->folder, real_oldpath, FALSE);
	imap_threaded_subscribe(item->folder, real_newpath, TRUE);

	g_free(real_oldpath);
	g_free(real_newpath);
	return FALSE;
}

static gint get_list_of_uids(IMAPSession *session, Folder *folder, IMAPFolderItem *item, GSList **msgnum_list)
{
	GSList *uidlist, *elem;
	int r = -1;
	clist * lep_uidlist;
	gint ok, nummsgs = 0;

	if (session == NULL) {
		return -1;
	}

	ok = imap_select(session, IMAP_FOLDER(folder), FOLDER_ITEM(item),
			 NULL, NULL, NULL, NULL, NULL, TRUE);
	if (ok != MAILIMAP_NO_ERROR) {
		return -1;
	}

	g_slist_free(item->uid_list);
	item->uid_list = NULL;

	uidlist = NULL;
	
	if (folder->account && folder->account->low_bandwidth) {
		r = imap_threaded_search(folder, IMAP_SEARCH_TYPE_SIMPLE,
				NULL, NULL, NULL, &lep_uidlist);
	}
	
	if (r == MAILIMAP_NO_ERROR) {
		uidlist = imap_uid_list_from_lep(lep_uidlist, NULL);
		mailimap_search_result_free(lep_uidlist);
	} else {
		carray * lep_uidtab;
		if (r != -1) { /* inited */
			imap_handle_error(SESSION(session), NULL, r);
			if (is_fatal(r))
				return -1;
		}
		r = imap_threaded_fetch_uid(folder, 1,
				    &lep_uidtab);
		if (r == MAILIMAP_NO_ERROR) {
			uidlist = imap_uid_list_from_lep_tab(lep_uidtab);
			imap_fetch_uid_list_free(lep_uidtab);
		}
	}
	
	if (r != MAILIMAP_NO_ERROR) {
		imap_handle_error(SESSION(session), NULL, r);
		return -1;
	}

	for (elem = uidlist; elem != NULL; elem = g_slist_next(elem)) {
		guint msgnum;

		msgnum = GPOINTER_TO_INT(elem->data);

		*msgnum_list = g_slist_prepend(*msgnum_list, GINT_TO_POINTER(msgnum));
		item->uid_list = g_slist_prepend(item->uid_list, GINT_TO_POINTER(msgnum));
		nummsgs++;
	}
	g_slist_free(uidlist);

	return nummsgs;

}

gint imap_get_num_list(Folder *folder, FolderItem *_item, GSList **msgnum_list, gboolean *old_uids_valid)
{
	IMAPFolderItem *item = (IMAPFolderItem *)_item;
	IMAPSession *session;
	gint nummsgs;
	GSList *uidlist = NULL;
	gchar *dir;
	gint known_list_len = 0;
	gchar *path;

	debug_print("get_num_list\n");
	
	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(item != NULL, -1);
	g_return_val_if_fail(item->item.path != NULL, -1);
	g_return_val_if_fail(FOLDER_CLASS(folder) == &imap_class, -1);
	g_return_val_if_fail(folder->account != NULL, -1);

	known_list_len = g_slist_length(item->uid_list);
	if (!item->should_update) {
		debug_print("get_num_list: nothing to update\n");
		*old_uids_valid = TRUE;
		if (known_list_len == item->item.total_msgs
		 && known_list_len > 0) {
			*msgnum_list = g_slist_copy(item->uid_list);
			return known_list_len;
		} else {
			debug_print("don't know the list length...\n");
		}
	}

	if (prefs_common.work_offline && 
	    !inc_offline_should_override(FALSE,
		_("Claws Mail needs network access in order "
		  "to access the IMAP server."))) {
		return -1;
	}
	
	path = folder_item_get_path(_item);
	if (!is_dir_exist(path)) {
		if(is_file_exist(path))
			claws_unlink(path);
		make_dir_hier(path);
	}
	g_free(path);

	debug_print("getting session...\n");
	session = imap_session_get(folder);
	g_return_val_if_fail(session != NULL, -1);

	lock_session(session);
	if (FOLDER_ITEM(item)->path) 
		statusbar_print_all(_("Scanning folder %s/%s..."),
				      FOLDER_ITEM(item)->folder->name, 
				      FOLDER_ITEM(item)->path);
	else
		statusbar_print_all(_("Scanning folder %s..."),
				      FOLDER_ITEM(item)->folder->name);

	if (item->should_trash_cache) {
		*old_uids_valid = FALSE;
		debug_print("get_num_list: trashing num list\n");
		debug_print("Freeing imap uid cache\n");
		item->lastuid = 0;
		g_slist_free(item->uid_list);
		item->uid_list = NULL;

		imap_delete_all_cached_messages((FolderItem *)item);
	} else {
		debug_print("get_num_list: updating num list\n");
		*old_uids_valid = TRUE;
	}

	nummsgs = get_list_of_uids(session, folder, item, &uidlist);

	unlock_session(session);

	/* session could be broken now, in case of fatal error */

	debug_print("get_num_list: got %d msgs\n", nummsgs);

	if (nummsgs < 0) {
		statusbar_pop_all();
		return -1;
	}

	*msgnum_list = uidlist;

	dir = folder_item_get_path((FolderItem *)item);
	debug_print("removing old messages from %s\n", dir);
	remove_numbered_files_not_in_list(dir, *msgnum_list);
	g_free(dir);
	
	debug_print("get_num_list - ok - %i\n", nummsgs);
	statusbar_pop_all();
	item->should_trash_cache = FALSE;
	item->should_update = FALSE;
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

	debug_print("getting session...\n");
	session = imap_session_get(folder);
	g_return_val_if_fail(session != NULL, NULL);

	lock_session(session); /* unlocked later in the function */

	debug_print("IMAP getting msginfos\n");
	ok = imap_select(session, IMAP_FOLDER(folder), item,
			 NULL, NULL, NULL, NULL, NULL, FALSE);
	if (ok != MAILIMAP_NO_ERROR) {
		return NULL;
	}
	if (!(folder_has_parent_of_type(item, F_DRAFT) || 
	      folder_has_parent_of_type(item, F_QUEUE))) {
		ret = g_slist_concat(ret,
			imap_get_uncached_messages(session, item,
						   msgnum_list, &ok));
		if (ok != MAILIMAP_NO_ERROR)
			return NULL;
		unlock_session(session);
	} else {
		MsgNumberList *sorted_list, *elem, *llast = NULL;
		gint startnum, lastnum;
	
		unlock_session(session);

 		sorted_list = g_slist_sort(g_slist_copy(msgnum_list), g_int_compare);

		startnum = lastnum = GPOINTER_TO_INT(sorted_list->data);

		llast = g_slist_last(ret);
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
							if (llast == NULL)
								llast = ret = g_slist_append(ret, msginfo);
							else {
								llast = g_slist_append(llast, msginfo);
								llast = llast->next;
							}
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

gboolean imap_scan_required(Folder *folder, FolderItem *_item)
{
	IMAPSession *session;
	IMAPFolderItem *item = (IMAPFolderItem *)_item;
	gint ok, exists = 0, unseen = 0;
	guint32 uid_next = 0, uid_val = 0;
	gboolean selected_folder;
	
	g_return_val_if_fail(folder != NULL, FALSE);
	g_return_val_if_fail(item != NULL, FALSE);
	g_return_val_if_fail(item->item.folder != NULL, FALSE);
	g_return_val_if_fail(FOLDER_CLASS(item->item.folder) == &imap_class, FALSE);

	if (item->item.path == NULL)
		return FALSE;

	if (item->should_update) {
		debug_print("scan already required\n");
		return TRUE;
	}
	debug_print("getting session...\n");
	session = imap_session_get(folder);
	
	g_return_val_if_fail(session != NULL, FALSE);
	lock_session(session); /* unlocked later in the function */

	selected_folder = (session->mbox != NULL) &&
			  (!strcmp(session->mbox, item->item.path));
	if (selected_folder) {
		if (!session->folder_content_changed) {
			ok = imap_cmd_noop(session);
			if (ok != MAILIMAP_NO_ERROR) {
				debug_print("disconnected!\n");
				if (!is_fatal(ok))
					session = imap_reconnect_if_possible(folder, session);
				else
					session = imap_session_get(folder);
				if (session == NULL)
					return FALSE;
			}

			if (session->folder_content_changed) {
				debug_print("CHANGED (self-noop)! scan_required\n");
				item->should_update = TRUE;
				if (session->uid_validity && session->uid_validity != item->item.mtime) {
					item->item.mtime = session->uid_validity;
					item->should_trash_cache = TRUE;
				}
				unlock_session(session);
				return TRUE;
			}
		} else {
			debug_print("CHANGED (previous noop)! scan_required\n");
			item->should_update = TRUE;
			if (session->uid_validity && session->uid_validity != item->item.mtime) {
				item->item.mtime = session->uid_validity;
				item->should_trash_cache = TRUE;
			}
			unlock_session(session);
			return TRUE;
		}
	} else {
		ok = imap_status(session, IMAP_FOLDER(folder), item->item.path, IMAP_FOLDER_ITEM(item),
				 &exists, &uid_next, &uid_val, &unseen, FALSE);
		if (ok != MAILIMAP_NO_ERROR) {
			return FALSE;
		}
		
		debug_print("exists %d, item->item.total_msgs %d\n"
			    "\tunseen %d, item->item.unread_msgs %d\n"
			    "\tuid_next %d, item->uid_next %d\n"
			    "\tuid_val %d, item->item.mtime %ld\n",
			    exists, item->item.total_msgs, unseen, item->item.unread_msgs,
			    uid_next, item->uid_next, uid_val, (long)(item->item.mtime));
		if (exists != item->item.total_msgs
		    || unseen != item->item.unread_msgs 
		    || uid_next != item->uid_next
		    || uid_val != item->item.mtime) {
			debug_print("CHANGED (status)! scan_required\n");
			item->last_change = time(NULL);
			item->should_update = TRUE;
			item->uid_next = uid_next;
			if (uid_val != item->item.mtime) {
				item->item.mtime = uid_val;
				item->should_trash_cache = TRUE;
			}
			unlock_session(session);
			return TRUE;
		}
	}
	unlock_session(session);

	item->should_update = FALSE;
	return FALSE;
}

void imap_change_flags(Folder *folder, FolderItem *item, MsgInfo *msginfo, MsgPermFlags newflags)
{
	IMAPSession *session;
	IMAPFlags flags_set = 0, flags_unset = 0;
	gint ok = MAILIMAP_NO_ERROR;
	MsgNumberList numlist;
	hashtable_data *ht_data = NULL;

	g_return_if_fail(folder != NULL);
	g_return_if_fail(folder->klass == &imap_class);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder == folder);
	g_return_if_fail(msginfo != NULL);
	g_return_if_fail(msginfo->folder == item);

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

	if (!MSG_IS_FORWARDED(msginfo->flags) &&  (newflags & MSG_FORWARDED))
		flags_set |= IMAP_FLAG_FORWARDED;
	if ( MSG_IS_FORWARDED(msginfo->flags) && !(newflags & MSG_FORWARDED))
		flags_unset |= IMAP_FLAG_FORWARDED;

	if (!MSG_IS_SPAM(msginfo->flags) &&  (newflags & MSG_SPAM)) {
		flags_set |= IMAP_FLAG_SPAM;
		flags_unset |= IMAP_FLAG_HAM;
	}
	if ( MSG_IS_SPAM(msginfo->flags) && !(newflags & MSG_SPAM)) {
		flags_set |= IMAP_FLAG_HAM;
		flags_unset |= IMAP_FLAG_SPAM;
	}
	if (!MSG_IS_DELETED(msginfo->flags) &&  (newflags & MSG_DELETED))
		flags_set |= IMAP_FLAG_DELETED;
	if ( MSG_IS_DELETED(msginfo->flags) && !(newflags & MSG_DELETED))
		flags_unset |= IMAP_FLAG_DELETED;

	if (!flags_set && !flags_unset) {
		/* the changed flags were not translatable to IMAP-speak.
		 * like MSG_POSTFILTERED, so just apply. */
		msginfo->flags.perm_flags = newflags;
		return;
	}

	debug_print("getting session...\n");
	session = imap_session_get(folder);
	if (!session) {
		return;
	}

	if ((ok = imap_select(session, IMAP_FOLDER(folder), msginfo->folder,
	    NULL, NULL, NULL, NULL, NULL, FALSE)) != MAILIMAP_NO_ERROR) {
		return;
	}
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
				ht_data->item = IMAP_FOLDER_ITEM(item);
				g_hash_table_insert(IMAP_FOLDER_ITEM(item)->flags_set_table, 
					GINT_TO_POINTER(flags_set), ht_data);
			}
			ht_data->msglist = g_slist_prepend(ht_data->msglist, GINT_TO_POINTER(msginfo->msgnum));
		} 
		if (flags_unset) {
			ht_data = g_hash_table_lookup(IMAP_FOLDER_ITEM(item)->flags_unset_table, 
				GINT_TO_POINTER(flags_unset));
			if (ht_data == NULL) {
				ht_data = g_new0(hashtable_data, 1);
				ht_data->item = IMAP_FOLDER_ITEM(item);
				g_hash_table_insert(IMAP_FOLDER_ITEM(item)->flags_unset_table, 
					GINT_TO_POINTER(flags_unset), ht_data);
			}
			ht_data->msglist = g_slist_prepend(ht_data->msglist, 
					GINT_TO_POINTER(msginfo->msgnum));		
		}
	} else {
		debug_print("IMAP changing flags\n");
		if (flags_set) {
			ok = imap_set_message_flags(session, IMAP_FOLDER_ITEM(item), &numlist, flags_set, NULL, TRUE);
			if (ok != MAILIMAP_NO_ERROR) {
				return;
			}
		}

		if (flags_unset) {
			ok = imap_set_message_flags(session, IMAP_FOLDER_ITEM(item), &numlist, flags_unset, NULL, FALSE);
			if (ok != MAILIMAP_NO_ERROR) {
				return;
			}
		}
	}
	msginfo->flags.perm_flags = newflags;
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

	debug_print("getting session...\n");
	session = imap_session_get(folder);
	if (!session) return -1;

	ok = imap_select(session, IMAP_FOLDER(folder), item,
			 NULL, NULL, NULL, NULL, NULL, FALSE);
	if (ok != MAILIMAP_NO_ERROR) {
		return ok;
	}
	numlist.next = NULL;
	numlist.data = GINT_TO_POINTER(uid);
	
	ok = imap_set_message_flags
		(session, IMAP_FOLDER_ITEM(item), &numlist, IMAP_FLAG_DELETED, NULL, TRUE);
	if (ok != MAILIMAP_NO_ERROR) {
		log_warning(LOG_PROTOCOL, _("can't set deleted flags: %d\n"), uid);
		return ok;
	}

	ok = imap_cmd_expunge(session);

	if (ok != MAILIMAP_NO_ERROR) {
		log_warning(LOG_PROTOCOL, _("can't expunge\n"));
		return ok;
	}

	IMAP_FOLDER_ITEM(item)->uid_list = g_slist_remove(
	    IMAP_FOLDER_ITEM(item)->uid_list, numlist.data);
	dir = folder_item_get_path(item);
	if (is_dir_exist(dir))
		remove_numbered_files(dir, uid, uid);
	g_free(dir);
	return MAILIMAP_NO_ERROR;
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

static gboolean flag_ok(IMAPFolderItem *item, guint flag)
{
	if (item->ok_flags && g_slist_find(item->ok_flags, GUINT_TO_POINTER(flag))) {
		debug_print("flag %d is OK\n", flag);
		return TRUE;
	}
	if (item->can_create_flags == ITEM_CAN_CREATE_FLAGS) {
		debug_print("creating flags is OK\n");
		return TRUE;
	}
	return FALSE;
}

/*
 * NEW and DELETED flags are not syncronized
 * - The NEW/RECENT flags in IMAP folders can not really be directly
 *   modified by Claws Mail
 * - The DELETE/DELETED flag in IMAP and Claws Mail do not have the same
 *   meaning, in IMAP it always removes the messages from the FolderItem
 *   in Claws Mail it can mean to move the message to Trash
 */

typedef struct _get_flags_data {
	Folder *folder;
	FolderItem *item;
	MsgInfoList *msginfo_list;
	GHashTable *msgflags;
	gboolean full_search;
	gboolean done;
} get_flags_data;

static /*gint*/ void *imap_get_flags_thread(void *data)
{
	get_flags_data *stuff = (get_flags_data *)data;
	Folder *folder = stuff->folder;
	FolderItem *fitem = (FolderItem *) stuff->item;
	MsgInfoList *msginfo_list = stuff->msginfo_list;
	GHashTable *msgflags = stuff->msgflags;
	GSList *elem;
	carray * lep_uidtab;
	IMAPSession *session;
	gint ok;
	int r = MAILIMAP_NO_ERROR;
	GHashTable *flags_hash = NULL;
	GHashTable *tags_hash = NULL;
	gboolean full_search = stuff->full_search;
	GSList *sorted_list = NULL;
	GSList *unseen = NULL, *answered = NULL, *flagged = NULL, *deleted = NULL, *forwarded = NULL, *spam = NULL;
	GSList *seq_list, *cur;
	gboolean reverse_seen = FALSE;
	gboolean selected_folder;
	gint exists_cnt, unseen_cnt;
	gboolean got_alien_tags = FALSE;

	session = imap_session_get(folder);

	if (session == NULL) {
		stuff->done = TRUE;
		return GINT_TO_POINTER(-1);
	}
	selected_folder = (session->mbox != NULL) &&
			  (!strcmp(session->mbox, fitem->path));

	lock_session(session);
	if (!selected_folder) {
		ok = imap_select(session, IMAP_FOLDER(folder), fitem,
			&exists_cnt, NULL, &unseen_cnt, NULL, NULL, TRUE);
		if (ok != MAILIMAP_NO_ERROR) {
			stuff->done = TRUE;
			return GINT_TO_POINTER(-1);
		}

		if (unseen_cnt > exists_cnt / 2)
			reverse_seen = TRUE;
	} 
	else {
		if (fitem->unread_msgs > fitem->total_msgs / 2)
			reverse_seen = TRUE;
	}

	sorted_list = g_slist_sort(g_slist_copy(msginfo_list), compare_msginfo);
	if (!full_search) {
		seq_list = imap_get_lep_set_from_msglist(IMAP_FOLDER(folder), msginfo_list);
	} else {
		struct mailimap_set * set;
		set = mailimap_set_new_interval(1, 0);
		seq_list = g_slist_append(NULL, set);
	}

	if (folder->account && folder->account->low_bandwidth) {
		for (cur = seq_list; cur != NULL; cur = g_slist_next(cur)) {
			struct mailimap_set * imapset;
			clist * lep_uidlist;
			int r;

			imapset = cur->data;
			if (reverse_seen) {
				r = imap_threaded_search(folder, IMAP_SEARCH_TYPE_SEEN, NULL,
							 NULL, full_search ? NULL:imapset, &lep_uidlist);
			}
			else {
				r = imap_threaded_search(folder,
							 IMAP_SEARCH_TYPE_UNSEEN, NULL,
							 NULL, full_search ? NULL:imapset, &lep_uidlist);
			}
			if (r == MAILIMAP_NO_ERROR) {
				GSList * uidlist;

				uidlist = imap_uid_list_from_lep(lep_uidlist, NULL);
				mailimap_search_result_free(lep_uidlist);

				unseen = g_slist_concat(unseen, uidlist);
			} else {
				imap_handle_error(SESSION(session), NULL, r);
				goto bail;
			}

			r = imap_threaded_search(folder, IMAP_SEARCH_TYPE_FLAGGED, NULL,
						 NULL, full_search ? NULL:imapset, &lep_uidlist);
			if (r == MAILIMAP_NO_ERROR) {
				GSList * uidlist;

				uidlist = imap_uid_list_from_lep(lep_uidlist, NULL);
				mailimap_search_result_free(lep_uidlist);

				flagged = g_slist_concat(flagged, uidlist);
			} else {
				imap_handle_error(SESSION(session), NULL, r);
				goto bail;
			}

			if (fitem->opened || fitem->processing_pending || fitem == folder->inbox) {
				r = imap_threaded_search(folder, IMAP_SEARCH_TYPE_ANSWERED, NULL,
							 NULL, full_search ? NULL:imapset, &lep_uidlist);
				if (r == MAILIMAP_NO_ERROR) {
					GSList * uidlist;

					uidlist = imap_uid_list_from_lep(lep_uidlist, NULL);
					mailimap_search_result_free(lep_uidlist);

					answered = g_slist_concat(answered, uidlist);
				} else {
					imap_handle_error(SESSION(session), NULL, r);
					goto bail;
				}

				if (flag_ok(IMAP_FOLDER_ITEM(fitem), IMAP_FLAG_FORWARDED)) {
					r = imap_threaded_search(folder, IMAP_SEARCH_TYPE_FORWARDED, NULL,
								 NULL, full_search ? NULL:imapset, &lep_uidlist);
					if (r == MAILIMAP_NO_ERROR) {
						GSList * uidlist;

						uidlist = imap_uid_list_from_lep(lep_uidlist, NULL);
						mailimap_search_result_free(lep_uidlist);

						forwarded = g_slist_concat(forwarded, uidlist);
					} else {
						imap_handle_error(SESSION(session), NULL, r);
						goto bail;
					}
				}

				if (flag_ok(IMAP_FOLDER_ITEM(fitem), IMAP_FLAG_SPAM)) {
					r = imap_threaded_search(folder, IMAP_SEARCH_TYPE_SPAM, NULL,
								 NULL, full_search ? NULL:imapset, &lep_uidlist);
					if (r == MAILIMAP_NO_ERROR) {
						GSList * uidlist;

						uidlist = imap_uid_list_from_lep(lep_uidlist, NULL);
						mailimap_search_result_free(lep_uidlist);

						spam = g_slist_concat(spam, uidlist);
					} else {
						imap_handle_error(SESSION(session), NULL, r);
						goto bail;
					}
				}

				r = imap_threaded_search(folder, IMAP_SEARCH_TYPE_DELETED, NULL,
							 NULL, full_search ? NULL:imapset, &lep_uidlist);
				if (r == MAILIMAP_NO_ERROR) {
					GSList * uidlist;

					uidlist = imap_uid_list_from_lep(lep_uidlist, NULL);
					mailimap_search_result_free(lep_uidlist);

					deleted = g_slist_concat(deleted, uidlist);
				} else {
					imap_handle_error(SESSION(session), NULL, r);
					goto bail;
				}
			}
		}

	} else {
		r = imap_threaded_fetch_uid_flags(folder, 1, &lep_uidtab);
		if (r == MAILIMAP_NO_ERROR) {
			flags_hash = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);
			tags_hash = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);
			imap_flags_hash_from_lep_uid_flags_tab(lep_uidtab, flags_hash, tags_hash);
			imap_fetch_uid_flags_list_free(lep_uidtab);
		} else {
			imap_handle_error(SESSION(session), NULL, r);
			goto bail;
		}
	}

bail:
	if (r == MAILIMAP_NO_ERROR)
		unlock_session(session);
	
	for (elem = sorted_list; elem != NULL; elem = g_slist_next(elem)) {
		MsgInfo *msginfo;
		MsgPermFlags flags, oldflags;
		gboolean wasnew;

		msginfo = (MsgInfo *) elem->data;
		flags = msginfo->flags.perm_flags;
		wasnew = (flags & MSG_NEW);
		oldflags = flags & ~(MSG_NEW|MSG_UNREAD|MSG_REPLIED|MSG_FORWARDED|MSG_MARKED|MSG_DELETED|MSG_SPAM);

		if (folder->account && folder->account->low_bandwidth) {
			if (fitem->opened || fitem->processing_pending || fitem == folder->inbox) {
				flags &= ~((reverse_seen ? 0 : MSG_UNREAD | MSG_NEW) | MSG_REPLIED | MSG_FORWARDED | MSG_MARKED | MSG_SPAM);
			} else {
				flags &= ~((reverse_seen ? 0 : MSG_UNREAD | MSG_NEW | MSG_MARKED));
			}
			if (reverse_seen)
				flags |= MSG_UNREAD | (wasnew ? MSG_NEW : 0);
			if (gslist_find_next_num(&unseen, msginfo->msgnum) == msginfo->msgnum) {
				if (!reverse_seen) {
					flags |= MSG_UNREAD | (wasnew ? MSG_NEW : 0);
				} else {
					flags &= ~(MSG_UNREAD | MSG_NEW);
				}
			}

			if (gslist_find_next_num(&flagged, msginfo->msgnum) == msginfo->msgnum)
				flags |= MSG_MARKED;
			else
				flags &= ~MSG_MARKED;

			if (fitem->opened || fitem->processing_pending || fitem == folder->inbox) {
				if (gslist_find_next_num(&answered, msginfo->msgnum) == msginfo->msgnum)
					flags |= MSG_REPLIED;
				else
					flags &= ~MSG_REPLIED;
				if (gslist_find_next_num(&forwarded, msginfo->msgnum) == msginfo->msgnum)
					flags |= MSG_FORWARDED;
				else
					flags &= ~MSG_FORWARDED;
				if (gslist_find_next_num(&spam, msginfo->msgnum) == msginfo->msgnum)
					flags |= MSG_SPAM;
				else
					flags &= ~MSG_SPAM;
				if (gslist_find_next_num(&deleted, msginfo->msgnum) == msginfo->msgnum)
					flags |= MSG_DELETED;
				else
					flags &= ~MSG_DELETED;
			}
		} else {
			if (flags_hash != NULL) {

				flags = GPOINTER_TO_INT(g_hash_table_lookup(flags_hash, 
						GINT_TO_POINTER(msginfo->msgnum)));
			}

			if ((flags & MSG_UNREAD) == 0)
				flags &= ~MSG_NEW;
			else if (wasnew)
				flags |= MSG_NEW;
			flags |= oldflags;
			
			if (tags_hash != NULL) {
				GSList *tags = g_hash_table_lookup(tags_hash, GINT_TO_POINTER(msginfo->msgnum));
				GSList *cur;

				g_slist_free(msginfo->tags);
				msginfo->tags = NULL;

				for (cur = tags; cur; cur = cur->next) {
					gchar *real_tag = imap_modified_utf7_to_utf8(cur->data, TRUE);
					gint id = 0;
					id = tags_get_id_for_str(real_tag);
					if (id == -1) {
						id = tags_add_tag(real_tag);
						got_alien_tags = TRUE;
					}
					msginfo->tags = g_slist_append(
								msginfo->tags,
								GINT_TO_POINTER(id));
					g_free(real_tag);
				}
				slist_free_strings_full(tags);
			}
		}

		g_hash_table_insert(msgflags, msginfo, GINT_TO_POINTER(flags));
	}
	
	if (got_alien_tags) {
		tags_write_tags();
		main_window_reflect_tags_changes(mainwindow_get_mainwindow());
	}

	if (flags_hash)
		g_hash_table_destroy(flags_hash);
	if (tags_hash)
		g_hash_table_destroy(tags_hash);

	imap_lep_set_free(seq_list);
	g_slist_free(flagged);
	g_slist_free(deleted);
	g_slist_free(answered);
	g_slist_free(forwarded);
	g_slist_free(spam);
	g_slist_free(unseen);
	g_slist_free(sorted_list);

	stuff->done = TRUE;
	return GINT_TO_POINTER(0);
}

static gint imap_get_flags(Folder *folder, FolderItem *item,
                           MsgInfoList *msginfo_list, GHashTable *msgflags)
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
	
	if (prefs_common.work_offline && 
	    !inc_offline_should_override(FALSE,
		_("Claws Mail needs network access in order "
		  "to access the IMAP server."))) {
		g_free(data);
		return -1;
	}

	tmp = folder_item_get_msg_list(item);

	if (g_slist_length(tmp) <= g_slist_length(msginfo_list))
		data->full_search = TRUE;
	
	for (cur = tmp; cur; cur = cur->next)
		procmsg_msginfo_free((MsgInfo **)&(cur->data));
	
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
	gint ok = MAILIMAP_ERROR_BAD_STATE;
	IMAPSession *session = NULL;
	
	debug_print("getting session...\n");
	session = imap_session_get(item->folder);

	data->msglist = g_slist_reverse(data->msglist);
	
	debug_print("IMAP %ssetting flags to %d for %d messages\n",
		flags_set?"":"un",
		flags_value,
		g_slist_length(data->msglist));
	
	lock_session(session);
	if (session) {
		ok = imap_select(session, IMAP_FOLDER(item->folder), item,
			 NULL, NULL, NULL, NULL, NULL, FALSE);
	}
	if (ok == MAILIMAP_NO_ERROR) {
		ok = imap_set_message_flags(session, IMAP_FOLDER_ITEM(item),
			data->msglist, flags_value, NULL, flags_set);
	} else {
		g_warning("can't select mailbox %s", item->path);
	}

	if (!is_fatal(ok))
		unlock_session(session);

	g_slist_free(data->msglist);	
	g_free(data);
	return TRUE;
}

static gboolean process_tags(gpointer key, gpointer value, gpointer user_data)
{
	gboolean tags_set = GPOINTER_TO_INT(user_data);
	TagsData *data = (TagsData *)value;
	IMAPFolderItem *_item = data->item;
	FolderItem *item = (FolderItem *)_item;
	gchar *str = data->str;
	gint ok = MAILIMAP_ERROR_BAD_STATE;
	IMAPSession *session = NULL;
	
	debug_print("getting session...\n");
	session = imap_session_get(item->folder);

	data->msglist = g_slist_reverse(data->msglist);
	
	debug_print("IMAP %ssetting tags %s for %d messages\n",
		tags_set?"":"un",
		str,
		g_slist_length(data->msglist));
	
	lock_session(session);
	if (session) {
		ok = imap_select(session, IMAP_FOLDER(item->folder), item,
			 NULL, NULL, NULL, NULL, NULL, FALSE);
	}
	if (ok == MAILIMAP_NO_ERROR) {
		GSList list;
		list.data = str;
		list.next = NULL;
		ok = imap_set_message_flags(session, IMAP_FOLDER_ITEM(item),
			data->msglist, 0, &list, tags_set);
	} else {
		g_warning("can't select mailbox %s", item->path);
	}

	if (!is_fatal(ok))
		unlock_session(session);

	g_slist_free(data->msglist);	
	g_free(data->str);
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
	if (item->tags_set_table) {
		g_hash_table_foreach_remove(item->tags_set_table, process_tags, GINT_TO_POINTER(TRUE));
		g_hash_table_destroy(item->tags_set_table);
		item->tags_set_table = NULL;
	}
	if (item->tags_unset_table) {
		g_hash_table_foreach_remove(item->tags_unset_table, process_tags, GINT_TO_POINTER(FALSE));
		g_hash_table_destroy(item->tags_unset_table);
		item->tags_unset_table = NULL;
	}
	
}

static void imap_set_batch (Folder *folder, FolderItem *_item, gboolean batch)
{
	IMAPFolderItem *item = (IMAPFolderItem *)_item;
	IMAPSession *session;

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
		if (!item->tags_set_table) {
			item->tags_set_table = g_hash_table_new(NULL, g_direct_equal);
		}
		if (!item->tags_unset_table) {
			item->tags_unset_table = g_hash_table_new(NULL, g_direct_equal);
		}
		session = imap_session_get(folder);
		if (session) {
			imap_refresh_sensitivity(session);
			session->sens_update_block = TRUE;
		}
	} else {
		debug_print("IMAP switching away from batch mode\n");
		/* process stuff */
		process_hashtable(item);
		item->batching = FALSE;
		session = imap_session_get(folder);
		if (session) {
			session->sens_update_block = FALSE;
			imap_refresh_sensitivity(session);
		}
	}
}



/* data types conversion libetpan <-> claws */



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
  
  if (imap_flags->mbf_oflags) {
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
  }  
  return flags;
}

static GSList * imap_list_from_lep(IMAPFolder * folder,
				   clist * list, const gchar * real_path, gboolean all)
{
	clistiter * iter;
	GSList * item_list = NULL, *llast = NULL;
	
	if (list) {
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
			if (!all && path_cmp(name, real_path) == 0) {
				g_free(base);
				free(dup_name);
				continue;
			}

			if (!all && dup_name[strlen(dup_name)-1] == '/') {
				g_free(base);
				free(dup_name);
				continue;
			}

			loc_name = imap_modified_utf7_to_utf8(base, FALSE);
			loc_path = imap_modified_utf7_to_utf8(dup_name, FALSE);

			new_item = folder_item_new(FOLDER(folder), loc_name, loc_path);
			if ((flags & ETPAN_IMAP_MB_NOINFERIORS) != 0)
				new_item->no_sub = TRUE;
			if (strcasecmp(dup_name, "INBOX") != 0 &&
			    ((flags & ETPAN_IMAP_MB_NOSELECT) != 0))
				new_item->no_select = TRUE;

			if (item_list == NULL)
				llast = item_list = g_slist_append(item_list, new_item);
			else {
				llast = g_slist_append(llast, new_item);
				llast = llast->next;
			}
			debug_print("folder '%s' found.\n", loc_path);
			g_free(base);
			g_free(loc_path);
			g_free(loc_name);

			free(dup_name);
		}
	}	
	return item_list;
}

static GSList * imap_get_lep_set_from_numlist(IMAPFolder *folder, MsgNumberList *numlist)
{
	GSList *sorted_list, *cur;
	guint first, last, next;
	GSList *ret_list = NULL, *llast = NULL;
	struct mailimap_set * current_set;
	unsigned int item_count;
	
	if (numlist == NULL)
		return NULL;
	
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

		if (last + 1 != next || next == 0 || item_count >= folder->max_set_size) {

			struct mailimap_set_item * item;
			item = mailimap_set_item_new(first, last);
			mailimap_set_add(current_set, item);
			
			first = next;

			if (item_count >= folder->max_set_size) {
				if (ret_list == NULL)
					llast = ret_list = g_slist_append(ret_list,
							  current_set);
				else {
					llast = g_slist_append(llast, current_set);
					llast = llast->next;
				}

				current_set = mailimap_set_new_empty();
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

static GSList * imap_get_lep_set_from_msglist(IMAPFolder *folder, MsgInfoList *msglist)
{
	MsgNumberList *numlist = NULL;
	GSList *seq_list;

	numlist = procmsg_get_number_list_for_msgs(msglist);

	seq_list = imap_get_lep_set_from_numlist(folder, numlist);
	g_slist_free(numlist);

	return seq_list;
}

static GSList * imap_uid_list_from_lep(clist * list, gint* length)
{
	clistiter * iter;
	GSList * result;
	gint len = 0;
	
	result = NULL;
	
	if (list) {
		for(iter = clist_begin(list) ; iter != NULL ;
		    iter = clist_next(iter)) {
			uint32_t * puid;

			puid = clist_content(iter);
			result = g_slist_prepend(result, GINT_TO_POINTER(* puid));
			len++;
		}
		result = g_slist_reverse(result);
	}
	if (length)
		*length = len;
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
		result = g_slist_prepend(result, GINT_TO_POINTER(* puid));
	}
	result = g_slist_reverse(result);
	return result;
}

static void imap_flags_hash_from_lep_uid_flags_tab(carray * list,
						   GHashTable * hash,
						   GHashTable * tags_hash)
{
	unsigned int i;
	
	for(i = 0 ; i < carray_count(list) ; i += 3) {
		uint32_t * puid;
		int * pflags;
		GSList *tags;
		
		puid = carray_get(list, i);
		pflags = carray_get(list, i + 1);
		tags = carray_get(list, i + 2);
		
		g_hash_table_insert(hash, GINT_TO_POINTER(*puid), GINT_TO_POINTER(* pflags));
		g_hash_table_insert(tags_hash, GINT_TO_POINTER(*puid), tags);
	}
}

static MsgInfo *imap_envelope_from_lep(struct imap_fetch_env_info * info,
				       FolderItem *item)
{
	MsgInfo *msginfo = NULL;
	guint32 uid = 0;
	goffset size = 0;
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
	size = (goffset) info->size;
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

static struct mailimap_flag_list * imap_flag_to_lep(IMAPFolderItem *item, IMAPFlags flags, GSList *tags)
{
	struct mailimap_flag_list * flag_list;
	GSList *cur = tags;

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
	if (IMAP_IS_FORWARDED(flags) && flag_ok(item, IMAP_FLAG_FORWARDED))
		mailimap_flag_list_add(flag_list,
				       mailimap_flag_new_flag_keyword(strdup(RTAG_FORWARDED)));
	if (IMAP_IS_SPAM(flags) && flag_ok(item, IMAP_FLAG_SPAM))
		mailimap_flag_list_add(flag_list,
				       mailimap_flag_new_flag_keyword(strdup(RTAG_JUNK)));
	else if (IMAP_IS_HAM(flags) && flag_ok(item, IMAP_FLAG_HAM))
		mailimap_flag_list_add(flag_list,
				       mailimap_flag_new_flag_keyword(strdup(RTAG_NON_JUNK)));
	
	for (; cur; cur = cur->next) {
		gchar *enc_str = 
			imap_utf8_to_modified_utf7(cur->data, TRUE);
		g_strstrip(enc_str);
	
		mailimap_flag_list_add(flag_list,
			mailimap_flag_new_flag_keyword(enc_str));
	}

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

void imap_disconnect_all(gboolean have_connectivity)
{
	GList *list;
	gboolean short_timeout;
#ifdef HAVE_NETWORKMANAGER_SUPPORT
	GError *error = NULL;

	short_timeout = !networkmanager_is_online(&error);
	if(error) {
		short_timeout = TRUE;
		g_error_free(error);
	}
#else
	short_timeout = TRUE;
#endif

	if(short_timeout)
		imap_main_set_timeout(1);

	for (list = account_get_list(); list != NULL; list = list->next) {
		PrefsAccount *account = list->data;
		if (account->protocol == A_IMAP4) {
			RemoteFolder *folder = (RemoteFolder *)account->folder;
			if (folder && folder->session) {
				if (imap_is_busy(FOLDER(folder)))
					imap_threaded_cancel(FOLDER(folder));

				IMAPSession *session = (IMAPSession *)folder->session;
				if (have_connectivity)
					imap_threaded_disconnect(FOLDER(folder));
				SESSION(session)->state = SESSION_DISCONNECTED;
				SESSION(session)->sock = NULL;
				imap_safe_destroy(session);
				folder->session = NULL;
			}
		}
	}

	if(short_timeout)
		imap_main_set_timeout(prefs_common.io_timeout_secs);
}

void imap_folder_unref(Folder *folder)
{
	if (((IMAPFolder *)folder)->refcnt > 0)
		((IMAPFolder *)folder)->refcnt--;
}

void imap_cancel_all(void)
{
	GList *folderlist;
	GList *cur;
	
	folderlist = folder_get_list();
	for (cur = folderlist; cur != NULL; cur = g_list_next(cur)) {
		Folder *folder = (Folder *) cur->data;

		if (folder->klass == &imap_class) {
			if (imap_is_busy(folder)) {
				IMAPSession *imap_session;
				RemoteFolder *rfolder;
				
				g_printerr("cancelled\n");
				imap_threaded_cancel(folder);
				rfolder = (RemoteFolder *) folder;
				imap_session = (IMAPSession *) rfolder->session;
				if (imap_session)
					imap_session->cancelled = 1;
			}
		}
	}
}

gboolean imap_cancel_all_enabled(void)
{
	GList *folderlist;
	GList *cur;
	
	folderlist = folder_get_list();
	for (cur = folderlist; cur != NULL; cur = g_list_next(cur)) {
		Folder *folder = (Folder *) cur->data;

		if (folder->klass == &imap_class) {
			if (imap_is_busy(folder)) {
				return TRUE;
			}
		}
	}
	
	return FALSE;
}

static gboolean imap_is_busy(Folder *folder)
{
	IMAPSession *imap_session;
	RemoteFolder *rfolder;
	
	rfolder = (RemoteFolder *) folder;
	imap_session = (IMAPSession *) rfolder->session;
	if (imap_session == NULL)
		return FALSE;
	
	return imap_session->busy;
}

#else /* HAVE_LIBETPAN */

static FolderClass imap_class;

static XMLTag *imap_item_get_xml(Folder *folder, FolderItem *item);
static void imap_item_set_xml(Folder *folder, FolderItem *item, XMLTag *tag);

static Folder	*imap_folder_new	(const gchar	*name,
					 const gchar	*path)
{
	static gboolean missing_imap_warning = TRUE;
	if (missing_imap_warning) {
		missing_imap_warning = FALSE;
		alertpanel_error(
			_("You have one or more IMAP accounts "
			  "defined. However this version of "
			  "Claws Mail has been built without "
			  "IMAP support; your IMAP accounts are "
			  "disabled.\n\n"
			  "You probably need to "
			  "install libetpan and recompile "
			  "Claws Mail."));
	}
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

gchar imap_get_path_separator_for_item(FolderItem *item)
{
	return '/';
}

FolderClass *imap_get_class(void)
{
	if (imap_class.idstr == NULL) {
		imap_class.type = F_IMAP;
		imap_class.idstr = "imap";
		imap_class.uistr = "IMAP";

		imap_class.new_folder = imap_folder_new;
		imap_class.create_tree = imap_create_tree;
		imap_class.create_folder = imap_create_folder;
		imap_class.rename_folder = imap_rename_folder;

		imap_class.set_xml = folder_set_xml;
		imap_class.get_xml = folder_get_xml;
		imap_class.item_set_xml = imap_item_set_xml;
		imap_class.item_get_xml = imap_item_get_xml;
		/* nothing implemented */
	}

	return &imap_class;
}

void imap_disconnect_all(gboolean have_connectivity)
{
}

gint imap_subscribe(Folder *folder, FolderItem *item, gchar *rpath, gboolean sub)
{
	return -1;
}

GList * imap_scan_subtree(Folder *folder, FolderItem *item, gboolean unsubs_only, gboolean recursive)
{
	return NULL;
}

void imap_cache_msg(FolderItem *item, gint msgnum)
{
}

void imap_cancel_all(void)
{
}

gboolean imap_cancel_all_enabled(void)
{
	return FALSE;
}

#endif

#ifdef HAVE_LIBETPAN
static void imap_synchronise(FolderItem *item, gint days) 
{
	if (IMAP_FOLDER_ITEM(item)->last_sync == IMAP_FOLDER_ITEM(item)->last_change) {
		debug_print("%s already synced\n", item->path?item->path:item->name);
		return;
	}
	debug_print("syncing %s\n", item->path?item->path:item->name);
	imap_gtk_synchronise(item, days);
	IMAP_FOLDER_ITEM(item)->last_sync = IMAP_FOLDER_ITEM(item)->last_change;
}
#endif

static void imap_item_set_xml(Folder *folder, FolderItem *item, XMLTag *tag)
{
#ifdef HAVE_LIBETPAN
	GList *cur;
#endif
	folder_item_set_xml(folder, item, tag);
	
#ifdef HAVE_LIBETPAN
	for (cur = tag->attr; cur != NULL; cur = g_list_next(cur)) {
		XMLAttr *attr = (XMLAttr *) cur->data;

		if (!attr || !attr->name || !attr->value) continue;
		if (!strcmp(attr->name, "uidnext"))
			IMAP_FOLDER_ITEM(item)->uid_next = atoi(attr->value);
		if (!strcmp(attr->name, "last_sync"))
			IMAP_FOLDER_ITEM(item)->last_sync = atoi(attr->value);
		if (!strcmp(attr->name, "last_change"))
			IMAP_FOLDER_ITEM(item)->last_change = atoi(attr->value);
	}
	if (IMAP_FOLDER_ITEM(item)->last_change == 0)
		IMAP_FOLDER_ITEM(item)->last_change = time(NULL);
#endif
}

static XMLTag *imap_item_get_xml(Folder *folder, FolderItem *item)
{
	XMLTag *tag;

	tag = folder_item_get_xml(folder, item);

#ifdef HAVE_LIBETPAN
	xml_tag_add_attr(tag, xml_attr_new_int("uidnext", 
			IMAP_FOLDER_ITEM(item)->uid_next));
	xml_tag_add_attr(tag, xml_attr_new_int("last_sync", 
			IMAP_FOLDER_ITEM(item)->last_sync));
	xml_tag_add_attr(tag, xml_attr_new_int("last_change", 
			IMAP_FOLDER_ITEM(item)->last_change));

#endif
	return tag;
}

/* ===================================================================
 * UTF-7 conversion routines as in RFC 2192
 * =================================================================== 
 * These two functions from: 
 * libimap library.
 * Copyright (C) 2003-2004 Pawel Salek. */

/* UTF7 modified base64 alphabet */
static char base64chars[] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+,";
#define UNDEFINED 64

/* UTF16 definitions */
#define UTF16MASK       0x03FFUL
#define UTF16SHIFT      10
#define UTF16BASE       0x10000UL
#define UTF16HIGHSTART  0xD800UL
#define UTF16HIGHEND    0xDBFFUL
#define UTF16LOSTART    0xDC00UL
#define UTF16LOEND      0xDFFFUL


/* Convert an IMAP mailbox to a UTF-8 string.
 *  dst needs to have roughly 4 times the storage space of src
 *    Hex encoding can triple the size of the input
 *    UTF-7 can be slightly denser than UTF-8
 *     (worst case: 8 octets UTF-7 becomes 9 octets UTF-8)
 */
char* imap_modified_utf7_to_utf8(const char *mbox, gboolean change_spaces)
{
  unsigned c, i, bitcount;
  unsigned long ucs4, utf16, bitbuf;
  unsigned char base64[256];
  const char *src;
  char *dst, *res  = g_malloc(2*strlen(mbox)+1);

  dst = res;
  src = mbox;
  if(!dst) return NULL;
  /* initialize modified base64 decoding table */
  memset(base64, UNDEFINED, sizeof (base64));
  for (i = 0; i < sizeof (base64chars); ++i) {
    base64[(unsigned)base64chars[i]] = i;
  }

  /* loop until end of string */
  while (*src != '\0') {
    c = *src++;
    /* deal with literal characters and &- */
    if (c != '&' || *src == '-') {
      /* encode literally */
      if (change_spaces && c == '_')
	*dst++ = ' ';
      else
        *dst++ = c;
      /* skip over the '-' if this is an &- sequence */
      if (c == '&') ++src;
    } else {
      /* convert modified UTF-7 -> UTF-16 -> UCS-4 -> UTF-8 -> HEX */
      bitbuf = 0;
      bitcount = 0;
      ucs4 = 0;
      while ((c = base64[(unsigned char) *src]) != UNDEFINED) {
        ++src;
        bitbuf = (bitbuf << 6) | c;
        bitcount += 6;
        /* enough bits for a UTF-16 character? */
        if (bitcount >= 16) {
          bitcount -= 16;
          utf16 = (bitcount ? bitbuf >> bitcount
                   : bitbuf) & 0xffff;
          /* convert UTF16 to UCS4 */
          if
            (utf16 >= UTF16HIGHSTART && utf16 <= UTF16HIGHEND) {
            ucs4 = (utf16 - UTF16HIGHSTART) << UTF16SHIFT;
            continue;
          } else if
            (utf16 >= UTF16LOSTART && utf16 <= UTF16LOEND) {
            ucs4 += utf16 - UTF16LOSTART + UTF16BASE;
          } else {
            ucs4 = utf16;
          }

          /* convert UTF-16 range of UCS4 to UTF-8 */
          if (ucs4 <= 0x7fUL) {
            dst[0] = ucs4;
            dst += 1;
          } else if (ucs4 <= 0x7ffUL) {
            dst[0] = 0xc0 | (ucs4 >> 6);
            dst[1] = 0x80 | (ucs4 & 0x3f);
            dst += 2;
          } else if (ucs4 <= 0xffffUL) {
            dst[0] = 0xe0 | (ucs4 >> 12);
            dst[1] = 0x80 | ((ucs4 >> 6) & 0x3f);
            dst[2] = 0x80 | (ucs4 & 0x3f);
            dst += 3;
          } else {
            dst[0] = 0xf0 | (ucs4 >> 18);
            dst[1] = 0x80 | ((ucs4 >> 12) & 0x3f);
            dst[2] = 0x80 | ((ucs4 >> 6) & 0x3f);
            dst[3] = 0x80 | (ucs4 & 0x3f);
            dst += 4;
          }
        }
      }
      /* skip over trailing '-' in modified UTF-7 encoding */
      if (*src == '-') ++src;
    }
  }
  /* terminate destination string */
  *dst = '\0';
  return res;
}

/* Convert hex coded UTF-8 string to modified UTF-7 IMAP mailbox
 *  dst should be about twice the length of src to deal with non-hex
 *  coded URLs
 */
char* imap_utf8_to_modified_utf7(const char *src, gboolean change_spaces)
{
  unsigned int utf8pos, utf8total, c, utf7mode, bitstogo, utf16flag;
  unsigned long ucs4 = 0, bitbuf = 0;

  /* initialize hex lookup table */
  char *dst, *res;

  if (!src) return NULL;

  res = malloc(2*strlen(src)+1);
  dst = res;
  if(!dst) return NULL;

  utf7mode = 0;
  utf8total = 0;
  bitstogo = 0;
  utf8pos = 0;
  while ((c = (unsigned char)*src) != '\0') {
    ++src;
    /* normal character? */
    if (c >= ' ' && c <= '~' && (c != '_' || !change_spaces)) {
      /* switch out of UTF-7 mode */
      if (utf7mode) {
        if (bitstogo) {
          *dst++ = base64chars[(bitbuf << (6 - bitstogo)) & 0x3F];
        }
        *dst++ = '-';
        utf7mode = 0;
        utf8pos  = 0;
        bitstogo = 0;
        utf8total= 0;
      }
      if (change_spaces && c == ' ')
        *dst++ = '_';
      else
	*dst++ = c;
      /* encode '&' as '&-' */
      if (c == '&') {
        *dst++ = '-';
      }
      continue;
    }
    /* switch to UTF-7 mode */
    if (!utf7mode) {
      *dst++ = '&';
      utf7mode = 1;
    }
    /* Encode US-ASCII characters as themselves */
    if (c < 0x80) {
      ucs4 = c;
    } else if (utf8total) {
      /* save UTF8 bits into UCS4 */
      ucs4 = (ucs4 << 6) | (c & 0x3FUL);
      if (++utf8pos < utf8total) {
        continue;
      }
    } else {
      utf8pos = 1;
      if (c < 0xE0) {
        utf8total = 2;
        ucs4 = c & 0x1F;
      } else if (c < 0xF0) {
        utf8total = 3;
        ucs4 = c & 0x0F;
      } else {
        /* NOTE: can't convert UTF8 sequences longer than 4 */
        utf8total = 4;
        ucs4 = c & 0x03;
      }
      continue;
    }
    /* loop to split ucs4 into two utf16 chars if necessary */
    utf8total = 0;
    do {
      if (ucs4 >= UTF16BASE) {
        ucs4 -= UTF16BASE;
        bitbuf = (bitbuf << 16) | ((ucs4 >> UTF16SHIFT)
                                   + UTF16HIGHSTART);
        ucs4 = (ucs4 & UTF16MASK) + UTF16LOSTART;
        utf16flag = 1;
      } else {
        bitbuf = (bitbuf << 16) | ucs4;
        utf16flag = 0;
      }
      bitstogo += 16;
      /* spew out base64 */
      while (bitstogo >= 6) {
        bitstogo -= 6;
        *dst++ = base64chars[(bitstogo ? (bitbuf >> bitstogo)
                              : bitbuf)
                             & 0x3F];
      }
    } while (utf16flag);
  }
  /* if in UTF-7 mode, finish in ASCII */
  if (utf7mode) {
    if (bitstogo) {
      *dst++ = base64chars[(bitbuf << (6 - bitstogo)) & 0x3F];
    }
    *dst++ = '-';
  }
  /* tie off string */
  *dst = '\0';
  return res;
}
