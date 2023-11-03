# Copyright 1999-2014 the Claws Mail team.
# This file is part of Claws Mail package, and distributed under the
# terms of the General Public License version 3 (or later).
# See COPYING file for license details.

plugindir = $(pkglibdir)/plugins

if BUILD_DEMO_PLUGIN
plugin_LTLIBRARIES = demo.la
endif

demo_la_SOURCES = \
	demo.c

demo_la_LDFLAGS = \
	-avoid-version -module -z noexecstack

if CYGWIN
cygwin_export_lib = -L$(top_builddir)/src -lclaws-mail
else
cygwin_export_lib = 
endif

demo_la_LIBADD = $(cygwin_export_lib) \
	$(GTK_LIBS) 

demo_la_CPPFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_builddir)/src/common \
	-I$(top_srcdir)/src/common \
	$(GLIB_CFLAGS)

.PHONY: test
