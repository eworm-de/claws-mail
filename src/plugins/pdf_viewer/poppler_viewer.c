/*
 * Claws Mail -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright(C) 1999-2007 the Claws Mail Team
 * This file Copyright (C) 2007 Salvatore De Paolis 
 * <iwkse@claws-mail.org> 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 *(at your option) any later version.
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>

#include "poppler_viewer.h"
#include "printing.h"
#include "prefs_common.h"
#include "gtk/gtkutils.h"
#include "mimeview.h"
#ifndef POPPLER_WITH_GDK
#include "stdbool.h"
#endif

static FileType pdf_viewer_mimepart_get_type(MimeInfo *partinfo);
static MimeViewerFactory pdf_viewer_factory;

static void pdf_viewer_show_mimepart(MimeViewer *_viewer, const gchar *infile,
				MimeInfo *partinfo);

static MimeViewer *pdf_viewer_create(void);
static void pdf_viewer_clear(MimeViewer *_viewer);
static void pdf_viewer_destroy(MimeViewer *_viewer);
static void pdf_viewer_update(MimeViewer *_viewer, gboolean reload_file, int page_num);

static GtkWidget *pdf_viewer_get_widget(MimeViewer *_viewer);

static void pdf_viewer_hide_index_pane(PdfViewer *viewer);
static void pdf_viewer_set_index_button_sensitive(PdfViewer *viewer);
static void pdf_viewer_scroll_to(PdfViewer *viewer, gfloat x, gfloat y);

static void search_matches_free(PdfViewer *viewer);
static gboolean	pdf_viewer_text_search(MimeViewer *_viewer, gboolean backward,
				     const gchar *str, gboolean case_sens);
static void pdf_viewer_render_selection(PdfViewer *viewer, PopplerRectangle *rect, PageResult *page_results);
static void pdf_viewer_render_page(PopplerPage *page, GtkWidget *view, double width, double height, double zoom, gint rotate);

static char * pdf_viewer_get_document_format_data(GTime utime);
static void pdf_viewer_get_document_index(PdfViewer *viewer, PopplerIndexIter *index_iter, GtkTreeIter *parentiter);
static void pdf_viewer_index_row_activated(GtkTreeView		*tree_view,
				   	GtkTreePath		*path,
				   	GtkTreeViewColumn	*column,
				   	gpointer		 data);

static GtkTable * pdf_viewer_fill_info_table(PdfViewer *viewer);

/* Callbacks */
static void pdf_viewer_move_events_cb(GtkWidget *widget, GdkEventMotion *event, PdfViewer *viewer); 
static void pdf_viewer_button_press_events_cb(GtkWidget *widget, GdkEventButton *event, PdfViewer *viewer); 
static void pdf_viewer_mouse_scroll_destroy_cb(GtkWidget *widget, GdkEventButton *event, PdfViewer *viewer); 
static void pdf_viewer_button_first_page_cb(GtkButton *button, PdfViewer *viewer);
static void pdf_viewer_button_last_page_cb(GtkButton *button, PdfViewer *viewer);
static void pdf_viewer_button_zoom_in_cb(GtkButton *button, PdfViewer *viewer);
static void pdf_viewer_button_zoom_out_cb(GtkButton *button, PdfViewer *viewer);
static void pdf_viewer_button_zoom_fit_cb(GtkButton *button, PdfViewer *viewer);
static void pdf_viewer_button_zoom_width_cb(GtkButton *button, PdfViewer *viewer);
static void pdf_viewer_button_rotate_right_cb(GtkButton *button, PdfViewer *viewer);
static void pdf_viewer_button_rotate_left_cb(GtkButton *button, PdfViewer *viewer);
static void pdf_viewer_spin_change_page_cb(GtkSpinButton *button, PdfViewer *viewer);
static void pdf_viewer_spin_zoom_scroll_cb(GtkSpinButton *button, PdfViewer *viewer);
/* Show/Hide the index pane */
static void pdf_viewer_show_document_index_cb(GtkButton *button, PdfViewer *viewer);
static void pdf_viewer_button_document_info_cb(GtkButton *button, PdfViewer *viewer);

static void pdf_viewer_show_controls(PdfViewer *viewer, gboolean show);
static gboolean pdf_viewer_scroll_page(MimeViewer *_viewer, gboolean up);
static void pdf_viewer_scroll_one_line(MimeViewer *_viewer, gboolean up);

/** Claws-Mail Plugin functions*/
gint plugin_init(gchar **error);
const gchar *plugin_name(void);
const gchar *plugin_desc(void);
const gchar *plugin_type(void);
const gchar *plugin_licence(void);
const gchar *plugin_version(void);
struct PluginFeature *plugin_provides(void);

#ifndef POPPLER_WITH_GDK
static void
copy_cairo_surface_to_pixbuf (cairo_surface_t *surface,
			      GdkPixbuf       *pixbuf)
{
	int cairo_width, cairo_height, cairo_rowstride;
	unsigned char *pixbuf_data, *dst, *cairo_data;
	int pixbuf_rowstride, pixbuf_n_channels;
	unsigned int *src;
	int x, y;

	cairo_width = cairo_image_surface_get_width (surface);
	cairo_height = cairo_image_surface_get_height (surface);
	cairo_rowstride = cairo_image_surface_get_stride (surface);
	cairo_data = cairo_image_surface_get_data (surface);

	pixbuf_data = gdk_pixbuf_get_pixels (pixbuf);
	pixbuf_rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	pixbuf_n_channels = gdk_pixbuf_get_n_channels (pixbuf);

	if (cairo_width > gdk_pixbuf_get_width (pixbuf))
		cairo_width = gdk_pixbuf_get_width (pixbuf);
	if (cairo_height > gdk_pixbuf_get_height (pixbuf))
		cairo_height = gdk_pixbuf_get_height (pixbuf);
	for (y = 0; y < cairo_height; y++) {
		src = (unsigned int *) (cairo_data + y * cairo_rowstride);
		dst = pixbuf_data + y * pixbuf_rowstride;
		for (x = 0; x < cairo_width; x++) {
			dst[0] = (*src >> 16) & 0xff;
			dst[1] = (*src >> 8) & 0xff; 
			dst[2] = (*src >> 0) & 0xff;
			if (pixbuf_n_channels == 4)
				dst[3] = (*src >> 24) & 0xff;
			dst += pixbuf_n_channels;
			src++;
		}
	}
}
static void
_poppler_page_render_to_pixbuf (PopplerPage *page,
				int src_x, int src_y,
				int src_width, int src_height,
				double scale,
				int rotation,
				gboolean printing,
				GdkPixbuf *pixbuf)
{
	cairo_t *cr;
	cairo_surface_t *surface;

	surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
					src_width, src_height);
	cr = cairo_create (surface);
	cairo_save (cr);
	switch (rotation) {
	case 90:
		cairo_translate (cr, src_x + src_width, -src_y);
		break;
	case 180:
		cairo_translate (cr, src_x + src_width, src_y + src_height);
		break;
	case 270:
		cairo_translate (cr, -src_x, src_y + src_height);
		break;
	default:
		cairo_translate (cr, -src_x, -src_y);
	}

	if (scale != 1.0)
		cairo_scale (cr, scale, scale);

	if (rotation != 0)
		cairo_rotate (cr, rotation * G_PI / 180.0);

	if (printing)
		poppler_page_render_for_printing (page, cr);
	else
		poppler_page_render (page, cr);
	cairo_restore (cr);

	cairo_set_operator (cr, CAIRO_OPERATOR_DEST_OVER);
	cairo_set_source_rgb (cr, 1., 1., 1.);
	cairo_paint (cr);

	cairo_destroy (cr);

	copy_cairo_surface_to_pixbuf (surface, pixbuf);
	cairo_surface_destroy (surface);
}

/**
 * poppler_page_render_to_pixbuf:
 * @page: the page to render from
 * @src_x: x coordinate of upper left corner  
 * @src_y: y coordinate of upper left corner  
 * @src_width: width of rectangle to render  
 * @src_height: height of rectangle to render
 * @scale: scale specified as pixels per point
 * @rotation: rotate the document by the specified degree
 * @pixbuf: pixbuf to render into
 *
 * First scale the document to match the specified pixels per point,
 * then render the rectangle given by the upper left corner at
 * (src_x, src_y) and src_width and src_height.
 * This function is for rendering a page that will be displayed.
 * If you want to render a page that will be printed use
 * poppler_page_render_to_pixbuf_for_printing() instead
 *
 * Deprecated: 0.16
 **/
static void
poppler_page_render_to_pixbuf (PopplerPage *page,
			       int src_x, int src_y,
			       int src_width, int src_height,
			       double scale,
			       int rotation,
			       GdkPixbuf *pixbuf)
{
	g_return_if_fail (POPPLER_IS_PAGE (page));
	g_return_if_fail (scale > 0.0);
	g_return_if_fail (pixbuf != NULL);

	_poppler_page_render_to_pixbuf (page, src_x, src_y,
				  src_width, src_height,
				  scale, rotation,
				  FALSE,
				  pixbuf);
}
#endif
static GtkWidget *pdf_viewer_get_widget(MimeViewer *_viewer)
{
	PdfViewer *viewer = (PdfViewer *) _viewer;
	debug_print("pdf_viewer_get_widget: %p\n", viewer->vbox);

	return GTK_WIDGET(viewer->vbox);
}
/** Hide the index panel */
static void pdf_viewer_hide_index_pane(PdfViewer *viewer)
{
	if (viewer->pdf_index) {   
		poppler_index_iter_free(viewer->pdf_index);
		viewer->pdf_index = NULL;
		gtk_widget_hide(GTK_WIDGET(viewer->frame_index));
	}
}

static void search_matches_free(PdfViewer *viewer)
{
	GList *cur; 
	for(cur = viewer->text_found; cur; cur = cur->next) {
		PageResult *res = (PageResult *)cur->data;
		g_list_free(res->results);
		g_free(res);
	}
	g_list_free(viewer->text_found);
	viewer->text_found = NULL;
	g_free(viewer->last_search);
	viewer->last_search = NULL;
	if (viewer->last_rect && viewer->last_page_result) {
		viewer->last_rect = NULL;
		viewer->last_page_result = NULL;
		pdf_viewer_update((MimeViewer *)viewer, 
			FALSE, 
			gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(viewer->cur_page)));
	}
}

