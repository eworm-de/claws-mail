include $(top_srcdir)/tests.mk

common_ldadd = \
	$(GLIB_LIBS)

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	-I.. \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common

TEST_PROGS += pgp_utils_test
pgp_utils_test_SOURCES = pgp_utils_test.c
pgp_utils_test_LDADD = $(common_ldadd) ../pgpcore_la-pgp_utils.o

noinst_PROGRAMS = $(TEST_PROGS)

.PHONY: test
