# vim:ts=32:noet
#
# Makefile for Sylpheed-Claws / Win32 / GCC
# - Win32/MinGW GCC 2.95/3.2
# - Cygwin/MinGW GCC 2.95/3.2
# - Linux/MinGW GCC 2.95/3.2
# Before building, read the README-w32.txt and
# 1. Create that directory structure
# 2. Set DEBUGVERSION, GCCVERSION, CYGWIN according to your environment
# 3. Call "make -f Make-ming.mak"
#    (use mingw32-make.exe for mingw-2.0.x)
#
# Note:
# Although this makefile doesnt make use of the shell, building under cygwin
# only works from inside cygwin's bash for me. Building with the standalone
# MinGW compiler works from windows commandline as well as from rxvt shipped
# with MSYS.
#

################################################################################

# DEBUGVERSION: set to 1 to include debugging symbols
DEBUGVERSION=1

# GCCVERSION: set to 3 if using gcc3.x (-fnative-struct | -mms-bitfields)
GCCVERSION=2

# CYGWIN: set to 1 if building from cygwin shell (-mno-cygwin)
CYGWIN=0

# CROSS: Cross comilation prefix
#CROSS=i586-mingw32msvc-

################################################################################
# directory strucure as in README-w32.txt

LIBDIR	=../../../lib
INCLUDEDIR	=../../../include
GLIBDIR	=$(INCLUDEDIR)/glib-2.0
GTKDIR	=$(INCLUDEDIR)/gtk
GDKDIR	=$(INCLUDEDIR)/gdk
GLIBLIBINCDIR	=$(LIBDIR)/glib-2.0/include
GTKLIBINCDIR	=$(LIBDIR)/gtk+/include

FNMATCHDIR	=../../fnmatch
GPGMEDIR	=../../gpgme
LIBCOMPFACEDIR	=../../libcompface
OPENSSLDIR	=../../openssl
REGEXDIR	=../../regex
W32LIBDIR	=../../w32lib
LIBJCONVDIR	=../libjconv

ROOTDIR	=..
SRCDIR	=$(ROOTDIR)/src
PODIR	=$(ROOTDIR)/po

SRCDIR_ESC	=\.\.\/src\/

################################################################################

ifeq ($(DEBUGVERSION),1)
	DEBUGFLAG=-g
	DEBUGDEF=-D_DEBUG
	APPNAME=sylpheed_d.exe
else
	NOCONSOLE=-mwindows
	OPTIMIZATION=-O3
	APPNAME=sylpheed.exe
endif

ifeq ($(GCCVERSION),3)
	# gcc-3.x: -mms-bitfields
	BITFIELD=-mms-bitfields
else
	# gcc-2.x: -fnative-struct
	BITFIELD=-fnative-struct
endif

ifeq ($(CYGWIN),1)
	# cygwin: built native w32 code
	NOCYGWIN=-mno-cygwin
endif

################################################################################

CC=$(CROSS)gcc
RESCOMP=$(CROSS)windres
FLEX=flex
YACC=bison -y
MSGFMT=msgfmt
ICONV=iconv
SED=sed
CD=cd
RM=rm

###

VPATH=$(SRCDIR):$(SRCDIR)/common:$(SRCDIR)/gtk:$(PODIR):$(LIBJCONVDIR):$(SRCDIR)/plugins/demo
DEFINES=-DHAVE_CONFIG_H -DHAVE_BYTE_TYPEDEF $(DEBUGDEF)
EXTRALIBS=-lwsock32
RESOURCE=appicon

FLAGS=$(DEBUGFLAG) $(BITFIELD) $(NOCYGWIN) $(OPTIMIZATION)

### version

CONFIGURE_IN=$(ROOTDIR)/configure.in
VERSION_H_IN=$(SRCDIR)/common/version.h.in
VERSION_H=$(SRCDIR)/common/version.h

PACKAGE=$(shell grep ^PACKAGE= $(CONFIGURE_IN)|sed -e "s/.*=//" -)
MAJOR_VERSION=$(shell grep ^MAJOR_VERSION= $(CONFIGURE_IN)|sed -e "s/.*=//" -)
MINOR_VERSION=$(shell grep ^MINOR_VERSION= $(CONFIGURE_IN)|sed -e "s/.*=//" -)
MICRO_VERSION=$(shell grep ^MICRO_VERSION= $(CONFIGURE_IN)|sed -e "s/.*=//" -)
EXTRA_VERSION=$(shell grep ^EXTRA_VERSION= $(CONFIGURE_IN)|sed -e "s/.*=//" -)
VERSION=$(MAJOR_VERSION).$(MINOR_VERSION).$(MICRO_VERSION)$(EXTRA_VERSION)

