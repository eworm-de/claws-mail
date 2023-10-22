/*
 * Claws Mail -- A GTK based, lightweight, and fast e-mail client
 * Copyright(C) 2019 the Claws Mail Team
 *
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
#include <glib/gi18n.h>
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

extern "C" {
const gchar *prefs_common_get_uri_cmd(void);
}

static gboolean draw_cb(GtkWidget *widget, cairo_t *cr,
		gpointer user_data);
static gboolean button_press_event(GtkWidget *widget, GdkEventButton *event,
		gpointer user_data);
static gboolean motion_notify_event(GtkWidget *widget, GdkEventButton *event,
        gpointer user_data);
static gboolean button_release_event(GtkWidget *widget, GdkEventButton *event,
        gpointer user_data);
static void open_link_cb(GtkMenuItem *item, gpointer user_data);
static void copy_link_cb(GtkMenuItem *item, gpointer user_data);

lh_widget::lh_widget()
{
	GtkWidget *item;

	m_force_render = false;
	m_blank = false;

	/* scrolled window */
	m_scrolled_window = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(m_scrolled_window),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);

	/* viewport */
	GtkScrolledWindow *scw = GTK_SCROLLED_WINDOW(m_scrolled_window);
	m_viewport = gtk_viewport_new(
			gtk_scrolled_window_get_hadjustment(scw),
			gtk_scrolled_window_get_vadjustment(scw));
	gtk_container_add(GTK_CONTAINER(m_scrolled_window), m_viewport);

	/* drawing area */
	m_drawing_area = gtk_drawing_area_new();
	gtk_container_add(GTK_CONTAINER(m_viewport), m_drawing_area);
	g_signal_connect(m_drawing_area, "draw",
			G_CALLBACK(draw_cb), this);
	g_signal_connect(m_drawing_area, "motion_notify_event",
			G_CALLBACK(motion_notify_event), this);
	g_signal_connect(m_drawing_area, "button_press_event",
			G_CALLBACK(button_press_event), this);
	g_signal_connect(m_drawing_area, "button_release_event",
			G_CALLBACK(button_release_event), this);

	gtk_widget_show_all(m_scrolled_window);

	/* context menu */
	m_context_menu = gtk_menu_new();

	item = gtk_menu_item_new_with_label(_("Open Link"));
	g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(open_link_cb), this);
	gtk_menu_shell_append(GTK_MENU_SHELL(m_context_menu), item);

	item = gtk_menu_item_new_with_label(_("Copy Link Location"));
	g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(copy_link_cb), this);
	gtk_menu_shell_append(GTK_MENU_SHELL(m_context_menu), item);

	m_html = NULL;
	m_rendered_width = 0;

	m_font_name = NULL;
	m_font_size = 0;

	m_partinfo = NULL;

	m_showing_url = FALSE;

	m_cairo_context = NULL;

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
	g_free(m_font_name);
}

GtkWidget *lh_widget::get_widget() const
{
	return m_scrolled_window;
}

void lh_widget::set_caption(const char *caption)
{
	debug_print("lh_widget set_caption\n");
	return;
}

void lh_widget::set_base_url(const char *base_url)
{
	debug_print("lh_widget set_base_url '%s'\n",
			(base_url ? base_url : "(null)"));
	if (base_url)
		m_base_url = base_url;
	else
		m_base_url.clear();

	return;
}

void lh_widget::on_anchor_click(const char *url, const litehtml::element::ptr& el)
{
	debug_print("lh_widget on_anchor_click. url -> %s\n", url);

	m_clicked_url = fullurl(url);
	return;
}

