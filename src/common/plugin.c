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

#include <stdio.h>

#include <glib.h>
#include <gmodule.h>

#include "intl.h"
#include "defs.h"
#include "utils.h"
#include "plugin.h"
#include "prefs.h"

struct _Plugin
{
	gchar	*filename;
	gchar	*name;
	GModule	*module;
	gchar	*desc;
};

/**
 * List of all loaded plugins
 */
GSList *plugins = NULL;

void plugin_save_list()
{
	gchar *rcpath;
	PrefFile *pfile;
	GSList *cur;
	Plugin *plugin;

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, "pluginsrc", NULL);
	if ((pfile = prefs_write_open(rcpath)) == NULL) {
		g_warning("failed to write plugin list\n");
		g_free(rcpath);
		return;
	}

	for (cur = plugins; cur != NULL; cur = g_slist_next(cur)) {
		plugin = (Plugin *)cur->data;
			
		fprintf(pfile->fp, "%s\n", plugin->filename);
	}

	if (prefs_write_close(pfile) < 0)
		g_warning("failed to write plugin list\n");

	g_free(rcpath);	
}

/**
 * Loads a plugin
 *
 * \param filename The filename of the plugin to load
 * \param error The location where an error string can be stored
 * \return 0 on success, -1 otherwise
 */
gint plugin_load(const gchar *filename, gchar **error)
{
	Plugin *plugin;
	gint (*plugin_init) (gchar **error);
	gint ok;

	g_return_val_if_fail(filename != NULL, -1);
	g_return_val_if_fail(error != NULL, -1);
	
	plugin = g_new0(Plugin, 1);
	if (plugin == NULL) {
		*error = g_strdup(_("Failed to allocate memory for Plugin"));
		return -1;
	}

	plugin->module = g_module_open(filename, 0);
	if (plugin->module == NULL) {
		*error = g_strdup(g_module_error());
		g_free(plugin);
		return -1;
	}

	if (!g_module_symbol(plugin->module, "plugin_name", (gpointer *)&plugin->name) ||
	    !g_module_symbol(plugin->module, "plugin_desc", (gpointer *)&plugin->desc) ||
	    !g_module_symbol(plugin->module, "plugin_init", (gpointer *)&plugin_init)) {
		*error = g_strdup(g_module_error());
		g_module_close(plugin->module);
		g_free(plugin);
		return -1;
	}

	if ((ok = plugin_init(error)) < 0) {
		g_module_close(plugin->module);
		g_free(plugin);
		return ok;
	}

	plugins = g_slist_append(plugins, plugin);

	debug_print("Plugin %s (from file %s) loaded\n", plugin->name, plugin->filename);

	return 0;
}

void plugin_unload(Plugin *plugin)
{
	void (*plugin_done) ();

	if (g_module_symbol(plugin->module, "plugin_done", (gpointer *)&plugin_done)) {
		plugin_done();
	}

	g_module_close(plugin->module);
	g_free(plugin);
}

void plugin_load_all()
{
	gchar *rcpath;
	gchar buf[BUFFSIZE];
	FILE *fp;

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, "pluginsrc", NULL);	
	if ((fp = fopen(rcpath, "rb")) != NULL) {
		gchar *error;

		while(fgets(buf, sizeof(buf), fp) != NULL) {
			g_strstrip(buf);
			
			if (plugin_load(buf, &error) < 0) {
				debug_print("plugin loading error: %s\n", error);
				g_free(error);
			}
		}

		fclose(fp);
	}

	g_free(rcpath);
}

void plugin_unload_all()
{
	GSList *cur;

	for (cur = plugins; cur != NULL; cur = g_slist_next(cur)) {
		plugin_unload((Plugin *)cur->data);
	}
	g_slist_free(plugins);
	plugins = NULL;
}

GSList *plugin_get_list()
{
	return g_slist_copy(plugins);
}

const gchar *plugin_get_name(Plugin *plugin)
{
	return plugin->name;
}

const gchar *plugin_get_desc(Plugin *plugin)
{
	return plugin->desc;
}
