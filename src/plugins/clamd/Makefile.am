SUBDIRS = libclamd
plugindir = $(pkglibdir)/plugins

if BUILD_CLAMD_PLUGIN
plugin_LTLIBRARIES = clamd.la
endif

clamd_la_SOURCES = \
	clamav_plugin.c \
	clamav_plugin_gtk.c \
	clamav_plugin.h

clamd_la_LDFLAGS = \
	-avoid-version -module \
	$(GTK_LIBS) \
	-L$(top_builddir)/src/plugins/clamd/libclamd

clamd_la_LIBADD = \
	-lclamd-plugin

INCLUDES = \
	-I$(top_srcdir)/src/plugins/clamd/libclamd \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_builddir)/src/common \
	-I$(top_srcdir)/src/gtk

AM_CPPFLAGS = \
	-Wall \
	$(CLAWS_MAIL_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	-DLOCALEDIR=\""$(localedir)"\"
