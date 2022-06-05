/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2005-2015 H.Merijn Brand and the Claws Mail Team
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

#include "config.h"

#include <errno.h>

#include <glib.h>
#include <glib/gi18n.h>

#include "version.h"
#include "claws.h"
#include "plugin.h"
#include "utils.h"
#include "hooks.h"
#include "procmsg.h"

#include <inttypes.h>

#include "file-utils.h"

#define LOG_NAME	"NewLog"
#define DEFAULT_DIR	"Mail"

static gulong hook_id = HOOK_NONE;

static FILE *NewLog   = NULL;
static char *LogName  = NULL;
static int   truncLog = 1;
static char *pluginDesc = NULL;

static gchar *defstr (gchar *s)
{
	return s ? s : "(null)";
} /* defstr */

gboolean newmail_hook (gpointer source, gpointer data)
{
	auto MsgInfo    *msginfo = (MsgInfo *)source;
	auto FolderItem *tof;

	if (!msginfo) return FALSE;
	if (!NewLog) return FALSE;

	tof = msginfo->folder;
	(void)fprintf (NewLog, "---\n"
		"Date:\t%s\n"
		"Subject:\t%s\n"
		"From:\t%s\n"
		"To:\t%s\n"
		"Cc:\t%s\n"
		"Size:\t%jd\n"
		"Path:\t%s\n"
		"Message:\t%d\n"
		"Folder:\t%s\n",
		defstr (msginfo->date),
		defstr (msginfo->subject),
		defstr (msginfo->from),
		defstr (msginfo->to),
		defstr (msginfo->cc),
		(intmax_t) msginfo->size,
		defstr (procmsg_get_message_file_path (msginfo)),
		msginfo->msgnum,
		tof ? defstr (tof->name) : "(null)");

	return (FALSE);
} /* newmail_hook */

gboolean plugin_done (void)
{
	if (NewLog) {
		(void)claws_fclose (NewLog);
		NewLog  = NULL;
	}
	if (LogName) {
		g_free(LogName);
		LogName = NULL;
	}
	if (pluginDesc) {
		g_free(pluginDesc);
		pluginDesc = NULL;
	}
	hooks_unregister_hook (MAIL_POSTFILTERING_HOOKLIST, hook_id);

	debug_print ("Newmail plugin unloaded\n");
	return TRUE;
} /* plugin_done */

gint plugin_init (gchar **error)
{
	if (!check_plugin_version(MAKE_NUMERIC_VERSION(2,9,2,72),
				VERSION_NUMERIC, _("NewMail"), error))
		return -1;

	hook_id = hooks_register_hook (MAIL_POSTFILTERING_HOOKLIST, newmail_hook, NULL);
	if (hook_id == HOOK_NONE) {
		*error = g_strdup (_("Failed to register newmail hook"));
		return (-1);
	}

	if (!NewLog) {
		auto char *mode = truncLog ? "w" : "a";
		if (!LogName) {
			LogName = g_strconcat(g_getenv ("HOME"), G_DIR_SEPARATOR_S, DEFAULT_DIR,
					G_DIR_SEPARATOR_S, LOG_NAME, NULL);
		}
		if (!(NewLog = claws_fopen (LogName, mode))) {
			debug_print ("Failed to open default log %s\n", LogName);
			/* try fallback location */
			g_free(LogName);
			LogName = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, LOG_NAME, NULL);
			if (!(NewLog = claws_fopen (LogName, mode))) {
				debug_print ("Failed to open fallback log %s\n", LogName);
				*error = g_strdup_printf(_("Could not open log file %s: %s\n"),
						LogName, g_strerror(errno));
				plugin_done ();
				return (-1);
			}
		}
		setbuf (NewLog, NULL);
	}

	debug_print ("Newmail plugin loaded\n"
              "Message header summaries written to %s\n", LogName);
	if (pluginDesc == NULL)
		pluginDesc = g_strdup_printf(
			_("This plugin writes a header summary to a log file for each "
			"mail received after sorting.\n\n"
			"Default is ~/Mail/NewLog\n\nCurrent log is %s"), LogName);
	return (0);
} /* plugin_init */

const gchar *plugin_name (void)
{
    return _("NewMail");
} /* plugin_name */

const gchar *plugin_desc (void)
{
    return pluginDesc;
} /* plugin_desc */

const gchar *plugin_type (void)
{
    return ("Common");
} /* plugin_type */

const gchar *plugin_licence (void)
{
    return ("GPL3+");
} /* plugin_licence */

const gchar *plugin_version (void)
{
    return (VERSION);
} /* plugin_version */

struct PluginFeature *plugin_provides(void)
{
	static struct PluginFeature features[] = 
		{ {PLUGIN_NOTIFIER, N_("Log file")},
		  {PLUGIN_NOTHING, NULL}};
	return features;
}
