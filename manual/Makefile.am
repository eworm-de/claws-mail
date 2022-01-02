# Copyright 1999-2021 the Claws Mail team.
# This file is part of Claws Mail package, and distributed under the
# terms of the General Public License version 3 (or later).
# See COPYING file for license details.

LANGS=en es fr
EXTRA_DIST=dtd $(LANGS)

if BUILD_MANUAL

HTML_MANUALS=$(foreach lang,$(LANGS),$(lang)/claws-mail-manual.html)
PDF_MANUALS=$(foreach lang,$(LANGS),$(lang)/claws-mail-manual.pdf)
PS_MANUALS=$(foreach lang,$(LANGS),$(lang)/claws-mail-manual.ps)
TXT_MANUALS=$(foreach lang,$(LANGS),$(lang)/claws-mail-manual.txt)

manual_deps = \
	$(1)/claws-mail-manual.xml \
	$(1)/account.xml \
	$(1)/ack.xml \
	$(1)/addrbook.xml \
	$(1)/advanced.xml \
	$(1)/faq.xml \
	$(1)/glossary.xml \
	$(1)/gpl.xml \
	$(1)/handling.xml \
	$(1)/intro.xml \
	$(1)/keyboard.xml \
	$(1)/plugins.xml \
	$(1)/starting.xml

define HTML_template =
$(1)/claws-mail-manual.$(2): $(manual_deps)
	@$(MKDIR_P) $(1)/$(2)
	SP_ENCODING=xml $(DOCBOOK2HTML) -u -o $(1)/$(2) $$<
	$(SED) 's,^><META,><META http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\"><META,' $(1)/$(2)/claws-mail-manual.$(2) > $$@
	rm -rf $(1)/$(2)
endef

define BASE_template =
$(1)/claws-mail-manual.$(2): $(manual_deps)
	@$(MKDIR_P) $(1)/$(2)
	$(3) -o $(1)/$(2) $$<
	mv -f $(1)/$(2)/claws-mail-manual.$(2) $$@
	rm -rf $(1)/$(2)
endef

if MANUAL_HTML
nobase_dist_manualhtml_DATA=$(HTML_MANUALS)
manualhtmldir=$(manualdir)
$(foreach lang,$(LANGS),$(eval $(call HTML_template,$(lang),html)))
endif

if MANUAL_PDF
nobase_dist_manualpdf_DATA=$(PDF_MANUALS)
manualpdfdir=$(manualdir)
$(foreach lang,$(LANGS),$(eval $(call BASE_template,$(lang),pdf,$(DOCBOOK2PDF))))
endif

if MANUAL_PS
nobase_dist_manualps_DATA=$(PS_MANUALS)
manualpsdir=$(manualdir)
$(foreach lang,$(LANGS),$(eval $(call BASE_template,$(lang),ps,$(DOCBOOK2PS))))
endif

if MANUAL_TXT
nobase_dist_manualtxt_DATA=$(TXT_MANUALS)
manualtxtdir=$(manualdir)
$(foreach lang,$(LANGS),$(eval $(call BASE_template,$(lang),txt,$(DOCBOOK2TXT))))
endif

maintainer-clean:
	-rm -f $(HTML_MANUALS) $(PDF_MANUALS) $(PS_MANUALS) $(TXT_MANUALS)

endif

.PHONY: test
