# Copyright 1999-2014 the Claws Mail team.
# This file is part of Claws Mail package, and distributed under the
# terms of the General Public License version 3 (or later).
# See COPYING file for license details.

plugindir = $(pkglibdir)/plugins

if BUILD_ACPI_NOTIFIER_PLUGIN
plugin_LTLIBRARIES = acpi_notifier.la
endif

acpi_notifier_la_SOURCES = \
	acpi_notifier.c

acpi_notifier_la_LDFLAGS = \
	-avoid-version -module -z noexecstack

acpi_notifier_la_LIBADD = \
	$(GTK_LIBS)


acpi_notifier_la_CPPFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_builddir)/src/common \
	-I$(top_srcdir)/src/gtk \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS)

.PHONY: test
