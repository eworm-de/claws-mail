noinst_LTLIBRARIES = libsylpheedcommon.la

libsylpheedcommon_la_SOURCES = \
	base64.c base64.h \
	defs.h \
	hooks.c hooks.h \
	intl.h \
	log.c log.h \
	md5.c md5.h \
	nntp.c nntp.h \
	plugin.c plugin.h \
	prefs.c prefs.h \
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
	version.h \
	uuencode.c uuencode.h

CPPFLAGS = \
	$(GLIB_CFLAGS) \
	$(OPENSSL_CFLAGS) \
	-DLOCALEDIR=\""$(localedir)"\"

libsylpheedcommon_la_LIBADD = \
	$(GLIB_LIBS) \
	$(OPENSSL_LIBS)

EXTRA_DIST = \
	version.h.in
