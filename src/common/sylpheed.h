/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999,2000 Hiroyuki Yamamoto
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

#ifndef SYLPHEED_H
#define SYLPHEED_H

#include <glib.h>

gboolean sylpheed_init			(int *argc, char ***argv);
void sylpheed_done			(void);
const gchar *sylpheed_get_startup_dir	(void);
guint sylpheed_get_version		(void);
void sylpheed_register_idle_function	(void (*idle_func)(void));
void sylpheed_do_idle			(void);

#endif /* SYLPHEED_H */
