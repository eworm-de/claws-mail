SUBDIRS = common gtk plugins

bin_PROGRAMS = sylpheed

sylpheed_SOURCES = \
	main.c main.h \
	mainwindow.c mainwindow.h \
	folderview.c folderview.h \
	summaryview.c summaryview.h \
	messageview.c messageview.h \
	headerview.c headerview.h \
	textview.c textview.h \
	mimeview.c mimeview.h \
	summary_search.c summary_search.h \
	message_search.c message_search.h \
	folder.c folder.h \
	procmsg.c procmsg.h \
	procheader.c procheader.h \
	action.c action.h \
	compose.c compose.h \
	stock_pixmap.c stock_pixmap.h \
	prefs_gtk.c prefs_gtk.h \
	prefs_common.c prefs_common.h \
	prefs_account.c prefs_account.h \
	prefs_folder_item.c prefs_folder_item.h \
	prefs_display_header.c prefs_display_header.h \
	prefs_customheader.c prefs_customheader.h \
	prefs_summary_column.c prefs_summary_column.h \
	prefs_template.c prefs_template.h \
	prefs_actions.c prefs_actions.h \
	account.c account.h \
	displayheader.c displayheader.h \
	customheader.c customheader.h \
	addressbook.c addressbook.h \
	addr_compl.c addr_compl.h \
	addressitem.h \
	adbookbase.h \
	addritem.c addritem.h \
	addrcache.c addrcache.h \
	addrclip.c addrclip.h \
	addrselect.c addrselect.h \
	addrbook.c addrbook.h \
	addrindex.c addrindex.h \
	vcard.c vcard.h \
	ldif.c ldif.h \
	importldif.c importldif.h \
	mutt.c mutt.h \
	importmutt.c importmutt.h \
	pine.c pine.h \
	importpine.c importpine.h \
	jpilot.c jpilot.h \
	syldap.c syldap.h \
	editbook.c editbook.h \
	editgroup.c editgroup.h \
	editaddress.c editaddress.h \
	editvcard.c editvcard.h \
	editjpilot.c editjpilot.h \
	editldap.c editldap.h \
	editldap_basedn.c editldap_basedn.h \
	addressadd.c addressadd.h \
	addrharvest.c addrharvest.h \
	addrgather.c addrgather.h \
	exporthtml.c exporthtml.h \
	exphtmldlg.c exphtmldlg.h \
	filesel.c filesel.h \
	foldersel.c foldersel.h \
	statusbar.c statusbar.h \
	logwindow.c logwindow.h \
	sourcewindow.c sourcewindow.h \
	undo.c undo.h \
	alertpanel.c alertpanel.h \
	inputdialog.c inputdialog.h \
	progressdialog.c progressdialog.h \
	grouplistdialog.c grouplistdialog.h \
	setup.c setup.h \
	codeconv.c codeconv.h \
	unmime.c unmime.h \
	ssl_manager.c ssl_manager.h \
	pop.c pop.h \
	mh.c mh.h \
	mbox.c mbox.h \
	send_message.c send_message.h \
	recv.c recv.h \
	inc.c inc.h \
	import.c import.h \
	export.c export.h \
	news.c news.h \
	imap.c imap.h \
	html.c html.h \
	enriched.c enriched.h \
	procmime.c procmime.h \
	rfc2015.c rfc2015.h \
	passphrase.c passphrase.h \
	select-keys.c select-keys.h \
	sigstatus.c sigstatus.h \
	simple-gettext.c \
	manual.c manual.h \
	scoring.c scoring.h \
	matcher.c matcher.h \
	prefs_matcher.c prefs_matcher.h \
	prefs_scoring.c prefs_scoring.h \
	filtering.c filtering.h \
	prefs_filtering.c prefs_filtering.h \
	mbox_folder.c mbox_folder.h \
	quote_fmt_lex.l quote_fmt_lex.h \
	quote_fmt_parse.y quote_fmt.c quote_fmt.h \
	gtkaspell.c gtkaspell.h \
	matcher_parser_lex.l matcher_parser_lex.h \
	matcher_parser_parse.y matcher_parser.h matcher_parser_parse.h \
	string_match.h string_match.c \
	msgcache.h msgcache.c \
	noticeview.h noticeview.c \
	toolbar.c toolbar.h \
	prefs_toolbar.c prefs_toolbar.h \
	crash.c crash.h

