plugindir = $(pkglibdir)/plugins

if BUILD_NEWMAIL_PLUGIN
plugin_LTLIBRARIES = newmail.la
endif

newmail_la_SOURCES = \
	newmail.c

newmail_la_LDFLAGS = \
	-avoid-version -module

AM_CPPFLAGS = \
        $(CLAWS_MAIL_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS)

INCLUDES = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_builddir)/src/common \
	-I$(top_srcdir)/src/gtk
