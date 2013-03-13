EXTRA_DIST = claws.def plugin.def version.rc

if OS_WIN32

LTRCCOMPILE = $(LIBTOOL) --mode=compile --tag=RC $(RC) \
     `echo $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) | \
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

if BUILD_FANCY_PLUGIN
plugin_LTLIBRARIES = fancy.la
endif

fancy_la_DEPENDENCIES = $(plugin_deps)

fancy_la_LIBADD = $(plugin_ldadd) $(cygwin_export_lib) \
	$(GTK_LIBS)

fancy_la_SOURCES = \
        fancy_viewer.c \
        fancy_viewer.h \
        fancy_prefs.c \
        fancy_prefs.h

fancy_la_LDFLAGS = \
	$(plugin_res_ldflag) $(no_undefined) $(export_symbols) \
	-avoid-version -module \
	$(GTK_LIBS) \
	$(WEBKIT_LIBS) \
	$(LIBSOUP_GNOME_LIBS) \
	$(CURL_LIBS)

INCLUDES = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_builddir)/src/common \
	-I$(top_srcdir)/src/gtk

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(WEBKIT_CFLAGS) \
	$(LIBSOUP_GNOME_CFLAGS) \
	$(CURL_CFLAGS)