BUILT_SOURCES = \
	quote_fmt_lex.c \
	quote_fmt_parse.c \
	quote_fmt_parse.h \
	matcher_parser_lex.c \
	matcher_parser_parse.c \
	matcher_parser_parse.h

EXTRA_DIST = \
	quote_fmt_parse.h \
	pixmaps/address_book.xpm \
	pixmaps/address.xpm \
	pixmaps/book.xpm \
	pixmaps/category.xpm \
	pixmaps/checkbox_off.xpm \
	pixmaps/checkbox_on.xpm \
	pixmaps/clip.xpm \
	pixmaps/clipkey.xpm \
	pixmaps/close.xpm \
	pixmaps/complete.xpm \
	pixmaps/continue.xpm \
	pixmaps/deleted.xpm \
	pixmaps/dir_close.xpm \
	pixmaps/dir_open.xpm \
	pixmaps/dir_open_hrm.xpm \
	pixmaps/drafts_close.xpm \
	pixmaps/drafts_open.xpm \
	pixmaps/down_arrow.xpm \
	pixmaps/error.xpm \
	pixmaps/exec.xpm \
	pixmaps/folder.xpm \
	pixmaps/forwarded.xpm \
	pixmaps/gpg_signed.xpm \
	pixmaps/group.xpm \
	pixmaps/ignorethread.xpm \
	pixmaps/inbox.xpm \
	pixmaps/inbox_hrm.xpm \
	pixmaps/interface.xpm \
	pixmaps/jpilot.xpm \
	pixmaps/key.xpm \
	pixmaps/ldap.xpm \
	pixmaps/linewrap.xpm \
	pixmaps/locked.xpm \
	pixmaps/mail.xpm \
	pixmaps/mail_attach.xpm \
	pixmaps/mail_compose.xpm \
	pixmaps/mail_forward.xpm \
	pixmaps/mail_receive.xpm \
	pixmaps/mail_receive_all.xpm \
	pixmaps/mail_reply.xpm \
	pixmaps/mail_reply_to_all.xpm \
	pixmaps/mail_reply_to_author.xpm \
	pixmaps/mail_send.xpm \
	pixmaps/mail_send_queue.xpm \
	pixmaps/mark.xpm \
	pixmaps/new.xpm \
	pixmaps/news_compose.xpm \
	pixmaps/outbox.xpm \
	pixmaps/outbox_hrm.xpm \
	pixmaps/paste.xpm \
	pixmaps/preferences.xpm \
	pixmaps/properties.xpm \
	pixmaps/regular.xpm \
	pixmaps/replied.xpm \
	pixmaps/search.xpm \
	pixmaps/sylpheed.xpm \
	pixmaps/sylpheed_logo.xpm \
	pixmaps/trash.xpm \
	pixmaps/trash_hrm.xpm \
	pixmaps/unread.xpm \
	pixmaps/up_arrow.xpm \
	pixmaps/vcard.xpm \
	pixmaps/work_offline.xpm \
	pixmaps/work_online.xpm \
	pixmaps/notice_warn.xpm \
	pixmaps/notice_error.xpm \
	pixmaps/notice_note.xpm \
	pixmaps/quicksearch.xpm

INCLUDES = \
	-Icommon \
	-Igtk \
	-DG_LOG_DOMAIN=\"Sylpheed\" \
	-I$(top_srcdir)/intl \
        $(ASPELL_CFLAGS)    \
	$(GTK_CFLAGS) \
	$(GPGME_CFLAGS) \
	$(OPENSSL_CFLAGS) \
	-I$(includedir)

sylpheed_LDFLAGS = \
	-export-dynamic

sylpheed_LDADD = \
	$(ASPELL_LIBS) \
	gtk/libsylpheedgtk.la \
	$(INTLLIBS) \
	$(GTK_LIBS) \
	$(GPGME_LIBS) \
	$(LDAP_LIBS) \
	$(OPENSSL_LIBS) \
	$(LIBICONV)

CPPFLAGS = \
	-DLOCALEDIR=\""$(localedir)"\" \
	-DMANUALDIR=\""$(manualdir)"\" \
	-DFAQDIR=\""$(faqdir)"\" \
	-DTARGET_ALIAS=\""$(target_triplet)"\" \
	-DSYSCONFDIR=\""$(sysconfdir)"\"

YFLAGS = -d
