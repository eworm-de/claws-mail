# Makefile.am for "src/plugins/pgpcore" directory
# This file is part of Claws Mail package.
# See COPYING file for license details.

appdata_in_files = claws-mail-pgpcore.metainfo.xml.in
appdatadir=$(datadir)/appdata
appdata_DATA = $(appdata_in_files:.xml.in=.xml)

@INTLTOOL_XML_RULE@

EXTRA_DIST = version.rc plugin.def claws.def $(appdata_DATA)

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

if BUILD_PGPCORE_PLUGIN
plugin_LTLIBRARIES = pgpcore.la
endif

pgpcore_la_SOURCES = \
	autocompletion.c \
	passphrase.c \
	plugin.c \
	prefs_gpg.c \
	select-keys.c \
	sgpgme.c \
	pgp_utils.c \
	pgp_viewer.c

pluginincludedir = $(pkgincludedir)/plugins/pgpcore
plugininclude_HEADERS = \
	autocompletion.h \
	passphrase.h \
	prefs_gpg.h \
	select-keys.h \
	sgpgme.h \
	pgp_utils.h \
	pgp_viewer.h

pgpcore_la_LDFLAGS = \
	$(plugin_res_ldflag) $(no_undefined) $(export_symbols) \
	-avoid-version -module 
pgpcore_la_DEPENDENCIES = $(plugin_deps)
pgpcore_la_LIBADD = $(cygwin_export_lib) $(plugin_ldadd) \
        $(GTK_LIBS) \
	$(GPGME_LIBS) \
	$(ENCHANT_LIBS)

pgpcore_la_CPPFLAGS = \
	$(IFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GPGME_CFLAGS) \
	$(ENCHANT_CFLAGS) \
	-Wno-deprecated-declarations

clean-local:
	rm -f libclaws.a

