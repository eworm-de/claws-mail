/*
 * Sylpheed templates subsystem 
 * Copyright (C) 2001 Alexander Barinov
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

#include "defs.h"

#include <gdk/gdk.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>

#include "utils.h"
#include "main.h"
#include "template.h"
#include "intl.h"

static GSList* template_load(GSList *tmpl_list, gchar *filename)
{
	Template *tmpl;
	FILE *fp;
	char buf[32000];
	gint bytes_read;

	LOG_MESSAGE(_("%s:%d loading template from %s\n"), __FILE__, __LINE__, filename);

	if ((fp = fopen(filename, "r")) == NULL) {
		FILE_OP_ERROR(filename, "fopen");
		return tmpl_list;
	}

	tmpl = g_new(Template, 1);
	
	if (fgets(buf, sizeof(buf), fp) == NULL) {
		FILE_OP_ERROR(filename, "fgets");
		g_free(tmpl);
		LOG_MESSAGE(_("%s:%d exiting\n"), __FILE__, __LINE__);
		return tmpl_list;
	}
	tmpl->name = g_strdup(g_strstrip(buf));

	memset(buf, 0, sizeof(buf));
	if ((bytes_read = fread(buf, 1, sizeof(buf)-1, fp)) == 0) {
		FILE_OP_ERROR(filename, "fread");
		g_free(tmpl->name);
		g_free(tmpl);
		return tmpl_list;
	}
	tmpl->value = g_strdup(buf);

	tmpl_list = g_slist_append(tmpl_list, tmpl);
	fclose(fp);
	return tmpl_list;
}

void template_free(Template *tmpl)
{
	g_free(tmpl->name);
	g_free(tmpl->value);
	g_free(tmpl);
}

void template_clear_config(GSList *tmpl_list)
{
	Template *tmpl;

	while (tmpl_list != NULL) {
		tmpl = tmpl_list->data;
		template_free(tmpl);
		tmpl_list = g_slist_remove(tmpl_list, tmpl);
	}
}

GSList* template_read_config(void)
{
	gchar *path;
	gchar *filename;
	DIR *dp;
	struct dirent *de;
	struct stat s;
	GSList *tmpl_list = NULL;

	path = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, TEMPLATES_DIR, NULL);
	LOG_MESSAGE(_("%s:%d reading templates dir %s\n"), __FILE__, __LINE__, path);

	if ((dp = opendir(path)) == NULL) {
		FILE_OP_ERROR(path, "opendir");
		return tmpl_list;
	}

	while ((de = readdir(dp)) != NULL) {
		filename = g_strconcat(path, G_DIR_SEPARATOR_S, de->d_name, NULL);
		LOG_MESSAGE(_("%s:%d found file %s\n"), __FILE__, __LINE__, filename);

		if (stat(filename, &s) != 0 || !S_ISREG(s.st_mode) ) {
			LOG_MESSAGE(_("%s:%d %s is not an ordinary file\n"), 
			            __FILE__, __LINE__, filename);
			continue;
		}

		tmpl_list = template_load(tmpl_list, filename);
		g_free(filename);
	}

	closedir(dp);
	g_free(path);
	return tmpl_list;
}

void template_write_config(GSList *tmpl_list)
{
	gchar *path;
	gchar *filename;
	GSList *cur;
	Template *tmpl;
	FILE *fp;
	gint tmpl_num = 1;

	path = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, TEMPLATES_DIR, NULL);

	if (!is_dir_exist(path)) {
		if (is_file_exist(path)) {
			LOG_MESSAGE(_("%s:%d file %s allready exists\n"), 
			            __FILE__, __LINE__, filename);
			g_free(path);
			return;
		}
		if (mkdir(path, S_IRWXU) < 0) {
			FILE_OP_ERROR(path, "mkdir");
			g_free(path);
			return;
		}
	}

	remove_all_files(path);

	for (cur = tmpl_list; cur != NULL; cur = cur->next) {
		tmpl = cur->data;

		filename = g_strconcat(path, G_DIR_SEPARATOR_S, itos(tmpl_num), NULL);

		if ((fp = fopen(filename, "w")) == NULL) {
			FILE_OP_ERROR(filename, "fopen");
			g_free(filename);
			g_free(path);
			return;
		}

		LOG_MESSAGE(_("%s:%d writing template \"%s\" to %s\n"), 
		            __FILE__, __LINE__, tmpl->name, filename);
		fputs(tmpl->name, fp);
		fputs("\n", fp);
		fwrite(tmpl->value, sizeof(gchar), strlen(tmpl->value), fp);
		fclose(fp);

		tmpl_num ++;
	}

	g_free(path);
}
