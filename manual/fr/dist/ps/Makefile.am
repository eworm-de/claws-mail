EXTRA_DIST=claws-mail-manual.ps
manualfr_DATA=claws-mail-manual.ps
manualfrdir=$(manualdir)/fr

claws-mail-manual.ps: \
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
	-docbook2ps -o . ../../claws-mail-manual.xml
	
maintainer-clean:
	-rm claws-mail-manual.ps
