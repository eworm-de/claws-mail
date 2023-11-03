# Copyright 1999-2017 the Claws Mail team.
# This file is part of Claws Mail package, and distributed under the
# terms of the General Public License version 3 (or later).
# See COPYING file for license details.

SUBDIRS = libclamd
plugindir = $(pkglibdir)/plugins

if BUILD_CLAMD_PLUGIN
plugin_LTLIBRARIES = clamd.la
endif

clamd_la_SOURCES = \
	clamav_plugin.c \
	clamav_plugin_gtk.c \
	clamav_plugin.h

clamd_la_LDFLAGS = \
	-avoid-version -module -z noexecstack \
	$(GTK_LIBS) \
	-L$(top_builddir)/src/plugins/clamd/libclamd

clamd_la_LIBADD = \
	-lclamd-plugin

clamd_la_CPPFLAGS = \
	-I$(top_srcdir)/src/plugins/clamd/libclamd \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_builddir)/src \
	-I$(top_srcdir)/src/gtk \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(ENCHANT_CFLAGS)

.PHONY: test
