/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2006 Hiroyuki Yamamoto and the Claws Mail Team
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

#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#include <glib.h>
#include <glib/gi18n.h>

#if HAVE_LOCALE_H
#  include <locale.h>
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

#include "bogofilter.h"
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

static guint hook_id = -1;
static MessageCallback message_callback;

static BogofilterConfig config;

static PrefParam param[] = {
	{"process_emails", "TRUE", &config.process_emails, P_BOOL,
	 NULL, NULL, NULL},
	{"receive_spam", "TRUE", &config.receive_spam, P_BOOL,
	 NULL, NULL, NULL},
	{"save_folder", NULL, &config.save_folder, P_STRING,
	 NULL, NULL, NULL},
	{"max_size", "250", &config.max_size, P_INT,
	 NULL, NULL, NULL},
	{"bogopath", "bogofilter", &config.bogopath, P_STRING,
	 NULL, NULL, NULL},
	{"insert_header", "FALSE", &config.insert_header, P_BOOL,
	 NULL, NULL, NULL},
	{"whitelist_ab", "TRUE", &config.whitelist_ab, P_BOOL,
	 NULL, NULL, NULL},

	{NULL, NULL, NULL, P_OTHER, NULL, NULL, NULL}
};

/*
 * Helper function for spawn_with_input() - write an entire
 * string to a fd.
 */
static gboolean
write_all (int         fd,
	   const char *buf,
	   gsize       to_write)
{
  while (to_write > 0)
    {
      gssize count = write (fd, buf, to_write);
      if (count < 0)
	{
	  if (errno != EINTR)
	    return FALSE;
	}
      else
	{
	  to_write -= count;
	  buf += count;
	}
    }

  return TRUE;
}

typedef struct _BogoFilterData {
	MailFilteringData *mail_filtering_data;
	gchar **bogo_args;
	GSList *msglist;
	GSList *new_hams;
	GSList *new_spams;
	GSList *spams_to_receive;
	gboolean done;
	int status;
	gboolean in_thread;
} BogoFilterData;

static BogoFilterData *to_filter_data = NULL;
#ifdef USE_PTHREAD
static gboolean filter_th_done = FALSE;
static pthread_mutex_t list_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t wait_mutex = PTHREAD_MUTEX_INITIALIZER; 
static pthread_cond_t wait_cond = PTHREAD_COND_INITIALIZER; 
#endif

static gboolean found_in_addressbook(const gchar *address)
{
	gchar *addr = NULL;
	gboolean found = FALSE;
	gint num_addr = 0;
	
	if (!address)
		return FALSE;
	
	addr = g_strdup(address);
	extract_address(addr);
	num_addr = complete_address(addr);
	if (num_addr > 1) {
		/* skip first item (this is the search string itself) */
		int i = 1;
		for (; i < num_addr && !found; i++) {
			gchar *caddr = get_complete_address(i);
			extract_address(caddr);
			if (strcasecmp(caddr, addr) == 0)
				found = TRUE;
			g_free(caddr);
		}
	}
	g_free(addr);
	return found;
}

