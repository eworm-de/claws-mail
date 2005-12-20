EXTRA_DIST = version.rc plugin.def sylpheed.def mypgpcore.def pgpmime.deps

if PLATFORM_WIN32

LTRCCOMPILE = $(LIBTOOL) --mode=compile $(RC) \
     `echo $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) | \
     sed -e 's/-I/--include-dir /g;s/-D/--define /g'`

%.lo : %.rc
	$(LTRCCOMPILE) -i $< -o $@

plugin_res = version.lo
plugin_res_ldflag = -Wl,.libs/version.o

no_undefined = -no-undefined
export_symbols = -export-symbols $(srcdir)/plugin.def

plugin_deps = libsylpheed.a libmypgpcore.a $(plugin_res) plugin.def

libsylpheed.a: sylpheed.def
	$(DLLTOOL) --output-lib $@ --def $<

libmypgpcore.a: mypgpcore.def
	$(DLLTOOL) --output-lib $@ --def $<

plugin_ldadd = -L . -lsylpheed -lmypgpcore

else
plugin_res =
plugin_res_ldflag =
no_undefined =
export_symbols =
plugin_deps =
plugin_ldadd =
endif


plugindir = $(pkglibdir)/plugins
plugin_DATA=pgpmime.deps

plugin_LTLIBRARIES = pgpmime.la


pgpmime_la_SOURCES = \
	plugin.c \
	pgpmime.c

pluginincludedir = $(pkgincludedir)/plugins/pgpmime
plugininclude_HEADERS = \
	pgpmime.h 

pgpmime_la_LDFLAGS = \
	$(plugin_res_ldflag) $(no_undefined) $(export_symbols) \
	-avoid-version -module \
        $(GTK_LIBS) \
	$(GPGME_LIBS)
pgpmime_la_DEPENDENCIES = $(plugin_deps)
pgpmime_la_LIBADD = $(plugin_ldadd)

INCLUDES = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_srcdir)/src/gtk

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GPGME_CFLAGS) \
	-Wno-deprecated-declarations

clean-local:
	rm -f libsylpheed.a
