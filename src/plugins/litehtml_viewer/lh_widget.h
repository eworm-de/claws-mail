#include <gtk/gtk.h>
#include <glib.h>
#include <gio/gio.h>

#include "container_linux.h"

class lh_widget : public container_linux
{
	public:
		lh_widget();
		~lh_widget();

		GtkWidget *get_widget() const;

		void set_caption(const litehtml::tchar_t* caption);
		void set_base_url(const litehtml::tchar_t* base_url);
		void on_anchor_click(const litehtml::tchar_t* url, const litehtml::element::ptr& el);
		void set_cursor(const litehtml::tchar_t* cursor);
		void import_css(litehtml::tstring& text, const litehtml::tstring& url, litehtml::tstring& baseurl);
		void get_client_rect(litehtml::position& client) const;
		GdkPixbuf *get_image(const litehtml::tchar_t* url, bool redraw_on_ready);

		gint height() const { return m_height; };
		void setHeight(gint height) { m_height = height; };
		void draw(cairo_t *cr);
		void redraw();
		void open_html(const gchar *contents);
		void clear();
		void update_cursor();
		void print();

		const litehtml::tchar_t *get_href_at(const gint x, const gint y) const;
		void popup_context_menu(const litehtml::tchar_t *url, GdkEventButton *event);

		litehtml::document::ptr m_html;
		litehtml::tstring m_clicked_url;

	private:
		void paint_white();

		gint m_rendered_width;
		GtkWidget *m_drawing_area;
		GtkWidget *m_scrolled_window;
		GtkWidget *m_viewport;
		GtkWidget *m_context_menu;
		litehtml::context m_context;
		gint m_height;
		litehtml::tstring m_cursor;
};
