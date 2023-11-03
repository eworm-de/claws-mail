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

if BUILD_VCALENDAR_PLUGIN
plugin_LTLIBRARIES = vcalendar.la
endif

vcalendar_la_LDFLAGS = \
	$(plugin_ldflags) \
	-avoid-version -module -z noexecstack

EXTRA_vcalendar_la_DEPENDENCIES = $(plugin_extra_deps)

vcalendar_la_LIBADD = $(plugin_libadd) \
	$(GTK_LIBS) \
	$(CURL_LIBS) \
	$(LIBICAL_LIBS)

vcalendar_la_CPPFLAGS = \
	$(IFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(ENCHANT_CFLAGS) \
	$(CURL_CFLAGS) \
	$(LIBICAL_CFLAGS)

vcalendar_la_SOURCES = \
	common-views.c \
	common-views.h \
	day-view.c \
	month-view.c \
	plugin.c \
	vcal_dbus.c \
	vcal_dbus.h \
	vcal_folder.c \
	vcal_folder.h \
	vcal_interface.h \
	vcal_manager.c \
	vcal_manager.h \
	vcal_meeting_gtk.c \
	vcal_meeting_gtk.h \
	vcal_prefs.c \
	vcal_prefs.h \
	vcalendar.c \
	vcalendar.h

.PHONY: test
