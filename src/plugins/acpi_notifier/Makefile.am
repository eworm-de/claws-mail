# Makefile.am for "src/plugins/acpi_notifier" directory
# This file is part of Claws Mail package.
# See COPYING file for license details.

appdata_files = claws-mail-acpi_notifier.metainfo.xml
appdatadir = $(datadir)/appdata
appdata_DATA = $(appdata_files)
EXTRA_DIST = $(appdata_files)

plugindir = $(pkglibdir)/plugins

if BUILD_ACPI_NOTIFIER_PLUGIN
plugin_LTLIBRARIES = acpi_notifier.la
endif

acpi_notifier_la_SOURCES = \
	acpi_notifier.c

acpi_notifier_la_LDFLAGS = \
	-avoid-version -module

acpi_notifier_la_LIBADD = \
	$(GTK_LIBS)


acpi_notifier_la_CPPFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_builddir)/src/common \
	-I$(top_srcdir)/src/gtk \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS)
