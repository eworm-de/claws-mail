EXTRA_DIST=sylpheed-claws-manual.txt
manualen_DATA=sylpheed-claws-manual.txt
manualendir=$(manualdir)/en

sylpheed-claws-manual.txt:
	-docbook2txt -o . ../../sylpheed-claws-manual.xml
	
clean:
	-rm sylpheed-claws-manual.txt
