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

#include <gtk/gtk.h>
#include <glib.h>
#include <gio/gio.h>
#include <atomic>

#include "procmime.h"

#include "container_linux.h"

struct pango_font
{
	PangoFontDescription *font;
	bool underline;
	bool strikethrough;
};

class lh_widget : public container_linux
{
	public:
		lh_widget();
		~lh_widget();

		GtkWidget *get_widget() const;

		/* Methods that litehtml calls */
		void set_caption(const char *caption);
		void set_base_url(const char *base_url);
		void on_anchor_click(const char *url, const litehtml::element::ptr& el);
		void set_cursor(const char *cursor);
		void import_css(litehtml::string& text, const litehtml::string& url, litehtml::string& baseurl);
		void get_client_rect(litehtml::position& client) const;
		inline const char *get_default_font_name() const { return m_font_name; };

		inline int get_default_font_size() const { return m_font_size; };

		void draw(cairo_t *cr);
		void rerender();
		void redraw();
		void open_html(const gchar *contents);
		void clear();
		void update_cursor(const char *cursor);
		void update_font();
		void print();

		const char *get_href_at(litehtml::element::const_ptr element);
		void popup_context_menu(const char *url, GdkEventButton *event);
		const litehtml::string fullurl(const char *url) const;

		void set_partinfo(MimeInfo *partinfo);
		GdkPixbuf *get_local_image(const litehtml::string url) const;

		void set_cairo_context(cairo_t *cr);

		litehtml::document::ptr m_html;
		litehtml::string m_clicked_url;
		litehtml::string m_base_url;

	private:
		gint m_rendered_width;
		GtkWidget *m_drawing_area;
		GtkWidget *m_scrolled_window;
		GtkWidget *m_viewport;
		GtkWidget *m_context_menu;
		gint m_height;
		litehtml::element::const_ptr m_over_element;
		gboolean m_showing_url;
		MimeInfo *m_partinfo;
		cairo_t *m_cairo_context;

		char *m_font_name;
		int m_font_size;
		std::atomic<bool> m_force_render;
		std::atomic<bool> m_blank;
};
