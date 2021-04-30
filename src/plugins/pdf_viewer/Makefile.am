# Copyright 1999-2014 the Claws Mail team.
# This file is part of Claws Mail package, and distributed under the
# terms of the General Public License version 3 (or later).
# See COPYING file for license details.

plugindir = $(pkglibdir)/plugins

if BUILD_PDF_VIEWER_PLUGIN
plugin_LTLIBRARIES = pdf_viewer.la
endif

pdf_viewer_la_SOURCES = \
         poppler_viewer.c \
         poppler_viewer.h

pdf_viewer_la_LDFLAGS = \
	-avoid-version -module \
	$(GTK_LIBS) \
	$(POPPLER_LIBS)

pdf_viewer_la_CPPFLAGS = \
	-I$(top_builddir)/src \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_builddir)/src/common \
	-I$(top_srcdir)/src/gtk \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(ENCHANT_CFLAGS) \
	$(POPPLER_CFLAGS)

.PHONY: test
