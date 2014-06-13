# Makefile.am for "src/plugins/clamd" directory
# This file is part of Claws Mail package.
# See COPYING file for license details.

appdata_in_files = claws-mail-clamd.metainfo.xml.in
appdatadir=$(datadir)/appdata
appdata_DATA = $(appdata_in_files:.xml.in=.xml)

@INTLTOOL_XML_RULE@

EXTRA_DIST = $(appdata_in_files)

SUBDIRS = libclamd
plugindir = $(pkglibdir)/plugins

if BUILD_CLAMD_PLUGIN
plugin_LTLIBRARIES = clamd.la
endif

clamd_la_SOURCES = \
	clamav_plugin.c \
	clamav_plugin_gtk.c \
	clamav_plugin.h

clamd_la_LDFLAGS = \
	-avoid-version -module \
	$(GTK_LIBS) \
	-L$(top_builddir)/src/plugins/clamd/libclamd

clamd_la_LIBADD = \
	-lclamd-plugin

clamd_la_CPPFLAGS = \
	-I$(top_srcdir)/src/plugins/clamd/libclamd \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_builddir)/src/common \
	-I$(top_srcdir)/src/gtk \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS)
