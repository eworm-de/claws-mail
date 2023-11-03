# Copyright 1999-2022 the Claws Mail team.
# This file is part of Claws Mail package, and distributed under the
# terms of the General Public License version 3 (or later).
# See COPYING file for license details.

include $(srcdir)/../win_plugin.mk

SUBDIRS = libfeed
if BUILD_TESTS
include $(top_srcdir)/tests.mk
SUBDIRS += . tests
endif

IFLAGS = \
	-I$(top_builddir)/src \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_srcdir)/src/gtk

plugindir = $(pkglibdir)/plugins

if BUILD_RSSYL_PLUGIN
plugin_LTLIBRARIES = rssyl.la
endif

rssyl_la_LDFLAGS = \
	$(plugin_ldflags) \
	-avoid-version -module -z noexecstack

EXTRA_rssyl_la_DEPENDENCIES = $(plugin_extra_deps) \
	libfeed/libfeed.la

rssyl_la_LIBADD = $(plugin_libadd) \
	$(GTK_LIBS) \
	$(CURL_LIBS) \
	$(EXPAT_LIBS) \
	libfeed/libfeed.la

rssyl_la_CPPFLAGS = \
	$(IFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(ENCHANT_CFLAGS) \
	$(CURL_CFLAGS) \
	$(EXPAT_CFLAGS)

rssyl_la_SOURCES = \
	old_feeds.c \
	old_feeds.h \
	opml_export.c \
	opml_export.h \
	opml_import.c \
	opml_import.h \
	parse822.c \
	parse822.h \
	plugin.c \
	rssyl.c \
	rssyl.h \
	rssyl_add_item.c \
	rssyl_add_item.h \
	rssyl_cb_menu.c \
	rssyl_cb_menu.h \
	rssyl_deleted.c \
	rssyl_deleted.h \
	rssyl_feed.c \
	rssyl_feed.h \
	rssyl_feed_props.c \
	rssyl_feed_props.h \
	rssyl_gtk.c \
	rssyl_gtk.h \
	rssyl_parse_feed.c \
	rssyl_parse_feed.h \
	rssyl_prefs.c \
	rssyl_prefs.h \
	rssyl_subscribe.c \
	rssyl_subscribe.h \
	rssyl_subscribe_gtk.c \
	rssyl_subscribe_gtk.h \
	rssyl_update_comments.c \
	rssyl_update_comments.h \
	rssyl_update_feed.c \
	rssyl_update_feed.h \
	rssyl_update_format.c \
	rssyl_update_format.h \
	strutils.c \
	strutils.h
