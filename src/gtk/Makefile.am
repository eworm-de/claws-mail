noinst_LTLIBRARIES = libsylpheedgtk.la

libsylpheedgtk_la_SOURCES = \
	gtksctree.c gtksctree.h \
	gtkstext.c gtkstext.h \
	sslcertwindow.c sslcertwindow.h

INCLUDES = \
	-I../common \
	$(GTK_CFLAGS)

libsylpheedgtk_la_LIBADD = \
	../common/libsylpheedcommon.la \
	$(GTK_LIBS)
