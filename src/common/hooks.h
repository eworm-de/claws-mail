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

#ifndef HOOKS_H
#define HOOKS_H

typedef gboolean (*SylpheedHookFunction)	(gpointer source,
						 gpointer userdata);

gint hooks_register_hook	(gchar			*hooklist_name,
				 SylpheedHookFunction	 hook_func,
				 gpointer		 userdata);
void hooks_unregister_hook	(gchar			*hooklist_name,
				 guint			 hook_id);
void hooks_invoke		(gchar			*hooklist_name,
				 gpointer		 source);

#endif /* HOOKS_H */
