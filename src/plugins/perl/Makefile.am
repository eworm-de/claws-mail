plugindir = $(pkglibdir)/plugins

plugin_LTLIBRARIES = perl_plugin.la

perl_plugin_la_SOURCES = \
	perl_plugin.c perl_plugin.h \
	perl_gtk.c perl_gtk.h

perl_plugin_la_LDFLAGS = \
	-avoid-version -module

perl_plugin_la_LIBADD = \
	$(PERL_LDFLAGS) \
	$(GTK_LIBS)

AM_CPPFLAGS = \
	$(CLAWS_MAIL_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(PERL_CFLAGS)

INCLUDES = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_builddir)/src/common \
	-I$(top_srcdir)/src/gtk
