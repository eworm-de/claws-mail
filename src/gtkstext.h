/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

/*
 * Modified by the Sylpheed Team and others 2001-2002.
 */

#ifndef __GTK_STEXT_H__
#define __GTK_STEXT_H__


#include <gdk/gdk.h>
#include <gtk/gtkadjustment.h>
#include <gtk/gtkeditable.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_TYPE_STEXT                  (gtk_stext_get_type ())
#define GTK_STEXT(obj)                  (GTK_CHECK_CAST ((obj), GTK_TYPE_STEXT, GtkSText))
#define GTK_STEXT_CLASS(klass)          (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_STEXT, GtkSTextClass))
#define GTK_IS_STEXT(obj)               (GTK_CHECK_TYPE ((obj), GTK_TYPE_STEXT))
#define GTK_IS_STEXT_CLASS(klass)       (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_STEXT))

typedef struct _GtkSTextFont       GtkSTextFont;
typedef struct _GtkSPropertyMark   GtkSPropertyMark;
typedef struct _GtkSText           GtkSText;
typedef struct _GtkSTextClass      GtkSTextClass;

typedef enum
{
  GTK_STEXT_CURSOR_LINE,
  GTK_STEXT_CURSOR_BLOCK
} GtkSTextCursorType;

struct _GtkSPropertyMark
{
  /* Position in list. */
  GList* property;

  /* Offset into that property. */
  guint offset;

  /* Current index. */
  guint index;
};

struct _GtkSText
{
  GtkEditable editable;

  GdkWindow *text_area;

  GtkAdjustment *hadj;
  GtkAdjustment *vadj;

  GdkGC *gc;

  GdkPixmap* line_wrap_bitmap;
  GdkPixmap* line_arrow_bitmap;

		      /* GAPPED TEXT SEGMENT */

  /* The text, a single segment of text a'la emacs, with a gap
   * where insertion occurs. */
  union { GdkWChar *wc; guchar  *ch; } text;
  /* The allocated length of the text segment. */
  guint text_len;
  /* The gap position, index into address where a char
   * should be inserted. */
  guint gap_position;
  /* The gap size, s.t. *(text + gap_position + gap_size) is
   * the first valid character following the gap. */
  guint gap_size;
  /* The last character position, index into address where a
   * character should be appeneded.  Thus, text_end - gap_size
   * is the length of the actual data. */
  guint text_end;
			/* LINE START CACHE */

  /* A cache of line-start information.  Data is a LineParam*. */
  GList *line_start_cache;
  /* Index to the start of the first visible line. */
  guint first_line_start_index;
  /* The number of pixels cut off of the top line. */
  guint first_cut_pixels;
  /* First visible horizontal pixel. */
  guint first_onscreen_hor_pixel;
  /* First visible vertical pixel. */
  guint first_onscreen_ver_pixel;

			     /* FLAGS */

  /* True iff this buffer is wrapping lines, otherwise it is using a
   * horizontal scrollbar. */
  guint line_wrap : 1;
  guint word_wrap : 1;
 /* If a fontset is supplied for the widget, use_wchar become true,
   * and we use GdkWchar as the encoding of text. */
  guint use_wchar : 1;

  /* Frozen, don't do updates. @@@ fixme */
  guint freeze_count;
			/* TEXT PROPERTIES */

  /* A doubly-linked-list containing TextProperty objects. */
  GList *text_properties;
  /* The end of this list. */
  GList *text_properties_end;
  /* The first node before or on the point along with its offset to
   * the point and the buffer's current point.  This is the only
   * PropertyMark whose index is guaranteed to remain correct
   * following a buffer insertion or deletion. */
  GtkSPropertyMark point;

			  /* SCRATCH AREA */

  union { GdkWChar *wc; guchar *ch; } scratch_buffer;
  guint   scratch_buffer_len;

			   /* SCROLLING */

  gint last_ver_value;

			     /* CURSOR */

  gint             cursor_pos_x;       /* Position of cursor. */
  gint             cursor_pos_y;       /* Baseline of line cursor is drawn on. */
  GtkSPropertyMark cursor_mark;        /* Where it is in the buffer. */
  GdkWChar         cursor_char;        /* Character to redraw. */
  gchar            cursor_char_offset; /* Distance from baseline of the font. */
  gint             cursor_virtual_x;   /* Where it would be if it could be. */
  gint             cursor_drawn_level; /* How many people have undrawn. */

			  /* Current Line */

  GList *current_line;

			   /* Tab Stops */

