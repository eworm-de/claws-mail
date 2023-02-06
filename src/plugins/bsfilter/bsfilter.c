/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2023 the Claws Mail team and
 * Colin Leroy <colin@colino.net>
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

#include <glib.h>
#include <glib/gi18n.h>

#include "defs.h"

#include <sys/types.h>
#if HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef USE_PTHREAD
#include <pthread.h>
#endif

#include "common/claws.h"
#include "common/version.h"
#include "plugin.h"
#include "common/utils.h"
#include "hooks.h"
#include "procmsg.h"
#include "folder.h"
#include "prefs.h"
#include "prefs_gtk.h"
#include "utils.h"

#include "bsfilter.h"
#include "main.h"
#include "inc.h"
#include "log.h"
#include "prefs_common.h"
#include "alertpanel.h"
#include "addr_compl.h"

#ifdef HAVE_SYSEXITS_H
#include <sysexits.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_SYS_ERRNO_H
#include <sys/errno.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif

#define PLUGIN_NAME (_("Bsfilter"))

static gulong hook_id = HOOK_NONE;
static MessageCallback message_callback;

static BsfilterConfig config;

extern SessionStats session_stats;

static PrefParam param[] = {
	{"process_emails", "TRUE", &config.process_emails, P_BOOL,
	 NULL, NULL, NULL},
	{"receive_spam", "TRUE", &config.receive_spam, P_BOOL,
	 NULL, NULL, NULL},
	{"save_folder", NULL, &config.save_folder, P_STRING,
	 NULL, NULL, NULL},
	{"max_size", "250", &config.max_size, P_INT,
	 NULL, NULL, NULL},
#ifndef G_OS_WIN32
	{"bspath", "bsfilter", &config.bspath, P_STRING,
	 NULL, NULL, NULL},
#else
	{"bspath", "bsfilterw.exe", &config.bspath, P_STRING,
	 NULL, NULL, NULL},
#endif
	{"whitelist_ab", "FALSE", &config.whitelist_ab, P_BOOL,
	 NULL, NULL, NULL},
	{"whitelist_ab_folder", N_("Any"), &config.whitelist_ab_folder, P_STRING,
	 NULL, NULL, NULL},
	{"learn_from_whitelist", "FALSE", &config.learn_from_whitelist, P_BOOL,
	 NULL, NULL, NULL},
	{"mark_as_read", "TRUE", &config.mark_as_read, P_BOOL,
	 NULL, NULL, NULL},

	{NULL, NULL, NULL, P_OTHER, NULL, NULL, NULL}
};

typedef struct _BsFilterData {
	MailFilteringData *mail_filtering_data;
	gchar **bs_args;
	MsgInfo *msginfo;
	gboolean done;
	int status;
	int whitelisted;
	gboolean in_thread;
} BsFilterData;

static BsFilterData *to_filter_data = NULL;
#ifdef USE_PTHREAD
static gboolean filter_th_done = FALSE;
static pthread_mutex_t list_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t wait_mutex = PTHREAD_MUTEX_INITIALIZER; 
static pthread_cond_t wait_cond = PTHREAD_COND_INITIALIZER; 
#endif

static void bsfilter_do_filter(BsFilterData *data)
{
	int status = 0;
	gchar *file = NULL;
	gboolean whitelisted = FALSE;
	MsgInfo *msginfo = to_filter_data->msginfo;

	if (config.whitelist_ab) {
		gchar *ab_folderpath;

		if (*config.whitelist_ab_folder == '\0' ||
			strcasecmp(config.whitelist_ab_folder, "Any") == 0) {
			/* match the whole addressbook */
			ab_folderpath = NULL;
		} else {
			/* match the specific book/folder of the addressbook */
			ab_folderpath = config.whitelist_ab_folder;
		}

		start_address_completion(ab_folderpath);
	}

	debug_print("Filtering message %d\n", msginfo->msgnum);

	if (config.whitelist_ab && msginfo->from && 
	    found_in_addressbook(msginfo->from))
		whitelisted = TRUE;

	/* can set flags (SCANNED, ATTACHMENT) but that's ok 
	 * as GUI updates are hooked not direct */

	file = procmsg_get_message_file(msginfo);

	if (file) {
#ifndef G_OS_WIN32
		gchar *classify = g_strconcat((config.bspath && *config.bspath) ? config.bspath:"bsfilter",
			" --homedir '",get_rc_dir(),"' '", file, "'", NULL);
#else
		gchar *classify = g_strconcat((config.bspath && *config.bspath) ? config.bspath:"bsfilterw.exe",
			" --homedir '",get_rc_dir(),"' '", file, "'", NULL);
#endif
		status = execute_command_line(classify, FALSE,
				claws_get_startup_dir());
		g_free(classify);
	}

	if (config.whitelist_ab)
		end_address_completion();

	to_filter_data->status = status; 
	to_filter_data->whitelisted = whitelisted; 
}

