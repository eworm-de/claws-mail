# Copyright 1999-2014 the Claws Mail team.
# This file is part of Claws Mail package, and distributed under the
# terms of the General Public License version 3 (or later).
# See COPYING file for license details.

if BUILD_TESTS
include $(top_srcdir)/tests.mk
SUBDIRS = . tests
endif

PLUGINDIR = $(pkglibdir)/plugins/
DESKTOPFILEPATH=$(datadir)/applications/claws-mail.desktop

noinst_LTLIBRARIES = libclawscommon.la

if OS_WIN32
arch_sources = w32_reg.c
arch_headers = w32_reg.h
else
arch_files = 
arch_header = 
endif

libclawscommon_la_SOURCES = $(arch_sources) \
	codeconv.c \
	file-utils.c \
	hooks.c \
	log.c \
	md5.c \
	mgutils.c \
	passcrypt.c \
	plugin.c \
	prefs.c \
	progressindicator.c \
	proxy.c \
	quoted-printable.c \
	session.c \
	smtp.c \
	socket.c \
	ssl.c \
	ssl_certificate.c \
	string_match.c \
	stringtable.c \
	claws.c \
	tags.c \
	template.c \
	utils.c \
	uuencode.c \
	xml.c \
	xmlprops.c \
	pkcs5_pbkdf2.c \
	unmime.c

clawscommonincludedir = $(pkgincludedir)/common
clawscommoninclude_HEADERS = $(arch_headers) \
	codeconv.h \
	file-utils.h \
	defs.h \
	hooks.h \
	log.h \
	md5.h \
	mgutils.h \
	passcrypt.h \
	plugin.h \
	prefs.h \
	progressindicator.h \
	proxy.h \
	quoted-printable.h \
	session.h \
	smtp.h \
	socket.h \
	ssl_certificate.h \
	ssl.h \
	string_match.h \
	stringtable.h \
	claws.h \
	tags.h \
	template.h \
	timing.h \
	tlds.h \
	utils.h \
	uuencode.h \
	version.h \
	xml.h \
	xmlprops.h \
	pkcs5_pbkdf2.h \
	unmime.h

AM_CPPFLAGS = \
	-I$(top_srcdir)/intl \
	$(GLIB_CFLAGS) \
	$(VALGRIND_CFLAGS) \
	-DLOCALEDIR=\""$(localedir)"\" \
	-DPLUGINDIR=\"$(PLUGINDIR)\" \
	-DDATAROOTDIR=\""$(datarootdir)"\" \
	-DDESKTOPFILEPATH=\"$(DESKTOPFILEPATH)\" \
	-DGTK_DISABLE_DEPRECATED

libclawscommon_la_LIBADD = \
	$(GLIB_LIBS) \
	$(CRYPT_LIBS) \
	$(PTHREAD_LIBS)

EXTRA_DIST = \
	version.h.in
