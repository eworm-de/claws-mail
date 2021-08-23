# Copyright 1999-2021 the Claws Mail team.
# This file is part of Claws Mail package, and distributed under the
# terms of the General Public License version 3 (or later).
# See COPYING file for license details.

EXTRA_DIST = claws.def plugin.def version.rc

IFLAGS = \
	-I$(top_builddir)/src \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_builddir)/src/common \
	-I$(top_srcdir)/src/gtk

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
else
no_undefined =
endif

if CYGWIN
cygwin_export_lib = -L$(top_builddir)/src -lclaws-mail
else
cygwin_export_lib =
endif

plugindir = $(pkglibdir)/plugins

if BUILD_PDF_VIEWER_PLUGIN
plugin_LTLIBRARIES = pdf_viewer.la
endif

pdf_viewer_la_SOURCES = \
	poppler_viewer.c \
	poppler_viewer.h

pdf_viewer_la_LDFLAGS = \
	$(plugin_res_ldflag) $(no_undefined) $(export_symbols) \
	-avoid-version -module

pdf_viewer_la_DEPENDENCIES = $(plugin_deps)

pdf_viewer_la_LIBADD = $(plugin_ldadd) $(cygwin_export_lib) \
	$(GTK_LIBS) \
	$(POPPLER_LIBS)

pdf_viewer_la_CPPFLAGS = \
	$(IFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(ENCHANT_CFLAGS) \
	$(POPPLER_CFLAGS)

.PHONY: test
