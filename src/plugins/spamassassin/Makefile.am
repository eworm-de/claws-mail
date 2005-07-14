plugindir = $(pkglibdir)/plugins
plugin_DATA=spamassassin_gtk.deps

plugin_LTLIBRARIES = spamassassin.la spamassassin_gtk.la

spamassassin_la_SOURCES = \
	spamassassin.c spamassassin.h \
	libspamc.c libspamc.h \
	utils.c utils.h

spamassassin_la_LDFLAGS = \
	-avoid-version -module \
	$(GTK_LIBS)

spamassassin_gtk_la_SOURCES = \
	spamassassin_gtk.c spamassassin.h

spamassassin_gtk_la_LDFLAGS = \
	-avoid-version -module \
	$(GTK_LIBS) \
	$(OPENSSL_LIBS)

INCLUDES = \
	-I../.. \
	-I../../common \
	-I../../gtk

AM_CPPFLAGS = \
	$(ASPELL_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(OPENSSL_CFLAGS)

EXTRA_DIST = \
	README spamassassin_gtk.deps
