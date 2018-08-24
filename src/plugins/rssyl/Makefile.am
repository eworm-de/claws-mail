# Copyright 1999-2014 the Claws Mail team.
# This file is part of Claws Mail package, and distributed under the
# terms of the General Public License version 3 (or later).
# See COPYING file for license details.

SUBDIRS = libfeed

EXTRA_DIST = claws.def plugin.def version.rc

IFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_builddir)/src \
	-I$(top_srcdir)/src/gtk

if OS_WIN32

LTRCCOMPILE = $(LIBTOOL) --mode=compile --tag=RC $(RC) \
     `echo $(DEFS) $(DEFAULT_INCLUDES) $(IFLAGS) | \
     sed -e 's/-I/--include-dir /g;s/-D/--define /g'`

%.lo : %.rc
	$(LTRCCOMPILE) -i $< -o $@

plugin_res = version.lo
plugin_res_ldflag = -Wl,.libs/version.o

export_symbols = -export-symbols $(srcdir)/plugin.def

plugin_deps = libclaws.a $(plugin_res) plugin.def

libclaws.a: claws.def
	$(DLLTOOL) --output-lib $@ --def $<

plugin_ldadd = -L. -lclaws

else
plugin_res =
plugin_res_ldflag =
export_symbols =
plugin_deps =
plugin_ldadd =
endif

if PLATFORM_WIN32
no_undefined = -no-undefined
else
no_undefined =
endif

if CYGWIN
cygwin_export_lib = -L$(top_builddir)/src -lclaws-mail
else
cygwin_export_lib = 
endif

plugindir = $(pkglibdir)/plugins

if BUILD_RSSYL_PLUGIN
plugin_LTLIBRARIES = rssyl.la
endif

rssyl_la_LDFLAGS = \
	$(plugin_res_ldflag) $(no_undefined) $(export_symbols) \
	-avoid-version -module \
	$(EXPAT_LIBS) \
	$(CURL_LIBS) \
	$(GTK_LIBS)

rssyl_la_DEPENDENCIES = $(plugin_deps) libfeed/libfeed.la

rssyl_la_LIBADD = $(plugin_ldadd) $(cygwin_export_lib) \
	$(GTK_LIBS) $(CURL_LIBS) libfeed/libfeed.la

rssyl_la_CPPFLAGS = \
	$(IFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(ENCHANT_CFLAGS) \
	$(CURL_CFLAGS) \
	$(EXPAT_CFLAGS)

rssyl_la_SOURCES = \
	old_feeds.c old_feeds.h \
	opml_export.c opml_export.h \
	opml_import.c opml_import.h \
	parse822.c parse822.h \
	plugin.c \
	rssyl.c rssyl.h \
	rssyl_add_item.c rssyl_add_item.h \
	rssyl_cb_menu.c rssyl_cb_menu.h \
	rssyl_deleted.c rssyl_deleted.h \
	rssyl_feed.c rssyl_feed.h \
	rssyl_feed_props.c rssyl_feed_props.h \
	rssyl_gtk.c rssyl_gtk.h \
	rssyl_parse_feed.c rssyl_parse_feed.h \
	rssyl_prefs.c rssyl_prefs.h \
	rssyl_update_comments.c rssyl_update_comments.h \
	rssyl_update_feed.c rssyl_update_feed.h \
	rssyl_update_format.c rssyl_update_format.h \
	rssyl_subscribe.c rssyl_subscribe.h \
	rssyl_subscribe_gtk.c rssyl_subscribe_gtk.h \
	strutils.c strutils.h
