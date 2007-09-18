/* Claws Mail -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2007 Holger Berndt <hb@claws-mail.org> 
 * and the Claws Mail team
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "printing.h"
#include "image_viewer.h"

#if GTK_CHECK_VERSION(2,10,0) && !defined(USE_GNOMEPRINT)

#include "gtkutils.h"
#include "prefs_common.h"

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <pango/pango.h>
#include <string.h>

typedef struct {
  PangoLayout *layout;
  PangoContext *pango_context;
  char *text;
  GList *page_breaks;
  GtkTextBuffer *buffer;
  gint sel_start;
  gint sel_end;
  GHashTable *images;
  gint img_cnt;
  gboolean print_started;
  gchar *old_print_preview;
} PrintData;


/* callbacks */
static void cb_begin_print(GtkPrintOperation*, GtkPrintContext*, gpointer);
static void cb_draw_page(GtkPrintOperation*, GtkPrintContext*, gint, gpointer);

/* variables */
static GtkPrintSettings *settings   = NULL;
static GtkPageSetup     *page_setup = NULL;

/* other static functions */
static void     printing_layout_set_text_attributes(PrintData*, GtkPrintContext *);
static gboolean printing_is_pango_gdk_color_equal(PangoColor*, GdkColor*); 
static gint     printing_text_iter_get_offset_bytes(PrintData *, const GtkTextIter*);

static gboolean claws_draw_page(GtkPrintOperation *op, GtkPrintContext *context, gint page_nr, gpointer user_data);

#define PREVIEW_SCALE 72
static void preview_destroy (GtkWindow *window, GtkPrintOperationPreview *preview)
{
  gtk_print_operation_preview_end_preview (preview);
}

static gboolean preview_close(GtkWidget *widget, GdkEventAny *event,
				 gpointer data)
{
	if (event->type == GDK_KEY_PRESS)
		if (((GdkEventKey *)event)->keyval != GDK_Escape)
			return FALSE;

	gtk_widget_destroy(widget);
	return FALSE;
}

