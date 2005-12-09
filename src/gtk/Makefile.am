PLUGINDIR = $(pkglibdir)/plugins/

noinst_LTLIBRARIES = libsylpheedgtk.la

EXTRA_DIST = \
	sylpheed-marshal.list \
	authors.h

libsylpheedgtk_la_SOURCES = \
	about.c \
	colorlabel.c \
	colorsel.c \
	description_window.c \
	filesel.c \
	foldersort.c \
	gtkaspell.c \
	gtksctree.c \
	gtkshruler.c \
	gtksctree.c \
	gtksourceprintjob.c \
	gtkutils.c \
	gtkvscrollbutton.c \
	icon_legend.c \
	inputdialog.c \
	logwindow.c \
	manage_window.c \
	menu.c \
	pluginwindow.c \
	prefswindow.c \
	progressdialog.c \
	quicksearch.c \
	sslcertwindow.c \
	sylpheed-marshal.c

sylpheedgtkincludedir = $(pkgincludedir)/gtk
sylpheedgtkinclude_HEADERS = \
	about.h \
	colorlabel.h \
	colorsel.h \
	description_window.h \
	filesel.h \
	foldersort.h \
	gtkaspell.h \
	gtksctree.h \
	gtkshruler.h \
	gtksctree.h \
	gtksourceprintjob.h \
	gtkutils.h \
	gtkvscrollbutton.h \
	icon_legend.h \
	inputdialog.h \
	logwindow.h \
	manage_window.h \
	menu.h \
	pluginwindow.h \
	prefswindow.h \
	gtkvscrollbutton.h \
	progressdialog.h \
	quicksearch.h \
	sslcertwindow.h \
	sylpheed-marshal.h

AM_CPPFLAGS = \
	-I../common \
	-I.. \
	$(GTK_CFLAGS) \
	$(OPENSSL_CFLAGS) \
	$(ASPELL_CFLAGS) \
	$(GNOMEPRINT_CFLAGS) \
	-DPLUGINDIR=\"$(PLUGINDIR)\" 

libsylpheedgtk_la_LIBADD = \
	../common/libsylpheedcommon.la \
	$(GTK_LIBS) \
	$(ASPELL_LIBS) \
	$(GNOMEPRINT_LIBS)

BUILT_SOURCES=sylpheed-marshal.c sylpheed-marshal.h authors.h

sylpheed-marshal.h: sylpheed-marshal.list
	$(GLIB_GENMARSHAL) $< --header --prefix=sylpheed_marshal > $@

sylpheed-marshal.c: sylpheed-marshal.list
	$(GLIB_GENMARSHAL) $< --body --prefix=sylpheed_marshal > $@
