SUBDIRS = ac po intl libkcc src manual

EXTRA_DIST = \
	ChangeLog.jp \
	INSTALL.jp \
	README.jp \
	TODO.jp \
	sylpheed.spec \
	sylpheed.spec.in \
	autogen.sh

BZIP2_ENV =

release: distdir
	-chmod -R a+r $(distdir)
	GZIP=$(GZIP_ENV) $(TAR) chozf $(distdir).tar.gz $(distdir)
	BZIP2=$(BZIP2_ENV) $(TAR) chojf $(distdir).tar.bz2 $(distdir)
	-rm -rf $(distdir)
