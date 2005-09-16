PLUGINDIR = $(pkglibdir)/plugins/

noinst_LTLIBRARIES = libsylpheedetpan.la

libsylpheedetpan_la_SOURCES = \
	etpan-thread-manager.c \
	etpan-log.c \
	imap-thread.c

sylpheedetpanincludedir = $(pkgincludedir)/etpan
sylpheedetpaninclude_HEADERS = \
	etpan-thread-manager-types.h \
	etpan-thread-manager.h \
	etpan-errors.h \
	etpan-log.h \
	imap-thread.h

AM_CPPFLAGS = \
	-I../common \
	-I.. \
	$(GTK_CFLAGS) \
	$(LIBETPAN_CFLAGS) 

libsylpheedetpan_la_LIBADD = \
	../common/libsylpheedcommon.la \
	$(GTK_LIBS) \
	$(LIBETPAN_LIBS)
