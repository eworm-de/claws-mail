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
  * Simple composite widget to provide vertical scrolling, based
  * on GtkRange widget code.
  * Modified by the Sylpheed Team and others 2003
  */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkmisc.h>
#include <gtk/gtk.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkmain.h>

#include "gtkvscrollbutton.h"

#define SCROLL_TIMER_LENGTH  20
#define SCROLL_INITIAL_DELAY 100	/* must hold button this long before ... */
#define SCROLL_LATER_DELAY   20	/* ... it starts repeating at this rate  */
#define SCROLL_DELAY_LENGTH  300


enum {
    ARG_0,
    ARG_ADJUSTMENT
};

static void gtk_vscrollbutton_class_init(GtkVScrollbuttonClass * klass);
static void gtk_vscrollbutton_init(GtkVScrollbutton * vscrollbutton);

GtkType gtk_vscrollbutton_get_type		(void);
GtkWidget *gtk_vscrollbutton_new		(GtkAdjustment 	  *adjustment);

static gint gtk_vscrollbutton_button_release	(GtkWidget 	  *widget,
					     	 GdkEventButton   *event,
					     	 GtkVScrollbutton *scrollbutton);

static void gtk_vscrollbutton_set_adjustment	(GtkVScrollbutton *scrollbutton,
					     	 GtkAdjustment 	  *adjustment);

static gint gtk_vscrollbutton_button_press	(GtkWidget 	  *widget,
					   	 GdkEventButton   *event,
					   	 GtkVScrollbutton *scrollbutton);

static gint gtk_vscrollbutton_button_release	(GtkWidget 	  *widget,
					     	 GdkEventButton   *event,
					     	 GtkVScrollbutton *scrollbutton);

gint gtk_vscrollbutton_scroll		(GtkVScrollbutton *scrollbutton);

static gboolean gtk_vscrollbutton_timer_1st_time(GtkVScrollbutton *scrollbutton);

static void gtk_vscrollbutton_add_timer		(GtkVScrollbutton *scrollbutton);

static void gtk_vscrollbutton_remove_timer	(GtkVScrollbutton *scrollbutton);

static gint gtk_real_vscrollbutton_timer	(GtkVScrollbutton *scrollbutton);

static void gtk_vscrollbutton_set_sensitivity   (GtkAdjustment    *adjustment,
						 GtkVScrollbutton *scrollbutton);

GtkType gtk_vscrollbutton_get_type(void)
{
    static GtkType vscrollbutton_type = 0;

    if (!vscrollbutton_type) {
	static const GtkTypeInfo vscrollbutton_info = {
	    "GtkVScrollbutton",
	    sizeof(GtkVScrollbutton),
	    sizeof(GtkVScrollbuttonClass),
	    (GtkClassInitFunc) gtk_vscrollbutton_class_init,
	    (GtkObjectInitFunc) gtk_vscrollbutton_init,
	    /* reserved_1 */ NULL,
	    /* reserved_2 */ NULL,
	    (GtkClassInitFunc) NULL,
	};

	vscrollbutton_type =
	    gtk_type_unique(GTK_TYPE_VBOX, &vscrollbutton_info);
    }

    return vscrollbutton_type;
}

static void gtk_vscrollbutton_class_init(GtkVScrollbuttonClass *class)
{
}

static void gtk_vscrollbutton_init(GtkVScrollbutton *scrollbutton)
{
    GtkWidget *arrow;
    scrollbutton->upbutton = gtk_button_new();
    scrollbutton->downbutton = gtk_button_new();
    arrow = gtk_arrow_new(GTK_ARROW_UP, GTK_SHADOW_NONE);
    gtk_widget_show(arrow);
    gtk_container_add(GTK_CONTAINER(scrollbutton->upbutton), arrow);
    gtk_widget_set_size_request(scrollbutton->upbutton, -1, 16);
    arrow = gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_NONE);
    gtk_widget_show(arrow);
    gtk_container_add(GTK_CONTAINER(scrollbutton->downbutton), arrow);
    gtk_widget_set_size_request(scrollbutton->downbutton, -1, 16);
    GTK_WIDGET_UNSET_FLAGS(scrollbutton->upbutton, GTK_CAN_FOCUS);
    GTK_WIDGET_UNSET_FLAGS(scrollbutton->downbutton, GTK_CAN_FOCUS);
    gtk_widget_show(scrollbutton->downbutton);
    gtk_widget_show(scrollbutton->upbutton);

    g_signal_connect(G_OBJECT(scrollbutton->upbutton),
		       "button_press_event",
		       G_CALLBACK(gtk_vscrollbutton_button_press),
		       scrollbutton);
    g_signal_connect(G_OBJECT(scrollbutton->downbutton),
		       "button_press_event",
		       G_CALLBACK(gtk_vscrollbutton_button_press),
		       scrollbutton);
    g_signal_connect(G_OBJECT(scrollbutton->upbutton),
		     "button_release_event",
		     G_CALLBACK
		     (gtk_vscrollbutton_button_release), scrollbutton);
    g_signal_connect(G_OBJECT(scrollbutton->downbutton),
		     "button_release_event",
		     G_CALLBACK
		     (gtk_vscrollbutton_button_release), scrollbutton);
    gtk_box_pack_start(GTK_BOX(&scrollbutton->vbox),
		       scrollbutton->upbutton, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(&scrollbutton->vbox),
		     scrollbutton->downbutton, TRUE, TRUE, 0);
    scrollbutton->timer = 0;
}

