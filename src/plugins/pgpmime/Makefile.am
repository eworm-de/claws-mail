plugindir = $(pkglibdir)/plugins

plugin_LTLIBRARIES = pgpmime.la

pgpmime_la_SOURCES = \
	passphrase.c passphrase.h \
	plugin.c \
	pgpmime.c pgpmime.h \
	prefs_gpg.c prefs_gpg.h \
	select-keys.c select-keys.h \
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
