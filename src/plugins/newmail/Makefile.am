# Makefile.am for "src/plugins/newmail" directory
# This file is part of Claws Mail package.
# See COPYING file for license details.

appdata_files = claws-mail-newmail.metainfo.xml
appdatadir = $(datadir)/appdata
appdata_DATA = $(appdata_files)
EXTRA_DIST = $(appdata_files)

plugindir = $(pkglibdir)/plugins

if BUILD_NEWMAIL_PLUGIN
plugin_LTLIBRARIES = newmail.la
endif

newmail_la_SOURCES = \
	newmail.c

newmail_la_LDFLAGS = \
	-avoid-version -module

newmail_la_CPPFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_builddir)/src/common \
	-I$(top_srcdir)/src/gtk \
        $(GLIB_CFLAGS) \
	$(GTK_CFLAGS)
