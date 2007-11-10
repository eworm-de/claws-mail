plugindir = $(pkglibdir)/plugins

plugin_LTLIBRARIES = clamav_plugin.la

clamav_plugin_la_SOURCES = \
	clamav_plugin.c clamav_plugin.h \
	clamav_plugin_gtk.c clamav_plugin.h

clamav_plugin_la_LDFLAGS = \
	-avoid-version -module
if CYGWIN
cygwin_export_lib = -L$(top_builddir)/src -lclaws-mail
else
cygwin_export_lib =
endif
clamav_plugin_la_LIBADD = $(cygwin_export_lib) \
	$(GTK_LIBS) \
	$(CLAMAV_LIBS) \
	$(MAEMO_LIBS)
	
INCLUDES = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_srcdir)/src/gtk

AM_CPPFLAGS = \
	$(ASPELL_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(MAEMO_CFLAGS)

EXTRA_DIST = \
	README
