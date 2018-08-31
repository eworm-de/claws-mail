include $(top_srcdir)/tests.mk

common_ldadd = \
	$(GLIB_LIBS)

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common

TEST_PROGS += entity_test
entity_test_SOURCES = entity_test.c
entity_test_LDADD = $(common_ldadd) ../entity.o

noinst_PROGRAMS = $(TEST_PROGS)

.PHONY: test
