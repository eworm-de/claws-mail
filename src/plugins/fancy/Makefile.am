# Copyright 1999-2022 the Claws Mail team.
# This file is part of Claws Mail package, and distributed under the
# terms of the General Public License version 3 (or later).
# See COPYING file for license details.

include $(srcdir)/../win_plugin.mk

IFLAGS = \
	-I$(top_builddir)/src \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_srcdir)/src/gtk

plugindir = $(pkglibdir)/plugins
webextensiondir = $(pkglibdir)/plugins/web_extensions

if BUILD_FANCY_PLUGIN
plugin_LTLIBRARIES = fancy.la
webextension_LTLIBRARIES = fancywebextension.la
endif

fancy_la_LDFLAGS = \
	$(plugin_ldflags) \
	-avoid-version -module

EXTRA_fancy_la_DEPENDENCIES = $(plugin_extra_deps)

fancy_la_LIBADD = $(plugin_libadd) \
	$(GTK_LIBS) \
	$(WEBKIT_LIBS) \
	$(LIBSOUP_GNOME_LIBS) \
	$(CURL_LIBS)

fancy_la_CPPFLAGS = \
	$(IFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(ENCHANT_CFLAGS) \
	$(WEBKIT_CFLAGS) \
	$(LIBSOUP_GNOME_CFLAGS) \
	$(CURL_CFLAGS) \
	-DFANCY_WEB_EXTENSIONS_DIR=\""$(pkglibdir)/plugins/web_extensions"\" \
	-DFANCY_WEB_EXTENSION_FILE=\""fancywebextension"\"

fancy_la_SOURCES = \
	fancy_prefs.c \
	fancy_prefs.h \
	fancy_viewer.c \
	fancy_viewer.h

fancywebextension_la_SOURCES = fancy_web_extension.c
fancywebextension_la_CFLAGS = $(WEBKIT_CFLAGS)
fancywebextension_la_LIBADD = $(WEBKIT_LIBS)
fancywebextension_la_LDFLAGS = -module -avoid-version -no-undefined

.PHONY: test