  GList *tab_stops;
  gint default_tab_width;

  GtkSTextFont *current_font;	/* Text font for current style */

  /* Timer used for auto-scrolling off ends */
  gint timer;
  
  guint button;			/* currently pressed mouse button */
  GdkGC *bg_gc;			/* gc for drawing background pixmap */

  /* SYLPHEED:
   * properties added for different cursor
   */
   gboolean cursor_visible;		/* whether cursor is visible */
   gboolean cursor_timer_on;		/* blinking enabled */
   guint cursor_off_ms;			/* cursor off time */
   guint cursor_on_ms;			/* cursor on time */
   gboolean cursor_state_on;		/* state */
   guint32 cursor_timer_id;		/* blinking timer */
   guint32 cursor_idle_time_timer_id;	/* timer id */

   GtkSTextCursorType cursor_type;

   /* SYLPHEED:
    * properties added for rmargin 
	*/
   gint		wrap_rmargin;					/* right margin */

   /* SYLPHEED:
    * properties added for persist column position when
	* using up and down key */
   gint		persist_column;		
};

struct _GtkSTextClass
{
  GtkEditableClass parent_class;

  void  (*set_scroll_adjustments)   (GtkSText	    *text,
				     GtkAdjustment  *hadjustment,
				     GtkAdjustment  *vadjustment);
};


GtkType    gtk_stext_get_type        (void);
GtkWidget* gtk_stext_new             (GtkAdjustment *hadj,
				      GtkAdjustment *vadj);
void       gtk_stext_set_editable    (GtkSText      *text,
				      gboolean       editable);
void       gtk_stext_set_word_wrap   (GtkSText      *text,
				      gint           word_wrap);
void       gtk_stext_set_line_wrap   (GtkSText      *text,
				      gint           line_wrap);
void       gtk_stext_set_adjustments (GtkSText      *text,
				      GtkAdjustment *hadj,
				      GtkAdjustment *vadj);
void       gtk_stext_set_point       (GtkSText      *text,
				      guint          index);
guint      gtk_stext_get_point       (GtkSText      *text);
guint      gtk_stext_get_length      (GtkSText      *text);
void       gtk_stext_freeze          (GtkSText      *text);
void       gtk_stext_thaw            (GtkSText      *text);
void       gtk_stext_insert          (GtkSText      *text,
				      GdkFont       *font,
				      GdkColor      *fore,
				      GdkColor      *back,
				      const char    *chars,
				      gint           length);
gint       gtk_stext_backward_delete (GtkSText      *text,
				      guint          nchars);
gint       gtk_stext_forward_delete  (GtkSText      *text,
				      guint          nchars);

/* SYLPHEED
 */
void       gtk_stext_set_blink       (GtkSText           *text,
				      gboolean            blinking_on);
void       gtk_stext_set_cursor_type (GtkSText           *text,
				      GtkSTextCursorType  cursor_type);
void       gtk_stext_compact_buffer	(GtkSText *text);

/* these are normally not exported! */
void gtk_stext_move_forward_character    (GtkSText          *text);
void gtk_stext_move_backward_character   (GtkSText          *text);
void gtk_stext_move_forward_word         (GtkSText          *text);
void gtk_stext_move_backward_word        (GtkSText          *text);
void gtk_stext_move_beginning_of_line    (GtkSText          *text);
void gtk_stext_move_end_of_line          (GtkSText          *text);
void gtk_stext_move_next_line            (GtkSText          *text);
void gtk_stext_move_previous_line        (GtkSText          *text);
void gtk_stext_delete_forward_character  (GtkSText          *text);
void gtk_stext_delete_backward_character (GtkSText          *text);
void gtk_stext_delete_forward_word       (GtkSText          *text);
void gtk_stext_delete_backward_word      (GtkSText          *text);
void gtk_stext_delete_line               (GtkSText          *text);
void gtk_stext_delete_to_line_end        (GtkSText          *text);


/* Set the rmargin for the stext. if rmargin is 0, then reset to old 
 * behaviour */
void	   gtk_stext_set_wrap_rmargin (GtkSText *text, gint rmargin);


#define GTK_STEXT_INDEX(t, index)	(((t)->use_wchar) \
	? ((index) < (t)->gap_position ? (t)->text.wc[index] : \
					(t)->text.wc[(index)+(t)->gap_size]) \
	: ((index) < (t)->gap_position ? (t)->text.ch[index] : \
					(t)->text.ch[(index)+(t)->gap_size]))

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_STEXT_H__ */
