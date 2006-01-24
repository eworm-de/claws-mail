EXTRA_DIST=sylpheed-claws-manual.txt
manualen_DATA=sylpheed-claws-manual.txt
manualendir=$(manualdir)/en

sylpheed-claws-manual.txt: \
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
	-docbook2txt -o . ../../sylpheed-claws-manual.xml
	
clean:
	-rm sylpheed-claws-manual.txt
