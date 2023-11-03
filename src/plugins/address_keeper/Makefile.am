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

if BUILD_ADDRESS_KEEPER_PLUGIN
plugin_LTLIBRARIES = address_keeper.la
endif

address_keeper_la_LDFLAGS = \
	$(plugin_ldflags) \
	-avoid-version -module -z noexecstack

EXTRA_address_keeper_la_DEPENDENCIES = $(plugin_extra_deps)

address_keeper_la_LIBADD = $(plugin_libadd) \
	$(GTK_LIBS)

address_keeper_la_CPPFLAGS = \
	$(IFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(ENCHANT_CFLAGS)

address_keeper_la_SOURCES = \
	address_keeper.c \
	address_keeper.h \
	address_keeper_prefs.c \
	address_keeper_prefs.h

.PHONY: test
