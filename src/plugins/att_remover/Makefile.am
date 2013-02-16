plugindir = $(pkglibdir)/plugins

plugin_LTLIBRARIES = att_remover.la

att_remover_la_SOURCES = \
	att_remover.c

att_remover_la_LDFLAGS = \
	-avoid-version -module

att_remover_la_LIBADD = \
	$(GTK_LIBS)

AM_CPPFLAGS = \
	$(CLAWS_MAIL_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS)

INCLUDES = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_builddir)/src/common \
	-I$(top_srcdir)/src/gtk
