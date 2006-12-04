PLUGINDIR = $(pkglibdir)/plugins/

noinst_LTLIBRARIES = libclawsgtk.la

EXTRA_DIST = \
	claws-marshal.list

libclawsgtk_la_SOURCES = \
	about.c \
	colorlabel.c \
	colorsel.c \
	combobox.c \
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
	claws-marshal.c

clawsgtkincludedir = $(pkgincludedir)/gtk
clawsgtkinclude_HEADERS = \
	about.h \
	authors.h \
	colorlabel.h \
	colorsel.h \
	combobox.h \
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
	claws-marshal.h

AM_CPPFLAGS = \
	-I$(srcdir)/../common \
	-I$(srcdir)/.. \
	$(GTK_CFLAGS) \
	$(OPENSSL_CFLAGS) \
	$(ASPELL_CFLAGS) \
	$(GNOMEPRINT_CFLAGS) \
	-DPLUGINDIR=\"$(PLUGINDIR)\" 

libclawsgtk_la_LIBADD = \
	../common/libclawscommon.la \
	$(GTK_LIBS) \
	$(ASPELL_LIBS) \
	$(GNOMEPRINT_LIBS)

BUILT_SOURCES=claws-marshal.c claws-marshal.h 

claws-marshal.h: claws-marshal.list
	$(GLIB_GENMARSHAL) $< --header --prefix=claws_marshal > $@

claws-marshal.c: claws-marshal.list
	$(GLIB_GENMARSHAL) $< --body --prefix=claws_marshal > $@
