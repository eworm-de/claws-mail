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

#include "defs.h"
#include <glib.h>
#include <glib/gi18n.h>
#include <gmodule.h>

#include "utils.h"
#include "plugin.h"
#include "prefs.h"

struct _Plugin
{
	gchar	*filename;
	GModule	*module;
	const gchar *(*name) (void);
	const gchar *(*desc) (void);
	const gchar *(*type) (void);
};

/**
 * List of all loaded plugins
 */
GSList *plugins = NULL;
GSList *plugin_types = NULL;

static gint list_find_by_string(gconstpointer data, gconstpointer str)
{
	return strcmp((gchar *)data, (gchar *)str) ? TRUE : FALSE;
}

static gint list_find_by_plugin_filename(const Plugin *plugin, const gchar *filename)
{
        g_return_val_if_fail(plugin, 1);
        g_return_val_if_fail(plugin->filename, 1);
        g_return_val_if_fail(filename, 1);
        
        return strcmp(filename, plugin->filename);
}

void plugin_save_list(void)
{
	gchar *rcpath, *block;
	PrefFile *pfile;
	GSList *type_cur, *plugin_cur;
	Plugin *plugin;

	for (type_cur = plugin_types; type_cur != NULL; type_cur = g_slist_next(type_cur)) {
		rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMON_RC, NULL);
		block = g_strconcat("Plugins_", type_cur->data, NULL);
		if ((pfile = prefs_write_open(rcpath)) == NULL ||
		    (prefs_set_block_label(pfile, block) < 0)) {
			g_warning("failed to write plugin list\n");
			g_free(rcpath);
			return;
		}
		g_free(block);

		for (plugin_cur = plugins; plugin_cur != NULL; plugin_cur = g_slist_next(plugin_cur)) {
			plugin = (Plugin *) plugin_cur->data;
			
			if (!strcmp(plugin->type(), type_cur->data))
				fprintf(pfile->fp, "%s\n", plugin->filename);
		}
		fprintf(pfile->fp, "\n");

		if (prefs_file_close(pfile) < 0)
			g_warning("failed to write plugin list\n");

		g_free(rcpath);	
	}
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
	gpointer plugin_name, plugin_desc;
	const gchar *(*plugin_type)(void);
	gint ok;

	g_return_val_if_fail(filename != NULL, -1);
	g_return_val_if_fail(error != NULL, -1);

        /* check duplicate plugin path name */
        if (g_slist_find_custom(plugins, filename, 
                                (GCompareFunc)list_find_by_plugin_filename)) {
                *error = g_strdup(_("Plugin already loaded"));
                return -1;                
        }                               
	
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

	if (!g_module_symbol(plugin->module, "plugin_name", &plugin_name) ||
	    !g_module_symbol(plugin->module, "plugin_desc", &plugin_desc) ||
	    !g_module_symbol(plugin->module, "plugin_type", (gpointer *) &plugin_type) ||
	    !g_module_symbol(plugin->module, "plugin_init", (gpointer *) &plugin_init)) {
		*error = g_strdup(g_module_error());
		g_module_close(plugin->module);
		g_free(plugin);
		return -1;
	}
	
	if (!strcmp(plugin_type(), "GTK")) {
		*error = g_strdup(_("This module is for Sylpheed-Claws GTK1."));
		g_module_close(plugin->module);
		g_free(plugin);
		return -1;
	}

	if ((ok = plugin_init(error)) < 0) {
		g_module_close(plugin->module);
		g_free(plugin);
		return ok;
	}

	plugin->name = plugin_name;
	plugin->desc = plugin_desc;
	plugin->type = plugin_type;
	plugin->filename = g_strdup(filename);

	plugins = g_slist_append(plugins, plugin);

	debug_print("Plugin %s (from file %s) loaded\n", plugin->name(), filename);

	return 0;
}

void plugin_unload(Plugin *plugin)
{
	void (*plugin_done) (void);

	if (g_module_symbol(plugin->module, "plugin_done", (gpointer *)&plugin_done)) {
		plugin_done();
	}

	g_module_close(plugin->module);
	plugins = g_slist_remove(plugins, plugin);
	g_free(plugin->filename);
	g_free(plugin);
}

void plugin_load_all(const gchar *type)
{
	gchar *rcpath;
	gchar buf[BUFFSIZE];
	PrefFile *pfile;
	gchar *error, *block;

	plugin_types = g_slist_append(plugin_types, g_strdup(type));

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMON_RC, NULL);	
	block = g_strconcat("Plugins_", type, NULL);
	if ((pfile = prefs_read_open(rcpath)) == NULL ||
	    (prefs_set_block_label(pfile, block) < 0)) {
		g_free(rcpath);
		return;
	}
	g_free(block);

	while (fgets(buf, sizeof(buf), pfile->fp) != NULL) {
		if (buf[0] == '[')
			break;

		g_strstrip(buf);
		if ((buf[0] != '\0') && (plugin_load(buf, &error) < 0)) {
			g_warning("plugin loading error: %s\n", error);
			g_free(error);
		}							
	}
	prefs_file_close(pfile);

	g_free(rcpath);
}

void plugin_unload_all(const gchar *type)
{
	GSList *list, *cur;

	list = g_slist_copy(plugins);
	list = g_slist_reverse(list);

	for(cur = list; cur != NULL; cur = g_slist_next(cur)) {
		Plugin *plugin = (Plugin *) cur->data;
		
		if (!strcmp(type, plugin->type()))
			plugin_unload(plugin);
	}
	g_slist_free(list);

	cur = g_slist_find_custom(plugin_types, (gpointer) type, list_find_by_string);
	if (cur) {
		g_free(cur->data);
		g_slist_remove(plugin_types, cur);
	}
}

GSList *plugin_get_list(void)
{
	return g_slist_copy(plugins);
}

const gchar *plugin_get_name(Plugin *plugin)
{
	return plugin->name();
}

const gchar *plugin_get_desc(Plugin *plugin)
{
	return plugin->desc();
}
