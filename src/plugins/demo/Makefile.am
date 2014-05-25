# Makefile.am for "src/plugins/demo" directory
# This file is part of Claws Mail package.
# See COPYING file for license details.

plugindir = $(pkglibdir)/plugins

if BUILD_DEMO_PLUGIN
plugin_LTLIBRARIES = demo.la
endif

demo_la_SOURCES = \
	demo.c

demo_la_LDFLAGS = \
	-avoid-version -module

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
