SUBDIRS = tools

plugindir = $(pkglibdir)/plugins

if BUILD_PERL_PLUGIN
plugin_LTLIBRARIES = perl.la
endif

perl_la_SOURCES = \
	perl_plugin.c perl_plugin.h \
	perl_gtk.c perl_gtk.h

perl_la_LDFLAGS = \
	-avoid-version -module

perl_la_LIBADD = \
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

EXTRA_DIST = cm_perl.pod

