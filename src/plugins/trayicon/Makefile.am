plugindir = $(pkglibdir)/plugins

plugin_LTLIBRARIES = trayicon.la

trayicon_la_SOURCES = \
	trayicon.c \
	eggtrayicon.c eggtrayicon.h

trayicon_la_LDFLAGS = \
	-avoid-version -module \
	$(GTK_LIBS)

INCLUDES = \
	-I../../ \
	-I../../common \
	-I../../gtk

CPPFLAGS = \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(ASPELL_CFLAGS)