#ifdef USE_PTHREAD
static void *bsfilter_filtering_thread(void *data) 
{
	while (!filter_th_done) {
		pthread_mutex_lock(&list_mutex);
		if (to_filter_data == NULL || to_filter_data->done == TRUE) {
			pthread_mutex_unlock(&list_mutex);
			debug_print("thread is waiting for something to filter\n");
			pthread_mutex_lock(&wait_mutex);
			pthread_cond_wait(&wait_cond, &wait_mutex);
			pthread_mutex_unlock(&wait_mutex);
		} else {
			debug_print("thread awaken with something to filter\n");
			to_filter_data->done = FALSE;
			bsfilter_do_filter(to_filter_data);
			pthread_mutex_unlock(&list_mutex);
			to_filter_data->done = TRUE;
			g_usleep(100);
		}
	}
	return NULL;
}

static pthread_t filter_th;
static int filter_th_started = 0;

static void bsfilter_start_thread(void)
{
	filter_th_done = FALSE;
	if (filter_th_started != 0)
		return;
	if (pthread_create(&filter_th, NULL, 
			bsfilter_filtering_thread, 
			NULL) != 0) {
		filter_th_started = 0;
		return;
	}
	debug_print("thread created\n");
	filter_th_started = 1;
}

static void bsfilter_stop_thread(void)
{
	void *res;
	while (pthread_mutex_trylock(&list_mutex) != 0) {
		GTK_EVENTS_FLUSH();
		g_usleep(100);
	}
	if (filter_th_started != 0) {
		filter_th_done = TRUE;
		debug_print("waking thread up\n");
		pthread_mutex_lock(&wait_mutex);
		pthread_cond_broadcast(&wait_cond);
		pthread_mutex_unlock(&wait_mutex);
		pthread_join(filter_th, &res);
		filter_th_started = 0;
	}
	pthread_mutex_unlock(&list_mutex);
	debug_print("thread done\n");
}
#endif