static void pdf_viewer_scroll_to(PdfViewer *viewer, gfloat x, gfloat y)
{
	GtkAdjustment *vadj;
	GtkAdjustment *hadj;
	vadj = gtk_scrolled_window_get_vadjustment(
		GTK_SCROLLED_WINDOW(viewer->scrollwin));
	
	if (y < vadj->value) {
		vadj->value = y;
	}
	else {
		while(y > vadj->value + vadj->page_size) {
			vadj->value += vadj->page_size;
		}
	}
	
	hadj = gtk_scrolled_window_get_hadjustment(
		GTK_SCROLLED_WINDOW(viewer->scrollwin));
	
	if (x < hadj->value) {
		hadj->value = x;
	}
	else {
		while(x > hadj->value + hadj->page_size) {
			hadj->value += hadj->page_size;
		}
	}

	g_signal_emit_by_name(G_OBJECT(hadj), "value-changed", 0);	
	g_signal_emit_by_name(G_OBJECT(vadj), "value-changed", 0);	
}
static void pdf_viewer_render_page(PopplerPage *page, GtkWidget *view, double width, 
												double height, double zoom, gint rotate)
{
	GdkPixbuf *pb;
	
	debug_print("width: %f\n", width);
	pb = gdk_pixbuf_new(GDK_COLORSPACE_RGB, 
				FALSE, 8, 
				(int)(width * zoom), 
				(int)(height * zoom));	
	
			poppler_page_render_to_pixbuf(page, 0, 0, 
				(int)(width * zoom), 
				(int)(height * zoom), 
				zoom, rotate, pb);
		
			gtk_image_set_from_pixbuf(GTK_IMAGE(view), pb);
			g_object_unref(G_OBJECT(pb));
}
static void pdf_viewer_render_selection(PdfViewer *viewer, PopplerRectangle *rect, PageResult *page_results)
{
	gint selw, selh;
	double width_points, height_points;
	gint width, height;
	GdkPixbuf *sel_pb, *page_pb;
	gfloat x1, x2, y1, y2;	
	

	gint cur_page_num = 
		gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(viewer->cur_page));
	
	viewer->last_match = viewer->res_cnt;
	
	viewer->last_rect = NULL;
	viewer->last_page_result = NULL;
	if (cur_page_num != page_results->page_num) {
		/* we changed page. update the view */
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(viewer->cur_page), 
	(gdouble) page_results->page_num);
	}
			
	viewer->last_rect = rect;
	viewer->last_page_result = page_results;

	GTK_EVENTS_FLUSH();
				
	poppler_page_get_size(POPPLER_PAGE(viewer->pdf_page), &width_points, &height_points);
	width = (int)((width_points * viewer->zoom) + 0.5);
	height = (int)((height_points * viewer->zoom) + 0.5);

	if (viewer->rotate == 90) {
		x1 = MIN(rect->y1,rect->y2) * viewer->zoom;
		x2 = MAX(rect->y1,rect->y2) * viewer->zoom;
		y1 = MAX(rect->x1,rect->x2) * viewer->zoom;
		y2 = MIN(rect->x1,rect->x2) * viewer->zoom;
		selw = (x2 - x1);
		selh = (y1 - y2);

	} else if (viewer->rotate == 180) {
		x1 = width - rect->x2 * viewer->zoom;
		x2 = width - rect->x1 * viewer->zoom;
		y1 = height - rect->y2 * viewer->zoom;
		y2 = height - rect->y1 * viewer->zoom;
		selw = (x2 - x1);
		selh = (y2 - y1);
		y1 = height - y1;
		y2 = height - y2;

	} else if (viewer->rotate == 270) {
		x1 = height - MAX(rect->y1,rect->y2) * viewer->zoom;
		x2 = height - MIN(rect->y1,rect->y2) * viewer->zoom;
		y1 = width - MIN(rect->x1,rect->x2) * viewer->zoom;
		y2 = width - MAX(rect->x1,rect->x2) * viewer->zoom;
		selw = (x2 - x1);
		selh = (y1 - y2);
	} else {
		x1 = rect->x1 * viewer->zoom;
		x2 = rect->x2 * viewer->zoom;
		y1 = rect->y1 * viewer->zoom;
		y2 = rect->y2 * viewer->zoom;
		selw = (x2 - x1);
		selh = (y2 - y1);
		y1 = height - y1;
		y2 = height - y2;
	}
				
	sel_pb = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, 
							selw, selh);

	gdk_pixbuf_fill(sel_pb, SELECTION_COLOR);
				
	page_pb = gtk_image_get_pixbuf(GTK_IMAGE(viewer->pdf_view));
				
	page_pb = gdk_pixbuf_new(GDK_COLORSPACE_RGB, 
					FALSE, 8, 
				(int)(viewer->width * viewer->zoom), 
				(int)(viewer->height * viewer->zoom));	
	
	poppler_page_render_to_pixbuf(viewer->pdf_page, 
					0, 
					0, 
				(int)(viewer->width * viewer->zoom), 
				(int)(viewer->height * viewer->zoom), 
					viewer->zoom, 
					viewer->rotate, 
					page_pb);
				
	gdk_pixbuf_composite(sel_pb, page_pb, 
					x1, y2, selw, selh, 0, 0, 
					viewer->zoom, viewer->zoom, 
					GDK_INTERP_BILINEAR, ALPHA_CHANNEL);
				
	gtk_image_set_from_pixbuf(GTK_IMAGE(viewer->pdf_view), page_pb);
	
	pdf_viewer_scroll_to(viewer, MIN(x1,x2), MIN(y1,y2));

	g_object_unref(G_OBJECT(sel_pb));
	g_object_unref(G_OBJECT(page_pb));

}

static gboolean	pdf_viewer_text_search(MimeViewer *_viewer, gboolean backward,
				     const gchar *str, gboolean case_sens)
{
	PdfViewer *viewer = (PdfViewer *)_viewer;
	GList *all_pages_results, *cur_page_results;
	viewer->res_cnt = 0;

	debug_print("pdf_viewer_text_search: %s\n", str);
	main_window_cursor_wait(mainwindow_get_mainwindow());
	
	if (viewer->last_search && strcmp(str, viewer->last_search)) {
		search_matches_free(viewer);
		viewer->last_match = -1;
		viewer->num_matches = 0;
	} else if (!viewer->last_search) {
		viewer->last_match = -1;
		viewer->num_matches = 0;
	}
	/* It's a new search, build list of matches 
	 * across all pages */
	if (viewer->last_match == -1) {
		gint i; 
		
		for(i = 1; i <= viewer->num_pages; i++) {
			
			PopplerPage *pdf_page = poppler_document_get_page(viewer->pdf_doc, i - 1);
			viewer->page_results = poppler_page_find_text(pdf_page, str);
			
			if (viewer->page_results != NULL) {
				debug_print("page_results %p\n", viewer->page_results);
				/* store results for this page */
				gint num_res = 0;
				PageResult *res = g_new0(PageResult, 1);
				res->results = viewer->page_results;
				res->page_num = i;
				/* found text, prepend this page(faster than append) */
				viewer->text_found = g_list_prepend(viewer->text_found, res);
				num_res = g_list_length(viewer->page_results);
				debug_print("%d results on page %d\n", num_res, i);
				viewer->num_matches += num_res;
			}
			g_object_unref(G_OBJECT(pdf_page));
		}

		if (viewer->text_found == NULL) {
			main_window_cursor_normal(mainwindow_get_mainwindow());
			return FALSE;
		}
		/* put back the list in the correct order */
		viewer->text_found = g_list_reverse(viewer->text_found);
	} 
	
	if (!viewer->text_found) {
		main_window_cursor_normal(mainwindow_get_mainwindow());
		return FALSE;
	} else {
		viewer->last_search = g_strdup(str);
	}
	
	if (backward) {
		/* if backward, we have to initialize stuff to search 
		 * from the end */
		viewer->res_cnt = viewer->num_matches-1;
		if (viewer->last_match == -1) {
			viewer->last_match = viewer->num_matches+1;
		}
		all_pages_results = g_list_last(viewer->text_found);
	} 
	else {
		all_pages_results = viewer->text_found;
	}
	
	for(; all_pages_results; all_pages_results = (backward?all_pages_results->prev:all_pages_results->next)) {
		
		PageResult * page_results = (PageResult *)all_pages_results->data;

		if (backward) {
			cur_page_results = g_list_last(page_results->results);
		}
		else {
			cur_page_results = page_results->results;
		}

		for(; cur_page_results; cur_page_results = (backward?cur_page_results->prev:cur_page_results->next)) {

			gboolean valid = FALSE;
			/* first valid result is the last+1 if searching
			 * forward, last-1 if searching backward */
			if (backward) {
				valid = (viewer->res_cnt < viewer->last_match);
			}
			else {
				valid = (viewer->res_cnt > viewer->last_match);
			}
			if (valid) {
				pdf_viewer_render_selection(viewer, 
					(PopplerRectangle *)cur_page_results->data,
						page_results);
				main_window_cursor_normal(mainwindow_get_mainwindow());
				return TRUE;
			}
			
			if (backward) {
				viewer->res_cnt--;
			}
			else {
				viewer->res_cnt++;
			}
		}
	}
	main_window_cursor_normal(mainwindow_get_mainwindow());
	search_matches_free(viewer);
	return FALSE;
}

static void pdf_viewer_get_document_index(PdfViewer *viewer, PopplerIndexIter *index_iter, GtkTreeIter *parentiter)
{
	PopplerAction *action;
	PopplerIndexIter *child;
	GtkTreeIter childiter;

	debug_print("get document index\n");
	do	{
		gint page_num = 0;
		
		action = poppler_index_iter_get_action(index_iter);

		if (action->type != POPPLER_ACTION_GOTO_DEST) {
			poppler_action_free(action);
			continue;
		}

		if (action->goto_dest.dest->type == POPPLER_DEST_XYZ || action->goto_dest.dest->type == POPPLER_DEST_FITH) {
			page_num = action->goto_dest.dest->page_num;
		}
#ifdef HAVE_POPPLER_DEST_NAMED
		else if (action->goto_dest.dest->type == POPPLER_DEST_NAMED) {
			PopplerDest *dest = poppler_document_find_dest(
					viewer->pdf_doc, action->goto_dest.dest->named_dest);
			if (dest->type != POPPLER_DEST_XYZ) {
				g_warning("couldn't figure out link\n");
				poppler_dest_free(dest);
				continue;
			}
			page_num = dest->page_num;
			poppler_dest_free(dest);
		} 
#endif
		else {
#ifdef HAVE_POPPLER_DEST_NAMED
			g_warning("unhandled link type %d\nplease contact developers\n", action->goto_dest.dest->type);
#else
			g_warning("unhandled link type %d\nplease upgrade libpoppler-glib to 0.5.4\n", action->goto_dest.dest->type);
#endif
			continue;
		}
		gtk_tree_store_append(GTK_TREE_STORE(viewer->index_model), &childiter, parentiter);
		gtk_tree_store_set(GTK_TREE_STORE(viewer->index_model), &childiter,
						INDEX_NAME, action->named.title,
						INDEX_PAGE, page_num,
						INDEX_TOP, action->goto_dest.dest->top,
						-1);
		poppler_action_free(action);
		child = poppler_index_iter_get_child(index_iter);
		if (child) {
			pdf_viewer_get_document_index(viewer, child, &childiter);
			poppler_index_iter_free(child);
		}
	}
	while(poppler_index_iter_next(index_iter));
}

