EXTRA_DIST=sylpheed-claws-manual.html
manualen_DATA=sylpheed-claws-manual.html
manualendir=$(manualdir)/en

sylpheed-claws-manual.html:
	-docbook2html -u -o . ../../sylpheed-claws-manual.xml
	
clean:
	-rm sylpheed-claws-manual.html
