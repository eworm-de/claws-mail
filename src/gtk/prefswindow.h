/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2012 Hiroyuki Yamamoto and the Claws Mail Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 * 
 */

#ifndef PREFSWINDOW_H
#define PREFSWINDOW_H 1

#include <glib.h>
#include <gtk/gtk.h>

typedef struct _PrefsPage PrefsPage;
typedef struct _PrefsWindow PrefsWindow;

typedef void (*PrefsCreateWidgetFunc) (PrefsPage *, GtkWindow *window, gpointer);
typedef void (*PrefsDestroyWidgetFunc) (PrefsPage *);
typedef void (*PrefsSavePageFunc) (PrefsPage *);
typedef gboolean (*PrefsCanClosePageFunc) (PrefsPage *);
typedef void (*PrefsOpenCallbackFunc) (PrefsWindow *);
typedef void (*PrefsApplyCallbackFunc) (PrefsWindow *);
typedef void (*PrefsCloseCallbackFunc) (PrefsWindow *);

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

enum {
	PREFS_PAGE_TITLE,		/* page title */
	PREFS_PAGE_DATA,		/* PrefsTreeNode data */
	PREFS_PAGE_DATA_AUTO_FREE,	/* auto free for PREFS_PAGE_DATA */
	PREFS_PAGE_WEIGHT,		/* weight */
	PREFS_PAGE_INDEX,		/* index in original page list */
	N_PREFS_PAGE_COLUMNS
};

typedef struct _PrefsTreeNode PrefsTreeNode;

struct _PrefsWindow
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *paned;
	GtkWidget *scrolledwindow1;
	GtkWidget *tree_view;
	GtkWidget *table2;
	GtkWidget *pagelabel;
	GtkWidget *labelframe;
	GtkWidget *vbox2;
	GtkWidget *notebook;
	GtkWidget *confirm_area;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;
	GtkWidget *apply_btn;
	gint *save_width;
	gint *save_height;
	PrefsCloseCallbackFunc open_cb;
	PrefsApplyCallbackFunc apply_cb;
	PrefsCloseCallbackFunc close_cb;
	gint dialog_response; /* Useful for close_cb callbacks */

	GtkWidget *empty_page;

	gpointer   	 data;
	GSList	  	*prefs_pages;
	GDestroyNotify func;
};

enum
{
	PREFSWINDOW_RESPONSE_CANCEL,
	PREFSWINDOW_RESPONSE_OK,
	PREFSWINDOW_RESPONSE_APPLY
};

struct _PrefsTreeNode
{
	PrefsPage *page;
	gfloat     treeweight; /* GTK2: not used */
};

void prefswindow_open_full		(const gchar *title, 
					 GSList *prefs_pages,
					 gpointer data,
					 GDestroyNotify func,
					 gint *save_width, gint *save_height,
					 gboolean preload_pages,
					 PrefsOpenCallbackFunc open_cb,
					 PrefsApplyCallbackFunc apply_cb,
					 PrefsCloseCallbackFunc close_cb);

void prefswindow_open			(const gchar *title, 
					 GSList *prefs_pages,
					 gpointer data,
					 gint *save_width, gint *save_height,
					 PrefsOpenCallbackFunc open_cb,
					 PrefsApplyCallbackFunc apply_cb,
					 PrefsCloseCallbackFunc close_cb);

#endif