### bisonfiles

# names of parser output files and escaped versions
LEX_YY_C=lex.yy.c
Y_TAB_C=y.tab.c
Y_TAB_H=y.tab.h
LEX_YY_C_ESC=lex\.yy\.c
Y_TAB_C_ESC=y\.tab\.c
Y_TAB_H_ESC=y\.tab\.h

QUOTE_FMT=$(SRCDIR)/quote_fmt
MATCHER_PARSER=$(SRCDIR)/matcher_parser
QUOTE_FMT_TARGETS=$(QUOTE_FMT)_lex.c $(QUOTE_FMT)_parse.c $(QUOTE_FMT)_parse.h
MATCHER_PARSER_TARGETS=$(MATCHER_PARSER)_lex.c $(MATCHER_PARSER)_parse.c $(MATCHER_PARSER)_parse.h

### translation

MOFILES=bg.mo cs.mo de.mo el.mo en_GB.mo es.mo fr.mo hr.mo hu.mo it.mo ja.mo ko.mo nl.mo pl.mo pt_BR.mo ru.mo sr.mo sv.mo
MONAME=sylpheed.mo
# avoid check for backslash, different shells need different escaping. yucc!
CHARSET_RE=\(.*Content-Type: text\/plain; charset=\)\([-a-zA-Z0-9]*\)\(.*\)

### sylpheed

INCLUDES= \
	-I. \
	-I$(SRCDIR) \
	-I$(SRCDIR)/common \
	-I$(SRCDIR)/gtk \
	-I$(LIBJCONVDIR) \
	-I$(INCLUDEDIR) \
	-I$(GLIBDIR) \
	-I$(GTKDIR) \
	-I$(GDKDIR) \
	-I$(GLIBLIBINCDIR) \
	-I$(GTKLIBINCDIR) \
	-I$(FNMATCHDIR)/src/posix \
	-I$(GPGMEDIR)/gpgme \
	-I$(LIBCOMPFACEDIR)/src \
	-I$(OPENSSLDIR)/src \
	-I$(REGEXDIR)/src \
	-I$(W32LIBDIR)/src \
	-I$(LIBJCONVDIR)

CFLAGS=$(FLAGS) $(DEFINES) $(INCLUDES)

LIBS= \
	$(LIBDIR)/glib-2.0.lib \
	$(LIBDIR)/gmodule-2.0.lib \
	$(LIBDIR)/gdk.lib \
	$(LIBDIR)/gtk.lib \
	$(LIBDIR)/iconv.lib \
	$(LIBDIR)/intl.lib \
	$(LIBDIR)/libeay32.lib \
	$(LIBDIR)/ssleay32.lib \
	$(LIBDIR)/RSAglue.lib \
	$(FNMATCHDIR)/fnmatch.lib \
	$(GPGMEDIR)/gpgme.lib \
	$(LIBCOMPFACEDIR)/libcompface.lib \
	$(REGEXDIR)/regex.lib \
	$(W32LIBDIR)/w32lib.lib

