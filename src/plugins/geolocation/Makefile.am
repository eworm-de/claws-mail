plugindir = $(pkglibdir)/plugins

if BUILD_GEOLOCATION_PLUGIN
plugin_LTLIBRARIES = geolocation.la
endif

geolocation_la_SOURCES = \
	geolocation_plugin.c \
	pixmap_earth.h

geolocation_la_LDFLAGS = \
	-avoid-version -module \
	$(GTK_LIBS) \
	$(LIBSOUP_LIBS) \
	$(CHAMPLAIN_LIBS)


INCLUDES = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_builddir)/src/common \
	-I$(top_srcdir)/src/gtk

AM_CPPFLAGS = \
	-Wall \
	$(CLAWS_MAIL_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(LIBSOUP_CFLAGS) \
	$(CHAMPLAIN_CFLAGS) \
	-DLOCALEDIR=\""$(localedir)"\"
