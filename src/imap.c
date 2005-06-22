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
#include "imap.h"
#include "imap_gtk.h"
#include "socket.h"
#include "ssl.h"
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

#ifdef USE_PTHREAD
#include <pthread.h>
static pthread_mutex_t imap_mutex;
static const char *mutex_hold = NULL;

#ifndef __GNUC__
#define __FUNCTION__ __FILE__
#endif 

#define MUTEX_TRYLOCK_OR_RETURN() {					\
	debug_print("%s: locking mutex\n", __FUNCTION__);		\
	if (pthread_mutex_trylock(&imap_mutex) == EBUSY) {		\
		g_warning("can't lock mutex (held by %s)\n", 		\
				mutex_hold ? mutex_hold:"(nil)");	\
		return;							\
	}								\
	mutex_hold = __FUNCTION__;					\
}

#define MUTEX_TRYLOCK_OR_RETURN_VAL(retval) {				\
	debug_print("%s: locking mutex\n", __FUNCTION__);		\
	if (pthread_mutex_trylock(&imap_mutex) == EBUSY) {		\
		g_warning("can't lock mutex (held by %s)\n", 		\
				mutex_hold ? mutex_hold:"(nil)");	\
		return retval;						\
	}								\
	mutex_hold = __FUNCTION__;					\
}

#define MUTEX_UNLOCK() {						\
	debug_print("%s: unlocking mutex\n", __FUNCTION__);\
	pthread_mutex_unlock(&imap_mutex);				\
	mutex_hold = NULL;						\
}

#else
#define MUTEX_TRYLOCK_OR_RETURN() do {} while(0)
#define MUTEX_TRYLOCK_OR_RETURN_VAL(retval) do {} while(0)
#define MUTEX_UNLOCK() do {} while(0)
#endif

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
};

struct _IMAPSession
{
	Session session;

	gboolean authenticated;

	gchar **capability;
	gboolean uidplus;

	gchar *mbox;
	guint cmd_count;

	/* CLAWS */
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

#define QUOTE_IF_REQUIRED(out, str)				\
{								\
	if (*str != '"' && strpbrk(str, " \t(){}[]%*\\") != NULL) {	\
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
	gboolean batching;
};

static void imap_folder_init		(Folder		*folder,
					 const gchar	*name,
					 const gchar	*path);

static Folder	*imap_folder_new	(const gchar	*name,
					 const gchar	*path);
static void	 imap_folder_destroy	(Folder		*folder);

static IMAPSession *imap_session_new	(const PrefsAccount 	*account);
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

static gint imap_greeting		(IMAPSession	*session);
static gint imap_auth			(IMAPSession	*session,
					 const gchar	*user,
					 const gchar	*pass,
					 IMAPAuthType	 type);

static gint imap_scan_tree_recursive	(IMAPSession	*session,
					 FolderItem	*item);
static GSList *imap_parse_list		(IMAPFolder	*folder,
					 IMAPSession	*session,
					 const gchar	*real_path,
					 gchar		*separator);

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

static gchar *imap_get_flag_str		(IMAPFlags	 flags);
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
					 gint		*messages,
					 gint		*recent,
					 guint32	*uid_next,
					 guint32	*uid_validity,
					 gint		*unseen,
					 gboolean	 block);

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

static gboolean imap_has_capability	(IMAPSession	*session,
 					 const gchar	*cap);
static void imap_free_capabilities	(IMAPSession 	*session);

/* low-level IMAP4rev1 commands */
static gint imap_cmd_authenticate
				(IMAPSession	*session,
				 const gchar	*user,
				 const gchar	*pass,
				 IMAPAuthType	 type);
static gint imap_cmd_login	(IMAPSession	*session,
				 const gchar	*user,
				 const gchar	*pass);
static gint imap_cmd_logout	(IMAPSession	*session);
static gint imap_cmd_noop	(IMAPSession	*session);
#if USE_OPENSSL
static gint imap_cmd_starttls	(IMAPSession	*session);
#endif
static gint imap_cmd_namespace	(IMAPSession	*session,
				 gchar	       **ns_str);
static gint imap_cmd_list	(IMAPSession	*session,
				 const gchar	*ref,
				 const gchar	*mailbox,
				 GPtrArray	*argbuf);
static gint imap_cmd_do_select	(IMAPSession	*session,
				 const gchar	*folder,
				 gboolean	 examine,
				 gint		*exists,
				 gint		*recent,
				 gint		*unseen,
				 guint32	*uid_validity,
				 gboolean	 block);
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
static gint imap_cmd_envelope	(IMAPSession	*session,
				 IMAPSet	 set);
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
static gint imap_cmd_copy       (IMAPSession    *session, 
                                 const gchar    *seq_set, 
                                 const gchar    *destfolder,
				 GRelation	*uid_mapping);
static gint imap_cmd_store	(IMAPSession	*session,
				 IMAPSet	 set,
				 gchar		*sub_cmd);
static gint imap_cmd_expunge	(IMAPSession	*session,
				 IMAPSet	 seq_set);
static gint imap_cmd_close      (IMAPSession    *session);

static gint imap_cmd_ok		(IMAPSession	*session,
				 GPtrArray	*argbuf);
static gint imap_cmd_ok_block	(IMAPSession	*session,
				 GPtrArray	*argbuf);
static gint imap_cmd_ok_with_block
				(IMAPSession 	*session, 
				 GPtrArray 	*argbuf, 
				 gboolean 	block);
static void imap_gen_send	(IMAPSession	*session,
				 const gchar	*format, ...);
static gint imap_gen_recv	(IMAPSession	*session,
				 gchar	       **ret);
static gint imap_gen_recv_block	(IMAPSession	*session,
				 gchar	       **ret);
static gint imap_gen_recv_with_block	
				(IMAPSession	*session,
				 gchar	       **ret,
				 gboolean	 block);

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

static gchar *imap_utf8_to_modified_utf7	(const gchar	*from);
static gchar *imap_modified_utf7_to_utf8	(const gchar	*mutf7_str);

static GSList *imap_get_seq_set_from_numlist    (MsgNumberList  *msglist);
static GSList *imap_get_seq_set_from_msglist	(MsgInfoList 	*msglist);
static void imap_seq_set_free                   (GSList         *seq_list);

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
static GHashTable *flags_set_table = NULL;
static GHashTable *flags_unset_table = NULL;
typedef struct _hashtable_data {
	IMAPSession *session;
	GSList *msglist;
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

#ifdef USE_PTHREAD
void *imap_getline_thread(void *data)
{
	thread_data *td = (thread_data *)data;
	gchar *line = NULL;
	
	line = sock_getline(td->sock);
	
	td->done = TRUE;
	
	return (void *)line;
}
#endif

/* imap_getline just wraps sock_getline inside a thread, 
 * performing gtk updates so that the interface isn't frozen.
 */
static gchar *imap_getline(SockInfo *sock)
{
#if (defined USE_PTHREAD && defined __GLIBC__ && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 3)))
	thread_data *td = g_new0(thread_data, 1);
	pthread_t pt;
	gchar *line;
	td->sock = sock;
	td->done = FALSE;
	if (pthread_create(&pt, PTHREAD_CREATE_JOINABLE,
			imap_getline_thread, td) != 0) {
		g_free(td);
		return sock_getline(sock);
	}
	
	debug_print("+++waiting for imap_getline_thread...\n");
	while(!td->done) {
		/* don't let the interface freeze while waiting */
		sylpheed_do_idle();
	}
	debug_print("---imap_getline_thread done\n");

	/* get the thread's return value and clean its resources */
	pthread_join(pt, (void *)&line);
	g_free(td);

	return line;
#else
	return sock_getline(sock);
#endif
}

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
#ifdef USE_PTREAD
		pthread_mutex_init(&imap_mutex, NULL);
#endif
	}
	
	return &imap_class;
}

static gchar *get_seq_set_from_seq_list(GSList *seq_list)
{
	gchar *val = NULL;
	gchar *tmp = NULL;
	GSList *cur = NULL;
	
	for (cur = seq_list; cur != NULL; cur = cur->next) {
		tmp = val?g_strdup(val):NULL;
		g_free(val);
		val = g_strconcat(tmp?tmp:"", tmp?",":"",(gchar *)cur->data,
				  NULL);
		g_free(tmp);
	}
	return val;
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

	dir = imap_folder_get_path(folder);
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

	if (imap_has_capability(session, "UIDPLUS")) 
    		session->uidplus = TRUE; 

	return 0;
}

static gint imap_auth(IMAPSession *session, const gchar *user, const gchar *pass,
		      IMAPAuthType type)
{
	gint ok;

	if (type == 0 || type == IMAP_AUTH_LOGIN)
		ok = imap_cmd_login(session, user, pass);
	else
		ok = imap_cmd_authenticate(session, user, pass, type);

	if (ok == IMAP_SUCCESS)
		session->authenticated = TRUE;

	return ok;
}

static IMAPSession *imap_session_get(Folder *folder)
{
	RemoteFolder *rfolder = REMOTE_FOLDER(folder);
	IMAPSession *session = NULL;

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(FOLDER_CLASS(folder) == &imap_class, NULL);
	g_return_val_if_fail(folder->account != NULL, NULL);
	
	if (prefs_common.work_offline && !imap_gtk_should_override()) {
		return NULL;
	}

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
	if (time(NULL) - SESSION(session)->last_access_time > SESSION_TIMEOUT_INTERVAL) {
		/* verify that the session is still alive */
		if (imap_cmd_noop(session) != IMAP_SUCCESS) {
			/* Check if this is the first try to establish a
			   connection, if yes we don't try to reconnect */
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
				session_destroy(SESSION(session));
				/* Clear folders session to make imap_session_get create
				   a new session, because of rfolder->session == NULL
				   it will not try to reconnect again and so avoid an
				   endless loop */
				rfolder->session = NULL;
				session = imap_session_get(folder);
				statusbar_pop_all();
			}
		}
	}

	rfolder->session = SESSION(session);
	
	return IMAP_SESSION(session);
}

