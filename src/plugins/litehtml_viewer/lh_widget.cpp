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
#include "lh_widget.h"
#include "lh_widget_wrapped.h"

char master_css[] = {
#include "css.inc"
};

/**
  * curl callback
  */
static char* response_mime = NULL;     /* response content-type. ex: "text/html" */
static char* response_data = NULL;     /* response data from server. */
static size_t response_size = 0;       /* response size of data */

static gboolean expose_event_cb(GtkWidget *widget, GdkEvent *event,
		gpointer user_data);
static void size_allocate_cb(GtkWidget *widget, GdkRectangle *allocation,
		gpointer user_data);
static size_t handle_returned_data(char* ptr, size_t size, size_t nmemb, void* stream);
static size_t handle_returned_header(void* ptr, size_t size, size_t nmemb, void* stream);

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

	gtk_widget_show_all(m_scrolled_window);

	m_html = NULL;
	m_rendered_width = 0;
	m_context.load_master_stylesheet(master_css);
	stream = NULL;
}

lh_widget::~lh_widget()
{
	g_object_unref(m_drawing_area);
	m_drawing_area = NULL;
	g_object_unref(m_scrolled_window);
	m_scrolled_window = NULL;
	m_html = NULL;
	if (stream) {
	    g_input_stream_close(stream, NULL, NULL);
	    stream = NULL;
	}
}

GtkWidget *lh_widget::get_widget() const
{
	return m_scrolled_window;
}

void lh_widget::set_caption(const litehtml::tchar_t* caption)
{
	g_log(NULL, G_LOG_LEVEL_MESSAGE, "lh_widget set_caption");
	return;
}

void lh_widget::set_base_url(const litehtml::tchar_t* base_url)
{
	g_log(NULL, G_LOG_LEVEL_MESSAGE, "lh_widget set_base_url");
	return;
}

void lh_widget::on_anchor_click(const litehtml::tchar_t* url, const litehtml::element::ptr& el)
{
	g_log(NULL, G_LOG_LEVEL_MESSAGE, "lh_widget on_anchor_click");
	return;
}

void lh_widget::set_cursor(const litehtml::tchar_t* cursor)
{
	g_log(NULL, G_LOG_LEVEL_MESSAGE, "lh_widget set_cursor");
	if (cursor == NULL)
		return;
}

void lh_widget::import_css(litehtml::tstring& text, const litehtml::tstring& url, litehtml::tstring& baseurl)
{
	g_log(NULL, G_LOG_LEVEL_MESSAGE, "lh_widget import_css");
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

//	g_log(NULL, G_LOG_LEVEL_MESSAGE, "lh_widget::get_client_rect: %dx%d",
//			client.width, client.height);
}

GdkPixbuf *lh_widget::get_image(const litehtml::tchar_t* url, bool redraw_on_ready)
{
	GError *error = NULL;
	GdkPixbuf *pixbuf = NULL;

	g_log(NULL, G_LOG_LEVEL_MESSAGE, "Loading... %s", url);

	GInputStream *image = load_url(url, &error);
	if (error) {
		g_log(NULL, G_LOG_LEVEL_MESSAGE, "Error: %s", error->message);
		g_error_free(error);
		return NULL;
	}

	GdkPixbufLoader* loader = gdk_pixbuf_loader_new();
	if (gdk_pixbuf_loader_write(loader, (const guchar*)response_data, response_size, &error)) {
		pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
	} else {
		g_log(NULL, G_LOG_LEVEL_ERROR, "lh_widget::get_image: Could not create pixbuf");
	}
	gdk_pixbuf_loader_close(loader, NULL);

        /* cleanup callback data */
        if (response_mime) g_free(response_mime);
        if (response_data) g_free(response_data);
        response_data = NULL;
        response_mime = NULL;
        response_size = 0;
	
	return pixbuf;
}

