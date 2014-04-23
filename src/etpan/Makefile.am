PLUGINDIR = $(pkglibdir)/plugins/

noinst_LTLIBRARIES = libclawsetpan.la

libclawsetpan_la_SOURCES = \
	etpan-thread-manager.c \
	imap-thread.c \
	nntp-thread.c \
	etpan-ssl.c

clawsetpanincludedir = $(pkgincludedir)/etpan
clawsetpaninclude_HEADERS = \
	etpan-thread-manager-types.h \
	etpan-thread-manager.h \
	etpan-errors.h \
	imap-thread.h \
	nntp-thread.h \
	etpan-ssl.h

INCLUDES = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_srcdir)/src/gtk

AM_CPPFLAGS = \
	$(GTK_CFLAGS) \
	$(LIBETPAN_CFLAGS) \
	$(ENCHANT_CFLAGS)

libclawsetpan_la_LIBADD = \
	../common/libclawscommon.la \
	$(GTK_LIBS) \
	$(LIBETPAN_LIBS) \
	$(ENCHANT_LIBS)
