if BUILD_HOTKEYS
noinst_LTLIBRARIES = libcmnpgtkhotkey.la
endif

libcmnpgtkhotkey_la_SOURCES = \
	gtk-hotkey-info.c \
	gtk-hotkey-error.c \
	gtk-hotkey-listener.c \
	gtk-hotkey-marshal.c \
	gtk-hotkey-x11-listener.c \
	x11/tomboykeybinder.c \
	x11/eggaccelerators.c \
	gtk-hotkey-registry.c \
	gtk-hotkey-key-file-registry.c \
	gtk-hotkey-utils.c
	
cmnpgtkhotkeyincludedir = $(includedir)/claws-mail/plugins/@PACKAGE@/gtkhotkey
cmnpgtkhotkeyinclude_HEADERS = \
	gtk-hotkey-error.h \
	gtkhotkey.h \
	gtk-hotkey-info.h \
	gtk-hotkey-key-file-registry.h \
	gtk-hotkey-listener.h \
	gtk-hotkey-registry.h \
	gtk-hotkey-x11-listener.h \
	gtk-hotkey-marshal.h \
	x11/tomboykeybinder.h \
	x11/eggaccelerators.h \
	gtk-hotkey-utils.h

libcmnpgtkhotkey_la_LIBADD = \
	$(GTK_LIBS) \
	$(CM_NP_HOTKEY_LIBS)

AM_CPPFLAGS = \
	-DGTK_HOTKEY_COMPILATION \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(CM_NP_HOTKEY_CFLAGS)
