if BUILD_HOTKEYS
hotkey_lib = libcmnpgtkhotkey.la
hotkey_lib_path = $(top_builddir)/src/plugins/notification/gtkhotkey/libcmnpgtkhotkey.la
else
hotkey_lib =
hotkey_lib_path =
endif

SUBDIRS=gtkhotkey
EXTRA_DIST = claws.def plugin.def version.rc
CFLAGS += "-Wall"

if OS_WIN32

LTRCCOMPILE = $(LIBTOOL) --mode=compile --tag=RC $(RC) \
     `echo $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) | \
     sed -e 's/-I/--include-dir /g;s/-D/--define /g'`

%.lo : %.rc
	$(LTRCCOMPILE) -i $< -o $@

plugin_res = version.lo
plugin_res_ldflag = -Wl,.libs/version.o

export_symbols = -export-symbols $(srcdir)/plugin.def

plugin_deps = libclaws.a $(plugin_res) plugin.def $(hotkey_lib) 

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

if BUILD_NOTIFICATION_PLUGIN
plugin_LTLIBRARIES = notification.la
endif

notification_la_SOURCES = \
	notification_plugin.c notification_plugin.h \
	notification_core.c notification_core.h \
	notification_popup.c notification_popup.h \
	notification_banner.c notification_banner.h \
	notification_command.c notification_command.h \
	notification_lcdproc.c notification_lcdproc.h \
	notification_trayicon.c notification_trayicon.h \
	notification_indicator.c notification_indicator.h \
	notification_hotkeys.c notification_hotkeys.h \
	notification_prefs.c notification_prefs.h \
	notification_foldercheck.c notification_foldercheck.h \
	notification_pixbuf.c notification_pixbuf.h \
	raw_claws_mail_logo_64x64.h

notification_la_LDFLAGS = \
	$(plugin_res_ldflag) $(no_undefined) $(export_symbols) \
	-avoid-version -module

notification_la_DEPENDENCIES = $(plugin_deps) \
	$(hotkey_lib_path)

INCLUDES = \
	-I$(top_srcdir)/src/plugins/notification/gtkhotkey \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_builddir)/src/common \
	-I$(top_srcdir)/src/gtk


notification_la_LIBADD = $(plugin_ldadd) $(cygwin_export_lib) \
        $(GTK_LIBS)  \
	$(libnotify_LIBS) \
	$(libindicate_LIBS) \
	$(libcanberra_gtk_LIBS) \
	$(hotkey_lib_path)

notification_la_CPPFLAGS = \
	$(NOTIFY_CFLAGS) \
	$(CLAWS_MAIL_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	-DLOCALEDIR=\""$(localedir)"\" \
	$(libnotify_CFLAGS) \
	$(libindicate_CFLAGS) \
	$(libcanberra_gtk_CFLAGS)

clean-local:
	rm -f libclaws.a
