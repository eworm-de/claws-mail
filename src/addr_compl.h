/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * 
 * Copyright (C) 2000-2004 by Alfons Hoogervorst & The Sylpheed Claws Team. 
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
 
#ifndef __ADDR_COMPL_H__
#define __ADDR_COMPL_H__

#include <gtk/gtk.h>

gint start_address_completion		(void);
guint complete_address			(const gchar *str);
gchar *get_complete_address		(gint index);
gint invalidate_address_completion	(void);
gint end_address_completion		(void);

/* ui functions */
void address_completion_start		(GtkWidget *mainwindow);
void address_completion_register_entry	(GtkEntry  *entry);
void address_completion_unregister_entry(GtkEntry  *entry);
void address_completion_end		(GtkWidget *mainwindow);

void addrcompl_initialize	( void );
void addrcompl_teardown		( void );

#endif /* __ADDR_COMPL_H__ */
