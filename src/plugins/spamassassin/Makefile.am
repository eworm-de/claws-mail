plugindir = $(pkglibdir)/plugins

plugin_LTLIBRARIES = spamassassin.la

spamassassin_la_SOURCES = \
	spamassassin.c \
	libspamc.c libspamc.h \
	utils.c utils.h

spamassassin_la_LDFLAGS = \
	-avoid-version -module \
	$(GTK_LIBS)

INCLUDES = \
	-I../.. \
	-I../../common \
	-I../../gtk

CPPFLAGS = \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS)

EXTRA_DIST = \
	README

