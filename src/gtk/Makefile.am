noinst_LTLIBRARIES = libsylpheedgtk.la

libsylpheedgtk_la_SOURCES = \
	about.c \
	colorlabel.c \
	colorsel.c \
	description_window.c \
	filesel.c \
	gtkaspell.c \
	gtkutils.c \
	gtkshruler.c \
	gtksctree.c \
	gtkstext.c \
	manage_window.c \
	menu.c \
	pluginwindow.c \
	prefswindow.c \
	sslcertwindow.c \
	gtkvscrollbutton.c

sylpheedgtkincludedir = $(pkgincludedir)/gtk
sylpheedgtkinclude_HEADERS = \
	about.h \
	colorlabel.h \
	colorsel.h \
	description_window.h \
	filesel.h \
	gtkaspell.h \
	gtkutils.h \
	gtkshruler.h \
	gtksctree.h \
	gtkstext.h \
	manage_window.h \
	menu.h \
	pluginwindow.h \
	prefswindow.h \
	sslcertwindow.h \
	gtkvscrollbutton.h

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
