# Copyright 1999-2014 the Claws Mail team.
# This file is part of Claws Mail package, and distributed under the
# terms of the General Public License version 3 (or later).
# See COPYING file for license details.

SUBDIRS = examples

plugindir = $(pkglibdir)/plugins

if BUILD_PYTHON_PLUGIN
plugin_LTLIBRARIES = python.la
endif

python_la_SOURCES = \
	accounttype.c \
	accounttype.h \
	clawsmailmodule.c \
	clawsmailmodule.h \
	composewindowtype.c \
	composewindowtype.h \
	folderpropertiestype.c \
	folderpropertiestype.h \
	foldertype.c \
	foldertype.h \
	mailboxtype.c \
	mailboxtype.h \
	messageinfotype.c \
	messageinfotype.h \
	nodetype.c \
	nodetype.h \
	python_plugin.c \
	python_prefs.c \
	python_prefs.h \
	python-hooks.c \
	python-hooks.h \
	python-shell.c \
	python-shell.h

python_la_LDFLAGS = \
	-avoid-version -module -z noexecstack \
	$(GLIB_LIBS) \
	$(GTK_LIBS) \
	$(PYGOBJECT_LIBS) \
	$(PYTHONEMBED_LIBS) \
	$(PYTHON_LIBS)


python_la_CPPFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_builddir)/src \
	-I$(top_srcdir)/src/gtk \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(ENCHANT_CFLAGS) \
	$(PYGOBJECT_CFLAGS) \
	$(PYTHONEMBED_CFLAGS) \
	$(PYTHON_CFLAGS) \
	-DPYTHON_SHARED_LIB="\"$(PYTHONEMBED_SHARED_LIB)\"" \
	-DENABLE_PYTHON \
	-fno-strict-aliasing

.PHONY: test