static void pdf_viewer_index_row_activated(GtkTreeView		*tree_view,
				   	GtkTreePath		*path,
				   	GtkTreeViewColumn	*column,
				   	gpointer		 data)
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model(tree_view);
	PdfViewer *viewer = (PdfViewer *)data;
	gint page_num = 0;
	
	debug_print("index_row_activated\n");
	if (!gtk_tree_model_get_iter(model, &iter, path)) return;

	gtk_tree_model_get(model, &iter, 
			   INDEX_PAGE, &page_num,
			   -1);

	if (page_num > 0) {
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(viewer->cur_page),(gdouble)page_num);
		debug_print("Page num: %d\n", page_num);
	}
	GTK_EVENTS_FLUSH();
}

/** Disable the index button if the document doesn't have an index */
static void pdf_viewer_set_index_button_sensitive(PdfViewer *viewer)
{
	viewer->pdf_index  = poppler_index_iter_new(viewer->pdf_doc);
	if (viewer->pdf_index) {	
		if (!gtk_widget_is_sensitive(viewer->doc_index)) {
			gtk_widget_set_sensitive(viewer->doc_index, TRUE);
		}
	}
	else {
		gtk_widget_set_sensitive(viewer->doc_index, FALSE);
	}

    poppler_index_iter_free(viewer->pdf_index);
    viewer->pdf_index = NULL;
}

static char * pdf_viewer_get_document_format_data(GTime utime) 
{
	time_t time = (time_t) utime;
	struct tm t;
	char s[256];
	const char *fmt_hack = "%c";
	size_t len;

	if (time == 0 || !localtime_r(&time, &t)) return NULL;

	len = strftime(s, sizeof(s), fmt_hack, &t);
	
	if (len == 0 || s[0] == '\0') return NULL;

	return g_locale_to_utf8(s, -1, NULL, NULL, NULL);
}

#define ADD_TO_TABLE(LABEL, VALUE) \
	label = gtk_label_new(LABEL); \
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5); \
	gtk_misc_set_padding(GTK_MISC(label), 4, 0); \
	gtk_table_attach(viewer->table_doc_info, label, 0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0); \
	\
	label = gtk_label_new(VALUE); \
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5); \
	gtk_misc_set_padding(GTK_MISC(label), 4, 0); \
	gtk_table_attach(viewer->table_doc_info, label, 1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0); \
	row++;

	
static GtkTable * pdf_viewer_fill_info_table(PdfViewer *viewer)
{
	GtkWidget *label;
	const gchar *title, *format, *author, *subject, *keywords, *creator, *producer;
	gboolean linearized;
	gchar *tmp;
	gint row = 0;

	GTime creation_date, mod_date;

	PopplerPageLayout layout;
	PopplerPageMode mode;
	PopplerPermissions permissions;
	PopplerViewerPreferences view_prefs;

	title = format = author = subject = keywords = creator = producer = tmp = 0;

	g_object_get(viewer->pdf_doc,
				"title", &title,
				"format", &format,
				"author", &author,
				"subject", &subject,
				"keywords", &keywords,
				"creation-date", &creation_date,
				"permissions", &permissions,
				"mod-date", &mod_date,
				"creator", &creator,
				"producer", &producer,	
				"linearized", &linearized,
				"page-mode", &mode,
				"page-layout", &layout,
				"viewer-preferences", &view_prefs,
				NULL);

	viewer->table_doc_info = GTK_TABLE(gtk_table_new(13, 2, FALSE));

	ADD_TO_TABLE(_("Filename:"), viewer->target_filename)
	ADD_TO_TABLE(_("Size:"), to_human_readable(viewer->to_load->length))
	ADD_TO_TABLE(NULL, NULL)
	ADD_TO_TABLE(_("Title:"), title)
	ADD_TO_TABLE(_("Subject:"), subject)
	ADD_TO_TABLE(_("Author:"), author)
	ADD_TO_TABLE(_("Keywords:"), keywords)
	ADD_TO_TABLE(_("Creator:"), creator)
	ADD_TO_TABLE(_("Producer:"), producer)

	tmp = pdf_viewer_get_document_format_data(creation_date);
	ADD_TO_TABLE(_("Created:"), tmp)
	g_free(tmp);

	tmp = pdf_viewer_get_document_format_data(mod_date);
	ADD_TO_TABLE(_("Modified:"), tmp)
	g_free(tmp);
	
	ADD_TO_TABLE(_("Format:"), format)
	if (linearized) {
		ADD_TO_TABLE(_("Optimized:"), _("Yes"))
	}
	else {
		ADD_TO_TABLE(_("Optimized:"), _("No"))
	}
	//ADD_TO_TABLE(_("Page Mode:"), pdf_viewer_get_document_info_mode(mode)) 
	//ADD_TO_TABLE(_("Page Layout:"), pdf_viewer_get_document_info_layout(layout))

	return(GtkTable *) viewer->table_doc_info;
}
#undef ADD_TO_TABLE

static FileType pdf_viewer_mimepart_get_type(MimeInfo *partinfo)
{
	gchar *content_type = NULL;
	FileType type = TYPE_UNKNOWN;
	debug_print("mimepart_get_type\n");
	if ((partinfo->type == MIMETYPE_APPLICATION) &&
	(!g_ascii_strcasecmp(partinfo->subtype, "octet-stream"))) {
		
		const gchar *filename;

		filename = procmime_mimeinfo_get_parameter(partinfo, "filename");
		
			if (filename == NULL)
				filename = procmime_mimeinfo_get_parameter(partinfo, "name");
			if (filename != NULL)
				content_type = procmime_get_mime_type(filename);
	} 
	else {
		content_type = procmime_get_content_type_str(partinfo->type, partinfo->subtype);
	}

	if (content_type == NULL) type = TYPE_UNKNOWN;
	else if (!strcmp(content_type, "application/pdf")) type = TYPE_PDF;
	else if (!strcmp(content_type, "application/postscript")) type = TYPE_PS;
	else type = TYPE_UNKNOWN;
	
	g_free(content_type);
	return type;
}

/* Callbacks */
static void pdf_viewer_button_first_page_cb(GtkButton *button, PdfViewer *viewer) 
{
	
	gtk_spin_button_spin(GTK_SPIN_BUTTON(viewer->cur_page), GTK_SPIN_HOME, 1);
}

static void pdf_viewer_button_prev_page_cb(GtkButton *button, PdfViewer *viewer) 
{
	
	gtk_spin_button_spin(GTK_SPIN_BUTTON(viewer->cur_page), GTK_SPIN_STEP_BACKWARD, 1);
}

static void pdf_viewer_button_next_page_cb(GtkButton *button, PdfViewer *viewer) 
{
	
	gtk_spin_button_spin(GTK_SPIN_BUTTON(viewer->cur_page), GTK_SPIN_STEP_FORWARD, 1);
}

static void pdf_viewer_button_last_page_cb(GtkButton *button, PdfViewer *viewer) 
{
	
	gtk_spin_button_spin(GTK_SPIN_BUTTON(viewer->cur_page), GTK_SPIN_END, 1);
}

static void pdf_viewer_spin_change_page_cb(GtkSpinButton *button, PdfViewer *viewer)
{
	pdf_viewer_update((MimeViewer *)viewer, 
			FALSE, 
			gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(viewer->cur_page)));
}

static void pdf_viewer_button_zoom_in_cb(GtkButton *button, PdfViewer *viewer) 
{

	gtk_spin_button_spin(GTK_SPIN_BUTTON(viewer->zoom_scroll), GTK_SPIN_STEP_FORWARD, ZOOM_FACTOR);
}

static void pdf_viewer_spin_zoom_scroll_cb(GtkSpinButton *button, PdfViewer *viewer)
{
	viewer->zoom = gtk_spin_button_get_value(GTK_SPIN_BUTTON(viewer->zoom_scroll));
	pdf_viewer_update((MimeViewer *)viewer,
			FALSE,
			gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(viewer->cur_page)));
}

static void pdf_viewer_button_zoom_out_cb(GtkButton *button, PdfViewer *viewer) 
{

	gtk_spin_button_spin(GTK_SPIN_BUTTON(viewer->zoom_scroll), GTK_SPIN_STEP_BACKWARD, ZOOM_FACTOR);
	
}

