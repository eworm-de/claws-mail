/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2006 Hiroyuki Yamamoto and the Sylpheed-Claws Team
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

#include <glib.h>
#include <glib/gi18n.h>

#if HAVE_LOCALE_H
#  include <locale.h>
#endif

#include "common/sylpheed.h"
#include "common/version.h"
#include "plugin.h"
#include "common/utils.h"
#include "hooks.h"
#include "procmsg.h"
#include "folder.h"
#include "prefs.h"
#include "prefs_gtk.h"

#include "libspamc.h"
#include "spamassassin.h"
#include "inc.h"
#include "log.h"
#include "prefs_common.h"
#include "alertpanel.h"

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

enum {
    CHILD_RUNNING = 1 << 0,
    TIMEOUT_RUNNING = 1 << 1,
};

static guint hook_id = -1;
static int flags = SPAMC_RAW_MODE | SPAMC_SAFE_FALLBACK | SPAMC_CHECK_ONLY;
static MessageCallback message_callback;

static SpamAssassinConfig config;

static PrefParam param[] = {
	{"enable", "FALSE", &config.enable, P_BOOL,
	NULL, NULL, NULL},
	{"transport", "0", &config.transport, P_INT,
	 NULL, NULL, NULL},
	{"hostname", "localhost", &config.hostname, P_STRING,
	 NULL, NULL, NULL},
	{"port", "783", &config.port, P_INT,
	 NULL, NULL, NULL},
	{"socket", "", &config.socket, P_STRING,
	 NULL, NULL, NULL},
	{"process_emails", "TRUE", &config.process_emails, P_BOOL,
	 NULL, NULL, NULL},
	{"receive_spam", "TRUE", &config.receive_spam, P_BOOL,
	 NULL, NULL, NULL},
	{"save_folder", NULL, &config.save_folder, P_STRING,
	 NULL, NULL, NULL},
	{"max_size", "250", &config.max_size, P_INT,
	 NULL, NULL, NULL},
	{"timeout", "30", &config.timeout, P_INT,
	 NULL, NULL, NULL},
	{"username", "", &config.username, P_STRING,
	 NULL, NULL, NULL},

	{NULL, NULL, NULL, P_OTHER, NULL, NULL, NULL}
};

gboolean timeout_func(gpointer data)
{
	gint *running = (gint *) data;

	if (*running & CHILD_RUNNING)
		return TRUE;

	*running &= ~TIMEOUT_RUNNING;
	return FALSE;
}

typedef enum {
	MSG_IS_HAM = 0,
	MSG_IS_SPAM = 1,
	MSG_FILTERING_ERROR = 2
} MsgStatus;

static MsgStatus msg_is_spam(FILE *fp)
{
	struct transport trans;
	struct message m;
	gboolean is_spam = FALSE;

	if (!config.enable)
		return MSG_IS_HAM;

	transport_init(&trans);
	switch (config.transport) {
	case SPAMASSASSIN_TRANSPORT_LOCALHOST:
		trans.type = TRANSPORT_LOCALHOST;
		trans.port = config.port;
		break;
	case SPAMASSASSIN_TRANSPORT_TCP:
		trans.type = TRANSPORT_TCP;
		trans.hostname = config.hostname;
		trans.port = config.port;
		break;
	case SPAMASSASSIN_TRANSPORT_UNIX:
		trans.type = TRANSPORT_UNIX;
		trans.socketpath = config.socket;
		break;
	default:
		return MSG_IS_HAM;
	}

	if (transport_setup(&trans, flags) != EX_OK) {
		log_error(_("SpamAssassin plugin couldn't connect to spamd.\n"));
		debug_print("failed to setup transport\n");
		return MSG_FILTERING_ERROR;
	}

	m.type = MESSAGE_NONE;
	m.max_len = config.max_size * 1024;
	m.timeout = config.timeout;

	if (message_read(fileno(fp), flags, &m) != EX_OK) {
		debug_print("failed to read message\n");
		message_cleanup(&m);
		return MSG_FILTERING_ERROR;
	}

	if (message_filter(&trans, config.username, flags, &m) != EX_OK) {
		log_error(_("SpamAssassin plugin filtering failed.\n"));
		debug_print("filtering the message failed\n");
		message_cleanup(&m);
		return MSG_FILTERING_ERROR;
	}

	if (m.is_spam == EX_ISSPAM)
		is_spam = TRUE;

	message_cleanup(&m);

	return is_spam ? MSG_IS_SPAM:MSG_IS_HAM;
}

