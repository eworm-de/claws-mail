libclamd_plugin_la_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(CLAWS_MAIL_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_builddir) \
        -I$(top_srcdir)/src \
        -I$(top_srcdir)/src/common \
        -I$(top_srcdir)/src/gtk

if BUILD_CLAMD_PLUGIN
noinst_LTLIBRARIES = libclamd-plugin.la
endif

libclamd_plugin_la_SOURCES = \
      clamd-plugin.h \
      clamd-plugin.c

noinst_HEADERS = clamd-plugin.h

libclamd_plugin_la_LIBADD = \
	@GLIB_LIBS@ \
	@GTK_LIBS@
