# Copyright 1999-2014 the Claws Mail team.
# This file is part of Claws Mail package, and distributed under the
# terms of the General Public License version 3 (or later).
# See COPYING file for license details.

if MANUAL_PDF
EXTRA_DIST=claws-mail-manual.pdf
manualde_DATA=claws-mail-manual.pdf
manualdedir=$(manualdir)/en

claws-mail-manual.pdf: \
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
	-docbook2pdf -o . "$(srcdir)/../../claws-mail-manual.xml"
	
maintainer-clean:
	-rm claws-mail-manual.pdf claws-mail-manual.fo claws-mail-manual.xsl
endif

.PHONY: test
