EXTRA_DIST=sylpheed-claws-manual.pdf
manual_DATA=sylpheed-claws-manual.pdf

sylpheed-claws-manual.pdf:
	-docbook2pdf -d ../../dtd/manual.dsl -o . ../../sylpheed-claws-manual.xml
	
clean:
	-rm sylpheed-claws-manual.pdf