static void pdf_viewer_button_press_events_cb(GtkWidget *widget, GdkEventButton *event, PdfViewer *viewer) 
{
	gchar *uri;
	#ifdef HAVE_POPPLER_DEST_NAMED
	PopplerDest *dest;
	#endif
	static GdkCursor *hand_cur = NULL;
	
	if (!hand_cur) hand_cur = gdk_cursor_new(GDK_FLEUR);

	/* Execute Poppler Links */
	if (event->button == 1 && viewer->in_link) {
		switch (viewer->link_action->type) {
		case POPPLER_ACTION_UNKNOWN:
			debug_print("action unknown\n");
			break;
		case POPPLER_ACTION_GOTO_DEST:
			if (viewer->link_action->goto_dest.dest->type == POPPLER_DEST_XYZ || 
					viewer->link_action->goto_dest.dest->type == POPPLER_DEST_FITH) {
				gtk_spin_button_set_value(GTK_SPIN_BUTTON(viewer->cur_page), 
									  (gdouble)viewer->link_action->goto_dest.dest->page_num);
			}
		#ifdef HAVE_POPPLER_DEST_NAMED
			else if (viewer->link_action->goto_dest.dest->type == POPPLER_DEST_NAMED) {
				dest = poppler_document_find_dest(
					viewer->pdf_doc, viewer->link_action->goto_dest.dest->named_dest);
			if (dest->type != POPPLER_DEST_XYZ) {
				g_warning("couldn't figure out link\n");
				poppler_dest_free(dest);
				break;
			}
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(viewer->cur_page), 
									  (gdouble)dest->page_num);
			poppler_dest_free(dest);
		} 
		#endif
			break;
		case POPPLER_ACTION_GOTO_REMOTE:
			#ifdef HAVE_POPPLER_DEST_NAMED
			dest = poppler_document_find_dest(
					viewer->pdf_doc, viewer->link_action->goto_remote.dest->named_dest);
			if (dest->type != POPPLER_DEST_XYZ) {
				g_warning ("couldn't figure out link\n");
				poppler_dest_free(dest);
				break;
			}
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(viewer->cur_page),
									  (gdouble)dest->page_num);
			poppler_dest_free(dest);
			#endif
			break;
		case POPPLER_ACTION_LAUNCH:
			debug_print("action launch not yet implemented\n");
			break;
		case POPPLER_ACTION_URI:
			uri = g_strdup(viewer->link_action->uri.uri);
			if (!g_ascii_strncasecmp(uri, "mailto:", 7)) 
				compose_new(NULL, uri + 7, NULL);
			else 
				open_uri(uri, prefs_common_get_uri_cmd());
			g_free(uri);
			break;
		case POPPLER_ACTION_NAMED:
			debug_print("action named not yet implemented\n");
			break;
		case POPPLER_ACTION_MOVIE:
			debug_print("yoyoyo ;-) a movie?\n");
			break;
		}
		if (((MimeViewer *)viewer)->mimeview && 
			((MimeViewer *)viewer)->mimeview->messageview && 
			((MimeViewer *)viewer)->mimeview->messageview->window && 
			((MimeViewer *)viewer)->mimeview->messageview->window->window) 
			gdk_window_set_cursor (((MimeViewer *)viewer)->mimeview->messageview->window->window, NULL);
		else
			gdk_window_set_cursor (mainwindow_get_mainwindow()->window->window, NULL);
	}

	/* Init document to be scrolled with left mouse click */
	if (event->button == 1 && !viewer->in_link) {
		viewer->pdf_view_scroll = TRUE;
		if (((MimeViewer *)viewer)->mimeview && 
			((MimeViewer *)viewer)->mimeview->messageview && 
			((MimeViewer *)viewer)->mimeview->messageview->window && 
			((MimeViewer *)viewer)->mimeview->messageview->window->window) 
			gdk_window_set_cursor (((MimeViewer *)viewer)->mimeview->messageview->window->window, hand_cur);
		else
			gdk_window_set_cursor (mainwindow_get_mainwindow()->window->window, hand_cur);

		viewer->last_x = event->x;
		viewer->last_y = event->y;
		viewer->last_dir_x = 0;
		viewer->last_dir_y = 0;
	}
}
/* Set the normal cursor*/
static void pdf_viewer_mouse_scroll_destroy_cb(GtkWidget *widget, GdkEventButton *event, PdfViewer *viewer) 
{
	
	if (event->button == 1) {
		viewer->pdf_view_scroll = FALSE;
		if (((MimeViewer *)viewer)->mimeview && 
			((MimeViewer *)viewer)->mimeview->messageview && 
			((MimeViewer *)viewer)->mimeview->messageview->window && 
			((MimeViewer *)viewer)->mimeview->messageview->window->window) 
			gdk_window_set_cursor (((MimeViewer *)viewer)->mimeview->messageview->window->window, NULL);
		else
			gdk_window_set_cursor (mainwindow_get_mainwindow()->window->window, NULL);
	}
}

static void pdf_viewer_move_events_cb(GtkWidget *widget, GdkEventMotion *event, PdfViewer *viewer) 
{
	/* Grab the document and scroll it with mouse */ 
	if (viewer->pdf_view_scroll) {

		viewer->pdf_view_vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(viewer->scrollwin));
		viewer->pdf_view_hadj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(viewer->scrollwin));
			
			if (event->x < viewer->last_x
					&& viewer->pdf_view_hadj->value < (viewer->pdf_view_hadj->upper - viewer->pdf_view_hadj->page_size)) {
				if (viewer->last_dir_x == -1) {
					viewer->pdf_view_hadj->value += viewer->last_x - event->x; 
					g_signal_emit_by_name(G_OBJECT(viewer->pdf_view_hadj),
								"value_changed", 0);
				}
				viewer->last_dir_x = -1;
			}
			else if (event->x > viewer->last_x
					&& viewer->pdf_view_hadj->value > 0.0)  {
				if (viewer->last_dir_x == +1) {
					viewer->pdf_view_hadj->value += viewer->last_x - event->x; 
					g_signal_emit_by_name(G_OBJECT(viewer->pdf_view_hadj),
								"value_changed", 0);
				}
				viewer->last_dir_x = +1;
			}

			if (event->y < viewer->last_y
					&& viewer->pdf_view_vadj->value < (viewer->pdf_view_vadj->upper - viewer->pdf_view_vadj->page_size)) {
				if (viewer->last_dir_y == -1) {
					viewer->pdf_view_vadj->value += viewer->last_y - event->y; 
					g_signal_emit_by_name(G_OBJECT(viewer->pdf_view_vadj),
								"value_changed", 0);
				}
				viewer->last_dir_y = -1;
			}
			else if (event->y > viewer->last_y
					&& viewer->pdf_view_vadj->value > 0.0)  {
				if (viewer->last_dir_y == +1) {
					viewer->pdf_view_vadj->value += viewer->last_y - event->y; 
					g_signal_emit_by_name(G_OBJECT(viewer->pdf_view_vadj),
								"value_changed", 0);
				}
				viewer->last_dir_y = +1;
			}
			viewer->last_x = event->x;
			viewer->last_y = event->y;
			GTK_EVENTS_FLUSH();
		} 
	else {	
	/* Link Mapping */
	static GList *l;
	static GdkCursor *link_cur = NULL;
	static GtkRequisition size;
	static gdouble x,y, x1, y1, x2, y2;
	gboolean ccur;
	
	viewer->pdf_view_vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(viewer->scrollwin));
	viewer->pdf_view_hadj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(viewer->scrollwin));
	
	if (!link_cur) link_cur = gdk_cursor_new(GDK_HAND2);

	ccur = FALSE;
	viewer->in_link = FALSE;	
	for (l = viewer->link_map; l; l = g_list_next (l)) {
		PopplerLinkMapping *lmapping;
		lmapping = (PopplerLinkMapping *)l->data;

		x1 = lmapping->area.x1;
		y1 = lmapping->area.y1;
		x2 = lmapping->area.x2;
		y2 = lmapping->area.y2;
		gtk_widget_size_request(viewer->pdf_view, &size);
		
		switch (viewer->rotate) {
		case 0:
		case 360:
				if (size.width != viewer->pdf_view_hadj->upper)
					x = (event->x - (viewer->pdf_view_hadj->upper - size.width) / 2) / viewer->zoom;
				else
					x = event->x / viewer->zoom;

				y = (viewer->pdf_view_vadj->upper - event->y) / viewer->zoom;
			break;
		case 90:
				if (size.width != viewer->pdf_view_hadj->upper)
					y = (event->x - (viewer->pdf_view_hadj->upper - size.width) / 2) / viewer->zoom;
				else
					y = event->x / viewer->zoom;

				x = (event->y) / viewer->zoom;
			break;
		case 180:
				if (size.width != viewer->pdf_view_hadj->upper)
					x = ((viewer->pdf_view_hadj->upper -  event->x) - ((viewer->pdf_view_hadj->upper - size.width) / 2)) / viewer->zoom;
				else
					x =  ((viewer->pdf_view_hadj->upper -  event->x) - (viewer->pdf_view_hadj->upper - size.width)) / viewer->zoom;

				y = (event->y) / viewer->zoom;
			break;
		case 270:
				if (size.width != viewer->pdf_view_hadj->upper)
					y = ((viewer->pdf_view_hadj->upper -  event->x) - ((viewer->pdf_view_hadj->upper - size.width) / 2)) / viewer->zoom;
				else
					y =  ((viewer->pdf_view_hadj->upper -  event->x) - (viewer->pdf_view_hadj->upper - size.width)) / viewer->zoom;

				x = (viewer->pdf_view_vadj->upper - event->y) / viewer->zoom;
			break;
		}

		if ( (x > x1 && x < x2) && (y > y1 && y < y2) ) {
				viewer->in_link = TRUE;
			if (((MimeViewer *)viewer)->mimeview && 
				((MimeViewer *)viewer)->mimeview->messageview && 
				((MimeViewer *)viewer)->mimeview->messageview->window && 
				((MimeViewer *)viewer)->mimeview->messageview->window->window) 
					gdk_window_set_cursor (((MimeViewer *)viewer)->mimeview->messageview->window->window, link_cur);
				else
					gdk_window_set_cursor (mainwindow_get_mainwindow()->window->window, link_cur);
				
				viewer->link_action = lmapping->action; 
				ccur = TRUE;
		}
		if (!ccur) {
			if (((MimeViewer *)viewer)->mimeview && 
				((MimeViewer *)viewer)->mimeview->messageview && 
				((MimeViewer *)viewer)->mimeview->messageview->window && 
				((MimeViewer *)viewer)->mimeview->messageview->window->window) 
				gdk_window_set_cursor (((MimeViewer *)viewer)->mimeview->messageview->window->window, NULL);
			else
				gdk_window_set_cursor (mainwindow_get_mainwindow()->window->window, NULL);
		}
	}
	g_free(l);
	}
}
static gboolean pdf_viewer_scroll_cb(GtkWidget *widget, GdkEventScroll *event,
				    PdfViewer *viewer)
{
	GtkAdjustment *adj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(viewer->scrollwin));
	static gboolean in_scroll_cb = FALSE;
	gboolean handled = FALSE;
	gint cur_p = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(viewer->cur_page));		

	if (in_scroll_cb)
		return FALSE;

	in_scroll_cb = TRUE;

	if (event->direction == GDK_SCROLL_UP &&
	    adj->value == adj->lower && 
	    cur_p > 1) {
		gtk_spin_button_spin(GTK_SPIN_BUTTON(viewer->cur_page), GTK_SPIN_STEP_BACKWARD, 1);
		adj->value = adj->upper - adj->page_size;
		handled = TRUE;
	} else if (event->direction == GDK_SCROLL_DOWN &&
	    adj->value + adj->page_size == adj->upper &&
	    cur_p < viewer->num_pages) {
		gtk_spin_button_spin(GTK_SPIN_BUTTON(viewer->cur_page), GTK_SPIN_STEP_FORWARD, 1);
		adj->value = 0.0;
		handled = TRUE;
	}
	in_scroll_cb = FALSE;
	return handled;
}

