plugindir = $(pkglibdir)/plugins

if BUILD_PYTHON_PLUGIN
plugin_LTLIBRARIES = python.la
endif

python_la_SOURCES = \
	clawsmailmodule.c \
	clawsmailmodule.h \
	composewindowtype.c \
	composewindowtype.h \
	foldertype.c \
	foldertype.h \
	messageinfotype.c \
	messageinfotype.h \
	nodetype.c \
	nodetype.h \
	python_plugin.c \
	python-hooks.c \
	python-hooks.h \
	python-shell.c \
	python-shell.h

python_la_LDFLAGS = \
	-avoid-version -module \
	$(GLIB_LIBS) \
	$(GTK_LIBS) \
	$(PYGTK_LIBS) \
	$(PYTHON_LIBS)

INCLUDES = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_builddir)/src/common \
	-I$(top_srcdir)/src/gtk

AM_CPPFLAGS = \
	-Wall \
	$(CLAWS_MAIL_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(PYTHON_CFLAGS) \
	$(PYGTK_CFLAGS) \
	-DPYTHON_SHARED_LIB="\"$(PYTHON_SHARED_LIB)\"" \
	-DENABLE_PYTHON \
	-fno-strict-aliasing \
	-DLOCALEDIR=\""$(localedir)"\"
