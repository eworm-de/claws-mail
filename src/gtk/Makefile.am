noinst_LTLIBRARIES = libsylpheedgtk.la

libsylpheedgtk_la_SOURCES = \
	about.c about.h \
	colorlabel.c colorlabel.h \
	description_window.c description_window.h \
	filesel.c filesel.h \
	gtkaspell.c gtkaspell.h \
	gtkutils.c gtkutils.h \
	gtkshruler.c gtkshruler.h \
	gtksctree.c gtksctree.h \
	gtkstext.c gtkstext.h \
	manage_window.c manage_window.h \
	menu.c menu.h \
	pluginwindow.c pluginwindow.h \
	prefswindow.c prefswindow.h \
	sslcertwindow.c sslcertwindow.h \
	gtkvscrollbutton.c gtkvscrollbutton.h

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
