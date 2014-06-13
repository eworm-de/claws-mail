# Makefile.am for "src/plugins/pdf_viewer" directory
# This file is part of Claws Mail package.
# See COPYING file for license details.

appdata_in_files = claws-mail-pdf_viewer.metainfo.xml.in
appdatadir=$(datadir)/appdata
appdata_DATA = $(appdata_in_files:.xml.in=.xml)

@INTLTOOL_XML_RULE@

EXTRA_DIST = $(appdata_in_files)

plugindir = $(pkglibdir)/plugins

if BUILD_PDF_VIEWER_PLUGIN
plugin_LTLIBRARIES = pdf_viewer.la
endif

pdf_viewer_la_SOURCES = \
         poppler_viewer.c \
         poppler_viewer.h

pdf_viewer_la_LDFLAGS = \
	-avoid-version -module \
	$(GTK_LIBS) \
	$(POPPLER_LIBS)

pdf_viewer_la_CPPFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_builddir)/src/common \
	-I$(top_srcdir)/src/gtk \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(POPPLER_CFLAGS)
