ACLOCAL_AMFLAGS = -I m4
AUTOMAKE_OPTIONS = dist-bzip2

if PLATFORM_WIN32
tools = 
else
tools = tools
endif

SUBDIRS = m4 po src $(tools) config doc manual

EXTRA_DIST = \
	ChangeLog \
	ChangeLog.gtk1 \
	RELEASE_NOTES \
	claws-mail-128x128.png \
	claws-mail-64x64.png \
	claws-mail.pc.in \
	claws-mail.desktop \
	claws-mail.png \
	autogen.sh

if CLAWS_GNOME2
# hicolor icon theme, base class of all icon themes
pixmapdir=$(datadir)/icons/hicolor/48x48/apps
pixmap_DATA=claws-mail.png
gnomapdir = $(datadir)/applications
gnomap_DATA=claws-mail.desktop

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

else
if CLAWS_GNOME
pixmapdir=$(gnomedatadir)/pixmaps
pixmap_DATA=claws-mail.png
gnomapdir=$(gnomedatadir)/gnome/apps/Internet
gnomap_DATA=claws-mail.desktop
else
pixmapdir=
pixmap_DATA=
gnomapdir=
gnomap_DATA=
endif
endif

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = claws-mail.pc

clawsincludedir = $(pkgincludedir)
clawsinclude_HEADERS = \
	config.h

relnotesdir = $(docdir)
relnotes_DATA = RELEASE_NOTES