static gboolean cb_preview (GtkPrintOperation        *operation,
                                     GtkPrintOperationPreview *preview,
                                     GtkPrintContext          *context,
                                     GtkWindow                *parent,
                                     PrintData                *print_data)
{
  GtkPageSetup    *page_setup = gtk_print_context_get_page_setup (context);
  GtkPaperSize    *paper_size;
  gdouble          paper_width;
  gdouble          paper_height;
  gdouble          top_margin;
  gdouble          bottom_margin;
  gdouble          left_margin;
  gdouble          right_margin;
  gint             preview_width;
  gint             preview_height;
  gint num_pages = 0, i = 0;
  cairo_t         *cr;
  cairo_surface_t *surface;
  GtkPageOrientation orientation;
  cairo_status_t   status;
  gchar           *fname;
  GtkWidget *dialog = NULL;
  GtkWidget *image, *notebook;
  GSList *pages = NULL, *cur;

  paper_size      = gtk_page_setup_get_paper_size    (page_setup);
  paper_width     = gtk_paper_size_get_width         (paper_size, GTK_UNIT_INCH);
  paper_height    = gtk_paper_size_get_height        (paper_size,  GTK_UNIT_INCH);
  top_margin      = gtk_page_setup_get_top_margin    (page_setup, GTK_UNIT_INCH);
  bottom_margin   = gtk_page_setup_get_bottom_margin (page_setup, GTK_UNIT_INCH);
  left_margin     = gtk_page_setup_get_left_margin   (page_setup, GTK_UNIT_INCH);
  right_margin    = gtk_page_setup_get_right_margin  (page_setup, GTK_UNIT_INCH);

  /* the print context does not have the page orientation, it is transformed */
  orientation     = gtk_page_setup_get_orientation (page_setup);

  if (orientation == GTK_PAGE_ORIENTATION_PORTRAIT ||
      orientation == GTK_PAGE_ORIENTATION_REVERSE_PORTRAIT)
    {
      preview_width  = PREVIEW_SCALE * paper_width;
      preview_height = PREVIEW_SCALE * paper_height;
    }
  else
    {
      preview_width  = PREVIEW_SCALE * paper_height;
      preview_height = PREVIEW_SCALE * paper_width;
    }


  num_pages = 1;  
  for (i = 0; i < num_pages; i++) {
    surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24,
                                              preview_width, preview_height);

    if (CAIRO_STATUS_SUCCESS != cairo_surface_status (surface)) {
      g_message ("Unable to create preview (not enough memory?)");
      return TRUE;
    }

    cr = cairo_create (surface);
    gtk_print_context_set_cairo_context (context, cr, PREVIEW_SCALE, PREVIEW_SCALE);

    /* fill page with white */
    cairo_set_source_rgb (cr, 1, 1, 1);
    cairo_new_path (cr);
    cairo_rectangle (cr, 0, 0, preview_width, preview_height);
    cairo_fill (cr);

    cairo_translate (cr, left_margin * PREVIEW_SCALE, right_margin * PREVIEW_SCALE);

    claws_draw_page (operation, context, i, print_data);
    num_pages = g_list_length(print_data->page_breaks) + 1;

    fname = get_tmp_file();
    status = cairo_surface_write_to_png (surface, fname);
    cairo_destroy (cr);
    cairo_surface_destroy (surface);
    if (status == CAIRO_STATUS_SUCCESS) {
	image = gtk_image_new_from_file (fname);
	g_unlink (fname);
	g_free (fname);
	pages = g_slist_prepend(pages, image);
	debug_print("added one page\n");
    }
  }
  pages = g_slist_reverse(pages);
  
  dialog = gtkut_window_new(GTK_WINDOW_TOPLEVEL, "print_preview");
  gtk_window_set_title(GTK_WINDOW(dialog), _("Print preview"));
  notebook = gtk_notebook_new();
  gtk_container_add(GTK_CONTAINER(dialog), notebook);
  i = 0;
  for (cur = pages; cur; cur = cur->next) {
    image = (GtkImage *)cur->data;
    if (gtk_print_operation_preview_is_selected(preview, i)) {
      debug_print("page %d sel\n", i);
      gtk_notebook_append_page(GTK_NOTEBOOK(notebook), image, NULL);
    }
    i++;
  }
  g_slist_free(pages);
  
  gtk_widget_show_all(dialog);
  
  g_signal_connect (dialog, "destroy",
                    G_CALLBACK (preview_destroy), preview);
  g_signal_connect (dialog, "key_press_event",
                    G_CALLBACK (preview_close), preview);
  
  return TRUE;
}

void printing_print(GtkTextView *text_view, GtkWindow *parent, gint sel_start, gint sel_end)
{
  GtkPrintOperation *op;
  GtkPrintOperationResult res;
  PrintData *print_data;
  GtkTextIter start, end;
  GtkTextBuffer *buffer;

  op = gtk_print_operation_new();

  print_data = g_new0(PrintData,1);

  print_data->images = g_hash_table_new(g_direct_hash, g_direct_equal);
  print_data->pango_context=gtk_widget_get_pango_context(GTK_WIDGET(text_view));

  /* get text */
  buffer = gtk_text_view_get_buffer(text_view);
  print_data->buffer = buffer;
  print_data->sel_start = sel_start;
  print_data->sel_end = sel_end;
  if (print_data->sel_start < 0 || print_data->sel_end <= print_data->sel_start) {
    gtk_text_buffer_get_start_iter(buffer, &start);
    gtk_text_buffer_get_end_iter(buffer, &end);
  } else {
    gtk_text_buffer_get_iter_at_offset(buffer, &start, print_data->sel_start);
    gtk_text_buffer_get_iter_at_offset(buffer, &end, print_data->sel_end);
  }

  print_data->text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

  if (settings == NULL) {
    settings = gtk_print_settings_new();
    gtk_print_settings_set_use_color(settings, prefs_common.print_use_color);
    gtk_print_settings_set_collate(settings, prefs_common.print_use_collate);
    gtk_print_settings_set_reverse(settings, prefs_common.print_use_reverse);
    gtk_print_settings_set_duplex(settings, prefs_common.print_use_duplex);
  }
  if (page_setup == NULL) {
    GtkPaperSize *paper = gtk_paper_size_new(prefs_common.print_paper_type);
    page_setup = gtk_page_setup_new();
    gtk_page_setup_set_paper_size(page_setup, paper);
    gtk_paper_size_free(paper);
    gtk_page_setup_set_orientation(page_setup, prefs_common.print_paper_orientation);
  }
  
  /* Config for printing */
  gtk_print_operation_set_print_settings(op, settings);
  gtk_print_operation_set_default_page_setup(op, page_setup);

  /* signals */
  g_signal_connect(op, "begin_print", G_CALLBACK(cb_begin_print), print_data);
  g_signal_connect(op, "draw_page", G_CALLBACK(cb_draw_page), print_data);
  g_signal_connect(op, "preview", G_CALLBACK(cb_preview), print_data);

  /* Start printing process */
  res = gtk_print_operation_run(op, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
				parent, NULL);

  if(res == GTK_PRINT_OPERATION_RESULT_ERROR) {
    GError *error = NULL;
    gtk_print_operation_get_error(op, &error);
    debug_print("Error printing message: %s",
	    error ? error->message : "no details");
  }
  else if(res == GTK_PRINT_OPERATION_RESULT_APPLY) {
    /* store settings for next printing session */
    if(settings != NULL)
      g_object_unref(settings);
    settings = g_object_ref(gtk_print_operation_get_print_settings(op));
    prefs_common.print_use_color = gtk_print_settings_get_use_color(settings);
    prefs_common.print_use_collate = gtk_print_settings_get_collate(settings);
    prefs_common.print_use_reverse = gtk_print_settings_get_reverse(settings);
    prefs_common.print_use_duplex = gtk_print_settings_get_duplex(settings);
  }

  g_hash_table_destroy(print_data->images);
  if(print_data->text)
    g_free(print_data->text);
  g_list_free(print_data->page_breaks);
  if(print_data->layout)
    g_object_unref(print_data->layout);

  if (print_data->old_print_preview) {
    g_object_set(gtk_settings_get_default(),
    	          "gtk-print-preview-command", print_data->old_print_preview, NULL);
    g_free(print_data->old_print_preview);
    print_data->old_print_preview = NULL;
  }

  g_free(print_data);

  g_object_unref(op);
  debug_print("printing_print finished\n");
}

