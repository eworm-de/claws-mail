plugindir = $(pkglibdir)/plugins

plugin_LTLIBRARIES = spamassassin.la

spamassassin_la_SOURCES = \
	spamassassin.c spamassassin.h \
	spamassassin_gtk.c spamassassin.h \
	libspamc.c libspamc.h \
	utils.c utils.h

spamassassin_la_LDFLAGS = \
	-avoid-version -module -no-undefined

if CYGWIN
cygwin_export_lib = -L$(top_builddir)/src -lsylpheed-claws
else
cygwin_export_lib =
endif
spamassassin_la_LIBADD = $(cygwin_export_lib) \
	$(GTK_LIBS)	\
	$(OPENSSL_LIBS)

INCLUDES = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_srcdir)/src/gtk

AM_CPPFLAGS = \
	$(ASPELL_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(OPENSSL_CFLAGS)

EXTRA_DIST = \
	README
