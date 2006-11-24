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

 #include <glib.h>
#include <glib/gi18n.h>


#include "version.h"
#include "sylpheed.h"
#include "plugin.h"
#include "utils.h"
#include "hooks.h"
#include "log.h"

gboolean my_log_hook(gpointer source, gpointer data)
{
	LogText *logtext = (LogText *)source;

	printf("*** Demo Plugin log: %s\n", logtext->text);

	return FALSE;
}

static guint hook_id;

gint plugin_init(gchar **error)
{
	if ((sylpheed_get_version() > VERSION_NUMERIC)) {
		*error = g_strdup(_("Your claws-mail version is newer than the version the plugin was built with"));
		return -1;
	}

	if ((sylpheed_get_version() < MAKE_NUMERIC_VERSION(0, 8, 11, 39))) {
		*error = g_strdup(_("Your claws-mail version is too old"));
		return -1;
	}

	hook_id = hooks_register_hook(LOG_APPEND_TEXT_HOOKLIST, my_log_hook, NULL);
	if (hook_id == -1) {
		*error = g_strdup(_("Failed to register log text hook"));
		return -1;
	}

	printf("Demo plugin loaded\n");

	return 0;
}

void plugin_done(void)
{
	hooks_unregister_hook(LOG_APPEND_TEXT_HOOKLIST, hook_id);

	printf("Demo plugin unloaded\n");
}

const gchar *plugin_name(void)
{
	return _("Demo");
}

const gchar *plugin_desc(void)
{
	return _("This Plugin is only a demo of how to write plugins for Claws Mail. "
	         "It installs a hook for new log output and writes it to stdout."
	         "\n\n"
	         "It is not really useful");
}

const gchar *plugin_type(void)
{
	return "Common";
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
		{ {PLUGIN_OTHER, N_("Demo")},
		  {PLUGIN_NOTHING, NULL}};
	return features;
}
