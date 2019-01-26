/*
 * Claws Mail -- A GTK+ based, lightweight, and fast e-mail client
 * Copyright(C) 1999-2015 the Claws Mail Team
 * == Fancy Plugin ==
 * This file Copyright (C) 2009-2015 Salvatore De Paolis
 * <iwkse@claws-mail.org> and the Claws Mail Team
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write tothe Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#include "claws-features.h"
#endif

#include <glib.h>
#include <glib/gstdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <curl/curl.h>
#include <gdk/gdk.h>

#include "utils.h"

#include "litehtml/litehtml.h"

#include "lh_prefs.h"
#include "lh_widget.h"
#include "lh_widget_wrapped.h"
#include "http.h"

char master_css[] = {
#include "css.inc"
};

static gboolean expose_event_cb(GtkWidget *widget, GdkEvent *event,
		gpointer user_data);
static void size_allocate_cb(GtkWidget *widget, GdkRectangle *allocation,
		gpointer user_data);
static gboolean button_press_event(GtkWidget *widget, GdkEventButton *event,
		gpointer user_data);
static gboolean motion_notify_event(GtkWidget *widget, GdkEventButton *event,
        gpointer user_data);
static gboolean button_release_event(GtkWidget *widget, GdkEventButton *event,
        gpointer user_data);

lh_widget::lh_widget()
{
	/* scrolled window */
	m_scrolled_window = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(m_scrolled_window),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	g_signal_connect(m_scrolled_window, "size-allocate",
			G_CALLBACK(size_allocate_cb), this);

	/* viewport */
	GtkScrolledWindow *scw = GTK_SCROLLED_WINDOW(m_scrolled_window);
	m_viewport = gtk_viewport_new(
			gtk_scrolled_window_get_hadjustment(scw),
			gtk_scrolled_window_get_vadjustment(scw));
	gtk_container_add(GTK_CONTAINER(m_scrolled_window), m_viewport);

	/* drawing area */
	m_drawing_area = gtk_drawing_area_new();
	gtk_container_add(GTK_CONTAINER(m_viewport), m_drawing_area);
	g_signal_connect(m_drawing_area, "expose-event",
			G_CALLBACK(expose_event_cb), this);
	g_signal_connect(m_drawing_area, "motion_notify_event",
			G_CALLBACK(motion_notify_event), this);
	g_signal_connect(m_drawing_area, "button_press_event",
			G_CALLBACK(button_press_event), this);
	g_signal_connect(m_drawing_area, "button_release_event",
			G_CALLBACK(button_release_event), this);

	gtk_widget_show_all(m_scrolled_window);

	m_html = NULL;
	m_rendered_width = 0;
	m_context.load_master_stylesheet(master_css);

	gtk_widget_set_events(m_drawing_area,
			        GDK_BUTTON_RELEASE_MASK
			      | GDK_BUTTON_PRESS_MASK
			      | GDK_POINTER_MOTION_MASK);

}

lh_widget::~lh_widget()
{
	g_object_unref(m_drawing_area);
	m_drawing_area = NULL;
	g_object_unref(m_scrolled_window);
	m_scrolled_window = NULL;
	m_html = NULL;
}

GtkWidget *lh_widget::get_widget() const
{
	return m_scrolled_window;
}

void lh_widget::set_caption(const litehtml::tchar_t* caption)
{
	debug_print("lh_widget set_caption\n");
	return;
}

void lh_widget::set_base_url(const litehtml::tchar_t* base_url)
{
	debug_print("lh_widget set_base_url\n");
	return;
}

void lh_widget::on_anchor_click(const litehtml::tchar_t* url, const litehtml::element::ptr& el)
{
	debug_print("lh_widget on_anchor_click. url -> %s\n", url);
	m_clicked_url = url;
	
	return;
}

void lh_widget::import_css(litehtml::tstring& text, const litehtml::tstring& url, litehtml::tstring& baseurl)
{
	debug_print("lh_widget import_css\n");
	baseurl = master_css;
}

void lh_widget::get_client_rect(litehtml::position& client) const
{
	if (m_drawing_area == NULL)
		return;

	client.width = m_rendered_width;
	client.height = m_height;
	client.x = 0;
	client.y = 0;

//	debug_print("lh_widget::get_client_rect: %dx%d\n",
//			client.width, client.height);
}

GdkPixbuf *lh_widget::get_image(const litehtml::tchar_t* url, bool redraw_on_ready)
{
	GError *error = NULL;
	GdkPixbuf *pixbuf = NULL;

	if (!lh_prefs_get()->enable_remote_content) {
		debug_print("blocking download of image from '%s'\n", url);
		return NULL;
	}

	debug_print("Loading... %s\n", url);
	gchar *msg = g_strdup_printf("Loading %s ...", url);
        lh_widget_statusbar_push(msg);
	g_free(msg);
	
	http http_loader;
	GInputStream *image = http_loader.load_url(url, &error);
    
	if (error || !image) {
	    if (error) {
		g_warning("lh_widget::get_image: Could not create pixbuf %s", error->message);
		g_clear_error(&error);
	    }
	    goto statusbar_pop;
	}

	pixbuf = gdk_pixbuf_new_from_stream(image, NULL, &error);
	if (error) {
	    g_warning("lh_widget::get_image: Could not create pixbuf %s", error->message);
	    //g_object_unref(pixbuf);
	    pixbuf = NULL;
	    g_clear_error(&error);
	}
	g_input_stream_close(image, NULL, NULL);

/*	if (redraw_on_ready) {
		redraw();
	}*/

statusbar_pop:
	lh_widget_statusbar_pop();
	
	return pixbuf;
}

