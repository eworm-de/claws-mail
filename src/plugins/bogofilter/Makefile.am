# Copyright 1999-2014 the Claws Mail team.
# This file is part of Claws Mail package, and distributed under the
# terms of the General Public License version 3 (or later).
# See COPYING file for license details.

plugindir = $(pkglibdir)/plugins

if BUILD_BOGOFILTER_PLUGIN
plugin_LTLIBRARIES = bogofilter.la
endif

bogofilter_la_SOURCES = \
	bogofilter.c bogofilter.h \
	bogofilter_gtk.c 

bogofilter_la_LDFLAGS = \
	-avoid-version -module -z noexecstack

if CYGWIN
cygwin_export_lib = -L$(top_builddir)/src -lclaws-mail
else
cygwin_export_lib =
endif
bogofilter_la_LIBADD = $(cygwin_export_lib) \
	$(GTK_LIBS)

bogofilter_la_CPPFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_builddir)/src \
	-I$(top_srcdir)/src/gtk \
	$(ENCHANT_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS)

.PHONY: test
