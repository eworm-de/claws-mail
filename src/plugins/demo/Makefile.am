plugindir = $(pkglibdir)/plugins

plugin_LTLIBRARIES = demo.la

demo_la_SOURCES = \
	demo.c

demo_la_LDFLAGS = \
	-avoid-version -module

INCLUDES = \
	-I../../common

CPPFLAGS = \
	$(GLIB_CFLAGS)
