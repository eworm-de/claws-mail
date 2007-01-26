SUBDIRS = libeggtrayicon

plugindir = $(pkglibdir)/plugins

plugin_LTLIBRARIES = trayicon.la

if CYGWIN
cygwin_export_lib = -L$(top_builddir)/src -lclaws-mail
else
cygwin_export_lib = 
endif 

trayicon_la_SOURCES = \
	trayicon.c \
	trayicon_prefs.c \
	trayicon_prefs.h

trayicon_la_LDFLAGS = \
	-avoid-version -module -no-undefined

trayicon_la_LIBADD = libeggtrayicon/libeggtrayicon.la \
	$(cygwin_export_lib) \
	$(GTK_LIBS) \
	-L/usr/X11R6/lib -lX11

INCLUDES = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_srcdir)/src/gtk \
	-I$(srcdir)/libeggtrayicon

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
	unreadmarkedmail.xpm \
	newmail.offline.xpm \
	newmarkedmail.offline.xpm \
	nomail.offline.xpm \
	unreadmail.offline.xpm \
	unreadmarkedmail.offline.xpm
