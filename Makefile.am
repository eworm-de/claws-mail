SUBDIRS = m4 po src tools config intl doc

EXTRA_DIST = \
	ChangeLog.claws \
	README.claws \
	RELEASE_NOTES.claws \
	sylpheed-128x128.png \
	sylpheed-64x64.png \
	sylpheed-claws.pc.in \
	sylpheed.desktop \
	sylpheed.png \
	sylpheed.spec \
	sylpheed.spec.in

if SYLPHEED_GNOME
pixmapdir=$(gnomedatadir)/pixmaps
pixmap_DATA=sylpheed.png
gnomapdir=$(gnomedatadir)/gnome/apps/Internet
gnomap_DATA=sylpheed.desktop
else
pixmapdir=
pixmap_DATA=
gnomapdir=
gnomap_DATA=
endif

BZIP2_ENV =

release: distdir
	-chmod -R a+r $(distdir)
	GZIP=$(GZIP_ENV) $(AMTAR)$(TAR) chozf $(distdir).tar.gz $(distdir)
	rm -f $(distdir)/sylpheed.spec
	sed '/^Source:/s/\.tar\.gz$$/\.tar\.bz2/' sylpheed.spec \
	  > $(distdir)/sylpheed.spec
	BZIP2=$(BZIP2_ENV) $(AMTAR)$(TAR) --bzip2 -chof $(distdir).tar.bz2 $(distdir)
	-rm -rf $(distdir)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = sylpheed-claws.pc

sylpheedincludedir = $(pkgincludedir)
sylpheedinclude_HEADERS = \
	config.h
