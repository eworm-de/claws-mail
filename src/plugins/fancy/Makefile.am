# Copyright 1999-2021 the Claws Mail team.
# This file is part of Claws Mail package, and distributed under the
# terms of the General Public License version 3 (or later).
# See COPYING file for license details.

EXTRA_DIST = claws.def plugin.def

IFLAGS = \
	-I$(top_builddir)/src \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_srcdir)/src/gtk

plugin_ldflags =
plugin_extra_deps =
plugin_libadd =

if OS_WIN32

libclaws.a: claws.def
	$(DLLTOOL) --output-lib $@ --def $<

%.lo : %.rc
	$(LIBTOOL) --mode=compile --tag=RC $(RC) -i $< -o $@

plugin_ldflags += \
	-Wl,.libs/version.o \
	-no-undefined \
	-export-symbols $(srcdir)/plugin.def
plugin_extra_deps += libclaws.a version.lo plugin.def
plugin_libadd += -L. -lclaws

endif

if CYGWIN
plugin_ldflags += -no-undefined
plugin_libadd += -L$(top_builddir)/src -lclaws-mail
endif

plugindir = $(pkglibdir)/plugins

if BUILD_FANCY_PLUGIN
plugin_LTLIBRARIES = fancy.la
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
	$(CURL_CFLAGS)

fancy_la_SOURCES = \
	fancy_prefs.c \
	fancy_prefs.h \
	fancy_viewer.c \
	fancy_viewer.h

clean-local:
	rm -f libclaws.a

.PHONY: test
