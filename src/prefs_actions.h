/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2002 Hiroyuki Yamamoto & The Sylpheed Claws Team
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

#ifndef __ACTIONS_H__
#define __ACTIONS_H__

#include "mainwindow.h"

void prefs_actions_read_config		(void);
void prefs_actions_write_config		(void);
void update_mainwin_actions_menu	(GtkItemFactory	*ifactory, 
					 MainWindow	*mainwin);
void update_compose_actions_menu	(GtkItemFactory	*ifactory, 
					 gchar		*branch_path,
					 Compose	*compose);
void prefs_actions_open			(MainWindow	*mainwin);

void actions_execute                    (gpointer       data, 
					 guint          action_nb,
					 GtkWidget      *widget,
					 gint           source);

#endif /* __ACTIONS_H__ */
