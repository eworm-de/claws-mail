# Copyright 1999-2017 the Claws Mail team.
# This file is part of Claws Mail package, and distributed under the
# terms of the General Public License version 3 (or later).
# See COPYING file for license details.

plugindir = $(pkglibdir)/plugins

if BUILD_ARCHIVE_PLUGIN
plugin_LTLIBRARIES = archive.la
endif

archive_la_SOURCES = \
	      libarchive_archive.c \
		  libarchive_archive.h \
		  archiver.c \
		  archiver.h \
		  archiver_gtk.c \
		  archiver_prefs.c \
		  archiver_prefs.h

archive_la_LDFLAGS = \
		-avoid-version -module -z noexecstack \
		$(GTK_LIBS) \
		$(LIBARCHIVE_LIBS)

archive_la_LIBADD = \
		@ARCHIVE_LIBS@


archive_la_CPPFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_builddir)/src \
	-I$(top_srcdir)/src/gtk \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(ENCHANT_CFLAGS) \
	$(LIBARCHIVE_CFLAGS)

.PHONY: test
