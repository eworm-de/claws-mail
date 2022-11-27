# Copyright 1999-2014 the Claws Mail team.
# This file is part of Claws Mail package, and distributed under the
# terms of the General Public License version 3 (or later).
# See COPYING file for license details.

if BUILD_TESTS
include $(top_srcdir)/tests.mk
endif

ACLOCAL_AMFLAGS = -I m4
AUTOMAKE_OPTIONS = dist-bzip2 dist-xz

SUBDIRS = po src tools config doc manual

EXTRA_DIST = \
	ChangeLog \
	ChangeLog.gtk1 \
	ChangeLog.pre2.0.0 \
	ChangeLog.2.0.0-3.0.0 \
	ChangeLog.3.0.0-3.9.0 \
	RELEASE_NOTES \
	claws-mail-128x128.png \
	claws-mail-64x64.png \
	claws-mail.pc.in \
	claws-mail.desktop \
	claws-mail.png \
	autogen.sh \
	claws-features.h.in \
	get-git-version \
	m4 \
	version

# hicolor icon theme, base class of all icon themes
pixmapdir=$(datadir)/icons/hicolor/48x48/apps
pixmap_DATA=claws-mail.png

pixmap64dir=$(datadir)/icons/hicolor/64x64/apps
pixmap64_DATA=claws-mail-64x64.png

pixmap128dir=$(datadir)/icons/hicolor/128x128/apps
pixmap128_DATA=claws-mail-128x128.png

# when changing the name or location of the .desktop file,
# also update src/common/Makefile.am
gnomapdir = $(datadir)/applications
gnomap_DATA=claws-mail.desktop

rename-icons:
	@cd $(DESTDIR)$(datadir)/icons/hicolor/64x64/apps && \
            mv claws-mail-64x64.png claws-mail.png
	@cd $(DESTDIR)$(datadir)/icons/hicolor/128x128/apps && \
            mv claws-mail-128x128.png claws-mail.png

remove-icons:
	rm -f $(DESTDIR)$(datadir)/icons/hicolor/64x64/apps/claws-mail.png
	rm -f $(DESTDIR)$(datadir)/icons/hicolor/128x128/apps/claws-mail.png

if UPDATE_GTK_ICON_CACHE
gtk_update_icon_cache = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor

install-data-hook: rename-icons update-icon-cache
uninstall-hook: remove-icons update-icon-cache

update-icon-cache:
	@-if test -z "$(DESTDIR)"; then					\
                echo "Updating Gtk icon cache.";			\
                $(gtk_update_icon_cache);				\
        else								\
                echo "*** Icon cache not updated. Remember to run:";	\
		echo "***";						\
                echo "***   $(gtk_update_icon_cache)";			\
		echo "***";						\
        fi
else

install-data-hook: rename-icons
uninstall-hook: remove-icons

endif

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = claws-mail.pc

clawsincludedir = $(pkgincludedir)
clawsinclude_HEADERS = \
	claws-features.h

relnotesdir = $(docdir)
relnotes_DATA = RELEASE_NOTES

dist-local: ChangeLog

maintainer-clean-local:
	@rm -f ChangeLog
	@rm -f version

ChangeLog: version
	@./tools/gitlog2changelog.py 3.9.0 > ChangeLog
