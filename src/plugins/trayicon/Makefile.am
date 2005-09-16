SUBDIRS = libeggtrayicon

plugindir = $(pkglibdir)/plugins

plugin_LTLIBRARIES = trayicon.la

trayicon_la_SOURCES = \
	trayicon.c

trayicon_la_LDFLAGS = \
	-avoid-version -module

trayicon_la_LIBADD = \
	$(GTK_LIBS) \
	libeggtrayicon/libeggtrayicon.la

INCLUDES = \
	-I../../ \
	-I../../common \
	-I../../gtk \
	-Ilibeggtrayicon

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(ASPELL_CFLAGS) \
	$(OPENSSL_CFLAGS) 

EXTRA_DIST = \
	README \
	newmail.xpm \
	newmarkedmail.xpm \
	nomail.xpm \
	unreadmail.xpm \
	unreadmarkedmail.xpm
