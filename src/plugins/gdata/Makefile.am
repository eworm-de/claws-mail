# Makefile.am for "src/plugins/gdata" directory
# This file is part of Claws Mail package.
# See COPYING file for license details.

appdata_files = claws-mail-gdata.metainfo.xml
appdatadir = $(datadir)/appdata
appdata_DATA = $(appdata_files)
EXTRA_DIST = $(appdata_files)

plugindir = $(pkglibdir)/plugins

if BUILD_GDATA_PLUGIN
plugin_LTLIBRARIES = gdata.la
endif

gdata_la_SOURCES = \
	gdata_plugin.c \
	gdata_plugin.h \
	cm_gdata_contacts.c \
	cm_gdata_contacts.h \
	cm_gdata_prefs.c \
	cm_gdata_prefs.h

gdata_la_LDFLAGS = \
	-avoid-version -module \
	$(GTK_LIBS) \
	$(GDATA_LIBS)

gdata_la_CPPFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_builddir)/src/common \
	-I$(top_srcdir)/src/gtk \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GDATA_CFLAGS)
