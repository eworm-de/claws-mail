/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2001 Hiroyuki Yamamoto & The Sylpheed Claws Team
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

#ifndef MBOX_FOLDER_H

#define MBOX_FOLDER_H

#include <glib.h>
#include "folder.h"

/*
mailfile mailfile_init(char * filename);
char * mailfile_readmsg(mailfile f, int index);
char * mailfile_readheader(mailfile f, int index);
void mailfile_done(mailfile f);
int mailfile_count(mailfile f);
*/
typedef struct _MBOXFolder	MBOXFolder;

#define MBOX_FOLDER(obj)	((MBOXFolder *)obj)

struct _MBOXFolder
{
	LocalFolder lfolder;
};

FolderClass *mbox_get_class	(void);
gchar * mbox_get_virtual_path(FolderItem * item);

#endif
