EXTRA_DIST=claws-mail-manual.html
manualpl_DATA=claws-mail-manual.html
manualpldir=$(manualdir)/pl

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
	-docbook2html -u -o . ../../claws-mail-manual.xml
	
maintainer-clean:
	-rm claws-mail-manual.html