static gboolean mail_filtering_hook(gpointer source, gpointer data)
{
	MailFilteringData *mail_filtering_data = (MailFilteringData *) source;
	MsgInfo *msginfo = mail_filtering_data->msginfo;
	static gboolean warned_error = FALSE;
	int status = 0, whitelisted = 0;
#ifndef G_OS_WIN32
	gchar *bs_exec = (config.bspath && *config.bspath) ? config.bspath:"bsfilter";
#else
	gchar *bs_exec = (config.bspath && *config.bspath) ? config.bspath:"bsfilterw.exe";
#endif
	gboolean filtered = FALSE;
	
	if (!config.process_emails) {
		return filtered;
	}
	
	if (msginfo == NULL) {
		g_warning("wrong call to bsfilter mail_filtering_hook");
		return filtered;
	}
		
	/* we have to make sure the mails are cached - or it'll break on IMAP */
	if (message_callback != NULL)
		message_callback(_("Bsfilter: fetching body..."), 0, 0, FALSE);
	if (msginfo) {
		gchar *file = procmsg_get_message_file(msginfo);
		g_free(file);
	}
	if (message_callback != NULL)
		message_callback(NULL, 0, 0, FALSE);

	if (message_callback != NULL)
		message_callback(_("Bsfilter: filtering message..."), 0, 0, FALSE);

#ifdef USE_PTHREAD
	while (pthread_mutex_trylock(&list_mutex) != 0) {
		GTK_EVENTS_FLUSH();
		g_usleep(100);
	}
#endif

	to_filter_data = g_new0(BsFilterData, 1);
	to_filter_data->msginfo = msginfo;
	to_filter_data->mail_filtering_data = mail_filtering_data;
	to_filter_data->done = FALSE;
	to_filter_data->status = -1;
	to_filter_data->whitelisted = 0;
#ifdef USE_PTHREAD
	to_filter_data->in_thread = (filter_th_started != 0);
#else
	to_filter_data->in_thread = FALSE;
#endif

#ifdef USE_PTHREAD
	pthread_mutex_unlock(&list_mutex);
	
	if (filter_th_started != 0) {
		debug_print("waking thread to let it filter things\n");
		pthread_mutex_lock(&wait_mutex);
		pthread_cond_broadcast(&wait_cond);
		pthread_mutex_unlock(&wait_mutex);

		while (!to_filter_data->done) {
			GTK_EVENTS_FLUSH();
			g_usleep(100);
		}
	}

	while (pthread_mutex_trylock(&list_mutex) != 0) {
		GTK_EVENTS_FLUSH();
		g_usleep(100);

	}
	if (filter_th_started == 0)
		bsfilter_do_filter(to_filter_data);
#else
	bsfilter_do_filter(to_filter_data);	
#endif

	status = to_filter_data->status;
	whitelisted = to_filter_data->whitelisted;

	g_free(to_filter_data);
	to_filter_data = NULL;
#ifdef USE_PTHREAD
	pthread_mutex_unlock(&list_mutex);
#endif

	if (status == 1) {
		procmsg_msginfo_unset_flags(msginfo, MSG_SPAM, 0);
		debug_print("unflagging ham: %d\n", msginfo->msgnum);
		filtered = FALSE;
	} else {
		if (!whitelisted || (whitelisted && !config.learn_from_whitelist)) {
			procmsg_msginfo_set_flags(msginfo, MSG_SPAM, 0);
			session_stats.spam++;
			debug_print("flagging spam: %d\n", msginfo->msgnum);
			filtered = TRUE;
		}
		if (whitelisted && config.learn_from_whitelist) {
			bsfilter_learn(msginfo, NULL, FALSE);
			procmsg_msginfo_unset_flags(msginfo, MSG_SPAM, 0);
			debug_print("unflagging ham: %d\n", msginfo->msgnum);
			filtered = FALSE;
		}
		if (MSG_IS_SPAM(msginfo->flags) && config.receive_spam) {
			if (config.receive_spam && config.mark_as_read)
				procmsg_msginfo_unset_flags(msginfo, (MSG_NEW|MSG_UNREAD), 0);
			if (!config.receive_spam)
				folder_item_remove_msg(msginfo->folder, msginfo->msgnum);
			filtered = TRUE;
		}
	}
		
	if (status < 0 || status > 2) { /* I/O or other errors */
		gchar *msg = NULL;
		
		if (status == 3)
			msg =  g_strdup_printf(_("The Bsfilter plugin couldn't filter "
					   "a message. The probable cause of the "
					   "error is that it didn't learn from any mail.\n"
					   "Use \"/Mark/Mark as spam\" and \"/Mark/Mark as "
					   "ham\" to train Bsfilter with a few hundred "
					   "spam and ham messages."));
		else
			msg =  g_strdup_printf(_("The Bsfilter plugin couldn't filter "
					   "a message. The command `%s` couldn't be run."), 
					   bs_exec);
		if (!prefs_common_get_prefs()->no_recv_err_panel) {
			if (!warned_error) {
				alertpanel_error("%s", msg);
			}
			warned_error = TRUE;
		} else {
			log_error(LOG_PROTOCOL, "%s\n", msg);
		}
		g_free(msg);
	}

	if (status == 0) {
		if (config.receive_spam && MSG_IS_SPAM(msginfo->flags)) {
			FolderItem *save_folder = NULL;

			if ((!config.save_folder) ||
			    (config.save_folder[0] == '\0') ||
			    ((save_folder = folder_find_item_from_identifier(config.save_folder)) == NULL)) {
			 	if (mail_filtering_data->account && mail_filtering_data->account->set_trash_folder) {
					save_folder = folder_find_item_from_identifier(
						mail_filtering_data->account->trash_folder);
					if (save_folder)
						debug_print("found trash folder from account's advanced settings\n");
				}
				if (save_folder == NULL && mail_filtering_data->account &&
				    mail_filtering_data->account->folder) {
				    	save_folder = mail_filtering_data->account->folder->trash;
					if (save_folder)
						debug_print("found trash folder from account's trash\n");
				}
				if (save_folder == NULL && mail_filtering_data->account &&
				    !mail_filtering_data->account->folder)  {
					if (mail_filtering_data->account->inbox) {
						FolderItem *item = folder_find_item_from_identifier(
							mail_filtering_data->account->inbox);
						if (item && item->folder->trash) {
							save_folder = item->folder->trash;
							debug_print("found trash folder from account's inbox\n");
						}
					} 
					if (!save_folder && mail_filtering_data->account->local_inbox) {
						FolderItem *item = folder_find_item_from_identifier(
							mail_filtering_data->account->local_inbox);
						if (item && item->folder->trash) {
							save_folder = item->folder->trash;
							debug_print("found trash folder from account's local_inbox\n");
						}
					}
				}
				if (save_folder == NULL) {
					debug_print("using default trash folder\n");
					save_folder = folder_get_default_trash();
				}
			}
			if (save_folder) {
				msginfo->filter_op = IS_MOVE;
				msginfo->to_filter_folder = save_folder;
			}
		}
	} 
	if (message_callback != NULL)
		message_callback(NULL, 0, 0, FALSE);
	
	return filtered;
}