void lh_widget::import_css(litehtml::string& text, const litehtml::string& url, litehtml::string& baseurl)
{
	debug_print("lh_widget import_css. url=\"%s\" baseurl=\"%s\"\n", url.c_str(), baseurl.c_str());
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

void lh_widget::open_html(const gchar *contents)
{
	gint num = clear_images(lh_prefs_get()->image_cache_size * 1024 * 1000);
	GtkAdjustment *adj;

	debug_print("LH: cleared %d images from image cache\n", num);

	update_font();

	lh_widget_statusbar_push("Loading HTML part ...");
	m_html = litehtml::document::createFromString(contents, this);
	m_rendered_width = 0;
	if (m_html != NULL) {
		debug_print("lh_widget::open_html created document\n");
		adj = gtk_scrolled_window_get_hadjustment(
				GTK_SCROLLED_WINDOW(m_scrolled_window));
		gtk_adjustment_set_value(adj, 0.0);
		adj = gtk_scrolled_window_get_vadjustment(
				GTK_SCROLLED_WINDOW(m_scrolled_window));
		gtk_adjustment_set_value(adj, 0.0);
		m_blank = false;
	}
	lh_widget_statusbar_pop();
}

void lh_widget::rerender()
{
	m_force_render = true;
	gtk_widget_queue_draw(m_drawing_area);
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
	gint width;
	GdkWindow *gdkwin;
	cairo_t *cr;
	cairo_region_t *creg;
	GdkDrawingContext *gdkctx;
	gboolean destroy = FALSE;

	if (m_html == NULL)
		return;

	/* Get width of the viewport. */
	gtk_widget_get_allocation(GTK_WIDGET(m_viewport), &rect);
	width = rect.width;
	m_height = rect.height;

	/* If the available width has changed, rerender the HTML content. */
	if (m_rendered_width != width || std::atomic_exchange(&m_force_render, false)) {
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

	/* Use provided cairo context, if any. Otherwise create our own. */
	if (m_cairo_context != NULL) {
		cr = m_cairo_context;
	} else {
		gdkwin = gtk_widget_get_window(m_drawing_area);
		if (gdkwin == NULL) {
			g_warning("lh_widget::redraw: No GdkWindow to draw on!");
			return;
		}
		creg = cairo_region_create_rectangle(&rect);
		gdkctx = gdk_window_begin_draw_frame(gdkwin, creg);
		cr = gdk_drawing_context_get_cairo_context(gdkctx);
		destroy = TRUE;
	}

	if(!std::atomic_exchange(&m_blank, false)) {
		draw(cr);
	} else {
		cairo_rectangle(cr, rect.x, rect.y, rect.width, rect.height);
		cairo_set_source_rgb(cr, 255, 255, 255);
		cairo_fill(cr);
	}

	/* Only destroy the used cairo context if we created it earlier. */
	if (destroy) {
		gdk_window_end_draw_frame(gdkwin, gdkctx);
		cairo_region_destroy(creg);
	}
}

void lh_widget::clear()
{
	m_html = nullptr;
	m_blank = true;
	m_rendered_width = 0;
	m_base_url.clear();
	m_clicked_url.clear();
}

void lh_widget::set_cursor(const char *cursor)
{
	litehtml::element::const_ptr over_el = m_html->get_over_element();

	if (m_showing_url &&
			(over_el == NULL || over_el != m_over_element)) {
		lh_widget_statusbar_pop();
		m_showing_url = FALSE;
	}

	if (over_el != m_over_element) {
		m_over_element = over_el;
		update_cursor(cursor);
	}
}

void lh_widget::update_cursor(const char *cursor)
{
	GdkCursorType cursType = GDK_ARROW;
	const char *href = get_href_at(m_over_element);

	/* If there is a href, and litehtml is okay with showing a pointer
	 * cursor ("pointer" or "auto"), set it, otherwise keep the
	 * default arrow cursor */
	if ((!strcmp(cursor, "pointer") || !strcmp(cursor, "auto")) &&
			href != NULL) {
		cursType = GDK_HAND2;
	}

	if (cursType == GDK_ARROW) {
		gdk_window_set_cursor(gtk_widget_get_window(m_drawing_area), NULL);
	} else {
		gdk_window_set_cursor(gtk_widget_get_window(m_drawing_area),
				gdk_cursor_new_for_display(gtk_widget_get_display(m_drawing_area),
					cursType));
	}

	/* If there is a href, show it in statusbar */
	if (href != NULL) {
		lh_widget_statusbar_push(fullurl(href).c_str());
		m_showing_url = TRUE;
	}
}

const char *lh_widget::get_href_at(litehtml::element::const_ptr element)
{
	litehtml::element::const_ptr el;

	if (element == NULL)
		return NULL;

	/* If it's not an anchor, check if it has a parent anchor
	 * (e.g. it's an image within an anchor) and grab a pointer
	 * to that. */
	if (strcmp(element->get_tagName(), "a") && element->parent()) {
		el = element->parent();
		while (el && el != m_html->root() && strcmp(el->get_tagName(), "a")) {
			el = el->parent();
		}

		if (!el || el == m_html->root())
			return NULL;
	} else {
		el = element;
	}

	/* At this point, over_el is pointing at an anchor tag, so let's
	 * grab its href attribute. */
	return el->get_attr("href");
}

void lh_widget::print()
{
    debug_print("lh_widget print\n");
    gtk_widget_realize(GTK_WIDGET(m_drawing_area));
}

void lh_widget::popup_context_menu(const char *url,
		GdkEventButton *event)
{
	cm_return_if_fail(url != NULL);
	cm_return_if_fail(event != NULL);

	debug_print("lh_widget showing context menu for '%s'\n", url);

	m_clicked_url = url;
	gtk_widget_show_all(m_context_menu);
	gtk_menu_popup_at_pointer(GTK_MENU(m_context_menu), (GdkEvent *)event);
}

void lh_widget::update_font()
{
	PangoFontDescription *pd =
		pango_font_description_from_string(lh_prefs_get()->default_font);
	gboolean absolute = pango_font_description_get_size_is_absolute(pd);

	g_free(m_font_name);
	m_font_name = g_strdup(pango_font_description_get_family(pd));
	m_font_size = pango_font_description_get_size(pd);

	pango_font_description_free(pd);

	if (!absolute)
		m_font_size /= PANGO_SCALE;

	debug_print("Font set to '%s', size %d\n", m_font_name, m_font_size);
}

const litehtml::string lh_widget::fullurl(const char *url) const
{
	if (*url == '#' && !m_base_url.empty())
		return m_base_url + url;

	return url;
}

void lh_widget::set_partinfo(MimeInfo *partinfo)
{
	m_partinfo = partinfo;
}

GdkPixbuf *lh_widget::get_local_image(const litehtml::string url) const
{
	GdkPixbuf *pixbuf;
	const gchar *name;
	MimeInfo *p = m_partinfo;

	if (strncmp(url.c_str(), "cid:", 4) != 0) {
		debug_print("lh_widget::get_local_image: '%s' is not a local URI, ignoring\n", url.c_str());
		return NULL;
	}

	name = url.c_str() + 4;
	debug_print("getting message part '%s'\n", name);

	while ((p = procmime_mimeinfo_next(p)) != NULL) {
		size_t len = strlen(name);

		/* p->id is in format "<partname>" */
		if (p->id != NULL &&
				strlen(p->id) >= len + 2 &&
				!strncasecmp(name, p->id + 1, len) &&
				*(p->id + len + 1) == '>') {
			GError *error = NULL;

			pixbuf = procmime_get_part_as_pixbuf(p, &error);
			if (error != NULL) {
				g_warning("couldn't load image: %s", error->message);
				g_error_free(error);
				return NULL;
			}

			return pixbuf;
		}
	}

	/* MIME part with requested name was not found */
	return NULL;
}

void lh_widget::set_cairo_context(cairo_t *cr)
{
	m_cairo_context = cr;
}


////////////////////////////////////////////////
static gboolean draw_cb(GtkWidget *widget, cairo_t *cr,
		gpointer user_data)
{
	lh_widget *w = (lh_widget *)user_data;
	w->set_cairo_context(cr);
	w->redraw();
	w->set_cairo_context(NULL);
	return FALSE;
}

static gboolean button_press_event(GtkWidget *widget, GdkEventButton *event,
		gpointer user_data)
{
	litehtml::position::vector redraw_boxes;
	lh_widget *w = (lh_widget *)user_data;

	if (w->m_html == NULL)
		return FALSE;

	//debug_print("lh_widget on_button_press_event\n");

	if (event->type == GDK_2BUTTON_PRESS ||
			event->type == GDK_3BUTTON_PRESS)
		return TRUE;

	/* Right-click */
	if (event->button == 3) {
		const char *url = w->get_href_at(w->m_html->get_over_element());

		if (url != NULL)
			w->popup_context_menu(url, event);

		return TRUE;
	}

	if(w->m_html->on_lbutton_down((int) event->x, (int) event->y,
				(int) event->x, (int) event->y, redraw_boxes)) {
		for(auto& pos : redraw_boxes) {
			debug_print("x: %d y:%d w: %d h: %d\n", pos.x, pos.y, pos.width, pos.height);
			gtk_widget_queue_draw_area(widget, pos.x, pos.y, pos.width, pos.height);
		}
	}
	
	return TRUE;
}

static gboolean motion_notify_event(GtkWidget *widget, GdkEventButton *event,
        gpointer user_data)
{
    litehtml::position::vector redraw_boxes;
    lh_widget *w = (lh_widget *)user_data;
    
    //debug_print("lh_widget on_motion_notify_event\n");

    if(w->m_html)
    {    
        if(w->m_html->on_mouse_over((int) event->x, (int) event->y, (int) event->x, (int) event->y, redraw_boxes))
        {
            for (auto& pos : redraw_boxes)
            {
		debug_print("x: %d y:%d w: %d h: %d\n", pos.x, pos.y, pos.width, pos.height);
                gtk_widget_queue_draw_area(widget, pos.x, pos.y, pos.width, pos.height);
            }
        }
	}
	
	return TRUE;
}

static gboolean button_release_event(GtkWidget *widget, GdkEventButton *event,
        gpointer user_data)
{
    litehtml::position::vector redraw_boxes;
    lh_widget *w = (lh_widget *)user_data;

	if (w->m_html == NULL)
		return FALSE;

	//debug_print("lh_widget on_button_release_event\n");

	if (event->type == GDK_2BUTTON_PRESS ||
			event->type == GDK_3BUTTON_PRESS)
		return TRUE;

	/* Right-click */
	if (event->button == 3)
		return TRUE;

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
            open_uri(w->m_clicked_url.c_str(), prefs_common_get_uri_cmd());
    }

	return TRUE;
}

static void open_link_cb(GtkMenuItem *item, gpointer user_data)
{
	lh_widget_wrapped *w = (lh_widget_wrapped *)user_data;

	open_uri(w->m_clicked_url.c_str(), prefs_common_get_uri_cmd());
}

static void copy_link_cb(GtkMenuItem *item, gpointer user_data)
{
	lh_widget_wrapped *w = (lh_widget_wrapped *)user_data;

	gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_PRIMARY),
			w->m_clicked_url.c_str(), -1);
	gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD),
			w->m_clicked_url.c_str(), -1);
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

void lh_widget_set_partinfo(lh_widget_wrapped *w, MimeInfo *partinfo)
{
	w->set_partinfo(partinfo);
}

} /* extern "C" */
