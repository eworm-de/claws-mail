/*
 * Sylpheed templates subsystem 
 * Copyright (C) 2001 Alexander Barinov
 * Copyright (C) 2001 Hiroyuki Yamamoto
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

#include <glib.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>

#include "intl.h"
#include "utils.h"
#include "template.h"
#include "../codeconv.h"

static GSList *template_list;

static Template *template_load(gchar *filename)
{
	Template *tmpl;
	FILE *fp;
	gchar buf[BUFFSIZE];
	gint bytes_read;
#warning FIXME_GTK2
	const gchar *src_codeset = conv_get_current_charset_str();
	const gchar *dest_codeset = CS_UTF_8;

	if ((fp = fopen(filename, "rb")) == NULL) {
		FILE_OP_ERROR(filename, "fopen");
		return NULL;
	}

	tmpl = g_new(Template, 1);
	tmpl->name = NULL;
	tmpl->subject = NULL;
	tmpl->to = NULL;
	tmpl->cc = NULL;	
	tmpl->bcc = NULL;	
	tmpl->value = NULL;

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		gchar *tmp = NULL;

		if (buf[0] == '\n')
			break;
		else if (!g_ascii_strncasecmp(buf, "Name:", 5)) {
			tmp = conv_codeset_strdup(buf + 5,
						  src_codeset,
						  dest_codeset);
			tmpl->name = tmp ? g_strstrip(tmp) : g_strdup(buf + 5);
		} else if (!g_ascii_strncasecmp(buf, "Subject:", 8)) {
			tmp = conv_codeset_strdup(buf + 8,
						  src_codeset,
						  dest_codeset);
			tmpl->subject = tmp ? g_strstrip(tmp) : g_strdup(buf + 8);
		} else if (!g_ascii_strncasecmp(buf, "To:", 3)) {
			tmp = conv_codeset_strdup(buf + 3,
						  src_codeset,
						  dest_codeset);
			tmpl->to = tmp ? g_strstrip(tmp) : g_strdup(buf + 3);
		} else if (!g_ascii_strncasecmp(buf, "Cc:", 3)) {
			tmp = conv_codeset_strdup(buf + 3,
						  src_codeset,
						  dest_codeset);
			tmpl->cc = tmp ? g_strstrip(tmp) : g_strdup(buf + 3);
		} else if (!g_ascii_strncasecmp(buf, "Bcc:", 4)) {
			tmp = conv_codeset_strdup(buf + 4,
						  src_codeset,
						  dest_codeset);
			tmpl->bcc = tmp ? g_strstrip(tmp) : g_strdup(buf + 4);
		}
	}

	if (!tmpl->name) {
		g_warning("wrong template format\n");
		template_free(tmpl);
		return NULL;
	}

	if ((bytes_read = fread(buf, 1, sizeof(buf), fp)) == 0) {
		if (ferror(fp)) {
			FILE_OP_ERROR(filename, "fread");
			template_free(tmpl);
			return NULL;
		}
	}
	fclose(fp);
	buf[bytes_read] = '\0';
	tmpl->value = conv_codeset_strdup(buf, src_codeset, dest_codeset);
	if (!tmpl->value)
		tmpl->value = g_strdup(buf);

	return tmpl;
}

void template_free(Template *tmpl)
{
	g_free(tmpl->name);
	g_free(tmpl->subject);
	g_free(tmpl->to);
	g_free(tmpl->cc);
	g_free(tmpl->bcc);		
	g_free(tmpl->value);
	g_free(tmpl);
}

void template_clear_config(GSList *tmpl_list)
{
	GSList *cur;
	Template *tmpl;

	for (cur = tmpl_list; cur != NULL; cur = cur->next) {
		tmpl = (Template *)cur->data;
		template_free(tmpl);
	}
	g_slist_free(tmpl_list);
}

GSList *template_read_config(void)
{
	const gchar *path;
	gchar *filename;
	DIR *dp;
	struct dirent *de;
	struct stat s;
	Template *tmpl;
	GSList *tmpl_list = NULL;

	path = get_template_dir();
	debug_print("%s:%d reading templates dir %s\n",
		    __FILE__, __LINE__, path);

	if (!is_dir_exist(path)) {
		if (make_dir(path) < 0)
			return NULL;
	}

	if ((dp = opendir(path)) == NULL) {
		FILE_OP_ERROR(path, "opendir");
		return NULL;
	}

	while ((de = readdir(dp)) != NULL) {
		if (*de->d_name == '.') continue;

		filename = g_strconcat(path, G_DIR_SEPARATOR_S, de->d_name, NULL);

		if (stat(filename, &s) != 0 || !S_ISREG(s.st_mode) ) {
			debug_print("%s:%d %s is not an ordinary file\n",
				    __FILE__, __LINE__, filename);
			continue;
		}

		tmpl = template_load(filename);
		if (tmpl)
			tmpl_list = g_slist_append(tmpl_list, tmpl);
		g_free(filename);
	}

	closedir(dp);

	return tmpl_list;
}

void template_write_config(GSList *tmpl_list)
{
	const gchar *path;
	GSList *cur;
	Template *tmpl;
	FILE *fp;
	gint tmpl_num;

	debug_print("%s:%d writing templates\n", __FILE__, __LINE__);

	path = get_template_dir();

	if (!is_dir_exist(path)) {
		if (is_file_exist(path)) {
			g_warning("file %s already exists\n", path);
			return;
		}
		if (make_dir(path) < 0)
			return;
	}

	remove_all_files(path);

	for (cur = tmpl_list, tmpl_num = 1; cur != NULL;
	     cur = cur->next, tmpl_num++) {
#warning FIXME_GTK2
		const gchar *src_codeset = CS_UTF_8;
		const gchar *dest_codeset = conv_get_current_charset_str();
		gchar *tmp = NULL;
		gchar *filename;

		tmpl = cur->data;

		filename = g_strconcat(path, G_DIR_SEPARATOR_S,
				       itos(tmpl_num), NULL);

		if ((fp = fopen(filename, "wb")) == NULL) {
			FILE_OP_ERROR(filename, "fopen");
			g_free(filename);
			return;
		}

		tmp = conv_codeset_strdup(tmpl->name, src_codeset, dest_codeset);
		if (!tmp)
			tmp = g_strdup(tmpl->name);
		fprintf(fp, "Name: %s\n", tmp ? tmp : "");
		g_free(tmp);

		if (tmpl->subject && *tmpl->subject != '\0') {
			tmp = conv_codeset_strdup(tmpl->subject,
						  src_codeset, dest_codeset);
			if (!tmp)
				tmp = g_strdup(tmpl->subject);
			fprintf(fp, "Subject: %s\n", tmp);
			g_free(tmp);
		}

		if (tmpl->to && *tmpl->to != '\0') {
			tmp = conv_codeset_strdup(tmpl->to,
						  src_codeset, dest_codeset);
			if (!tmp)
				tmp = g_strdup(tmpl->to);
			fprintf(fp, "To: %s\n", tmp);
			g_free(tmp);
		}

		if (tmpl->cc && *tmpl->cc != '\0') {
			tmp = conv_codeset_strdup(tmpl->cc,
						  src_codeset, dest_codeset);
			if (!tmp)
				tmp = g_strdup(tmpl->cc);
			fprintf(fp, "Cc: %s\n", tmp);
			g_free(tmp);
		}

		if (tmpl->bcc && *tmpl->bcc != '\0') {
			tmp = conv_codeset_strdup(tmpl->bcc,
						  src_codeset, dest_codeset);
			if (!tmp)
				tmp = g_strdup(tmpl->bcc);
			fprintf(fp, "Bcc: %s\n", tmp);
			g_free(tmp);
		}

		fputs("\n", fp);
		tmp = conv_codeset_strdup(tmpl->value,
					  src_codeset, dest_codeset);
		if (!tmp)
			tmp = g_strdup(tmpl->value);
		fwrite(tmp, sizeof(gchar) * strlen(tmp), 1, fp);
		g_free(tmp);
		fclose(fp);
		g_free(filename);
	}
}

GSList *template_get_config(void)
{
	if (!template_list)
		template_list = template_read_config();

	return template_list;
}

void template_set_config(GSList *tmpl_list)
{
	template_clear_config(template_list);
	template_write_config(tmpl_list);
	template_list = tmpl_list;
}
