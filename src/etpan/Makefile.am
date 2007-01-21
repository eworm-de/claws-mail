PLUGINDIR = $(pkglibdir)/plugins/

noinst_LTLIBRARIES = libclawsetpan.la

libclawsetpan_la_SOURCES = \
	etpan-thread-manager.c \
	imap-thread.c

clawsetpanincludedir = $(pkgincludedir)/etpan
clawsetpaninclude_HEADERS = \
	etpan-thread-manager-types.h \
	etpan-thread-manager.h \
	etpan-errors.h \
	imap-thread.h

AM_CPPFLAGS = \
	-I$(top_srcdir)/src/common \
	-I$(top_srcdir)/src \
	$(GTK_CFLAGS) \
	$(LIBETPAN_CFLAGS) 

libclawsetpan_la_LIBADD = \
	../common/libclawscommon.la \
	$(GTK_LIBS) \
	$(LIBETPAN_LIBS)
