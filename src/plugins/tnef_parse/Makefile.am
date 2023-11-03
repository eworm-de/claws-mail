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

if BUILD_TNEF_PARSE_PLUGIN
plugin_LTLIBRARIES = tnef_parse.la
endif

tnef_parse_la_LDFLAGS = \
	$(plugin_ldflags) \
	-avoid-version -module -z noexecstack

EXTRA_tnef_parse_la_DEPENDENCIES = $(plugin_extra_deps)

tnef_parse_la_LIBADD = $(plugin_libadd) \
	$(GTK_LIBS) \
	$(YTNEF_LIBS)

tnef_parse_la_CPPFLAGS = \
	$(IFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(YTNEF_CFLAGS)

tnef_parse_la_SOURCES = \
	tnef_dump.c \
	tnef_dump.h \
	tnef_parse.c

.PHONY: test
