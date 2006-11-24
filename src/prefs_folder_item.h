/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2006 Hiroyuki Yamamoto and the Claws Mail team
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

#ifndef PREFS_FOLDER_ITEM_H
#define PREFS_FOLDER_ITEM_H

#include <glib.h>
#include <sys/types.h>

#include "folder.h"
#include "folderview.h"
#include "folder_item_prefs.h"
#include "prefswindow.h"

void prefs_folder_item_create(FolderView *folderview, FolderItem *item); 

void prefs_folder_item_open		(FolderItem 	*item);
void prefs_folder_item_register_page	(PrefsPage 	*page);
void prefs_folder_item_unregister_page	(PrefsPage 	*page);

#endif /* PREFS_FOLDER_ITEM_H */
