noinst_LTLIBRARIES = libsylpheedcommon.la

libsylpheedcommon_la_SOURCES = \
	base64.c base64.h \
	hooks.c hooks.h \
	log.c log.h \
	md5.c md5.h \
	nntp.c nntp.h \
	socket.c socket.h \
	ssl.c ssl.h \
	sylpheed.c sylpheed.h \
	template.c template.h \
	utils.c utils.h \
	ssl_certificate.c ssl_certificate.h \
	uuencode.c uuencode.h

INCLUDES = \
	$(GLIB_CFLAGS) \
	$(OPENSSL_CFLAGS)

CPPFLAGS = \
	-DLOCALEDIR=\""$(localedir)"\"

libsylpheedcommon_la_LIBADD = \
	$(GLIB_LIBS) \
	$(OPENSSL_LIBS)
