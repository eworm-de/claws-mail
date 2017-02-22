# Copyright 1999-2014 the Claws Mail team.
# This file is part of Claws Mail package, and distributed under the
# terms of the General Public License version 3 (or later).
# See COPYING file for license details.

EXTRA_DIST = claws.def plugin.def version.rc

if OS_WIN32

LTRCCOMPILE = $(LIBTOOL) --mode=compile --tag=RC $(RC) \
     `echo $(DEFS) $(DEFAULT_INCLUDES) $(IFLAGS) | \
     sed -e 's/-I/--include-dir /g;s/-D/--define /g'`

%.lo : %.rc
	$(LTRCCOMPILE) -i $< -o $@

plugin_res = version.lo
plugin_res_ldflag = -Wl,.libs/version.o

export_symbols = -export-symbols $(srcdir)/plugin.def

plugin_deps = libclaws.a $(plugin_res) plugin.def

libclaws.a: claws.def
	$(DLLTOOL) --output-lib $@ --def $<

plugin_ldadd = -L. -lclaws

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

if BUILD_TNEF_PARSE_PLUGIN
plugin_LTLIBRARIES = tnef_parse.la
endif

tnef_parse_la_SOURCES = \
         tnef_parse.c \
         tnef_dump.c \
         tnef_dump.h

tnef_parse_la_LDFLAGS = \
	$(plugin_res_ldflag) $(no_undefined) $(export_symbols) \
	-avoid-version -module \
	$(GTK_LIBS) \
	$(YTNEF_LIBS)

tnef_parse_la_DEPENDENCIES = $(plugin_deps)

tnef_parse_la_LIBADD = $(plugin_ldadd) $(cygwin_export_lib) \
	$(GTK_LIBS)

noinst_HEADERS = \
	tnef_dump.h

IFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_builddir)/src/common \
	-I$(top_srcdir)/src/gtk

tnef_parse_la_CPPFLAGS = \
	$(IFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(YTNEF_CFLAGS)
