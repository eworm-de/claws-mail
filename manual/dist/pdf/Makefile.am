EXTRA_DIST=sylpheed-claws-manual.pdf
manualen_DATA=sylpheed-claws-manual.pdf
manualendir=$(manualdir)/en

sylpheed-claws-manual.pdf:
	-docbook2pdf -d ../../dtd/manual.dsl -o . ../../sylpheed-claws-manual.xml
	
clean:
	-rm sylpheed-claws-manual.pdf
