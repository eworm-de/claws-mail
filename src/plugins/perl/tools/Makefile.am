# Copyright 1999-2014 the Claws Mail team.
# This file is part of Claws Mail package, and distributed under the
# terms of the General Public License version 3 (or later).
# See COPYING file for license details.

EXTRA_TOOLS = \
	matcherrc2perlfilter.pl

EXTRA_DIST = \
	$(EXTRA_TOOLS)

MAKE_EXE = chmod u+x $(EXTRA_TOOLS)

all-local:
	for file in $(EXTRA_TOOLS); do \
	if [ ! -e ${top_builddir}/src/plugins/perl/tools/$$file ]; then \
	todir=${top_builddir}/src/plugins/perl/tools; \
	dir=$$(dirname $$file); \
	if [ ! $$dir = . ]; then \
	todir=$$todir/$$dir; \
	fi; \
	cp ${top_srcdir}/src/plugins/perl/tools/$$file $$todir; \
	fi; \
	done;
	$(MAKE_EXE)

distclean-local:
	if [ ! ${top_builddir} = ${top_srcdir} ]; then \
	for file in $(EXTRA_TOOLS); do \
	rm -f $$file; \
	done; \
	fi

.PHONY: test
