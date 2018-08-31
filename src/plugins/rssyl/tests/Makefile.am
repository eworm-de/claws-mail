include $(top_srcdir)/tests.mk

common_ldadd = \
	$(GLIB_LIBS)

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-I.. \
	-I$(top_srcdir)/src

TEST_PROGS += strutils_test
strutils_test_SOURCES = strutils_test.c
strutils_test_LDADD = $(common_ldadd) ../rssyl_la-strutils.o

noinst_PROGRAMS = $(TEST_PROGS)

.PHONY: test
