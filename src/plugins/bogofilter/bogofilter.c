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

#include "bogofilter.h"
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

	{NULL, NULL, NULL, P_OTHER, NULL, NULL, NULL}
};

static gboolean mail_filtering_hook(gpointer source, gpointer data)
{
	MailFilteringData *mail_filtering_data = (MailFilteringData *) source;
	MsgInfo *msginfo = mail_filtering_data->msginfo;
	gboolean is_spam = FALSE;
	static gboolean warned_error = FALSE;
	gchar *file = NULL, *cmd = NULL;
	int status = 3;

	if (!config.process_emails) {
		return FALSE;
	}
	debug_print("Filtering message %d\n", msginfo->msgnum);
	if (message_callback != NULL)
		message_callback(_("Bogofilter: filtering message..."), 0, 0);

	file = procmsg_get_message_file(msginfo);

	if (file)
		cmd = g_strdup_printf("bogofilter -I %s", file);
	
	if (cmd)
		status = system(cmd);
	
	if (status == -1)
		status = 3;
	else 
		status = WEXITSTATUS(status);

	g_free(cmd);
	g_free(file);
	printf("bogofilter status %d\n", status);
	is_spam = (status == 0);
	
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
	
	if (status == 3) { /* I/O or other errors */
		if (!warned_error) {
			alertpanel_error(_("The Bogofilter plugin couldn't filter "
					   "a message. The probable error cause is "
					   "that it didn't learn from any mail.\n"
					   "Use \"Mark/As Spam(Ham)\" from messages' "
					   "contextual menu to train Bogofilter with "
					   "a few hundreds spam and ham messages."));
		}
		warned_error = TRUE;
	}
	
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

	if (msginfo == NULL && msglist == NULL) {
		return -1;
	}

	if (msginfo) {
		file = procmsg_get_message_file(msginfo);
		if (file == NULL) {
			return -1;
		} else {
			if (message_callback != NULL)
				message_callback(_("Bogofilter: learning from message..."), 0, 0);
			cmd = g_strdup_printf("bogofilter -%c -I %s",
							spam ? 's':'n', file);
			execute_command_line(cmd, FALSE);
			g_free(cmd);
			g_free(file);
			if (message_callback != NULL)
				message_callback(NULL, 0, 0);
			return 0;
		}
	}
	if (msglist) {
		GSList *cur = msglist;
		MsgInfo *info;
		int total = g_slist_length(msglist);
		int done = 0;
		if (message_callback != NULL)
			message_callback(_("Bogofilter: learning from messages..."), total, 0);
		
		for (; cur; cur = cur->next) {
			info = (MsgInfo *)cur->data;
			file = procmsg_get_message_file(info);

			cmd = g_strdup_printf("bogofilter -%c -I %s",
							spam ? 's':'n', file);
			execute_command_line(cmd, FALSE);
			g_free(cmd);
			g_free(file);
			done++;
			if (message_callback != NULL)
				message_callback(NULL, total, done);
		}
		if (message_callback != NULL)
			message_callback(NULL, 0, 0);
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

	if ((sylpheed_get_version() > VERSION_NUMERIC)) {
		*error = g_strdup("Your version of Sylpheed-Claws is newer than the version the Bogofilter plugin was built with");
		return -1;
	}

	if ((sylpheed_get_version() < MAKE_NUMERIC_VERSION(0, 9, 3, 86))) {
		*error = g_strdup("Your version of Sylpheed-Claws is too old for the Bogofilter plugin");
		return -1;
	}

	prefs_set_default(param);
	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMON_RC, NULL);
	prefs_read_config(param, "Bogofilter", rcpath, NULL);
	g_free(rcpath);

	bogofilter_gtk_init();
		
	debug_print("Bogofilter plugin loaded\n");

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
	         "IMAP, LOCAL or POP account for spam using a Bogofilter "
		 "server. You will need Bogofilter installed locally.\n "
	         "\n"
		 "Before Bogofilter can recognize spam messages, you have to "
		 "train it by marking a few hundreds spam and ham messages. "
		 "Use \"Mark/As Spam(Ham)\" from messages' contextual menu "
		 "to train Bogofilter.\n"
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
	hook_id = hooks_register_hook(MAIL_FILTERING_HOOKLIST, mail_filtering_hook, NULL);
	if (hook_id == -1) {
		g_warning("Failed to register mail filtering hook");
		config.process_emails = FALSE;
	}
}

void bogofilter_unregister_hook(void)
{
	if (hook_id != -1) {
		hooks_unregister_hook(MAIL_FILTERING_HOOKLIST, hook_id);
	}
}
