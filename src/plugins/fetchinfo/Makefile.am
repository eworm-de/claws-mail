plugindir = $(pkglibdir)/plugins

plugin_LTLIBRARIES = fetchinfo_plugin.la

fetchinfo_plugin_la_SOURCES = \
	fetchinfo_plugin.c fetchinfo_plugin.h \
	fetchinfo_plugin_gtk.c

fetchinfo_plugin_la_LDFLAGS = \
	-avoid-version -module \
	$(GTK_LIBS)

AM_CPPFLAGS = \
	$(CLAWS_MAIL_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS)

INCLUDES = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_srcdir)/src/gtk

EXTRA_DIST=
