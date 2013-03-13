plugindir = $(pkglibdir)/plugins

if BUILD_FANCY_PLUGIN
plugin_LTLIBRARIES = fancy.la
endif

fancy_la_SOURCES = \
        fancy_viewer.c \
        fancy_viewer.h \
        fancy_prefs.c \
        fancy_prefs.h

fancy_la_LDFLAGS = \
	-avoid-version -module \
	$(GTK_LIBS) \
	$(GTKPRINTUNIX_LIBS) \
	$(WEBKIT_LIBS) \
	$(LIBSOUP_GNOME_LIBS) \
	$(CURL_LIBS)

INCLUDES = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_builddir)/src/common \
	-I$(top_srcdir)/src/gtk

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(WEBKIT_CFLAGS) \
	$(LIBSOUP_GNOME_CFLAGS) \
	$(CURL_CFLAGS)