OBJECTS= \
	about.o \
	account.o \
	addr_compl.o \
	addrbook.o \
	addrcache.o \
	addrclip.o \
	addressadd.o \
	addressbook.o \
	addrgather.o \
	addrharvest.o \
	addrindex.o \
	addritem.o \
	addrselect.o \
	alertpanel.o \
	automaton.o \
	base64.o \
	codeconv.o \
	colorlabel.o \
	compat.o \
	compose.o \
	conv.o \
	customheader.o \
	displayheader.o \
	editaddress.o \
	editbook.o \
	editgroup.o \
	editjpilot.o \
	editldap.o \
	editldap_basedn.o \
	editvcard.o \
	enriched.o \
	exphtmldlg.o \
	export.o \
	exporthtml.o \
	filesel.o \
	filtering.o \
	folder.o \
	foldersel.o \
	folderview.o \
	grouplistdialog.o \
	gtkaspell.o \
	gtksctree.o \
	gtkshruler.o \
	gtkstext.o \
	gtkutils.o \
	headerview.o \
	hooks.o \
	html.o \
	imageview.o \
	imap.o \
	import.o \
	importldif.o \
	importmutt.o \
	importpine.o \
	inc.o \
	info.o \
	inputdialog.o \
	jpilot.o \
	ldif.o \
	log.o \
	logwindow.o \
	main.o \
	mainwindow.o \
	manage_window.o \
	manual.o \
	matcher.o \
	matcher_parser_lex.o \
	matcher_parser_parse.o \
	mbox.o \
	mbox_folder.o \
	md5.o \
	menu.o \
	message_search.o \
	messageview.o \
	mgutils.o \
	mh.o \
	mimeview.o \
	msgcache.o \
	mutt.o \
	news.o \
	nntp.o \
	noticeview.o \
	passphrase.o \
	pgptext.o \
	pine.o \
	plugin.o \
	pop.o \
	prefs.o \
	prefs_account.o \
	prefs_actions.o \
	prefs_common.o \
	prefs_customheader.o \
	prefs_display_header.o \
	prefs_filtering.o \
	prefs_folder_item.o \
	prefs_gtk.o \
	prefs_matcher.o \
	prefs_scoring.o \
	prefs_summary_column.o \
	prefs_template.o \
	prefs_toolbar.o \
	procheader.o \
	procmime.o \
	procmsg.o \
	progressdialog.o \
	quote_fmt.o \
	quote_fmt_lex.o \
	quote_fmt_parse.o \
	quoted-printable.o \
	recv.o \
	rfc2015.o \
	scoring.o \
	select-keys.o \
	selective_download.o \
	send.o \
	session.o \
	setup.o \
	sigstatus.o \
	simple-gettext.o \
	smtp.o \
	socket.o \
	sourcewindow.o \
	ssl.o \
	ssl_certificate.o \
	ssl_manager.o \
	sslcertwindow.o \
	statusbar.o \
	stock_pixmap.o \
	string_match.o \
	stringtable.o \
	summary_search.o \
	summaryview.o \
	syldap.o \
	sylpheed.o \
	template.o \
	textview.o \
	toolbar.o \
	undo.o \
	unmime.o \
	utils.o \
	uuencode.o \
	vcard.o \
	w32_aspell_init.o \
	w32_mailcap.o \
	xml.o \
	xmlprops.o \

### targets

all: version bisonfiles compile translation

compile: $(APPNAME)
version: $(VERSION_H)
bisonfiles: $(QUOTE_FMT_TARGETS) $(MATCHER_PARSER_TARGETS)
translation: $(MOFILES)

$(APPNAME): $(OBJECTS) $(RESOURCE).o
	$(CC) $(NOCONSOLE) $(FLAGS) $(OBJECTS) $(RESOURCE).o $(EXTRALIBS) $(LIBS) -o $(APPNAME)
$(RESOURCE).o: $(RESOURCE).rc
	$(RESCOMP) $< $@

$(VERSION_H): $(CONFIGURE_IN) $(VERSION_H_IN)
	echo Version: $(PACKAGE) $(VERSION)
	$(SED) -e "s/@PACKAGE@/$(PACKAGE)/;s/@VERSION@/$(VERSION)/;" $(VERSION_H_IN) > $@

%.mo: $(PODIR)/%.po
	@# change Content-Type to "utf-8" (needed for unix msgfmt)
	$(SED) -e "/$(CHARSET_RE)/ s/$(CHARSET_RE)/\1utf-8\3/; " $< > $(*F)-tmp.po
	@# convert to utf-8, extract unmodified charset from original .po
	$(ICONV) -f $(shell sed -n -e "/$(CHARSET_RE)/ {s/$(CHARSET_RE)/\2/;p;} " $<) -t utf-8 $(*F)-tmp.po > $(*F)-utf8.po
	@# create final .mo file
	$(MSGFMT) -o $@ $(*F)-utf8.po
	@# cleanup
	@-$(RM) $(*F)-tmp.po $(*F)-utf8.po

%_lex.c: $(SRCDIR)/%_lex.l
	$(FLEX) $(SRCDIR)/$(*F)_lex.l
	$(SED) -e "s/$(SRCDIR_ESC)//;s/$(LEX_YY_C_ESC)/$(@F)/" $(LEX_YY_C) > $(SRCDIR)/$(*F)_lex.c

