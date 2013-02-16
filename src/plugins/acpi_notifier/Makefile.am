plugindir = $(pkglibdir)/plugins

plugin_LTLIBRARIES = acpi_notifier.la

acpi_notifier_la_SOURCES = \
	acpi_notifier.c

acpi_notifier_la_LDFLAGS = \
	-avoid-version -module

acpi_notifier_la_LIBADD = \
	$(GTK_LIBS)

INCLUDES = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_builddir)/src/common \
	-I$(top_srcdir)/src/gtk

AM_CPPFLAGS = \
	$(CLAWS_MAIL_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	-DLOCALEDIR=\""$(localedir)"\" 