static void bogofilter_do_filter(BogoFilterData *data)
{
	GPid bogo_pid;
	gint bogo_stdin, bogo_stdout;
	GError *error = NULL;
	gboolean bogo_forked;
	int status = 0;
	MsgInfo *msginfo;
	GSList *cur = NULL;
	int total = 0, curnum = 0;
	gchar *file = NULL;
	gchar buf[BUFSIZ];

	total = g_slist_length(data->msglist);

	bogo_forked = g_spawn_async_with_pipes(
			NULL, data->bogo_args,NULL, G_SPAWN_SEARCH_PATH|G_SPAWN_DO_NOT_REAP_CHILD,
			NULL, NULL, &bogo_pid, &bogo_stdin,
			&bogo_stdout, NULL, &error);
		
	if (bogo_forked == FALSE) {
		g_warning("%s\n", error ? error->message:"ERROR???");
		g_error_free(error);
		error = NULL;
		status = -1;
	} else {
	
		if (config.whitelist_ab)
			start_address_completion(NULL);

		for (cur = data->msglist; cur; cur = cur->next) {
			gboolean whitelisted = FALSE;
			msginfo = (MsgInfo *)cur->data;
			debug_print("Filtering message %d (%d/%d)\n", msginfo->msgnum, curnum, total);

			if (message_callback != NULL)
				message_callback(NULL, total, curnum++, data->in_thread);

			if (config.whitelist_ab && msginfo->from && 
			    found_in_addressbook(msginfo->from))
				whitelisted = TRUE;

			/* can set flags (SCANNED, ATTACHMENT) but that's ok 
			 * as GUI updates are hooked not direct */

			file = procmsg_get_message_file(msginfo);

			if (file) {
				gchar *tmp = g_strdup_printf("%s\n",file);
				write_all(bogo_stdin, tmp, strlen(tmp));
				g_free(tmp);
				memset(buf, 0, sizeof(buf));
				if (read(bogo_stdout, buf, sizeof(buf)-1) < 0) {
					g_warning("bogofilter short read\n");
					debug_print("message %d is ham\n", msginfo->msgnum);
					data->mail_filtering_data->unfiltered = g_slist_prepend(
						data->mail_filtering_data->unfiltered, msginfo);
					data->new_hams = g_slist_prepend(data->new_hams, msginfo);
				} else {
					gchar **parts = NULL;
					if (strchr(buf, '/')) {
						tmp = strrchr(buf, '/')+1;
					} else {
						tmp = buf;
					}
					parts = g_strsplit(tmp, " ", 0);
					debug_print("read %s\n", buf);
					if (parts && parts[0] && parts[1] && parts[2] && 
					    FOLDER_TYPE(msginfo->folder->folder) == F_MH &&
					    config.insert_header) {
						gchar *tmpfile = get_tmp_file();
						FILE *input = fopen(file, "r");
						FILE *output = fopen(tmpfile, "w");
						if (strstr(parts[2], "\n"))
							*(strstr(parts[2], "\n")) = '\0';
						if (input && !output) 
							fclose (input);
						else if (!input && output)
							fclose (output);
						else {
							gchar tmpbuf[BUFFSIZE];
							const gchar *bogosity = *parts[1] == 'S' ? "Spam":
										 (*parts[1] == 'H' ? "Ham":"Unsure");
							gchar *tmpstr = g_strdup_printf(
									"X-Claws-Bogosity: %s, spamicity=%s%s\n",
									bogosity, parts[2],
									whitelisted?" [whitelisted]":"");
							fwrite(tmpstr, 1, strlen(tmpstr), output);
							while (fgets(tmpbuf, sizeof(buf), input))
								fputs(tmpbuf, output);
							fclose(input);
							fclose(output);
							move_file(tmpfile, file, TRUE);
							g_free(tmpstr);
						}
						g_free(tmpfile);
					}
					if (!whitelisted && parts && parts[0] && parts[1] && *parts[1] == 'S') {
						debug_print("message %d is spam\n", msginfo->msgnum);
						if (config.receive_spam) {
							data->spams_to_receive = g_slist_prepend(data->spams_to_receive, msginfo);
						} 

						data->mail_filtering_data->filtered = g_slist_prepend(
							data->mail_filtering_data->filtered, msginfo);
						data->new_spams = g_slist_prepend(data->new_spams, msginfo);
					} else {
						debug_print("message %d is ham\n", msginfo->msgnum);
						data->mail_filtering_data->unfiltered = g_slist_prepend(
							data->mail_filtering_data->unfiltered, msginfo);
						data->new_hams = g_slist_prepend(data->new_hams, msginfo);
					}
					g_strfreev(parts);
				}
				g_free(file);
			} else {
				data->mail_filtering_data->unfiltered = g_slist_prepend(
					data->mail_filtering_data->unfiltered, msginfo);
				data->new_hams = g_slist_prepend(data->new_hams, msginfo);
			}
		}
		if (config.whitelist_ab)
			end_address_completion();
	}
	if (status != -1) {
		close(bogo_stdout);
		close(bogo_stdin);
		waitpid(bogo_pid, &status, 0);
		if (!WIFEXITED(status))
			status = -1;
		else
			status = WEXITSTATUS(status);
	}
	
	to_filter_data->status = status; 
}

