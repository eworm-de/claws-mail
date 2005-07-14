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
	-avoid-version -module

INCLUDES = \
	-I../../ \
	-I../../common \
	-I../../gtk

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GPGME_CFLAGS)
