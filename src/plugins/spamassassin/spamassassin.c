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

#include "libspamc.h"

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
static int max_size;
static int flags = SPAMC_RAW_MODE | SPAMC_SAFE_FALLBACK | SPAMC_CHECK_ONLY;
static gchar *hostname = NULL;
static int port;

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
	
	debug_print("Filtering message %d\n", msginfo->msgnum);

	oldlocale = g_strdup(setlocale(LC_ALL, NULL));
	if (oldlocale == NULL)
		goto CATCH;

	setlocale(LC_ALL, "C");

	ret = lookup_host(hostname, port, &addr);
	if (ret != EX_OK)
		goto CATCH;

	m.type = MESSAGE_NONE;
	m.max_len = max_size;

	username = g_get_user_name();
	if (username == NULL)
		goto CATCH;

	fp = procmsg_open_message(msginfo);
	if (fp == NULL)
		goto CATCH;

	ret = message_read(fileno(fp), flags, &m);
	if (ret != EX_OK)
		goto CATCH;

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
		debug_print("Message is spam\n");

		folder_item_move_msg(folder_get_default_trash(), msginfo);
		return TRUE;
	}
	
	return FALSE;
}

static void spamassassin_read_config()
{
	max_size = 250*1024;
	hostname = "127.0.0.1";
	port = 783;
}

gint plugin_init(gchar **error)
{
	hook_id = hooks_register_hook(MAIL_FILTERING_HOOKLIST, mail_filtering_hook, NULL);
	if (hook_id == -1) {
		*error = g_strdup("Failed to register mail filtering hook");
		return -1;
	}

	spamassassin_read_config();

	debug_print("Spamassassin plugin loaded\n");

	return 0;
	
}

void plugin_done()
{
	hooks_unregister_hook(MAIL_FILTERING_HOOKLIST, hook_id);

	debug_print("Spamassassin plugin unloaded\n");
}

const gchar *plugin_name()
{
	return "Spamassassin Plugin";
}

const gchar *plugin_desc()
{
	return "Check incoming mails for spam with spamassassin";
}

