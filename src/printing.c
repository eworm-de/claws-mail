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

#if GTK_CHECK_VERSION(2,10,0) && !defined(USE_GNOMEPRINT)

#include "prefs_common.h"

#include <glib/gi18n.h>
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
} PrintData;


/* callbacks */
static void cb_begin_print(GtkPrintOperation*, GtkPrintContext*, gpointer);
static void cb_draw_page(GtkPrintOperation*, GtkPrintContext*, gint, gpointer);

/* variables */
static GtkPrintSettings *settings   = NULL;
static GtkPageSetup     *page_setup = NULL;

/* other static functions */
static void     printing_layout_set_text_attributes(PrintData*);
static gboolean printing_is_pango_gdk_color_equal(PangoColor*, GdkColor*); 
static gint     printing_text_iter_get_offset_bytes(PrintData *, const GtkTextIter*);

void printing_print(GtkTextView *text_view, GtkWindow *parent, gint sel_start, gint sel_end)
{
  GtkPrintOperation *op;
  GtkPrintOperationResult res;
  PrintData *print_data;
  GtkTextIter start, end;
  GtkTextBuffer *buffer;

  op = gtk_print_operation_new();

  print_data = g_new0(PrintData,1);

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

  /* Config for printing */
  if(settings != NULL)
    gtk_print_operation_set_print_settings(op, settings);
  if(page_setup != NULL)
    gtk_print_operation_set_default_page_setup(op, page_setup);

  /* signals */
  g_signal_connect(op, "begin_print", G_CALLBACK(cb_begin_print), print_data);
  g_signal_connect(op, "draw_page", G_CALLBACK(cb_draw_page), print_data);

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
  }

  if(print_data->text)
    g_free(print_data->text);
  g_list_free(print_data->page_breaks);
  if(print_data->layout)
    g_object_unref(print_data->layout);
  g_free(print_data);

  g_object_unref(op);
  debug_print("printing_print finished\n");
}

void printing_page_setup(GtkWindow *parent)
{
  GtkPageSetup *new_page_setup;

  if(settings == NULL)
    settings = gtk_print_settings_new();

  new_page_setup = gtk_print_run_page_setup_dialog(parent,page_setup,settings);

  if(page_setup)
    g_object_unref(page_setup);

  page_setup = new_page_setup;
}

static void cb_begin_print(GtkPrintOperation *op, GtkPrintContext *context,
			   gpointer user_data)
{
  double width, height;
  int num_lines, line;
  double page_height;
  GList *page_breaks;
  PrintData *print_data;
  PangoLayoutLine *layout_line;
  PangoFontDescription *desc;

  debug_print("Preparing print job...\n");
	
  print_data = (PrintData*) user_data;
  width  = gtk_print_context_get_width(context);
  height = gtk_print_context_get_height(context);

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

  printing_layout_set_text_attributes(print_data);

  num_lines = pango_layout_get_line_count(print_data->layout);

  page_breaks = NULL;
  page_height = 0;
  for(line = 0; line < num_lines; line++) {
    PangoRectangle ink_rect, logical_rect;
    double line_height;

#define PANGO_HAS_READONLY 0
#if defined(PANGO_VERSION_CHECK)
#if PANGO_VERSION_CHECK(1,16,0)
#define PANGO_HAS_READONLY 1
#endif
#endif

#if PANGO_HAS_READONLY
    layout_line = pango_layout_get_line_readonly(print_data->layout, line);
#else
    layout_line = pango_layout_get_line(print_data->layout, line);
#endif
    pango_layout_line_get_extents(layout_line, &ink_rect, &logical_rect);

    line_height = ((double)logical_rect.height) / PANGO_SCALE;

    /* TODO: Check if there is a pixbuf in that line */
    
    if((page_height + line_height) > height) {
      page_breaks = g_list_prepend(page_breaks, GINT_TO_POINTER(line));
      page_height = 0;
    }

    page_height += line_height;
  }

  page_breaks = g_list_reverse(page_breaks);
  gtk_print_operation_set_n_pages(op, g_list_length(page_breaks) + 1);
	
  print_data->page_breaks = page_breaks;

  debug_print("Starting print job...\n");
}

static void cb_draw_page(GtkPrintOperation *op, GtkPrintContext *context,
			 int page_nr, gpointer user_data)
{
  cairo_t *cr;
  PrintData *print_data;
  int start, end, ii;
  GList *pagebreak;
  PangoLayoutIter *iter;
  double start_pos;
	
  print_data = (PrintData*) user_data;

  if (page_nr == 0)
    start = 0;
  else {
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
      pango_cairo_show_layout_line(cr, line);
    }
    ii++;
  } while(ii < end && pango_layout_iter_next_line(iter));

  pango_layout_iter_free(iter);
	
  debug_print("Sent page %d to printer\n", page_nr+1);
}

static void printing_layout_set_text_attributes(PrintData *print_data)
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
  text = gtk_text_iter_get_slice(&start, iter);
  off_bytes = strlen(text);
  g_free(text);
  return off_bytes;
}

#endif /* GTK+ >= 2.10.0 */