#ifdef USE_PTHREAD
static void *bogofilter_filtering_thread(void *data) 
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
			bogofilter_do_filter(to_filter_data);
			pthread_mutex_unlock(&list_mutex);
			to_filter_data->done = TRUE;
			usleep(100);
		}
	}
	return NULL;
}

static pthread_t filter_th = 0;

static void bogofilter_start_thread(void)
{
	filter_th_done = FALSE;
	if (filter_th != 0 || 1)
		return;
	if (pthread_create(&filter_th, 0, 
			bogofilter_filtering_thread, 
			NULL) != 0) {
		filter_th = 0;
		return;
	}
	debug_print("thread created\n");
}

static void bogofilter_stop_thread(void)
{
	void *res;
	while (pthread_mutex_trylock(&list_mutex) != 0) {
		GTK_EVENTS_FLUSH();
		usleep(100);
	}
	if (filter_th != 0) {
		filter_th_done = TRUE;
		debug_print("waking thread up\n");
		pthread_mutex_lock(&wait_mutex);
		pthread_cond_broadcast(&wait_cond);
		pthread_mutex_unlock(&wait_mutex);
		pthread_join(filter_th, &res);
		filter_th = 0;
	}
	pthread_mutex_unlock(&list_mutex);
	debug_print("thread done\n");
}
#endif

