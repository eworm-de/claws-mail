/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2007 Hiroyuki Yamamoto and the Claws Mail team
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

#ifndef HOOKS_H
#define HOOKS_H

#include <glib.h>

typedef gboolean (*SylpheedHookFunction)	(gpointer source,
						 gpointer userdata);

guint hooks_register_hook	(const gchar		*hooklist_name,
				 SylpheedHookFunction	 hook_func,
				 gpointer		 userdata);
void hooks_unregister_hook	(const gchar		*hooklist_name,
				 guint			 hook_id);
gboolean hooks_invoke		(const gchar		*hooklist_name,
				 gpointer		 source);

#endif /* HOOKS_H */
