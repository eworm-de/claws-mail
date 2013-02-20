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

if BUILD_FETCHINFO_PLUGIN
plugin_LTLIBRARIES = fetchinfo.la
endif

fetchinfo_la_SOURCES = \
	fetchinfo_plugin.c fetchinfo_plugin.h \
	fetchinfo_plugin_gtk.c

fetchinfo_la_LDFLAGS = \
	$(plugin_res_ldflag) $(no_undefined) $(export_symbols) \
	-avoid-version -module \
	$(GTK_LIBS)

fetchinfo_la_LIBADD = $(plugin_ldadd) $(cygwin_export_lib) \
	$(GTK_LIBS)

fetchinfo_la_DEPENDENCIES = $(plugin_deps)

AM_CPPFLAGS = \
	$(CLAWS_MAIL_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS)

INCLUDES = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_builddir)/src/common \
	-I$(top_srcdir)/src/gtk

EXTRA_DIST = claws.def plugin.def version.rc
