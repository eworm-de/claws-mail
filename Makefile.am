SUBDIRS = m4 po src tools config doc

EXTRA_DIST = \
	ChangeLog \
	ChangeLog.gtk1 \
	RELEASE_NOTES \
	sylpheed-claws-128x128.png \
	sylpheed-claws-64x64.png \
	sylpheed-claws.pc.in \
	sylpheed-claws.desktop \
	sylpheed-claws.png \
	autogen.sh

if SYLPHEED_GNOME2
# hicolor icon theme, base class of all icon themes
pixmapdir=$(datadir)/icons/hicolor/48x48/apps
pixmap_DATA=sylpheed-claws.png
gnomapdir = $(datadir)/applications
gnomap_DATA=sylpheed-claws.desktop
else
if SYLPHEED_GNOME
pixmapdir=$(gnomedatadir)/pixmaps
pixmap_DATA=sylpheed-claws.png
gnomapdir=$(gnomedatadir)/gnome/apps/Internet
gnomap_DATA=sylpheed-claws.desktop
else
pixmapdir=
pixmap_DATA=
gnomapdir=
gnomap_DATA=
endif
endif

BZIP2_ENV =

release: distdir
	-chmod -R a+r $(distdir)
	GZIP=$(GZIP_ENV) $(AMTAR)$(TAR) chozf $(distdir).tar.gz $(distdir)
	BZIP2=$(BZIP2_ENV) $(AMTAR)$(TAR) --bzip2 -chof $(distdir).tar.bz2 $(distdir)
	-rm -rf $(distdir)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = sylpheed-claws.pc

sylpheedincludedir = $(pkgincludedir)
sylpheedinclude_HEADERS = \
	config.h
