plugindir = $(pkglibdir)/plugins

plugin_LTLIBRARIES = dillo_viewer.la

dillo_viewer_la_SOURCES = \
         dillo_viewer.c \
         dillo_prefs.c dillo_prefs.h

dillo_viewer_la_LDFLAGS = \
	-avoid-version -module -no-undefined

if CYGWIN
cygwin_export_lib = -L$(top_builddir)/src -lsylpheed-claws
else
cygwin_export_lib =
endif
dillo_viewer_la_LIBADD = $(cygwin_export_lib) \
	$(GTK_LIBS)

INCLUDES = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_srcdir)/src/gtk

AM_CPPFLAGS = \
	$(ASPELL_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(OPENSSL_CFLAGS) 
