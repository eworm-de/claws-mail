# Copyright 1999-2014 the Claws Mail team.
# This file is part of Claws Mail package, and distributed under the
# terms of the General Public License version 3 (or later).
# See COPYING file for license details.

if BUILD_HOTKEYS
noinst_LTLIBRARIES = libcmnpgtkhotkey.la
endif

AUTOMAKE_OPTIONS = subdir-objects

libcmnpgtkhotkey_la_SOURCES = \
	gtk-hotkey-info.c gtk-hotkey-info.h \
	gtk-hotkey-error.c gtk-hotkey-error.h \
	gtk-hotkey-listener.c gtk-hotkey-listener.h \
	gtk-hotkey-marshal.c gtk-hotkey-marshal.h \
	gtk-hotkey-x11-listener.c gtk-hotkey-x11-listener.h \
	x11/tomboykeybinder.c x11/tomboykeybinder.h \
	x11/eggaccelerators.c x11/eggaccelerators.h \
	gtk-hotkey-registry.c gtk-hotkey-registry.h \
	gtk-hotkey-key-file-registry.c gtk-hotkey-key-file-registry.h \
	gtk-hotkey-utils.c gtk-hotkey-utils.h \
	gtkhotkey.h
	
libcmnpgtkhotkey_la_LIBADD = \
	$(GTK_LIBS) \
	$(CM_NP_HOTKEY_LIBS)

libcmnpgtkhotkey_la_CPPFLAGS = \
	-DGTK_HOTKEY_COMPILATION \
        -I$(top_srcdir)/src/common \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(CM_NP_HOTKEY_CFLAGS)

.PHONY: test
