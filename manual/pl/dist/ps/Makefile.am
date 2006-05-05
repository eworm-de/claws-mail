EXTRA_DIST=sylpheed-claws-manual.ps
manualpl_DATA=sylpheed-claws-manual.ps
manualpldir=$(manualdir)/pl

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
	
clean:
	-rm sylpheed-claws-manual.ps
