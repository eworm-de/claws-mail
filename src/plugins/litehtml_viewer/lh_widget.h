#include <gtk/gtk.h>
#include <glib.h>
#include <gio/gio.h>

#include "container_linux.h"

#define HTTP_GET_TIMEOUT 60L

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

	private:
		void paint_white();
		GInputStream *load_url(const gchar *url, GError **error);

		GInputStream *stream;
		litehtml::document::ptr m_html;
		gint m_rendered_width;
		GtkWidget *m_drawing_area;
		GtkWidget *m_scrolled_window;
		GtkWidget *m_viewport;
		litehtml::context m_context;
		gint m_height;

};