static gboolean mail_filtering_hook(gpointer source, gpointer data)
{
	MailFilteringData *mail_filtering_data = (MailFilteringData *) source;
	MsgInfo *msginfo = mail_filtering_data->msginfo;
	gboolean is_spam = FALSE, error = FALSE;
	static gboolean warned_error = FALSE;
	FILE *fp = NULL;
	int pid = 0;
	int status;

	/* SPAMASSASSIN_DISABLED : keep test for compatibility purpose */
	if (!config.enable || config.transport == SPAMASSASSIN_DISABLED) {
		log_warning(_("SpamAssassin plugin is disabled by its preferences.\n"));
		return FALSE;
	}
	debug_print("Filtering message %d\n", msginfo->msgnum);
	if (message_callback != NULL)
		message_callback(_("SpamAssassin: filtering message..."));

	if ((fp = procmsg_open_message(msginfo)) == NULL) {
		debug_print("failed to open message file\n");
		return FALSE;
	}

	pid = fork();
	if (pid == 0) {
		_exit(msg_is_spam(fp));
	} else {
		gint running = 0;

		running |= CHILD_RUNNING;

		g_timeout_add(50, timeout_func, &running);
		running |= TIMEOUT_RUNNING;

		while(running & CHILD_RUNNING) {
			int ret;

			ret = waitpid(pid, &status, WNOHANG);
			if (ret == pid) {
				if (WIFEXITED(status)) {
					MsgStatus result = MSG_IS_HAM;
					running &= ~CHILD_RUNNING;
					result = WEXITSTATUS(status);
    					is_spam = (result == MSG_IS_SPAM) ? TRUE : FALSE;
					error = (result == MSG_FILTERING_ERROR);
				}
			} if (ret < 0) {
				running &= ~CHILD_RUNNING;
			} /* ret == 0 continue */
	    
			g_main_iteration(TRUE);
    		}

		while (running & TIMEOUT_RUNNING)
			g_main_iteration(TRUE);
	}

	fclose(fp);

	if (is_spam) {
		debug_print("message is spam\n");
		procmsg_msginfo_set_flags(msginfo, MSG_SPAM, 0);
		if (config.receive_spam) {
			FolderItem *save_folder;

			if ((!config.save_folder) ||
			    (config.save_folder[0] == '\0') ||
			    ((save_folder = folder_find_item_from_identifier(config.save_folder)) == NULL))
				save_folder = folder_get_default_trash();

			procmsg_msginfo_unset_flags(msginfo, ~0, 0);
			procmsg_msginfo_set_flags(msginfo, MSG_SPAM, 0);
			folder_item_move_msg(save_folder, msginfo);
		} else {
			folder_item_remove_msg(msginfo->folder, msginfo->msgnum);
		}

		return TRUE;
	} else {
		debug_print("message is ham\n");
		procmsg_msginfo_unset_flags(msginfo, MSG_SPAM, 0);
	}
	
	if (error) {
		gchar *msg = _("The SpamAssassin plugin couldn't filter "
					   "a message. The probable cause of the error "
					   "is an unreachable spamd daemon. Please make "
					   "sure spamd is running and accessible.");
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
	}
	
	return FALSE;
}

SpamAssassinConfig *spamassassin_get_config(void)
{
	return &config;
}