void lh_widget::open_html(const gchar *contents)
{
	lh_widget_statusbar_push("Loading HTML part ...");
	m_html = litehtml::document::createFromString(contents, this, &m_context);
	m_rendered_width = 0;
	if (m_html != NULL) {
		debug_print("lh_widget::open_html created document\n");
		redraw();
	}
	lh_widget_statusbar_pop();
}

void lh_widget::draw(cairo_t *cr)
{
	double x1, x2, y1, y2;
	double width, height;

	if (m_html == NULL)
		return;

	cairo_clip_extents(cr, &x1, &y1, &x2, &y2);

	width = x2 - x1;
	height = y2 - y1;

	litehtml::position pos;
	pos.width = (int)width;
	pos.height = (int)height;
	pos.x = (int)x1;
	pos.y = (int)y1;

	m_html->draw((litehtml::uint_ptr)cr, 0, 0, &pos);
}

void lh_widget::redraw()
{
	GtkAllocation rect;
	gint width, height;
	GdkWindow *gdkwin;
	cairo_t *cr;

	if (m_html == NULL) {
		g_warning("lh_widget::redraw: No document!");
		return;
	}

	/* Get width of the viewport. */
	gdkwin = gtk_viewport_get_view_window(GTK_VIEWPORT(m_viewport));
	gdk_drawable_get_size(gdkwin, &width, NULL);

	/* If the available width has changed, rerender the HTML content. */
	if (m_rendered_width != width) {
		debug_print("lh_widget::redraw: width changed: %d != %d\n",
				m_rendered_width, width);

		/* Update our internally stored width, mainly so that
		 * lh_widget::get_client_rect() gives correct width during the
		 * render. */
		m_rendered_width = width;

		/* Re-render HTML for this width. */
		m_html->media_changed();
		m_html->render(m_rendered_width);
		debug_print("render is %dx%d\n", m_html->width(), m_html->height());

		/* Change drawing area's size to match what was rendered. */
		gtk_widget_set_size_request(m_drawing_area,
				m_html->width(), m_html->height());
	}

	paint_white();

	/* Paint the rendered HTML. */
	gdkwin = gtk_widget_get_window(m_drawing_area);
	if (gdkwin == NULL) {
		g_warning("lh_widget::redraw: No GdkWindow to draw on!");
		return;
	}
	cr = gdk_cairo_create(GDK_DRAWABLE(gdkwin));
	draw(cr);

	cairo_destroy(cr);
}

void lh_widget::paint_white()
{
	GdkWindow *gdkwin = gtk_widget_get_window(m_drawing_area);
	if (gdkwin == NULL) {
		g_warning("lh_widget::clear: No GdkWindow to draw on!");
		return;
	}
	cairo_t *cr = gdk_cairo_create(GDK_DRAWABLE(gdkwin));

	/* Paint white background. */
	gint width, height;
	gdk_drawable_get_size(gdkwin, &width, &height);
	cairo_rectangle(cr, 0, 0, width, height);
	cairo_set_source_rgb(cr, 255, 255, 255);
	cairo_fill(cr);

	cairo_destroy(cr);
}
void lh_widget::clear()
{
	paint_white();
	m_rendered_width = 0;
}

void lh_widget::set_cursor(const litehtml::tchar_t* cursor)
{
	if (cursor) {
		if (m_cursor != cursor) {
			m_cursor = cursor;
			update_cursor();
		}
	}
}

void lh_widget::update_cursor()
{
	gint x, y;
	litehtml::element::ptr root_el, over_el, el;
	GdkWindow *w = gdk_display_get_window_at_pointer(gdk_display_get_default(),
			&x, &y);
	GdkCursorType cursType = GDK_ARROW;

	if (m_cursor == _t("pointer")) {
		cursType = GDK_HAND2;
	}

	if (cursType == GDK_ARROW) {
		gdk_window_set_cursor(gtk_widget_get_window(m_drawing_area), NULL);
	} else {
		gdk_window_set_cursor(gtk_widget_get_window(m_drawing_area), gdk_cursor_new(cursType));
	}

	if (w != gtk_widget_get_window(m_drawing_area))
		return;

	/* Find the element we are hovering over */
	root_el = m_html->root();
	g_return_if_fail(root_el != NULL);
	over_el = root_el->get_element_by_point(x, y, x, y);
	g_return_if_fail(over_el != NULL);

	/* If it's not an anchor, check if it has a parent anchor
	 * (e.g. it's an image within an anchor) and grab a pointer
	 * to that. */
	if (strcmp(over_el->get_tagName(), "a") && over_el->parent()) {
		el = over_el->parent();
		while (el && el != root_el && strcmp(el->get_tagName(), "a")) {
			el = el->parent();
		}

		if (el && el != root_el)
			over_el = el;
	}

	/* If it's an anchor, show its "href" attribute in statusbar,
	 * otherwise clear statusbar. */
	if (!strcmp(over_el->get_tagName(), "a")) {
		lh_widget_statusbar_push(over_el->get_attr(_t("href")));
	} else {
		lh_widget_statusbar_pop();
	}
}

