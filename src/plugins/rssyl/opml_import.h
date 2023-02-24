/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2005-2023 the Claws Mail Team and Andrej Kacian <andrej@kacian.sk>
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __OPML_IMPORT
#define __OPML_IMPORT

struct _OPMLImportCtx {
	GSList *current;
	gint depth;
	gint failures;
};

typedef struct _OPMLImportCtx OPMLImportCtx;

gint rssyl_folder_depth(FolderItem *item);
void rssyl_opml_import_func(gchar *title, gchar *url, gint depth, gpointer data);

#endif /* __OPML_IMPORT */
