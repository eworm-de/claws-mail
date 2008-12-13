ACLOCAL_AMFLAGS = -I m4
AUTOMAKE_OPTIONS = dist-bzip2

SUBDIRS = m4 po src tools config doc manual

EXTRA_DIST = \
	ChangeLog \
	ChangeLog.gtk1 \
	ChangeLog.pre2.0.0 \
	ChangeLog.2.0.0-3.0.0 \
	RELEASE_NOTES \
	claws-mail-128x128.png \
	claws-mail-64x64.png \
	claws-mail.pc.in \
	claws-mail.desktop \
	claws-mail.png \
	autogen.sh

# hicolor icon theme, base class of all icon themes
pixmapdir=$(datadir)/icons/hicolor/48x48/apps
pixmap_DATA=claws-mail.png
gnomapdir = $(datadir)/applications
gnomap_DATA=claws-mail.desktop

if CLAWS_GNOME2
gtk_update_icon_cache = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor

install-data-hook:
	@-if test -z "$(DESTDIR)"; then					\
                echo "Updating Gtk icon cache.";			\
                $(gtk_update_icon_cache);				\
        else								\
                echo "*** Icon cache not updated. Remember to run:";	\
		echo "***";						\
                echo "***   $(gtk_update_icon_cache)";			\
		echo "***";						\
        fi
endif

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = claws-mail.pc

clawsincludedir = $(pkgincludedir)
clawsinclude_HEADERS = \
	config.h

relnotesdir = $(docdir)
relnotes_DATA = RELEASE_NOTES
