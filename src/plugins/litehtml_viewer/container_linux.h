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

#ifndef LH_CONTAINER_LINUX_H
#define LH_CONTAINER_LINUX_H

#include <sys/time.h>

#include "litehtml/litehtml.h"
#include <cairo.h>
#include <gtk/gtk.h>
#include <pango/pangocairo.h>

struct cairo_clip_box
{
	typedef std::vector<cairo_clip_box> vector;
	litehtml::position	box;
	litehtml::border_radiuses radius;

	cairo_clip_box(const litehtml::position& vBox, const litehtml::border_radiuses& vRad)
	{
		box = vBox;
		radius = vRad;
	}

	cairo_clip_box(const cairo_clip_box& val)
	{
		box = val.box;
		radius = val.radius;
	}
	cairo_clip_box& operator=(const cairo_clip_box& val)
	{
		box = val.box;
		radius = val.radius;
		return *this;
	}
};

struct cairo_font
{
    PangoFontDescription* font;
	int size;
	bool underline;
	bool strikeout;
    int ascent;
    int descent;
    int underline_thickness;
    int underline_position;
    int strikethrough_thickness;
    int strikethrough_position;
};

class container_linux :	public litehtml::document_container
{
	typedef std::pair<GdkPixbuf*, struct timeval> img_cache_entry;
	typedef std::map<litehtml::string, img_cache_entry> images_map;

protected:
	cairo_surface_t*			m_temp_surface;
	cairo_t*					m_temp_cr;
	images_map					m_images;
    cairo_clip_box::vector		m_clips;
	GRecMutex					m_images_lock;
public:
	container_linux();
	virtual ~container_linux();

	litehtml::uint_ptr create_font(const char* faceName, int size, int weight, litehtml::font_style italic, unsigned int decoration, litehtml::font_metrics* fm) override;
	void delete_font(litehtml::uint_ptr hFont) override;
	int text_width(const char* text, litehtml::uint_ptr hFont) override;
	void draw_text(litehtml::uint_ptr hdc, const char* text, litehtml::uint_ptr hFont, litehtml::web_color color, const litehtml::position& pos) override;
	int pt_to_px(int pt) const override;
	//int get_default_font_size() const override;
	//const char*	get_default_font_name() const override;
	void load_image(const char* src, const char* baseurl, bool redraw_on_ready) override;
	void get_image_size(const char* src, const char* baseurl, litehtml::size& sz) override;
	void draw_background(litehtml::uint_ptr hdc, const std::vector<litehtml::background_paint>& bg) override;
	void draw_borders(litehtml::uint_ptr hdc, const litehtml::borders& borders, const litehtml::position& draw_pos, bool root) override;
	void draw_list_marker(litehtml::uint_ptr hdc, const litehtml::list_marker& marker) override;
	std::shared_ptr<litehtml::element>	create_element(const char *tag_name,
														 const litehtml::string_map &attributes,
														 const std::shared_ptr<litehtml::document> &doc) override;
	void get_media_features(litehtml::media_features& media) const override;
	void get_language(litehtml::string& language, litehtml::string & culture) const override;
	void link(const std::shared_ptr<litehtml::document> &ptr, const litehtml::element::ptr& el) override;


	void transform_text(litehtml::string& text, litehtml::text_transform tt) override;
	void set_clip(const litehtml::position& pos, const litehtml::border_radiuses& bdr_radius) override;
	void del_clip() override;

	virtual void make_url( const char* url, const char* basepath, litehtml::string& out );

	void clear_images();

	/* Trim down images cache to less than desired_size [bytes],
	 * starting from oldest stored. */
	gint clear_images(gsize desired_size);

	void update_image_cache(const gchar *url, GdkPixbuf *image);
	virtual void rerender() = 0;
	virtual GdkPixbuf *get_local_image(const litehtml::string url) const = 0;

protected:
	virtual void draw_ellipse(cairo_t* cr, int x, int y, int width, int height, const litehtml::web_color& color, int line_width);
	virtual void fill_ellipse(cairo_t* cr, int x, int y, int width, int height, const litehtml::web_color& color);
	virtual void rounded_rectangle( cairo_t* cr, const litehtml::position &pos, const litehtml::border_radiuses &radius );

private:
	void apply_clip(cairo_t* cr);

	static void add_path_arc(cairo_t* cr, double x, double y, double rx, double ry, double a1, double a2, bool neg);
	static void set_color(cairo_t* cr, const litehtml::web_color& color)	{ cairo_set_source_rgba(cr, color.red / 255.0, color.green / 255.0, color.blue / 255.0, color.alpha / 255.0); }
	static cairo_surface_t* surface_from_pixbuf(const GdkPixbuf *bmp);
	static void draw_pixbuf(cairo_t* cr, const GdkPixbuf *bmp, int x, int y, int cx, int cy);

	void lock_images_cache(void);
	void unlock_images_cache(void);
};

#endif
