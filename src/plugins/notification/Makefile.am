# Copyright 1999-2021 the Claws Mail team.
# This file is part of Claws Mail package, and distributed under the
# terms of the General Public License version 3 (or later).
# See COPYING file for license details.

if BUILD_HOTKEYS
hotkey_lib = libcmnpgtkhotkey.la
hotkey_lib_path = $(top_builddir)/src/plugins/notification/gtkhotkey/libcmnpgtkhotkey.la
else
hotkey_lib =
hotkey_lib_path =
endif

SUBDIRS=gtkhotkey
EXTRA_DIST = claws.def plugin.def

IFLAGS = \
	-I$(top_srcdir)/src/plugins/notification/gtkhotkey \
	-I$(top_builddir)/src \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_srcdir)/src/gtk

plugin_ldflags =
plugin_extra_deps =
plugin_libadd =

if OS_WIN32

libclaws.a: claws.def
	$(DLLTOOL) --output-lib $@ --def $<

%.lo : %.rc
	$(LIBTOOL) --mode=compile --tag=RC $(RC) -i $< -o $@

plugin_ldflags += \
	-Wl,.libs/version.o \
	-no-undefined \
	-export-symbols $(srcdir)/plugin.def
plugin_extra_deps += libclaws.a version.lo plugin.def $(hotkey_lib)
plugin_libadd += -L. -lclaws

endif

if CYGWIN
plugin_ldflags += -no-undefined
plugin_libadd += -L$(top_builddir)/src -lclaws-mail
endif

plugindir = $(pkglibdir)/plugins

if BUILD_NOTIFICATION_PLUGIN
plugin_LTLIBRARIES = notification.la
endif

notification_la_LDFLAGS = \
	$(plugin_ldflags) \
	-avoid-version -module

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

clean-local:
	rm -f libclaws.a

.PHONY: test
