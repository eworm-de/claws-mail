plugindir = $(pkglibdir)/plugins

plugin_LTLIBRARIES = pgpmime.la

pgpmime_la_SOURCES = \
	passphrase.c \
	plugin.c \
	pgpmime.c \
	prefs_gpg.c \
	select-keys.c \
	sgpgme.c

pluginincludedir = $(pkgincludedir)/plugins/pgpmime
plugininclude_HEADERS = \
	passphrase.h \
	pgpmime.h \
	prefs_gpg.h \
	select-keys.h \
	sgpgme.h

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
