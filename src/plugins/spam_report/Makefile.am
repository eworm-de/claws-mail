plugindir = $(pkglibdir)/plugins

if BUILD_SPAM_REPORT_PLUGIN
plugin_LTLIBRARIES = spamreport.la
endif

spamreport_la_SOURCES = \
         spam_report.c \
	 spam_report_prefs.c \
	 spam_report_prefs.h

spamreport_la_LDFLAGS = \
	-avoid-version -module \
	$(GTK_LIBS)

spamreport_la_LIBADD = \
	$(CURL_LIBS) \
	$(GTK_LIBS)

INCLUDES = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_builddir)/src/common \
	-I$(top_srcdir)/src/gtk

AM_CPPFLAGS = \
	-Wall \
	$(CLAWS_MAIL_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(CURL_CFLAGS) \
	$(GTK_CFLAGS) \
	-DLOCALEDIR=\""$(localedir)"\"
