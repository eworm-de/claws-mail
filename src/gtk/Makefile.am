PLUGINDIR = $(pkglibdir)/plugins/

noinst_LTLIBRARIES = libsylpheedgtk.la

libsylpheedgtk_la_SOURCES = \
	about.c \
	colorlabel.c \
	colorsel.c \
	description_window.c \
	filesel.c \
	gtkaspell.c \
	gtksctree.c \
	gtkshruler.c \
	gtkstext.c \
	gtkutils.c \
	gtkvscrollbutton.c \
	logwindow.c \
	manage_window.c \
	menu.c \
	pluginwindow.c \
	prefswindow.c \
	progressdialog.c \
	sslcertwindow.c

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
	gtkstext.h \
	gtkutils.h \
	gtkvscrollbutton.h \
	logwindow.h \
	manage_window.h \
	menu.h \
	pluginwindow.h \
	prefswindow.h \
	progressdialog.h \
	sslcertwindow.h

AM_CPPFLAGS = \
	-I../common \
	-I.. \
	$(GTK_CFLAGS) \
	$(OPENSSL_CFLAGS) \
	$(ASPELL_CFLAGS) \
	-DPLUGINDIR=\"$(PLUGINDIR)\"

libsylpheedgtk_la_LIBADD = \
	../common/libsylpheedcommon.la \
	$(GTK_LIBS) \
	$(ASPELL_LIBS)
