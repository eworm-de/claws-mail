# vim:ts=32:noet
#
# Makefile for Sylpheed-Claws / Win32 / GCC
# - MinGW GCC 2.95
# - MinGW GCC 3.2
# - Cygwin/MinGW GCC 2.95
# 
# Before building, read the README-w32.txt and
# 1. Create that directory structure
# 2. Generate missing sources (-> patch_claws.bat)
# 3. Set DEBUGVERSION, GCCVERSION, CYGWIN according to your environment
# 4. Call "make -f Make-ming.mak"
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
DEBUGVERSION=0

# GCCVERSION: set to 3 if using gcc3.x (-fnative-struct | -mms-bitfields)
GCCVERSION=3

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

SRCDIR	=../src

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
VPATH=../src:../libjconv
DEFINES=-DHAVE_CONFIG_H -DHAVE_BYTE_TYPEDEF $(DEBUGDEF)
EXTRALIBS=-lwsock32
RESOURCE=appicon
RESCOMP=$(CROSS)windres

FLAGS=$(DEBUGFLAG) $(BITFIELD) $(NOCYGWIN) $(OPTIMIZATION)

INCLUDES= \
	-I. \
	-I$(SRCDIR) \
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
	pop.o \
	prefs.o \
	prefs_account.o \
	prefs_actions.o \
	prefs_common.o \
	prefs_customheader.o \
	prefs_display_header.o \
	prefs_filtering.o \
	prefs_folder_item.o \
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
	statusbar.o \
	stock_pixmap.o \
	string_match.o \
	stringtable.o \
	summary_search.o \
	summaryview.o \
	syldap.o \
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

all: $(OBJECTS) $(RESOURCE) link
$(RESOURCE):
	$(RESCOMP) $@.rc $@.o
link: $(APPNAME)
$(APPNAME):
	$(CC) $(NOCONSOLE) $(FLAGS) $(OBJECTS) $(RESOURCE).o $(EXTRALIBS) $(LIBS) -o $(APPNAME)
clean:
	-rm *.o $(APPNAME)

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
pop.o: 	pop.c pop.h
prefs.o: 	prefs.c prefs.h
prefs_account.o: 	prefs_account.c prefs_account.h
prefs_actions.o: 	prefs_actions.c prefs_actions.h
prefs_common.o: 	prefs_common.c prefs_common.h
prefs_customheader.o: 	prefs_customheader.c prefs_customheader.h
prefs_display_header.o: 	prefs_display_header.c prefs_display_header.h
prefs_filtering.o: 	prefs_filtering.c prefs_filtering.h
prefs_folder_item.o: 	prefs_folder_item.c prefs_folder_item.h
prefs_matcher.o: 	prefs_matcher.c prefs_matcher.h
prefs_scoring.o: 	prefs_scoring.c prefs_scoring.h
prefs_summary_column.o: 	prefs_summary_column.c prefs_summary_column.h
prefs_template.o: 	prefs_template.c prefs_template.h
prefs_toolbar.o: 	prefs_toolbar.c prefs_toolbar.h
procheader.o: 	procheader.c procheader.h
procmime.o: 	procmime.c procmime.h
procmsg.o: 	procmsg.c procmsg.h
progressdialog.o: 	progressdialog.c progressdialog.h
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
statusbar.o: 	statusbar.c statusbar.h
stock_pixmap.o: 	stock_pixmap.c stock_pixmap.h
string_match.o:	string_match.c string_match.h 
stringtable.o: 	stringtable.c stringtable.h
summary_search.o: 	summary_search.c summary_search.h
summaryview.o: 	summaryview.c summaryview.h
syldap.o: 	syldap.c syldap.h
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
quote_fmt.o:	quote_fmt.c quote_fmt.h
quote_fmt_lex.o:	quote_fmt_lex.c quote_fmt_lex.h
quote_fmt_parse.o: 	quote_fmt_parse.c quote_fmt_parse.h
# win32 additions
w32_aspell_init.o:	w32_aspell_init.c w32_aspell_init.h
w32_mailcap.o:	w32_mailcap.c w32_mailcap.h
# libjconv
compat.o:	compat.c jconv.h
conv.o:	conv.c jconv.h
info.o:	info.c jconv.h