static gboolean mail_filtering_hook(gpointer source, gpointer data)
{
	MailFilteringData *mail_filtering_data = (MailFilteringData *) source;
	MsgInfo *msginfo = mail_filtering_data->msginfo;
	GSList *msglist = mail_filtering_data->msglist;
	GSList *cur = NULL;
	static gboolean warned_error = FALSE;
	int status = 0;
	int total = 0, curnum = 0;
	GSList *spams_to_receive = NULL, *new_hams = NULL, *new_spams = NULL;
	gchar *bogo_exec = (config.bogopath && *config.bogopath) ? config.bogopath:"bogofilter";
	gchar *bogo_args[4];
	gboolean ok_to_thread = TRUE;

	bogo_args[0] = bogo_exec;
	bogo_args[1] = "-T";
	bogo_args[2] = "-b";
	bogo_args[3] = NULL;
	
	if (!config.process_emails) {
		return FALSE;
	}
	
	if (msglist == NULL && msginfo != NULL) {
		g_warning("wrong call to bogofilter mail_filtering_hook");
		return FALSE;
	}
	
	total = g_slist_length(msglist);
	
	/* we have to make sure the mails are cached - or it'll break on IMAP */
	if (message_callback != NULL)
		message_callback(_("Bogofilter: fetching bodies..."), total, 0, FALSE);
	for (cur = msglist; cur; cur = cur->next) {
		gchar *file = procmsg_get_message_file((MsgInfo *)cur->data);
		if (file == NULL)
			ok_to_thread = FALSE;
		if (message_callback != NULL)
			message_callback(NULL, total, curnum++, FALSE);
		g_free(file);
	}
	if (message_callback != NULL)
		message_callback(NULL, 0, 0, FALSE);

	if (message_callback != NULL)
		message_callback(_("Bogofilter: filtering messages..."), total, 0, FALSE);

#ifdef USE_PTHREAD
	while (pthread_mutex_trylock(&list_mutex) != 0) {
		GTK_EVENTS_FLUSH();
		usleep(100);
	}
#endif
	to_filter_data = g_new0(BogoFilterData, 1);
	to_filter_data->msglist = msglist;
	to_filter_data->mail_filtering_data = mail_filtering_data;
	to_filter_data->spams_to_receive = NULL;
	to_filter_data->new_hams = NULL;
	to_filter_data->new_spams = NULL;
	to_filter_data->done = FALSE;
	to_filter_data->status = -1;
	to_filter_data->bogo_args = bogo_args;
#ifdef USE_PTHREAD
	to_filter_data->in_thread = (filter_th != 0 && ok_to_thread);
#else
	to_filter_data->in_thread = FALSE;
#endif

#ifdef USE_PTHREAD
	pthread_mutex_unlock(&list_mutex);
	
	if (filter_th != 0 && ok_to_thread) {
		debug_print("waking thread to let it filter things\n");
		pthread_mutex_lock(&wait_mutex);
		pthread_cond_broadcast(&wait_cond);
		pthread_mutex_unlock(&wait_mutex);

		while (!to_filter_data->done) {
			GTK_EVENTS_FLUSH();
			usleep(100);
		}
	}

	while (pthread_mutex_trylock(&list_mutex) != 0) {
		GTK_EVENTS_FLUSH();
		usleep(100);

	}
	if (filter_th == 0 || !ok_to_thread)
		bogofilter_do_filter(to_filter_data);
#else
	bogofilter_do_filter(to_filter_data);	
#endif

	spams_to_receive = to_filter_data->spams_to_receive;
	new_hams = to_filter_data->new_hams;
	new_spams = to_filter_data->new_spams;
	status = to_filter_data->status;
	g_free(to_filter_data);
	to_filter_data = NULL;
#ifdef USE_PTHREAD
	pthread_mutex_unlock(&list_mutex);
#endif


	/* flag hams */
	for (cur = new_hams; cur; cur = cur->next) {
		MsgInfo *msginfo = (MsgInfo *)cur->data;
		procmsg_msginfo_unset_flags(msginfo, MSG_SPAM, 0);
	}
	g_slist_free(new_hams);
	/* flag spams */
	for (cur = new_spams; cur; cur = cur->next) {
		MsgInfo *msginfo = (MsgInfo *)cur->data;
		if (config.receive_spam) {
			procmsg_msginfo_change_flags(msginfo, MSG_SPAM, 0, ~0, 0);
		} else {
			folder_item_remove_msg(msginfo->folder, msginfo->msgnum);
		}
	}
	g_slist_free(new_spams);
	
	if (status < 0 || status > 2) { /* I/O or other errors */
		gchar *msg = NULL;
		
		if (status == 3)
			msg =  g_strdup_printf(_("The Bogofilter plugin couldn't filter "
					   "a message. The probable cause of the "
					   "error is that it didn't learn from any mail.\n"
					   "Use \"/Mark/Mark as spam\" and \"/Mark/Mark as "
					   "ham\" to train Bogofilter with a few hundred "
					   "spam and ham messages."));
		else
			msg =  g_strdup_printf(_("The Bogofilter plugin couldn't filter "
					   "a message. the command `%s %s %s` couldn't be run."), 
					   bogo_args[0], bogo_args[1], bogo_args[2]);
		if (!prefs_common.no_recv_err_panel) {
			if (!warned_error) {
				alertpanel_error(msg);
			}
			warned_error = TRUE;
		} else {
			gchar *tmp = g_strdup_printf("%s\n", msg);
			log_error(tmp);
			g_free(tmp);
		}
		g_free(msg);
	}
	if (status < 0 || status > 2) {
		g_slist_free(mail_filtering_data->filtered);
		g_slist_free(mail_filtering_data->unfiltered);
		g_slist_free(spams_to_receive);
		mail_filtering_data->filtered = NULL;
		mail_filtering_data->unfiltered = NULL;
	} else if (config.receive_spam && spams_to_receive) {
		FolderItem *save_folder;

		if ((!config.save_folder) ||
		    (config.save_folder[0] == '\0') ||
		    ((save_folder = folder_find_item_from_identifier(config.save_folder)) == NULL))
			save_folder = folder_get_default_trash();
		if (save_folder) {
			for (cur = spams_to_receive; cur; cur = cur->next) {
				msginfo = (MsgInfo *)cur->data;
				msginfo->is_move = TRUE;
				msginfo->to_filter_folder = save_folder;
			}
		}
	} 

	if (message_callback != NULL)
		message_callback(NULL, 0, 0, FALSE);
	mail_filtering_data->filtered = g_slist_reverse(
		mail_filtering_data->filtered);
	mail_filtering_data->unfiltered = g_slist_reverse(
		mail_filtering_data->unfiltered);
	
	return FALSE;
}

BogofilterConfig *bogofilter_get_config(void)
{
	return &config;
}

