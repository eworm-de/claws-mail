plugindir = $(pkglibdir)/plugins

if BUILD_QR_AVATAR_PLUGIN
plugin_LTLIBRARIES = qr-avatar.la
endif

qr_avatar_la_SOURCES = \
	qr-avatar.c

qr_avatar_la_LDFLAGS = \
	$(GTK_LIBS) \
	$(QRENCODE_LIBS) \
	-avoid-version -module

AM_CPPFLAGS = \
	$(GTK_CFLAGS) \
	$(QRENCODE_CFLAGS)

INCLUDES = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_builddir)/src/common \
	-I$(top_srcdir)/src/gtk \
	-I/usr/include
