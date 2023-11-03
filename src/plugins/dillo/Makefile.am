# Copyright 1999-2014 the Claws Mail team.
# This file is part of Claws Mail package, and distributed under the
# terms of the General Public License version 3 (or later).
# See COPYING file for license details.

plugindir = $(pkglibdir)/plugins

if BUILD_DILLO_PLUGIN
plugin_LTLIBRARIES = dillo.la
endif

dillo_la_SOURCES = \
     dillo_viewer.c \
     dillo_prefs.c \
	 dillo_prefs.h

dillo_la_LDFLAGS = \
	-avoid-version -module -z noexecstack

if CYGWIN
cygwin_export_lib = -L$(top_builddir)/src -lclaws-mail
else
cygwin_export_lib =
endif

dillo_la_LIBADD = $(cygwin_export_lib) \
	$(GTK_LIBS) 

dillo_la_CPPFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_builddir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_srcdir)/src/gtk \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(ENCHANT_CFLAGS)

.PHONY: test
