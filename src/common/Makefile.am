noinst_LTLIBRARIES = libsylpheedcommon.la

libsylpheedcommon_la_SOURCES = \
	base64.c base64.h \
	defs.h \
	hooks.c hooks.h \
	intl.h \
	log.c log.h \
	md5.c md5.h \
	mgutils.c mgutils.h \
	nntp.c nntp.h \
	passcrypt.c passcrypt.h \
	plugin.c plugin.h \
	prefs.c prefs.h \
	progressindicator.c progressindicator.h \
	quoted-printable.c quoted-printable.h \
	session.c session.h \
	smtp.c smtp.h \
	socket.c socket.h \
	ssl_certificate.c ssl_certificate.h \
	ssl.c ssl.h \
	stringtable.c stringtable.h \
	sylpheed.c sylpheed.h \
	template.c template.h \
	utils.c utils.h \
	uuencode.c uuencode.h \
	version.h \
	xml.c xml.h \
	xmlprops.c xmlprops.h

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
