plugindir = $(pkglibdir)/plugins

plugin_LTLIBRARIES = smime.la
plugin_DATA = smime.deps

smime_la_SOURCES = \
	plugin.c \
	smime.c

smime_la_LDFLAGS = \
	-avoid-version -module

smime_la_LIBADD = \
	$(GTK_LIBS) \
	$(GPGME_LIBS)

INCLUDES = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_builddir)/src/common \
	-I$(top_srcdir)/src/gtk

AM_CPPFLAGS = \
	$(CLAWS_MAIL_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GPGME_CFLAGS) \
	-Wno-deprecated-declarations

EXTRA_DIST = \
	smime.h smime.deps