static void pdf_viewer_button_zoom_fit_cb(GtkButton *button, PdfViewer *viewer)
{
	GtkAllocation *allocation;
	double xratio, yratio;
	allocation = &(viewer->scrollwin->allocation);
	debug_print("width: %d\n", allocation->width);
	debug_print("height: %d\n", allocation->height);
	xratio = allocation->width / viewer->width;
	yratio = allocation->height / viewer->height;

	if (xratio >= yratio) {
		viewer->zoom = yratio;
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(viewer->zoom_scroll),viewer->zoom);
	}
	else {
		viewer->zoom = xratio;
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(viewer->zoom_scroll),viewer->zoom);
	}
}

static void pdf_viewer_button_zoom_width_cb(GtkButton *button, PdfViewer *viewer)
{
	GtkAllocation *allocation;
	double xratio;
	allocation = &(viewer->scrollwin->allocation);
	debug_print("width: %d\n", allocation->width);
	xratio = allocation->width / viewer->width;
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(viewer->zoom_scroll), xratio);
}

static void pdf_viewer_button_rotate_right_cb(GtkButton *button, PdfViewer *viewer)
{
	if (viewer->rotate == 360) {
		viewer->rotate = 0;
	}
	
	viewer->rotate += (gint) ROTATION;
	pdf_viewer_update((MimeViewer *)viewer, FALSE,
		gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(viewer->cur_page)));
}

static void pdf_viewer_button_rotate_left_cb(GtkButton *button, PdfViewer *viewer)
{
	if (viewer->rotate == 0) {
		viewer->rotate = 360;
	}
	
	viewer->rotate = abs(viewer->rotate -(gint) ROTATION);
	pdf_viewer_update((MimeViewer *)viewer, FALSE,
		gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(viewer->cur_page)));
}

/* Show/Hide the index pane */
static void pdf_viewer_show_document_index_cb(GtkButton *button, PdfViewer *viewer)
{
	if (!viewer->pdf_index) {
		viewer->pdf_index = poppler_index_iter_new(viewer->pdf_doc);
	}
	
	gtk_tree_store_clear(GTK_TREE_STORE(viewer->index_model));
	
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(viewer->doc_index))) {
		pdf_viewer_get_document_index(viewer,(PopplerIndexIter *) viewer->pdf_index, NULL);
		gtk_widget_show(GTK_WIDGET(viewer->frame_index));
	}
	else {
		pdf_viewer_hide_index_pane(viewer);
	}
		
}

static void pdf_viewer_button_document_info_cb(GtkButton *button, PdfViewer *viewer)
{
	alertpanel_full(_("PDF properties"), NULL, GTK_STOCK_CLOSE, NULL, NULL,
			FALSE, (GtkWidget *) pdf_viewer_fill_info_table(viewer), 
			ALERT_NOTICE, G_ALERTDEFAULT);
}

/*
static const char * poppler_get_document_info_mode(PopplerPageMode mode)
{
	GEnumValue *enum_value;

	enum_value = g_enum_get_value((GEnumClass *) g_type_class_peek(POPPLER_TYPE_PAGE_MODE), mode);
	return(gchar *) enum_value->value_name;
}
static const char * poppler_get_document_info_layout(PopplerPageLayout layout)
{

	GEnumValue *enum_value;

	enum_value = g_enum_get_value((GEnumClass *) g_type_class_peek(POPPLER_TYPE_PAGE_LAYOUT), layout);
	return(gchar *) enum_value->value_name;
}
*/
static void pdf_viewer_show_controls(PdfViewer *viewer, gboolean show)
{
	if (show) {
		gtk_widget_show(viewer->first_page);
		gtk_widget_show(viewer->cur_page);
		gtk_widget_show(viewer->prev_page);
		gtk_widget_show(viewer->next_page);
		gtk_widget_show(viewer->last_page);
		gtk_widget_show(viewer->zoom_in);
		gtk_widget_show(viewer->zoom_out);
		gtk_widget_show(viewer->zoom_fit);
		gtk_widget_show(viewer->zoom_width);
		gtk_widget_show(viewer->zoom_scroll);
		gtk_widget_show(viewer->widgets_table);
		gtk_widget_show(viewer->rotate_right);
		gtk_widget_show(viewer->rotate_left);
		gtk_widget_show(viewer->doc_info);
		gtk_widget_show(viewer->doc_index);
	} else {
		gtk_widget_hide(viewer->first_page);
		gtk_widget_hide(viewer->cur_page);
		gtk_widget_hide(viewer->prev_page);
		gtk_widget_hide(viewer->next_page);
		gtk_widget_hide(viewer->last_page);
		gtk_widget_hide(viewer->zoom_in);
		gtk_widget_hide(viewer->zoom_out);
		gtk_widget_hide(viewer->zoom_fit);
		gtk_widget_hide(viewer->zoom_width);
		gtk_widget_hide(viewer->widgets_table);
		gtk_widget_hide(viewer->rotate_right);
		gtk_widget_hide(viewer->rotate_left);
		gtk_widget_hide(viewer->doc_info);
		gtk_widget_hide(viewer->doc_index);
		gtk_widget_hide(viewer->zoom_scroll);
	}
}
/** Render the current page, page_num on the viewer */
static void pdf_viewer_update(MimeViewer *_viewer, gboolean reload_file, int page_num) 
{

	PdfViewer *viewer = (PdfViewer *) _viewer;
	GError *error;
	gchar *tmpfile = NULL;
	gchar *tmp;

	debug_print("pdf_viewer_update\n");

	error = NULL;
	if (reload_file) {
		if (viewer->pdf_doc) {
			g_object_unref(G_OBJECT(viewer->pdf_doc));
			viewer->pdf_doc = NULL;
		}
		
		if (pdf_viewer_mimepart_get_type(viewer->to_load) == TYPE_PS) {
			stock_pixbuf_gdk(viewer->hbox, 
					STOCK_PIXMAP_MIME_PS, 
					&viewer->icon_pixbuf);
			gtk_image_set_from_pixbuf(GTK_IMAGE(viewer->icon_type),
									viewer->icon_pixbuf);
		} 
		else if (pdf_viewer_mimepart_get_type(viewer->to_load) == TYPE_PDF) {
			stock_pixbuf_gdk(viewer->hbox, 
			STOCK_PIXMAP_MIME_PDF, 
			&viewer->icon_pixbuf);
			gtk_image_set_from_pixbuf(GTK_IMAGE(viewer->icon_type), 
									viewer->icon_pixbuf);
		} 
		else {
			stock_pixbuf_gdk(viewer->hbox, 
			STOCK_PIXMAP_MIME_APPLICATION, 
			&viewer->icon_pixbuf);
			gtk_image_set_from_pixbuf(GTK_IMAGE(viewer->icon_type), 
									viewer->icon_pixbuf);
		}

		gtk_label_set_text(GTK_LABEL(viewer->doc_label), _("Loading..."));	
		pdf_viewer_show_controls(viewer, FALSE);
		main_window_cursor_wait(mainwindow_get_mainwindow());

		GTK_EVENTS_FLUSH();

		if (pdf_viewer_mimepart_get_type(viewer->to_load) == TYPE_PS) {
			gchar *cmdline = NULL, *tmp = NULL, *gspath = NULL;
			gint result = 0;

			if ((gspath = g_find_program_in_path("gs")) != NULL) {
				g_free(gspath);
				/* convert postscript to pdf */
				tmpfile = get_tmp_file();
				cmdline = g_strdup_printf(
					"gs -dSAFER -dCompatibilityLevel=1.2 -q -dNOPAUSE -dBATCH "
					  "-sDEVICE=pdfwrite -sOutputFile=%s -c .setpdfwrite -f \"%s\"",
					tmpfile, viewer->filename);
				result = execute_command_line(cmdline, FALSE);
				if (result == 0) {
					tmp = g_filename_to_uri(tmpfile, NULL, NULL);
					viewer->pdf_doc = poppler_document_new_from_file( tmp, NULL, &error);
					g_free(tmp);
				} 
				else {
					g_warning("gs conversion failed: %s returned %d\n", cmdline, result);
					tmp = g_strdup_printf("gs: err %d", result);
					alertpanel_warning("%s", tmp);
					g_free(tmp);
				}
	
				g_free(cmdline);
				claws_unlink(tmpfile);
				g_free(tmpfile);
			}
			else {
				g_warning("gs conversion disabled: gs binary was not found\n");
				alertpanel_warning("PostScript view disabled: required gs program not found");
				result = 1;
					
			}
			if (result != 0) {
				main_window_cursor_normal(mainwindow_get_mainwindow());
				return;
			}
		}   
		else {
			viewer->pdf_doc = poppler_document_new_from_file( viewer->fsname, NULL, &error);
		}
		
		viewer->num_pages = poppler_document_get_n_pages(viewer->pdf_doc);

		g_signal_handlers_block_by_func(G_OBJECT(viewer->cur_page), pdf_viewer_spin_change_page_cb,(gpointer *)viewer);
		gtk_spin_button_set_range(GTK_SPIN_BUTTON(viewer->cur_page), 
									1, 
								(gdouble)viewer->num_pages );

		g_signal_handlers_unblock_by_func(G_OBJECT(viewer->cur_page), pdf_viewer_spin_change_page_cb,(gpointer *)viewer);
		gtk_spin_button_spin(GTK_SPIN_BUTTON(viewer->cur_page), GTK_SPIN_HOME, 1);
		tmp = g_strdup_printf(_("%s Document"),pdf_viewer_mimepart_get_type(viewer->to_load) == TYPE_PDF ? "PDF":"Postscript");
		CLAWS_SET_TIP(
				GTK_WIDGET(viewer->icon_type_ebox),
				tmp);
		g_free(tmp);

		tmp = g_strdup_printf(_("of %d"), viewer->num_pages);
		gtk_label_set_text(GTK_LABEL(viewer->doc_label), tmp);
		g_free(tmp);

		pdf_viewer_show_controls(viewer, TRUE);
		main_window_cursor_normal(mainwindow_get_mainwindow());
	} 
	if (viewer->pdf_doc == NULL) {
		strretchomp(error->message);
		stock_pixbuf_gdk(viewer->hbox, 
				STOCK_PIXMAP_MIME_APPLICATION, 
				&viewer->icon_pixbuf);

		gtk_image_set_from_pixbuf(GTK_IMAGE(viewer->icon_type), viewer->icon_pixbuf);
		alertpanel_error("%s", error->message);
	
		pdf_viewer_show_controls(viewer, FALSE);
		g_error_free(error);
		return;
	}

	if (page_num == 1) { 
		gtk_widget_set_sensitive(viewer->first_page, FALSE);
		gtk_widget_set_sensitive(viewer->prev_page, FALSE);
	}
	else {
		gtk_widget_set_sensitive(viewer->first_page, TRUE);
		gtk_widget_set_sensitive(viewer->prev_page, TRUE);
	}

	if (page_num == viewer->num_pages) { 
		gtk_widget_set_sensitive(viewer->last_page, FALSE);
		gtk_widget_set_sensitive(viewer->next_page, FALSE);
	}
	else {
		gtk_widget_set_sensitive(viewer->last_page, TRUE);
		gtk_widget_set_sensitive(viewer->next_page, TRUE);
	}

	/* check for the index if exists */
	pdf_viewer_set_index_button_sensitive((PdfViewer *) viewer);
	
	if (page_num > 0 && page_num <= viewer->num_pages) {

		GTK_EVENTS_FLUSH();
		
		if (viewer->pdf_page) {
			g_object_unref(G_OBJECT(viewer->pdf_page));
		}
			
		viewer->pdf_page = poppler_document_get_page(viewer->pdf_doc, page_num - 1);

		if (viewer->pdf_page == NULL) {
			g_warning("Page not found\n");
			return;
		}   
	
		if (viewer->rotate == 90 || viewer->rotate == 270) {
			poppler_page_get_size(viewer->pdf_page, &viewer->height, &viewer->width);
		} 
		else {
			poppler_page_get_size(viewer->pdf_page, &viewer->width, &viewer->height);
		}

		if (viewer->last_rect && viewer->last_page_result &&
		    viewer->last_page_result->page_num == page_num) {
			pdf_viewer_render_selection(viewer, viewer->last_rect, viewer->last_page_result);
		}
		else {
			pdf_viewer_render_page(viewer->pdf_page, viewer->pdf_view, viewer->width, 
									viewer->height, viewer->zoom, viewer->rotate);

		}
	
	/* Get Links Mapping */
	if (viewer->link_map) {
		poppler_page_free_link_mapping(viewer->link_map);
	}
	viewer->link_map = poppler_page_get_link_mapping(viewer->pdf_page);
	
	}
}