gchar* spamassassin_create_tmp_spamc_wrapper(gboolean spam)
{
	gchar *contents;
	gchar *fname = get_tmp_file();

	if (fname != NULL) {
		contents = g_strdup_printf(
						"spamc -d %s -p %u -u %s -t %u -s %u -L %s<\"$*\";exit $?",
						config.hostname, config.port, 
						config.username, config.timeout,
						config.max_size * 1024, spam?"spam":"ham");
		if (str_write_to_file(contents, fname) < 0) {
			g_free(fname);
			fname = NULL;
		}
		g_free(contents);
	}
	/* returned pointer must be free'ed by caller */
	return fname;
}

int spamassassin_learn(MsgInfo *msginfo, GSList *msglist, gboolean spam)
{
	gchar *cmd = NULL;
	gchar *file = NULL;
	const gchar *shell = g_getenv("SHELL");
	gchar *spamc_wrapper = NULL;

	if (msginfo == NULL && msglist == NULL) {
		return -1;
	}

	if (config.transport == SPAMASSASSIN_TRANSPORT_TCP
	&&  prefs_common.work_offline
	&&  !inc_offline_should_override(
		_("Sylpheed-Claws needs network access in order "
		  "to feed this mail(s) to the remote learner."))) {
		return -1;
	}

	if (msginfo) {
		file = procmsg_get_message_file(msginfo);
		if (file == NULL) {
			return -1;
		}
		if (config.transport == SPAMASSASSIN_TRANSPORT_TCP) {
			spamc_wrapper = spamassassin_create_tmp_spamc_wrapper(spam);
			if (spamc_wrapper != NULL) {
				cmd = g_strconcat(shell?shell:"sh", " ",
								spamc_wrapper, " ", file, NULL);
			}
		} else {
			cmd = g_strdup_printf("sa-learn -u %s %s %s %s",
							config.username,
							prefs_common.work_offline?"-L":"",
							spam?"--spam":"--ham", file);
		}
	}
	if (msglist) {
		GSList *cur = msglist;
		MsgInfo *info;

		if (config.transport == SPAMASSASSIN_TRANSPORT_TCP) {
			/* execute n-times the spamc command */
			for (; cur; cur = cur->next) {
				info = (MsgInfo *)cur->data;
				gchar *tmpcmd = NULL;
				gchar *tmpfile = get_tmp_file();

				if (spamc_wrapper == NULL) {
					spamc_wrapper = spamassassin_create_tmp_spamc_wrapper(spam);
				}

				if (spamc_wrapper && tmpfile &&
			    	copy_file(procmsg_get_message_file(info), tmpfile, TRUE) == 0) {
					tmpcmd = g_strconcat(shell?shell:"sh", " ", spamc_wrapper, " ",
										tmpfile, NULL);
					debug_print("%s\n", tmpcmd);
					execute_command_line(tmpcmd, FALSE);
					g_free(tmpcmd);
				}
				g_free(tmpfile);
			}
			g_free(spamc_wrapper);
			return 0;
		} else {
			cmd = g_strdup_printf("sa-learn -u %s %s %s",
					config.username,
					prefs_common.work_offline?"-L":"",
					spam?"--spam":"--ham");

			/* concatenate all message tmpfiles to the sa-learn command-line */
			for (; cur; cur = cur->next) {
				info = (MsgInfo *)cur->data;
				gchar *tmpcmd = NULL;
				gchar *tmpfile = get_tmp_file();

				if (tmpfile &&
			    	copy_file(procmsg_get_message_file(info), tmpfile, TRUE) == 0) {			
					tmpcmd = g_strconcat(cmd, " ", tmpfile, NULL);
					g_free(cmd);
					cmd = tmpcmd;
				}
				g_free(tmpfile);
			}
		}
	}
	if (cmd == NULL) {
		return -1;
	}
	debug_print("%s\n", cmd);
	/* only run sync calls to sa-learn/spamc to prevent system lockdown */
	execute_command_line(cmd, FALSE);
	g_free(cmd);
	g_free(spamc_wrapper);

	return 0;
}

