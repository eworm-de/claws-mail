/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999,2000 Hiroyuki Yamamoto and the Sylpheed-Claws team
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

#ifndef __MBOX_H__
#define __MBOX_H__

#include <glib.h>

#include "folder.h"

typedef enum {
	LOCK_FILE,
	LOCK_FLOCK
} LockType;


gint proc_mbox		(FolderItem	*dest,
			 const gchar	*mbox,
			 gboolean	 apply_filter,
			 PrefsAccount	*account);
gint lock_mbox		(const gchar	*base,
			 LockType	 type);
gint unlock_mbox	(const gchar	*base,
			 gint		 fd,
			 LockType	 type);
gint copy_mbox		(const gchar	*src,
			 const gchar	*dest);
void empty_mbox		(const gchar	*mbox);

gint export_to_mbox	(FolderItem	*src,
			 const gchar	*mbox);
gint export_list_to_mbox(GSList 	*mlist, 
			 const gchar 	*mbox);

#endif /* __MBOX_H__ */
