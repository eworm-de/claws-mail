plugindir = $(pkglibdir)/plugins

plugin_LTLIBRARIES = mathml_viewer.la

mathml_viewer_la_SOURCES = \
	mathml_viewer.c

mathml_viewer_la_LDFLAGS = \
	-avoid-version -module \
	$(GTK_LIBS) \
	$(GTK_MATH_VIEW_LIBS)

INCLUDES = \
	-I../.. \
	-I../../common \
	-I../../gtk

AM_CPPFLAGS = \
	$(ASPELL_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GTK_MATH_VIEW_CFLAGS)
