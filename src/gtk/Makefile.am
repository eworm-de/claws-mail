noinst_LTLIBRARIES = libsylpheedgtk.la

EXTRA_DIST = \
	sylpheed-marshal.list

libsylpheedgtk_la_SOURCES = \
	about.c \
	colorlabel.c \
	colorsel.c \
	description_window.c \
	filesel.c \
	gtkaspell.c \
	gtksctree.c \
	gtkshruler.c \
	gtksctree.c \
	gtkutils.c \
	gtkvscrollbutton.c \
	logwindow.c \
	manage_window.c \
	menu.c \
	pluginwindow.c \
	prefswindow.c \
	progressdialog.c \
	sslcertwindow.c \
	sylpheed-marshal.c

sylpheedgtkincludedir = $(pkgincludedir)/gtk
sylpheedgtkinclude_HEADERS = \
	about.h \
	colorlabel.h \
	colorsel.h \
	description_window.h \
	filesel.h \
	gtkaspell.h \
	gtksctree.h \
	gtkshruler.h \
	gtksctree.h \
	gtkutils.h \
	gtkvscrollbutton.h \
	logwindow.h \
	manage_window.h \
	menu.h \
	pluginwindow.h \
	prefswindow.h \
	gtkvscrollbutton.h \
	progressdialog.h \
	sslcertwindow.h \
	sylpheed-marshal.h

AM_CPPFLAGS = \
	-I../common \
	-I.. \
	$(GTK_CFLAGS) \
	$(OPENSSL_CFLAGS) \
	$(ASPELL_CFLAGS)

libsylpheedgtk_la_LIBADD = \
	../common/libsylpheedcommon.la \
	$(GTK_LIBS) \
	$(ASPELL_LIBS)

BUILT_SOURCES=sylpheed-marshal.c sylpheed-marshal.h

sylpheed-marshal.h: sylpheed-marshal.list
	$(GLIB_GENMARSHAL) $< --header --prefix=sylpheed_marshal > $@

sylpheed-marshal.c: sylpheed-marshal.list
	$(GLIB_GENMARSHAL) $< --body --prefix=sylpheed_marshal > $@
