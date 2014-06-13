# Makefile.am for "src/plugins/python" directory
# This file is part of Claws Mail package.
# See COPYING file for license details.

appdata_in_files = claws-mail-python.metainfo.xml.in
appdatadir=$(datadir)/appdata
appdata_DATA = $(appdata_in_files:.xml.in=.xml)

@INTLTOOL_XML_RULE@

EXTRA_DIST = $(appdata_in_files)

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
	python-hooks.c \
	python-hooks.h \
	python-shell.c \
	python-shell.h

python_la_LDFLAGS = \
	-avoid-version -module \
	$(GLIB_LIBS) \
	$(GTK_LIBS) \
	$(PYGTK_LIBS) \
	$(PYTHON_LIBS)


python_la_CPPFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_builddir)/src/common \
	-I$(top_srcdir)/src/gtk \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(PYTHON_CFLAGS) \
	$(PYGTK_CFLAGS) \
	-DPYTHON_SHARED_LIB="\"$(PYTHON_SHARED_LIB)\"" \
	-DENABLE_PYTHON \
	-fno-strict-aliasing
