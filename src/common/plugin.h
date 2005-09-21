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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef PLUGIN_H
#define PLUGIN_H 1

#include <glib.h>

typedef struct _Plugin Plugin;

/* Functions to implement by the plugin */
gint plugin_init		(gchar		**error);
void plugin_done		(void);
const gchar *plugin_name	(void);
const gchar *plugin_desc	(void);

/* Functions by the sylpheed plugin system */
Plugin *plugin_load		(const gchar	 *filename,
				 gchar		**error);
void plugin_unload		(Plugin		 *plugin);
void plugin_load_all		(const gchar	 *type);
void plugin_unload_all		(const gchar	 *type);
void plugin_save_list		(void);

GSList *plugin_get_list		(void);
const gchar *plugin_get_name	(Plugin		 *plugin);
const gchar *plugin_get_desc	(Plugin		 *plugin);

#endif
