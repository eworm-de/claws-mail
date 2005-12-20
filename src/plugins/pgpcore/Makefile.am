EXTRA_DIST = version.rc plugin.def sylpheed.def

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

plugin_deps = libsylpheed.a $(plugin_res) plugin.def

libsylpheed.a: sylpheed.def
	$(DLLTOOL) --output-lib $@ --def $<

plugin_ldadd = -L . -lsylpheed

else
plugin_res =
plugin_res_ldflag =
no_undefined =
export_symbols =
plugin_deps =
plugin_ldadd =
endif


plugindir = $(pkglibdir)/plugins

plugin_LTLIBRARIES = pgpcore.la

pgpcore_la_SOURCES = \
	passphrase.c \
	plugin.c \
	prefs_gpg.c \
	select-keys.c \
	sgpgme.c

pluginincludedir = $(pkgincludedir)/plugins/pgpcore
plugininclude_HEADERS = \
	passphrase.h \
	prefs_gpg.h \
	select-keys.h \
	sgpgme.h

pgpcore_la_LDFLAGS = \
	$(plugin_res_ldflag) $(no_undefined) $(export_symbols) \
	-avoid-version -module \
        $(GTK_LIBS) \
	$(GPGME_LIBS)
pgpcore_la_DEPENDENCIES = $(plugin_deps)
pgpcore_la_LIBADD = $(plugin_ldadd)

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

