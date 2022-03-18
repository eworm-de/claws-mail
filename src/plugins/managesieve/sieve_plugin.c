/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2015 the Claws Mail Team
 * Copyright (C) 2014-2015 Charles Lehner
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

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>

#include "version.h"
#include "claws.h"
#include "plugin.h"
#include "utils.h"
#include "hooks.h"
#include "menu.h"
#include "mainwindow.h"
#include "log.h"
#include "sieve_prefs.h"
#include "sieve_manager.h"
#include "sieve_editor.h"

#define PLUGIN_NAME (_("ManageSieve"))

static gint main_menu_id = 0;

static void manage_cb(GtkAction *action, gpointer data) {
	sieve_manager_show();
}

static GtkActionEntry sieve_main_menu[] = {{
	"Tools/ManageSieveFilters",
	NULL, N_("Manage Sieve Filters..."), NULL, NULL, G_CALLBACK(manage_cb)
}};

/**
 * Initialize plugin.
 *
 * @param error  For storing the returned error message.
 *
 * @return 0 if initialization succeeds, -1 on failure.
 */
gint plugin_init(gchar **error)
{
	MainWindow *mainwin = mainwindow_get_mainwindow();

	if (!check_plugin_version(MAKE_NUMERIC_VERSION(2,9,2,72),
				VERSION_NUMERIC, PLUGIN_NAME, error))
		return -1;

	gtk_action_group_add_actions(mainwin->action_group, sieve_main_menu, 1,
			(gpointer)mainwin);
	MENUITEM_ADDUI_ID_MANAGER(mainwin->ui_manager,
			"/Menu/Tools", "ManageSieveFilters", "Tools/ManageSieveFilters",
			GTK_UI_MANAGER_MENUITEM, main_menu_id)

	sieve_prefs_init();

	debug_print("ManageSieve plugin loaded\n");

	return 0;
}

/**
 * Destructor for the plugin.
 * Unregister callback functions and free stuff.
 *
 * @return Always TRUE.
 */
gboolean plugin_done(void)
{
	MainWindow *mainwin = mainwindow_get_mainwindow();

	sieve_prefs_done();
	sieve_managers_done();
	sieve_editors_close();
	sieve_sessions_close();

	if (mainwin)
		MENUITEM_REMUI_MANAGER(mainwin->ui_manager,
				mainwin->action_group,
				"Tools/ManageSieveFilters", main_menu_id);

	debug_print("ManageSieve plugin unloaded\n");

	return TRUE;
}

const gchar *plugin_name(void)
{
	return PLUGIN_NAME;
}

/**
 * Get the description of the plugin.
 *
 * @return The plugin's description, maybe translated.
 */
const gchar *plugin_desc(void)
{
	return _("Manage sieve filters on a server using the ManageSieve protocol.");
}

/**
 * Get the kind of plugin.
 *
 * @return The "GTK3" constant.
 */
const gchar *plugin_type(void)
{
	return "GTK3";
}
/**
 * Get the license acronym the plugin is released under.
 *
 * @return The "GPL3+" constant.
 */
const gchar *plugin_licence(void)
{
	return "GPL3+";
}

/**
 * Get the version of the plugin.
 *
 * @return The current version string.
 */
const gchar *plugin_version(void)
{
	return VERSION;
}

/**
 * Get the features implemented by the plugin.
 *
 * @return A constant PluginFeature structure with the features.
 */
struct PluginFeature *plugin_provides(void)
{
	static struct PluginFeature features[] =
		{ {PLUGIN_UTILITY, N_("ManageSieve")},
		  {PLUGIN_NOTHING, NULL}};

	return features;
}
