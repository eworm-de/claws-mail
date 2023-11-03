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

if BUILD_LIBRAVATAR_PLUGIN
plugin_LTLIBRARIES = libravatar.la
endif

libravatar_la_LDFLAGS = \
	$(plugin_ldflags) \
	-avoid-version -module -z noexecstack

EXTRA_libravatar_la_DEPENDENCIES = $(plugin_extra_deps)

libravatar_la_LIBADD = $(plugin_libadd) \
	$(GTK_LIBS) \
	$(CURL_LIBS)

libravatar_la_CPPFLAGS = \
	$(IFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(CURL_CFLAGS) \
	$(ENCHANT_CFLAGS)

libravatar_la_SOURCES = \
	libravatar.c \
	libravatar.h \
	libravatar_cache.c \
	libravatar_cache.h \
	libravatar_federation.c \
	libravatar_federation.h \
	libravatar_image.c \
	libravatar_image.h \
	libravatar_missing.c \
	libravatar_missing.h \
	libravatar_prefs.c \
	libravatar_prefs.h

.PHONY: test
