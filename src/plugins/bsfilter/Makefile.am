# Makefile.am for "src/plugins/bsfilter" directory
# This file is part of Claws Mail package.
# See COPYING file for license details.

appdata_files = claws-mail-bsfilter.metainfo.xml
appdatadir = $(datadir)/appdata
appdata_DATA = $(appdata_files)

@INTLTOOL_XML_RULE@

EXTRA_DIST = claws.def plugin.def version.rc $(appdata_DATA) $(appdata_in_files)

IFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_builddir)/src/common \
	-I$(top_srcdir)/src/gtk

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

if BUILD_BSFILTER_PLUGIN
plugin_LTLIBRARIES = bsfilter.la
endif

bsfilter_la_LDFLAGS = \
	$(plugin_res_ldflag) $(no_undefined) $(export_symbols) \
	-avoid-version -module \
	$(GTK_LIBS) $(PTHREAD_LIBS) 

bsfilter_la_DEPENDENCIES = $(plugin_deps)

bsfilter_la_LIBADD = $(plugin_ldadd) $(cygwin_export_lib) \
	$(GTK_LIBS) $(PTHREAD_LIBS) 

bsfilter_la_CPPFLAGS = \
	$(IFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS)

bsfilter_la_SOURCES = \
	bsfilter.c bsfilter.h \
	bsfilter_gtk.c

