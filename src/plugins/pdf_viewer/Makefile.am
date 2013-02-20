plugindir = $(pkglibdir)/plugins

if BUILD_PDF_VIEWER_PLUGIN
plugin_LTLIBRARIES = pdf_viewer.la
endif

pdf_viewer_la_SOURCES = \
         poppler_viewer.c \
         poppler_viewer.h

pdf_viewer_la_LDFLAGS = \
	-avoid-version -module \
	$(GTK_LIBS) \
	$(POPPLER_LIBS)

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
	$(POPPLER_CFLAGS) \
	-DLOCALEDIR=\""$(localedir)"\"
