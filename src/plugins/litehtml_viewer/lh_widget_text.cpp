/*
 * Claws Mail -- A GTK based, lightweight, and fast e-mail client
 * Copyright(C) 2019 the Claws Mail Team
 *
 * litehtml callbacks related to text rendering
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

#include "config.h"

#include <glib.h>

#include "litehtml/litehtml.h"

#include "lh_widget.h"

litehtml::uint_ptr lh_widget::create_font( const litehtml::tchar_t* faceName, int size, int weight, litehtml::font_style italic, unsigned int decoration, litehtml::font_metrics* fm )
{
	PangoFontDescription *desc =
		pango_font_description_from_string(faceName);

	pango_font_description_set_size(desc, size * PANGO_SCALE);
	pango_font_description_set_weight(desc, (PangoWeight)weight);

	if (italic == litehtml::fontStyleItalic)
		pango_font_description_set_style(desc, PANGO_STYLE_ITALIC);
	else
		pango_font_description_set_style(desc, PANGO_STYLE_NORMAL);

	if(fm != NULL) {
		PangoContext *context = gtk_widget_get_pango_context(m_drawing_area);
		PangoFontMetrics *metrics = pango_context_get_metrics(
				context, desc,
				pango_context_get_language(context));
		PangoLayout *x_layout;
		PangoRectangle rect;

		x_layout = pango_layout_new(context);
		pango_layout_set_font_description(x_layout, desc);
		pango_layout_set_text(x_layout, "x", -1);
		pango_layout_get_pixel_extents(x_layout, NULL, &rect);

		fm->ascent		= pango_font_metrics_get_ascent(metrics) / PANGO_SCALE;
		fm->descent		= pango_font_metrics_get_descent(metrics) / PANGO_SCALE;
		fm->height		= fm->ascent + fm->descent;
		fm->x_height	= rect.height;

		g_object_unref(x_layout);
		pango_font_metrics_unref(metrics);
	}

	pango_font *ret = new pango_font;
	ret->font = desc;
	ret->strikethrough = (decoration & litehtml::font_decoration_linethrough) ? true : false;
	ret->underline = (decoration & litehtml::font_decoration_underline) ? true : false;

	return (litehtml::uint_ptr) ret;
}

void lh_widget::delete_font( litehtml::uint_ptr hFont )
{
	pango_font *fnt = (pango_font *)hFont;

	if (fnt != NULL) {
		pango_font_description_free(fnt->font);
		delete fnt;
	}
}

int lh_widget::text_width( const litehtml::tchar_t* text, litehtml::uint_ptr hFont )
{
	pango_font *fnt = (pango_font *) hFont;
	PangoContext *context = gtk_widget_get_pango_context(m_drawing_area);
	PangoLayout *layout = pango_layout_new(context);
	PangoRectangle rect;

	if (fnt)
		pango_layout_set_font_description(layout, fnt->font);

	pango_layout_set_text(layout, text, -1);
	pango_layout_get_pixel_extents(layout, NULL, &rect);

	g_object_unref(layout);

	return rect.width;
}

void lh_widget::draw_text( litehtml::uint_ptr hdc, const litehtml::tchar_t* text, litehtml::uint_ptr hFont, litehtml::web_color color, const litehtml::position& pos )
{
	pango_font *fnt = (pango_font *)hFont;
	cairo_t *cr = (cairo_t *)hdc;
	PangoLayout *layout = pango_cairo_create_layout(cr);
	PangoContext *context = pango_layout_get_context(layout);
	GdkScreen* screen = gdk_screen_get_default();
	double dpi = gdk_screen_get_resolution(screen);

	pango_cairo_context_set_resolution(context, dpi);

	if (fnt != NULL) {
		/* Set font */
		pango_layout_set_font_description(layout, fnt->font);

		/* Set additional font attributes */
		if (fnt->underline || fnt->strikethrough) {
			PangoAttrList *attr_list = pango_attr_list_new();
			PangoUnderline ul;

			if (fnt->underline )
				ul = PANGO_UNDERLINE_SINGLE;
			else
				ul = PANGO_UNDERLINE_NONE;

			pango_attr_list_insert(attr_list,
					pango_attr_underline_new(ul));
			pango_attr_list_insert(attr_list,
					pango_attr_strikethrough_new(fnt->strikethrough));

			pango_layout_set_attributes(layout, attr_list);
			pango_attr_list_unref(attr_list);
		}
	}

	/* Set actual text content */
	pango_layout_set_text(layout, text, -1);

	cairo_save(cr);

	/* Draw the text where it's supposed to be */
	apply_clip(cr);
	set_color(cr, color);
	cairo_move_to(cr, pos.left(), pos.top());
	pango_cairo_show_layout(cr, layout);

	/* Cleanup */
	g_object_unref(layout);
	cairo_restore(cr);
}
