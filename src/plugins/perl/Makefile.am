# Makefile.am for "src/plugins/perl" directory
# This file is part of Claws Mail package.
# See COPYING file for license details.

SUBDIRS = tools

plugindir = $(pkglibdir)/plugins

if BUILD_PERL_PLUGIN
plugin_LTLIBRARIES = perl.la
endif

perl_la_SOURCES = \
	perl_plugin.c perl_plugin.h \
	perl_gtk.c perl_gtk.h

perl_la_LDFLAGS = \
	-avoid-version -module

perl_la_LIBADD = \
	$(PERL_LDFLAGS) \
	$(GTK_LIBS)

perl_la_CPPFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_builddir)/src/common \
	-I$(top_srcdir)/src/gtk \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(PERL_CFLAGS)

EXTRA_DIST = cm_perl.pod

