/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2002 Hiroyuki Yamamoto and the Sylpheed-Claws Team
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

#if HAVE_LOCALE_H
#  include <locale.h>
#endif

#include "plugin.h"
#include "common/utils.h"
#include "hooks.h"
#include "inc.h"
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

static gint hook_id;
static int flags = SPAMC_RAW_MODE | SPAMC_SAFE_FALLBACK | SPAMC_CHECK_ONLY;

gboolean spamassassin_enable;
gchar *spamassassin_hostname;
int spamassassin_port;
int spamassassin_max_size;
gboolean spamassassin_receive_spam;
gchar *spamassassin_save_folder;

static PrefParam param[] = {
	{"enable", "FALSE", &spamassassin_enable, P_BOOL,
	 NULL, NULL, NULL},
	{"hostname", "localhost", &spamassassin_hostname, P_STRING,
	 NULL, NULL, NULL},
	{"port", "783", &spamassassin_port, P_USHORT,
	 NULL, NULL, NULL},
	{"max_size", "250", &spamassassin_max_size, P_USHORT,
	 NULL, NULL, NULL},
	{"receive_spam", "250", &spamassassin_receive_spam, P_BOOL,
	 NULL, NULL, NULL},
	{"save_folder", NULL, &spamassassin_save_folder, P_STRING,
	 NULL, NULL, NULL},

	{NULL, NULL, NULL, P_OTHER, NULL, NULL, NULL}
};

static gboolean mail_filtering_hook(gpointer source, gpointer data)
{
	MailFilteringData *mail_filtering_data = (MailFilteringData *) source;
	MsgInfo *msginfo = mail_filtering_data->msginfo;
	gboolean is_spam = FALSE;
	FILE *fp = NULL;
	struct message m;
	int ret;
	gchar *username = NULL, *oldlocale = NULL;
	struct sockaddr addr;

	if (!spamassassin_enable)
		return FALSE;

	debug_print("Filtering message %d\n", msginfo->msgnum);

	oldlocale = g_strdup(setlocale(LC_ALL, NULL));
	if (oldlocale == NULL) {
		debug_print("failed to get locale");
		goto CATCH;
	}

	setlocale(LC_ALL, "C");

	ret = lookup_host(spamassassin_hostname, spamassassin_port, &addr);
	if (ret != EX_OK) {
		debug_print("failed to look up spamd host");
		goto CATCH;
	}

	m.type = MESSAGE_NONE;
	m.max_len = spamassassin_max_size * 1024;

	username = g_get_user_name();
	if (username == NULL) {
		debug_print("failed to get username");
		goto CATCH;
	}

	fp = procmsg_open_message(msginfo);
	if (fp == NULL) {
		debug_print("failed to open message file");
		goto CATCH;
	}

	ret = message_read(fileno(fp), flags, &m);
	if (ret != EX_OK) {
		debug_print("failed to read message");
		goto CATCH;
	}

	ret = message_filter(&addr, username, flags, &m);
	if ((ret == EX_OK) && (m.is_spam == EX_ISSPAM))
		is_spam = TRUE;

CATCH:
	if (fp != NULL)
		fclose(fp);
	message_cleanup(&m);
	if (oldlocale != NULL) {
		setlocale(LC_ALL, oldlocale);
		g_free(oldlocale);
	}

	if (is_spam) {
		if (spamassassin_receive_spam) {
			FolderItem *save_folder;

			debug_print("message is spam\n");
			    
			if ((!spamassassin_save_folder) ||
			    (spamassassin_save_folder[0] == '\0') ||
			    ((save_folder = folder_find_item_from_identifier(spamassassin_save_folder)) == NULL))
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

void spamassassin_save_config()
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

#ifdef WIN32
/* cfg to client*/
void spamassassin_getconf(_spamassassin_cfg * const spamassassin_cfg)
{
	TO_SPAMCFG_STRUCT(spamassassin_cfg);
}

/* cfg from client*/
void spamassassin_setconf(const _spamassassin_cfg * const spamassassin_cfg)
{
	FROM_SPAMCFG_STRUCT(spamassassin_cfg);
}
#endif

gint plugin_init(gchar **error)
{
	hook_id = hooks_register_hook(MAIL_FILTERING_HOOKLIST, mail_filtering_hook, NULL);
	if (hook_id == -1) {
		*error = g_strdup("Failed to register mail filtering hook");
		return -1;
	}

	prefs_set_default(param);
	prefs_read_config(param, "SpamAssassin", COMMON_RC);

	debug_print("Spamassassin plugin loaded\n");

	return 0;
	
}

void plugin_done()
{
	hooks_unregister_hook(MAIL_FILTERING_HOOKLIST, hook_id);
	g_free(spamassassin_hostname);

	debug_print("Spamassassin plugin unloaded\n");
}

const gchar *plugin_name()
{
	return "SpamAssassin";
}

const gchar *plugin_desc()
{
	return "This plugin checks all messages that are received from a POP "
	       "account for spam using a SpamAssassin server. You will need "
	       "a SpamAssassin Server (spamd) running somewhere.\n"
	       "\n"
	       "When a message is identified as spam it can be deleted or "
	       "saved into a special folder.\n"
	       "\n"
	       "This plugin only contains the actual function for filtering "
	       "and deleting or moving the message. You probably want to load "
	       "a User Interface plugin too. Otherwise you would have to "
	       "manually write the plugin configuration.\n";
}
