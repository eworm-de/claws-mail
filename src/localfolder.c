/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2002 by the Sylpheed Claws Team and Hiroyuki Yamamoto
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

#include <glib.h>

#include "folder.h"
#include "localfolder.h"
#include "xml.h"
#ifdef WIN32
#include "utils.h"
#endif

void folder_local_folder_init(Folder *folder, const gchar *name,
			      const gchar *path)
{
	folder_init(folder, name);
	LOCAL_FOLDER(folder)->rootpath = g_strdup(path);
}

void folder_local_folder_destroy(LocalFolder *lfolder)
{
	g_return_if_fail(lfolder != NULL);

	g_free(lfolder->rootpath);
}

void folder_local_set_xml(Folder *_folder, XMLTag *tag)
{
	LocalFolder *folder = LOCAL_FOLDER(_folder);
	GList *cur;

	folder_set_xml(_folder, tag);

	for (cur = tag->attr; cur != NULL; cur = g_list_next(cur)) {
		XMLAttr *attr = (XMLAttr *) cur->data;

		if (!attr || !attr->name || !attr->value) continue;
		if (!strcmp(attr->name, "path")) {
#ifdef WIN32
			gchar *path = g_strdup(attr->value);
#endif
			if (folder->rootpath != NULL)
				g_free(folder->rootpath);
#ifdef WIN32
			subst_char(path, G_DIR_SEPARATOR, '/');
			folder->rootpath = path;
#else
			folder->rootpath = g_strdup(attr->value);
#endif
		}
	}
}

XMLTag *folder_local_get_xml(Folder *_folder)
{
	LocalFolder *folder = LOCAL_FOLDER(_folder);
	XMLTag *tag;
#ifdef WIN32
	gchar *path = g_strdup(folder->rootpath);
#endif

	tag = folder_get_xml(_folder);

#ifdef WIN32
	if (path)
		subst_char(path, G_DIR_SEPARATOR, '/');
	xml_tag_add_attr(tag, "path", path);
#else
	xml_tag_add_attr(tag, "path", g_strdup(folder->rootpath));
#endif

	return tag;
}
