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

if BUILD_BSFILTER_PLUGIN
plugin_LTLIBRARIES = bsfilter.la
endif

bsfilter_la_LDFLAGS = \
	$(plugin_ldflags) \
	-avoid-version -module

EXTRA_bsfilter_la_DEPENDENCIES = $(plugin_extra_deps)

bsfilter_la_LIBADD = $(plugin_libadd) \
	$(GTK_LIBS) \
	$(PTHREAD_LIBS)

bsfilter_la_CPPFLAGS = \
	$(IFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(ENCHANT_CFLAGS)

bsfilter_la_SOURCES = \
	bsfilter.c \
	bsfilter.h \
	bsfilter_gtk.c

clean-local:
	rm -f libclaws.a

.PHONY: test