BsfilterConfig *bsfilter_get_config(void)
{
	return &config;
}

int bsfilter_learn(MsgInfo *msginfo, GSList *msglist, gboolean spam)
{
	gchar *cmd = NULL;
	gchar *file = NULL;
#ifndef G_OS_WIN32
	gchar *bs_exec = (config.bspath && *config.bspath) ? config.bspath:"bsfilter";
#else
	gchar *bs_exec = (config.bspath && *config.bspath) ? config.bspath:"bsfilterw.exe";
#endif
	gint status = 0;
	gboolean free_list = FALSE;
	GSList *cur = NULL;

	if (msginfo == NULL && msglist == NULL) {
		return -1;
	}
	/* process *either* a msginfo or a msglist */
	if (msginfo != NULL && msglist == NULL) {
		msglist = g_slist_append(NULL, msginfo);
		free_list = TRUE;
	}
	for (cur = msglist; cur; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;
		file = procmsg_get_message_file(msginfo);
		if (file == NULL) {
			return -1;
		} else {
			if (message_callback != NULL)
				message_callback(_("Bsfilter: learning from message..."), 0, 0, FALSE);
			if (spam)
				/* learn as spam */
				cmd = g_strdup_printf("%s --homedir '%s' -su '%s'", bs_exec, get_rc_dir(), file);
			else 
				/* learn as ham */
				cmd = g_strdup_printf("%s --homedir '%s' -cu '%s'", bs_exec, get_rc_dir(), file);
				
			debug_print("%s\n", cmd);
			if ((status = execute_command_line(cmd, FALSE,
							claws_get_startup_dir())) != 0)
				log_error(LOG_PROTOCOL, _("Learning failed; `%s` returned with status %d."),
						cmd, status);
			g_free(cmd);
			g_free(file);
			if (message_callback != NULL)
				message_callback(NULL, 0, 0, FALSE);
		}
	}
	if (free_list)
		g_slist_free(msglist);

	return 0;
}

void bsfilter_save_config(void)
{
	PrefFile *pfile;
	gchar *rcpath;

	debug_print("Saving Bsfilter Page\n");

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMON_RC, NULL);
	pfile = prefs_write_open(rcpath);
	g_free(rcpath);
	if (!pfile || (prefs_set_block_label(pfile, "Bsfilter") < 0))
		return;

	if (prefs_write_param(param, pfile->fp) < 0) {
		g_warning("failed to write Bsfilter configuration to file");
		prefs_file_close_revert(pfile);
		return;
	}
        if (fprintf(pfile->fp, "\n") < 0) {
		FILE_OP_ERROR(rcpath, "fprintf");
		prefs_file_close_revert(pfile);
	} else
	        prefs_file_close(pfile);
}