static IMAPSession *imap_session_new(const PrefsAccount *account)
{
	IMAPSession *session;
	SockInfo *imap_sock;
	gushort port;

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

	session->authenticated = FALSE;
	session->mbox = NULL;
	session->cmd_count = 0;

	/* Only need to log in if the connection was not PREAUTH */
	if (imap_greeting(session) != IMAP_SUCCESS) {
		session_destroy(SESSION(session));
		return NULL;
	}

#if USE_OPENSSL
	if (account->ssl_imap == SSL_STARTTLS && 
	    imap_has_capability(session, "STARTTLS")) {
		gint ok;

		ok = imap_cmd_starttls(session);
		if (ok != IMAP_SUCCESS) {
			log_warning(_("Can't start TLS session.\n"));
			session_destroy(SESSION(session));
			return NULL;
		}
		if (!ssl_init_socket_with_method(SESSION(session)->sock, 
		    SSL_METHOD_TLSv1)) {
			session_destroy(SESSION(session));
			return NULL;
		}

		imap_free_capabilities(session);
		session->authenticated = FALSE;
		session->uidplus = FALSE;
		session->cmd_count = 1;

		if (imap_greeting(session) != IMAP_SUCCESS) {
			session_destroy(SESSION(session));
			return NULL;
		}		
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
			return;
		Xstrdup_a(pass, tmp_pass, {g_free(tmp_pass); return;});
		g_free(tmp_pass);
	}
	statusbar_print_all(_("Connecting to IMAP4 server %s...\n"),
				account->recv_server);
	if (imap_auth(session, account->userid, pass, account->imap_auth_type) != IMAP_SUCCESS) {
		imap_cmd_logout(session);
		statusbar_pop_all();
		
		return;
	}
	statusbar_pop_all();
	session->authenticated = TRUE;
}

static void imap_session_destroy(Session *session)
{
	imap_free_capabilities(IMAP_SESSION(session));
	g_free(IMAP_SESSION(session)->mbox);
	sock_close(session->sock);
	session->sock = NULL;
}

static gchar *imap_fetch_msg(Folder *folder, FolderItem *item, gint uid)
{
	return imap_fetch_msg_full(folder, item, uid, TRUE, TRUE);
}

static guint get_size_with_lfs(MsgInfo *info) 
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
		guint have_size = get_size_with_lfs(msginfo);
		debug_print("message %d has been already %scached (%d/%d).\n", uid,
				have_size == cached->size ? "fully ":"",
				have_size, cached? (int)cached->size : -1);
				
		if (cached && (cached->size == have_size || !body)) {
			procmsg_msginfo_free(cached);
			procmsg_msginfo_free(msginfo);
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

	debug_print("IMAP fetching messages\n");
	ok = imap_select(session, IMAP_FOLDER(folder), item->path,
			 NULL, NULL, NULL, NULL, FALSE);
	if (ok != IMAP_SUCCESS) {
		g_warning("can't select mailbox %s\n", item->path);
		g_free(filename);
		return NULL;
	}

	debug_print("getting message %d...\n", uid);
	ok = imap_cmd_fetch(session, (guint32)uid, filename, headers, body);

	if (ok != IMAP_SUCCESS) {
		g_warning("can't fetch message %d\n", uid);
		g_free(filename);
		return NULL;
	}

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


	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(file_list != NULL, -1);
	
	MUTEX_TRYLOCK_OR_RETURN_VAL(-1);

	session = imap_session_get(folder);
	if (!session) {
		MUTEX_UNLOCK();
		return -1;
	}
	destdir = imap_get_real_path(IMAP_FOLDER(folder), dest->path);

	for (cur = file_list; cur != NULL; cur = cur->next) {
		IMAPFlags iflags = 0;
		guint32 new_uid = 0;

		fileinfo = (MsgFileInfo *)cur->data;

		if (fileinfo->flags) {
			if (MSG_IS_MARKED(*fileinfo->flags))
				iflags |= IMAP_FLAG_FLAGGED;
			if (MSG_IS_REPLIED(*fileinfo->flags))
				iflags |= IMAP_FLAG_ANSWERED;
			if (!MSG_IS_UNREAD(*fileinfo->flags))
				iflags |= IMAP_FLAG_SEEN;
		}

		if (dest->stype == F_OUTBOX ||
		    dest->stype == F_QUEUE  ||
		    dest->stype == F_DRAFT  ||
		    dest->stype == F_TRASH)
			iflags |= IMAP_FLAG_SEEN;

		ok = imap_cmd_append(session, destdir, fileinfo->file, iflags, 
				     &new_uid);

		if (ok != IMAP_SUCCESS) {
			g_warning("can't append message %s\n", fileinfo->file);
			g_free(destdir);
			MUTEX_UNLOCK();
			return -1;
		}

		if (relation != NULL)
			g_relation_insert(relation, fileinfo->msginfo != NULL ? 
					  (gpointer) fileinfo->msginfo : (gpointer) fileinfo,
					  GINT_TO_POINTER(dest->last_num + 1));
		if (last_uid < new_uid)
			last_uid = new_uid;
	}

	g_free(destdir);

	MUTEX_UNLOCK();
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
	msginfo = (MsgInfo *)msglist->data;

	src = msginfo->folder;
	if (src == dest) {
		g_warning("the src folder is identical to the dest.\n");
		return -1;
	}

	ok = imap_select(session, IMAP_FOLDER(folder), msginfo->folder->path,
			 NULL, NULL, NULL, NULL, FALSE);
	if (ok != IMAP_SUCCESS) {
		return ok;
	}

	destdir = imap_get_real_path(IMAP_FOLDER(folder), dest->path);
	seq_list = imap_get_seq_set_from_msglist(msglist);
	uid_mapping = g_relation_new(2);
	g_relation_index(uid_mapping, 0, g_direct_hash, g_direct_equal);
	
	for (cur = seq_list; cur != NULL; cur = g_slist_next(cur)) {
		gchar *seq_set = (gchar *)cur->data;

		debug_print("Copying message %s%c[%s] to %s ...\n",
			    src->path, G_DIR_SEPARATOR,
			    seq_set, destdir);

		ok = imap_cmd_copy(session, seq_set, destdir, uid_mapping);
		if (ok != IMAP_SUCCESS) {
			g_relation_destroy(uid_mapping);
			imap_seq_set_free(seq_list);
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

	g_relation_destroy(uid_mapping);
	imap_seq_set_free(seq_list);

	g_free(destdir);
	
	IMAP_FOLDER_ITEM(dest)->lastuid = 0;
	IMAP_FOLDER_ITEM(dest)->uid_next = 0;
	g_slist_free(IMAP_FOLDER_ITEM(dest)->uid_list);
	IMAP_FOLDER_ITEM(dest)->uid_list = NULL;

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

	MUTEX_TRYLOCK_OR_RETURN_VAL(-1);

	msginfo = (MsgInfo *)msglist->data;
	g_return_val_if_fail(msginfo->folder != NULL, -1);

	if (folder == msginfo->folder->folder) {
		ret = imap_do_copy_msgs(folder, dest, msglist, relation);
		MUTEX_UNLOCK();
		return ret;
	}

	file_list = procmsg_get_message_file_list(msglist);
	g_return_val_if_fail(file_list != NULL, -1);

	MUTEX_UNLOCK();
	ret = imap_add_msgs(folder, dest, file_list, relation);

	procmsg_message_file_list_free(file_list);

	return ret;
}


static gint imap_do_remove_msgs(Folder *folder, FolderItem *dest, 
			        MsgInfoList *msglist, GRelation *relation)
{
	gchar *destdir;
	GSList *seq_list = NULL, *cur;
	MsgInfo *msginfo;
	IMAPSession *session;
	gint ok = IMAP_SUCCESS;
	GRelation *uid_mapping;
	
	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(msglist != NULL, -1);

	MUTEX_TRYLOCK_OR_RETURN_VAL(-1);

	session = imap_session_get(folder);
	if (!session) {
		MUTEX_UNLOCK();
		return -1;
	}
	msginfo = (MsgInfo *)msglist->data;

	ok = imap_select(session, IMAP_FOLDER(folder), msginfo->folder->path,
			 NULL, NULL, NULL, NULL, FALSE);
	if (ok != IMAP_SUCCESS) {
		MUTEX_UNLOCK();
		return ok;
	}

	destdir = imap_get_real_path(IMAP_FOLDER(folder), dest->path);
	for (cur = msglist; cur; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;
		seq_list = g_slist_append(seq_list, GINT_TO_POINTER(msginfo->msgnum));
	}

	uid_mapping = g_relation_new(2);
	g_relation_index(uid_mapping, 0, g_direct_hash, g_direct_equal);

	ok = imap_set_message_flags
		(IMAP_SESSION(REMOTE_FOLDER(folder)->session),
		seq_list, IMAP_FLAG_DELETED, TRUE);
	if (ok != IMAP_SUCCESS) {
		log_warning(_("can't set deleted flags\n"));
		MUTEX_UNLOCK();
		return ok;
	}
	ok = imap_cmd_expunge(session, NULL);
	if (ok != IMAP_SUCCESS) {
		log_warning(_("can't expunge\n"));
		MUTEX_UNLOCK();
		return ok;
	}
	
	g_relation_destroy(uid_mapping);
	g_slist_free(seq_list);

	g_free(destdir);

	MUTEX_UNLOCK();
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
	g_slist_free(list);
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
	gint ok;
	IMAPSession *session;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(item != NULL, -1);
	g_return_val_if_fail(item->path != NULL, -1);

	session = imap_session_get(folder);
	if (!session) return -1;

	if (session->mbox) {
		if (strcmp2(session->mbox, item->path) != 0) return -1;

		ok = imap_cmd_close(session);
		if (ok != IMAP_SUCCESS)
			log_warning(_("can't close folder\n"));

		g_free(session->mbox);

		session->mbox = NULL;

		return ok;
	}

	return 0;
}

static gint imap_scan_tree(Folder *folder)
{
	FolderItem *item = NULL;
	IMAPSession *session;
	gchar *root_folder = NULL;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(folder->account != NULL, -1);

	MUTEX_TRYLOCK_OR_RETURN_VAL(-1);

	session = imap_session_get(folder);
	if (!session) {
		if (!folder->node) {
			folder_tree_destroy(folder);
			item = folder_item_new(folder, folder->name, NULL);
			item->folder = folder;
			folder->node = item->node = g_node_new(item);
		}
		MUTEX_UNLOCK();
		return -1;
	}

	if (folder->account->imap_dir && *folder->account->imap_dir) {
		gchar *real_path;
		GPtrArray *argbuf;
		gint ok;

		Xstrdup_a(root_folder, folder->account->imap_dir, {MUTEX_UNLOCK();return -1;});
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
		argbuf = g_ptr_array_new();
		ok = imap_cmd_list(session, NULL, real_path, argbuf);
		if (ok != IMAP_SUCCESS ||
		    search_array_str(argbuf, "LIST ") == NULL) {
			log_warning(_("root folder %s does not exist\n"), real_path);
			g_ptr_array_free(argbuf, TRUE);
			g_free(real_path);

			if (!folder->node) {
				item = folder_item_new(folder, folder->name, NULL);
				item->folder = folder;
				folder->node = item->node = g_node_new(item);
			}
			MUTEX_UNLOCK();
			return -1;
		}
		g_ptr_array_free(argbuf, TRUE);
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

	MUTEX_UNLOCK();
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
	QUOTE_IF_REQUIRED(wildcard_path, wildcard_path);

	imap_gen_send(session, "LIST \"\" %s",
		      wildcard_path);

	strtailchomp(real_path, separator);
	item_list = imap_parse_list(imapfolder, session, real_path, NULL);
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

static GSList *imap_parse_list(IMAPFolder *folder, IMAPSession *session,
			       const gchar *real_path, gchar *separator)
{
	gchar buf[IMAPBUFSIZE];
	gchar flags[256];
	gchar separator_str[16];
	gchar *p;
	gchar *base;
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
			if (sscanf(buf, "%*d %16s", buf) < 1 ||
			    strcmp(buf, "OK") != 0)
				log_warning(_("error occurred while getting LIST.\n"));
				
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
		base = g_path_get_basename(buf);
		if (base[0] == '.') continue;

		loc_name = imap_modified_utf7_to_utf8(base);
		loc_path = imap_modified_utf7_to_utf8(buf);
		new_item = folder_item_new(FOLDER(folder), loc_name, loc_path);
		if (strcasestr(flags, "\\Noinferiors") != NULL)
			new_item->no_sub = TRUE;
		if (strcmp(buf, "INBOX") != 0 &&
		    strcasestr(flags, "\\Noselect") != NULL)
			new_item->no_select = TRUE;

		item_list = g_slist_append(item_list, new_item);

		debug_print("folder '%s' found.\n", loc_path);
		g_free(base);
		g_free(loc_path);
		g_free(loc_name);
	}

	g_string_free(str, TRUE);

	return item_list;
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

	g_return_val_if_fail(folder != NULL, NULL);
	g_return_val_if_fail(folder->account != NULL, NULL);
	g_return_val_if_fail(parent != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);

	MUTEX_TRYLOCK_OR_RETURN_VAL(NULL);

	session = imap_session_get(folder);
	if (!session) {
		MUTEX_UNLOCK();
		return NULL;
	}

	if (!folder_item_parent(parent) && strcmp(name, "INBOX") == 0)
		dirpath = g_strdup(name);
	else if (parent->path)
		dirpath = g_strconcat(parent->path, "/", name, NULL);
	else if ((p = strchr(name, '/')) != NULL && *(p + 1) != '\0')
		dirpath = g_strdup(name);
	else if (folder->account->imap_dir && *folder->account->imap_dir) {
		gchar *imap_dir;

		Xstrdup_a(imap_dir, folder->account->imap_dir, {MUTEX_UNLOCK();return NULL;});
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
		MUTEX_UNLOCK();
		return NULL;});

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
			MUTEX_UNLOCK();
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
				MUTEX_UNLOCK();
				return NULL;
			}
		}
	}

	new_item = folder_item_new(folder, new_name, dirpath);
	folder_item_append(parent, new_item);
	g_free(imap_path);
	g_free(dirpath);

	dirpath = folder_item_get_path(new_item);
	if (!is_dir_exist(dirpath))
		make_dir_hier(dirpath);
	g_free(dirpath);

	MUTEX_UNLOCK();
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

	MUTEX_TRYLOCK_OR_RETURN_VAL(-1);

	if (strchr(name, imap_get_path_separator(IMAP_FOLDER(folder), item->path)) != NULL) {
		g_warning(_("New folder name must not contain the namespace "
			    "path separator"));
		MUTEX_UNLOCK();
		return -1;
	}

	session = imap_session_get(folder);
	if (!session) {
		MUTEX_UNLOCK();
		return -1;
	}
	real_oldpath = imap_get_real_path(IMAP_FOLDER(folder), item->path);

	g_free(session->mbox);
	session->mbox = NULL;
	ok = imap_cmd_examine(session, "INBOX",
			      &exists, &recent, &unseen, &uid_validity, FALSE);
	if (ok != IMAP_SUCCESS) {
		g_free(real_oldpath);
		MUTEX_UNLOCK();
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
		MUTEX_UNLOCK();
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

	MUTEX_UNLOCK();
	return 0;
}

