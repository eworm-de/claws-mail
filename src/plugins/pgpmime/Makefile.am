plugindir = $(pkglibdir)/plugins
plugin_DATA=pgpmime.deps

plugin_LTLIBRARIES = pgpmime.la

EXTRA_DIST=pgpmime.deps

pgpmime_la_SOURCES = \
	plugin.c \
	pgpmime.c

pluginincludedir = $(pkgincludedir)/plugins/pgpmime
plugininclude_HEADERS = \
	pgpmime.h 

pgpmime_la_LDFLAGS = \
	-avoid-version -module

INCLUDES = \
	-I../../ \
	-I../../common \
	-I../../gtk

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GPGME_CFLAGS)
