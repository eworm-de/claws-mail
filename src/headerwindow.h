/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999,2000 Hiroyuki Yamamoto
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

#ifndef __HEADERWINDOW_H__
#define __HEADERWINDOW_H__

#include <glib.h>
#include <gtk/gtkwidget.h>

typedef struct _HeaderWindow	HeaderWindow;

#include "procmsg.h"

struct _HeaderWindow
{
	GtkWidget *window;
	GtkWidget *scrolledwin;
	GtkWidget *text;
};

HeaderWindow *header_window_create(void);
void header_window_init(HeaderWindow *headerwin);
void header_window_show(HeaderWindow *headerwin, MsgInfo *msginfo);
void header_window_show_cb(gpointer data, guint action, GtkWidget *widget);

#endif /* __HEADERWINDOW_H__ */