void lh_widget::print()
{
    debug_print("lh_widget print\n");
    gtk_widget_realize(GTK_WIDGET(m_drawing_area));
}

static gboolean expose_event_cb(GtkWidget *widget, GdkEvent *event,
		gpointer user_data)
{
	lh_widget *w = (lh_widget *)user_data;
	w->redraw();
	return FALSE;
}

static void size_allocate_cb(GtkWidget *widget, GdkRectangle *allocation,
		gpointer user_data)
{
	lh_widget *w = (lh_widget *)user_data;

	debug_print("size_allocate_cb: %dx%d\n",
			allocation->width, allocation->height);

	w->setHeight(allocation->height);
	w->redraw();
}

static gboolean button_press_event(GtkWidget *widget, GdkEventButton *event,
		gpointer user_data)
{
    litehtml::position::vector redraw_boxes;
    lh_widget *w = (lh_widget *)user_data;
    
    debug_print("lh_widget on_button_press_event\n");

    if(w->m_html)
    {    
        if(w->m_html->on_lbutton_down((int) event->x, (int) event->y, (int) event->x, (int) event->y, redraw_boxes))
        {
            for(auto& pos : redraw_boxes)
            {
		debug_print("x: %d y:%d w: %d h: %d\n", pos.x, pos.y, pos.width, pos.height);
                gtk_widget_queue_draw_area(widget, pos.x, pos.y, pos.width, pos.height);
            }
        }
	}
	
	return true;
}

static gboolean motion_notify_event(GtkWidget *widget, GdkEventButton *event,
        gpointer user_data)
{
    litehtml::position::vector redraw_boxes;
    lh_widget *w = (lh_widget *)user_data;
    
    //debug_print("lh_widget on_motion_notify_event\n");

    if(w->m_html)
    {    
	//if(m_cursor == _t("pointer"))
        if(w->m_html->on_mouse_over((int) event->x, (int) event->y, (int) event->x, (int) event->y, redraw_boxes))
        {
            for (auto& pos : redraw_boxes)
            {
		debug_print("x: %d y:%d w: %d h: %d\n", pos.x, pos.y, pos.width, pos.height);
                gtk_widget_queue_draw_area(widget, pos.x, pos.y, pos.width, pos.height);
            }
        }
	}
	
	return true;
}

static gboolean button_release_event(GtkWidget *widget, GdkEventButton *event,
        gpointer user_data)
{
    litehtml::position::vector redraw_boxes;
    lh_widget *w = (lh_widget *)user_data;
    GError* error = NULL;

	debug_print("lh_widget on_button_release_event\n");
	
	if(w->m_html)
	{
	    w->m_clicked_url.clear();
	    if(w->m_html->on_lbutton_up((int) event->x, (int) event->y, (int) event->x, (int) event->y, redraw_boxes))
        {
            for (auto& pos : redraw_boxes)
            {
		debug_print("x: %d y:%d w: %d h: %d\n", pos.x, pos.y, pos.width, pos.height);
                gtk_widget_queue_draw_area(widget, pos.x, pos.y, pos.width, pos.height);
            }
        }
        
        if (!w->m_clicked_url.empty())
        {
                debug_print("Open in browser: %s\n", w->m_clicked_url.c_str());
		gtk_show_uri(gdk_screen_get_default(),
			     w->m_clicked_url.c_str(),
			     GDK_CURRENT_TIME, &error);
                if (error) {
                    g_warning("Failed opening url(%s): %s", w->m_clicked_url, error->message);
                    g_clear_error(&error);
                }
        }
    }

	return true;
}

///////////////////////////////////////////////////////////
extern "C" {

lh_widget_wrapped *lh_widget_new()
{
	return new lh_widget;
}

GtkWidget *lh_widget_get_widget(lh_widget_wrapped *w)
{
	return w->get_widget();
}

void lh_widget_open_html(lh_widget_wrapped *w, const gchar *path)
{
	w->open_html(path);
}

void lh_widget_clear(lh_widget_wrapped *w)
{
	w->clear();
}

void lh_widget_destroy(lh_widget_wrapped *w)
{
	delete w;
}

void lh_widget_print(lh_widget_wrapped *w) {
	w->print();
}

} /* extern "C" */
