/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2006 Hiroyuki Yamamoto and the Sylpheed-Claws team
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

#ifndef __FILESEL_H__
#define __FILESEL_H__

#include <glib.h>

gchar *filesel_select_file_open(const gchar *title, const gchar *path);
gchar *filesel_select_file_open_with_filter(const gchar *title, const gchar *path,
					    const gchar *filter);
gchar *filesel_select_file_save(const gchar *title, const gchar *path);
gchar *filesel_select_file_open_folder(const gchar *title, const gchar *path);
gchar *filesel_select_file_save_folder(const gchar *title, const gchar *path);

GList *filesel_select_multiple_files_open(const gchar *title);
GList *filesel_select_multiple_files_open_with_filter(	const gchar *title,
							const gchar *path,
							const gchar *filter);

#endif /* __FILESEL_H__ */