void printing_page_setup(GtkWindow *parent)
{
  GtkPageSetup *new_page_setup;

  if(settings == NULL) {
    settings = gtk_print_settings_new();
    gtk_print_settings_set_use_color(settings, prefs_common.print_use_color);
    gtk_print_settings_set_collate(settings, prefs_common.print_use_collate);
    gtk_print_settings_set_reverse(settings, prefs_common.print_use_reverse);
    gtk_print_settings_set_duplex(settings, prefs_common.print_use_duplex);
  }
  if (page_setup == NULL) {
    GtkPaperSize *paper = gtk_paper_size_new(prefs_common.print_paper_type);
    page_setup = gtk_page_setup_new();
    gtk_page_setup_set_paper_size(page_setup, paper);
    gtk_paper_size_free(paper);
    gtk_page_setup_set_orientation(page_setup, prefs_common.print_paper_orientation);
  }

  new_page_setup = gtk_print_run_page_setup_dialog(parent,page_setup,settings);

  if(page_setup)
    g_object_unref(page_setup);

  page_setup = new_page_setup;
  
  g_free(prefs_common.print_paper_type);
  prefs_common.print_paper_type = g_strdup(gtk_paper_size_get_name(
  		gtk_page_setup_get_paper_size(page_setup)));
  prefs_common.print_paper_orientation = gtk_page_setup_get_orientation(page_setup);
}

