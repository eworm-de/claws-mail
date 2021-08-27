# Copyright 1999-2021 the Claws Mail team.
# This file is part of Claws Mail package, and distributed under the
# terms of the General Public License version 3 (or later).
# See COPYING file for license details.

EXTRA_DIST = claws.def plugin.def

IFLAGS = \
	-I$(top_builddir)/src \
	-I$(top_builddir)/src/common \
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

if BUILD_MANAGESIEVE_PLUGIN
plugin_LTLIBRARIES = managesieve.la
endif

managesieve_la_LDFLAGS = \
	$(plugin_ldflags) \
	-avoid-version -module

EXTRA_managesieve_la_DEPENDENCIES = $(plugin_extra_deps)

managesieve_la_LIBADD = $(plugin_libadd) \
	$(GTK_LIBS) \
	$(CURL_LIBS)

managesieve_la_CPPFLAGS = \
	$(IFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(ENCHANT_CFLAGS) \
	$(CURL_CFLAGS)

managesieve_la_SOURCES = \
	managesieve.c \
	managesieve.h \
	sieve_editor.c \
	sieve_editor.h \
	sieve_manager.c \
	sieve_manager.h \
	sieve_plugin.c \
	sieve_prefs.c \
	sieve_prefs.h

clean-local:
	rm -f libclaws.a

.PHONY: test
