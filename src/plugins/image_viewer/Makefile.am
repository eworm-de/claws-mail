plugindir = $(pkglibdir)/plugins

plugin_LTLIBRARIES = image_viewer.la

image_viewer_la_SOURCES = \
	plugin.c \
	viewer.c viewer.h \
	viewerprefs.c viewerprefs.h

image_viewer_la_LDFLAGS = \
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
	$(GDK_PIXBUF_CFLAGS) \
	$(OPENSSL_CFLAGS)
