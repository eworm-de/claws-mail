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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __IMAP_H__
#define __IMAP_H__

#include "folder.h"

typedef enum
{
	IMAP_AUTH_LOGIN		= 1 << 0,
	IMAP_AUTH_CRAM_MD5	= 1 << 1
} IMAPAuthType;

FolderClass *imap_get_class		(void);
guint imap_folder_get_refcnt(Folder *folder);
void imap_folder_ref(Folder *folder);
void imap_folder_unref(Folder *folder);

#endif /* __IMAP_H__ */
