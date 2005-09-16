plugindir = $(pkglibdir)/plugins

plugin_LTLIBRARIES = dillo_viewer.la

dillo_viewer_la_SOURCES = \
         dillo_viewer.c \
         dillo_prefs.c dillo_prefs.h

dillo_viewer_la_LDFLAGS = \
	-avoid-version -module \
	$(GTK_LIBS)

INCLUDES = \
	-I../.. \
	-I../../common \
	-I../../gtk

AM_CPPFLAGS = \
	$(ASPELL_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(OPENSSL_CFLAGS) 
