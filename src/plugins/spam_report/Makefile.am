# Copyright 1999-2022 the Claws Mail team.
# This file is part of Claws Mail package, and distributed under the
# terms of the General Public License version 3 (or later).
# See COPYING file for license details.

include $(srcdir)/../win_plugin.mk

IFLAGS = \
	-I$(top_builddir)/src \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_srcdir)/src/gtk

plugindir = $(pkglibdir)/plugins

if BUILD_SPAM_REPORT_PLUGIN
plugin_LTLIBRARIES = spamreport.la
endif

spamreport_la_LDFLAGS = \
	$(plugin_ldflags) \
	-avoid-version -module -z noexecstack

EXTRA_spamreport_la_DEPENDENCIES = $(plugin_extra_deps)

spamreport_la_LIBADD = $(plugin_libadd) \
	$(GTK_LIBS) \
	$(CURL_LIBS)

spamreport_la_CPPFLAGS = \
	$(IFLAGS) \
	$(GLIB_CFLAGS) \
	$(CURL_CFLAGS) \
	$(GTK_CFLAGS) \
	$(ENCHANT_CFLAGS)

spamreport_la_SOURCES = \
	spam_report.c \
	spam_report_prefs.c \
	spam_report_prefs.h

.PHONY: test
