/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2002 Hiroyuki Yamamoto
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

#ifndef __SELECTIVE_DOWNLOAD_H__
#define __SELECTIVE_DOWNLOAD_H__

#include "mainwindow.h"

typedef struct _HeaderItems HeaderItems;

typedef enum {
	SD_CHECKED,      /* checkbox selected for action */ 
	SD_UNCHECKED,    /* checkbox untouched */
	SD_REMOVE,       /* ask server to delete msg */
	SD_REMOVED,      /* msg successfully removed from server */
	SD_DOWNLOAD,     /* ask to download msg */
	SD_DOWNLOADED,   /* msg successfully received + removed from server */
} SD_State;

struct _HeaderItems {
	gint        index;     /* msg reference number on server */
	SD_State    state;
	gchar       *from;
	gchar       *subject;
	gchar       *date;
	gint        size;
	guint       received : 1;
	guint       del_by_old_session : 1;
};

void selective_download(MainWindow *mainwin);

#endif /* __SELECTIVE_DOWNLOAD_H__ */
