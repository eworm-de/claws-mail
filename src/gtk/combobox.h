/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2006 Andrej Kacian and the Claws Mail team
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

#ifndef __COMBOBOX_H__
#define __COMBOBOX_H__

#include <glib.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkcombobox.h>

#define COMBOBOX_ADD(menu, label, data) 		 \
{ 								 \
	gtk_list_store_append(menu, &iter); \
	gtk_list_store_set(menu, &iter, \
			0, (label ? label : ""), \
			1, data, \
			-1); \
}

void combobox_select_by_data	(GtkComboBox 		*combobox,
				 gint		 data);

gint combobox_get_active_data	(GtkComboBox 		*combobox);

void combobox_unset_popdown_strings(GtkComboBox		*combobox);
void combobox_set_popdown_strings(GtkComboBox		*combobox,
				 GList       *list);

#endif /* __COMBOBOX_H__ */
