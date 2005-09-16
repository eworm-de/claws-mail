plugindir = $(pkglibdir)/plugins
plugin_DATA=clamav_plugin_gtk.deps

plugin_LTLIBRARIES = clamav_plugin.la clamav_plugin_gtk.la

clamav_plugin_la_SOURCES = \
	clamav_plugin.c clamav_plugin.h

clamav_plugin_la_LDFLAGS = \
	-avoid-version -module \
	$(GTK_LIBS) \
	$(CLAMAV_LIBS)
	
clamav_plugin_gtk_la_SOURCES = \
	clamav_plugin_gtk.c clamav_plugin.h

clamav_plugin_gtk_la_LDFLAGS = \
	-avoid-version -module \
	$(GTK_LIBS)

INCLUDES = \
	-I../.. \
	-I../../common \
	-I../../gtk

AM_CPPFLAGS = \
	$(ASPELL_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) 

EXTRA_DIST = \
	README clamav_plugin_gtk.deps