static void cb_begin_print(GtkPrintOperation *op, GtkPrintContext *context,
			   gpointer user_data)
{
  double width, height;
  int num_lines;
  double page_height;
  GList *page_breaks;
  PrintData *print_data;
  PangoFontDescription *desc;
  int start, ii;
  PangoLayoutIter *iter;
  double start_pos;
  double line_height =0.;

  print_data = (PrintData*) user_data;

  if (print_data->print_started)
    return;
  debug_print("Preparing print job...\n");
	
  width  = gtk_print_context_get_width(context);
  height = gtk_print_context_get_height(context);

  if (print_data->layout == NULL)
    print_data->layout = gtk_print_context_create_pango_layout(context);
	
  if(prefs_common.use_different_print_font)
    desc = pango_font_description_from_string(prefs_common.printfont);
  else
    desc = pango_font_description_copy(
		pango_context_get_font_description(print_data->pango_context));

  pango_layout_set_font_description(print_data->layout, desc);
  pango_font_description_free(desc);

  pango_layout_set_width(print_data->layout, width * PANGO_SCALE);
  pango_layout_set_text(print_data->layout, print_data->text, -1);

  printing_layout_set_text_attributes(print_data, context);

  num_lines = pango_layout_get_line_count(print_data->layout);

  page_breaks = NULL;
  page_height = 0;
  start = 0;
  ii = 0;
  start_pos = 0.;
  iter = pango_layout_get_iter(print_data->layout);
  do {
    PangoRectangle logical_rect;
    PangoLayoutLine *line;
    PangoAttrShape *attr = NULL;
    int baseline;

    if(ii >= start) {
      line = pango_layout_iter_get_line(iter);

      pango_layout_iter_get_line_extents(iter, NULL, &logical_rect);
      baseline = pango_layout_iter_get_baseline(iter);

      if ((attr = g_hash_table_lookup(print_data->images, GINT_TO_POINTER(pango_layout_iter_get_index(iter)))) != NULL) {
        line_height = (double)gdk_pixbuf_get_height(GDK_PIXBUF(attr->data));
      }	else {
        line_height = ((double)logical_rect.height) / PANGO_SCALE;
      }
    }
    if((page_height + line_height) > height) {
      page_breaks = g_list_prepend(page_breaks, GINT_TO_POINTER(ii));
      page_height = 0;
    }

    page_height += line_height;
    ii++;
  } while(ii < num_lines && pango_layout_iter_next_line(iter));
  pango_layout_iter_free(iter);

  page_breaks = g_list_reverse(page_breaks);
  gtk_print_operation_set_n_pages(op, g_list_length(page_breaks) + 1);
	
  print_data->page_breaks = page_breaks;

  debug_print("Starting print job...\n");
  print_data->print_started = TRUE;
}

static cairo_surface_t *pixbuf_to_surface(GdkPixbuf *pixbuf)
{
	  cairo_surface_t *surface;
	  cairo_format_t format;
	  static const cairo_user_data_key_t key;
	  guchar *pixels = g_malloc(
	  			4*
				gdk_pixbuf_get_width(pixbuf)*
				gdk_pixbuf_get_height(pixbuf));
          guchar *src_pixels = gdk_pixbuf_get_pixels (pixbuf);
	  gint width = gdk_pixbuf_get_width(pixbuf);
	  gint height = gdk_pixbuf_get_height(pixbuf);
          gint nchans = gdk_pixbuf_get_n_channels (pixbuf);
	  gint stride = gdk_pixbuf_get_rowstride (pixbuf);
	  gint j;

	  if (nchans == 3)
	    format = CAIRO_FORMAT_RGB24;
	  else
	    format = CAIRO_FORMAT_ARGB32;
	  surface = cairo_image_surface_create_for_data (pixels, 	 
						format, width, height, 4*width);
          cairo_surface_set_user_data (surface, &key, 
	  	pixels, (cairo_destroy_func_t)g_free);

	  for (j = height; j; j--) {
		guchar *p = src_pixels;
		guchar *q = pixels;

		if (nchans == 3) {
			guchar *end = p + 3 * width;

			while (p < end) {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
				q[0] = p[2];
				q[1] = p[1];
				q[2] = p[0];
#else
				q[1] = p[0];
				q[2] = p[1];
				q[3] = p[2];
#endif
				p += 3;
				q += 4;
			}
		} else {
			guchar *end = p + 4 * width;
			guint t1,t2,t3;

#define MULT(d,c,a,t) G_STMT_START { t = c * a + 0x7f; d = ((t >> 8) + t) >> 8; } G_STMT_END

			while (p < end) {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
				MULT(q[0], p[2], p[3], t1);
				MULT(q[1], p[1], p[3], t2);
				MULT(q[2], p[0], p[3], t3);
				q[3] = p[3];
#else
				q[0] = p[3];
				MULT(q[1], p[0], p[3], t1);
				MULT(q[2], p[1], p[3], t2);
				MULT(q[3], p[2], p[3], t3);
#endif

				p += 4;
				q += 4;
			}

#undef MULT
		}

		src_pixels += stride;
		pixels += 4 * width;
	}
		
	return surface;
}

