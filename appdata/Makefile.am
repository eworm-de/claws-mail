# Copyright 1999-2015 the Claws Mail team.
# This file is part of Claws Mail package, and distributed under the
# terms of the General Public License version 3 (or later).
# See COPYING file for license details.

@INTLTOOL_XML_RULE@

appdata_in_files = claws-mail.appdata.xml.in \
	claws-mail-acpi_notifier.metainfo.xml.in \
	claws-mail-address_keeper.metainfo.xml.in \
	claws-mail-archive.metainfo.xml.in \
	claws-mail-att_remover.metainfo.xml.in \
	claws-mail-attachwarner.metainfo.xml.in \
	claws-mail-bogofilter.metainfo.xml.in \
	claws-mail-bsfilter.metainfo.xml.in \
	claws-mail-clamd.metainfo.xml.in \
	claws-mail-fancy.metainfo.xml.in \
	claws-mail-fetchinfo.metainfo.xml.in \
	claws-mail-gdata.metainfo.xml.in \
	claws-mail-libravatar.metainfo.xml.in \
	claws-mail-mailmbox.metainfo.xml.in \
	claws-mail-managesieve.metainfo.xml.in \
	claws-mail-newmail.metainfo.xml.in \
	claws-mail-notification.metainfo.xml.in \
	claws-mail-pdf_viewer.metainfo.xml.in \
	claws-mail-perl.metainfo.xml.in \
	claws-mail-pgpcore.metainfo.xml.in \
	claws-mail-pgpinline.metainfo.xml.in \
	claws-mail-pgpmime.metainfo.xml.in \
	claws-mail-python.metainfo.xml.in \
	claws-mail-rssyl.metainfo.xml.in \
	claws-mail-smime.metainfo.xml.in \
	claws-mail-spam_report.metainfo.xml.in \
	claws-mail-spamassassin.metainfo.xml.in \
	claws-mail-tnef_parse.metainfo.xml.in \
	claws-mail-vcalendar.metainfo.xml.in

appdatadir=$(datarootdir)/appdata
appdata_DATA = $(appdata_in_files:.xml.in=.xml)

EXTRA_DIST = $(appdata_DATA) $(appdata_in_files)


.PHONY: test
