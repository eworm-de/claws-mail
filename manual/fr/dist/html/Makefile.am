# Copyright 1999-2014 the Claws Mail team.
# This file is part of Claws Mail package, and distributed under the
# terms of the General Public License version 3 (or later).
# See COPYING file for license details.

if MANUAL_HTML
EXTRA_DIST=claws-mail-manual.html
manualde_DATA=claws-mail-manual.html
manualdedir=$(manualdir)/fr

claws-mail-manual.html: \
	../../account.xml \
	../../ack.xml \
	../../addrbook.xml \
	../../advanced.xml \
	../../faq.xml \
	../../glossary.xml \
	../../gpl.xml \
	../../handling.xml \
	../../intro.xml \
	../../keyboard.xml \
	../../plugins.xml \
	../../starting.xml \
	../../claws-mail-manual.xml 
	-SP_ENCODING=xml $(DOCBOOK2HTML) -u -o . \
		../../claws-mail-manual.xml
	-mv -f claws-mail-manual.html claws-mail-manual.html.raw
	-$(SED) 's,^><META,><META http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\"><META,' < claws-mail-manual.html.raw > claws-mail-manual.html
	
maintainer-clean:
	-rm claws-mail-manual.html
	-rm claws-mail-manual.html.raw
endif

.PHONY: test
