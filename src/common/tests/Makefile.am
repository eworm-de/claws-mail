include $(top_srcdir)/tests.mk

common_ldadd = \
	$(GLIB_LIBS)

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-DLOCALEDIR=\""$(localedir)"\" \
	-DPLUGINDIR=\"$(PLUGINDIR)\" \
	-I$(top_srcdir)/src \
	-I..

TEST_PROGS += xml_test
xml_test_SOURCES = xml_test.c
xml_test_LDADD = $(common_ldadd) ../xml.o ../stringtable.o ../utils.o ../codeconv.o ../quoted-printable.o ../unmime.o

TEST_PROGS += codeconv_test
codeconv_test_SOURCES = codeconv_test.c
codeconv_test_LDADD = $(common_ldadd) ../codeconv.o ../utils.o ../quoted-printable.o ../unmime.o

TEST_PROGS += md5_test
md5_test_SOURCES = md5_test.c
md5_test_LDADD = $(common_ldadd) ../md5.o

TEST_PROGS += pkcs5_pbkdf2_test
pkcs5_pbkdf2_test_SOURCES = pkcs5_pbkdf2_test.c
pkcs5_pbkdf2_test_LDADD = $(common_ldadd) ../pkcs5_pbkdf2.o

noinst_PROGRAMS = $(TEST_PROGS)

.PHONY: test
