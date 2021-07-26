# Copyright 1999-2018 the Claws Mail team.
# This file is part of Claws Mail package, and distributed under the
# terms of the General Public License version 3 (or later).
# See COPYING file for license details.

SUBDIRS = litehtml
EXTRA_DIST = claws.def plugin.def version.rc css.inc

IFLAGS = \
        -I$(top_srcdir)/src \
        -I$(top_srcdir)/src/common \
        -I$(top_builddir)/src \
        -I$(top_srcdir)/src/gtk \
        -I$(top_srcdir)/src/plugins/litehtml_viewer/litehtml

if OS_WIN32

LTRCCOMPILE = $(LIBTOOL) --mode=compile --tag=RC $(RC) \
		`echo $(DEFS) $(DEFAULT_INCLUDES) $(IFLAGS) | \
		sed -e 's/-I/--include-dir /g;s/-D/--define /g'`

%.lo: %.rc
	$(LTRCCOMPILE) -i $< -o $@

plugin_res = version.lo
plugin_res_ldflag = -Wl,.libs/version.o
export_symbols = -export-symbols $(srcdir)/plugin.def
plugin_deps = libclaws.a $(plugin_res) plugin.def
plugin_ldadd = -L. -lclaws

libclaws.a: claws.def
	$(DLLTOOL) --output-lib $@ --def $<
else
plugin_res =
plugin_res_ldflag =
export_symbols =
plugin_deps =
plugin_ldadd =
endif

if PLATFORM_WIN32
no_undefined = -no-undefined
def_win32 = -DWIN32
else
no_undefined =
def_win32 =
endif

export def_win32

if CYGWIN
cygwin_export_lib = -L$(top_builddir)/src -lclaws-mail
else
cygwin_export_lib =
endif

plugindir = $(pkglibdir)/plugins

if BUILD_LITEHTML_VIEWER_PLUGIN
plugin_LTLIBRARIES = litehtml_viewer.la
endif

litehtml_viewer_la_DEPENDENCIES = $(plugin_deps) litehtml/liblitehtml.la

litehtml_viewer_la_LIBADD = $(plugin_ldadd) $(cygwin_export_lib) \
	$(top_builddir)/src/plugins/litehtml_viewer/litehtml/liblitehtml.la \
        $(GTK_LIBS)

litehtml_viewer_la_CXXFLAGS = -std=c++11
litehtml_viewer_la_CFLAGS = -std=c99

litehtml_viewer_la_SOURCES = \
	container_linux.cpp \
	container_linux_images.cpp \
	plugin.c \
	lh_prefs.c \
	lh_viewer.c \
	lh_widget.cpp \
	lh_widget_text.cpp \
	container_linux.h \
	lh_prefs.h \
	lh_viewer.h \
	lh_widget.h \
	lh_widget_wrapped.h \
	http.h \
	http.cpp

litehtml_viewer_la_LDFLAGS = \
	$(plugin_res_ldflag) $(no_undefined) $(export_symbols) \
	-avoid-version -module \
	$(GTK_LIBS) \
	$(FONTCONFIG_LIBS) \
	$(CAIRO_LIBS) \
	$(CURL_LIBS) \
	$(LIBGUMBO_LIBS)

litehtml_viewer_la_CPPFLAGS = \
	$(def_win32) \
	$(IFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(ENCHANT_CFLAGS) \
	$(FONTCONFIG_CFLAGS) \
	$(CAIRO_CFLAGS) \
	$(CURL_CFLAGS) \
	$(LIBGUMBO_CFLAGS)

.PHONY: test

