EXTRA_DIST=sylpheed-claws-manual.html
manuales_DATA=sylpheed-claws-manual.html
manualesdir=$(manualdir)/es

sylpheed-claws-manual.html: \
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
	-docbook2html -u -o . ../../sylpheed-claws-manual.xml
	
maintainer-clean:
	-rm sylpheed-claws-manual.html
