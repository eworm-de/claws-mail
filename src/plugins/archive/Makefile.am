plugindir = $(pkglibdir)/plugins

if BUILD_ARCHIVE_PLUGIN
plugin_LTLIBRARIES = archive.la
endif

archive_la_SOURCES = \
	      libarchive_archive.c \
		  libarchive_archive.h \
		  archiver.c \
		  archiver.h \
		  archiver_gtk.c \
		  archiver_prefs.c

archiveincludedir = $(includedir)/claws-mail/plugins/@PACKAGE@
archiveinclude_HEADERS = \
	archiver.h \
	archiver_prefs.h

archive_la_LDFLAGS = \
		-avoid-version -module \
		$(GTK_LIBS)

archive_la_LIBADD = \
		@ARCHIVE_LIBS@

INCLUDES = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_builddir)/src/common \
	-I$(top_srcdir)/src/gtk

AM_CPPFLAGS = \
		$(CLAWS_MAIL_CFLAGS) \
		$(GLIB_CFLAGS) \
		$(GTK_CFLAGS) \
		-DLOCALEDIR=\""$(localedir)"\"