static void pdf_viewer_show_mimepart(MimeViewer *_viewer, const gchar *infile,
				MimeInfo *partinfo)
{
	PdfViewer *viewer = (PdfViewer *) _viewer;
	gchar buf[4096];
	const gchar *charset = NULL;
	MessageView *messageview = ((MimeViewer *)viewer)->mimeview 
					?((MimeViewer *)viewer)->mimeview->messageview 
					: NULL;

	viewer->rotate = 0;
	viewer->to_load = partinfo;

	memset(buf, 0, sizeof(buf));
	messageview->updating = TRUE;
	debug_print("pdf_viewer_show_mimepart\n");

	if (viewer->filename != NULL) {
		claws_unlink(viewer->filename);
		g_free(viewer->filename);
		viewer->filename = NULL;
	}
	
	viewer->mimeinfo = NULL;
	
	if (partinfo) {
		viewer->target_filename = procmime_get_part_file_name(partinfo);
		viewer->filename = procmime_get_tmp_file_name(partinfo);
		viewer->fsname = g_filename_to_uri(viewer->filename, NULL, NULL);
	}
	
	if (partinfo && !(procmime_get_part(viewer->filename, partinfo) < 0)) {

		if (_viewer && _viewer->mimeview && 
				_viewer->mimeview->messageview->forced_charset) {
			charset = _viewer->mimeview->messageview->forced_charset;
		}
		else {
			charset = procmime_mimeinfo_get_parameter(partinfo, "charset");
		}
		if (charset == NULL) {
			charset = conv_get_locale_charset_str();
		}

		debug_print("using charset %s\n", charset);
		
		viewer->mimeinfo = partinfo;
	}
	
	pdf_viewer_update((MimeViewer *)viewer, TRUE, 1);
	
	messageview->updating = FALSE;
}

static void pdf_viewer_clear(MimeViewer *_viewer)
{
	PdfViewer *viewer = (PdfViewer *) _viewer;
	GtkAdjustment *vadj;
		
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(viewer->doc_index), FALSE);
	gtk_widget_hide(viewer->frame_index);

	debug_print("pdf_viewer_clear\n");
	viewer->to_load = NULL;
	
	if (viewer->pdf_doc) {
		g_object_unref(G_OBJECT(viewer->pdf_doc));
		viewer->pdf_doc = NULL;
	}
	
	vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(viewer->scrollwin));
	vadj->value = 0.0;
	g_signal_emit_by_name(G_OBJECT(vadj), "value-changed", 0);
	vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(viewer->scrollwin_index));
	vadj->value = 0.0;
	g_signal_emit_by_name(G_OBJECT(vadj), "value-changed", 0);
	gtk_tree_store_clear(GTK_TREE_STORE(viewer->index_model));
	gtk_image_set_from_pixbuf(GTK_IMAGE(viewer->pdf_view), NULL);
}

static void pdf_viewer_destroy(MimeViewer *_viewer)
{
	PdfViewer *viewer = (PdfViewer *) _viewer;

	debug_print("pdf_viewer_destroy\n");
	
	if (viewer->pdf_index) poppler_index_iter_free(viewer->pdf_index);

	poppler_page_free_link_mapping (viewer->link_map);
	g_object_unref(GTK_WIDGET(viewer->vbox));
	g_object_unref(GTK_WIDGET(viewer->pdf_view));
	g_object_unref(GTK_WIDGET(viewer->doc_index_pane));
	g_object_unref(GTK_WIDGET(viewer->scrollwin));
	g_object_unref(GTK_WIDGET(viewer->scrollwin_index));
	claws_unlink(viewer->filename);
	g_free(viewer->filename);
	g_free(viewer);
}

static gboolean pdf_viewer_scroll_page(MimeViewer *_viewer, gboolean up)
{
	PdfViewer *viewer = (PdfViewer *)_viewer;
	GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(
				GTK_SCROLLED_WINDOW(viewer->scrollwin));
	
	gint cur_p = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(viewer->cur_page));
	
	if (viewer->pdf_view == NULL) return FALSE;

	if (!gtkutils_scroll_page(GTK_WIDGET(viewer->pdf_view), vadj, up)) {
		if (!up && cur_p != viewer->num_pages) {
			gtk_spin_button_spin(GTK_SPIN_BUTTON(viewer->cur_page), GTK_SPIN_STEP_FORWARD, 1);
			vadj = gtk_scrolled_window_get_vadjustment(
					GTK_SCROLLED_WINDOW(viewer->scrollwin));
			vadj->value = 0.0;
			g_signal_emit_by_name(G_OBJECT(vadj), "value-changed", 0);
			return TRUE;
		} 
		else if (up && cur_p != 1) {
			gtk_spin_button_spin(GTK_SPIN_BUTTON(viewer->cur_page), GTK_SPIN_STEP_BACKWARD, 1);
			vadj = gtk_scrolled_window_get_vadjustment(
					GTK_SCROLLED_WINDOW(viewer->scrollwin));
			vadj->value = vadj->upper - vadj->page_size;
			g_signal_emit_by_name(G_OBJECT(vadj), "value-changed", 0);
			return TRUE;
		} 
		return FALSE;
	} 
	else return TRUE;
}

static void pdf_viewer_scroll_one_line(MimeViewer *_viewer, gboolean up)
{
	PdfViewer *viewer = (PdfViewer *)_viewer;
	GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(
				GTK_SCROLLED_WINDOW(viewer->scrollwin));
	gint cur_p;
	cur_p = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(viewer->cur_page));

	if (viewer->pdf_view == NULL) return; 
		debug_print("up: %d\n", up);	
		if (vadj->value <(vadj->upper - vadj->page_size))  {
			gtkutils_scroll_one_line(GTK_WIDGET(viewer->pdf_view), vadj, up);
		}
		else {
			if (cur_p != viewer->num_pages) {
				pdf_viewer_scroll_page((MimeViewer *)viewer, up);
			}
		}

}

#define BUTTON_H_PADDING 3
#define ADD_BUTTON_TO_TABLE(widget, stock_image) \
	widget = gtk_button_new(); \
	img = gtk_image_new_from_stock(stock_image, GTK_ICON_SIZE_MENU); \
	gtk_button_set_image(GTK_BUTTON(widget), img); \
	gtk_table_attach(GTK_TABLE(viewer->widgets_table), GTK_WIDGET(widget), \
				col, col+1, 0, 1, 0, 0, BUTTON_H_PADDING, 0); \
	col++;

#define ADD_SEP_TO_TABLE \
	sep = gtk_label_new(""); \
	gtk_table_attach(GTK_TABLE(viewer->widgets_table), GTK_WIDGET(sep), \
					col, col+1, 0, 1, 0, 0, 0, 0); \
	gtk_table_set_col_spacing(GTK_TABLE(viewer->widgets_table), col, 3*BUTTON_H_PADDING); \
	col++;

#if GTK_CHECK_VERSION(2,10,0) && POPPLER_HAS_CAIRO && !USE_LIBGNOMEPRINT
static PangoContext *pdf_viewer_get_pango_context(gpointer data)
{
	return NULL;
}

static gpointer pdf_viewer_get_data_to_print(gpointer data, gint sel_start, gint sel_end)
{
	return NULL; /* we don't need it */
}

static void pdf_viewer_cb_begin_print(GtkPrintOperation *op, GtkPrintContext *context,
			   gpointer user_data)
{
  PrintData *print_data;
  PopplerDocument *pdf_doc;
  gint n_pages = 0;
  print_data = (PrintData*) user_data;
  pdf_doc = (PopplerDocument *)printing_get_renderer_data(print_data);

  debug_print("Preparing print job...\n");

  n_pages = poppler_document_get_n_pages(pdf_doc);
  printing_set_n_pages(print_data, n_pages);
  gtk_print_operation_set_n_pages(op, n_pages);

  debug_print("Starting print job...\n");
}

