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

#ifndef __MIMEVIEW_H__
#define __MIMEVIEW_H__

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkctree.h>

typedef struct _MimeView		MimeView;
typedef struct _MimeViewerFactory 	MimeViewerFactory;
typedef struct _MimeViewer 		MimeViewer;

#include "textview.h"
#include "imageview.h"
#include "messageview.h"
#include "procmime.h"

typedef enum
{
	MIMEVIEW_TEXT,
	MIMEVIEW_IMAGE,
	MIMEVIEW_VIEWER,
} MimeViewType;

struct _MimeView
{
	GtkWidget *notebook;
	GtkWidget *vbox;

	GtkWidget *paned;
	GtkWidget *scrolledwin;
	GtkWidget *ctree;
	GtkWidget *mime_vbox;

	MimeViewType type;

	GtkWidget *popupmenu;
	GtkItemFactory *popupfactory;

	GtkCTreeNode *opened;

	TextView *textview;
	ImageView *imageview;
	MimeViewer *mimeviewer;

	MessageView *messageview;

	MimeInfo *mimeinfo;

	gchar *file;

	GSList *viewers;
};

struct _MimeViewerFactory
{
	gchar *content_type;
	gint priority;
	
	MimeViewer *(*create_viewer) ();
};

struct _MimeViewer
{
	MimeViewerFactory *factory;
    
	GtkWidget 	*(*get_widget)		(MimeViewer *);
	void 	 	(*show_mimepart)	(MimeViewer *, const gchar *infile, MimeInfo *);
	void		(*clear_viewer)		(MimeViewer *);
	void		(*destroy_viewer)	(MimeViewer *);
};

MimeView *mimeview_create	(void);
void mimeview_init		(MimeView	*mimeview);
void mimeview_show_message	(MimeView	*mimeview,
				 MimeInfo	*mimeinfo,
				 const gchar	*file);
void mimeview_destroy		(MimeView	*mimeview);

#if USE_GPGME
void mimeview_check_signature	(MimeView 	*mimeview);
#endif
void mimeview_pass_key_press_event	(MimeView	*mimeview,
					 GdkEventKey	*event);

void mimeview_register_viewer_factory	(MimeViewerFactory *factory);
void mimeview_unregister_viewer_factory	(MimeViewerFactory *factory);

#endif /* __MIMEVIEW_H__ */
