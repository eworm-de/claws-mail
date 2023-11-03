# Copyright 1999-2014 the Claws Mail team.
# This file is part of Claws Mail package, and distributed under the
# terms of the General Public License version 3 (or later).
# See COPYING file for license details.

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
	-avoid-version -module -z noexecstack \
	$(GTK_LIBS) \
	$(GDATA_LIBS)

gdata_la_CPPFLAGS = \
	-I$(top_builddir)/src \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_builddir)/src/common \
	-I$(top_srcdir)/src/gtk \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(ENCHANT_CFLAGS) \
	$(GDATA_CFLAGS)

.PHONY: test
