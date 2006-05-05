EXTRA_DIST=sylpheed-claws-manual.pdf
manualpl_DATA=sylpheed-claws-manual.pdf
manualpldir=$(manualdir)/pl

sylpheed-claws-manual.pdf: \
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
	-docbook2pdf -d ../../../dtd/manual.dsl -o . ../../sylpheed-claws-manual.xml
	
clean:
	-rm sylpheed-claws-manual.pdf
