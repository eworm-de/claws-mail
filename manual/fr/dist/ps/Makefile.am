EXTRA_DIST=sylpheed-claws-manual.ps
manualfr_DATA=sylpheed-claws-manual.ps
manualfrdir=$(manualdir)/fr

sylpheed-claws-manual.ps: \
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
	../../sylpheed-claws-manual.xml 
	-docbook2ps -o . ../../sylpheed-claws-manual.xml
	
maintainer-clean:
	-rm sylpheed-claws-manual.ps
