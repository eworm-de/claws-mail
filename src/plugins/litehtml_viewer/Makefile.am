# Copyright 1999-2022 the Claws Mail team.
# This file is part of Claws Mail package, and distributed under the
# terms of the General Public License version 3 (or later).
# See COPYING file for license details.

include $(srcdir)/../win_plugin.mk

SUBDIRS = litehtml

IFLAGS = \
	-I$(top_builddir)/src \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_srcdir)/src/gtk \
	-I$(top_srcdir)/src/plugins/litehtml_viewer/litehtml

plugin_cppflags =

if OS_WIN32

plugin_cppflags += -DLITEHTML_UTF8

endif

plugindir = $(pkglibdir)/plugins

if BUILD_LITEHTML_VIEWER_PLUGIN
plugin_LTLIBRARIES = litehtml_viewer.la
endif

litehtml_viewer_la_LDFLAGS = \
	$(plugin_ldflags) \
	-avoid-version -module -z noexecstack

EXTRA_litehtml_viewer_la_DEPENDENCIES = $(plugin_extra_deps) \
	litehtml/liblitehtml.la

litehtml_viewer_la_LIBADD = $(plugin_libadd) \
	$(GTK_LIBS) \
	$(FONTCONFIG_LIBS) \
	$(CAIRO_LIBS) \
	$(CURL_LIBS) \
	$(LIBGUMBO_LIBS) \
	litehtml/liblitehtml.la

litehtml_viewer_la_CPPFLAGS = \
	$(plugin_cppflags) \
	$(IFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(ENCHANT_CFLAGS) \
	$(FONTCONFIG_CFLAGS) \
	$(CAIRO_CFLAGS) \
	$(CURL_CFLAGS) \
	$(LIBGUMBO_CFLAGS)

litehtml_viewer_la_CXXFLAGS = -std=c++11
litehtml_viewer_la_CFLAGS = -std=c99

litehtml_viewer_la_SOURCES = \
	container_linux.cpp \
	container_linux.h \
	container_linux_images.cpp \
	container_linux_images.h \
	http.cpp \
	http.h \
	lh_prefs.c \
	lh_prefs.h \
	lh_viewer.c \
	lh_viewer.h \
	lh_widget.cpp \
	lh_widget.h \
	lh_widget_wrapped.h \
	plugin.c

.PHONY: test
