# Copyright 1999-2022 the Claws Mail team.
# This file is part of Claws Mail package, and distributed under the
# terms of the General Public License version 3 (or later).
# See COPYING file for license details.

include $(srcdir)/../win_plugin.mk

EXTRA_DIST = pgpinline.deps

IFLAGS = \
	-I$(top_builddir)/src/common \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_srcdir)/src/gtk

include $(srcdir)/../win_pgpcore.mk

plugindir = $(pkglibdir)/plugins
plugin_DATA=pgpinline.deps

if BUILD_PGPINLINE_PLUGIN
plugin_LTLIBRARIES = pgpinline.la
endif

pgpinline_la_LDFLAGS = \
	$(plugin_ldflags) \
	-avoid-version -module -z noexecstack

EXTRA_pgpinline_la_DEPENDENCIES = $(plugin_extra_deps)

pgpinline_la_LIBADD = $(plugin_libadd) \
	$(GTK_LIBS)  \
	$(GPGME_LIBS)

pgpinline_la_CPPFLAGS = \
	$(IFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GPGME_CFLAGS) \
	-Wno-deprecated-declarations

pgpinline_la_SOURCES = \
	pgpinline.c \
	pgpinline.h \
	plugin.c

.PHONY: test
