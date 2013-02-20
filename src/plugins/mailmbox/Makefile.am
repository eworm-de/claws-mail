EXTRA_DIST = claws.def plugin.def version.rc

if OS_WIN32

LTRCCOMPILE = $(LIBTOOL) --mode=compile --tag=RC $(RC) \
     `echo $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) | \
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

if BUILD_MAILMBOX_PLUGIN
plugin_LTLIBRARIES = mailmbox.la
endif

## Mailmbox folder plugin

mailmbox_la_SOURCES = \
	plugin.c plugin_gtk.c \
	carray.c carray.h \
	chash.c chash.h \
	clist.c clist.h \
	mailimf.c mailimf.h \
	mailimf_types.c mailimf_types.h \
	mailimf_types_helper.c mailimf_types_helper.h \
	mailimf_write.c mailimf_write.h \
	maillock.c maillock.h \
	mailmbox.c mailmbox.h \
	mailmbox_folder.c mailmbox_folder.h \
	mailmbox_parse.c mailmbox_parse.h \
	mailmbox_types.c mailmbox_types.h \
	mmapstring.c mmapstring.h \
	plugin_gtk.h

mailmbox_la_LDFLAGS = \
	-avoid-version -module

mailmbox_la_LIBADD = \
	$(GTK_LIBS)

mailmbox_la_CPPFLAGS = \
	$(CLAWS_MAIL_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS)

AM_CPPFLAGS = \
	$(CLAWS_MAIL_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS)

INCLUDES = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_builddir)/src/common \
	-I$(top_srcdir)/src/gtk
