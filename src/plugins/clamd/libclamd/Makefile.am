# Copyright 1999-2017 the Claws Mail team.
# This file is part of Claws Mail package, and distributed under the
# terms of the General Public License version 3 (or later).
# See COPYING file for license details.

libclamd_plugin_la_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I$(top_builddir)/src \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_srcdir)/src/gtk

if BUILD_CLAMD_PLUGIN
noinst_LTLIBRARIES = libclamd-plugin.la
endif

libclamd_plugin_la_SOURCES = \
      clamd-plugin.h \
      clamd-plugin.c

noinst_HEADERS = clamd-plugin.h

libclamd_plugin_la_LIBADD = \
	@GLIB_LIBS@ \
	@GTK_LIBS@

.PHONY: test
