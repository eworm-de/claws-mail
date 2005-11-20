# Makefile for Sylpheed-Claws manual generation from DocBook sources.
# Copyright 2005 by Ricardo Mones and the Sylpheed-Claws Team.
# Inspired by "The Debian SGML/XML HOWTO" by Stéphane Bortzmeyer.
# This is distributed according to the General Public License.
#

MAIN=sylpheed-claws-manual
XMLS=$(glob *.xml)
OUT=dist

all: $(MAIN)

$(MAIN): $(OUT) $(MAIN).pdf $(MAIN).ps $(MAIN).txt $(MAIN).html
	@echo "Generation Ok."

$(OUT):
	mkdir -p $(OUT)

$(MAIN).pdf: $(XMLS)
	docbook2pdf -d dtd/manual.dsl -o $(OUT)/pdf $(MAIN).xml

$(MAIN).ps: $(XMLS)
	docbook2ps -o $(OUT)/ps $(MAIN).xml

$(MAIN).html: $(XMLS)
	docbook2html -u -o $(OUT)/html $(MAIN).xml

$(MAIN).txt: $(XMLS)
	docbook2txt -o $(OUT)/txt $(MAIN).xml

clean:
	rm -rf dist

