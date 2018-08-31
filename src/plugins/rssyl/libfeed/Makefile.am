if BUILD_RSSYL_PLUGIN
noinst_LTLIBRARIES = libfeed.la

if BUILD_TESTS
include $(top_srcdir)/tests.mk
SUBDIRS = . tests
endif

endif

libfeed_la_CPPFLAGS = \
	-Wall \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir)/src

libfeed_la_SOURCES = \
	date.c date.h \
	feed.c feed.h \
	feeditem.c feeditem.h \
	feeditemenclosure.c feeditemenclosure.h \
	parser.c parser.h \
	parser_atom10.c parser_atom10.h \
	parser_opml.c parser_opml.h \
	parser_rdf.c parser_rdf.h \
	parser_rss20.c parser_rss20.h
