# Makefile.am for "src/plugins/archive" directory
# This file is part of Claws Mail package.
# See COPYING file for license details.
#
appdata_in_files = claws-mail-archive.metainfo.xml.in
appdatadir=$(datadir)/appdata
appdata_DATA = $(appdata_in_files:.xml.in=.xml)

@INTLTOOL_XML_RULE@

EXTRA_DIST = $(appdata_in_files)

plugindir = $(pkglibdir)/plugins

if BUILD_ARCHIVE_PLUGIN
plugin_LTLIBRARIES = archive.la
endif

archive_la_SOURCES = \
	      libarchive_archive.c \
		  libarchive_archive.h \
		  archiver.c \
		  archiver.h \
		  archiver_gtk.c \
		  archiver_prefs.c

archiveincludedir = $(includedir)/claws-mail/plugins/@PACKAGE@
archiveinclude_HEADERS = \
	archiver.h \
	archiver_prefs.h

archive_la_LDFLAGS = \
		-avoid-version -module \
		$(GTK_LIBS)

archive_la_LIBADD = \
		@ARCHIVE_LIBS@


archive_la_CPPFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_builddir)/src/common \
	-I$(top_srcdir)/src/gtk \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS)
