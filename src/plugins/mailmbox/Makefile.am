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

if BUILD_MAILMBOX_PLUGIN
plugin_LTLIBRARIES = mailmbox.la
endif

mailmbox_la_LDFLAGS = \
	$(plugin_ldflags) \
	-avoid-version -module -z noexecstack

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

.PHONY: test
