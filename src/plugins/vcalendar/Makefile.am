# Makefile.am for "src/plugins/vcalendar" directory
# This file is part of Claws Mail package.
# See COPYING file for license details.

appdata_in_files = claws-mail-vcalendar.metainfo.xml.in
appdatadir=$(datadir)/appdata
appdata_DATA = $(appdata_in_files:.xml.in=.xml)

@INTLTOOL_XML_RULE@

SUBDIRS = libical
EXTRA_DIST = claws.def plugin.def version.rc $(appdata_DATA)

IFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_builddir)/src/common \
	-I$(top_srcdir)/src/gtk \
	-I$(top_srcdir)/src/plugins/vcalendar/libical/libical

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

if BUILD_VCALENDAR_PLUGIN
plugin_LTLIBRARIES = vcalendar.la
endif

vcalendar_la_SOURCES = \
	plugin.c \
	vcalendar.c vcalendar.h \
	vcal_manager.c vcal_manager.h \
	vcal_folder.c vcal_folder.h \
	vcal_meeting_gtk.c vcal_meeting_gtk.h \
	vcal_prefs.c vcal_prefs.h \
	vcal_dbus.c vcal_dbus.h \
	icaltime_as_local.c icaltime_as_local.h \
	common-views.c common-views.h \
	day-view.c month-view.c

vcalendarincludedir = $(includedir)/claws-mail/plugins/vcalendar
vcalendarinclude_HEADERS = \
	vcal_interface.h
 
vcalendar_la_LDFLAGS = \
	$(plugin_res_ldflag) $(no_undefined) $(export_symbols) \
	-avoid-version -module \
	$(GTK_LIBS)

vcalendar_la_DEPENDENCIES = $(plugin_deps)

vcalendar_la_LIBADD = $(plugin_ldadd) $(cygwin_export_lib) \
	$(GTK_LIBS) \
	$(CURL_LIBS) \
        $(top_builddir)/src/plugins/vcalendar/libical/libical/libical.la

vcalendar_la_CPPFLAGS = \
	$(IFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(CURL_CFLAGS)

clean-local:
	rm -f libclaws.a
