/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2016 Salvatore De Paolis & the Claws Mail Team
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
 */

#ifndef POPPLER_VIEWER_H
#define POPPLER_VIEWER_H

//#include <unistd.h>
#include <stdio.h>
#include <gtk/gtk.h>
#ifndef G_OS_WIN32
#include <gdk/gdkx.h>
#endif
#include <gdk/gdkkeysyms.h>
#include <poppler.h>
#include <version.h>
#include <plugin.h>
#include <messageview.h>
#include <mimeview.h>
#include <alertpanel.h>

/*#ifdef USE_PTHREAD
 *#include <pthread.h>
 * #endif*/

#define ZOOM_FACTOR 0.25
#define ROTATION 90 
#define ALPHA_CHANNEL 60
#define SELECTION_COLOR 0xFF00FF

static gchar *msg = NULL;

struct _PageResult
{
	GList *results;
	gint page_num;
};

typedef struct _PageResult PageResult;

struct _PdfViewer
{
	MimeViewer			mimeviewer;
	GtkWidget			*widgets_table;
	GtkWidget			*vbox;
	GtkWidget			*hbox;
	GtkWidget			*frame_index;
	GtkWidget			*pdf_view;
	GtkWidget			*scrollwin;
	GtkWidget			*scrollwin_index;
	GtkWidget			*pdf_view_ebox;
	GtkWidget			*icon_type_ebox;
	GtkWidget			*icon_type;
	GdkPixbuf			*icon_pixbuf;
	GtkWidget			*doc_label;
	GtkWidget			*cur_page;
	GtkWidget			*doc_index_pane;
	GtkWidget			*index_list;
	/* begin GtkButtons */
	GtkWidget			*first_page;
	GtkWidget			*last_page;
	GtkWidget			*prev_page;
	GtkWidget			*next_page;
	GtkWidget			*zoom_in;
	GtkWidget			*zoom_out;
	GtkWidget			*zoom_scroll;
	GtkWidget			*zoom_fit;
	GtkWidget			*zoom_width;
	GtkWidget			*rotate_left;
	GtkWidget			*rotate_right;
	GtkWidget			*print;
	GtkWidget			*doc_info;
	GtkWidget			*doc_index;
	/* end GtkButtons */
	GtkWidget			*table_doc_info;

	PopplerDocument		*pdf_doc;
	PopplerPage			*pdf_page;
	PopplerIndexIter	*pdf_index;
	PopplerRectangle	*last_rect;
	PopplerAction		*link_action;
	PageResult			*last_page_result;
	GtkAdjustment		*pdf_view_vadj;
	GtkAdjustment		*pdf_view_hadj;
	GtkTreeModel		*index_model;

	GList				*link_map;
	GList				*page_results;
	GList				*text_found; /* GList of PageResults */
	gchar				*last_search;
	gint				 last_match;
	gint				 num_matches;

	gchar				*target_filename;
	gchar				*filename;
	gchar				*fsname;
	gchar				*doc_info_text;

	gint				res_cnt;
	gint				rotate;
	gint				num_pages;
	gdouble				zoom;
	gdouble				width;
	gdouble				height;
	gdouble				last_x;
	gdouble				last_y;
	gint				last_dir_x;
	gint				last_dir_y;
	gboolean			pdf_view_scroll;
	gboolean			in_link;
	MimeInfo			*mimeinfo;
	MimeInfo			*to_load;
	
};
static gchar *content_types[] =
	{"application/pdf", 
	 "application/postscript", 
	 NULL};
typedef enum {
	TYPE_UNKNOWN,
	TYPE_PDF,
	TYPE_PS
} FileType;

enum {
	INDEX_NAME,
	INDEX_PAGE,
	INDEX_TOP,
	N_INDEX_COLUMNS
};

typedef struct _PdfViewer PdfViewer;

#endif /* POPPLER_VIEWER_H */