static gboolean claws_draw_page(GtkPrintOperation *op, GtkPrintContext *context, gint page_nr, gpointer user_data)
{
  cairo_t *cr;
  PrintData *print_data;
  int start, end, ii;
  GList *pagebreak;
  PangoLayoutIter *iter;
  double start_pos;
  gboolean notlast = TRUE;
  print_data = (PrintData*) user_data;

  if (print_data->print_started == FALSE)
    cb_begin_print(op, context, print_data);
  if (page_nr == 0) {
    start = 0;
  } else {
    pagebreak = g_list_nth(print_data->page_breaks, page_nr - 1);
    start = GPOINTER_TO_INT(pagebreak->data);
  }

  pagebreak = g_list_nth(print_data->page_breaks, page_nr);
  if(pagebreak == NULL)
    end = pango_layout_get_line_count(print_data->layout);
  else
    end = GPOINTER_TO_INT(pagebreak->data);

  cr = gtk_print_context_get_cairo_context(context);
  cairo_set_source_rgb(cr, 0., 0., 0.);

  ii = 0;
  start_pos = 0.;
  iter = pango_layout_get_iter(print_data->layout);
  do {
    PangoRectangle logical_rect;
    PangoLayoutLine *line;
    PangoAttrShape *attr = NULL;
    int baseline;

    if(ii >= start) {
      line = pango_layout_iter_get_line(iter);

      pango_layout_iter_get_line_extents(iter, NULL, &logical_rect);
      baseline = pango_layout_iter_get_baseline(iter);

      if(ii == start)
	start_pos = ((double)logical_rect.y) / PANGO_SCALE;
      
      cairo_move_to(cr,
		    ((double)logical_rect.x) / PANGO_SCALE,
		    ((double)baseline) / PANGO_SCALE - start_pos);
		    
      if ((attr = g_hash_table_lookup(print_data->images, GINT_TO_POINTER(pango_layout_iter_get_index(iter)))) != NULL) {
	  cairo_surface_t *surface;

	  surface = pixbuf_to_surface(GDK_PIXBUF(attr->data));
          cairo_set_source_surface (cr, surface, 
		((double)logical_rect.x) / PANGO_SCALE, 
		((double)baseline) / PANGO_SCALE - start_pos);
	  cairo_paint (cr);
	  cairo_surface_destroy (surface);
	  g_object_unref(GDK_PIXBUF(attr->data));
      }	else {
        pango_cairo_show_layout_line(cr, line);
      }
    }
    ii++;
  } while(ii < end && (notlast = pango_layout_iter_next_line(iter)));
  pango_layout_iter_free(iter);
  return TRUE;
}

static void cb_draw_page(GtkPrintOperation *op, GtkPrintContext *context,
			 int page_nr, gpointer user_data)
{
  claws_draw_page(op, context, page_nr, user_data);
  debug_print("Sent page %d to printer\n", page_nr+1);
}

