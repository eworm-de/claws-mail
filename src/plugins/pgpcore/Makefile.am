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

if BUILD_PGPCORE_PLUGIN
plugin_LTLIBRARIES = pgpcore.la
endif

pgpcore_la_LDFLAGS = \
	$(plugin_ldflags) \
	-avoid-version -module -z noexecstack

EXTRA_pgpcore_la_DEPENDENCIES = $(plugin_extra_deps)

pgpcore_la_LIBADD = $(plugin_libadd) \
	$(GTK_LIBS) \
	$(GPGME_LIBS) \
	$(LIBGPG_ERROR_LIBS) \
	$(ENCHANT_LIBS)

pgpcore_la_CPPFLAGS = \
	$(IFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GPGME_CFLAGS) \
	$(ENCHANT_CFLAGS) \
	-Wno-deprecated-declarations

pgpcore_la_SOURCES = \
	autocompletion.c \
	autocompletion.h \
	passphrase.c \
	passphrase.h \
	pgp_utils.c \
	pgp_utils.h \
	pgp_viewer.c \
	pgp_viewer.h \
	plugin.c \
	prefs_gpg.c \
	prefs_gpg.h \
	select-keys.c \
	select-keys.h \
	sgpgme.c \
	sgpgme.h

pluginincludedir = $(pkgincludedir)/plugins/pgpcore
plugininclude_HEADERS = \
	passphrase.h \
	pgp_utils.h \
	prefs_gpg.h \
	sgpgme.h
