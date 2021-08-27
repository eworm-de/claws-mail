# Copyright 1999-2021 the Claws Mail team.
# This file is part of Claws Mail package, and distributed under the
# terms of the General Public License version 3 (or later).
# See COPYING file for license details.

EXTRA_DIST = claws.def plugin.def mypgpcore.def smime.deps

IFLAGS = \
	-I$(top_builddir)/src \
	-I$(top_builddir)/src/common \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_srcdir)/src/gtk

plugin_ldflags =
plugin_extra_deps =
plugin_libadd =

if OS_WIN32

libclaws.a: claws.def
	$(DLLTOOL) --output-lib $@ --def $<

libmypgpcore.a: mypgpcore.def
	$(DLLTOOL) --output-lib $@ --def $<

%.lo : %.rc
	$(LIBTOOL) --mode=compile --tag=RC $(RC) -i $< -o $@

plugin_ldflags += \
	-Wl,.libs/version.o \
	-no-undefined \
	-export-symbols $(srcdir)/plugin.def
plugin_extra_deps += libclaws.a version.lo plugin.def libmypgpcore.a
plugin_libadd += -L. -lclaws -lmypgpcore

endif

if CYGWIN
plugin_ldflags += -no-undefined
plugin_libadd += -L$(top_builddir)/src -lclaws-mail ../pgpcore/pgpcore.la
endif

plugindir = $(pkglibdir)/plugins
plugin_DATA=smime.deps

if BUILD_SMIME_PLUGIN
plugin_LTLIBRARIES = smime.la
endif

smime_la_LDFLAGS = \
	$(plugin_ldflags) \
	-avoid-version -module 

EXTRA_smime_la_DEPENDENCIES = $(plugin_extra_deps)

smime_la_LIBADD = $(plugin_libadd) \
	$(GTK_LIBS)  \
	$(GPGME_LIBS) \
	$(ENCHANT_LIBS)

smime_la_CPPFLAGS = \
	$(IFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GPGME_CFLAGS) \
	$(ENCHANT_CFLAGS) \
	-Wno-deprecated-declarations

smime_la_SOURCES = \
	plugin.c \
	smime.c \
	smime.h

clean-local:
	rm -f libclaws.a libmypgpcore.a

.PHONY: test
