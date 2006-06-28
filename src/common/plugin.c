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

#include <stdio.h>

#include "defs.h"
#include <glib.h>
#ifdef ENABLE_NLS
#include <glib/gi18n.h>
#else
#define _(a) (a)
#define N_(a) (a)
#endif
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
	const gchar *(*version) (void);
	const gchar *(*type) (void);
	const gchar *(*licence) (void);
	GSList *rdeps;
	gchar *error;
};

/**
 * List of all loaded plugins
 */
GSList *plugins = NULL;
GSList *plugin_types = NULL;

/* 
 * List of plugins unloaded for some fixable reason
 */
static GSList *unloaded_plugins = NULL;

static gint list_find_by_string(gconstpointer data, gconstpointer str)
{
	return strcmp((gchar *)data, (gchar *)str) ? TRUE : FALSE;
}

static gint list_find_by_plugin_filename(const Plugin *plugin, const gchar *filename)
{
	/* FIXME: There is a problem in case of symlinks or when a
	   user tries to load a plugin with the same name from a
	   different directory.  I think it would be better to compare
	   only the basename of the filename here (case-insensitive on
	   W32). */
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
#ifdef G_OS_WIN32
		block = g_strconcat("PluginsWin32_", type_cur->data, NULL);
#else
		block = g_strconcat("Plugins_", type_cur->data, NULL);
#endif
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
		for (plugin_cur = unloaded_plugins; plugin_cur != NULL; plugin_cur = g_slist_next(plugin_cur)) {
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

static gboolean plugin_is_loaded(const gchar *filename)
{
	return (g_slist_find_custom(plugins, filename, 
		  (GCompareFunc)list_find_by_plugin_filename) != NULL);
}

static Plugin *plugin_get_by_filename(const gchar *filename)
{
	GSList *cur = plugins;
	for(; cur; cur = cur->next) {
		Plugin *p = (Plugin *)cur->data;
		if (!strcmp(p->filename, filename)) {
			return p;
		} 
	}
	return NULL;
}

/** 
 * Loads a plugin dependancies
 * 
 * Plugin dependancies are, optionnaly, listed in a file in
 * get_plugin_dir()/$pluginname.deps.
 * \param filename The filename of the plugin for which we have to load deps
 * \param error The location where an error string can be stored
 * \return 0 on success, -1 otherwise
 */
static gint plugin_load_deps(const gchar *filename, gchar **error)
{
	gchar *tmp;
	gchar *deps_file = NULL;
	FILE *fp = NULL;
	gchar buf[BUFFSIZE];
	gchar *p;

	tmp = g_strdup(filename);
	if( (p = strrchr(tmp, '.')) )
	  *p = '\0';
	deps_file = g_strconcat(tmp, ".deps", NULL);
	g_free(tmp);
	
	fp = g_fopen(deps_file, "rb");
	g_free(deps_file);
	
	if (!fp)
		return 0;
	
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		Plugin *dep_plugin = NULL;
		gchar *path = NULL;
		buf[strlen(buf)-1]='\0'; /* chop off \n */
		path = g_strconcat(get_plugin_dir(), buf,
				".", G_MODULE_SUFFIX, NULL);
		if ((dep_plugin = plugin_get_by_filename(path)) == NULL) {
			debug_print("trying to load %s\n", path);
			dep_plugin = plugin_load(path, error);
			if (dep_plugin == NULL) {
				g_free(path);
				return -1;
			}
		}
		g_free(path);
		if (!g_slist_find_custom(dep_plugin->rdeps, 
				(gpointer) filename, list_find_by_string)) {
			debug_print("adding %s to %s rdeps\n",
				filename,
				dep_plugin->filename);
			dep_plugin->rdeps = 
				g_slist_append(dep_plugin->rdeps, 
					g_strdup(filename));
		}
	}
	fclose(fp);
	return 0;
}

static void plugin_unload_rdeps(Plugin *plugin)
{
	GSList *cur = plugin->rdeps;
	debug_print("removing %s rdeps\n", plugin->filename);
	while (cur) {
		gchar *file = (gchar *)cur->data;
		Plugin *rdep_plugin = file?plugin_get_by_filename(file):NULL;
		debug_print(" rdep %s: %p\n", file, rdep_plugin);
		if (rdep_plugin) {
			plugin_unload(rdep_plugin);
		}
		g_free(file);
		cur = cur->next;
	}
	g_slist_free(plugin->rdeps);
	plugin->rdeps = NULL;
}

static void plugin_remove_from_unloaded_list (const gchar *filename)
{
	GSList *item = g_slist_find_custom(unloaded_plugins, 
				(gpointer) filename, (GCompareFunc)list_find_by_plugin_filename);
	Plugin *unloaded_plugin = item ? ((Plugin *)item->data):NULL;
	if (unloaded_plugin != NULL) {
		debug_print("removing %s from unloaded list\n", unloaded_plugin->filename);
		unloaded_plugins = g_slist_remove(unloaded_plugins, unloaded_plugin);
		g_module_close(unloaded_plugin->module);
		g_free(unloaded_plugin->filename);
		g_free(unloaded_plugin->error);
		g_free(unloaded_plugin);
	}
}

/**
 * Loads a plugin
 *
 * \param filename The filename of the plugin to load
 * \param error The location where an error string can be stored
 * \return the plugin on success, NULL otherwise
 */
Plugin *plugin_load(const gchar *filename, gchar **error)
{
	Plugin *plugin;
	gint (*plugin_init) (gchar **error);
	gpointer plugin_name, plugin_desc, plugin_version;
	const gchar *(*plugin_type)(void);
	const gchar *(*plugin_licence)(void);
	gint ok;

	g_return_val_if_fail(filename != NULL, NULL);
	g_return_val_if_fail(error != NULL, NULL);

	/* check duplicate plugin path name */
	if (plugin_is_loaded(filename)) {
		*error = g_strdup(_("Plugin already loaded"));
		return NULL;		
	}			       
	
	plugin_remove_from_unloaded_list(filename);
	
	if (plugin_load_deps(filename, error) < 0)
		return NULL;
	plugin = g_new0(Plugin, 1);
	if (plugin == NULL) {
		*error = g_strdup(_("Failed to allocate memory for Plugin"));
		return NULL;
	}

	debug_print("trying to load `%s'\n", filename);
	plugin->module = g_module_open(filename, 0);
	if (plugin->module == NULL) {
		*error = g_strdup(g_module_error());
		g_free(plugin);
		return NULL;
	}

	if (!g_module_symbol(plugin->module, "plugin_name", &plugin_name) ||
	    !g_module_symbol(plugin->module, "plugin_desc", &plugin_desc) ||
	    !g_module_symbol(plugin->module, "plugin_version", &plugin_version) ||
	    !g_module_symbol(plugin->module, "plugin_type", (gpointer)&plugin_type) ||
	    !g_module_symbol(plugin->module, "plugin_licence", (gpointer)&plugin_licence) ||
	    !g_module_symbol(plugin->module, "plugin_init", (gpointer)&plugin_init)) {
		*error = g_strdup(g_module_error());
		g_module_close(plugin->module);
		g_free(plugin);
		return NULL;
	}
	
	if (strcmp(plugin_licence(), "GPL")
	&&  strncmp(plugin_licence(), "GPL-compatible", strlen("GPL-compatible"))) {
		*error = g_strdup(_("This module is not licenced under a GPL compatible licence."));
		g_module_close(plugin->module);
		g_free(plugin);
		return NULL;
	}

	if (!strcmp(plugin_type(), "GTK")) {
		*error = g_strdup(_("This module is for Sylpheed-Claws GTK1."));
		g_module_close(plugin->module);
		g_free(plugin);
		return NULL;
	}

	plugin->name = plugin_name;
	plugin->desc = plugin_desc;
	plugin->version = plugin_version;
	plugin->type = plugin_type;
	plugin->licence = plugin_licence;
	plugin->filename = g_strdup(filename);
	plugin->error = NULL;
	if ((ok = plugin_init(error)) < 0) {
		if (*error)
			plugin->error = g_strdup(*error);
		unloaded_plugins = g_slist_append(unloaded_plugins, plugin);
		return NULL;
	}

	plugins = g_slist_append(plugins, plugin);

	debug_print("Plugin %s (from file %s) loaded\n", plugin->name(), filename);

	return plugin;
}

void plugin_unload(Plugin *plugin)
{
	void (*plugin_done) (void);

	plugin_unload_rdeps(plugin);

	if (plugin->error) {
		plugin_remove_from_unloaded_list(plugin->filename);
		return;
	}
	if (g_module_symbol(plugin->module, "plugin_done", (gpointer) &plugin_done)) {
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
	gchar *error = NULL, *block;

	plugin_types = g_slist_append(plugin_types, g_strdup(type));

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMON_RC, NULL);	
#ifdef G_OS_WIN32
	block = g_strconcat("PluginsWin32_", type, NULL);
#else
	block = g_strconcat("Plugins_", type, NULL);
#endif
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
		if ((buf[0] != '\0') && (plugin_load(buf, &error) == NULL)) {
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
		plugin_types = g_slist_remove(plugin_types, cur);
	}
}


/* Load those plugins we always want to use.  No error output; just
 * try. */
void plugin_load_standard_plugins (void)
{
	static const char *names[] = {
#ifdef G_OS_WIN32 
		"pgpmime",
		"pgpinline",
#endif
		NULL
	};
	int i;
	gchar *error, *filename;
	
	for (i=0; names[i]; i++) {
		/* Simple hack to check whether the plugin has already
		 * been loaded but checking only for the basename. */
		GSList *cur = plugins;
		for(; cur; cur = cur->next) {
			Plugin *p = (Plugin *)cur->data;
			if (strstr(p->filename, names[i]))
				break;
		}
		if (!cur) { /* Not yet loaded. */
			/* FIXME: get_plugin_dir () returns with a trailing
			 * (back)slash; this should be fixed so that we can use
			 * g_module_build_path here. */
			filename = g_strconcat (get_plugin_dir(),
						names[i], NULL);
			error = NULL;
			plugin_load(filename, &error);
			g_free (error);
			g_free(filename);
		}
	}
}


GSList *plugin_get_list(void)
{
	return g_slist_copy(plugins);
}

GSList *plugin_get_unloaded_list(void)
{
	return g_slist_copy(unloaded_plugins);
}

const gchar *plugin_get_name(Plugin *plugin)
{
	return plugin->name();
}

const gchar *plugin_get_desc(Plugin *plugin)
{
	return plugin->desc();
}

const gchar *plugin_get_version(Plugin *plugin)
{
	return plugin->version();
}

const gchar *plugin_get_error(Plugin *plugin)
{
	return plugin->error;
}
