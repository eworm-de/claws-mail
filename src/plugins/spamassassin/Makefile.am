# Copyright 1999-2014 the Claws Mail team.
# This file is part of Claws Mail package, and distributed under the
# terms of the General Public License version 3 (or later).
# See COPYING file for license details.

plugindir = $(pkglibdir)/plugins

if BUILD_SPAMASSASSIN_PLUGIN
plugin_LTLIBRARIES = spamassassin.la
endif

spamassassin_la_SOURCES = \
	spamassassin.c spamassassin.h \
	spamassassin_gtk.c spamassassin.h \
	libspamc.c libspamc.h \
	utils.c utils.h

spamassassin_la_LDFLAGS = \
	-avoid-version -module -z noexecstack \
	$(SPAMASSASSIN_LIBS)

if CYGWIN
cygwin_export_lib = -L$(top_builddir)/src -lclaws-mail
else
cygwin_export_lib =
endif
spamassassin_la_LIBADD = $(cygwin_export_lib) \
	$(GTK_LIBS)

spamassassin_la_CPPFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_builddir)/src \
	-I$(top_srcdir)/src/gtk \
	$(ENCHANT_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(SPAMASSASSIN_CFLAGS)

EXTRA_DIST = README NOTICE

.PHONY: test
