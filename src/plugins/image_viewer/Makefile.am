plugindir = $(pkglibdir)/plugins

plugin_LTLIBRARIES = image_viewer.la

image_viewer_la_SOURCES = \
	plugin.c \
	viewer.c viewer.h \
	viewerprefs.c viewerprefs.h

image_viewer_la_LDFLAGS = \
	-avoid-version -module \
	$(GTK_LIBS) \
	$(GDK_IMLIB_LIBS) \
	$(GDK_PIXBUF_LIBS)

INCLUDES = \
	-I../.. \
	-I../../common \
	-I../../gtk

CPPFLAGS = \
	$(ASPELL_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GDK_IMLIB_CFLAGS) \
	$(GDK_PIXBUF_CFLAGS)
