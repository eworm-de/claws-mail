noinst_LTLIBRARIES = libsylpheedcommon.la

libsylpheedcommon_la_SOURCES = \
	base64.c \
	hooks.c \
	log.c \
	md5.c \
	mgutils.c \
	nntp.c \
	passcrypt.c \
	plugin.c \
	prefs.c \
	progressindicator.c \
	quoted-printable.c \
	session.c \
	smtp.c \
	socket.c \
	ssl.c \
	ssl_certificate.c \
	stringtable.c \
	sylpheed.c \
	template.c \
	utils.c \
	uuencode.c \
	xml.c \
	xmlprops.c

sylpheedcommonincludedir = $(pkgincludedir)/common
sylpheedcommoninclude_HEADERS = \
	base64.h \
	defs.h \
	hooks.h \
	intl.h \
	log.h \
	md5.h \
	mgutils.h \
	nntp.h \
	passcrypt.h \
	plugin.h \
	prefs.h \
	progressindicator.h \
	quoted-printable.h \
	session.h \
	smtp.h \
	socket.h \
	ssl_certificate.h \
	ssl.h \
	stringtable.h \
	sylpheed.h \
	template.h \
	utils.h \
	uuencode.h \
	version.h \
	xml.h \
	xmlprops.h

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	$(OPENSSL_CFLAGS) \
	-DLOCALEDIR=\""$(localedir)"\"

libsylpheedcommon_la_LIBADD = \
	$(GLIB_LIBS) \
	$(OPENSSL_LIBS) \
	-lcrypt

EXTRA_DIST = \
	version.h.in
