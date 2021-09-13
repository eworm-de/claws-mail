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
	attributes.h \
	background.cpp background.h \
	borders.h \
	box.cpp box.h \
	context.cpp context.h \
	css_length.cpp css_length.h \
	css_margins.h css_offsets.h css_position.h \
	css_selector.cpp css_selector.h \
	document.cpp document.h \
	el_anchor.cpp el_anchor.h \
	el_base.cpp el_base.h \
	el_before_after.cpp el_before_after.h \
	el_body.cpp el_body.h \
	el_break.cpp el_break.h \
	el_cdata.cpp el_cdata.h \
	el_comment.cpp el_comment.h \
	el_div.cpp el_div.h \
	element.cpp element.h \
	el_font.cpp el_font.h \
	el_image.cpp el_image.h \
	el_li.cpp el_li.h \
	el_link.cpp el_link.h \
	el_para.cpp el_para.h \
	el_script.cpp el_script.h \
	el_space.cpp el_space.h \
	el_style.cpp el_style.h \
	el_table.cpp el_table.h \
	el_td.cpp el_td.h \
	el_text.cpp el_text.h \
	el_title.cpp el_title.h \
	el_tr.cpp el_tr.h \
	html.cpp html.h \
	html_tag.cpp html_tag.h \
	iterators.cpp iterators.h \
	litehtml.h \
	media_query.cpp media_query.h \
	num_cvt.cpp num_cvt.h \
	os_types.h \
	style.cpp style.h \
	stylesheet.cpp stylesheet.h \
	table.cpp table.h \
	types.h \
	utf8_strings.cpp utf8_strings.h \
	web_color.cpp web_color.h

liblitehtml_la_LDFLAGS = \
        -avoid-version -module \
        $(LIBGUMBO_LIBS)

liblitehtml_la_CPPFLAGS = \
        $(lh_cppflags) \
        $(LIBGUMBO_CFLAGS)

EXTRA_DIST = \
	LICENSE \
	README.md
