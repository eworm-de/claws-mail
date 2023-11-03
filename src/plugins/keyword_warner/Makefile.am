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

if BUILD_KEYWORD_WARNER_PLUGIN
plugin_LTLIBRARIES = keyword_warner.la
endif

keyword_warner_la_LDFLAGS = \
	$(plugin_ldflags) \
	-avoid-version -module -z noexecstack

EXTRA_keyword_warner_la_DEPENDENCIES = $(plugin_extra_deps)

keyword_warner_la_LIBADD = $(plugin_libadd) \
	$(GTK_LIBS)

keyword_warner_la_CPPFLAGS = \
	$(IFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(ENCHANT_CFLAGS)

keyword_warner_la_SOURCES = \
	keyword_warner.c \
	keyword_warner.h \
	keyword_warner_prefs.c \
	keyword_warner_prefs.h

.PHONY: test
