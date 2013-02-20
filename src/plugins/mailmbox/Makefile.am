plugindir = $(pkglibdir)/plugins

if BUILD_MAILMBOX_PLUGIN
plugin_LTLIBRARIES = mailmbox.la
endif

## Mailmbox folder plugin

mailmbox_la_SOURCES = \
	plugin.c plugin_gtk.c \
	carray.c carray.h \
	chash.c chash.h \
	clist.c clist.h \
	mailimf.c mailimf.h \
	mailimf_types.c mailimf_types.h \
	mailimf_types_helper.c mailimf_types_helper.h \
	mailimf_write.c mailimf_write.h \
	maillock.c maillock.h \
	mailmbox.c mailmbox.h \
	mailmbox_folder.c mailmbox_folder.h \
	mailmbox_parse.c mailmbox_parse.h \
	mailmbox_types.c mailmbox_types.h \
	mmapstring.c mmapstring.h \
	plugin_gtk.h

mailmbox_la_LDFLAGS = \
	-avoid-version -module

mailmbox_la_LIBADD = \
	$(GTK_LIBS)

mailmbox_la_CPPFLAGS = \
	$(CLAWS_MAIL_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS)

AM_CPPFLAGS = \
	$(CLAWS_MAIL_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS)

INCLUDES = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_builddir)/src/common \
	-I$(top_srcdir)/src/gtk
