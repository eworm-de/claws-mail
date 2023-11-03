# Copyright 1999-2022 the Claws Mail team.
# This file is part of Claws Mail package, and distributed under the
# terms of the General Public License version 3 (or later).
# See COPYING file for license details.

include $(srcdir)/../win_plugin.mk

IFLAGS = \
	-I$(top_builddir)/src \
	-I$(top_builddir)/src/common \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_srcdir)/src/gtk

plugindir = $(pkglibdir)/plugins

if BUILD_PDF_VIEWER_PLUGIN
plugin_LTLIBRARIES = pdf_viewer.la
endif

pdf_viewer_la_LDFLAGS = \
	$(plugin_ldflags) \
	-avoid-version -module -z noexecstack

EXTRA_pdf_viewer_la_DEPENDENCIES = $(plugin_extra_deps)

pdf_viewer_la_LIBADD = $(plugin_libadd) \
	$(GTK_LIBS) \
	$(POPPLER_LIBS)

pdf_viewer_la_CPPFLAGS = \
	$(IFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(ENCHANT_CFLAGS) \
	$(POPPLER_CFLAGS)

pdf_viewer_la_SOURCES = \
	poppler_viewer.c \
	poppler_viewer.h

.PHONY: test