GtkWidget *gtk_vscrollbutton_new(GtkAdjustment *adjustment)
{
    GtkWidget *vscrollbutton;
    vscrollbutton = GTK_WIDGET(gtk_type_new(gtk_vscrollbutton_get_type()));
    gtk_vscrollbutton_set_adjustment(GTK_VSCROLLBUTTON(vscrollbutton),
				     adjustment);
    g_signal_connect(G_OBJECT(GTK_VSCROLLBUTTON(vscrollbutton)->adjustment),
		       "value_changed",
		       G_CALLBACK
		       (gtk_vscrollbutton_set_sensitivity), vscrollbutton);
    g_signal_connect(G_OBJECT(GTK_VSCROLLBUTTON(vscrollbutton)->adjustment),
		       "changed",
		       G_CALLBACK
		       (gtk_vscrollbutton_set_sensitivity), vscrollbutton);
    return vscrollbutton;
}


void gtk_vscrollbutton_set_adjustment(GtkVScrollbutton *scrollbutton,
				      GtkAdjustment *adjustment)
{
    g_return_if_fail(scrollbutton != NULL);
    g_return_if_fail(GTK_IS_VSCROLLBUTTON(scrollbutton));

    if (!adjustment)
	    adjustment =
	    GTK_ADJUSTMENT(gtk_adjustment_new(0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
    else
	g_return_if_fail(GTK_IS_ADJUSTMENT(adjustment));

    if (scrollbutton->adjustment != adjustment) {
	if (scrollbutton->adjustment) {
	    g_signal_handlers_disconnect_matched(scrollbutton->adjustment,
	    					 G_SIGNAL_MATCH_DATA,
	    					 0, 0, NULL, NULL, 
						 (gpointer) scrollbutton);
	    g_object_unref(G_OBJECT(scrollbutton->adjustment));
	}

	scrollbutton->adjustment = adjustment;
	g_object_ref(G_OBJECT(adjustment));
	gtk_object_sink(G_OBJECT(adjustment));
    }
}

static gint gtk_vscrollbutton_button_press(GtkWidget *widget,
					   GdkEventButton *event,
					   GtkVScrollbutton *scrollbutton)
{
    if (!GTK_WIDGET_HAS_FOCUS(widget))
	gtk_widget_grab_focus(widget);

    if (scrollbutton->button == 0) {
	gtk_grab_add(widget);
	scrollbutton->button = event->button;

	if (widget == scrollbutton->downbutton)
	    scrollbutton->scroll_type = GTK_SCROLL_STEP_FORWARD;
	else
	    scrollbutton->scroll_type = GTK_SCROLL_STEP_BACKWARD;
	gtk_vscrollbutton_scroll(scrollbutton);
	gtk_vscrollbutton_add_timer(scrollbutton);
    }
    return TRUE;
}


static gint gtk_vscrollbutton_button_release(GtkWidget *widget,
					     GdkEventButton *event,
					     GtkVScrollbutton *scrollbutton)
{
    if (!GTK_WIDGET_HAS_FOCUS(widget))
	gtk_widget_grab_focus(widget);

    if (scrollbutton->button == event->button) {
	gtk_grab_remove(widget);

	scrollbutton->button = 0;
	gtk_vscrollbutton_remove_timer(scrollbutton);
	gtk_vscrollbutton_set_sensitivity(scrollbutton->adjustment, scrollbutton);
    }
    return TRUE;
}

gint gtk_vscrollbutton_scroll(GtkVScrollbutton *scrollbutton)
{
    gfloat new_value;
    gint return_val;

    g_return_val_if_fail(scrollbutton != NULL, FALSE);
    g_return_val_if_fail(GTK_IS_VSCROLLBUTTON(scrollbutton), FALSE);

    new_value = scrollbutton->adjustment->value;
    return_val = TRUE;

    switch (scrollbutton->scroll_type) {

    case GTK_SCROLL_STEP_BACKWARD:
	new_value -= scrollbutton->adjustment->step_increment;
	if (new_value <= scrollbutton->adjustment->lower) {
	    new_value = scrollbutton->adjustment->lower;
	    return_val = FALSE;
	    scrollbutton->timer = 0;
	}
	break;

    case GTK_SCROLL_STEP_FORWARD:
	new_value += scrollbutton->adjustment->step_increment;
	if (new_value >=
	    (scrollbutton->adjustment->upper -
	     scrollbutton->adjustment->page_size)) {
	    new_value =
		scrollbutton->adjustment->upper -
		scrollbutton->adjustment->page_size;
	    return_val = FALSE;
	    scrollbutton->timer = 0;
	}
	break;
    
    default:
	break;
    
    }

    if (new_value != scrollbutton->adjustment->value) {
	scrollbutton->adjustment->value = new_value;
	g_signal_emit_by_name(G_OBJECT
				(scrollbutton->adjustment),
				"value_changed");
	gtk_widget_queue_resize(GTK_WIDGET(scrollbutton)); /* ensure resize */
    }

    return return_val;
}

static gboolean
gtk_vscrollbutton_timer_1st_time(GtkVScrollbutton *scrollbutton)
{
    /*
     * If the real timeout function succeeds and the timeout is still set,
     * replace it with a quicker one so successive scrolling goes faster.
     */
    g_object_ref(G_OBJECT(scrollbutton));
    if (scrollbutton->timer) {
	/* We explicitely remove ourselves here in the paranoia
	 * that due to things happening above in the callback
	 * above, we might have been removed, and another added.
	 */
	g_source_remove(scrollbutton->timer);
	scrollbutton->timer = g_timeout_add(SCROLL_LATER_DELAY,
					    (GtkFunction)
					    gtk_real_vscrollbutton_timer,
					    scrollbutton);
    }
    g_object_unref(G_OBJECT(scrollbutton));
    return FALSE;		/* don't keep calling this function */
}


static void gtk_vscrollbutton_add_timer(GtkVScrollbutton *scrollbutton)
{
    g_return_if_fail(scrollbutton != NULL);
    g_return_if_fail(GTK_IS_VSCROLLBUTTON(scrollbutton));

    if (!scrollbutton->timer) {
	scrollbutton->need_timer = TRUE;
	scrollbutton->timer = g_timeout_add(SCROLL_INITIAL_DELAY,
					    (GtkFunction)
					    gtk_vscrollbutton_timer_1st_time,
					    scrollbutton);
    }
}

static void gtk_vscrollbutton_remove_timer(GtkVScrollbutton *scrollbutton)
{
    g_return_if_fail(scrollbutton != NULL);
    g_return_if_fail(GTK_IS_VSCROLLBUTTON(scrollbutton));

    if (scrollbutton->timer) {
	g_source_remove(scrollbutton->timer);
	scrollbutton->timer = 0;
    }
    scrollbutton->need_timer = FALSE;
}

static gint gtk_real_vscrollbutton_timer(GtkVScrollbutton *scrollbutton)
{
    gint return_val;

    GDK_THREADS_ENTER();

    return_val = TRUE;
    if (!scrollbutton->timer) {
	return_val = FALSE;
	if (scrollbutton->need_timer)
	    scrollbutton->timer =
		g_timeout_add(SCROLL_TIMER_LENGTH, 
			      (GtkFunction) gtk_real_vscrollbutton_timer,
			      (gpointer) scrollbutton);
	else {
	    GDK_THREADS_LEAVE();
	    return FALSE;
	}
	scrollbutton->need_timer = FALSE;
    }
    GDK_THREADS_LEAVE();
    return_val = gtk_vscrollbutton_scroll(scrollbutton);
    return return_val;
}

static void gtk_vscrollbutton_set_sensitivity   (GtkAdjustment    *adjustment,
						 GtkVScrollbutton *scrollbutton)
{
	if (!GTK_WIDGET_REALIZED(GTK_WIDGET(scrollbutton))) return;
	if (scrollbutton->button != 0) return; /* not while something is pressed */
	
	gtk_widget_set_sensitive(scrollbutton->upbutton, 
				 (adjustment->value > adjustment->lower));
	gtk_widget_set_sensitive(scrollbutton->downbutton, 
				 (adjustment->value <  (adjustment->upper - adjustment->page_size)));
}