int bogofilter_learn(MsgInfo *msginfo, GSList *msglist, gboolean spam)
{
	gchar *cmd = NULL;
	gchar *file = NULL;
	const gchar *bogo_exec = (config.bogopath && *config.bogopath) ? config.bogopath:"bogofilter";
	gint status = 0;
	if (msginfo == NULL && msglist == NULL) {
		return -1;
	}

	if (msginfo) {
		file = procmsg_get_message_file(msginfo);
		if (file == NULL) {
			return -1;
		} else {
			if (message_callback != NULL)
				message_callback(_("Bogofilter: learning from message..."), 0, 0, FALSE);
			if (spam)
				/* learn as spam */
				cmd = g_strdup_printf("%s -s -I '%s'", bogo_exec, file);
			else if (MSG_IS_SPAM(msginfo->flags))
				/* correct bogofilter, this wasn't spam */
				cmd = g_strdup_printf("%s -Sn -I '%s'", bogo_exec, file);
			else 
				/* learn as ham */
				cmd = g_strdup_printf("%s -n -I '%s'", bogo_exec, file);
			if ((status = execute_command_line(cmd, FALSE)) != 0)
				log_error(_("Learning failed; `%s` returned with status %d."),
						cmd, status);
			g_free(cmd);
			g_free(file);
			if (message_callback != NULL)
				message_callback(NULL, 0, 0, FALSE);
			return 0;
		}
	}
	if (msglist) {
		GSList *cur = msglist;
		MsgInfo *info;
		int total = g_slist_length(msglist);
		int done = 0;
		gboolean some_correction = FALSE, some_no_correction = FALSE;
	
		if (message_callback != NULL)
			message_callback(_("Bogofilter: learning from messages..."), total, 0, FALSE);
		
		for (cur = msglist; cur && status == 0; cur = cur->next) {
			info = (MsgInfo *)cur->data;
			if (spam)
				some_no_correction = TRUE;
			else if (MSG_IS_SPAM(info->flags))
				/* correct bogofilter, this wasn't spam */
				some_correction = TRUE;
			else 
				some_no_correction = TRUE;
			
		}
		
		if (some_correction && some_no_correction) {
			/* we potentially have to do different stuff for every mail */
			for (cur = msglist; cur && status == 0; cur = cur->next) {
				info = (MsgInfo *)cur->data;
				file = procmsg_get_message_file(info);

				if (spam)
					/* learn as spam */
					cmd = g_strdup_printf("%s -s -I '%s'", bogo_exec, file);
				else if (MSG_IS_SPAM(info->flags))
					/* correct bogofilter, this wasn't spam */
					cmd = g_strdup_printf("%s -Sn -I '%s'", bogo_exec, file);
				else 
					/* learn as ham */
					cmd = g_strdup_printf("%s -n -I '%s'", bogo_exec, file);

				if ((status = execute_command_line(cmd, FALSE)) != 0)
					log_error(_("Learning failed; `%s` returned with status %d."),
							cmd, status);

				g_free(cmd);
				g_free(file);
				done++;
				if (message_callback != NULL)
					message_callback(NULL, total, done, FALSE);
			}
		} else if (some_correction || some_no_correction) {
			cur = msglist;
			
			gchar *bogo_args[4];
			GPid bogo_pid;
			gint bogo_stdin;
			GError *error = NULL;
			gboolean bogo_forked;

			bogo_args[0] = (gchar *)bogo_exec;
			if (some_correction && !some_no_correction)
				bogo_args[1] = "-Sn";
			else if (some_no_correction && !some_correction)
				bogo_args[1] = spam ? "-s":"-n";
			bogo_args[2] = "-b";
			bogo_args[3] = NULL;

			bogo_forked = g_spawn_async_with_pipes(
					NULL, bogo_args,NULL, G_SPAWN_SEARCH_PATH|G_SPAWN_DO_NOT_REAP_CHILD,
					NULL, NULL, &bogo_pid, &bogo_stdin,
					NULL, NULL, &error);

			while (bogo_forked && cur) {
				gchar *tmp = NULL;
				info = (MsgInfo *)cur->data;
				file = procmsg_get_message_file(info);
				if (file) {
					tmp = g_strdup_printf("%s\n", 
						file);
					write_all(bogo_stdin, tmp, strlen(tmp));
					g_free(tmp);
				}
				g_free(file);
				done++;
				if (message_callback != NULL)
					message_callback(NULL, total, done, FALSE);
				cur = cur->next;
			}
			if (bogo_forked) {
				close(bogo_stdin);
				waitpid(bogo_pid, &status, 0);
				if (!WIFEXITED(status))
					status = -1;
				else
					status = WEXITSTATUS(status);
			}
			if (!bogo_forked || status != 0) {
				log_error(_("Learning failed; `%s %s %s` returned with error:\n%s"),
						bogo_args[0], bogo_args[1], bogo_args[2], 
						error ? error->message:_("Unknown error"));
				if (error)
					g_error_free(error);
			}

		}

		if (message_callback != NULL)
			message_callback(NULL, 0, 0, FALSE);
		return 0;
	}
	return -1;
}