static void printing_layout_set_text_attributes(PrintData *print_data, GtkPrintContext *context)
{
  GtkTextIter iter;
  PangoAttrList *attr_list;
  PangoAttribute *attr;
  GSList *open_attrs, *attr_walk;

  attr_list = pango_attr_list_new();
  if (print_data->sel_start < 0 || print_data->sel_end <= print_data->sel_start) {
    gtk_text_buffer_get_start_iter(print_data->buffer, &iter);
  } else {
    gtk_text_buffer_get_iter_at_offset(print_data->buffer, &iter, print_data->sel_start);
  }

  open_attrs = NULL;
  do {
    gboolean fg_set, bg_set, under_set, strike_set;
    GSList *tags, *tag_walk;
    GtkTextTag *tag;
    GdkColor *color;
    PangoUnderline underline;
    gboolean strikethrough;
    GdkPixbuf *image;

    if (prefs_common.print_imgs && (image = gtk_text_iter_get_pixbuf(&iter)) != NULL) {
      PangoRectangle rect = {0, 0, 0, 0};
      gint startpos = printing_text_iter_get_offset_bytes(print_data, &iter);
      gint h = gdk_pixbuf_get_height(image);
      gint w = gdk_pixbuf_get_width(image);
      gint a_h = gtk_print_context_get_height(context);
      gint a_w = gtk_print_context_get_width(context);
      gint r_h, r_w;
      GdkPixbuf *scaled = NULL;
      image_viewer_get_resized_size(w, h, a_w, a_h, &r_w, &r_h);
      rect.x = 0;
      rect.y = 0;
      rect.width = r_w * PANGO_SCALE;
      rect.height = r_h * PANGO_SCALE;
      
      scaled = gdk_pixbuf_scale_simple(image, r_w, r_h, GDK_INTERP_BILINEAR);
      attr = pango_attr_shape_new_with_data (&rect, &rect,
               scaled, NULL, NULL);
      attr->start_index = startpos;
      attr->end_index = startpos+1;
      pango_attr_list_insert(attr_list, attr);
      g_hash_table_insert(print_data->images, GINT_TO_POINTER(startpos), attr);
      print_data->img_cnt++;
    }

    if(gtk_text_iter_ends_tag(&iter, NULL)) {
      PangoAttrColor *attr_color;
      PangoAttrInt   *attr_int;

      tags = gtk_text_iter_get_toggled_tags(&iter, FALSE);
      for(tag_walk = tags; tag_walk != NULL; tag_walk = tag_walk->next) {
	gboolean found;

	tag = GTK_TEXT_TAG(tag_walk->data);
	g_object_get(G_OBJECT(tag),
		     "background-set", &bg_set,
		     "foreground-set", &fg_set,
		     "underline-set",&under_set,
		     "strikethrough-set", &strike_set,
		     NULL);

	if(fg_set) {
	  found = FALSE;
	  for(attr_walk = open_attrs; attr_walk != NULL; attr_walk = attr_walk->next) {
	    attr = (PangoAttribute*)attr_walk->data;
	    if(attr->klass->type == PANGO_ATTR_FOREGROUND) {
	      attr_color = (PangoAttrColor*) attr;
	      g_object_get(G_OBJECT(tag), "foreground_gdk", &color, NULL);
	      if(printing_is_pango_gdk_color_equal(&(attr_color->color), color)) {
		attr->end_index = printing_text_iter_get_offset_bytes(print_data, &iter);
		pango_attr_list_insert(attr_list, attr);
		found = TRUE;
		open_attrs = g_slist_delete_link(open_attrs, attr_walk);
		break;
	      }
	    }
	  }
	  if(!found)
	    debug_print("Error generating attribute list.\n");
	}

	if(bg_set) {
	  found = FALSE;
	  for(attr_walk = open_attrs; attr_walk != NULL; attr_walk = attr_walk->next) {
	    attr = (PangoAttribute*)attr_walk->data;
	    if(attr->klass->type == PANGO_ATTR_BACKGROUND) {
	      attr_color = (PangoAttrColor*) attr;
	      g_object_get(G_OBJECT(tag), "background-gdk", &color, NULL);
	      if(printing_is_pango_gdk_color_equal(&(attr_color->color), color)) {
		attr->end_index = printing_text_iter_get_offset_bytes(print_data, &iter);
		pango_attr_list_insert(attr_list, attr);
		found = TRUE;
		open_attrs = g_slist_delete_link(open_attrs, attr_walk);
		break;
	      }
	    }
	  }
	  if(!found)
	    debug_print("Error generating attribute list.\n");
	}

	if(under_set) {
	  found = FALSE;
	  for(attr_walk = open_attrs; attr_walk != NULL; attr_walk = attr_walk->next) {
	    attr = (PangoAttribute*)attr_walk->data;
	    if(attr->klass->type == PANGO_ATTR_UNDERLINE) {
	      attr_int = (PangoAttrInt*)attr;
	      g_object_get(G_OBJECT(tag), "underline", &underline, NULL);
	      if(attr_int->value == underline) {
		attr->end_index = printing_text_iter_get_offset_bytes(print_data, &iter);
		pango_attr_list_insert(attr_list, attr);
		found = TRUE;
		open_attrs = g_slist_delete_link(open_attrs, attr_walk);
		break;
	      }
	    }
	  }
	  if(!found)
	    debug_print("Error generating attribute list.\n");
	}

	if(strike_set) {
	  found = FALSE;
	  for(attr_walk = open_attrs; attr_walk != NULL; attr_walk = attr_walk->next) {
	    attr = (PangoAttribute*)attr_walk->data;
	    if(attr->klass->type == PANGO_ATTR_STRIKETHROUGH) {
	      attr_int = (PangoAttrInt*)attr;
	      g_object_get(G_OBJECT(tag), "strikethrough", &strikethrough, NULL);
	      if(attr_int->value == strikethrough) {
		attr->end_index = printing_text_iter_get_offset_bytes(print_data, &iter);
		pango_attr_list_insert(attr_list, attr);
		found = TRUE;
		open_attrs = g_slist_delete_link(open_attrs, attr_walk);
		break;
	      }
	    }
	  }
	  if(!found)
	    debug_print("Error generating attribute list.\n");
	}

      }
      g_slist_free(tags);
    }

    if(gtk_text_iter_begins_tag(&iter, NULL)) {
      tags = gtk_text_iter_get_toggled_tags(&iter, TRUE);
      for(tag_walk = tags; tag_walk != NULL; tag_walk = tag_walk->next) {
	tag = GTK_TEXT_TAG(tag_walk->data);
	g_object_get(G_OBJECT(tag),
		     "background-set", &bg_set,
		     "foreground-set", &fg_set,
		     "underline-set", &under_set,
		     "strikethrough-set", &strike_set,
		     NULL);
	if(fg_set) {
	  g_object_get(G_OBJECT(tag), "foreground-gdk", &color, NULL);
	  attr = pango_attr_foreground_new(color->red,color->green,color->blue);
	  attr->start_index = printing_text_iter_get_offset_bytes(print_data, &iter);
	  open_attrs = g_slist_prepend(open_attrs, attr);
	}
	if(bg_set) {
	  g_object_get(G_OBJECT(tag), "background-gdk", &color, NULL);
	  attr = pango_attr_background_new(color->red,color->green,color->blue);
	  attr->start_index = printing_text_iter_get_offset_bytes(print_data, &iter);
	  open_attrs = g_slist_prepend(open_attrs, attr);
	}
	if(under_set) {
	  g_object_get(G_OBJECT(tag), "underline", &underline, NULL);
	  attr = pango_attr_underline_new(underline);
	  attr->start_index = printing_text_iter_get_offset_bytes(print_data, &iter);
	  open_attrs = g_slist_prepend(open_attrs, attr);	  
	}
	if(strike_set) {
	  g_object_get(G_OBJECT(tag), "strikethrough", &strikethrough, NULL);
	  attr = pango_attr_strikethrough_new(strikethrough);
	  attr->start_index = printing_text_iter_get_offset_bytes(print_data, &iter);
	  open_attrs = g_slist_prepend(open_attrs, attr);	  
	}
      }
      g_slist_free(tags);
    }
    
  } while(!gtk_text_iter_is_end(&iter) && gtk_text_iter_forward_to_tag_toggle(&iter, NULL));
	  
  /* close all open attributes */
  for(attr_walk = open_attrs; attr_walk != NULL; attr_walk = attr_walk->next) {
    attr = (PangoAttribute*) attr_walk->data;
    attr->end_index = printing_text_iter_get_offset_bytes(print_data, &iter);
    pango_attr_list_insert(attr_list, attr);
  }
  g_slist_free(open_attrs);

  pango_layout_set_attributes(print_data->layout, attr_list);
  pango_attr_list_unref(attr_list);
}

static gboolean printing_is_pango_gdk_color_equal(PangoColor *p, GdkColor *g)
{
  return ((p->red == g->red) && (p->green == g->green) && (p->blue == g->blue));
}

/* Pango has it's attribute in bytes, but GtkTextIter gets only an offset
 * in characters, so here we're returning an offset in bytes. 
 */
static gint printing_text_iter_get_offset_bytes(PrintData *print_data, const GtkTextIter *iter)
{
  gint off_chars;
  gint off_bytes;
  gchar *text;
  GtkTextIter start;

  off_chars = gtk_text_iter_get_offset(iter);
  if (print_data->sel_start < 0 || print_data->sel_end <= print_data->sel_start) {
    gtk_text_buffer_get_start_iter(gtk_text_iter_get_buffer(iter), &start);
  } else {
    gtk_text_buffer_get_iter_at_offset(gtk_text_iter_get_buffer(iter), &start, print_data->sel_start);
  }
  text = gtk_text_iter_get_text(&start, iter);
  off_bytes = strlen(text);
  g_free(text);
  return off_bytes;
}

#endif /* GTK+ >= 2.10.0 */
