/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2002 Hiroyuki Yamamoto and the Sylpheed-Claws Team
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

#ifndef PREFSWINDOW_H
#define PREFSWINDOW_H 1

#include <glib.h>
#include <gtk/gtk.h>

typedef struct _PrefsPage PrefsPage;

typedef void (*PrefsCreateWidgetFunc) (PrefsPage *, GtkWindow *window, gpointer);
typedef void (*PrefsDestroyWidgetFunc) (PrefsPage *);
typedef void (*PrefsSavePageFunc) (PrefsPage *);
typedef gboolean (*PrefsCanClosePageFunc) (PrefsPage *);

struct _PrefsPage
{
	gchar 			**path;
	gboolean		  page_open;
	GtkWidget		 *widget;
	gfloat			  weight;

	PrefsCreateWidgetFunc	  create_widget;
	PrefsDestroyWidgetFunc	  destroy_widget;
	PrefsSavePageFunc	  save_page;
	PrefsCanClosePageFunc	  can_close;
};

void prefswindow_open_full		(const gchar *title, 
					 GSList *prefs_pages,
					 gpointer data,
					 GtkDestroyNotify func);

void prefswindow_open			(const gchar *title, 
					 GSList *prefs_pages,
					 gpointer data);

#endif
