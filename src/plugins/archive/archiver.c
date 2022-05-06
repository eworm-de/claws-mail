/* vim:et:ts=4:sw=4:et:sts=4:ai:set list listchars=tab\:»·,trail\:·: */

/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2018 Michael Rasmussen and the Claws Mail Team
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>

#include <gtk/gtk.h>

#include "common/claws.h"
#include "common/version.h"
#include "plugin.h"
#include "mimeview.h"
#include "utils.h"
#include "alertpanel.h"
#include "statusbar.h"
#include "archiver.h"
#include "archiver_prefs.h"
#include "menu.h"

#include <archive.h>

#define PLUGIN_NAME (_("Mail Archiver"))

static void create_archive_cb(GtkAction *action, gpointer data) {

	debug_print("Call-back function called\n");
	
	archiver_gtk_show();
}

static GtkActionEntry archiver_main_menu[] = {{
	"Tools/CreateArchive",
	NULL, N_("Create Archive..."), NULL, NULL, G_CALLBACK(create_archive_cb)
}};

static gint main_menu_id = 0;
static char *plugin_description = NULL;

gint plugin_init(gchar** error)
{
	MainWindow *mainwin = mainwindow_get_mainwindow();

	if (!check_plugin_version(MAKE_NUMERIC_VERSION(3,4,0,65),
				VERSION_NUMERIC, PLUGIN_NAME, error))
		return -1;

	gtk_action_group_add_actions(mainwin->action_group, archiver_main_menu,
			1, (gpointer)mainwin);
	MENUITEM_ADDUI_ID_MANAGER(mainwin->ui_manager, "/Menu/Tools", "CreateArchive", 
			  "Tools/CreateArchive", GTK_UI_MANAGER_MENUITEM,
			  main_menu_id)

	archiver_prefs_init();

	debug_print("archive plugin loaded\n");

	return 0;
}

gboolean plugin_done(void)
{
	MainWindow *mainwin = mainwindow_get_mainwindow();

	if (mainwin == NULL)
		return FALSE;

	MENUITEM_REMUI_MANAGER(mainwin->ui_manager,mainwin->action_group, "Tools/CreateArchive", main_menu_id);
	main_menu_id = 0;

	if (plugin_description != NULL) {
		g_free(plugin_description);
		plugin_description = NULL;
	}

	archiver_prefs_done();
	debug_print("archive plugin unloaded\n");

	return TRUE;
}

const gchar* plugin_licence(void) {
	return "GPL3+";
}

const gchar* plugin_version(void) {
	return VERSION;
}

const gchar* plugin_type(void) {
	return "GTK3";
}

const gchar* plugin_name(void) {
	return PLUGIN_NAME;
}

const gchar* plugin_desc(void) {
	if (plugin_description == NULL) {

		plugin_description = g_strdup_printf(_("This plugin adds archiving features to Claws Mail.\n"
			"\n"
			"It enables you to select a mail folder that you want "
			"to be archived, and then choose a name, format and "
			"location for the archive. Subfolders can be included "
			"and MD5 checksums can be added for each file in the "
			"archive. Several archiving options are also available.\n"
			"\n"
			"The archive can be stored as:\n%s"
			"\n"
			"The archive can be compressed using:\n%s"
			"\n"
			"The archives can be restored with any standard tool "
			"that supports the chosen format and compression.\n"
			"\n"
			"The supported folder types are MH, IMAP, RSSyl and "
			"vCalendar.\n"
			"\n"
			"To activate the archiving feature go to /Tools/Create Archive\n"
			"\n"
			"Default options can be set in /Configuration/Preferences/Plugins"
			"/Mail Archiver"),

/* archive formats (untranslated, libarchive-version dependant) */
			"\tTAR\n\tPAX\n\tSHAR\n\tCPIO\n",

/* compression formats (untranslated, libarchive-version dependant) */
			"\tGZIP\n\tBZIP2\n\tCOMPRESS\n"
#if ARCHIVE_VERSION_NUMBER >= 2006990
			"\tLZMA\n\tXZ\n"
#endif
#if ARCHIVE_VERSION_NUMBER >= 3000000
			"\tLZIP\n"
#endif
#if ARCHIVE_VERSION_NUMBER >= 3001000
			"\tLRZIP\n\tLZOP\n\tGRZIP\n"
#endif
#if ARCHIVE_VERSION_NUMBER >= 3001900
			"\tLZ4\n"
#endif
			);
	}
	return plugin_description;
}

struct PluginFeature* plugin_provides(void) {
	static struct PluginFeature features[] =
	{ {PLUGIN_UTILITY, N_("Archiver")},
	  {PLUGIN_NOTHING, NULL} };
	return features;
}
