/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2002 Hiroyuki Yamamoto
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#ifdef WIN32
#include <defs.h>
#endif

#include "prefs.h"
#include "utils.h"

PrefFile *prefs_write_open(const gchar *path)
{
	PrefFile *pfile;
	gchar *tmppath;
	FILE *fp;

	g_return_val_if_fail(path != NULL, NULL);

	if (prefs_is_readonly(path)) {
		g_warning("no permission - %s\n", path);
		return NULL;
	}

	tmppath = g_strconcat(path, ".tmp", NULL);
	if ((fp = fopen(tmppath, "wb")) == NULL) {
		FILE_OP_ERROR(tmppath, "fopen");
		g_free(tmppath);
		return NULL;
	}

	if (change_file_mode_rw(fp, tmppath) < 0)
		FILE_OP_ERROR(tmppath, "chmod");

	g_free(tmppath);

	pfile = g_new(PrefFile, 1);
	pfile->fp = fp;
	pfile->path = g_strdup(path);

	return pfile;
}

gint prefs_write_close(PrefFile *pfile)
{
	FILE *fp;
	gchar *path;
	gchar *tmppath;
	gchar *bakpath = NULL;

	g_return_val_if_fail(pfile != NULL, -1);

	fp = pfile->fp;
	path = pfile->path;
	g_free(pfile);

	tmppath = g_strconcat(path, ".tmp", NULL);
	if (fclose(fp) == EOF) {
		FILE_OP_ERROR(tmppath, "fclose");
		unlink(tmppath);
		g_free(path);
		g_free(tmppath);
		return -1;
	}

	if (is_file_exist(path)) {
		bakpath = g_strconcat(path, ".bak", NULL);
		if (Xrename(path, bakpath) < 0) {
			FILE_OP_ERROR(path, "rename");
			unlink(tmppath);
			g_free(path);
			g_free(tmppath);
			g_free(bakpath);
			return -1;
		}
	}

	if (Xrename(tmppath, path) < 0) {
		FILE_OP_ERROR(tmppath, "rename");
		unlink(tmppath);
		g_free(path);
		g_free(tmppath);
		g_free(bakpath);
		return -1;
	}

	g_free(path);
	g_free(tmppath);
	g_free(bakpath);
	return 0;
}

gint prefs_write_close_revert(PrefFile *pfile)
{
	gchar *tmppath;

	g_return_val_if_fail(pfile != NULL, -1);

	tmppath = g_strconcat(pfile->path, ".tmp", NULL);
	fclose(pfile->fp);
	if (unlink(tmppath) < 0) FILE_OP_ERROR(tmppath, "unlink");
	g_free(tmppath);
	g_free(pfile->path);
	g_free(pfile);

	return 0;
}

gboolean prefs_is_readonly(const gchar * path)
{
	if (path == NULL)
		return TRUE;

	return (access(path, W_OK) != 0 && access(path, F_OK) == 0);
}

gboolean prefs_rc_is_readonly(const gchar * rcfile)
{
	gboolean result;
	gchar * rcpath;

	if (rcfile == NULL)
		return TRUE;

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, rcfile, NULL);
	result = prefs_is_readonly(rcpath);
	g_free(rcpath);

	return result;
}
