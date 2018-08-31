include $(top_srcdir)/tests.mk

common_ldadd = \
	$(GLIB_LIBS) \
	$(EXPAT_LIBS) \
	$(CURL_LIBS)

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-I..

TEST_PROGS += date_test
date_test_SOURCES = date_test.c
date_test_LDADD = $(common_ldadd) ../libfeed_la-date.o

TEST_PROGS += feed_test
feed_test_SOURCES = feed_test.c
feed_test_LDADD = $(common_ldadd) ../libfeed.la

noinst_PROGRAMS = $(TEST_PROGS)

.PHONY: test
