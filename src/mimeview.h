/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2007 Hiroyuki Yamamoto and the Claws Mail team
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

#ifndef MIMEVIEW_H
#define MIMEVIEW_H

typedef struct _MimeView		MimeView;
typedef struct _MimeViewerFactory 	MimeViewerFactory;
typedef struct _MimeViewer 		MimeViewer;

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkctree.h>
#include <gtk/gtktooltips.h>
#ifdef USE_PTHREAD
#include <pthread.h>
#endif

#include "textview.h"
#include "messageview.h"
#include "procmime.h"
#include "noticeview.h"

typedef enum
{
	MIMEVIEW_TEXT,
	MIMEVIEW_VIEWER
} MimeViewType;

#ifdef USE_PTHREAD
typedef struct _SigCheckData SigCheckData;
struct _SigCheckData
{
	pthread_t th;
	MimeInfo *siginfo;
	gboolean free_after_use;
	gboolean destroy_mimeview;
	gboolean timeout;
};
#endif

struct _MimeView
{
	GtkWidget *hbox;
	GtkWidget *paned;
	GtkWidget *scrolledwin;
	GtkWidget *ctree;
	GtkWidget *mime_notebook;
	GtkWidget *ctree_mainbox;
	GtkWidget *icon_scroll;
	GtkWidget *icon_vbox;
	GtkWidget *icon_mainbox;
	GtkWidget *mime_toggle;
	GtkWidget *scrollbutton;
	MimeViewType type;

	GtkWidget *popupmenu;
	GtkItemFactory *popupfactory;

	GtkCTreeNode *opened;

	TextView *textview;
	MimeViewer *mimeviewer;

	MessageView *messageview;

	MimeInfo *mimeinfo;

	gchar *file;

	GSList *viewers;

	GtkTargetList *target_list; /* DnD */

	gint icon_count;
	MainWindow *mainwin;
	GtkTooltips *tooltips;
	gint oldsize;

	NoticeView *siginfoview;
	MimeInfo *siginfo;
#ifdef USE_PTHREAD
	SigCheckData *check_data;
#endif
	MimeInfo *spec_part;
};

struct _MimeViewerFactory
{
	/**
         * Array of supported content types.
	 * Must be NULL terminated and lower case
	 */
	gchar **content_types;
	gint priority;
	
	MimeViewer *(*create_viewer) (void);
};

struct _MimeViewer
{
	MimeViewerFactory *factory;
	
	GtkWidget 	*(*get_widget)		(MimeViewer *);
	void 	 	(*show_mimepart)	(MimeViewer *, const gchar *infile, MimeInfo *);
	void		(*clear_viewer)		(MimeViewer *);
	void		(*destroy_viewer)	(MimeViewer *);
	gchar 		*(*get_selection)	(MimeViewer *);
	gboolean	(*scroll_page)		(MimeViewer *, gboolean up);
	void		(*scroll_one_line)	(MimeViewer *, gboolean up);
	MimeView	*mimeview;
};

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


MimeView *mimeview_create	(MainWindow	*mainwin);
void mimeview_init		(MimeView	*mimeview);
void mimeview_show_message	(MimeView	*mimeview,
				 MimeInfo	*mimeinfo,
				 const gchar	*file);
gboolean mimeview_show_part	(MimeView 	*mimeview, 
				 MimeInfo 	*partinfo);
void mimeview_destroy		(MimeView	*mimeview);
void mimeview_update		(MimeView 	*mimeview);
void mimeview_clear		(MimeView	*mimeview);

MimeInfo *mimeview_get_selected_part	(MimeView	*mimeview);

void mimeview_check_signature		(MimeView 	*mimeview);
void mimeview_pass_key_press_event	(MimeView	*mimeview,
					 GdkEventKey	*event);

void mimeview_register_viewer_factory	(MimeViewerFactory *factory);
void mimeview_unregister_viewer_factory	(MimeViewerFactory *factory);
void mimeview_handle_cmd		(MimeView 	*mimeview, 
					 const gchar 	*cmd,
					 GdkEventButton *event,
					 gpointer	 data);
void mimeview_select_mimepart_icon	(MimeView 	*mimeview, 
					 MimeInfo 	*partinfo);
gboolean mimeview_scroll_page		(MimeView 	*mimeview, 
					 gboolean 	 up);
void mimeview_scroll_one_line		(MimeView 	*mimeview, 
					 gboolean 	 up);
gint mimeview_get_selected_part_num	(MimeView 	*mimeview);
void mimeview_select_part_num		(MimeView 	*mimeview, 
					 gint 		 i);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __MIMEVIEW_H__ */
