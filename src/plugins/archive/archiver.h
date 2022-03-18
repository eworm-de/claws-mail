/* vim: set textwidth=80 tabstop=4: */

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

#ifndef __ARCHIVER_H__
#define __ARCHIVER_H__

#include <glib.h>
#include <gtk/gtk.h>

static const char INVALID_UNIX_CHARS[] = {
		'?', '%', '*', ':', '"', '|', '<', '>',
		'(', ')', '#', ',', '\0'};

struct ArchivePage {
	gchar*		path;
	gchar*		name;
	gboolean	response;
	gboolean	force_overwrite;
	gboolean	md5;
	gboolean	rename;
	GtkWidget*	folder;
	GtkWidget*	file;
	guint		files;
	guint		total_size;
	GSList*		compress_methods;
	GSList*		archive_formats;
	GtkWidget*	recursive;
	GtkWidget*	md5sum;
	GtkWidget*	rename_files;
	gboolean	cancelled;
    GtkWidget*  isoDate;
    gboolean    unlink;
    GtkWidget*  unlink_files;
};

void archiver_gtk_show();
void archiver_gtk_done();
void set_progress_file_label(const gchar* file);
void set_progress_print_all(guint fraction, guint total, guint step);
void stop_archiving();

#endif