void lh_widget::open_html(const gchar *contents)
{
	m_html = litehtml::document::createFromString(contents, this, &m_context);
	m_rendered_width = 0;
	if (m_html != NULL) {
		g_log(NULL, G_LOG_LEVEL_MESSAGE, "lh_widget::open_html created document");
		redraw();
	}
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
		g_log(NULL, G_LOG_LEVEL_WARNING, "lh_widget::redraw: No document!");
		return;
	}

	/* Get width of the viewport. */
	gdkwin = gtk_viewport_get_view_window(GTK_VIEWPORT(m_viewport));
	gdk_drawable_get_size(gdkwin, &width, NULL);

	/* If the available width has changed, rerender the HTML content. */
	if (m_rendered_width != width) {
		g_log(NULL, G_LOG_LEVEL_MESSAGE,
				"lh_widget::redraw: width changed: %d != %d",
				m_rendered_width, width);

		/* Update our internally stored width, mainly so that
		 * lh_widget::get_client_rect() gives correct width during the
		 * render. */
		m_rendered_width = width;

		/* Re-render HTML for this width. */
		m_html->media_changed();
		m_html->render(m_rendered_width);
		g_log(NULL, G_LOG_LEVEL_MESSAGE, "render is %dx%d",
				m_html->width(), m_html->height());

		/* Change drawing area's size to match what was rendered. */
		gtk_widget_set_size_request(m_drawing_area,
				m_html->width(), m_html->height());
	}

	paint_white();

	/* Paint the rendered HTML. */
	gdkwin = gtk_widget_get_window(m_drawing_area);
	if (gdkwin == NULL) {
		g_log(NULL, G_LOG_LEVEL_WARNING, "lh_widget::redraw: No GdkWindow to draw on!");
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
		g_log(NULL, G_LOG_LEVEL_WARNING, "lh_widget::clear: No GdkWindow to draw on!");
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

GInputStream *lh_widget::load_url(const gchar *url, GError **error)
{
	GError* _error = NULL;
	CURL* curl = NULL;
	CURLcode res = CURLE_OK;
	gsize len;
	gchar* content;

	/* initialize callback data */
	response_mime = NULL;
	response_data = NULL;
	response_size = 0;
	if (stream) {
		g_input_stream_close(stream, NULL, &_error);
		if (_error) {
			if (error) *error = _error;
			return NULL;
		}
	}
		
	stream = NULL;

	if (!strncmp(url, "file:///", 8) || g_file_test(url, G_FILE_TEST_EXISTS)) {
		gchar* newurl = g_filename_from_uri(url, NULL, NULL);
		if (g_file_get_contents(newurl ? newurl : url, &content, &len, &_error)) {
			stream = g_memory_input_stream_new_from_data(content, len, g_free);
		} else {
			g_log(NULL, G_LOG_LEVEL_MESSAGE, "%s", _error->message);
		}
		g_free(newurl);
	} else {
		curl = curl_easy_init();
		if (!curl) return NULL;
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, handle_returned_data);
		curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, handle_returned_header);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, HTTP_GET_TIMEOUT);
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
		res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);
		if (res == CURLE_OK) {
			stream = g_memory_input_stream_new_from_data(content, response_size, g_free);
		} else
			_error = g_error_new_literal(G_FILE_ERROR, res, curl_easy_strerror(res));
	}

	if (error && _error) *error = _error;

	return stream;
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

	g_log(NULL, G_LOG_LEVEL_MESSAGE, "size_allocate_cb: %dx%d",
			allocation->width, allocation->height);

	w->setHeight(allocation->height);
	w->redraw();
}

static size_t handle_returned_data(char* ptr, size_t size, size_t nmemb, void* stream) {
	if (!response_data)
		response_data = (char*)malloc(size*nmemb);
	else
		response_data = (char*)realloc(response_data, response_size+size*nmemb);
	if (response_data) {
		memcpy(response_data+response_size, ptr, size*nmemb);
		response_size += size*nmemb;
	}
	return size*nmemb;
}

static size_t handle_returned_header(void* ptr, size_t size, size_t nmemb, void* stream) {
	char* header = NULL;

	header = (char*) malloc(size*nmemb + 1);
	memcpy(header, ptr, size*nmemb);
	header[size*nmemb] = 0;
	if (strncmp(header, "Content-Type: ", 14) == 0) {
		char* stop = header + 14;
		stop = strpbrk(header + 14, "\r\n;");
		if (stop) *stop = 0;
		response_mime = strdup(header + 14);
	}
	free(header);
	return size*nmemb;
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

} /* extern "C" */
