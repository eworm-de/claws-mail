# Copyright 1999-2022 the Claws Mail team.
# This file is part of Claws Mail package, and distributed under the
# terms of the General Public License version 3 (or later).
# See COPYING file for license details.

include $(srcdir)/../win_plugin.mk

EXTRA_DIST = smime.deps

IFLAGS = \
	-I$(top_builddir)/src \
	-I$(top_builddir)/src/common \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_srcdir)/src/gtk

include $(srcdir)/../win_pgpcore.mk

plugindir = $(pkglibdir)/plugins
plugin_DATA=smime.deps

if BUILD_SMIME_PLUGIN
plugin_LTLIBRARIES = smime.la
endif

smime_la_LDFLAGS = \
	$(plugin_ldflags) \
	-avoid-version -module -z noexecstack

EXTRA_smime_la_DEPENDENCIES = $(plugin_extra_deps)

smime_la_LIBADD = $(plugin_libadd) \
	$(GTK_LIBS)  \
	$(GPGME_LIBS) \
	$(LIBGPG_ERROR_LIBS) \
	$(ENCHANT_LIBS)

smime_la_CPPFLAGS = \
	$(IFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GPGME_CFLAGS) \
	$(ENCHANT_CFLAGS) \
	-Wno-deprecated-declarations

smime_la_SOURCES = \
	plugin.c \
	smime.c \
	smime.h

.PHONY: test