void bsfilter_set_message_callback(MessageCallback callback)
{
	message_callback = callback;
}

gint plugin_init(gchar **error)
{
	gchar *rcpath;
	hook_id = HOOK_NONE;

	if (!check_plugin_version(MAKE_NUMERIC_VERSION(2,9,2,72),
				VERSION_NUMERIC, PLUGIN_NAME, error))
		return -1;

	prefs_set_default(param);
	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMON_RC, NULL);
	prefs_read_config(param, "Bsfilter", rcpath, NULL);
	g_free(rcpath);

	bsfilter_gtk_init();
		
	debug_print("Bsfilter plugin loaded\n");

#ifdef USE_PTHREAD
	bsfilter_start_thread();
#endif

	if (config.process_emails) {
		bsfilter_register_hook();
	}

	procmsg_register_spam_learner(bsfilter_learn);
	procmsg_spam_set_folder(config.save_folder, bsfilter_get_spam_folder);

	return 0;
	
}

FolderItem *bsfilter_get_spam_folder(MsgInfo *msginfo)
{
	FolderItem *item = NULL;
	
	if (config.save_folder != NULL) {
		item = folder_find_item_from_identifier(config.save_folder);
	}

	if (item || msginfo == NULL || msginfo->folder == NULL)
		return item;

	if (msginfo->folder->folder &&
	    msginfo->folder->folder->account && 
	    msginfo->folder->folder->account->set_trash_folder) {
		item = folder_find_item_from_identifier(
			msginfo->folder->folder->account->trash_folder);
	}

	if (item == NULL && 
	    msginfo->folder->folder &&
	    msginfo->folder->folder->trash)
		item = msginfo->folder->folder->trash;
		
	if (item == NULL)
		item = folder_get_default_trash();
		
	debug_print("bs spam dir: %s\n", folder_item_get_path(item));
	return item;
}

gboolean plugin_done(void)
{
	if (hook_id != HOOK_NONE) {
		bsfilter_unregister_hook();
	}
#ifdef USE_PTHREAD
	bsfilter_stop_thread();
#endif
	g_free(config.save_folder);
	bsfilter_gtk_done();
	procmsg_unregister_spam_learner(bsfilter_learn);
	procmsg_spam_set_folder(NULL, NULL);
	debug_print("Bsfilter plugin unloaded\n");
	return TRUE;
}

const gchar *plugin_name(void)
{
	return PLUGIN_NAME;
}

const gchar *plugin_desc(void)
{
	return _("This plugin can check all messages that are received from an "
	         "IMAP, LOCAL or POP account for spam using Bsfilter. "
		 "You will need Bsfilter installed locally.\n"
	         "\n"
		 "Before Bsfilter can recognize spam messages, you have to "
		 "train it by marking a few hundred spam and ham messages "
		 "with the use of \"/Mark/Mark as spam\" and \"/Mark/Mark as "
		 "ham\".\n"
	         "\n"
	         "When a message is identified as spam it can be deleted or "
	         "saved in a specially designated folder.\n"
	         "\n"
		 "Options can be found in /Configuration/Preferences/Plugins/Bsfilter");
}

const gchar *plugin_type(void)
{
	return "GTK3";
}

const gchar *plugin_licence(void)
{
	return "GPL3+";
}

const gchar *plugin_version(void)
{
	return VERSION;
}

struct PluginFeature *plugin_provides(void)
{
	static struct PluginFeature features[] = 
		{ {PLUGIN_FILTERING, N_("Spam detection")},
		  {PLUGIN_FILTERING, N_("Spam learning")},
		  {PLUGIN_NOTHING, NULL}};
	return features;
}

void bsfilter_register_hook(void)
{
	if (hook_id == HOOK_NONE)
		hook_id = hooks_register_hook(MAIL_FILTERING_HOOKLIST, mail_filtering_hook, NULL);
	if (hook_id == HOOK_NONE) {
		g_warning("failed to register mail filtering hook");
		config.process_emails = FALSE;
	}
}

void bsfilter_unregister_hook(void)
{
	if (hook_id != HOOK_NONE) {
		hooks_unregister_hook(MAIL_FILTERING_HOOKLIST, hook_id);
	}
	hook_id = HOOK_NONE;
}
