SUBDIRS = ac po intl libkcc src manual faq tools

EXTRA_DIST = \
	ChangeLog.jp \
	INSTALL.jp \
	README.jp \
	ChangeLog.claws \
	README.claws \
	TODO.jp \
	sylpheed.desktop \
	sylpheed.spec \
	sylpheed.spec.in \
	sylpheed.png \
	sylpheed-128x128.png \
	sylpheed-64x64.png \
	autogen.sh 

if SYLPHEED_GNOME
pixmapdir=$(gnomedir)/share/pixmaps
pixmap_DATA=sylpheed.png
gnomapdir=$(gnomedir)/share/gnome/apps/Internet
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
	GZIP=$(GZIP_ENV) $(TAR) chozf $(distdir).tar.gz $(distdir)
	rm -f $(distdir)/sylpheed.spec
	sed '/^Source:/s/\.tar\.gz$$/\.tar\.bz2/' sylpheed.spec \
	  > $(distdir)/sylpheed.spec
	BZIP2=$(BZIP2_ENV) $(TAR) --bzip2 -chof $(distdir).tar.bz2 $(distdir)
	-rm -rf $(distdir)
