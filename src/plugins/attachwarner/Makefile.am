# Copyright 1999-2022 the Claws Mail team.
# This file is part of Claws Mail package, and distributed under the
# terms of the General Public License version 3 (or later).
# See COPYING file for license details.

include $(srcdir)/../win_plugin.mk

IFLAGS = \
	-I$(top_builddir)/src \
	-I$(top_builddir)/src/common \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_srcdir)/src/gtk

plugindir = $(pkglibdir)/plugins

if BUILD_ATTACHWARNER_PLUGIN
plugin_LTLIBRARIES = attachwarner.la
endif

attachwarner_la_LDFLAGS = \
	$(plugin_ldflags) \
	-avoid-version -module -z noexecstack

EXTRA_attachwarner_la_DEPENDENCIES = $(plugin_extra_deps)

attachwarner_la_LIBADD = $(plugin_libadd) \
	$(GTK_LIBS)

attachwarner_la_CPPFLAGS = \
	$(IFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(ENCHANT_CFLAGS)

attachwarner_la_SOURCES = \
	attachwarner.c \
	attachwarner.h \
	attachwarner_prefs.c \
	attachwarner_prefs.h

.PHONY: test