void bogofilter_save_config(void)
{
	PrefFile *pfile;
	gchar *rcpath;

	debug_print("Saving Bogofilter Page\n");

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMON_RC, NULL);
	pfile = prefs_write_open(rcpath);
	g_free(rcpath);
	if (!pfile || (prefs_set_block_label(pfile, "Bogofilter") < 0))
		return;

	if (prefs_write_param(param, pfile->fp) < 0) {
		g_warning("Failed to write Bogofilter configuration to file\n");
		prefs_file_close_revert(pfile);
		return;
	}
	fprintf(pfile->fp, "\n");

	prefs_file_close(pfile);
}

void bogofilter_set_message_callback(MessageCallback callback)
{
	message_callback = callback;
}

gint plugin_init(gchar **error)
{
	gchar *rcpath;

	hook_id = -1;

	if ((claws_get_version() > VERSION_NUMERIC)) {
		*error = g_strdup(_("Your version of Claws Mail is newer than the version the Bogofilter plugin was built with"));
		return -1;
	}

	if ((claws_get_version() < MAKE_NUMERIC_VERSION(0, 9, 3, 86))) {
		*error = g_strdup(_("Your version of Claws Mail is too old for the Bogofilter plugin"));
		return -1;
	}

	prefs_set_default(param);
	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMON_RC, NULL);
	prefs_read_config(param, "Bogofilter", rcpath, NULL);
	g_free(rcpath);

	bogofilter_gtk_init();
		
	debug_print("Bogofilter plugin loaded\n");

#ifdef USE_PTHREAD
	bogofilter_start_thread();
#endif

	if (config.process_emails) {
		bogofilter_register_hook();
	}

	procmsg_register_spam_learner(bogofilter_learn);
	procmsg_spam_set_folder(config.save_folder);

	return 0;
	
}

void plugin_done(void)
{
	if (hook_id != -1) {
		bogofilter_unregister_hook();
	}
#ifdef USE_PTHREAD
	bogofilter_stop_thread();
#endif
	g_free(config.save_folder);
	bogofilter_gtk_done();
	procmsg_unregister_spam_learner(bogofilter_learn);
	procmsg_spam_set_folder(NULL);
	debug_print("Bogofilter plugin unloaded\n");
}

const gchar *plugin_name(void)
{
	return _("Bogofilter");
}

const gchar *plugin_desc(void)
{
	return _("This plugin can check all messages that are received from an "
	         "IMAP, LOCAL or POP account for spam using Bogofilter. "
		 "You will need Bogofilter installed locally.\n "
	         "\n"
		 "Before Bogofilter can recognize spam messages, you have to "
		 "train it by marking a few hundred spam and ham messages. "
		 "Use \"/Mark/Mark as spam\" and \"/Mark/Mark as ham\" to "
		 "train Bogofilter.\n"
	         "\n"
	         "When a message is identified as spam it can be deleted or "
	         "saved in a specially designated folder.\n"
	         "\n"
		 "Options can be found in /Configuration/Preferences/Plugins/Bogofilter");
}

const gchar *plugin_type(void)
{
	return "GTK2";
}

const gchar *plugin_licence(void)
{
	return "GPL";
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

void bogofilter_register_hook(void)
{
	if (hook_id == -1)
		hook_id = hooks_register_hook(MAIL_LISTFILTERING_HOOKLIST, mail_filtering_hook, NULL);
	if (hook_id == -1) {
		g_warning("Failed to register mail filtering hook");
		config.process_emails = FALSE;
	}
}

void bogofilter_unregister_hook(void)
{
	if (hook_id != -1) {
		hooks_unregister_hook(MAIL_LISTFILTERING_HOOKLIST, hook_id);
	}
	hook_id = -1;
}
