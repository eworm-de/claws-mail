noinst_LTLIBRARIES = libsylpheedgtk.la

libsylpheedgtk_la_SOURCES = \
	gtkshruler.c gtkshruler.h \
	gtksctree.c gtksctree.h \
	gtkstext.c gtkstext.h \
	manage_window.c manage_window.h \
	menu.c menu.h \
	prefswindow.c prefswindow.h \
	sslcertwindow.c sslcertwindow.h

INCLUDES = \
	-I../common \
	$(GTK_CFLAGS)

libsylpheedgtk_la_LIBADD = \
	../common/libsylpheedcommon.la \
	$(GTK_LIBS)