void spamassassin_save_config(void)
{
	PrefFile *pfile;
	gchar *rcpath;

	debug_print("Saving SpamAssassin Page\n");

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMON_RC, NULL);
	pfile = prefs_write_open(rcpath);
	g_free(rcpath);
	if (!pfile || (prefs_set_block_label(pfile, "SpamAssassin") < 0))
		return;

	if (prefs_write_param(param, pfile->fp) < 0) {
		g_warning("Failed to write SpamAssassin configuration to file\n");
		prefs_file_close_revert(pfile);
		return;
	}
	fprintf(pfile->fp, "\n");

	prefs_file_close(pfile);
}

gboolean spamassassin_check_username(void)
{
	if (config.username == NULL || config.username[0] == '\0') {
		config.username = (gchar*)g_get_user_name();
		if (config.username == NULL) {
			if (hook_id != -1) {
				spamassassin_unregister_hook();
			}
			procmsg_unregister_spam_learner(spamassassin_learn);
			procmsg_spam_set_folder(NULL);
			return FALSE;
		}
	}
	return TRUE;
}

void spamassassin_set_message_callback(MessageCallback callback)
{
	message_callback = callback;
}

gint plugin_init(gchar **error)
{
	gchar *rcpath;

	hook_id = -1;

	if ((sylpheed_get_version() > VERSION_NUMERIC)) {
		*error = g_strdup("Your version of Sylpheed-Claws is newer than the version the SpamAssassin plugin was built with");
		return -1;
	}

	if ((sylpheed_get_version() < MAKE_NUMERIC_VERSION(0, 9, 3, 86))) {
		*error = g_strdup("Your version of Sylpheed-Claws is too old for the SpamAssassin plugin");
		return -1;
	}

	prefs_set_default(param);
	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMON_RC, NULL);
	prefs_read_config(param, "SpamAssassin", rcpath, NULL);
	g_free(rcpath);
	if (!spamassassin_check_username()) {
		*error = g_strdup("Failed to get username");
		return -1;
	}
	spamassassin_gtk_init();
		
	debug_print("SpamAssassin plugin loaded\n");

	if (config.process_emails) {
		spamassassin_register_hook();
	}

	if (!config.enable || config.transport == SPAMASSASSIN_DISABLED) {
		log_warning(_("SpamAssassin plugin is loaded but disabled by its preferences.\n"));
	}
	else {
		if (config.transport == SPAMASSASSIN_TRANSPORT_TCP)
			debug_print("Enabling learner with a remote spamassassin server requires spamc/spamd 3.1.x\n");
		procmsg_register_spam_learner(spamassassin_learn);
		procmsg_spam_set_folder(config.save_folder);
	}

	return 0;
	
}

void plugin_done(void)
{
	if (hook_id != -1) {
		spamassassin_unregister_hook();
	}
	g_free(config.hostname);
	g_free(config.save_folder);
	spamassassin_gtk_done();
	procmsg_unregister_spam_learner(spamassassin_learn);
	procmsg_spam_set_folder(NULL);
	debug_print("SpamAssassin plugin unloaded\n");
}

const gchar *plugin_name(void)
{
	return _("SpamAssassin");
}

const gchar *plugin_desc(void)
{
	return _("This plugin can check all messages that are received from an "
	         "IMAP, LOCAL or POP account for spam using a SpamAssassin "
		 "server. You will need a SpamAssassin Server (spamd) running "
		 "somewhere.\n"
	         "\n"
		 "It can also be used for marking messages as Ham or Spam.\n"
	         "\n"
	         "When a message is identified as spam it can be deleted or "
	         "saved in a specially designated folder.\n"
	         "\n"
		 "Options can be found in /Configuration/Preferences/Plugins/SpamAssassin");
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

void spamassassin_register_hook(void)
{
	hook_id = hooks_register_hook(MAIL_FILTERING_HOOKLIST, mail_filtering_hook, NULL);
	if (hook_id == -1) {
		g_warning("Failed to register mail filtering hook");
		config.process_emails = FALSE;
	}
}

void spamassassin_unregister_hook(void)
{
	if (hook_id != -1) {
		hooks_unregister_hook(MAIL_FILTERING_HOOKLIST, hook_id);
	}
}
