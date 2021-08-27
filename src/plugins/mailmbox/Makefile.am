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

if BUILD_MAILMBOX_PLUGIN
plugin_LTLIBRARIES = mailmbox.la
endif

mailmbox_la_LDFLAGS = \
	$(plugin_ldflags) \
	-avoid-version -module

EXTRA_mailmbox_la_DEPENDENCIES = $(plugin_extra_deps)

mailmbox_la_LIBADD = $(plugin_libadd) \
	$(GTK_LIBS)

mailmbox_la_CPPFLAGS = \
	$(IFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(ENCHANT_CFLAGS)

mailmbox_la_SOURCES = \
	carray.c \
	carray.h \
	chash.c \
	chash.h \
	clist.c \
	clist.h \
	mailimf.c \
	mailimf.h \
	mailimf_types.c \
	mailimf_types.h \
	mailimf_types_helper.c \
	mailimf_types_helper.h \
	mailimf_write.c \
	mailimf_write.h \
	maillock.c \
	maillock.h \
	mailmbox.c \
	mailmbox.h \
	mailmbox_folder.c \
	mailmbox_folder.h \
	mailmbox_parse.c \
	mailmbox_parse.h \
	mailmbox_types.c \
	mailmbox_types.h \
	mmapstring.c \
	mmapstring.h \
	plugin.c \
	plugin_gtk.c \
	plugin_gtk.h

clean-local:
	rm -f libclaws.a

.PHONY: test
