/*
 * mailmbox Plugin -- mbox support for Sylpheed
 * Copyright (C) 2003-2005 Christoph Hohmann, 
 *			   Hoa v. Dinh, 
 *			   Alfons Hoogervorst
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __PLUGIN_GTK_H__
#define __PLUGIN_GTK_H__

#include <glib.h>

gint plugin_gtk_init(gchar **error);
void plugin_gtk_done(void);

#endif