static void pdf_viewer_cb_draw_page(GtkPrintOperation *op, GtkPrintContext *context,
			 int page_nr, gpointer user_data)
{
  cairo_t *cr;
  PrintData *print_data;
  PopplerDocument *pdf_doc;
  PopplerPage *pdf_page;
  
  print_data = (PrintData*) user_data;
  pdf_doc = (PopplerDocument *)printing_get_renderer_data(print_data);
  pdf_page = poppler_document_get_page(pdf_doc, page_nr);

  cr = gtk_print_context_get_cairo_context(context);
  cairo_scale(cr, printing_get_zoom(print_data), printing_get_zoom(print_data));
  cairo_set_source_rgb(cr, 0., 0., 0.);

  poppler_page_render(pdf_page, cr);

  g_object_unref(G_OBJECT(pdf_page));

  debug_print("Sent page %d to printer\n", page_nr+1);
}

static void pdf_viewer_print(MimeViewer *mviewer)
{
	PdfViewer *viewer = (PdfViewer *)mviewer;
	PrintRenderer *pdf_renderer = g_new0(PrintRenderer, 1);
	MainWindow *mainwin = mainwindow_get_mainwindow();
	
	pdf_renderer->get_pango_context = pdf_viewer_get_pango_context;
	pdf_renderer->get_data_to_print = pdf_viewer_get_data_to_print;
	pdf_renderer->cb_begin_print    = pdf_viewer_cb_begin_print;
	pdf_renderer->cb_draw_page      = pdf_viewer_cb_draw_page;

	printing_print_full(mainwin ? GTK_WINDOW(mainwin->window):NULL, 
			pdf_renderer, viewer->pdf_doc, -1, -1);
	
	g_free(pdf_renderer);
}
#endif

static MimeViewer *pdf_viewer_create(void)
{
	PdfViewer *viewer;
 	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkTreeStore *tree_store;
	GtkWidget *sep;
	GtkWidget *img;
	gint col = 0;

	viewer = g_new0(PdfViewer, 1);
	debug_print("pdf_viewer_create\n");
    
	viewer->last_x = 0;
	viewer->last_y = 0;
	viewer->mimeviewer.factory = &pdf_viewer_factory;
	viewer->mimeviewer.get_widget = pdf_viewer_get_widget;
	viewer->mimeviewer.show_mimepart = pdf_viewer_show_mimepart;
	viewer->mimeviewer.clear_viewer = pdf_viewer_clear;
	viewer->mimeviewer.destroy_viewer = pdf_viewer_destroy;
	viewer->mimeviewer.text_search = pdf_viewer_text_search;
	viewer->mimeviewer.scroll_page = pdf_viewer_scroll_page;
	viewer->mimeviewer.scroll_one_line = pdf_viewer_scroll_one_line;
#if GTK_CHECK_VERSION(2,10,0) && POPPLER_HAS_CAIRO && !USE_LIBGNOMEPRINT
	viewer->mimeviewer.print = pdf_viewer_print;
#endif
	viewer->scrollwin = gtk_scrolled_window_new(NULL, NULL);
	viewer->scrollwin_index = gtk_scrolled_window_new(NULL, NULL);
	viewer->pdf_view_ebox = gtk_event_box_new();
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(viewer->pdf_view_ebox), FALSE);
							
	viewer->mimeinfo  = NULL;

	viewer->pdf_view = gtk_image_new();
	gtk_widget_set_events(viewer->pdf_view_ebox,
						GDK_BUTTON_RELEASE_MASK
						| GDK_POINTER_MOTION_MASK
						| GDK_BUTTON_PRESS_MASK
						| GDK_BUTTON_MOTION_MASK
					    );
	gtk_container_add (GTK_CONTAINER(viewer->pdf_view_ebox), viewer->pdf_view);
	viewer->icon_type = gtk_image_new();
	viewer->icon_type_ebox = gtk_event_box_new();

	gtk_container_add(GTK_CONTAINER(viewer->icon_type_ebox), viewer->icon_type);

	viewer->doc_label = gtk_label_new("");

	viewer->widgets_table = gtk_table_new(1, 1, FALSE);

	viewer->doc_index_pane = gtk_hpaned_new();

	viewer->frame_index = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(viewer->frame_index), GTK_SHADOW_IN);
	gtk_widget_set_size_request(viewer->frame_index, 18, -1);
	gtk_frame_set_label(GTK_FRAME(viewer->frame_index), _("Document Index"));

#if !(GTK_CHECK_VERSION(2,12,0))
	viewer->button_bar_tips = tips;
