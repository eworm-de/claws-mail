plugindir = $(pkglibdir)/plugins

plugin_LTLIBRARIES = pgpmime.la

pgpmime_la_SOURCES = \
	plugin.c \
	pgpmime.c pgpmime.h \
	sgpgme.c sgpgme.h

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
