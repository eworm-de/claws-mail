# Copyright 1999-2022 the Claws Mail team.
# This file is part of Claws Mail package, and distributed under the
# terms of the General Public License version 3 (or later).
# See COPYING file for license details.

include $(srcdir)/../win_plugin.mk

if BUILD_HOTKEYS
hotkey_lib = libcmnpgtkhotkey.la
hotkey_lib_path = $(top_builddir)/src/plugins/notification/gtkhotkey/libcmnpgtkhotkey.la
else
hotkey_lib =
hotkey_lib_path =
endif

SUBDIRS=gtkhotkey

IFLAGS = \
	-I$(top_srcdir)/src/plugins/notification/gtkhotkey \
	-I$(top_builddir)/src \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_srcdir)/src/gtk

plugindir = $(pkglibdir)/plugins

if BUILD_NOTIFICATION_PLUGIN
plugin_LTLIBRARIES = notification.la
endif

notification_la_LDFLAGS = \
	$(plugin_ldflags) \
	-avoid-version -module -z noexecstack

EXTRA_notification_la_DEPENDENCIES = $(plugin_extra_deps) \
	$(hotkey_lib_path)

notification_la_LIBADD = $(plugin_libadd) \
	$(GTK_LIBS)  \
	$(libnotify_LIBS) \
	$(unity_LIBS) \
	$(libcanberra_gtk3_LIBS) \
	$(hotkey_lib_path)

notification_la_CPPFLAGS = \
	$(IFLAGS) \
	$(NOTIFY_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(ENCHANT_CFLAGS) \
	$(libnotify_CFLAGS) \
	$(unity_CFLAGS) \
	$(libcanberra_gtk3_CFLAGS) \
	-Wall

notification_la_SOURCES = \
	notification_banner.c \
	notification_banner.h \
	notification_command.c \
	notification_command.h \
	notification_core.c \
	notification_core.h \
	notification_foldercheck.c \
	notification_foldercheck.h \
	notification_hotkeys.c \
	notification_hotkeys.h \
	notification_indicator.c \
	notification_indicator.h \
	notification_lcdproc.c \
	notification_lcdproc.h \
	notification_pixbuf.c \
	notification_pixbuf.h \
	notification_plugin.c \
	notification_plugin.h \
	notification_popup.c \
	notification_popup.h \
	notification_prefs.c \
	notification_prefs.h \
	notification_trayicon.c \
	notification_trayicon.h

.PHONY: test