#endif

	ADD_SEP_TO_TABLE
	ADD_BUTTON_TO_TABLE(viewer->first_page, GTK_STOCK_GOTO_FIRST)
	ADD_BUTTON_TO_TABLE(viewer->prev_page, GTK_STOCK_GO_BACK)
	viewer->cur_page = gtk_spin_button_new_with_range(0.0, 0.0, 1.0);
	viewer->zoom_scroll = gtk_spin_button_new_with_range(0.20, 8.0, 0.20);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(viewer->zoom_scroll), 1.0);
	viewer->zoom = 1.0;
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(viewer->cur_page), TRUE);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(viewer->zoom_scroll), TRUE);
	gtk_table_attach(GTK_TABLE(viewer->widgets_table), GTK_WIDGET(viewer->cur_page),
					col, col+1, 
					0, 1, 0, 0, 
					BUTTON_H_PADDING, 
					0);
	col++;
	gtk_table_attach(GTK_TABLE(viewer->widgets_table), GTK_WIDGET(viewer->doc_label),
					col, col+1, 
					0, 1, 0, 0, 
					BUTTON_H_PADDING, 
					0);
	col++;

	ADD_BUTTON_TO_TABLE(viewer->next_page, GTK_STOCK_GO_FORWARD)
	ADD_BUTTON_TO_TABLE(viewer->last_page, GTK_STOCK_GOTO_LAST)
	ADD_SEP_TO_TABLE
	ADD_BUTTON_TO_TABLE(viewer->zoom_fit, GTK_STOCK_ZOOM_FIT)
	ADD_BUTTON_TO_TABLE(viewer->zoom_in, GTK_STOCK_ZOOM_IN)
	gtk_table_attach(GTK_TABLE(viewer->widgets_table), GTK_WIDGET(viewer->zoom_scroll),
					col, col+1, 
					0, 1, 0, 0, 
					BUTTON_H_PADDING, 
					0);
	col++;
	ADD_BUTTON_TO_TABLE(viewer->zoom_out, GTK_STOCK_ZOOM_OUT)
	ADD_BUTTON_TO_TABLE(viewer->zoom_width, GTK_STOCK_FULLSCREEN)
	ADD_SEP_TO_TABLE
	ADD_BUTTON_TO_TABLE(viewer->rotate_left, GTK_STOCK_UNDO)
	ADD_BUTTON_TO_TABLE(viewer->rotate_right, GTK_STOCK_REDO)
	ADD_SEP_TO_TABLE
	ADD_BUTTON_TO_TABLE(viewer->doc_info, GTK_STOCK_INFO)

	viewer->doc_index = GTK_WIDGET(gtk_toggle_tool_button_new_from_stock(GTK_STOCK_INDEX));
	gtk_widget_set_size_request(GTK_WIDGET(viewer->doc_index), 26, 26);
	gtk_table_attach(GTK_TABLE(viewer->widgets_table), GTK_WIDGET(viewer->doc_index),
					col, col+1, 
					0, 1, 0, 0, 
					BUTTON_H_PADDING, 
					0);
	col++;
	
	gtk_scrolled_window_set_policy(
			GTK_SCROLLED_WINDOW(viewer->scrollwin), 
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	gtk_scrolled_window_set_shadow_type(
			GTK_SCROLLED_WINDOW(viewer->scrollwin),
			GTK_SHADOW_IN);

	gtk_scrolled_window_add_with_viewport(
			GTK_SCROLLED_WINDOW(viewer->scrollwin),
			viewer->pdf_view_ebox);

	viewer->vbox = gtk_vbox_new(FALSE, 4);
	viewer->hbox = gtk_hbox_new(FALSE, 4);

    /* treeview */
	tree_store = gtk_tree_store_new(N_INDEX_COLUMNS,
					G_TYPE_STRING,
					G_TYPE_INT,
					G_TYPE_DOUBLE);

	viewer->index_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(tree_store));
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Name"),  renderer, "text", 0,  NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(viewer->index_list), column);		
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(viewer->index_list), FALSE);
	
	viewer->index_model = GTK_TREE_MODEL(tree_store);

	gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(viewer->index_list)), 
								GTK_SELECTION_SINGLE);

	g_signal_connect(G_OBJECT(viewer->index_list), "row_activated",
	                 G_CALLBACK(pdf_viewer_index_row_activated),
					 viewer);

	gtk_scrolled_window_set_policy(
			GTK_SCROLLED_WINDOW(viewer->scrollwin_index), 
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	gtk_scrolled_window_set_shadow_type(
			GTK_SCROLLED_WINDOW(viewer->scrollwin_index),
			GTK_SHADOW_IN);

	gtk_scrolled_window_add_with_viewport(
			GTK_SCROLLED_WINDOW(viewer->scrollwin_index),
			viewer->index_list);

	/* end treeview */

	stock_pixbuf_gdk(viewer->hbox, 
			STOCK_PIXMAP_MIME_TEXT_PLAIN, 
			&viewer->icon_pixbuf);

	gtk_image_set_from_pixbuf(GTK_IMAGE(viewer->icon_type), 
				viewer->icon_pixbuf);

	/* pack widgets*/
	gtk_box_pack_start(GTK_BOX(viewer->hbox), viewer->icon_type_ebox, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(viewer->hbox), viewer->widgets_table, FALSE, FALSE, 0);

	gtk_container_add(GTK_CONTAINER(viewer->frame_index), viewer->scrollwin_index);

	gtk_paned_pack1(GTK_PANED(viewer->doc_index_pane), viewer->frame_index, FALSE, FALSE);
	gtk_paned_pack2(GTK_PANED(viewer->doc_index_pane), viewer->scrollwin, FALSE, FALSE);

	gtk_box_pack_start(GTK_BOX(viewer->vbox), viewer->hbox, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(viewer->vbox), viewer->doc_index_pane, TRUE, TRUE, 0);
	/* show widgets */
	gtk_widget_show(GTK_WIDGET(viewer->doc_index_pane));
	g_object_ref(GTK_WIDGET(viewer->doc_index_pane));
	gtk_widget_show(GTK_WIDGET(viewer->scrollwin));
	g_object_ref(GTK_WIDGET(viewer->scrollwin));
	gtk_widget_show(GTK_WIDGET(viewer->icon_type_ebox));
	g_object_ref(GTK_WIDGET(viewer->icon_type_ebox));
	gtk_widget_show(GTK_WIDGET(viewer->pdf_view_ebox));
	g_object_ref(GTK_WIDGET(viewer->pdf_view_ebox));
	gtk_widget_show(GTK_WIDGET(viewer->scrollwin_index));
	g_object_ref(GTK_WIDGET(viewer->scrollwin_index));
	gtk_widget_show(GTK_WIDGET(viewer->hbox));
	g_object_ref(GTK_WIDGET(viewer->hbox));	
	gtk_widget_show(GTK_WIDGET(viewer->vbox));
	g_object_ref(GTK_WIDGET(viewer->vbox));

	gtk_widget_show(GTK_WIDGET(viewer->widgets_table));
	g_object_ref(GTK_WIDGET(viewer->widgets_table));

	gtk_widget_show(GTK_WIDGET(viewer->cur_page));
	g_object_ref(GTK_WIDGET(viewer->cur_page));

	gtk_widget_show(GTK_WIDGET(viewer->first_page));
	g_object_ref(GTK_WIDGET(viewer->first_page));

	gtk_widget_show(GTK_WIDGET(viewer->last_page));
	g_object_ref(GTK_WIDGET(viewer->last_page));

	gtk_widget_show(GTK_WIDGET(viewer->prev_page));
	g_object_ref(GTK_WIDGET(viewer->prev_page));

	gtk_widget_show(GTK_WIDGET(viewer->next_page));
	g_object_ref(GTK_WIDGET(viewer->next_page));

	gtk_widget_show(GTK_WIDGET(viewer->zoom_in));
	g_object_ref(GTK_WIDGET(viewer->zoom_in));
	gtk_widget_show(GTK_WIDGET(viewer->zoom_out));
	g_object_ref(GTK_WIDGET(viewer->zoom_out));
	gtk_widget_show(GTK_WIDGET(viewer->zoom_fit));
	g_object_ref(GTK_WIDGET(viewer->zoom_fit));
	gtk_widget_show(GTK_WIDGET(viewer->zoom_width));
	g_object_ref(GTK_WIDGET(viewer->zoom_width));

	gtk_widget_show(GTK_WIDGET(viewer->rotate_right));
	g_object_ref(GTK_WIDGET(viewer->rotate_right));
	gtk_widget_show(GTK_WIDGET(viewer->rotate_left));
	g_object_ref(GTK_WIDGET(viewer->rotate_left));
	gtk_widget_show(GTK_WIDGET(viewer->doc_info));
	g_object_ref(GTK_WIDGET(viewer->doc_info));
	gtk_widget_show(GTK_WIDGET(viewer->doc_index));
	g_object_ref(GTK_WIDGET(viewer->doc_index));

	gtk_widget_show(GTK_WIDGET(viewer->doc_label));
	g_object_ref(GTK_WIDGET(viewer->doc_label));
	gtk_widget_show(GTK_WIDGET(viewer->icon_type));
	g_object_ref(GTK_WIDGET(viewer->icon_type));	
	gtk_widget_show(GTK_WIDGET(viewer->pdf_view));
	g_object_ref(GTK_WIDGET(viewer->pdf_view));
	gtk_widget_show(GTK_WIDGET(viewer->zoom_scroll));
	g_object_ref(GTK_WIDGET(viewer->zoom_scroll));

	gtk_widget_show(GTK_WIDGET(viewer->index_list));
	g_object_ref(GTK_WIDGET(viewer->index_list));

	/* Set Tooltips*/
	CLAWS_SET_TIP(viewer->first_page,
				_("First Page"));

	CLAWS_SET_TIP(viewer->prev_page,
				_("Previous Page"));

	CLAWS_SET_TIP(viewer->next_page,
				_("Next Page"));
	
	CLAWS_SET_TIP(viewer->last_page,
				_("Last Page"));

	CLAWS_SET_TIP(viewer->zoom_in,
				_("Zoom In"));
	CLAWS_SET_TIP(viewer->zoom_out,
				_("Zoom Out"));

	CLAWS_SET_TIP(viewer->zoom_fit,
				_("Fit Page"));

	CLAWS_SET_TIP(viewer->zoom_width,
				_("Fit Page Width"));

	CLAWS_SET_TIP(viewer->rotate_left,
				_("Rotate Left"));

	CLAWS_SET_TIP(viewer->rotate_right,
				_("Rotate Right"));

	CLAWS_SET_TIP(viewer->doc_info,
				_("Document Info"));

	CLAWS_SET_TIP(viewer->doc_index,
				_("Document Index"));
	CLAWS_SET_TIP(viewer->cur_page,
				_("Page Number"));
	CLAWS_SET_TIP(viewer->zoom_scroll,
				_("Zoom Factor"));
	/* Connect Signals */
	g_signal_connect(G_OBJECT(viewer->cur_page), 
				    "value-changed", 
				    G_CALLBACK(pdf_viewer_spin_change_page_cb), 
				   (gpointer) viewer);

	g_signal_connect(G_OBJECT(viewer->first_page), 
				    "clicked", 
				    G_CALLBACK(pdf_viewer_button_first_page_cb), 
				   (gpointer) viewer);
	g_signal_connect(G_OBJECT(viewer->prev_page), 
				    "clicked", 
				    G_CALLBACK(pdf_viewer_button_prev_page_cb), 
				   (gpointer) viewer);
	g_signal_connect(G_OBJECT(viewer->next_page), 
				    "clicked", 
				    G_CALLBACK(pdf_viewer_button_next_page_cb), 
				   (gpointer) viewer);
	g_signal_connect(G_OBJECT(viewer->last_page), 
				    "clicked", 
				    G_CALLBACK(pdf_viewer_button_last_page_cb), 
				   (gpointer) viewer);
	g_signal_connect(G_OBJECT(viewer->zoom_in), 
				    "clicked", 
				    G_CALLBACK(pdf_viewer_button_zoom_in_cb), 
				   (gpointer) viewer);
	g_signal_connect(G_OBJECT(viewer->zoom_out), 
				    "clicked", 
				    G_CALLBACK(pdf_viewer_button_zoom_out_cb), 
				   (gpointer) viewer);
	g_signal_connect(G_OBJECT(viewer->zoom_scroll), 
				    "value-changed", 
				    G_CALLBACK(pdf_viewer_spin_zoom_scroll_cb), 
				   (gpointer) viewer);

	g_signal_connect(G_OBJECT(viewer->zoom_fit), 
				   "clicked", 
				    G_CALLBACK(pdf_viewer_button_zoom_fit_cb), 
				   (gpointer) viewer);

	g_signal_connect(G_OBJECT(viewer->zoom_width), 
				    "clicked", 
				    G_CALLBACK(pdf_viewer_button_zoom_width_cb), 
				   (gpointer) viewer);

	g_signal_connect(G_OBJECT(viewer->rotate_right), 
				    "clicked", 
				    G_CALLBACK(pdf_viewer_button_rotate_right_cb), 
				   (gpointer) viewer);
	
	g_signal_connect(G_OBJECT(viewer->rotate_left), 
				    "clicked", 
				    G_CALLBACK(pdf_viewer_button_rotate_left_cb), 
				   (gpointer) viewer);
	
	g_signal_connect(G_OBJECT(viewer->doc_info), 
				    "clicked", 
				    G_CALLBACK(pdf_viewer_button_document_info_cb), 
				   (gpointer) viewer);	
	
	g_signal_connect(G_OBJECT(viewer->doc_index), 
				    "clicked", 
				    G_CALLBACK(pdf_viewer_show_document_index_cb), 
				   (gpointer) viewer);
	g_signal_connect(G_OBJECT(viewer->scrollwin), 
				    "scroll-event", 
				    G_CALLBACK(pdf_viewer_scroll_cb), 
				   (gpointer) viewer);
	g_signal_connect(G_OBJECT(viewer->pdf_view_ebox), 
				    "button_press_event", 
				    G_CALLBACK(pdf_viewer_button_press_events_cb), 
				   (gpointer) viewer);
	g_signal_connect(G_OBJECT(viewer->pdf_view_ebox), 
				    "button_release_event", 
				    G_CALLBACK(pdf_viewer_mouse_scroll_destroy_cb), 
				   (gpointer) viewer);
	g_signal_connect(G_OBJECT(viewer->pdf_view_ebox), 
				    "motion_notify_event", 
				    G_CALLBACK(pdf_viewer_move_events_cb), 
				   (gpointer) viewer);

	viewer->target_filename = NULL;
	viewer->filename = NULL;
	viewer->fsname = NULL;

	return(MimeViewer *) viewer;
}

#undef ADD_BUTTON_TO_TABLE
#undef ADD_SEP_TO_TABLE
#undef BUTTON_H_PADDING
#undef SEP_H_PADDING
		
static MimeViewerFactory pdf_viewer_factory =
{
	content_types,
	0,
	pdf_viewer_create,
};

gint plugin_init(gchar **error)
{
	gchar *gspath = NULL;

	msg = g_strdup_printf(_("This plugin enables the viewing of PDF and PostScript "
				"attachments using the Poppler %s Lib and the gs tool.\n\n"
				"Any feedback is welcome: iwkse@claws-mail.org"
				), poppler_get_version());

	if (!check_plugin_version(MAKE_NUMERIC_VERSION(3,8,1,46),
		    VERSION_NUMERIC, _("PDF Viewer"), error)) return -1;

	if ((gspath = g_find_program_in_path("gs")) == NULL) {
		gchar *pmsg = msg;
		msg = g_strdup_printf(_("Warning: could not find ghostscript binary (gs) required "
					"for %s plugin to process PostScript attachments, only PDF "
					"attachments will be displayed. To enable PostScript "
					"support please install gs program.\n\n%s"
					), _("PDF Viewer"), pmsg);
		g_free(pmsg);
	}
	else {
		g_free(gspath);
	}
	
	mimeview_register_viewer_factory(&pdf_viewer_factory);
	return 0;
}

gboolean plugin_done(void)
{
	g_free(msg);	
	mimeview_unregister_viewer_factory(&pdf_viewer_factory);
	return TRUE;
}

const gchar *plugin_name(void)
{
	return _("PDF Viewer");
}

const gchar *plugin_desc(void)
{
	return msg;
}

const gchar *plugin_type(void)
{
	return "GTK2";
}

const gchar *plugin_licence(void)
{
	return "GPL3+";
}

const gchar *plugin_version(void)
{
	return VERSION;
}

struct PluginFeature *plugin_provides(void)
{
	static struct PluginFeature features[] = 
		{ {PLUGIN_MIMEVIEWER, "application/pdf"},
		  {PLUGIN_MIMEVIEWER, "application/postscript"},
		  {PLUGIN_NOTHING, NULL} };
	return features;
}