static gint imap_remove_folder_real(Folder *folder, FolderItem *item)
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

	MUTEX_TRYLOCK_OR_RETURN_VAL(-1);

	session = imap_session_get(folder);
	if (!session) {
		MUTEX_UNLOCK();
		return -1;
	}
	path = imap_get_real_path(IMAP_FOLDER(folder), item->path);

	ok = imap_cmd_examine(session, "INBOX",
			      &exists, &recent, &unseen, &uid_validity, FALSE);
	if (ok != IMAP_SUCCESS) {
		g_free(path);
		MUTEX_UNLOCK();
		return -1;
	}

	ok = imap_cmd_delete(session, path);
	if (ok != IMAP_SUCCESS) {
		log_warning(_("can't delete mailbox\n"));
		g_free(path);
		MUTEX_UNLOCK();
		return -1;
	}

	g_free(path);
	cache_dir = folder_item_get_path(item);
	if (is_dir_exist(cache_dir) && remove_dir_recursive(cache_dir) < 0)
		g_warning("can't remove directory '%s'\n", cache_dir);
	g_free(cache_dir);
	folder_item_remove(item);

	MUTEX_UNLOCK();
	return 0;
}

static gint imap_remove_folder(Folder *folder, FolderItem *item)
{
	GNode *node, *next;

	g_return_val_if_fail(item != NULL, -1);
	g_return_val_if_fail(item->folder != NULL, -1);
	g_return_val_if_fail(item->node != NULL, -1);
	g_return_val_if_fail(item->no_select == FALSE, -1);

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

	gchar *tmp;
	GSList *newlist = NULL;
	GSList *llast = NULL;
	GString *str = NULL;
	MsgInfo *msginfo;
	GSList *seq_list, *cur;
	IMAPSet imapset;
	
	stuff->total = g_slist_length(numlist);
	stuff->cur = 0;

	if (session == NULL || item == NULL || item->folder == NULL
	    || FOLDER_CLASS(item->folder) != &imap_class) {
		stuff->done = TRUE;
		return NULL;
	}

	seq_list = imap_get_seq_set_from_numlist(numlist);
	for (cur = seq_list; cur != NULL; cur = g_slist_next(cur)) {
		imapset = cur->data;
		
		if (!imapset || strlen(imapset) == 0)
			continue;

		if (imap_cmd_envelope(session, imapset)
		    != IMAP_SUCCESS) {
			log_warning(_("can't get envelope\n"));
			continue;
		}

		str = g_string_new(NULL);

		for (;;) {
			if ((tmp =sock_getline(SESSION(session)->sock)) == NULL) {
				log_warning(_("error occurred while getting envelope.\n"));
				g_string_free(str, TRUE);
				str = NULL;
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
			
			stuff->cur++;

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
		if (str)
			g_string_free(str, TRUE);
	}
	imap_seq_set_free(seq_list);
	
	session_set_access_time(SESSION(session));
	stuff->done = TRUE;

	return newlist;
}

static GSList *imap_get_uncached_messages(IMAPSession *session,
					FolderItem *item,
					MsgNumberList *numlist)
{
	uncached_data *data = g_new0(uncached_data, 1);
	GSList *result = NULL;
	gint last_cur = 0;
#ifdef USE_PTHREAD
	pthread_t pt;
#endif
	data->done = FALSE;
	data->session = session;
	data->item = item;
	data->numlist = numlist;
	data->cur = 0;
	data->total = 0;
	
	if (prefs_common.work_offline && !imap_gtk_should_override()) {
		g_free(data);
		return NULL;
	}

#if (defined USE_PTHREAD && defined __GLIBC__ && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 3)))
	MUTEX_TRYLOCK_OR_RETURN_VAL(NULL);

	if (pthread_create(&pt, PTHREAD_CREATE_JOINABLE,
			imap_get_uncached_messages_thread, data) != 0) {
		result = (GSList *)imap_get_uncached_messages_thread(data);
		g_free(data);
		MUTEX_UNLOCK();
		return result;
	}
	debug_print("+++waiting for imap_get_uncached_messages_thread...\n");
	statusbar_print_all(_("IMAP4 Fetching uncached short headers..."));
	while(!data->done) {
		/* don't let the interface freeze while waiting */
		sylpheed_do_idle();
		if (data->total != 0 && last_cur != data->cur && data->cur % 10 == 0) {
			gchar buf[32];
			g_snprintf(buf, sizeof(buf), "%d / %d",
				   data->cur, data->total);
			gtk_progress_bar_set_text
				(GTK_PROGRESS_BAR(mainwindow_get_mainwindow()->progressbar), buf);
			gtk_progress_bar_set_fraction
			(GTK_PROGRESS_BAR(mainwindow_get_mainwindow()->progressbar),
			 (gfloat)data->cur / (gfloat)data->total);
			last_cur = data->cur;
		}
	}
	gtk_progress_bar_set_fraction
		(GTK_PROGRESS_BAR(mainwindow_get_mainwindow()->progressbar), 0);
	gtk_progress_bar_set_text
		(GTK_PROGRESS_BAR(mainwindow_get_mainwindow()->progressbar), "");
	statusbar_pop_all();

	debug_print("---imap_get_uncached_messages_thread done\n");

	/* get the thread's return value and clean its resources */
	pthread_join(pt, (void *)&result);
	MUTEX_UNLOCK();
#else
	result = (GSList *)imap_get_uncached_messages_thread(data);
#endif
	g_free(data);
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

void *imap_open_thread(void *data)
{
	SockInfo *sock = NULL;
	thread_data *td = (thread_data *)data;
	if ((sock = sock_connect(td->server, td->port)) == NULL) {
		log_warning(_("Can't connect to IMAP4 server: %s:%d\n"),
			    td->server, td->port);
		td->done = TRUE;
		return NULL;
	}

	td->done = TRUE;
	return sock;
}

#if USE_OPENSSL
static SockInfo *imap_open(const gchar *server, gushort port,
			   SSLType ssl_type)
#else
static SockInfo *imap_open(const gchar *server, gushort port)
#endif
{
	thread_data *td = g_new0(thread_data, 1);
#if USE_PTHREAD
	pthread_t pt;
#endif
	SockInfo *sock = NULL;

#if USE_OPENSSL
	td->ssl_type = ssl_type;
#endif
	td->server = g_strdup(server);
	td->port = port;
	td->done = FALSE;

	statusbar_print_all(_("Connecting to IMAP4 server: %s..."), server);

#if (defined USE_PTHREAD && defined __GLIBC__ && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 3)))
	if (pthread_create(&pt, PTHREAD_CREATE_JOINABLE,
			imap_open_thread, td) != 0) {
		statusbar_pop_all();
		sock = imap_open_thread(td);
	} else {	
		debug_print("+++waiting for imap_open_thread...\n");
		while(!td->done) {
			/* don't let the interface freeze while waiting */
			sylpheed_do_idle();
		}

		/* get the thread's return value and clean its resources */
		pthread_join(pt, (void *)&sock);
	}
#else
	sock = imap_open_thread(td);
#endif
#if USE_OPENSSL
	if (sock && td->ssl_type == SSL_TUNNEL && !ssl_init_socket(sock)) {
		log_warning(_("Can't establish IMAP4 session with: %s:%d\n"),
			    td->server, td->port);
		sock_close(sock);
		sock = NULL;
		td->done = TRUE;
	}
#endif
	g_free(td->server);
	g_free(td);

	debug_print("---imap_open_thread returned %p\n", sock);
	statusbar_pop_all();

	if(!sock && !prefs_common.no_recv_err_panel) {
		alertpanel_error(_("Can't connect to IMAP4 server: %s:%d"),
				 server, port);
	}

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
	guchar *p = str;
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

static gchar *imap_parse_atom(SockInfo *sock, gchar *src,
			      gchar *dest, gint dest_len, GString *str)
{
	gchar *cur_pos = src;
	gchar *nextline;

	g_return_val_if_fail(str != NULL, cur_pos);

	/* read the next line if the current response buffer is empty */
	while (isspace(*(guchar *)cur_pos)) cur_pos++;
	while (*cur_pos == '\0') {
		if ((nextline = imap_getline(sock)) == NULL)
			return cur_pos;
		g_string_assign(str, nextline);
		cur_pos = str->str;
		strretchomp(nextline);
		/* log_print("IMAP4< %s\n", nextline); */
		debug_print("IMAP4< %s\n", nextline);
		g_free(nextline);

		while (isspace(*(guchar *)cur_pos)) cur_pos++;
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
		g_return_val_if_fail(len >= 0, cur_pos);

		g_string_truncate(str, 0);
		cur_pos = str->str;

		do {
			if ((nextline = imap_getline(sock)) == NULL)
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

	while (isspace(*(guchar *)cur_pos)) cur_pos++;

	g_return_val_if_fail(*cur_pos == '{', cur_pos);

	cur_pos = strchr_cpy(cur_pos + 1, '}', buf, sizeof(buf));
	len = atoi(buf);
	g_return_val_if_fail(len >= 0, cur_pos);

	g_string_truncate(str, 0);
	cur_pos = str->str;

	do {
		if ((nextline = sock_getline(sock)) == NULL) {
			*headers = NULL;
			return cur_pos;
		}
		block_len += strlen(nextline);
		g_string_append(str, nextline);
		cur_pos = str->str;
		strretchomp(nextline);
		/* debug_print("IMAP4< %s\n", nextline); */
		g_free(nextline);
	} while (block_len < len);

	debug_print("IMAP4< [contents of BODY.PEEK[HEADER_FIELDS (...)]\n");

	*headers = g_strndup(cur_pos, len);
	cur_pos += len;

	while (isspace(*(guchar *)cur_pos)) cur_pos++;
	while (*cur_pos == '\0') {
		if ((nextline = sock_getline(sock)) == NULL)
			return cur_pos;
		g_string_assign(str, nextline);
		cur_pos = str->str;
		strretchomp(nextline);
		debug_print("IMAP4< %s\n", nextline);
		g_free(nextline);

		while (isspace(*(guchar *)cur_pos)) cur_pos++;
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

		if (g_ascii_strncasecmp(p, "Recent", 6) == 0 && MSG_IS_UNREAD(flags)) {
			MSG_SET_PERM_FLAGS(flags, MSG_NEW);
		} else if (g_ascii_strncasecmp(p, "Seen", 4) == 0) {
			MSG_UNSET_PERM_FLAGS(flags, MSG_NEW|MSG_UNREAD);
		} else if (g_ascii_strncasecmp(p, "Deleted", 7) == 0) {
			MSG_SET_PERM_FLAGS(flags, MSG_DELETED);
		} else if (g_ascii_strncasecmp(p, "Flagged", 7) == 0) {
			MSG_SET_PERM_FLAGS(flags, MSG_MARKED);
		} else if (g_ascii_strncasecmp(p, "Answered", 8) == 0) {
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

static gchar *imap_get_flag_str(IMAPFlags flags)
{
	GString *str;
	gchar *ret;

	str = g_string_new(NULL);

	if (IMAP_IS_SEEN(flags))	g_string_append(str, "\\Seen ");
	if (IMAP_IS_ANSWERED(flags))	g_string_append(str, "\\Answered ");
	if (IMAP_IS_FLAGGED(flags))	g_string_append(str, "\\Flagged ");
	if (IMAP_IS_DELETED(flags))	g_string_append(str, "\\Deleted ");
	if (IMAP_IS_DRAFT(flags))	g_string_append(str, "\\Draft");

	if (str->len > 0 && str->str[str->len - 1] == ' ')
		g_string_truncate(str, str->len - 1);

	ret = str->str;
	g_string_free(str, FALSE);

	return ret;
}

static gint imap_set_message_flags(IMAPSession *session,
				   MsgNumberList *numlist,
				   IMAPFlags flags,
				   gboolean is_set)
{
	gchar *cmd;
	gchar *flag_str;
	gint ok = 0;
	GSList *seq_list;
	IMAPSet imapset;

	flag_str = imap_get_flag_str(flags);
	cmd = g_strconcat(is_set ? "+FLAGS.SILENT (" : "-FLAGS.SILENT (",
			  flag_str, ")", NULL);
	g_free(flag_str);

	seq_list = imap_get_seq_set_from_numlist(numlist);
	imapset = get_seq_set_from_seq_list(seq_list);

	ok = imap_cmd_store(session, imapset, cmd);
	
	g_free(imapset);
	imap_seq_set_free(seq_list);
	g_free(cmd);

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

static void *imap_select_thread(void *data)
{
	select_data *stuff = (select_data *)data;
	IMAPSession *session = stuff->session;
	gchar *real_path = stuff->real_path;
	gint *exists = stuff->exists;
	gint *recent = stuff->recent;
	gint *unseen = stuff->unseen;
	guint32 *uid_validity = stuff->uid_validity;
	gint ok;
	
	ok = imap_cmd_select(session, real_path,
			     exists, recent, unseen, uid_validity, TRUE);
	stuff->done = TRUE;
	return GINT_TO_POINTER(ok);
}
		
static gint imap_select(IMAPSession *session, IMAPFolder *folder,
			const gchar *path,
			gint *exists, gint *recent, gint *unseen,
			guint32 *uid_validity, gboolean block)
{
	gchar *real_path;
	gint ok;
	gint exists_, recent_, unseen_;
	guint32 uid_validity_;
	
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
	
#if (defined USE_PTHREAD && defined __GLIBC__ && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 3)))

	if (block == FALSE) {
		select_data *data = g_new0(select_data, 1);
		void *tmp;
		pthread_t pt;
		data->session = session;
		data->real_path = real_path;
		data->exists = exists;
		data->recent = recent;
		data->unseen = unseen;
		data->uid_validity = uid_validity;
		data->done = FALSE;
		
		if (pthread_create(&pt, PTHREAD_CREATE_JOINABLE,
				imap_select_thread, data) != 0) {
			ok = GPOINTER_TO_INT(imap_select_thread(data));
			g_free(data);
		} else {
			debug_print("+++waiting for imap_select_thread...\n");
			while(!data->done) {
				/* don't let the interface freeze while waiting */
				sylpheed_do_idle();
			}
			debug_print("---imap_select_thread done\n");

			/* get the thread's return value and clean its resources */
			pthread_join(pt, &tmp);
			ok = GPOINTER_TO_INT(tmp);
			g_free(data);
		}
	} else {
		ok = imap_cmd_select(session, real_path,
			     exists, recent, unseen, uid_validity, block);
	}
#else
	ok = imap_cmd_select(session, real_path,
			     exists, recent, unseen, uid_validity, block);
#endif
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
			gint *unseen, gboolean block)
{
	gchar *real_path;
	gchar *real_path_;
	gint ok;
	GPtrArray *argbuf = NULL;
	gchar *str;

	if (messages && recent && uid_next && uid_validity && unseen) {
		*messages = *recent = *uid_next = *uid_validity = *unseen = 0;
		argbuf = g_ptr_array_new();
	}

	real_path = imap_get_real_path(folder, path);
	QUOTE_IF_REQUIRED(real_path_, real_path);
	imap_gen_send(session, "STATUS %s "
			  "(MESSAGES RECENT UIDNEXT UIDVALIDITY UNSEEN)",
			  real_path_);

	ok = imap_cmd_ok_with_block(session, argbuf, block);
	if (ok != IMAP_SUCCESS || !argbuf) THROW(ok);

	str = search_array_str(argbuf, "STATUS");
	if (!str) THROW(IMAP_ERROR);

	str = strrchr(str, '(');
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
	if (argbuf) {
		ptr_array_free_strings(argbuf);
		g_ptr_array_free(argbuf, TRUE);
	}

	return ok;
}

#undef THROW

static gboolean imap_has_capability(IMAPSession *session, const gchar *cap)
{
	gchar **p;
	
	for (p = session->capability; *p != NULL; ++p) {
		if (!g_ascii_strcasecmp(*p, cap))
			return TRUE;
	}

	return FALSE;
}

static void imap_free_capabilities(IMAPSession *session)
{
	g_strfreev(session->capability);
	session->capability = NULL;
}

/* low-level IMAP4rev1 commands */

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
	log_print("IMAP< [Decoded: %s]\n", challenge);

	md5_hex_hmac(hexdigest, challenge, challenge_len, pass, strlen(pass));
	g_free(challenge);

	response = g_strdup_printf("%s %s", user, hexdigest);
	log_print("IMAP> [Encoded: %s]\n", response);
	response64 = g_malloc((strlen(response) + 3) * 2 + 1);
	base64_encode(response64, response, strlen(response));
	g_free(response);

	log_print("IMAP> %s\n", response64);
	sock_puts(SESSION(session)->sock, response64);
	ok = imap_cmd_ok(session, NULL);
	if (ok != IMAP_SUCCESS)
		log_warning(_("IMAP4 authentication failed.\n"));

	return ok;
}

static gint imap_cmd_login(IMAPSession *session,
			   const gchar *user, const gchar *pass)
{
	gchar *ans;
	gint ok;

	imap_gen_send(session, "LOGIN {%d}\r\n%s {%d}\r\n%s", 
				strlen(user), user, 
				strlen(pass), pass);
				
	ok = imap_gen_recv_with_block(session, &ans, TRUE);
	if (ok != IMAP_SUCCESS || ans[0] != '+' || ans[1] != ' ') {
		g_free(ans);
		return IMAP_ERROR;
	}
	g_free(ans);
	ok = imap_gen_recv_with_block(session, &ans, TRUE);
	if (ok != IMAP_SUCCESS || ans[0] != '+' || ans[1] != ' ') {
		g_free(ans);
		return IMAP_ERROR;
	}
	g_free(ans);
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

static gint imap_cmd_noop(IMAPSession *session)
{
	imap_gen_send(session, "NOOP");
	return imap_cmd_ok(session, NULL);
}

#if USE_OPENSSL
static gint imap_cmd_starttls(IMAPSession *session)
{
	imap_gen_send(session, "STARTTLS");
	return imap_cmd_ok(session, NULL);
}
#endif

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
			       guint32 *uid_validity, gboolean block)
{
	gint ok;
	gchar *resp_str;
	GPtrArray *argbuf;
	gchar *select_cmd;
	gchar *folder_;
	unsigned int uid_validity_;

	*exists = *recent = *unseen = *uid_validity = 0;
	argbuf = g_ptr_array_new();

	if (examine)
		select_cmd = "EXAMINE";
	else
		select_cmd = "SELECT";

	QUOTE_IF_REQUIRED(folder_, folder);
	imap_gen_send(session, "%s %s", select_cmd, folder_);

	if ((ok = imap_cmd_ok_with_block(session, argbuf, block)) != IMAP_SUCCESS) THROW;

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
		if (sscanf(resp_str, "OK [UIDVALIDITY %u] ", &uid_validity_)
		    != 1) {
			g_warning("imap_cmd_select(): invalid UIDVALIDITY line.\n");
			THROW;
		}
		*uid_validity = uid_validity_;
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
			    guint32 *uid_validity, gboolean block)
{
	return imap_cmd_do_select(session, folder, FALSE,
				  exists, recent, unseen, uid_validity, block);
}

static gint imap_cmd_examine(IMAPSession *session, const gchar *folder,
			     gint *exists, gint *recent, gint *unseen,
			     guint32 *uid_validity, gboolean block)
{
	return imap_cmd_do_select(session, folder, TRUE,
				  exists, recent, unseen, uid_validity, block);
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

static gint imap_cmd_search(IMAPSession *session, const gchar *criteria, 
			    GSList **list, gboolean block)
{
	gint ok;
	gchar *uidlist;
	GPtrArray *argbuf;

	g_return_val_if_fail(criteria != NULL, IMAP_ERROR);
	g_return_val_if_fail(list != NULL, IMAP_ERROR);

	*list = NULL;
	
	argbuf = g_ptr_array_new();
	imap_gen_send(session, "UID SEARCH %s", criteria);

	ok = imap_cmd_ok_with_block(session, argbuf, block);
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

			if (sscanf(*p, "%u", &msgnum) == 1)
				*list = g_slist_append(*list, GINT_TO_POINTER(msgnum));
		}
		g_strfreev(strlist);
	}
	ptr_array_free_strings(argbuf);
	g_ptr_array_free(argbuf, TRUE);

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
	
	gint ok;
	gchar *buf = NULL;
	gchar *cur_pos;
	gchar size_str[32];
	glong size_num;

	if (filename == NULL) {
		stuff->done = TRUE;
		return GINT_TO_POINTER(IMAP_ERROR);
	}
	
	if (stuff->body)
		imap_gen_send(session, "UID FETCH %d BODY.PEEK[]", uid);
	else
		imap_gen_send(session, "UID FETCH %d BODY.PEEK[HEADER]",
				uid);
	while ((ok = imap_gen_recv_block(session, &buf)) == IMAP_SUCCESS) {
		if (buf[0] != '*' || buf[1] != ' ') {
			g_free(buf);
			stuff->done = TRUE;
			return GINT_TO_POINTER(IMAP_ERROR);
		}
		if (strstr(buf, "FETCH") != NULL) break;
		g_free(buf);
	}
	if (ok != IMAP_SUCCESS) {
		g_free(buf);
		stuff->done = TRUE;
		return GINT_TO_POINTER(ok);
	}

#define RETURN_ERROR_IF_FAIL(cond)	\
	if (!(cond)) {			\
		g_free(buf);		\
		stuff->done = TRUE;	\
		return GINT_TO_POINTER(IMAP_ERROR);	\
	}

	cur_pos = strchr(buf, '{');
	RETURN_ERROR_IF_FAIL(cur_pos != NULL);
	cur_pos = strchr_cpy(cur_pos + 1, '}', size_str, sizeof(size_str));
	RETURN_ERROR_IF_FAIL(cur_pos != NULL);
	size_num = atol(size_str);
	RETURN_ERROR_IF_FAIL(size_num >= 0);

	RETURN_ERROR_IF_FAIL(*cur_pos == '\0');

#undef RETURN_ERROR_IF_FAIL

	g_free(buf);

	if (recv_bytes_write_to_file(SESSION(session)->sock,
				     size_num, filename) != 0) {
		stuff->done = TRUE;
		return GINT_TO_POINTER(IMAP_ERROR);
	}
	if (imap_gen_recv_block(session, &buf) != IMAP_SUCCESS) {
		g_free(buf);
		stuff->done = TRUE;
		return GINT_TO_POINTER(IMAP_ERROR);
	}

	if (buf[0] == '\0' || buf[strlen(buf) - 1] != ')') {
		g_free(buf);
		stuff->done = TRUE;
		return GINT_TO_POINTER(IMAP_ERROR);
	}
	g_free(buf);

	ok = imap_cmd_ok_block(session, NULL);

	stuff->done = TRUE;
	return GINT_TO_POINTER(ok);
}

static gint imap_cmd_fetch(IMAPSession *session, guint32 uid,
				const gchar *filename, gboolean headers,
				gboolean body)
{
	fetch_data *data = g_new0(fetch_data, 1);
	int result = 0;
#ifdef USE_PTHREAD
	void *tmp;
	pthread_t pt;
#endif
	data->done = FALSE;
	data->session = session;
	data->uid = uid;
	data->filename = filename;
	data->headers = headers;
	data->body = body;

	if (prefs_common.work_offline && !imap_gtk_should_override()) {
		g_free(data);
		return -1;
	}

#if (defined USE_PTHREAD && defined __GLIBC__ && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 3)))
	MUTEX_TRYLOCK_OR_RETURN_VAL(-1);
	if (pthread_create(&pt, PTHREAD_CREATE_JOINABLE,
			imap_cmd_fetch_thread, data) != 0) {
		result = GPOINTER_TO_INT(imap_cmd_fetch_thread(data));
		g_free(data);
		MUTEX_UNLOCK();
		return result;
	}
	debug_print("+++waiting for imap_cmd_fetch_thread...\n");
	while(!data->done) {
		/* don't let the interface freeze while waiting */
		sylpheed_do_idle();
	}
	debug_print("---imap_cmd_fetch_thread done\n");

	/* get the thread's return value and clean its resources */
	pthread_join(pt, &tmp);
	result = GPOINTER_TO_INT(tmp);
	MUTEX_UNLOCK();
#else
	result = GPOINTER_TO_INT(imap_cmd_fetch_thread(data));
#endif
	g_free(data);
	return result;
}

static gint imap_cmd_append(IMAPSession *session, const gchar *destfolder,
			    const gchar *file, IMAPFlags flags, 
			    guint32 *new_uid)
{
	gint ok;
	gint size;
	gchar *destfolder_;
	gchar *flag_str;
	unsigned int new_uid_;
	gchar *ret = NULL;
	gchar buf[BUFFSIZE];
	FILE *fp;
	GPtrArray *argbuf;
	gchar *resp_str;

	g_return_val_if_fail(file != NULL, IMAP_ERROR);

	size = get_file_size_as_crlf(file);
	if ((fp = fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		return -1;
	}
	QUOTE_IF_REQUIRED(destfolder_, destfolder);
	flag_str = imap_get_flag_str(flags);
	imap_gen_send(session, "APPEND %s (%s) {%d}", 
		      destfolder_, flag_str, size);
	g_free(flag_str);

	ok = imap_gen_recv(session, &ret);
	if (ok != IMAP_SUCCESS || ret[0] != '+' || ret[1] != ' ') {
		log_warning(_("can't append %s to %s\n"), file, destfolder_);
		g_free(ret);
		fclose(fp);
		return IMAP_ERROR;
	}
	g_free(ret);

	log_print("IMAP4> %s\n", "(sending file...)");

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

	if (new_uid != NULL)
		*new_uid = 0;

	if (new_uid != NULL && session->uidplus) {
		argbuf = g_ptr_array_new();

		ok = imap_cmd_ok(session, argbuf);
		if ((ok == IMAP_SUCCESS) && (argbuf->len > 0)) {
			resp_str = g_ptr_array_index(argbuf, argbuf->len - 1);
			if (resp_str &&
			    sscanf(resp_str, "%*u OK [APPENDUID %*u %u]",
				   &new_uid_) == 1) {
				*new_uid = new_uid_;
			}
		}

		ptr_array_free_strings(argbuf);
		g_ptr_array_free(argbuf, TRUE);
	} else
		ok = imap_cmd_ok(session, NULL);

	if (ok != IMAP_SUCCESS)
		log_warning(_("can't append message to %s\n"),
			    destfolder_);

	return ok;
}

static MsgNumberList *imapset_to_numlist(IMAPSet imapset)
{
	gchar **ranges, **range;
	unsigned int low, high;
	MsgNumberList *uids = NULL;
	
	ranges = g_strsplit(imapset, ",", 0);
	for (range = ranges; *range != NULL; range++) {
		if (sscanf(*range, "%u:%u", &low, &high) == 1)
			uids = g_slist_prepend(uids, GINT_TO_POINTER(low));
		else {
			int i;
			for (i = low; i <= high; i++)
				uids = g_slist_prepend(uids, GINT_TO_POINTER(i));
		}
	}
	uids = g_slist_reverse(uids);
	g_strfreev(ranges);

	return uids;
}

static gint imap_cmd_copy(IMAPSession *session, const gchar *seq_set,
			  const gchar *destfolder, GRelation *uid_mapping)
{
	gint ok;
	gchar *destfolder_;
	
	g_return_val_if_fail(session != NULL, IMAP_ERROR);
	g_return_val_if_fail(seq_set != NULL, IMAP_ERROR);
	g_return_val_if_fail(destfolder != NULL, IMAP_ERROR);

	QUOTE_IF_REQUIRED(destfolder_, destfolder);
	imap_gen_send(session, "UID COPY %s %s", seq_set, destfolder_);

	if (uid_mapping != NULL && session->uidplus) {
		GPtrArray *reply;		
		gchar *resp_str = NULL, *olduids_str, *newuids_str;
		MsgNumberList *olduids, *old_cur, *newuids, *new_cur;

		reply = g_ptr_array_new();
		ok = imap_cmd_ok(session, reply);
		if ((ok == IMAP_SUCCESS) && (reply->len > 0)) {
			resp_str = g_ptr_array_index(reply, reply->len - 1);
			if (resp_str) {
				olduids_str = g_new0(gchar, strlen(resp_str));
				newuids_str = g_new0(gchar, strlen(resp_str));
				if (sscanf(resp_str, "%*s OK [COPYUID %*u %[0-9,:] %[0-9,:]]",
					   olduids_str, newuids_str) == 2) {
					olduids = imapset_to_numlist(olduids_str);
					newuids = imapset_to_numlist(newuids_str);

					old_cur = olduids;
					new_cur = newuids;
					while(old_cur != NULL && new_cur != NULL) {
						g_relation_insert(uid_mapping, 
								  GPOINTER_TO_INT(old_cur->data),
								  GPOINTER_TO_INT(new_cur->data));
					        old_cur = g_slist_next(old_cur);
						new_cur = g_slist_next(new_cur);
					}

					g_slist_free(olduids);
					g_slist_free(newuids);
				}
				g_free(olduids_str);
				g_free(newuids_str);
			}
		}
		ptr_array_free_strings(reply);
		g_ptr_array_free(reply, TRUE);
	} else
		ok = imap_cmd_ok(session, NULL);

	if (ok != IMAP_SUCCESS)
		log_warning(_("can't copy %s to %s\n"), seq_set, destfolder_);

	return ok;
}

gint imap_cmd_envelope(IMAPSession *session, IMAPSet set)
{
	static gchar *header_fields = 
		"Date From To Cc Subject Message-ID References In-Reply-To" ;

	imap_gen_send
		(session, "UID FETCH %s (UID FLAGS RFC822.SIZE BODY.PEEK[HEADER.FIELDS (%s)])",
		 set, header_fields);

	return IMAP_SUCCESS;
}

static gint imap_cmd_store(IMAPSession *session, IMAPSet seq_set,
			   gchar *sub_cmd)
{
	gint ok;

	imap_gen_send(session, "UID STORE %s %s", seq_set, sub_cmd);

	if ((ok = imap_cmd_ok(session, NULL)) != IMAP_SUCCESS) {
		log_warning(_("error while imap command: STORE %s %s\n"),
			    seq_set, sub_cmd);
		return ok;
	}

	return IMAP_SUCCESS;
}

typedef struct _expunge_data {
	IMAPSession *session;
	IMAPSet seq_set;
	gboolean done;
} expunge_data;

static void *imap_cmd_expunge_thread(void *data)
{
	expunge_data *stuff = (expunge_data *)data;
	IMAPSession *session = stuff->session;
	IMAPSet seq_set = stuff->seq_set;

	gint ok;

	if (seq_set && session->uidplus)
		imap_gen_send(session, "UID EXPUNGE %s", seq_set);
	else	
		imap_gen_send(session, "EXPUNGE");
	if ((ok = imap_cmd_ok_with_block(session, NULL, TRUE)) != IMAP_SUCCESS) {
		log_warning(_("error while imap command: EXPUNGE\n"));
		stuff->done = TRUE;
		return GINT_TO_POINTER(ok);
	}

	stuff->done = TRUE;
	return GINT_TO_POINTER(IMAP_SUCCESS);
}

static gint imap_cmd_expunge(IMAPSession *session, IMAPSet seq_set)
{
	expunge_data *data = g_new0(expunge_data, 1);
	int result;
#ifdef USE_PTHREAD
	void *tmp;
	pthread_t pt;
#endif
	data->done = FALSE;
	data->session = session;
	data->seq_set = seq_set;

	if (prefs_common.work_offline && !imap_gtk_should_override()) {
		g_free(data);
		return -1;
	}

#if (defined USE_PTHREAD && defined __GLIBC__ && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 3)))
	if (pthread_create(&pt, PTHREAD_CREATE_JOINABLE,
			imap_cmd_expunge_thread, data) != 0) {
		result = GPOINTER_TO_INT(imap_cmd_expunge_thread(data));
		g_free(data);
		return result;
	}
	debug_print("+++waiting for imap_cmd_expunge_thread...\n");
	while(!data->done) {
		/* don't let the interface freeze while waiting */
		sylpheed_do_idle();
	}
	debug_print("---imap_cmd_expunge_thread done\n");

	/* get the thread's return value and clean its resources */
	pthread_join(pt, &tmp);
	result = GPOINTER_TO_INT(tmp);
#else
	result = GPOINTER_TO_INT(imap_cmd_expunge_thread(data));
#endif
	g_free(data);
	return result;
}

static gint imap_cmd_close(IMAPSession *session)
{
	gint ok;

	imap_gen_send(session, "CLOSE");
	if ((ok = imap_cmd_ok(session, NULL)) != IMAP_SUCCESS)
		log_warning(_("error while imap command: CLOSE\n"));

	return ok;
}

static gint imap_cmd_ok_with_block(IMAPSession *session, GPtrArray *argbuf, gboolean block)
{
	gint ok = IMAP_SUCCESS;
	gchar *buf;
	gint cmd_num;
	gchar *data;

	while ((ok = imap_gen_recv_with_block(session, &buf, block))
	       == IMAP_SUCCESS) {
		/* make sure data is long enough for any substring of buf */
		data = alloca(strlen(buf) + 1);

		/* untagged line read */
		if (buf[0] == '*' && buf[1] == ' ') {
			gint num;
			if (argbuf)
				g_ptr_array_add(argbuf, g_strdup(buf + 2));

			if (sscanf(buf + 2, "%d %s", &num, data) >= 2) {
				if (!strcmp(data, "EXISTS")) {
					session->exists = num;
					session->folder_content_changed = TRUE;
				}

				if(!strcmp(data, "EXPUNGE")) {
					session->exists--;
					session->folder_content_changed = TRUE;
				}
			}
		/* tagged line with correct tag and OK response found */
		} else if ((sscanf(buf, "%d %s", &cmd_num, data) >= 2) &&
			   (cmd_num == session->cmd_count) &&
			   !strcmp(data, "OK")) {
			if (argbuf)
				g_ptr_array_add(argbuf, g_strdup(buf));
			break;
		/* everything else */
		} else {
			ok = IMAP_ERROR;
			break;
		}
		g_free(buf);
	}
	g_free(buf);

	return ok;
}
static gint imap_cmd_ok(IMAPSession *session, GPtrArray *argbuf)
{
	return imap_cmd_ok_with_block(session, argbuf, FALSE);
}
static gint imap_cmd_ok_block(IMAPSession *session, GPtrArray *argbuf)
{
	return imap_cmd_ok_with_block(session, argbuf, TRUE);
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
	if (!g_ascii_strncasecmp(tmp, "LOGIN ", 6) && (p = strchr(tmp + 6, ' '))) {
		*p = '\0';
		log_print("IMAP4> %d %s ********\n", session->cmd_count, tmp);
	} else
		log_print("IMAP4> %d %s\n", session->cmd_count, tmp);

	sock_write_all(SESSION(session)->sock, buf, strlen(buf));
	g_free(tmp);
	g_free(buf);
}

static gint imap_gen_recv_with_block(IMAPSession *session, gchar **ret, gboolean block)
{
	if (!block) {
		if ((*ret = imap_getline(SESSION(session)->sock)) == NULL)
			return IMAP_SOCKET;
	} else {
		if ((*ret = sock_getline(SESSION(session)->sock)) == NULL)
			return IMAP_SOCKET;
	}
	strretchomp(*ret);

	log_print("IMAP4< %s\n", *ret);
	
	session_set_access_time(SESSION(session));

	return IMAP_SUCCESS;
}

static gint imap_gen_recv_block(IMAPSession *session, gchar **ret)
{
	return imap_gen_recv_with_block(session, ret, TRUE);
}

static gint imap_gen_recv(IMAPSession *session, gchar **ret)
{
	return imap_gen_recv_with_block(session, ret, FALSE);
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

static GSList *imap_get_seq_set_from_numlist(MsgNumberList *numlist)
{
	GString *str;
	GSList *sorted_list, *cur;
	guint first, last, next;
	gchar *ret_str;
	GSList *ret_list = NULL;

	if (numlist == NULL)
		return NULL;

	str = g_string_sized_new(256);

	sorted_list = g_slist_copy(numlist);
	sorted_list = g_slist_sort(sorted_list, g_int_compare);

	first = GPOINTER_TO_INT(sorted_list->data);

	for (cur = sorted_list; cur != NULL; cur = g_slist_next(cur)) {
		if (GPOINTER_TO_INT(cur->data) == 0)
			continue;

		last = GPOINTER_TO_INT(cur->data);
		if (cur->next)
			next = GPOINTER_TO_INT(cur->next->data);
		else
			next = 0;

		if (last + 1 != next || next == 0) {
			if (str->len > 0)
				g_string_append_c(str, ',');
			if (first == last)
				g_string_append_printf(str, "%u", first);
			else
				g_string_append_printf(str, "%u:%u", first, last);

			first = next;

			if (str->len > IMAP_CMD_LIMIT) {
				ret_str = g_strdup(str->str);
				ret_list = g_slist_append(ret_list, ret_str);
				g_string_truncate(str, 0);
			}
		}
	}

	if (str->len > 0) {
		ret_str = g_strdup(str->str);
		ret_list = g_slist_append(ret_list, ret_str);
	}

	g_slist_free(sorted_list);
	g_string_free(str, TRUE);

	return ret_list;
}

static GSList *imap_get_seq_set_from_msglist(MsgInfoList *msglist)
{
	MsgNumberList *numlist = NULL;
	MsgInfoList *cur;
	GSList *seq_list;

	for (cur = msglist; cur != NULL; cur = g_slist_next(cur)) {
		MsgInfo *msginfo = (MsgInfo *) cur->data;

		numlist = g_slist_append(numlist, GINT_TO_POINTER(msginfo->msgnum));
	}
	seq_list = imap_get_seq_set_from_numlist(numlist);
	g_slist_free(numlist);

	return seq_list;
}

static void imap_seq_set_free(GSList *seq_list)
{
	slist_free_strings(seq_list);
	g_slist_free(seq_list);
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
	gchar *cmd_buf;

	session = imap_session_get(folder);
	if (session == NULL) {
		stuff->done = TRUE;
		return GINT_TO_POINTER(-1);
	}

	ok = imap_select(session, IMAP_FOLDER(folder), item->item.path,
			 NULL, NULL, NULL, NULL, TRUE);
	if (ok != IMAP_SUCCESS) {
		stuff->done = TRUE;
		return GINT_TO_POINTER(-1);
	}

	cmd_buf = g_strdup_printf("UID %d:*", item->lastuid + 1);
	ok = imap_cmd_search(session, cmd_buf, &uidlist, TRUE);
	g_free(cmd_buf);

	if (ok == IMAP_SOCKET) {
		session_destroy((Session *)session);
		((RemoteFolder *)folder)->session = NULL;
		stuff->done = TRUE;
		return GINT_TO_POINTER(-1);
	}

	if (ok != IMAP_SUCCESS) {
		gint i;
		GPtrArray *argbuf;

		argbuf = g_ptr_array_new();

		cmd_buf = g_strdup_printf("UID FETCH %d:* (UID)", item->lastuid + 1);
		imap_gen_send(session, cmd_buf);
		g_free(cmd_buf);
		ok = imap_cmd_ok_block(session, argbuf);
		if (ok != IMAP_SUCCESS) {
			ptr_array_free_strings(argbuf);
			g_ptr_array_free(argbuf, TRUE);
			stuff->done = TRUE;
			return GINT_TO_POINTER(-1);
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

	stuff->done = TRUE;
	return GINT_TO_POINTER(nummsgs);
}

static gint get_list_of_uids(Folder *folder, IMAPFolderItem *item, GSList **msgnum_list)
{
	gint result;
	get_list_uid_data *data = g_new0(get_list_uid_data, 1);
#ifdef USE_PTHREAD
	void *tmp;
	pthread_t pt;
#endif
	data->done = FALSE;
	data->folder = folder;
	data->item = item;
	data->msgnum_list = msgnum_list;

	if (prefs_common.work_offline && !imap_gtk_should_override()) {
		g_free(data);
		return -1;
	}

#if (defined USE_PTHREAD && defined __GLIBC__ && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 3)))
	if (pthread_create(&pt, PTHREAD_CREATE_JOINABLE,
			get_list_of_uids_thread, data) != 0) {
		result = GPOINTER_TO_INT(get_list_of_uids_thread(data));
		g_free(data);
		return result;
	}
	debug_print("+++waiting for get_list_of_uids_thread...\n");
	while(!data->done) {
		/* don't let the interface freeze while waiting */
		sylpheed_do_idle();
	}
	debug_print("---get_list_of_uids_thread done\n");

	/* get the thread's return value and clean its resources */
	pthread_join(pt, &tmp);
	result = GPOINTER_TO_INT(tmp);
#else
	result = GPOINTER_TO_INT(get_list_of_uids_thread(data));
#endif
	g_free(data);
	return result;

}

gint imap_get_num_list(Folder *folder, FolderItem *_item, GSList **msgnum_list, gboolean *old_uids_valid)
{
	IMAPFolderItem *item = (IMAPFolderItem *)_item;
	IMAPSession *session;
	gint ok, nummsgs = 0, exists, recent, uid_val, uid_next, unseen;
	GSList *uidlist = NULL;
	gchar *dir;
	gboolean selected_folder;
	
	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(item != NULL, -1);
	g_return_val_if_fail(item->item.path != NULL, -1);
	g_return_val_if_fail(FOLDER_CLASS(folder) == &imap_class, -1);
	g_return_val_if_fail(folder->account != NULL, -1);

	MUTEX_TRYLOCK_OR_RETURN_VAL(-1);

	session = imap_session_get(folder);
	g_return_val_if_fail(session != NULL, -1);

	selected_folder = (session->mbox != NULL) &&
			  (!strcmp(session->mbox, item->item.path));
	if (selected_folder) {
		ok = imap_cmd_noop(session);
		if (ok != IMAP_SUCCESS) {
			MUTEX_UNLOCK();
			return -1;
		}
		exists = session->exists;

		*old_uids_valid = TRUE;
	} else {
		ok = imap_status(session, IMAP_FOLDER(folder), item->item.path,
				 &exists, &recent, &uid_next, &uid_val, &unseen, FALSE);
		if (ok != IMAP_SUCCESS) {
			MUTEX_UNLOCK();
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
			MUTEX_UNLOCK();
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
		MUTEX_UNLOCK();
		return 0;
	}

	nummsgs = get_list_of_uids(folder, item, &uidlist);

	if (nummsgs < 0) {
		MUTEX_UNLOCK();
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

		nummsgs = get_list_of_uids(folder, item, &uidlist);
	}

	*msgnum_list = uidlist;

	dir = folder_item_get_path((FolderItem *)item);
	debug_print("removing old messages from %s\n", dir);
	remove_numbered_files_not_in_list(dir, *msgnum_list);
	g_free(dir);
	MUTEX_UNLOCK();

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
	
	msginfo->plaintext_file = g_strdup(file);
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

	debug_print("IMAP getting msginfos\n");
	ok = imap_select(session, IMAP_FOLDER(folder), item->path,
			 NULL, NULL, NULL, NULL, FALSE);
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
	gint ok, exists = 0, recent = 0, unseen = 0;
	guint32 uid_next, uid_val = 0;
	gboolean selected_folder;
	
	g_return_val_if_fail(folder != NULL, FALSE);
	g_return_val_if_fail(item != NULL, FALSE);
	g_return_val_if_fail(item->item.folder != NULL, FALSE);
	g_return_val_if_fail(FOLDER_CLASS(item->item.folder) == &imap_class, FALSE);

	if (item->item.path == NULL)
		return FALSE;

	session = imap_session_get(folder);
	g_return_val_if_fail(session != NULL, FALSE);

	selected_folder = (session->mbox != NULL) &&
			  (!strcmp(session->mbox, item->item.path));
	if (selected_folder) {
		ok = imap_cmd_noop(session);
		if (ok != IMAP_SUCCESS)
			return FALSE;

		if (session->folder_content_changed)
			return TRUE;
	} else {
		ok = imap_status(session, IMAP_FOLDER(folder), item->item.path,
				 &exists, &recent, &uid_next, &uid_val, &unseen, FALSE);
		if (ok != IMAP_SUCCESS)
			return FALSE;

		if ((uid_next != item->uid_next) || (exists != item->item.total_msgs))
			return TRUE;
	}

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

	MUTEX_TRYLOCK_OR_RETURN();

	session = imap_session_get(folder);
	if (!session) {
		MUTEX_UNLOCK();
		return;
	}
	if ((ok = imap_select(session, IMAP_FOLDER(folder), msginfo->folder->path,
	    NULL, NULL, NULL, NULL, FALSE)) != IMAP_SUCCESS) {
		MUTEX_UNLOCK();
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
		flags_set |= IMAP_FLAG_ANSWERED;

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
			ht_data = g_hash_table_lookup(flags_set_table, GINT_TO_POINTER(flags_set));
			if (ht_data == NULL) {
				ht_data = g_new0(hashtable_data, 1);
				ht_data->session = session;
				g_hash_table_insert(flags_set_table, GINT_TO_POINTER(flags_set), ht_data);
			}
			if (!g_slist_find(ht_data->msglist, GINT_TO_POINTER(msginfo->msgnum)))
				ht_data->msglist = g_slist_prepend(ht_data->msglist, GINT_TO_POINTER(msginfo->msgnum));
		} 
		if (flags_unset) {
			ht_data = g_hash_table_lookup(flags_unset_table, GINT_TO_POINTER(flags_unset));
			if (ht_data == NULL) {
				ht_data = g_new0(hashtable_data, 1);
				ht_data->session = session;
				g_hash_table_insert(flags_unset_table, GINT_TO_POINTER(flags_unset), ht_data);
			}
			if (!g_slist_find(ht_data->msglist, GINT_TO_POINTER(msginfo->msgnum)))
				ht_data->msglist = g_slist_prepend(ht_data->msglist, GINT_TO_POINTER(msginfo->msgnum));		
		}
	} else {
		debug_print("IMAP changing flags\n");
		if (flags_set) {
			ok = imap_set_message_flags(session, &numlist, flags_set, TRUE);
			if (ok != IMAP_SUCCESS) {
				MUTEX_UNLOCK();
				return;
			}
		}

		if (flags_unset) {
			ok = imap_set_message_flags(session, &numlist, flags_unset, FALSE);
			if (ok != IMAP_SUCCESS) {
				MUTEX_UNLOCK();
				return;
			}
		}
	}
	msginfo->flags.perm_flags = newflags;
	
	MUTEX_UNLOCK();
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

	ok = imap_select(session, IMAP_FOLDER(folder), item->path,
			 NULL, NULL, NULL, NULL, FALSE);
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

	if (!session->uidplus) {
		ok = imap_cmd_expunge(session, NULL);
	} else {
		gchar *uidstr;

		uidstr = g_strdup_printf("%u", uid);
		ok = imap_cmd_expunge(session, uidstr);
		g_free(uidstr);
	}
	if (ok != IMAP_SUCCESS) {
		log_warning(_("can't expunge\n"));
		return ok;
	}

	IMAP_FOLDER_ITEM(item)->uid_list = g_slist_remove(
	    IMAP_FOLDER_ITEM(item)->uid_list, numlist.data);
	dir = folder_item_get_path(item);
	if (is_dir_exist(dir))
		remove_numbered_files(dir, uid, uid);
	g_free(dir);

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
	gboolean done;
} get_flags_data;

static /*gint*/ void *imap_get_flags_thread(void *data)
{
	get_flags_data *stuff = (get_flags_data *)data;
	Folder *folder = stuff->folder;
	FolderItem *item = stuff->item;
	MsgInfoList *msginfo_list = stuff->msginfo_list;
	GRelation *msgflags = stuff->msgflags;
	IMAPSession *session;
	GSList *sorted_list;
	GSList *unseen = NULL, *answered = NULL, *flagged = NULL;
	GSList *p_unseen, *p_answered, *p_flagged;
	GSList *elem;
	GSList *seq_list, *cur;
	gboolean reverse_seen = FALSE;
	GString *cmd_buf;
	gint ok;
	gint exists_cnt, recent_cnt, unseen_cnt, uid_next;
	guint32 uidvalidity;
	gboolean selected_folder;
	
	if (folder == NULL || item == NULL) {
		stuff->done = TRUE;
		return GINT_TO_POINTER(-1);
	}
	if (msginfo_list == NULL) {
		stuff->done = TRUE;
		return GINT_TO_POINTER(0);
	}

	session = imap_session_get(folder);
	if (session == NULL) {
		stuff->done = TRUE;
		return GINT_TO_POINTER(-1);
	}

	selected_folder = (session->mbox != NULL) &&
			  (!strcmp(session->mbox, item->path));

	if (!selected_folder) {
		ok = imap_status(session, IMAP_FOLDER(folder), item->path,
			 &exists_cnt, &recent_cnt, &uid_next, &uidvalidity, &unseen_cnt, TRUE);
		ok = imap_select(session, IMAP_FOLDER(folder), item->path,
			NULL, NULL, NULL, NULL, TRUE);
		if (ok != IMAP_SUCCESS) {
			stuff->done = TRUE;
			return GINT_TO_POINTER(-1);
		}

	} 

	if (unseen_cnt > exists_cnt / 2)
		reverse_seen = TRUE;

	cmd_buf = g_string_new(NULL);

	sorted_list = g_slist_sort(g_slist_copy(msginfo_list), compare_msginfo);

	seq_list = imap_get_seq_set_from_msglist(msginfo_list);

	for (cur = seq_list; cur != NULL; cur = g_slist_next(cur)) {
		IMAPSet imapset = cur->data;

		g_string_sprintf(cmd_buf, "%sSEEN UID %s", reverse_seen ? "" : "UN", imapset);
		imap_cmd_search(session, cmd_buf->str, &p_unseen, TRUE);
		unseen = g_slist_concat(unseen, p_unseen);

		g_string_sprintf(cmd_buf, "ANSWERED UID %s", imapset);
		imap_cmd_search(session, cmd_buf->str, &p_answered, TRUE);
		answered = g_slist_concat(answered, p_answered);

		g_string_sprintf(cmd_buf, "FLAGGED UID %s", imapset);
		imap_cmd_search(session, cmd_buf->str, &p_flagged, TRUE);
		flagged = g_slist_concat(flagged, p_flagged);
	}

	p_unseen = unseen;
	p_answered = answered;
	p_flagged = flagged;

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
		if (gslist_find_next_num(&p_flagged, msginfo->msgnum) == msginfo->msgnum)
			flags |= MSG_MARKED;
		g_relation_insert(msgflags, msginfo, GINT_TO_POINTER(flags));
	}

	imap_seq_set_free(seq_list);
	g_slist_free(flagged);
	g_slist_free(answered);
	g_slist_free(unseen);
	g_slist_free(sorted_list);
	g_string_free(cmd_buf, TRUE);

	stuff->done = TRUE;
	return GINT_TO_POINTER(0);
}

static gint imap_get_flags(Folder *folder, FolderItem *item,
                           MsgInfoList *msginfo_list, GRelation *msgflags)
{
	gint result;
	get_flags_data *data = g_new0(get_flags_data, 1);
#ifdef USE_PTHREAD
	void *tmp;
	pthread_t pt;
#endif
	data->done = FALSE;
	data->folder = folder;
	data->item = item;
	data->msginfo_list = msginfo_list;
	data->msgflags = msgflags;

	if (prefs_common.work_offline && !imap_gtk_should_override()) {
		g_free(data);
		return -1;
	}

#if (defined USE_PTHREAD && defined __GLIBC__ && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 3)))
	MUTEX_TRYLOCK_OR_RETURN_VAL(-1);
	if (pthread_create(&pt, PTHREAD_CREATE_JOINABLE,
			imap_get_flags_thread, data) != 0) {
		result = GPOINTER_TO_INT(imap_get_flags_thread(data));
		g_free(data);
		MUTEX_UNLOCK();
		return result;
	}
	debug_print("+++waiting for imap_get_flags_thread...\n");
	while(!data->done) {
		/* don't let the interface freeze while waiting */
		sylpheed_do_idle();
	}
	debug_print("---imap_get_flags_thread done\n");

	/* get the thread's return value and clean its resources */
	pthread_join(pt, &tmp);
	result = GPOINTER_TO_INT(tmp);
	MUTEX_UNLOCK();
#else
	result = GPOINTER_TO_INT(imap_get_flags_thread(data));
#endif
	g_free(data);
	return result;

}

static gboolean process_flags(gpointer key, gpointer value, gpointer user_data)
{
	gboolean flags_set = GPOINTER_TO_INT(user_data);
	gint flags_value = GPOINTER_TO_INT(key);
	hashtable_data *data = (hashtable_data *)value;
	
	data->msglist = g_slist_reverse(data->msglist);
	
	debug_print("IMAP %ssetting flags to %d for %d messages\n",
		flags_set?"":"un",
		flags_value,
		g_slist_length(data->msglist));
	imap_set_message_flags(data->session, data->msglist, flags_value, flags_set);
	
	g_slist_free(data->msglist);	
	g_free(data);
	return TRUE;
}

static void process_hashtable(void)
{
	MUTEX_TRYLOCK_OR_RETURN();
	if (flags_set_table) {
		g_hash_table_foreach_remove(flags_set_table, process_flags, GINT_TO_POINTER(TRUE));
		g_free(flags_set_table);
		flags_set_table = NULL;
	}
	if (flags_unset_table) {
		g_hash_table_foreach_remove(flags_unset_table, process_flags, GINT_TO_POINTER(FALSE));
		g_free(flags_unset_table);
		flags_unset_table = NULL;
	}
	MUTEX_UNLOCK();
}

static IMAPFolderItem *batching_item = NULL;

static void imap_set_batch (Folder *folder, FolderItem *_item, gboolean batch)
{
	IMAPFolderItem *item = (IMAPFolderItem *)_item;

	g_return_if_fail(item != NULL);
	
	if (batch && batching_item != NULL) {
		g_warning("already batching on %s\n", batching_item->item.path);
		return;
	}
	
	if (item->batching == batch)
		return;
	
	item->batching = batch;
	
	batching_item = batch?item:NULL;
	
	if (batch) {
		debug_print("IMAP switching to batch mode\n");
		if (flags_set_table) {
			g_warning("flags_set_table non-null but we just entered batch mode!\n");
			flags_set_table = NULL;
		}
		if (flags_unset_table) {
			g_warning("flags_unset_table non-null but we just entered batch mode!\n");
			flags_unset_table = NULL;
		}
		flags_set_table = g_hash_table_new(NULL, g_direct_equal);
		flags_unset_table = g_hash_table_new(NULL, g_direct_equal);
	} else {
		debug_print("IMAP switching away from batch mode\n");
		/* process stuff */
		process_hashtable();
	}
}
