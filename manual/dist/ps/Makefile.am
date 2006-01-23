EXTRA_DIST=sylpheed-claws-manual.ps
manualen_DATA=sylpheed-claws-manual.ps
manualendir=$(manualdir)/en

sylpheed-claws-manual.ps:
	-docbook2ps -o . ../../sylpheed-claws-manual.xml
	
clean:
	-rm sylpheed-claws-manual.ps
