# Copyright 1999-2021 the Claws Mail team.
# This file is part of Claws Mail package, and distributed under the
# terms of the General Public License version 3 (or later).
# See COPYING file for license details.

if OS_WIN32
lh_cppflags = -DLITEHTML_UTF8
else
lh_cppflags =
endif

if BUILD_LITEHTML_VIEWER_PLUGIN
noinst_LTLIBRARIES = liblitehtml.la
endif

liblitehtml_la_CXXFLAGS = -std=c++11

liblitehtml_la_SOURCES = \
	background.h \
	borders.h \
	codepoint.cpp \
	codepoint.h \
	css_borders.cpp \
	css_length.cpp \
	css_length.h \
	css_margins.h \
	css_offsets.h \
	css_position.h \
	css_properties.cpp \
	css_properties.h \
	css_selector.cpp \
	css_selector.h \
	document_container.cpp \
	document_container.h \
	document.cpp \
	document.h \
	el_anchor.cpp \
	el_anchor.h \
	el_base.cpp \
	el_base.h \
	el_before_after.cpp \
	el_before_after.h \
	el_body.cpp \
	el_body.h \
	el_break.cpp \
	el_break.h \
	el_cdata.cpp \
	el_cdata.h \
	el_comment.cpp \
	el_comment.h \
	el_div.cpp \
	el_div.h \
	element.cpp \
	element.h \
	el_font.cpp \
	el_font.h \
	el_image.cpp \
	el_image.h \
	el_link.cpp \
	el_link.h \
	el_para.cpp \
	el_para.h \
	el_script.cpp \
	el_script.h \
	el_space.cpp \
	el_space.h \
	el_style.cpp \
	el_style.h \
	el_table.cpp \
	el_table.h \
	el_td.cpp \
	el_td.h \
	el_text.cpp \
	el_text.h \
	el_title.cpp \
	el_title.h \
	el_tr.cpp \
	el_tr.h \
	formatting_context.cpp \
	formatting_context.h \
	html.cpp \
	html.h \
	html_tag.cpp \
	html_tag.h \
	iterators.cpp \
	iterators.h \
	line_box.cpp \
	line_box.h \
	litehtml.h \
	Makefile.am \
	master_css.h \
	media_query.cpp \
	media_query.h \
	num_cvt.cpp \
	num_cvt.h \
	os_types.h \
	render_block_context.cpp \
	render_block_context.h \
	render_block.cpp \
	render_block.h \
	render_flex.cpp \
	render_flex.h \
	render_image.cpp \
	render_image.h \
	render_inline_context.cpp \
	render_inline_context.h \
	render_inline.cpp \
	render_inline.h \
	render_item.cpp \
	render_item.h \
	render_table.cpp \
	render_table.h \
	string_id.cpp \
	string_id.h \
	strtod.cpp \
	style.cpp \
	style.h \
	stylesheet.cpp \
	stylesheet.h \
	table.cpp \
	table.h \
	tstring_view.cpp \
	tstring_view.h \
	types.h \
	url.cpp \
	url.h \
	url_path.cpp \
	url_path.h \
	utf8_strings.cpp \
	utf8_strings.h \
	web_color.cpp \
	web_color.h

liblitehtml_la_LDFLAGS = \
        -avoid-version -module \
        $(LIBGUMBO_LIBS)

liblitehtml_la_CPPFLAGS = \
        $(lh_cppflags) \
        $(LIBGUMBO_CFLAGS)

EXTRA_DIST = \
	LICENSE \
	README.md
