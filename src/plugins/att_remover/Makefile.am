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

if BUILD_ATT_REMOVER_PLUGIN
plugin_LTLIBRARIES = att_remover.la
endif

att_remover_la_LDFLAGS = \
	$(plugin_ldflags) \
	-avoid-version -module -z noexecstack

EXTRA_att_remover_la_DEPENDENCIES = $(plugin_extra_deps)

att_remover_la_LIBADD = $(plugin_libadd) \
	$(GTK_LIBS)

att_remover_la_CPPFLAGS = \
	$(IFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(ENCHANT_CFLAGS)

att_remover_la_SOURCES = \
	att_remover.c

.PHONY: test
