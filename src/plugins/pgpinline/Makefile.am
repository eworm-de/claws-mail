plugindir = $(pkglibdir)/plugins
plugin_DATA=pgpinline.deps

plugin_LTLIBRARIES = pgpinline.la

EXTRA_DIST=pgpinline.deps

pgpinline_la_SOURCES = \
	plugin.c \
	pgpinline.c

pluginincludedir = $(pkgincludedir)/plugins/pgpinline
plugininclude_HEADERS = \
	pgpinline.h 

pgpinline_la_LDFLAGS = \
	-avoid-version -module \
        $(GTK_LIBS) \
	$(GPGME_LIBS)
		

INCLUDES = \
	-I../../ \
	-I../../common \
	-I../../gtk

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GPGME_CFLAGS) \
	-Wno-deprecated-declarations
