# Makefile.am for "src/plugins/spamassassin" directory
# This file is part of Claws Mail package.
# See COPYING file for license details.

appdata_in_files = claws-mail-spamassassin.metainfo.xml.in
appdatadir=$(datadir)/appdata
appdata_DATA = $(appdata_in_files:.xml.in=.xml)

@INTLTOOL_XML_RULE@

plugindir = $(pkglibdir)/plugins

if BUILD_SPAMASSASSIN_PLUGIN
plugin_LTLIBRARIES = spamassassin.la
endif

spamassassin_la_SOURCES = \
	spamassassin.c spamassassin.h \
	spamassassin_gtk.c spamassassin.h \
	libspamc.c libspamc.h \
	utils.c utils.h

spamassassin_la_LDFLAGS = \
	-avoid-version -module

if CYGWIN
cygwin_export_lib = -L$(top_builddir)/src -lclaws-mail
else
cygwin_export_lib =
endif
spamassassin_la_LIBADD = $(cygwin_export_lib) \
	$(GTK_LIBS)

spamassassin_la_CPPFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_builddir)/src/common \
	-I$(top_srcdir)/src/gtk \
	$(ENCHANT_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS)

EXTRA_DIST = README $(appdata_DATA) $(appdata_in_files)
