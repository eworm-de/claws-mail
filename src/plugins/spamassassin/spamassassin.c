/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2003 Hiroyuki Yamamoto and the Sylpheed-Claws Team
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

static guint hook_id;
static int flags = SPAMC_RAW_MODE | SPAMC_SAFE_FALLBACK | SPAMC_CHECK_ONLY;
static gchar *username = NULL;
static MessageCallback message_callback;

static SpamAssassinConfig config;

static PrefParam param[] = {
	{"transport", "0", &config.transport, P_INT,
	 NULL, NULL, NULL},
	{"hostname", "localhost", &config.hostname, P_STRING,
	 NULL, NULL, NULL},
	{"port", "783", &config.port, P_INT,
	 NULL, NULL, NULL},
	{"socket", "", &config.socket, P_STRING,
	 NULL, NULL, NULL},
	{"receive_spam", "TRUE", &config.receive_spam, P_BOOL,
	 NULL, NULL, NULL},
	{"save_folder", NULL, &config.save_folder, P_STRING,
	 NULL, NULL, NULL},
	{"max_size", "250", &config.max_size, P_INT,
	 NULL, NULL, NULL},
	{"timeout", "30", &config.timeout, P_INT,
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

static gboolean msg_is_spam(FILE *fp)
{
	struct transport trans;
	struct message m;
	gboolean is_spam = FALSE;

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
		return FALSE;
	}

	if (transport_setup(&trans, flags) != EX_OK) {
		debug_print("failed to setup transport\n");
		return FALSE;
	}

	m.type = MESSAGE_NONE;
	m.max_len = config.max_size * 1024;
	m.timeout = config.timeout;

	if (message_read(fileno(fp), flags, &m) != EX_OK) {
		debug_print("failed to read message\n");
		message_cleanup(&m);
		return FALSE;
	}

	if (message_filter(&trans, username, flags, &m) != EX_OK) {
		debug_print("filtering the message failed\n");
		message_cleanup(&m);
		return FALSE;
	}

	if (m.is_spam == EX_ISSPAM)
		is_spam = TRUE;

	message_cleanup(&m);

	return is_spam;
}

static gboolean mail_filtering_hook(gpointer source, gpointer data)
{
	MailFilteringData *mail_filtering_data = (MailFilteringData *) source;
	MsgInfo *msginfo = mail_filtering_data->msginfo;
	gboolean is_spam = FALSE;
	FILE *fp = NULL;
	int pid = 0;
	int status;

	if (config.transport == SPAMASSASSIN_DISABLED)
		return FALSE;

	debug_print("Filtering message %d\n", msginfo->msgnum);
	if (message_callback != NULL)
		message_callback(_("SpamAssassin: filtering message..."));

	if ((fp = procmsg_open_message(msginfo)) == NULL) {
		debug_print("failed to open message file\n");
		return FALSE;
	}

	pid = fork();
	if (pid == 0) {
		_exit(msg_is_spam(fp) ? 1 : 0);
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
					running &= ~CHILD_RUNNING;
    					is_spam = WEXITSTATUS(status) == 1 ? TRUE : FALSE;
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
			    
		if (config.receive_spam) {
			FolderItem *save_folder;

			if ((!config.save_folder) ||
			    (config.save_folder[0] == '\0') ||
			    ((save_folder = folder_find_item_from_identifier(config.save_folder)) == NULL))
				save_folder = folder_get_default_trash();

			procmsg_msginfo_unset_flags(msginfo, ~0, 0);
			folder_item_move_msg(save_folder, msginfo);
		} else {
			folder_item_remove_msg(msginfo->folder, msginfo->msgnum);
		}

		return TRUE;
	}
	
	return FALSE;
}

SpamAssassinConfig *spamassassin_get_config(void)
{
	return &config;
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
		g_warning("failed to write SpamAssassin configuration to file\n");
		prefs_file_close_revert(pfile);
		return;
	}
	fprintf(pfile->fp, "\n");

	prefs_file_close(pfile);
}

void spamassassin_set_message_callback(MessageCallback callback)
{
	message_callback = callback;
}

gint plugin_init(gchar **error)
{
	gchar *rcpath;

	if ((sylpheed_get_version() > VERSION_NUMERIC)) {
		*error = g_strdup("Your version of Sylpheed-Claws is newer than the version the SpamAssassin plugin was built with");
		return -1;
	}

	if ((sylpheed_get_version() < MAKE_NUMERIC_VERSION(0, 9, 3, 86))) {
		*error = g_strdup("Your version of Sylpheed-Claws is too old for the SpamAssassin plugin");
		return -1;
	}

	hook_id = hooks_register_hook(MAIL_FILTERING_HOOKLIST, mail_filtering_hook, NULL);
	if (hook_id == -1) {
		*error = g_strdup("Failed to register mail filtering hook");
		return -1;
	}

	username = (gchar*)g_get_user_name();
	if (username == NULL) {
		hooks_unregister_hook(MAIL_FILTERING_HOOKLIST, hook_id);
		*error = g_strdup("Failed to get username");
		return -1;
	}

	prefs_set_default(param);
	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMON_RC, NULL);
	prefs_read_config(param, "SpamAssassin", rcpath, NULL);
	g_free(rcpath);
	spamassassin_gtk_init();

	debug_print("Spamassassin plugin loaded\n");

	return 0;
	
}

void plugin_done(void)
{
	hooks_unregister_hook(MAIL_FILTERING_HOOKLIST, hook_id);
	g_free(config.hostname);
	g_free(config.save_folder);

	debug_print("Spamassassin plugin unloaded\n");
}

const gchar *plugin_name(void)
{
	return _("SpamAssassin");
}

const gchar *plugin_desc(void)
{
	return _("This plugin checks all messages that are received from an "
	         "IMAP, LOCAL or POP account for spam using a SpamAssassin "
		 "server. You will need a SpamAssassin Server (spamd) running "
		 "somewhere.\n"
	         "\n"
	         "When a message is identified as spam it can be deleted or "
	         "saved into a special folder.\n"
	         "\n");
}

const gchar *plugin_type(void)
{
	return "Common";
}
