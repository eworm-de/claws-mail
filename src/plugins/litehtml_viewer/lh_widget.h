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
		void set_caption(const litehtml::tchar_t* caption);
		void set_base_url(const litehtml::tchar_t* base_url);
		void on_anchor_click(const litehtml::tchar_t* url, const litehtml::element::ptr& el);
		void set_cursor(const litehtml::tchar_t* cursor);
		void import_css(litehtml::tstring& text, const litehtml::tstring& url, litehtml::tstring& baseurl);
		void get_client_rect(litehtml::position& client) const;
		inline const litehtml::tchar_t *get_default_font_name() const { return m_font_name; };

		inline int get_default_font_size() const { return m_font_size; };
		litehtml::uint_ptr create_font(const litehtml::tchar_t* faceName, int size, int weight, litehtml::font_style italic, unsigned int decoration, litehtml::font_metrics* fm);
		void delete_font(litehtml::uint_ptr hFont);
		int text_width(const litehtml::tchar_t* text, litehtml::uint_ptr hFont);
		void draw_text(litehtml::uint_ptr hdc, const litehtml::tchar_t* text, litehtml::uint_ptr hFont, litehtml::web_color color, const litehtml::position& pos);

		void draw(cairo_t *cr);
		void rerender();
		void redraw();
		void open_html(const gchar *contents);
		void clear();
		void update_cursor(const litehtml::tchar_t* cursor);
		void update_font();
		void print();

		const litehtml::tchar_t *get_href_at(litehtml::element::ptr element) const;
		const litehtml::tchar_t *get_href_at(const gint x, const gint y) const;
		void popup_context_menu(const litehtml::tchar_t *url, GdkEventButton *event);
		const litehtml::tstring fullurl(const litehtml::tchar_t *url) const;

		void set_partinfo(MimeInfo *partinfo);
		GdkPixbuf *get_local_image(const litehtml::tstring url) const;

		void set_cairo_context(cairo_t *cr);

		litehtml::document::ptr m_html;
		litehtml::tstring m_clicked_url;
		litehtml::tstring m_base_url;

	private:
		gint m_rendered_width;
		GtkWidget *m_drawing_area;
		GtkWidget *m_scrolled_window;
		GtkWidget *m_viewport;
		GtkWidget *m_context_menu;
		litehtml::context m_context;
		gint m_height;
		litehtml::element::ptr m_over_element;
		gboolean m_showing_url;
		MimeInfo *m_partinfo;
		cairo_t *m_cairo_context;

		litehtml::tchar_t *m_font_name;
		int m_font_size;
		std::atomic<bool> m_force_render;
		std::atomic<bool> m_blank;
};
