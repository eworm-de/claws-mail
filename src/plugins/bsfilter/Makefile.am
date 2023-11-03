# Copyright 1999-2022 the Claws Mail team.
# This file is part of Claws Mail package, and distributed under the
# terms of the General Public License version 3 (or later).
# See COPYING file for license details.

include $(srcdir)/../win_plugin.mk

IFLAGS = \
	-I$(top_builddir)/src \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_srcdir)/src/gtk

plugindir = $(pkglibdir)/plugins

if BUILD_BSFILTER_PLUGIN
plugin_LTLIBRARIES = bsfilter.la
endif

bsfilter_la_LDFLAGS = \
	$(plugin_ldflags) \
	-avoid-version -module -z noexecstack

EXTRA_bsfilter_la_DEPENDENCIES = $(plugin_extra_deps)

bsfilter_la_LIBADD = $(plugin_libadd) \
	$(GTK_LIBS) \
	$(PTHREAD_LIBS)

bsfilter_la_CPPFLAGS = \
	$(IFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(ENCHANT_CFLAGS)

bsfilter_la_SOURCES = \
	bsfilter.c \
	bsfilter.h \
	bsfilter_gtk.c

.PHONY: test