%_parse.c %_parse.h: $(SRCDIR)/%_parse.y
	$(YACC) -d $(SRCDIR)/$(*F)_parse.y
	$(SED) -e "s/$(Y_TAB_C_ESC)/$(*F)_parse.c/" $(Y_TAB_C) > $(SRCDIR)/$(*F)_parse.c
	$(SED) -e "s/$(Y_TAB_H_ESC)/$(*F)_parse.h/" $(Y_TAB_H) > $(SRCDIR)/$(*F)_parse.h
# hack: prevent autogeneration
%_lex.l %_parse.y:
	@echo dummy:$@

clean:
	-$(RM) *.o $(APPNAME)
	-$(RM) $(LEX_YY_C) $(Y_TAB_C) $(Y_TAB_H)
	-$(CD) $(SRCDIR); $(RM) $(QUOTE_FMT_TARGETS) $(MATCHER_PARSER_TARGETS)

### dependencies
# sylpheed
about.o: 	about.c about.h
account.o: 	account.c account.h
addr_compl.o: 	addr_compl.c addr_compl.h
addrbook.o: 	addrbook.c addrbook.h
addrcache.o: 	addrcache.c addrcache.h
addrclip.o: 	addrclip.c addrclip.h
addressadd.o: 	addressadd.c addressadd.h
addressbook.o: 	addressbook.c addressbook.h
addrgather.o: 	addrgather.c addrgather.h
addrharvest.o: 	addrharvest.c addrharvest.h
addrindex.o: 	addrindex.c addrindex.h
addritem.o: 	addritem.c addritem.h
addrselect.o: 	addrselect.c addrselect.h
alertpanel.o: 	alertpanel.c alertpanel.h
automaton.o: 	automaton.c automaton.h
base64.o: 	base64.c base64.h
codeconv.o: 	codeconv.c codeconv.h
colorlabel.o: 	colorlabel.c colorlabel.h
compose.o: 	compose.c compose.h
customheader.o: 	customheader.c customheader.h
displayheader.o: 	displayheader.c displayheader.h
editaddress.o: 	editaddress.c editaddress.h
editbook.o: 	editbook.c editbook.h
editgroup.o: 	editgroup.c editgroup.h
editjpilot.o: 	editjpilot.c editjpilot.h
editldap.o: 	editldap.c editldap.h
editldap_basedn.o: 	editldap_basedn.c editldap_basedn.h
editvcard.o: 	editvcard.c editvcard.h
enriched.o: 	enriched.c enriched.h
exphtmldlg.o: 	exphtmldlg.c exphtmldlg.h
export.o: 	export.c export.h
exporthtml.o: 	exporthtml.c exporthtml.h
filesel.o: 	filesel.c filesel.h
filtering.o: 	filtering.c filtering.h
folder.o: 	folder.c folder.h
foldersel.o: 	foldersel.c foldersel.h
folderview.o: 	folderview.c folderview.h
grouplistdialog.o: 	grouplistdialog.c grouplistdialog.h
gtkaspell.o: 	gtkaspell.c gtkaspell.h gtkxtext.h
gtksctree.o: 	gtksctree.c gtksctree.h
gtkshruler.o: 	gtkshruler.c gtkshruler.h
gtkstext.o: 	gtkstext.c gtkstext.h
gtkutils.o: 	gtkutils.c gtkutils.h
headerview.o: 	headerview.c headerview.h
hooks.o:	hooks.c hooks.h
html.o: 	html.c html.h
imageview.o: 	imageview.c imageview.h
imap.o: 	imap.c imap.h
import.o: 	import.c import.h
importldif.o: 	importldif.c importldif.h
importmutt.o: 	importmutt.c importmutt.h
importpine.o: 	importpine.c importpine.h
inc.o: 	inc.c inc.h
inputdialog.o: 	inputdialog.c inputdialog.h
jpilot.o: 	jpilot.c jpilot.h
ldif.o: 	ldif.c ldif.h
log.o:	log.c log.h
logwindow.o: 	logwindow.c logwindow.h
main.o: 	main.c main.h
mainwindow.o: 	mainwindow.c mainwindow.h
manage_window.o: 	manage_window.c manage_window.h
manual.o: 	manual.c manual.h
matcher.o: 	matcher.c matcher.h
mbox.o: 	mbox.c mbox.h
mbox_folder.o: 	mbox_folder.c mbox_folder.h
md5.o: 	md5.c md5.h
menu.o: 	menu.c menu.h
message_search.o: 	message_search.c message_search.h
messageview.o: 	messageview.c messageview.h
mgutils.o: 	mgutils.c mgutils.h
mh.o: 	mh.c mh.h
mimeview.o: 	mimeview.c mimeview.h
msgcache.o:	msgcache.c msgcache.h 
mutt.o: 	mutt.c mutt.h
news.o: 	news.c news.h
nntp.o: 	nntp.c nntp.h
noticeview.o:	noticeview.c noticeview.h 
passphrase.o: 	passphrase.c passphrase.h
pgptext.o: 	pgptext.c pgptext.h
pine.o: 	pine.c pine.h
plugin.o: 	plugin.c plugin.h
pop.o: 	pop.c pop.h
prefs.o: 	prefs.c prefs.h
prefs_account.o: 	prefs_account.c prefs_account.h
prefs_actions.o: 	prefs_actions.c prefs_actions.h
prefs_common.o: 	prefs_common.c prefs_common.h
prefs_customheader.o: 	prefs_customheader.c prefs_customheader.h
prefs_display_header.o: 	prefs_display_header.c prefs_display_header.h
prefs_filtering.o: 	prefs_filtering.c prefs_filtering.h
prefs_folder_item.o: 	prefs_folder_item.c prefs_folder_item.h
prefs_gtk.o: 	prefs_gtk.c prefs_gtk.h
prefs_matcher.o: 	prefs_matcher.c prefs_matcher.h
prefs_scoring.o: 	prefs_scoring.c prefs_scoring.h
prefs_summary_column.o: 	prefs_summary_column.c prefs_summary_column.h
prefs_template.o: 	prefs_template.c prefs_template.h
prefs_toolbar.o: 	prefs_toolbar.c prefs_toolbar.h
procheader.o: 	procheader.c procheader.h
procmime.o: 	procmime.c procmime.h
procmsg.o: 	procmsg.c procmsg.h
progressdialog.o: 	progressdialog.c progressdialog.h
quote_fmt.o:	quote_fmt.c quote_fmt.h
recv.o: 	recv.c recv.h
rfc2015.o: 	rfc2015.c rfc2015.h
scoring.o: 	scoring.c scoring.h
select-keys.o: 	select-keys.c select-keys.h
selective_download.o: 	selective_download.c selective_download.h
send.o: 	send.c send.h
session.o: 	session.c session.h
setup.o: 	setup.c setup.h
sigstatus.o: 	sigstatus.c sigstatus.h
simple-gettext.o: 	simple-gettext.c
smtp.o: 	smtp.c smtp.h
socket.o: 	socket.c socket.h
sourcewindow.o: 	sourcewindow.c sourcewindow.h
ssl.o: 	ssl.c ssl.h
ssl_certificate.o: 	ssl_certificate.c ssl_certificate.h
ssl_manager.o:	ssl_manager.c ssl_manager.h
sslcertwindow.o:	sslcertwindow.c sslcertwindow.h
statusbar.o: 	statusbar.c statusbar.h
stock_pixmap.o: 	stock_pixmap.c stock_pixmap.h
string_match.o:	string_match.c string_match.h 
stringtable.o: 	stringtable.c stringtable.h
summary_search.o: 	summary_search.c summary_search.h
summaryview.o: 	summaryview.c summaryview.h
syldap.o: 	syldap.c syldap.h
sylpheed.o:	sylpheed.c sylpheed.h
template.o: 	template.c template.h
textview.o: 	textview.c textview.h
toolbar.o: 	toolbar.c toolbar.h
undo.o: 	undo.c undo.h
unmime.o: 	unmime.c unmime.h
utils.o: 	utils.c utils.h
uuencode.o: 	uuencode.c uuencode.h
vcard.o: 	vcard.c vcard.h
xml.o: 	xml.c xml.h
xmlprops.o: 	xmlprops.c xmlprops.h
# bison / flex generated
matcher_parser_lex.o: 	matcher_parser_lex.c matcher_parser_lex.h
matcher_parser_parse.o: 	matcher_parser_parse.c matcher_parser_parse.h
quote_fmt_lex.o:	quote_fmt_lex.c quote_fmt_lex.h
quote_fmt_parse.o: 	quote_fmt_parse.c quote_fmt_parse.h
quoted-printable.o:	quoted-printable.c quoted-printable.h
# win32 additions
w32_aspell_init.o:	w32_aspell_init.c w32_aspell_init.h
w32_mailcap.o:	w32_mailcap.c w32_mailcap.h
# libjconv
compat.o:	compat.c jconv.h
conv.o:	conv.c jconv.h
info.o:	info.c jconv.h
