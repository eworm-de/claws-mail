/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2021 the Claws Mail Team
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#  include "claws-features.h"
#endif

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>

/* common */
#include "version.h"
#include "claws.h"
#include "plugin.h"
#include "utils.h"
#include "hooks.h"
#include "inc.h"
#include "prefs.h"
#include "prefs_gtk.h"
#include "fetchinfo_plugin.h"
/* add headers */
#include "pop.h"
#include "quoted-printable.h"
/* parse headers */
#include "procheader.h"

static gulong mail_receive_hook_id = HOOK_NONE;

static FetchinfoConfig config;

static PrefParam param[] = {
	{"fetchinfo_enable",  "FALSE", &config.fetchinfo_enable, P_BOOL, NULL, NULL, NULL},
	{"fetchinfo_uidl",    "TRUE", &config.fetchinfo_uidl, P_BOOL, NULL, NULL, NULL},
	{"fetchinfo_account", "TRUE", &config.fetchinfo_account, P_BOOL, NULL, NULL, NULL},
	{"fetchinfo_server",  "TRUE", &config.fetchinfo_server, P_BOOL,	NULL, NULL, NULL},
	{"fetchinfo_userid",  "TRUE", &config.fetchinfo_userid, P_BOOL,	NULL, NULL, NULL},
	{"fetchinfo_time",    "TRUE", &config.fetchinfo_time, P_BOOL, NULL, NULL, NULL},

	{NULL, NULL, NULL, P_OTHER, NULL, NULL, NULL}
};

gchar *fetchinfo_add_header(gchar **data, const gchar *header,const gchar *value)
{
	gchar *line;
	gchar *qpline;
	gchar *newdata;

	line = g_strdup_printf("%s: %s", header, value);
	qpline = g_malloc(strlen(line)*4);
	qp_encode_line(qpline, line);
	newdata = g_strconcat(*data, qpline, NULL);
	g_free(line);
	g_free(qpline);
	g_free(*data);
	*data = newdata;
	return newdata;
}

static gboolean mail_receive_hook(gpointer source, gpointer data)
{
	MailReceiveData *mail_receive_data = (MailReceiveData *) source;
	Pop3Session *session;
	gchar *newheaders;
	gchar *newdata;
	gchar date[RFC822_DATE_BUFFSIZE];
	
	if (!config.fetchinfo_enable) {
		return FALSE;
	}

	g_return_val_if_fail( 
			      mail_receive_data
			      && mail_receive_data->session
			      && mail_receive_data->data,
			      FALSE );

	session = mail_receive_data->session;
	get_rfc822_date(date, sizeof(date));
	newheaders = g_strdup("");

	if (config.fetchinfo_uidl)
		fetchinfo_add_header(&newheaders, "X-FETCH-UIDL", 
			session->msg[session->cur_msg].uidl);
	if (config.fetchinfo_account)
		fetchinfo_add_header(&newheaders, "X-FETCH-ACCOUNT", 
			session->ac_prefs->account_name);
	if (config.fetchinfo_server)
		fetchinfo_add_header(&newheaders, "X-FETCH-SERVER", 
			session->ac_prefs->recv_server);
	if (config.fetchinfo_userid)
		fetchinfo_add_header(&newheaders, "X-FETCH-USERID", 
			session->ac_prefs->userid);
	if (config.fetchinfo_time)
		fetchinfo_add_header(&newheaders, "X-FETCH-TIME", 
			date);

	newdata = g_strconcat(newheaders, mail_receive_data->data, NULL);
	g_free(newheaders);
	g_free(mail_receive_data->data);
	mail_receive_data->data = newdata;
	mail_receive_data->data_len = strlen(newdata);
	return FALSE;
}

FetchinfoConfig *fetchinfo_get_config(void)
{
	return &config;
}

void fetchinfo_save_config(void)
{
	PrefFile *pfile;
	gchar *rcpath;

	debug_print("Saving Fetchinfo Page\n");

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMON_RC, NULL);
	pfile = prefs_write_open(rcpath);
	g_free(rcpath);
	if (!pfile || (prefs_set_block_label(pfile, "Fetchinfo") < 0))
		return;

	if (prefs_write_param(param, pfile->fp) < 0) {
		/* i18n: Possible error message during plugin load */
		g_warning("failed to write Fetchinfo configuration to file");
		prefs_file_close_revert(pfile);
		return;
	}
        if (fprintf(pfile->fp, "\n") < 0) {
		FILE_OP_ERROR(rcpath, "fprintf");
		prefs_file_close_revert(pfile);
	} else
	        prefs_file_close(pfile);
}

gint plugin_init(gchar **error)
{
	gchar *rcpath;

	if (!check_plugin_version(MAKE_NUMERIC_VERSION(2,9,2,72),
				VERSION_NUMERIC, _("Fetchinfo"), error))
		return -1;

	mail_receive_hook_id = hooks_register_hook(MAIL_RECEIVE_HOOKLIST, mail_receive_hook, NULL);
	if (mail_receive_hook_id == HOOK_NONE) {
		/* TRANSLATORS: Possible error message during plugin load */
		*error = g_strdup(_("Failed to register mail receive hook"));
		return -1;
	}

	prefs_set_default(param);
	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMON_RC, NULL);
	prefs_read_config(param, "Fetchinfo", rcpath, NULL);
	g_free(rcpath);

	fetchinfo_gtk_init();

	debug_print("Fetchinfo plugin loaded\n");

	return 0;
}

gboolean plugin_done(void)
{
	hooks_unregister_hook(MAIL_RECEIVE_HOOKLIST, mail_receive_hook_id);
	fetchinfo_gtk_done();

	debug_print("Fetchinfo plugin unloaded\n");
	return TRUE;
}

const gchar *plugin_name(void)
{
	return _("Fetchinfo");
}

const gchar *plugin_desc(void)
{
	/* TRANSLATORS: Description seen in plugins dialog.
	 * Translation of "Plugins" part of preferences path should to be
	 * the same as translation of "Plugins" string in Claws Mail message
	 * catalog. */
	return _("This plugin modifies the downloaded messages. "
	         "It inserts headers containing some download "
		 "information: UIDL, Claws Mail account name, "
		 "POP server, user ID and retrieval time.\n"
	     "\n"
	     "Options can be found in /Configuration/Preferences/Plugins/Fetchinfo");
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
		/* TRANSLATORS: Description of functionality added by this plugin */
		{ {PLUGIN_UTILITY, N_("Mail marking")},
		  {PLUGIN_NOTHING, NULL}};
	return features;
}
