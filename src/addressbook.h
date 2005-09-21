/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2003 Hiroyuki Yamamoto
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

#ifndef __ADDRESSBOOK_H__
#define __ADDRESSBOOK_H__

#include <glib.h>
#include <gtk/gtkwidget.h>

#include "compose.h"
#include "addritem.h"

void addressbook_open			( Compose *target );
void addressbook_set_target_compose	( Compose *target );
Compose *addressbook_get_target_compose	( void );
void addressbook_read_file		( void );
void addressbook_export_to_file		( void );
gint addressbook_obj_name_compare	( gconstpointer a,
					  gconstpointer b );
void addressbook_destroy		( void );

gboolean addressbook_add_contact	( const gchar *name,
					  const gchar *address,
					  const gchar *remarks );
					  
gboolean addressbook_load_completion	(gint (*callBackFunc) 
					       (const gchar *, 
					  	const gchar *, 
					  	const gchar *));

void addressbook_gather			( FolderItem *folderItem,
					  gboolean sourceInd,
					  GList *msgList );
void addressbook_harvest		( FolderItem *folderItem,
					  gboolean sourceInd,
					  GList *msgList);

void addressbook_read_all		( void );

#endif /* __ADDRESSBOOK_H__ */

